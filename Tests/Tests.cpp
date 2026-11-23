// Console test runner for StepFlux: pattern logic, timing, audio behaviour, state and editor.
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <iostream>

using namespace sfx;

static int failures = 0, checks = 0;
#define CHECK(cond) do { ++checks; if (! (cond)) { ++failures; std::cout << "FAIL " << __LINE__ << ": " #cond << std::endl; } } while (false)
#define check(cond, msg) CHECK(cond)
#define CHECK_NEAR(a, b, tol) do { ++checks; const double _a = (a), _b = (b); if (std::abs (_a - _b) > (tol)) { ++failures; \
    std::cout << "FAIL " << __LINE__ << ": " #a " = " << _a << ", expected " << _b << std::endl; } } while (false)

struct TestPlayHead : juce::AudioPlayHead
{
    double ppq = 0.0, bpm = 120.0;
    bool playing = true;
    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo p;
        p.setBpm (bpm);
        p.setPpqPosition (ppq);
        p.setIsPlaying (playing);
        return p;
    }
};

static void setParam (StepFxProcessor& p, const char* id, float realValue)
{
    auto* prm = p.apvts.getParameter (id);
    jassert (prm != nullptr);
    prm->setValueNotifyingHost (prm->convertTo0to1 (realValue));
}

static void allEffectsOff (StepFxProcessor& p)
{
    for (auto* id : { "filterOn", "modOn", "delayOn", "distOn", "reverbOn" }) setParam (p, id, 0.0f);
    for (auto* id : { "m1On", "m2On", "m3On" }) setParam (p, id, 0.0f);
}

/** Renders `seconds` of a constant 0.5 signal and returns the output (stereo). */
static juce::AudioBuffer<float> render (StepFxProcessor& p, TestPlayHead& ph, double seconds, double sr = 48000.0, int block = 256,
                                        float inputValue = 0.5f, bool noise = false)
{
    p.setPlayHead (&ph);
    p.setRateAndBufferSizeDetails (sr, block);
    p.prepareToPlay (sr, block);
    const int total = (int) (seconds * sr);
    juce::AudioBuffer<float> out (2, total), buf (2, block);
    juce::MidiBuffer midi;
    juce::Random r (1);
    int pos = 0;
    while (pos < total)
    {
        const int n = juce::jmin (block, total - pos);
        buf.setSize (2, n, false, false, true);
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < n; ++i)
                buf.setSample (c, i, noise ? (r.nextFloat() * 2.0f - 1.0f) * inputValue : inputValue);
        ph.ppq = (double) pos * ph.bpm / 60.0 / sr;
        p.processBlock (buf, midi);
        for (int c = 0; c < 2; ++c) out.copyFrom (c, pos, buf, c, 0, n);
        pos += n;
    }
    return out;
}

static bool allFinite (const juce::AudioBuffer<float>& b, float limit = 8.01f)
{
    for (int c = 0; c < b.getNumChannels(); ++c)
        for (int i = 0; i < b.getNumSamples(); ++i)
        {
            const float v = b.getSample (c, i);
            if (! std::isfinite (v) || std::abs (v) > limit) return false;
        }
    return true;
}

