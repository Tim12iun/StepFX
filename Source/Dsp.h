#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>
#include <array>
#include <cmath>

namespace sfx
{
inline float sanitize (float x) { return std::isfinite (x) ? juce::jlimit (-8.0f, 8.0f, x) : 0.0f; }
inline float fastTanh (float x) { return std::tanh (x); }

/** Balance style mix: at 50 % both dry and wet are at full level. */
inline void mixGains (float mix, float& dry, float& wet)
{
    dry = juce::jmin (1.0f, 2.0f * (1.0f - mix));
    wet = juce::jmin (1.0f, 2.0f * mix);
}

//==============================================================================
class DelayLine
{
public:
    void resize (int maxSamples)
    {
        size = juce::nextPowerOfTwo (juce::jmax (8, maxSamples + 4));
        mask = size - 1;
        buf.assign ((size_t) size, 0.0f);
        w = 0;
    }
    void clear() { std::fill (buf.begin(), buf.end(), 0.0f); }
    void push (float x) { buf[(size_t) w] = x; w = (w + 1) & mask; }
    float read (float delay) const
    {
        delay = juce::jlimit (1.0f, (float) (size - 3), delay);
        const float rp = (float) w - delay;
        const int i0 = (int) std::floor (rp);
        const float f = rp - (float) i0;
        const float a = buf[(size_t) (i0 & mask)], b = buf[(size_t) ((i0 + 1) & mask)];
        return a + f * (b - a);
    }
    int capacity() const { return size - 4; }

private:
    std::vector<float> buf;
    int size = 0, mask = 0, w = 0;
};

//==============================================================================
/** TPT state variable filter core. */
struct Svf
{
    float ic1 = 0, ic2 = 0;
    float g = 0.1f, k = 1.0f, a1 = 0, a2 = 0, a3 = 0;
    float lp = 0, bp = 0, hp = 0;

    void reset() { ic1 = ic2 = lp = bp = hp = 0; }
    void set (float freq, float kDamp, float sr)
    {
        freq = juce::jlimit (16.0f, sr * 0.45f, freq);
        g = std::tan (juce::MathConstants<float>::pi * freq / sr);
        k = kDamp;
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }
    void tick (float x)
    {
        const float v3 = x - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        lp = v2; bp = v1; hp = x - k * v1 - v2;
    }
};

//==============================================================================
namespace FilterTypes
{
    enum Family { LP, BP, HP, Notch, Peak, CombPos, CombNeg, CombPM, RingMod, Bitcrusher, Downsample, Mech, Tube };
    enum Variant { Smooth, Edgy, Rich, Sharp, Clean, Gritty };

    struct Info { const char* name; Family family; Variant variant; int stages; };

