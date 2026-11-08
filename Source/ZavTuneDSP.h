// ZavTune DSP core — pitch detection (YIN) + scale snapping + delay-line pitch shifter.
// No JUCE dependency, so it can be unit-tested on its own.
#pragma once
#include <vector>
#include <cmath>
#include <algorithm>
#include <array>

namespace zav {

enum class Scale { Chromatic = 0, Major, Minor, MajorPentatonic, MinorPentatonic, Blues, HarmonicMinor, NumScales };

inline const char* scaleName (int i)
{
    static const char* names[] = { "Chromatic", "Major", "Minor", "Major Pentatonic",
                                   "Minor Pentatonic", "Blues", "Harmonic Minor" };
    return names[i];
}

// Bitmask of allowed pitch classes relative to the key root.
inline std::array<bool, 12> scaleMask (Scale s)
{
    std::array<int, 12> intervals {};
    int n = 0;
    auto set = [&] (std::initializer_list<int> l) { for (int v : l) intervals[n++] = v; };
    switch (s)
    {
        case Scale::Major:           set ({ 0, 2, 4, 5, 7, 9, 11 }); break;
        case Scale::Minor:           set ({ 0, 2, 3, 5, 7, 8, 10 }); break;
        case Scale::MajorPentatonic: set ({ 0, 2, 4, 7, 9 }); break;
        case Scale::MinorPentatonic: set ({ 0, 3, 5, 7, 10 }); break;
        case Scale::Blues:           set ({ 0, 3, 5, 6, 7, 10 }); break;
        case Scale::HarmonicMinor:   set ({ 0, 2, 3, 5, 7, 8, 11 }); break;
        default:                     set ({ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 }); break;
    }
    std::array<bool, 12> m {};
    for (int i = 0; i < n; ++i) m[(size_t) intervals[i]] = true;
    return m;
}

//==============================================================================
// YIN pitch detector on a mono stream. Call push() per sample; it re-analyses every hop.
class PitchDetector
{
public:
    void prepare (double sampleRate, float minHz = 70.0f, float maxHz = 1000.0f)
    {
        fs = sampleRate;
        int capTau = (int) (fs / 60.0) + 4;                 // allocate for the widest range
        buf.assign ((size_t) (capTau * 2 + 4) * 2, 0.0f);
        diff.assign ((size_t) capTau + 2, 0.0f);
        setRange (minHz, maxHz, 5.0f);
    }

    // Real-time safe: no allocation. hopMs = how often pitch is re-analysed.
    void setRange (float minHz, float maxHz, float hopMs)
    {
        minHz = std::max (minHz, 60.0f);
        tauMin = std::max (2, (int) (fs / maxHz));
        tauMax = std::max (tauMin + 2, (int) (fs / minHz));
        window = tauMax;
        bufLen = window + tauMax + 2;
        hop = std::max (32, (int) (fs * hopMs * 0.001));
        writePos = 0; counter = 0; filled = 0;
    }

    int getHop() const { return hop; }

    void setThreshold (float t) { threshold = t; }
    void setGate (float linear) { gate = linear; }

    // returns true when a new analysis frame was produced
    bool push (float x)
    {
        // duplicate-write ring buffer so a frame is always contiguous
        buf[(size_t) writePos] = x;
        buf[(size_t) (writePos + bufLen)] = x;
        writePos = (writePos + 1) % bufLen;
        if (filled < bufLen) ++filled;

        if (++counter >= hop)
        {
            counter = 0;
            if (filled >= bufLen) { analyse (&buf[(size_t) writePos]); return true; }
        }
        return false;
    }

    float getFrequency() const { return freq; }   // 0 when unvoiced
    float getConfidence() const { return confidence; }

private:
    void analyse (const float* x)
    {
        // energy gate
        double e = 0;
        for (int j = 0; j < window; ++j) e += (double) x[j] * x[j];
        float rms = (float) std::sqrt (e / window);
        if (rms < gate) { freq = 0; confidence = 0; return; }

        // difference function
        for (int tau = 1; tau <= tauMax; ++tau)
        {
            float s = 0;
            const float* a = x;
            const float* b = x + tau;
            for (int j = 0; j < window; ++j) { float d = a[j] - b[j]; s += d * d; }
            diff[(size_t) tau] = s;
        }
        // cumulative mean normalised difference
        diff[0] = 1.0f;
        float running = 0;
        for (int tau = 1; tau <= tauMax; ++tau)
        {
            running += diff[(size_t) tau];
            diff[(size_t) tau] = running > 0 ? diff[(size_t) tau] * tau / running : 1.0f;
        }
        // absolute threshold
        int best = -1;
        for (int tau = tauMin; tau < tauMax; ++tau)
        {
            if (diff[(size_t) tau] < threshold)
            {
                while (tau + 1 < tauMax && diff[(size_t) tau + 1] < diff[(size_t) tau]) ++tau;
                best = tau; break;
            }
        }
        if (best < 0) { freq = 0; confidence = 0; return; }

        // parabolic interpolation
        float s0 = diff[(size_t) best - 1], s1 = diff[(size_t) best], s2 = diff[(size_t) best + 1];
        float denom = s0 + s2 - 2 * s1;
        float t = (float) best + (std::abs (denom) > 1e-9f ? 0.5f * (s0 - s2) / denom : 0.0f);
        freq = (float) (fs / t);
        confidence = 1.0f - s1;
    }

