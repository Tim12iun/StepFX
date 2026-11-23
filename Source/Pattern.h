#pragma once
#include <juce_data_structures/juce_data_structures.h>
#include <array>
#include <atomic>
#include <cmath>

namespace sfx
{
constexpr int kMaxSteps = 128;
constexpr int kNumMods = 3;

/** Plain pattern data: one step lane of a modulator. */
struct PatternData
{
    int numSteps = 16;
    std::array<float, kMaxSteps> level {};   // 0..1
    std::array<float, kMaxSteps> length {};  // 0.05..1 (fraction of a step)
    std::array<bool, kMaxSteps> on {};
    std::array<bool, kMaxSteps> tie {};      // step i is tied to step i+1
    // envelope, fractions of the note length
    float attack = 0.0f, hold = 1.0f, release = 0.0f;
    float attackCurve = 0.0f, releaseCurve = 0.0f; // -1..1

    PatternData() { reset(); }

    void reset()
    {
        numSteps = 16;
        level.fill (1.0f);
        length.fill (1.0f);
        on.fill (true);
        tie.fill (false);
        attack = 0.0f; hold = 1.0f; release = 0.0f;
        attackCurve = releaseCurve = 0.0f;
    }

    void sanitise()
    {
        numSteps = juce::jlimit (1, kMaxSteps, numSteps);
        for (int i = 0; i < kMaxSteps; ++i)
        {
            level[(size_t) i] = std::isfinite (level[(size_t) i]) ? juce::jlimit (0.0f, 1.0f, level[(size_t) i]) : 1.0f;
            length[(size_t) i] = std::isfinite (length[(size_t) i]) ? juce::jlimit (0.05f, 1.0f, length[(size_t) i]) : 1.0f;
        }
        auto fix = [] (float v, float lo, float hi, float def) { return std::isfinite (v) ? juce::jlimit (lo, hi, v) : def; };
        attack = fix (attack, 0.0f, 1.0f, 0.0f);
        hold = fix (hold, 0.0f, 1.0f, 1.0f);
        release = fix (release, 0.0f, 1.0f, 0.0f);
        attackCurve = fix (attackCurve, -1.0f, 1.0f, 0.0f);
        releaseCurve = fix (releaseCurve, -1.0f, 1.0f, 0.0f);
    }

    //==========================================================================
    // Action menu operations
    void randomFull (juce::Random& r)
    {
        for (int i = 0; i < numSteps; ++i)
            level[(size_t) i] = r.nextFloat();
    }

    void randomDeviation (juce::Random& r, float amount = 0.15f)
    {
        for (int i = 0; i < numSteps; ++i)
            level[(size_t) i] = juce::jlimit (0.0f, 1.0f, level[(size_t) i] + (r.nextFloat() * 2.0f - 1.0f) * amount);
    }

    void shift (int dir) // +1 = right, -1 = left, wraps around
    {
        if (numSteps < 2) return;
        PatternData src = *this;
        for (int i = 0; i < numSteps; ++i)
        {
            const int from = ((i - dir) % numSteps + numSteps) % numSteps;
            copyStep (src, from, i);
        }
    }

    void doubleNoteLength()
    {
        for (int i = 0; i < numSteps; ++i)
        {
            const auto idx = (size_t) i;
            if (! on[idx] || tie[idx]) continue;
            if (length[idx] > 0.5f)
            {
                length[idx] = 1.0f;
                if (i + 1 < numSteps) { tie[idx] = true; on[idx + 1] = true; length[idx + 1] = 1.0f; ++i; }
            }
            else
                length[idx] = juce::jmin (1.0f, length[idx] * 2.0f);
        }
    }

    void reverse()
    {
        const PatternData src = *this;
        const int n = numSteps;
        for (int i = 0; i < n; ++i)
        {
            const auto to = (size_t) i, from = (size_t) (n - 1 - i);
            level[to] = src.level[from];
            length[to] = src.length[from];
            on[to] = src.on[from];
            // a tie from->from+1 becomes a tie (i-1)->i, i.e. the tie flag of the previous source step
            tie[to] = from > 0 ? src.tie[from - 1] : false;
        }
    }

    void invert()
    {
        for (int i = 0; i < numSteps; ++i)
            level[(size_t) i] = 1.0f - level[(size_t) i];
    }

    void appendDuplicate (bool reversed)
    {
        const int n = numSteps;
        const int total = juce::jmin (kMaxSteps, n * 2);
        PatternData src = *this;
        if (reversed) { src.reverse(); }
        tie[(size_t) (n - 1)] = false;
        for (int i = n; i < total; ++i)
            copyStep (src, i - n, i);
        numSteps = total;
        tie[(size_t) (total - 1)] = false;
    }