    inline const std::vector<Info>& all()
    {
        static const std::vector<Info> v = {
            { "Lowpass 12dB Smooth", LP, Smooth, 1 }, { "Lowpass 24dB Smooth", LP, Smooth, 2 },
            { "Lowpass 12dB Edgy", LP, Edgy, 1 },     { "Lowpass 24dB Edgy", LP, Edgy, 2 },
            { "Lowpass 12dB Rich", LP, Rich, 1 },     { "Lowpass 24dB Rich", LP, Rich, 2 },
            { "Lowpass 12dB Sharp", LP, Sharp, 1 },   { "Lowpass 12dB Clean", LP, Clean, 1 },
            { "Lowpass 12dB Gritty", LP, Gritty, 1 },
            { "Bandpass 12dB Smooth", BP, Smooth, 1 }, { "Bandpass 24dB Smooth", BP, Smooth, 2 },
            { "Bandpass 12dB Edgy", BP, Edgy, 1 },     { "Bandpass 24dB Edgy", BP, Edgy, 2 },
            { "Bandpass 12dB Rich", BP, Rich, 1 },     { "Bandpass 24dB Rich", BP, Rich, 2 },
            { "Bandpass 12dB Sharp", BP, Sharp, 1 },   { "Bandpass 12dB Clean", BP, Clean, 1 },
            { "Bandpass 12dB Gritty", BP, Gritty, 1 },
            { "Highpass 12dB Smooth", HP, Smooth, 1 }, { "Highpass 24dB Smooth", HP, Smooth, 2 },
            { "Highpass 12dB Edgy", HP, Edgy, 1 },     { "Highpass 24dB Edgy", HP, Edgy, 2 },
            { "Highpass 12dB Rich", HP, Rich, 1 },     { "Highpass 24dB Rich", HP, Rich, 2 },
            { "Highpass 12dB Sharp", HP, Sharp, 1 },   { "Highpass 12dB Clean", HP, Clean, 1 },
            { "Highpass 12dB Gritty", HP, Gritty, 1 },
            { "Notch", Notch, Clean, 1 }, { "Peaking", Peak, Clean, 1 },
            { "Comb Pos", CombPos, Clean, 1 }, { "Comb Neg", CombNeg, Clean, 1 }, { "Comb PM", CombPM, Clean, 1 },
            { "RingMod", RingMod, Clean, 1 },
            { "Bitcrusher", Bitcrusher, Clean, 1 }, { "Downsample", Downsample, Clean, 1 },
            { "Mech", Mech, Clean, 1 }, { "Tube", Tube, Clean, 1 }
        };
        return v;
    }

    inline int defaultIndex() { return 3; } // Lowpass 24dB Edgy

    /** Knob captions per filter family (empty = knob unused). */
    inline std::array<const char*, 3> knobNames (int typeIndex)
    {
        const auto& t = all()[(size_t) juce::jlimit (0, (int) all().size() - 1, typeIndex)];
        switch (t.family)
        {
            case LP: case BP: case HP: return { "Cutoff", "Res", t.variant == Clean ? "" : "Drive" };
            case Notch:   return { "Cutoff", "Amount", "" };
            case Peak:    return { "Cutoff", "Gain", "BW" };
            case CombPos: case CombNeg: case CombPM: return { "Cutoff", "F-Back", "Damp" };
            case RingMod: return { "Frequency", "Mix", "" };
            case Downsample: return { "Freq", "Amount", "Mix" };
            case Bitcrusher: case Mech: case Tube: return { "", "Amount", "Mix" };
        }
        return { "Cutoff", "Res", "Drive" };
    }
}

class MultiFilter
{
public:
    void prepare (double sampleRate)
    {
        sr = (float) sampleRate;
        for (auto& c : comb) c.resize ((int) (sr * 0.07f));
        reset();
    }

    void reset()
    {
        for (auto& ch : svf) for (auto& s : ch) s.reset();
        for (auto& c : comb) c.clear();
        combLp = { 0, 0 }; hold = { 0, 0 }; holdPhase = { 1.0f, 1.0f };
        ringPhase = 0; combDelaySm = 0;
    }

    /** freq in Hz, p2/p3 = 0..1 */
    void setParams (int typeIndex, float freqHz, float p2In, float p3In)
    {
        const auto& all = FilterTypes::all();
        info = all[(size_t) juce::jlimit (0, (int) all.size() - 1, typeIndex)];
        freq = juce::jlimit (20.0f, juce::jmin (20000.0f, sr * 0.45f), freqHz);
        p2 = juce::jlimit (0.0f, 1.0f, p2In);
        p3 = juce::jlimit (0.0f, 1.0f, p3In);

        using namespace FilterTypes;
        float k = 1.0f;
        switch (info.family)
        {
            case LP: case BP: case HP:
            {
                float kMin = 0.06f;
                if (info.variant == Edgy || info.variant == Gritty || info.variant == Rich) kMin = 0.0f; // self oscillating
                if (info.variant == Sharp) kMin = 0.012f;
                const float resCurve = info.variant == Sharp ? std::pow (p2, 0.6f) : p2;
                k = 2.0f - (2.0f - kMin) * std::pow (resCurve, 0.8f);
                if (info.stages == 2) k = juce::jmax (k, kMin + 0.02f) * 0.5f + k * 0.5f;
                break;
            }
            case Notch: k = 0.35f; break;
            case Peak:  k = 1.0f / (0.5f + (1.0f - p3) * (1.0f - p3) * 19.5f); break; // BW: wider to the right
            default: break;
        }
        for (auto& ch : svf) for (auto& s : ch) s.set (freq, k, sr);
    }