    double fs = 44100;
    int tauMin = 40, tauMax = 600, window = 600, bufLen = 1202, hop = 256;
    int writePos = 0, counter = 0, filled = 0;
    float threshold = 0.15f, gate = 0.003f;
    float freq = 0, confidence = 0;
    std::vector<float> buf, diff;
};

//==============================================================================
// Two-tap crossfaded delay-line pitch shifter (one per channel).
class PitchShifter
{
public:
    void prepare (double sampleRate)
    {
        fs = sampleRate;
        size = 1;
        while (size < (int) (0.030 * fs * 2 + 16)) size <<= 1;   // room for the longest grain
        line.assign ((size_t) size, 0.0f);
        mask = size - 1;
        w = 0; phase = 0;
        setWindowMs (30.0f);
    }

    // Grain length. Latency = half of this. Real-time safe.
    void setWindowMs (float ms) { winLen = (float) (std::min (ms, 30.0f) * 0.001 * fs); }

    float process (float in, float ratio)
    {
        line[(size_t) w] = in;
        phase += (1.0f - ratio) / winLen;
        phase -= std::floor (phase);

        float p2 = phase + 0.5f; if (p2 >= 1.0f) p2 -= 1.0f;
        float out = tap (phase) * gain (phase) + tap (p2) * gain (p2);
        w = (w + 1) & mask;
        return out;
    }

    int latencySamples() const { return (int) (winLen * 0.5f); }   // average grain delay

private:
    static float gain (float p) { float s = std::sin (3.14159265f * p); return s * s; }

    float tap (float p) const
    {
        float delay = 2.0f + p * winLen;
        float r = (float) w - delay;
        while (r < 0) r += (float) size;
        int i0 = (int) r;
        float f = r - (float) i0;
        // cubic (Hermite) interpolation
        float xm1 = line[(size_t) ((i0 - 1) & mask)], x0 = line[(size_t) (i0 & mask)];
        float x1 = line[(size_t) ((i0 + 1) & mask)], x2 = line[(size_t) ((i0 + 2) & mask)];
        float c1 = 0.5f * (x1 - xm1);
        float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
        float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
        return ((c3 * f + c2) * f + c1) * f + x0;
    }

    double fs = 44100;
    float winLen = 1323, phase = 0;
    int size = 4096, mask = 4095, w = 0;
    std::vector<float> line;
};

//==============================================================================
struct Params
{
    int key = 0;                     // 0 = C ... 11 = B
    Scale scale = Scale::Chromatic;
    float retuneMs = 0.0f;           // 0 = instant (hard "robot" effect)
    float amount = 1.0f;             // 0..1 correction strength
    float transpose = 0.0f;          // semitones
    float mix = 1.0f;                // dry/wet
    float humanize = 0.0f;           // 0..1: slows retune on sustained notes
    bool lowLatency = false;         // short grains + faster analysis
};

// Per-mode settings
constexpr float kNormalWindowMs = 30.0f, kLowWindowMs = 10.0f;
inline int latencyFor (bool lowLatency, double fs)
{
    return (int) ((lowLatency ? kLowWindowMs : kNormalWindowMs) * 0.001 * fs * 0.5);
}

// Full engine: detection + note snapping + smoothing + shifting for N channels.
class AutoTuneEngine
{
public:
    void prepare (double sampleRate, int numChannels)
    {
        fs = sampleRate;
        detector.prepare (fs);
        shifters.resize ((size_t) std::max (1, numChannels));
        for (auto& s : shifters) s.prepare (fs);
        dryDelay.assign ((size_t) std::max (1, numChannels), std::vector<float> (4096, 0.0f));
        dryPos = 0;
        shift = 0; targetShift = 0; lastTargetNote = -1; holdSamples = 0; noteAge = 0;
        configured = false;
        applyMode (params.lowLatency);
    }