    void copyStep (const PatternData& src, int from, int to)
    {
        level[(size_t) to] = src.level[(size_t) from];
        length[(size_t) to] = src.length[(size_t) from];
        on[(size_t) to] = src.on[(size_t) from];
        tie[(size_t) to] = src.tie[(size_t) from];
    }

    //==========================================================================
    juce::ValueTree toValueTree (const juce::Identifier& type) const
    {
        juce::ValueTree t (type);
        t.setProperty ("numSteps", numSteps, nullptr);
        juce::StringArray lv, ln;
        juce::String onS, tieS;
        for (int i = 0; i < kMaxSteps; ++i)
        {
            lv.add (juce::String (level[(size_t) i], 4));
            ln.add (juce::String (length[(size_t) i], 4));
            onS << (on[(size_t) i] ? '1' : '0');
            tieS << (tie[(size_t) i] ? '1' : '0');
        }
        t.setProperty ("levels", lv.joinIntoString (" "), nullptr);
        t.setProperty ("lengths", ln.joinIntoString (" "), nullptr);
        t.setProperty ("on", onS, nullptr);
        t.setProperty ("tie", tieS, nullptr);
        t.setProperty ("attack", attack, nullptr);
        t.setProperty ("hold", hold, nullptr);
        t.setProperty ("release", release, nullptr);
        t.setProperty ("attackCurve", attackCurve, nullptr);
        t.setProperty ("releaseCurve", releaseCurve, nullptr);
        return t;
    }

    static PatternData fromValueTree (const juce::ValueTree& t)
    {
        PatternData p;
        if (! t.isValid()) return p;
        p.numSteps = (int) t.getProperty ("numSteps", 16);
        auto lv = juce::StringArray::fromTokens (t.getProperty ("levels").toString(), " ", "");
        auto ln = juce::StringArray::fromTokens (t.getProperty ("lengths").toString(), " ", "");
        auto onS = t.getProperty ("on").toString();
        auto tieS = t.getProperty ("tie").toString();
        for (int i = 0; i < kMaxSteps; ++i)
        {
            if (i < lv.size()) p.level[(size_t) i] = lv[i].getFloatValue();
            if (i < ln.size()) p.length[(size_t) i] = ln[i].getFloatValue();
            if (i < onS.length()) p.on[(size_t) i] = onS[i] == '1';
            if (i < tieS.length()) p.tie[(size_t) i] = tieS[i] == '1';
        }
        p.attack = (float) (double) t.getProperty ("attack", 0.0);
        p.hold = (float) (double) t.getProperty ("hold", 1.0);
        p.release = (float) (double) t.getProperty ("release", 0.0);
        p.attackCurve = (float) (double) t.getProperty ("attackCurve", 0.0);
        p.releaseCurve = (float) (double) t.getProperty ("releaseCurve", 0.0);
        p.sanitise();
        return p;
    }
};

//==============================================================================
/** Lock-free pattern shared between the UI thread (writer) and audio thread (reader). */
class SharedPattern
{
public:
    SharedPattern() { store (PatternData()); }

    void store (const PatternData& p)
    {
        for (int i = 0; i < kMaxSteps; ++i)
        {
            level[(size_t) i].store (p.level[(size_t) i], std::memory_order_relaxed);
            length[(size_t) i].store (p.length[(size_t) i], std::memory_order_relaxed);
            on[(size_t) i].store (p.on[(size_t) i], std::memory_order_relaxed);
            tie[(size_t) i].store (p.tie[(size_t) i], std::memory_order_relaxed);
        }
        attack.store (p.attack); hold.store (p.hold); release.store (p.release);
        attackCurve.store (p.attackCurve); releaseCurve.store (p.releaseCurve);
        numSteps.store (juce::jlimit (1, kMaxSteps, p.numSteps));
    }