    void process (float* l, float* r, int n)
    {
        using namespace FilterTypes;
        float* io[2] = { l, r };
        switch (info.family)
        {
            case LP: case BP: case HP: processSvf (io, n); break;
            case Notch:
                for (int c = 0; c < 2; ++c)
                    for (int i = 0; i < n; ++i)
                    {
                        auto& s = svf[(size_t) c][0];
                        s.tick (io[c][i]);
                        io[c][i] = sanitize (io[c][i] - p2 * s.k * s.bp);
                    }
                break;
            case Peak:
            {
                const float gain = juce::Decibels::decibelsToGain (p2 * 24.0f) - 1.0f;
                for (int c = 0; c < 2; ++c)
                    for (int i = 0; i < n; ++i)
                    {
                        auto& s = svf[(size_t) c][0];
                        s.tick (io[c][i]);
                        io[c][i] = sanitize (io[c][i] + gain * s.k * s.bp);
                    }
                break;
            }
            case CombPos: case CombNeg: case CombPM: processComb (io, n); break;
            case RingMod:
            {
                const float inc = juce::MathConstants<float>::twoPi * freq / sr;
                for (int i = 0; i < n; ++i)
                {
                    const float c = std::sin (ringPhase);
                    ringPhase += inc;
                    if (ringPhase > juce::MathConstants<float>::twoPi) ringPhase -= juce::MathConstants<float>::twoPi;
                    const float carrier = (1.0f - p2) * c + p2 * (0.5f + 0.5f * c);
                    l[i] *= carrier; r[i] *= carrier;
                }
                break;
            }
            case Bitcrusher:
            {
                const float bits = 16.0f - p2 * 14.5f;
                const float q = std::pow (2.0f, bits - 1.0f);
                for (int c = 0; c < 2; ++c)
                    for (int i = 0; i < n; ++i)
                    {
                        const float x = io[c][i];
                        const float w = std::round (x * q) / q;
                        io[c][i] = x + p3 * (w - x);
                    }
                break;
            }
            case Downsample:
            {
                // hold rate glides from the sample rate down to "Freq" with Amount
                const float rate = sr * std::pow (juce::jlimit (0.0005f, 1.0f, freq / sr), p2);
                const float inc = juce::jlimit (0.0001f, 1.0f, rate / sr);
                for (int c = 0; c < 2; ++c)
                    for (int i = 0; i < n; ++i)
                    {
                        holdPhase[(size_t) c] += inc;
                        if (holdPhase[(size_t) c] >= 1.0f) { holdPhase[(size_t) c] -= 1.0f; hold[(size_t) c] = io[c][i]; }
                        io[c][i] += p3 * (hold[(size_t) c] - io[c][i]);
                    }
                break;
            }
            case Mech:
            {
                const float drive = 1.0f + p2 * 14.0f;
                for (int c = 0; c < 2; ++c)
                    for (int i = 0; i < n; ++i)
                    {
                        const float x = io[c][i];
                        const float w = std::sin (x * drive * juce::MathConstants<float>::halfPi) * (1.0f / (1.0f + p2 * 0.5f));
                        io[c][i] = x + p3 * (w - x);
                    }
                break;
            }
            case Tube:
            {
                const float drive = 1.0f + p2 * 24.0f;
                const float bias = 0.25f * p2;
                const float comp = 1.0f / std::pow (drive, 0.55f);
                const float off = fastTanh (bias);
                for (int c = 0; c < 2; ++c)
                    for (int i = 0; i < n; ++i)
                    {
                        const float x = io[c][i];
                        const float w = (fastTanh (x * drive + bias) - off) * comp;
                        io[c][i] = x + p3 * (w - x);
                    }
                break;
            }
        }
    }

private:
    void processSvf (float** io, int n)
    {
        using namespace FilterTypes;
        const bool clean = info.variant == Clean;
        const float drive = clean ? 1.0f : 1.0f + p3 * p3 * 15.0f;
        const float comp = 1.0f / std::sqrt (drive);
        for (int c = 0; c < 2; ++c)
        {
            for (int i = 0; i < n; ++i)
            {
                float x = io[c][i] * drive;
                switch (info.variant)
                {
                    case Smooth: if (drive > 1.001f) x = fastTanh (x); break;
                    case Edgy:   x = fastTanh (x); break;
                    case Rich:   x = fastTanh (x + 0.18f * x * x) ; break;
                    case Sharp:  if (drive > 1.001f) x = fastTanh (x); break;
                    case Gritty: x = juce::jlimit (-1.0f, 1.0f, x * (1.0f + p2 * 2.0f)); break;
                    case Clean:  break;
                }
                for (int st = 0; st < info.stages; ++st)
                {
                    auto& s = svf[(size_t) c][(size_t) st];
                    s.tick (x);
                    // keep the integrator states bounded: this is what limits self oscillation
                    if (info.variant == Gritty) { s.ic1 = juce::jlimit (-1.5f, 1.5f, s.ic1); s.ic2 = juce::jlimit (-1.5f, 1.5f, s.ic2); }
                    else if (! clean)           { s.ic1 = 2.0f * fastTanh (s.ic1 * 0.5f); s.ic2 = 2.0f * fastTanh (s.ic2 * 0.5f); }
                    else                        { s.ic1 = juce::jlimit (-16.0f, 16.0f, s.ic1); s.ic2 = juce::jlimit (-16.0f, 16.0f, s.ic2); }
                    x = info.family == LP ? s.lp : (info.family == BP ? s.bp * juce::jmax (0.5f, s.k) : s.hp);
                }
                io[c][i] = sanitize (x * comp);
            }
        }
    }