//==============================================================================
static void testPatternOps()
{
    PatternData p;
    p.numSteps = 4;
    p.level = {}; p.level[0] = 0.15f; p.level[1] = 0.2f; p.level[2] = 0.3f; p.level[3] = 0.7f;

    auto s = p; s.shift (1);
    CHECK_NEAR (s.level[0], 0.7, 1e-6); CHECK_NEAR (s.level[1], 0.15, 1e-6);
    s.shift (-1);
    CHECK_NEAR (s.level[0], 0.15, 1e-6); CHECK_NEAR (s.level[3], 0.7, 1e-6);

    auto inv = p; inv.invert();
    CHECK_NEAR (inv.level[2], 0.7, 1e-6);

    auto rev = p; rev.reverse();
    CHECK_NEAR (rev.level[0], 0.7, 1e-6); CHECK_NEAR (rev.level[3], 0.15, 1e-6);

    auto dup = p; dup.appendDuplicate (false);
    CHECK (dup.numSteps == 8); CHECK_NEAR (dup.level[4], 0.15, 1e-6); CHECK_NEAR (dup.level[7], 0.7, 1e-6);

    auto rd = p; rd.appendDuplicate (true);
    CHECK (rd.numSteps == 8);
    const float expect[8] = { 0.15f, 0.2f, 0.3f, 0.7f, 0.7f, 0.3f, 0.2f, 0.15f };
    for (int i = 0; i < 8; ++i) CHECK_NEAR (rd.level[(size_t) i], expect[i], 1e-6);

    PatternData big; big.numSteps = 100; big.appendDuplicate (false);
    CHECK (big.numSteps == 128);

    PatternData d; d.numSteps = 4; d.length[0] = 0.25f; d.length[1] = 0.5f; d.length[2] = 1.0f; d.length[3] = 1.0f;
    d.doubleNoteLength();
    CHECK_NEAR (d.length[0], 0.5, 1e-6); CHECK_NEAR (d.length[1], 1.0, 1e-6); CHECK (d.tie[2]); CHECK (! d.tie[3]);

    // reverse keeps tied notes together
    PatternData t; t.numSteps = 4; t.tie[0] = true; // note over steps 1+2
    t.reverse();
    CHECK (t.tie[2]); CHECK (! t.tie[0]); CHECK (! t.tie[3]);

    juce::Random r (7);
    auto rf = p; rf.randomFull (r);
    for (int i = 0; i < 4; ++i) CHECK (rf.level[(size_t) i] >= 0.0f && rf.level[(size_t) i] <= 1.0f);
    auto rdv = p; rdv.randomDeviation (r);
    for (int i = 0; i < 4; ++i) CHECK (std::abs (rdv.level[(size_t) i] - p.level[(size_t) i]) <= 0.1501f);

    // serialisation round trip
    PatternData x = FactoryPatterns::get (10);
    x.attack = 0.2f; x.hold = 0.3f; x.release = 0.4f; x.attackCurve = -0.5f; x.numSteps = 77;
    auto y = PatternData::fromValueTree (x.toValueTree ("Pattern"));
    CHECK (y.numSteps == 77); CHECK_NEAR (y.attack, 0.2, 1e-4); CHECK_NEAR (y.attackCurve, -0.5, 1e-4);
    for (int i = 0; i < kMaxSteps; ++i)
    {
        CHECK_NEAR (y.level[(size_t) i], x.level[(size_t) i], 1e-3);
        CHECK (y.on[(size_t) i] == x.on[(size_t) i]); CHECK (y.tie[(size_t) i] == x.tie[(size_t) i]);
    }
    // garbage input is sanitised
    juce::ValueTree bad ("Pattern");
    bad.setProperty ("numSteps", 9999, nullptr);
    bad.setProperty ("levels", "nan 5 -3", nullptr);
    auto b = PatternData::fromValueTree (bad);
    CHECK (b.numSteps == 128); CHECK (b.level[1] == 1.0f); CHECK (b.level[2] == 0.0f); CHECK (std::isfinite (b.level[0]));

    for (int i = 0; i < FactoryPatterns::names().size(); ++i)
    {
        auto f = FactoryPatterns::get (i);
        CHECK (f.numSteps >= 1 && f.numSteps <= 128);
    }
}