    PatternData load() const
    {
        PatternData p;
        p.numSteps = numSteps.load();
        for (int i = 0; i < kMaxSteps; ++i)
        {
            p.level[(size_t) i] = level[(size_t) i].load (std::memory_order_relaxed);
            p.length[(size_t) i] = length[(size_t) i].load (std::memory_order_relaxed);
            p.on[(size_t) i] = on[(size_t) i].load (std::memory_order_relaxed);
            p.tie[(size_t) i] = tie[(size_t) i].load (std::memory_order_relaxed);
        }
        p.attack = attack.load(); p.hold = hold.load(); p.release = release.load();
        p.attackCurve = attackCurve.load(); p.releaseCurve = releaseCurve.load();
        return p;
    }

private:
    std::atomic<int> numSteps { 16 };
    std::array<std::atomic<float>, kMaxSteps> level, length;
    std::array<std::atomic<bool>, kMaxSteps> on, tie;
    std::atomic<float> attack { 0 }, hold { 1 }, release { 0 }, attackCurve { 0 }, releaseCurve { 0 };
};

//==============================================================================
/** Timing helper: a pattern prepared for playback (notes resolved from ties, swing applied). */
struct PlaybackPattern
{
    PatternData p;
    float swing = 0.0f; // 0..1
    std::array<int, kMaxSteps> noteStart {};      // -1 = rest
    std::array<float, kMaxSteps> noteStartTime {};
    std::array<float, kMaxSteps> noteDur {};

    float pairSplit() const { return 0.5f + 0.25f * swing; } // fraction of a step pair used by the odd step

    float stepStartTime (int k) const
    {
        if ((k & 1) == 0) return (float) k;
        return (float) (k - 1) + 2.0f * pairSplit();
    }

    float stepDur (int k) const
    {
        const bool hasPartner = (k & 1) ? true : (k + 1 < p.numSteps);
        if (! hasPartner) return 1.0f;
        return (k & 1) ? 2.0f - 2.0f * pairSplit() : 2.0f * pairSplit();
    }

    void prepare (const PatternData& src, float swingAmount)
    {
        p = src;
        swing = juce::jlimit (0.0f, 1.0f, swingAmount);
        const int n = p.numSteps;
        int i = 0;
        while (i < n)
        {
            if (! p.on[(size_t) i]) { noteStart[(size_t) i] = -1; ++i; continue; }
            int j = i;
            while (j + 1 < n && p.tie[(size_t) j] && p.on[(size_t) (j + 1)]) ++j;
            const float start = stepStartTime (i);
            const float dur = stepStartTime (j) - start + p.length[(size_t) j] * stepDur (j);
            for (int s = i; s <= j; ++s)
            {
                noteStart[(size_t) s] = i;
                noteStartTime[(size_t) s] = start;
                noteDur[(size_t) s] = dur;
            }
            i = j + 1;
        }
    }

    /** x = position in step units, 0..numSteps. */
    int stepAt (float x) const
    {
        const int n = p.numSteps;
        int pair = ((int) std::floor (x * 0.5f)) * 2;
        pair = juce::jlimit (0, juce::jmax (0, n - 1), pair);
        if (pair + 1 >= n) return pair;
        return (x - (float) pair) < 2.0f * pairSplit() ? pair : pair + 1;
    }

    static float curve (float t, float c)
    {
        if (std::abs (c) < 0.001f) return t;
        return std::pow (juce::jlimit (0.0f, 1.0f, t), std::pow (2.0f, -c * 2.5f));
    }

    float envelope (float t) const // t = 0..1 across the note
    {
        if (t < 0.0f || t >= 1.0f) return 0.0f;
        float a = p.attack, h = p.hold, r = p.release;
        const float sum = a + h + r;
        if (sum > 1.0f) { a /= sum; h /= sum; r /= sum; }
        if (t < a) return curve (t / a, p.attackCurve);
        if (t < a + h) return 1.0f;
        if (t < a + h + r) return 1.0f - curve ((t - a - h) / r, -p.releaseCurve);
        return 0.0f;
    }

    struct Value { float value = 0.0f; float level = 0.0f; float env = 0.0f; int step = 0; };

    Value evaluate (float x) const
    {
        Value v;
        v.step = stepAt (x);
        const int ns = noteStart[(size_t) v.step];
        if (ns < 0) return v;
        const float t = (x - noteStartTime[(size_t) v.step]) / juce::jmax (1.0e-4f, noteDur[(size_t) v.step]);
        v.level = p.level[(size_t) ns];
        v.env = envelope (t);
        v.value = v.level * v.env;
        return v;
    }
};

//==============================================================================
/** Own factory patterns (generated; none are copied from any other product). */
struct FactoryPatterns
{
    static juce::StringArray names()
    {
        return { "Default", "Ramp Up", "Ramp Down", "Triangle", "Sine", "Square", "Pulse 25%", "Stairs 4",
                 "Offbeat Gate", "Trance Gate A", "Trance Gate B", "Dotted Chop", "Long Short", "Swell 32",
                 "Random Walk", "Accent 3-3-2", "Stutter Fill", "Half Time Pump" };
    }