    void processComb (float** io, int n)
    {
        using namespace FilterTypes;
        float fb = p2 * 0.97f;
        if (info.family == CombNeg) fb = -fb;
        if (info.family == CombPM) fb = (p2 * 2.0f - 1.0f) * 0.97f;
        const float target = juce::jlimit (2.0f, (float) comb[0].capacity(), sr / freq);
        if (combDelaySm <= 0.0f) combDelaySm = target;
        const float dampCoef = 1.0f - p3 * 0.92f; // 1 = no damping
        const float norm = 1.0f - 0.5f * std::abs (fb);
        for (int i = 0; i < n; ++i)
        {
            combDelaySm += 0.002f * (target - combDelaySm);
            for (int c = 0; c < 2; ++c)
            {
                const float d = comb[(size_t) c].read (combDelaySm);
                combLp[(size_t) c] += dampCoef * (d - combLp[(size_t) c]);
                const float y = sanitize (io[c][i] + fb * combLp[(size_t) c]);
                comb[(size_t) c].push (y);
                io[c][i] = y * norm;
            }
        }
    }

    float sr = 44100.0f;
    FilterTypes::Info info { "Lowpass 24dB Edgy", FilterTypes::LP, FilterTypes::Edgy, 2 };
    float freq = 1000.0f, p2 = 0, p3 = 0;
    std::array<std::array<Svf, 2>, 2> svf;
    std::array<DelayLine, 2> comb;
    std::array<float, 2> combLp { 0, 0 }, hold { 0, 0 }, holdPhase { 1.0f, 1.0f };
    float ringPhase = 0, combDelaySm = 0;
};

//==============================================================================
class ModFx
{
public:
    void prepare (double sampleRate)
    {
        sr = (float) sampleRate;
        for (auto& d : dl) d.resize ((int) (sr * 0.08f));
        reset();
    }
    void reset() { for (auto& d : dl) d.clear(); phase = 0; fbState = { 0, 0 }; delaySm = 0; }