static void testPlayback()
{
    PlaybackPattern pp;
    PatternData p;
    pp.prepare (p, 0.0f);
    for (float x = 0.0f; x < 16.0f; x += 0.37f) CHECK_NEAR (pp.evaluate (x).value, 1.0, 1e-6);

    // step length 50 %: second half of each step is closed
    p.length.fill (0.5f);
    pp.prepare (p, 0.0f);
    CHECK_NEAR (pp.evaluate (3.25f).value, 1.0, 1e-6);
    CHECK_NEAR (pp.evaluate (3.75f).value, 0.0, 1e-6);

    // rest
    p.on[5] = false;
    pp.prepare (p, 0.0f);
    CHECK_NEAR (pp.evaluate (5.1f).value, 0.0, 1e-6);
    CHECK (pp.evaluate (5.1f).step == 5);

    // tie: step 1 (level .4) tied over steps 2 and 3, last one 50 %
    PatternData t;
    t.level[0] = 0.4f; t.level[1] = 0.9f; t.tie[0] = true; t.tie[1] = true; t.length[2] = 0.5f;
    pp.prepare (t, 0.0f);
    CHECK_NEAR (pp.evaluate (1.5f).value, 0.4, 1e-6);
    CHECK_NEAR (pp.evaluate (2.4f).value, 0.4, 1e-6);
    CHECK_NEAR (pp.evaluate (2.6f).value, 0.0, 1e-6);
    CHECK_NEAR (pp.evaluate (3.1f).value, 1.0, 1e-6);

    // envelope: triangle A 50 / H 0 / R 50
    PatternData e; e.attack = 0.5f; e.hold = 0.0f; e.release = 0.5f;
    pp.prepare (e, 0.0f);
    CHECK_NEAR (pp.evaluate (0.25f).value, 0.5, 1e-4);
    CHECK_NEAR (pp.evaluate (0.5f).value, 1.0, 0.02);
    CHECK_NEAR (pp.evaluate (0.75f).value, 0.5, 1e-4);
    // envelope shorter than the step: silent afterwards
    e.attack = 0.1f; e.hold = 0.2f; e.release = 0.1f;
    pp.prepare (e, 0.0f);
    CHECK_NEAR (pp.evaluate (0.2f).value, 1.0, 1e-6);
    CHECK_NEAR (pp.evaluate (0.6f).value, 0.0, 1e-6);

    // swing: odd steps longer, pair length unchanged
    PatternData s;
    pp.prepare (s, 1.0f);
    CHECK (pp.stepAt (1.4f) == 0); CHECK (pp.stepAt (1.6f) == 1); CHECK (pp.stepAt (2.0f) == 2);
    CHECK_NEAR (pp.stepDur (0) + pp.stepDur (1), 2.0, 1e-6);
    // odd pattern length: the last step has no partner and keeps its length
    s.numSteps = 3;
    pp.prepare (s, 1.0f);
    CHECK (pp.stepAt (2.9f) == 2); CHECK_NEAR (pp.stepDur (2), 1.0, 1e-6);

    // single step pattern
    PatternData one; one.numSteps = 1;
    pp.prepare (one, 0.5f);
    CHECK (pp.stepAt (0.5f) == 0); CHECK_NEAR (pp.evaluate (0.5f).value, 1.0, 1e-6);
}

static float meanAbs (const juce::AudioBuffer<float>& b, int from, int to, int ch = 0)
{
    double s = 0;
    for (int i = from; i < to; ++i) s += std::abs (b.getSample (ch, i));
    return (float) (s / juce::jmax (1, to - from));
}