    static PatternData get (int index)
    {
        PatternData p;
        auto setAll = [&p] (int n, auto&& fn)
        {
            p.numSteps = n;
            for (int i = 0; i < n; ++i) p.level[(size_t) i] = juce::jlimit (0.0f, 1.0f, (float) fn (i));
        };
        const float pi = juce::MathConstants<float>::pi;
        switch (index)
        {
            default:
            case 0: break;
            case 1: setAll (16, [] (int i) { return (float) i / 15.0f; }); break;
            case 2: setAll (16, [] (int i) { return 1.0f - (float) i / 15.0f; }); break;
            case 3: setAll (16, [] (int i) { return i <= 8 ? (float) i / 8.0f : (float) (16 - i) / 8.0f; }); break;
            case 4: setAll (16, [pi] (int i) { return 0.5f - 0.5f * std::cos (2.0f * pi * (float) i / 16.0f); }); break;
            case 5: setAll (16, [] (int i) { return (i % 8) < 4 ? 1.0f : 0.0f; }); break;
            case 6: setAll (16, [] (int i) { return (i % 4) == 0 ? 1.0f : 0.0f; }); break;
            case 7: setAll (16, [] (int i) { return (float) (i / 4 + 1) / 4.0f; }); break;
            case 8:
                setAll (16, [] (int) { return 1.0f; });
                for (int i = 0; i < 16; ++i) p.on[(size_t) i] = (i % 4) == 2 || (i % 4) == 3;
                p.attack = 0.05f; p.hold = 0.75f; p.release = 0.2f;
                break;
            case 9:
            {
                const char* s = "x.xx.xx.x.xxx.x.";
                setAll (16, [] (int) { return 1.0f; });
                for (int i = 0; i < 16; ++i) p.on[(size_t) i] = s[i] == 'x';
                p.attack = 0.02f; p.hold = 0.78f; p.release = 0.2f;
                break;
            }
            case 10:
            {
                const char* s = "xx.xx.x.xx.x.xxx";
                const float lv[] = { 1.0f, 0.6f, 0, 1.0f, 0.5f, 0, 0.8f, 0, 1.0f, 0.6f, 0, 0.9f, 0, 1.0f, 0.7f, 0.4f };
                setAll (16, [&lv] (int i) { return lv[i]; });
                for (int i = 0; i < 16; ++i) p.on[(size_t) i] = s[i] == 'x';
                p.attack = 0.0f; p.hold = 0.7f; p.release = 0.3f;
                break;
            }
            case 11:
                setAll (12, [] (int) { return 1.0f; });
                for (int i = 0; i < 12; ++i) { p.on[(size_t) i] = (i % 3) != 2; p.tie[(size_t) i] = (i % 3) == 0; }
                p.hold = 0.8f; p.release = 0.2f;
                break;
            case 12:
                setAll (16, [] (int i) { return (i % 4) == 0 ? 1.0f : 0.7f; });
                for (int i = 0; i < 16; ++i)
                {
                    p.on[(size_t) i] = (i % 4) != 1;
                    p.tie[(size_t) i] = (i % 4) == 0;
                    p.length[(size_t) i] = (i % 4) >= 2 ? 0.5f : 1.0f;
                }
                break;
            case 13: setAll (32, [] (int i) { const float t = (float) i / 31.0f; return t * t; }); break;
            case 14:
            {
                juce::Random r (4242);
                float v = 0.5f;
                setAll (32, [&] (int) { v = juce::jlimit (0.05f, 1.0f, v + (r.nextFloat() - 0.5f) * 0.45f); return v; });
                break;
            }
            case 15:
            {
                const float lv[] = { 1.0f, 0.3f, 0.3f, 1.0f, 0.3f, 0.3f, 1.0f, 0.3f };
                setAll (8, [&lv] (int i) { return lv[i]; });
                p.hold = 0.6f; p.release = 0.4f;
                break;
            }
            case 16:
                setAll (16, [] (int i) { return i < 8 ? 1.0f : 0.5f + 0.5f * (float) (i - 8) / 7.0f; });
                for (int i = 0; i < 16; ++i)
                {
                    if (i < 8) { p.on[(size_t) i] = (i % 2) == 0; p.tie[(size_t) i] = false; }
                    else p.length[(size_t) i] = 0.5f;
                }
                break;
            case 17:
                setAll (8, [] (int i) { return (i % 4) == 0 ? 0.0f : (float) (i % 4) / 3.0f; });
                p.attack = 0.5f; p.hold = 0.5f; p.release = 0.0f;
                break;
        }
        p.sanitise();
        return p;
    }
};
} // namespace sfx