    void setParams (float delayMs, float rateHz, float depth01, float feedback01, float mix01)
    {
        baseDelay = juce::jlimit (0.1f, 30.0f, delayMs) * 0.001f * sr;
        rate = juce::jlimit (0.005f, 25.0f, rateHz);
        depth = juce::jlimit (0.0f, 1.0f, depth01);
        feedback = juce::jlimit (0.0f, 1.0f, feedback01) * 0.94f;
        mix = juce::jlimit (0.0f, 1.0f, mix01);
    }

    void process (float* l, float* r, int n)
    {
        float* io[2] = { l, r };
        const float inc = rate / sr;
        float dry, wet;
        mixGains (mix, dry, wet);
        if (delaySm <= 0.0f) delaySm = baseDelay;
        for (int i = 0; i < n; ++i)
        {
            delaySm += 0.001f * (baseDelay - delaySm);
            phase += inc;
            if (phase >= 1.0f) phase -= 1.0f;
            for (int c = 0; c < 2; ++c)
            {
                const float ph = phase + (c == 1 ? 0.25f : 0.0f);
                const float lfo = std::sin (juce::MathConstants<float>::twoPi * ph);
                const float d = juce::jmax (1.0f, delaySm * (1.0f + depth * 0.95f * lfo));
                const float x = io[c][i];
                const float w = dl[(size_t) c].read (d);
                dl[(size_t) c].push (sanitize (x + fastTanh (w * feedback)));
                io[c][i] = x * dry + w * wet;
            }
        }
    }

private:
    float sr = 44100.0f, baseDelay = 400.0f, rate = 0.5f, depth = 0.5f, feedback = 0.0f, mix = 0.5f;
    float phase = 0, delaySm = 0;
    std::array<DelayLine, 2> dl;
    std::array<float, 2> fbState { 0, 0 };
};

//==============================================================================
class StereoDelay
{
public:
    enum FilterType { LPF, BPF, HPF, LPLoCut };

    void prepare (double sampleRate)
    {
        sr = (float) sampleRate;
        for (auto& d : dl) d.resize ((int) (sr * maxSeconds));
        reset();
    }
    void reset()
    {
        for (auto& d : dl) d.clear();
        for (auto& f : filt) f.reset();
        for (auto& f : loCut) f.reset();
        timeSm = { 0, 0 };
    }

    void setParams (float secondsL, float secondsR, float feedback01, int filterType, float cutoffHz, float mix01)
    {
        time[0] = juce::jlimit (0.001f, maxSeconds, secondsL) * sr;
        time[1] = juce::jlimit (0.001f, maxSeconds, secondsR) * sr;
        feedback = juce::jlimit (0.0f, 1.0f, feedback01) * 0.97f;
        type = filterType;
        mix = juce::jlimit (0.0f, 1.0f, mix01);
        for (auto& f : filt) f.set (cutoffHz, type == BPF ? 0.9f : 1.414f, sr);
        for (auto& f : loCut) f.set (220.0f, 1.414f, sr);
    }