static void testGateAndLoop()
{
    const double sr = 48000.0;
    const int step = 6000; // 1/16 at 120 bpm
    {
        StepFxProcessor p; TestPlayHead ph;
        allEffectsOff (p);
        auto out = render (p, ph, 0.5, sr);
        CHECK_NEAR (meanAbs (out, 2000, 20000), 0.5, 1e-4); // unity when everything is off
    }
    {   // Gate Mix, alternating steps, depth 100 %
        StepFxProcessor p; TestPlayHead ph;
        allEffectsOff (p);
        setParam (p, "m1On", 1.0f); setParam (p, "m1Target", (float) tGateMix); setParam (p, "m1Depth", 100.0f);
        PatternData pat;
        for (int i = 0; i < 16; ++i) pat.on[(size_t) i] = (i % 2) == 0;
        p.setPattern (0, pat, "t");
        auto out = render (p, ph, 1.0, sr);
        CHECK_NEAR (meanAbs (out, 1000, step - 500), 0.5, 0.01);
        CHECK_NEAR (meanAbs (out, step + 1000, 2 * step - 500), 0.0, 0.005);
        CHECK_NEAR (meanAbs (out, 2 * step + 1000, 3 * step - 500), 0.5, 0.01);

        // depth 50 %: closed steps at half level; depth 0: unchanged
        setParam (p, "m1Depth", 50.0f);
        out = render (p, ph, 0.5, sr);
        CHECK_NEAR (meanAbs (out, step + 1000, 2 * step - 500), 0.25, 0.01);
        setParam (p, "m1Depth", 0.0f);
        out = render (p, ph, 0.5, sr);
        CHECK_NEAR (meanAbs (out, step + 1000, 2 * step - 500), 0.5, 0.01);
        // negative depth inverts the pattern
        setParam (p, "m1Depth", -100.0f);
        out = render (p, ph, 0.5, sr);
        CHECK_NEAR (meanAbs (out, 1000, step - 500), 0.0, 0.005);
        CHECK_NEAR (meanAbs (out, step + 1000, 2 * step - 500), 0.5, 0.01);

        // Gate Add: depth 0 = silence, depth 100 = like Gate Mix
        setParam (p, "m1Target", (float) tGateAdd); setParam (p, "m1Depth", 0.0f);
        out = render (p, ph, 0.5, sr);
        CHECK_NEAR (meanAbs (out, 3000, 20000), 0.0, 0.002);
        setParam (p, "m1Depth", 100.0f);
        out = render (p, ph, 0.5, sr);
        CHECK_NEAR (meanAbs (out, 1000, step - 500), 0.5, 0.01);
        CHECK_NEAR (meanAbs (out, step + 1000, 2 * step - 500), 0.0, 0.005);
    }
    {   // step levels act as volume (tremolo), loop length 3
        StepFxProcessor p; TestPlayHead ph;
        allEffectsOff (p);
        setParam (p, "m1On", 1.0f); setParam (p, "m1Target", (float) tGateMix); setParam (p, "m1Depth", 100.0f);
        PatternData pat; pat.numSteps = 3;
        pat.level[0] = 1.0f; pat.level[1] = 0.5f; pat.level[2] = 0.2f;
        p.setPattern (0, pat, "t");
        auto out = render (p, ph, 1.0, sr);
        const float expect[7] = { 1.0f, 0.5f, 0.2f, 1.0f, 0.5f, 0.2f, 1.0f };
        for (int k = 0; k < 7; ++k)
            CHECK_NEAR (meanAbs (out, k * step + 1000, (k + 1) * step - 500), 0.5f * expect[k], 0.01);
    }
    {   // rate 1/8 doubles the step time; two gate modulators multiply
        StepFxProcessor p; TestPlayHead ph;
        allEffectsOff (p);
        setParam (p, "m1On", 1.0f); setParam (p, "m1Target", (float) tGateMix); setParam (p, "m1Depth", 100.0f);
        setParam (p, "m1Rate", 8.0f); // 1/8
        PatternData pat; pat.numSteps = 2; pat.level[1] = 0.5f;
        p.setPattern (0, pat, "t");
        setParam (p, "m2On", 1.0f); setParam (p, "m2Target", (float) tGateMix); setParam (p, "m2Depth", 100.0f);
        PatternData pat2; pat2.numSteps = 2; pat2.level[1] = 0.5f; // 1/16
        p.setPattern (1, pat2, "t");
        auto out = render (p, ph, 1.0, sr);
        const float expect[4] = { 1.0f, 0.5f, 0.5f, 0.25f };
        for (int k = 0; k < 4; ++k)
            CHECK_NEAR (meanAbs (out, k * step + 1000, (k + 1) * step - 500), 0.5f * expect[k], 0.01);
    }
    {   // pan: hard left on step 1, hard right on step 2
        StepFxProcessor p; TestPlayHead ph;
        allEffectsOff (p);
        setParam (p, "m1On", 1.0f); setParam (p, "m1Target", (float) tPan); setParam (p, "m1Depth", 100.0f);
        PatternData pat; pat.numSteps = 2; pat.level[0] = 0.0f; pat.level[1] = 1.0f;
        p.setPattern (0, pat, "t");
        auto out = render (p, ph, 0.5, sr);
        CHECK_NEAR (meanAbs (out, 1000, step - 500, 0), 0.5, 0.01);
        CHECK_NEAR (meanAbs (out, 1000, step - 500, 1), 0.0, 0.005);
        CHECK_NEAR (meanAbs (out, step + 1000, 2 * step - 500, 0), 0.0, 0.005);
        CHECK_NEAR (meanAbs (out, step + 1000, 2 * step - 500, 1), 0.5, 0.01);
    }
    {   // master: mix 0 = dry, output gain, xy pad modulating master output
        StepFxProcessor p; TestPlayHead ph;
        allEffectsOff (p);
        setParam (p, "masterOutput", -6.0f);
        auto out = render (p, ph, 0.3, sr);
        CHECK_NEAR (meanAbs (out, 4000, 12000), 0.5 * 0.5012, 0.003);
        setParam (p, "masterOutput", 0.0f);
        setParam (p, "masterInput", -24.0f); setParam (p, "masterMix", 0.0f);
        out = render (p, ph, 0.3, sr);
        CHECK_NEAR (meanAbs (out, 4000, 12000), 0.5, 0.003);
        setParam (p, "masterInput", 0.0f); setParam (p, "masterMix", 100.0f);
        // X axis drives Master Output from -24 dB (param at minimum) to +24 dB (pad fully right)
        setParam (p, "masterOutput", -24.0f);
        setParam (p, "xTarget1", (float) (tMasterOutput - 2)); setParam (p, "xyX", 1.0f); setParam (p, "xDepth1", 50.0f);
        out = render (p, ph, 0.3, sr);
        CHECK_NEAR (meanAbs (out, 6000, 12000), 0.5, 0.01); // -24 dB + half range = 0 dB
        setParam (p, "xOn", 0.0f);
        out = render (p, ph, 0.3, sr);
        CHECK_NEAR (meanAbs (out, 6000, 12000), 0.5 * 0.0631, 0.003);
    }
    {   // not playing: the modulator keeps running on its own clock
        StepFxProcessor p; TestPlayHead ph; ph.playing = false;
        allEffectsOff (p);
        setParam (p, "m1On", 1.0f); setParam (p, "m1Target", (float) tGateMix); setParam (p, "m1Depth", 100.0f);
        PatternData pat; pat.numSteps = 2; pat.on[1] = false;
        p.setPattern (0, pat, "t");
        auto out = render (p, ph, 0.5, sr);
        CHECK_NEAR (meanAbs (out, 1000, step - 500), 0.5, 0.01);
        CHECK_NEAR (meanAbs (out, step + 1000, 2 * step - 500), 0.0, 0.005);
    }
    {   // modulating the filter cutoff changes the sound step by step
        StepFxProcessor p; TestPlayHead ph;
        allEffectsOff (p);
        setParam (p, "filterOn", 1.0f); setParam (p, "filterCutoff", 20.0f); setParam (p, "filterRes", 0.0f);
        setParam (p, "m1On", 1.0f); setParam (p, "m1Target", (float) tFilterCutoff); setParam (p, "m1Depth", 100.0f);
        PatternData pat; pat.numSteps = 2; pat.level[0] = 0.0f; pat.level[1] = 1.0f;
        p.setPattern (0, pat, "t");
        auto out = render (p, ph, 0.5, sr, 256, 0.5f, true);
        const float closed = meanAbs (out, 2000, step - 500), open = meanAbs (out, step + 2000, 2 * step - 500);
        CHECK (open > closed * 5.0f);
        CHECK (p.currentStep[0].load() >= 0);
    }
}