    void setParams (const Params& p)
    {
        if (p.lowLatency != params.lowLatency || ! configured) applyMode (p.lowLatency);
        params = p; mask = scaleMask (p.scale);
    }

    int getLatencySamples() const { return latencyFor (params.lowLatency, fs); }

    // Interleaving-free: channels[ch][i]
    void process (float* const* channels, int numChannels, int numSamples)
    {
        const int lat = getLatencySamples();
        for (int i = 0; i < numSamples; ++i)
        {
            float mono = 0;
            for (int ch = 0; ch < numChannels; ++ch) mono += channels[ch][i];
            mono /= (float) numChannels;

            if (detector.push (mono))
                updateTarget (detector.getFrequency());

            // smoothing toward target shift
            float coeff = smoothingCoeff();
            shift += (targetShift - shift) * coeff;

            float semis = shift * params.amount + params.transpose;
            float ratio = std::pow (2.0f, semis / 12.0f);

            for (int ch = 0; ch < numChannels && ch < (int) shifters.size(); ++ch)
            {
                float dry = channels[ch][i];
                auto& dl = dryDelay[(size_t) ch];
                dl[(size_t) dryPos] = dry;
                float dryAligned = dl[(size_t) ((dryPos - lat) & 4095)];
                float wet = shifters[(size_t) ch].process (dry, ratio);
                channels[ch][i] = dryAligned + (wet - dryAligned) * params.mix;
            }
            dryPos = (dryPos + 1) & 4095;
            ++noteAge;
        }
    }

    float getDetectedHz() const { return lastHz; }
    int getTargetNote() const { return lastTargetNote; }
    float getCurrentShift() const { return shift; }

private:
    float smoothingCoeff() const
    {
        float ms = params.retuneMs;
        // humanize: sustained notes (> 250 ms) relax correction speed
        if (params.humanize > 0 && noteAge > (long) (0.25 * fs))
            ms += params.humanize * 200.0f;
        if (ms <= 0.5f) return 1.0f;
        return 1.0f - std::exp (-1.0f / (float) (ms * 0.001 * fs));
    }

    void updateTarget (float hz)
    {
        lastHz = hz;
        if (hz <= 0)
        {
            // unvoiced: hold the last correction briefly, then release
            if (holdSamples > 0) holdSamples -= detectorHop();
            else targetShift = 0;
            return;
        }
        holdSamples = (int) (0.08 * fs);

        float midi = 69.0f + 12.0f * std::log2 (hz / 440.0f);
        int note = nearestAllowed (midi);

        // hysteresis: keep the previous note unless clearly closer to another
        if (lastTargetNote >= 0 && note != lastTargetNote
            && std::abs (midi - (float) lastTargetNote) < std::abs (midi - (float) note) + 0.15f
            && isAllowed (lastTargetNote))
            note = lastTargetNote;

        if (note != lastTargetNote) noteAge = 0;
        lastTargetNote = note;
        targetShift = (float) note - midi;
    }

    bool isAllowed (int note) const { return mask[(size_t) (((note - params.key) % 12 + 12) % 12)]; }

    int nearestAllowed (float midi) const
    {
        int base = (int) std::lround (midi);
        for (int d = 0; d <= 6; ++d)
        {
            int lo = base - d, hi = base + d;
            bool a = isAllowed (lo), b = isAllowed (hi);
            if (a && b) return (std::abs (midi - lo) <= std::abs (midi - hi)) ? lo : hi;
            if (a) return lo;
            if (b) return hi;
        }
        return base;
    }

    void applyMode (bool low)
    {
        // Low latency: 10 ms grains (5 ms latency), 2.5 ms analysis hop, 80 Hz floor
        // so the detector looks back over a shorter stretch of audio.
        for (auto& s : shifters) s.setWindowMs (low ? kLowWindowMs : kNormalWindowMs);
        detector.setRange (low ? 80.0f : 70.0f, 1000.0f, low ? 2.5f : 5.0f);
        configured = true;
    }

    int detectorHop() const { return detector.getHop(); }

    double fs = 44100;
    Params params;
    std::array<bool, 12> mask = scaleMask (Scale::Chromatic);
    PitchDetector detector;
    std::vector<PitchShifter> shifters;
    std::vector<std::vector<float>> dryDelay;
    int dryPos = 0;
    float shift = 0, targetShift = 0, lastHz = 0;
    int lastTargetNote = -1, holdSamples = 0;
    long noteAge = 0;
    bool configured = false;
};

} // namespace zav