    void process (float* l, float* r, int n)
    {
        float* io[2] = { l, r };
        float dry, wet;
        mixGains (mix, dry, wet);
        for (int c = 0; c < 2; ++c)
        {
            auto ci = (size_t) c;
            if (timeSm[ci] <= 0.0f) timeSm[ci] = time[c];
            for (int i = 0; i < n; ++i)
            {
                timeSm[ci] += 0.0006f * (time[c] - timeSm[ci]);
                const float x = io[c][i];
                float w = dl[ci].read (timeSm[ci]);
                filt[ci].tick (w);
                switch (type)
                {
                    case BPF: w = filt[ci].bp * filt[ci].k; break;
                    case HPF: w = filt[ci].hp; break;
                    case LPLoCut: loCut[ci].tick (filt[ci].lp); w = loCut[ci].hp; break;
                    default: w = filt[ci].lp; break;
                }
                w = sanitize (w);
                dl[ci].push (sanitize (x + fastTanh (w * feedback)));
                io[c][i] = x * dry + w * wet;
            }
        }
    }

    static constexpr float maxSeconds = 8.0f;

private:
    float sr = 44100.0f, feedback = 0.3f, mix = 0.3f;
    float time[2] = { 11025.0f, 11025.0f };
    int type = LPF;
    std::array<float, 2> timeSm { 0, 0 };
    std::array<DelayLine, 2> dl;
    std::array<Svf, 2> filt, loCut;
};

//==============================================================================
class Exciter
{
public:
    void prepare (double sampleRate) { sr = (float) sampleRate; reset(); }
    void reset() { for (auto& f : pre) f.reset(); for (auto& f : post) f.reset(); }
    void setAmount (float a)
    {
        amount = juce::jlimit (0.0f, 1.0f, a);
        for (auto& f : pre) f.set (2800.0f, 1.414f, sr);
        for (auto& f : post) f.set (3500.0f, 1.414f, sr);
    }
    void process (float* l, float* r, int n)
    {
        if (amount <= 0.0001f) return;
        float* io[2] = { l, r };
        const float drive = 2.0f + amount * 10.0f;
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < n; ++i)
            {
                auto ci = (size_t) c;
                pre[ci].tick (io[c][i]);
                const float h = pre[ci].hp * drive;
                const float shaped = fastTanh (h + 0.35f * std::abs (h)); // asymmetric: even and odd harmonics
                post[ci].tick (shaped);
                io[c][i] = sanitize (io[c][i] + post[ci].hp * amount * 0.6f);
            }
    }

private:
    float sr = 44100.0f, amount = 0;
    std::array<Svf, 2> pre, post;
};

class Dirt
{
public:
    void prepare (double) { reset(); }
    void reset() { dc = { 0, 0 }; dcIn = { 0, 0 }; }
    void setAmount (float a) { amount = juce::jlimit (0.0f, 1.0f, a); }
    void process (float* l, float* r, int n)
    {
        if (amount <= 0.0001f) return;
        float* io[2] = { l, r };
        const float drive = 1.0f + amount * amount * 39.0f;
        const float bias = 0.12f * amount;
        const float off = fastTanh (bias);
        const float comp = 1.0f / std::pow (drive, 0.6f);
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < n; ++i)
            {
                auto ci = (size_t) c;
                const float y = (fastTanh (io[c][i] * drive + bias) - off) * comp;
                // DC blocker
                dc[ci] = y - dcIn[ci] + 0.9995f * dc[ci];
                dcIn[ci] = y;
                io[c][i] = sanitize (dc[ci]);
            }
    }

private:
    float amount = 0;
    std::array<float, 2> dc { 0, 0 }, dcIn { 0, 0 };
};

//==============================================================================
/** 8 line feedback delay network reverb with RT60 control. */
class FdnReverb
{
public:
    static juce::StringArray typeNames() { return { "Room", "Chamber", "Hall", "Plate", "Cathedral", "Space" }; }
    static int defaultType() { return 2; }

    void prepare (double sampleRate)
    {
        sr = (float) sampleRate;
        for (auto& d : lines) d.resize ((int) (sr * 0.25f));
        for (auto& d : pre) d.resize ((int) (sr * 0.12f));
        for (auto& a : ap) a.resize ((int) (sr * 0.02f));
        reset();
        setParams (currentType, rt60, mix);
    }