static void testStability()
{
    juce::Random r (99);
    const double rates[] = { 44100.0, 48000.0, 96000.0, 192000.0 };
    const int blocks[] = { 1, 17, 64, 512, 4096 };
    const int numTypes = (int) FilterTypes::all().size();
    int runs = 0;
    for (int type = 0; type < numTypes; ++type)
    {
        for (int variant = 0; variant < 3; ++variant)
        {
            StepFxProcessor p; TestPlayHead ph;
            ph.bpm = variant == 0 ? 120.0 : (variant == 1 ? 40.0 : 300.0);
            for (auto* prm : p.getParameters())
                prm->setValueNotifyingHost (variant == 0 ? r.nextFloat() : (variant == 1 ? 1.0f : (r.nextBool() ? 0.0f : 1.0f)));
            for (auto* id : { "filterOn", "modOn", "delayOn", "distOn", "reverbOn", "m1On", "m2On", "m3On" }) setParam (p, id, 1.0f);
            setParam (p, "filterType", (float) type);
            setParam (p, "m1Target", (float) tFilterCutoff); setParam (p, "m2Target", (float) tFilterRes);
            setParam (p, "m3Target", (float) (1 + r.nextInt (kNumModTargets - 1)));
            for (int m = 0; m < 3; ++m)
            {
                auto pat = FactoryPatterns::get (r.nextInt (FactoryPatterns::names().size()));
                pat.numSteps = 1 + r.nextInt (128);
                pat.randomFull (r);
                p.setPattern (m, pat, "x");
            }
            std::array<int, kNumBlocks> ord {};
            for (int i = 0; i < kNumBlocks; ++i) ord[(size_t) i] = i;
            for (int i = kNumBlocks - 1; i > 0; --i) std::swap (ord[(size_t) i], ord[(size_t) r.nextInt (i + 1)]);
            p.setOrder (ord);
            const double sr = rates[(type + variant) % 4];
            const int block = blocks[(type * 3 + variant) % 5];
            auto out = render (p, ph, block == 1 ? 0.05 : 0.4, sr, block, 0.9f, true);
            const bool ok = allFinite (out);
            if (! ok) std::cout << "  unstable: filter type " << FilterTypes::all()[(size_t) type].name << " variant " << variant << std::endl;
            CHECK (ok);
            ++runs;
        }
    }
    {   // silence in, nothing self-oscillating at default settings; NaN input does not poison the plugin
        StepFxProcessor p; TestPlayHead ph;
        auto out = render (p, ph, 0.5, 48000.0, 256, 0.0f);
        CHECK (out.getMagnitude (0, out.getNumSamples()) < 1.0e-4f);
        p.prepareToPlay (48000.0, 64);
        juce::AudioBuffer<float> b (2, 64); juce::MidiBuffer m;
        for (int i = 0; i < 64; ++i) { b.setSample (0, i, std::numeric_limits<float>::quiet_NaN()); b.setSample (1, i, std::numeric_limits<float>::infinity()); }
        p.processBlock (b, m);
        CHECK (allFinite (b));
        for (int k = 0; k < 50; ++k) { b.clear(); p.processBlock (b, m); }
        CHECK (allFinite (b));
    }
    {   // mono in / mono out and a block larger than announced
        StepFxProcessor p;
        juce::AudioProcessor::BusesLayout mono;
        mono.inputBuses.add (juce::AudioChannelSet::mono()); mono.outputBuses.add (juce::AudioChannelSet::mono());
        CHECK (p.setBusesLayout (mono));
        p.setRateAndBufferSizeDetails (44100.0, 32);
        p.prepareToPlay (44100.0, 32);
        juce::AudioBuffer<float> b (1, 2048); juce::MidiBuffer m;
        for (int i = 0; i < 2048; ++i) b.setSample (0, i, 0.25f);
        p.processBlock (b, m);
        CHECK (allFinite (b));
    }
    std::cout << "  stability runs: " << runs << std::endl;
}

static void testPresets()
{
    StepFxProcessor p;
    p.prepareToPlay (48000.0, 256);
    const auto names = StepFxProcessor::factoryPresetNames();
    for (int i = 0; i < names.size(); ++i)
    {
        p.loadFactoryPreset (i);
        check (p.getPresetName() == names[i], "factory preset name");
        juce::AudioBuffer<float> buf (2, 256);
        juce::MidiBuffer midi;
        bool finite = true;
        for (int b = 0; b < 200; ++b)
        {
            for (int c = 0; c < 2; ++c)
                for (int n = 0; n < 256; ++n) buf.setSample (c, n, 0.5f * std::sin (0.05f * (float) (b * 256 + n)));
            p.processBlock (buf, midi);
            for (int c = 0; c < 2; ++c)
                for (int n = 0; n < 256; ++n) { const float v = buf.getSample (c, n); if (! std::isfinite (v) || std::abs (v) > 8.0f) finite = false; }
        }
        check (finite, "factory preset output stays finite and bounded");
    }
    // file round trip
    p.loadFactoryPreset (7);
    const auto file = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("stepflux_test.stepflux");
    check (p.savePresetFile (file, "stepflux_test"), "preset saved");
    const auto fb = p.apvts.getParameter ("delayFeedback")->getValue();
    const auto pat = p.getPattern (1);
    p.loadFactoryPreset (0);
    check (p.loadPresetFile (file), "preset loaded");
    check (std::abs (p.apvts.getParameter ("delayFeedback")->getValue() - fb) < 1.0e-4f, "preset restores parameters");
    check (p.getPattern (1).numSteps == pat.numSteps && p.getPatternName (1) == "Dotted Chop", "preset restores patterns");
    check (p.getPresetName() == "stepflux_test", "preset name follows file");
    file.deleteFile();
}

static void testState()
{
    StepFxProcessor a;
    juce::Random r (5);
    for (auto* prm : a.getParameters()) prm->setValueNotifyingHost (r.nextFloat());
    PatternData pat = FactoryPatterns::get (9);
    pat.numSteps = 23; pat.release = 0.33f;
    a.setPattern (1, pat, "My Pattern");
    std::array<int, kNumBlocks> ord { 7, 6, 5, 4, 3, 2, 1, 0 };
    a.setOrder (ord);
    juce::MemoryBlock mb;
    a.getStateInformation (mb);

    StepFxProcessor b;
    b.setStateInformation (mb.getData(), (int) mb.getSize());
    for (int i = 0; i < a.getParameters().size(); ++i)
        CHECK (a.getParameters()[i]->getCurrentValueAsText() == b.getParameters()[i]->getCurrentValueAsText());
    CHECK (b.getPattern (1).numSteps == 23);
    CHECK_NEAR (b.getPattern (1).release, 0.33, 1e-3);
    CHECK (b.getPatternName (1) == "My Pattern");
    CHECK (b.getOrder() == ord);
    for (int i = 0; i < 16; ++i) CHECK (b.getPattern (1).on[(size_t) i] == pat.on[(size_t) i]);

    // saving twice must not grow the state (no duplicated children)
    juce::MemoryBlock mb2, mb3;
    b.getStateInformation (mb2);
    b.setStateInformation (mb2.getData(), (int) mb2.getSize());
    b.getStateInformation (mb3);
    CHECK (mb2.getSize() == mb3.getSize());

    // garbage and invalid order are ignored
    const char junk[] = "this is not a state";
    b.setStateInformation (junk, (int) sizeof (junk));
    b.setStateInformation (nullptr, 0);
    b.setOrder ({ 0, 0, 1, 2, 3, 4, 5, 6 });
    CHECK (b.getOrder() == ord);
}