    void reset()
    {
        for (auto& d : lines) d.clear();
        for (auto& d : pre) d.clear();
        for (auto& a : ap) a.clear();
        damp.fill (0.0f);
    }

    void setParams (int type, float seconds, float mix01)
    {
        struct T { float size, damping, predelayMs, diffusion; };
        static const T types[] = {
            { 0.35f, 0.45f, 2.0f, 0.55f },   // Room
            { 0.55f, 0.35f, 8.0f, 0.62f },   // Chamber
            { 1.00f, 0.40f, 18.0f, 0.70f },  // Hall
            { 0.45f, 0.12f, 1.0f, 0.75f },   // Plate
            { 1.55f, 0.55f, 35.0f, 0.72f },  // Cathedral
            { 1.90f, 0.20f, 50.0f, 0.78f }   // Space
        };
        currentType = juce::jlimit (0, 5, type);
        const auto& t = types[currentType];
        rt60 = juce::jlimit (0.1f, 20.0f, seconds);
        mix = juce::jlimit (0.0f, 1.0f, mix01);
        static const float baseMs[8] = { 29.7f, 37.1f, 41.1f, 43.7f, 53.3f, 59.9f, 67.3f, 73.1f };
        for (int i = 0; i < 8; ++i)
        {
            len[(size_t) i] = juce::jlimit (8.0f, (float) lines[0].capacity(), baseMs[i] * 0.001f * sr * t.size);
            gain[(size_t) i] = std::pow (10.0f, -3.0f * len[(size_t) i] / (rt60 * sr));
        }
        dampCoef = 1.0f - t.damping * 0.85f;
        preDelay = juce::jmax (1.0f, t.predelayMs * 0.001f * sr);
        diffusion = t.diffusion;
    }

    void process (float* l, float* r, int n)
    {
        float dry, wet;
        mixGains (mix, dry, wet);
        static const float apMs[4] = { 4.77f, 3.59f, 12.73f, 9.31f };
        for (int i = 0; i < n; ++i)
        {
            float in[2] = { l[i], r[i] };
            for (int c = 0; c < 2; ++c)
            {
                auto ci = (size_t) c;
                pre[ci].push (in[c]);
                float x = pre[ci].read (preDelay);
                for (int a = 0; a < 2; ++a)
                {
                    auto& line = ap[(size_t) (c * 2 + a)];
                    const float d = line.read (apMs[c * 2 + a] * 0.001f * sr);
                    const float v = x + diffusion * d;
                    line.push (sanitize (v));
                    x = d - diffusion * v;
                }
                in[c] = x;
            }

            float out[8], sum = 0;
            for (int k = 0; k < 8; ++k)
            {
                auto ki = (size_t) k;
                const float d = lines[ki].read (len[ki]);
                damp[ki] += dampCoef * (d - damp[ki]);
                out[k] = damp[ki] * gain[ki];
                sum += out[k];
            }
            sum *= 0.25f; // Householder: x - (2/N) * sum
            for (int k = 0; k < 8; ++k)
                lines[(size_t) k].push (sanitize (out[k] - sum + in[k & 1] * 0.5f));

            const float wl = (out[0] - out[2] + out[4] - out[6]) * 0.5f;
            const float wr = (out[1] - out[3] + out[5] - out[7]) * 0.5f;
            l[i] = l[i] * dry + wl * wet;
            r[i] = r[i] * dry + wr * wet;
        }
    }

private:
    float sr = 44100.0f, rt60 = 2.0f, mix = 0.2f, dampCoef = 0.6f, preDelay = 100.0f, diffusion = 0.7f;
    int currentType = 2;
    std::array<DelayLine, 8> lines;
    std::array<DelayLine, 2> pre;
    std::array<DelayLine, 4> ap;
    std::array<float, 8> len {}, gain {}, damp {};
};
} // namespace sfx