static juce::MouseEvent mouseAt (juce::Component& c, float x, float y, juce::ModifierKeys mods = juce::ModifierKeys::leftButtonModifier,
                                 juce::Point<float> down = { -1, -1 })
{
    auto src = juce::Desktop::getInstance().getMainMouseSource();
    const auto now = juce::Time::getCurrentTime();
    return juce::MouseEvent (src, { x, y }, mods, 1.0f, 0, 0, 0, 0, &c, &c, now, down.x < 0 ? juce::Point<float> { x, y } : down, now, 1, false);
}

static void testEditor (const juce::File& shotDir)
{
    StepFxProcessor p;
    setParam (p, "m1Target", (float) tGateMix); setParam (p, "m1Depth", 100.0f);
    setParam (p, "m2On", 1.0f); setParam (p, "m2Target", (float) tFilterCutoff); setParam (p, "m2Depth", -45.0f);
    setParam (p, "filterCutoff", 6000.0f);
    setParam (p, "xTarget1", (float) (tReverbMix - 2)); setParam (p, "xyX", 0.4f); setParam (p, "xyY", 0.7f);
    p.setPattern (0, FactoryPatterns::get (10), "Trance Gate B");
    std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
    auto* e = dynamic_cast<StepFxEditor*> (ed.get());
    CHECK (e != nullptr);
    if (e == nullptr) return;
    CHECK (e->getWidth() > 800 && e->getHeight() > 500);

    auto& se = e->getStepEditor();
    CHECK (se.getPattern().numSteps == 16);
    const float sw = se.stepWidth();
    const auto bars = se.barsArea().toFloat();
    const auto nums = se.numbersArea().toFloat();

    // draw a level on step 3 (middle of the lane)
    e->runAction (StepFxEditor::aInvert); e->runAction (StepFxEditor::aInvert);
    se.mouseDown (mouseAt (se, se.stepX (2) + sw * 0.4f, bars.getCentreY()));
    se.mouseUp (mouseAt (se, se.stepX (2) + sw * 0.4f, bars.getCentreY()));
    CHECK_NEAR (p.getPattern (0).level[2], 0.5, 0.05);
    CHECK (p.getPatternName (0) == "Custom");

    // drag across steps 5..8 near the top: all get a high level
    se.mouseDown (mouseAt (se, se.stepX (4) + sw * 0.15f, bars.getY() + 6.0f));
    se.mouseDrag (mouseAt (se, se.stepX (7) + sw * 0.15f, bars.getY() + 6.0f));
    se.mouseUp (mouseAt (se, se.stepX (7) + sw * 0.4f, bars.getY() + 6.0f));
    for (int i = 4; i <= 7; ++i) CHECK (p.getPattern (0).level[(size_t) i] > 0.9f);

    // toggle step 1 off and on again; its level survives
    const bool was = p.getPattern (0).on[0];
    const float lv = p.getPattern (0).level[0];
    se.mouseDown (mouseAt (se, se.stepX (0) + sw * 0.5f, nums.getCentreY()));
    se.mouseUp (mouseAt (se, se.stepX (0) + sw * 0.5f, nums.getCentreY()));
    CHECK (p.getPattern (0).on[0] != was);
    se.mouseDown (mouseAt (se, se.stepX (0) + sw * 0.5f, nums.getCentreY()));
    se.mouseUp (mouseAt (se, se.stepX (0) + sw * 0.5f, nums.getCentreY()));
    CHECK (p.getPattern (0).on[0] == was); CHECK_NEAR (p.getPattern (0).level[0], lv, 1e-6);

    // loop length: drag the handle from 16 to 8 steps, then to 24
    float hx = se.stepX (16) + 4.0f;
    se.mouseDown (mouseAt (se, hx, bars.getCentreY()));
    se.mouseDrag (mouseAt (se, se.stepX (8), bars.getCentreY()));
    se.mouseUp (mouseAt (se, se.stepX (8), bars.getCentreY()));
    CHECK (p.getPattern (0).numSteps == 8);
    hx = se.stepX (8) + 4.0f;
    se.mouseDown (mouseAt (se, hx, bars.getCentreY()));
    se.mouseDrag (mouseAt (se, se.stepX (8) + se.stepWidth() * 16.0f, bars.getCentreY()));
    se.mouseUp (mouseAt (se, 0, 0));
    CHECK (p.getPattern (0).numSteps > 8);

    // step length: reset, then shorten step 2 to 50 % and stretch step 4 over steps 5 and 6 (tie)
    p.setPattern (0, PatternData(), "Default");
    e->selectMod (0);
    const float w2 = se.stepWidth();
    se.mouseDown (mouseAt (se, se.stepX (1) + w2 - 1.0f, bars.getCentreY()));
    se.mouseDrag (mouseAt (se, se.stepX (1) + w2 * 0.5f, bars.getCentreY()));
    se.mouseUp (mouseAt (se, 0, 0));
    CHECK_NEAR (p.getPattern (0).length[1], 0.5, 1e-4);
    se.mouseDown (mouseAt (se, se.stepX (3) + w2 - 1.0f, bars.getCentreY()));
    se.mouseDrag (mouseAt (se, se.stepX (3) + w2 * 2.75f, bars.getCentreY()));
    se.mouseUp (mouseAt (se, 0, 0));
    CHECK (p.getPattern (0).tie[3]); CHECK (p.getPattern (0).tie[4]); CHECK (! p.getPattern (0).tie[5]);
    CHECK_NEAR (p.getPattern (0).length[5], 0.75, 1e-4);
    // shorten the tied note again down to a single step at 25 %
    se.mouseDown (mouseAt (se, se.stepX (3) + w2 * 2.75f, bars.getCentreY()));
    se.mouseDrag (mouseAt (se, se.stepX (3) + w2 * 0.25f, bars.getCentreY()));
    se.mouseUp (mouseAt (se, 0, 0));
    CHECK (! p.getPattern (0).tie[3]); CHECK (! p.getPattern (0).tie[4]);
    CHECK_NEAR (p.getPattern (0).length[3], 0.25, 1e-4);
    // tie by clicking between two step numbers, and remove it again
    se.mouseDown (mouseAt (se, se.stepX (9) - 1.0f, nums.getCentreY()));
    CHECK (p.getPattern (0).tie[8]);
    se.mouseDown (mouseAt (se, se.stepX (9) + 1.0f, nums.getCentreY()));
    CHECK (! p.getPattern (0).tie[8]);

    // action menu entries
    e->runAction (StepFxEditor::aAppendDup);
    CHECK (p.getPattern (0).numSteps == 32);
    e->runAction (StepFxEditor::aAppendDup); e->runAction (StepFxEditor::aAppendDup); e->runAction (StepFxEditor::aAppendDup);
    CHECK (p.getPattern (0).numSteps == 128);
    for (int a = StepFxEditor::aRandomFull; a <= StepFxEditor::aAppendRev; ++a) e->runAction (a);
    CHECK (p.getPattern (0).numSteps == 128);

    // selecting another modulator shows its pattern
    e->selectMod (1);
    CHECK (se.getPattern().numSteps == p.getPattern (1).numSteps);
    e->selectMod (0);

    if (shotDir.isDirectory())
    {
        p.setPattern (0, FactoryPatterns::get (10), "Trance Gate B");
        p.setPattern (1, FactoryPatterns::get (4), "Sine");
        p.currentStep[0].store (3);
        for (int view = 0; view < 2; ++view)
        {
            e->selectMod (view);
            auto img = e->createComponentSnapshot (e->getLocalBounds(), true, 2.0f);
            juce::File f = shotDir.getChildFile ("editor" + juce::String (view + 1) + ".png");
            f.deleteFile();
            juce::FileOutputStream os (f);
            juce::PNGImageFormat png;
            CHECK (os.openedOk() && png.writeImageToStream (img, os));
        }
    }
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    std::cout << "pattern ops" << std::endl;   testPatternOps();
    std::cout << "playback" << std::endl;      testPlayback();
    std::cout << "gate / loop / pan" << std::endl; testGateAndLoop();
    std::cout << "stability" << std::endl;     testStability();
    std::cout << "state" << std::endl;         testState();
    std::cout << "presets" << std::endl;       testPresets();
    std::cout << "editor" << std::endl;        testEditor (argc > 1 ? juce::File (argv[1]) : juce::File());
    std::cout << checks << " checks, " << failures << " failures" << std::endl;
    return failures == 0 ? 0 : 1;
}
