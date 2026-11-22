#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace sfx
{
struct TargetInfo { const char* name; const char* paramId; };
static const TargetInfo kTargets[kNumTargets] = {
    { "None", nullptr }, { "Gate Mix", nullptr }, { "Gate Add", nullptr }, { "Pan", nullptr },
    { "Filter Cutoff", "filterCutoff" }, { "Filter Res", "filterRes" }, { "Filter Drive", "filterDrive" },
    { "Mod FX Delay", "modDelay" }, { "Mod FX Rate", "modRate" }, { "Mod FX Depth", "modDepth" },
    { "Mod FX Feedback", "modFeedback" }, { "Mod FX Mix", "modMix" },
    { "Delay Feedback", "delayFeedback" }, { "Delay Cutoff", "delayCutoff" }, { "Delay Mix", "delayMix" },
    { "Exciter", "exciter" }, { "Dirt", "dirt" },
    { "Reverb Time", "reverbTime" }, { "Reverb Mix", "reverbMix" },
    { "Master Input", "masterInput" }, { "Master Mix", "masterMix" }, { "Master Output", "masterOutput" },
    { "Mod 1 Depth", "m1Depth" }, { "Mod 2 Depth", "m2Depth" }, { "Mod 3 Depth", "m3Depth" }
};

const char* targetName (int t) { return kTargets[juce::jlimit (0, kNumTargets - 1, t)].name; }
const char* targetParamId (int t) { return kTargets[juce::jlimit (0, kNumTargets - 1, t)].paramId; }

juce::StringArray modTargetNames()
{
    juce::StringArray s;
    for (int i = 0; i < kNumModTargets; ++i) s.add (kTargets[i].name);
    return s;
}

juce::StringArray xyTargetNames()
{
    juce::StringArray s { "None" };
    for (int i = tPan; i < kNumTargets; ++i) s.add (kTargets[i].name);
    return s;
}

const char* blockName (int b)
{
    static const char* names[kNumBlocks] = { "Gate", "Pan", "Mod FX", "Delay", "Filter", "Exciter", "Dirt", "Reverb" };
    return names[juce::jlimit (0, kNumBlocks - 1, b)];
}

struct Div { const char* name; double beats; };
static const Div kRates[] = {
    { "1/64", 0.0625 }, { "1/32T", 0.25 / 3.0 }, { "1/32", 0.125 }, { "1/16T", 0.5 / 3.0 }, { "1/32.", 0.1875 },
    { "1/16", 0.25 }, { "1/8T", 1.0 / 3.0 }, { "1/16.", 0.375 }, { "1/8", 0.5 }, { "1/4T", 2.0 / 3.0 },
    { "1/8.", 0.75 }, { "1/4", 1.0 }, { "1/2T", 4.0 / 3.0 }, { "1/4.", 1.5 }, { "1/2", 2.0 }, { "1/2.", 3.0 },
    { "1/1", 4.0 }, { "2/1", 8.0 }, { "4/1", 16.0 }
};
static const int kNumRates = (int) (sizeof (kRates) / sizeof (kRates[0]));

juce::StringArray rateNames() { juce::StringArray s; for (auto& r : kRates) s.add (r.name); return s; }
double rateBeats (int i) { return kRates[juce::jlimit (0, kNumRates - 1, i)].beats; }
juce::StringArray delayDivNames() { return rateNames(); }
double delayDivBeats (int i) { return rateBeats (i); }
}

using namespace sfx;

//==============================================================================
static juce::String pctText (float v, int) { return juce::String (juce::roundToInt (v)) + " %"; }
static juce::String hzText (float v, int) { return v >= 1000.0f ? juce::String (v / 1000.0f, 2) + " kHz" : juce::String (juce::roundToInt (v)) + " Hz"; }
static juce::String msText (float v, int) { return v >= 100.0f ? juce::String (juce::roundToInt (v)) + " ms" : juce::String (v, 1) + " ms"; }
static juce::String dbText (float v, int) { return (v > 0.0f ? "+" : "") + juce::String (v, 1) + " dB"; }
static juce::String secText (float v, int) { return juce::String (v, 2) + " s"; }
static juce::String rateText (float v, int) { return juce::String (v, 2) + " Hz"; }

static juce::NormalisableRange<float> skewed (float lo, float hi, float centre)
{
    juce::NormalisableRange<float> r (lo, hi);
    r.setSkewForCentre (centre);
    return r;
}

juce::AudioProcessorValueTreeState::ParameterLayout StepFxProcessor::createLayout()
{
    using F = juce::AudioParameterFloat;
    using B = juce::AudioParameterBool;
    using C = juce::AudioParameterChoice;
    using A = juce::AudioParameterFloatAttributes;
    juce::AudioProcessorValueTreeState::ParameterLayout l;

    auto pid = [] (const juce::String& id) { return juce::ParameterID { id, 1 }; };
    auto flt = [&] (const char* id, const char* name, juce::NormalisableRange<float> range, float def, juce::String (*txt) (float, int))
    {
        l.add (std::make_unique<F> (pid (id), name, range, def, A().withStringFromValueFunction (txt)));
    };
    auto pct = [&] (const char* id, const char* name, float def) { flt (id, name, { 0.0f, 100.0f }, def, pctText); };
    auto bipct = [&] (const juce::String& id, const juce::String& name, float def)
    {
        l.add (std::make_unique<F> (pid (id), name, juce::NormalisableRange<float> (-100.0f, 100.0f), def, A().withStringFromValueFunction (pctText)));
    };

    // step modulators
    for (int m = 0; m < kNumMods; ++m)
    {
        const juce::String p = "m" + juce::String (m + 1), n = "Mod " + juce::String (m + 1) + " ";
        l.add (std::make_unique<B> (pid (p + "On"), n + "On", m == 0));
        l.add (std::make_unique<C> (pid (p + "Target"), n + "Target", modTargetNames(), m == 0 ? (int) tFilterCutoff : (int) tNone));
        bipct (p + "Depth", n + "Depth", 0.0f);
        l.add (std::make_unique<C> (pid (p + "Rate"), n + "Rate", rateNames(), 5));
        l.add (std::make_unique<F> (pid (p + "Swing"), n + "Swing", juce::NormalisableRange<float> (0.0f, 100.0f), 0.0f, A().withStringFromValueFunction (pctText)));
    }

    // filter
    l.add (std::make_unique<B> (pid ("filterOn"), "Filter On", true));
    {
        juce::StringArray names;
        for (auto& t : FilterTypes::all()) names.add (t.name);
        l.add (std::make_unique<C> (pid ("filterType"), "Filter Type", names, FilterTypes::defaultIndex()));
    }
    flt ("filterCutoff", "Filter Cutoff", skewed (20.0f, 20000.0f, 1000.0f), 8000.0f, hzText);
    pct ("filterRes", "Filter Res", 20.0f);
    pct ("filterDrive", "Filter Drive", 0.0f);

    // mod fx
    l.add (std::make_unique<B> (pid ("modOn"), "Mod FX On", false));
    flt ("modDelay", "Mod FX Delay", skewed (0.1f, 30.0f, 5.0f), 7.0f, msText);
    flt ("modRate", "Mod FX Rate", skewed (0.01f, 20.0f, 1.0f), 0.6f, rateText);
    pct ("modDepth", "Mod FX Depth", 40.0f);
    pct ("modFeedback", "Mod FX Feedback", 0.0f);
    pct ("modMix", "Mod FX Mix", 50.0f);

    // delay
    l.add (std::make_unique<B> (pid ("delayOn"), "Delay On", false));
    l.add (std::make_unique<B> (pid ("delaySync"), "Delay Sync", true));
    flt ("delayTimeL", "Delay Time L", skewed (1.0f, 2000.0f, 250.0f), 250.0f, msText);
    flt ("delayTimeR", "Delay Time R", skewed (1.0f, 2000.0f, 250.0f), 375.0f, msText);
    l.add (std::make_unique<C> (pid ("delayDivL"), "Delay Division L", delayDivNames(), 8));
    l.add (std::make_unique<C> (pid ("delayDivR"), "Delay Division R", delayDivNames(), 10));
    pct ("delayFeedback", "Delay Feedback", 35.0f);
    l.add (std::make_unique<C> (pid ("delayFilter"), "Delay Filter Type", juce::StringArray { "LP", "BP", "HP", "LP+LoCut" }, 0));
    flt ("delayCutoff", "Delay Cutoff", skewed (20.0f, 20000.0f, 1000.0f), 6000.0f, hzText);
    pct ("delayMix", "Delay Mix", 25.0f);

    // distortion
    l.add (std::make_unique<B> (pid ("distOn"), "Distortion On", false));
    pct ("exciter", "Exciter", 0.0f);
    pct ("dirt", "Dirt", 25.0f);

    // reverb
    l.add (std::make_unique<B> (pid ("reverbOn"), "Reverb On", true));
    l.add (std::make_unique<C> (pid ("reverbType"), "Reverb Type", FdnReverb::typeNames(), FdnReverb::defaultType()));
    flt ("reverbTime", "Reverb Time", skewed (0.2f, 12.0f, 2.0f), 2.0f, secText);
    pct ("reverbMix", "Reverb Mix", 15.0f);

    // master
    flt ("masterInput", "Input", { -24.0f, 24.0f }, 0.0f, dbText);
    pct ("masterMix", "Mix", 100.0f);
    flt ("masterOutput", "Output", { -24.0f, 24.0f }, 0.0f, dbText);

    // xy pad
    l.add (std::make_unique<F> (pid ("xyX"), "XY Pad X", juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f));
    l.add (std::make_unique<F> (pid ("xyY"), "XY Pad Y", juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f));
    l.add (std::make_unique<B> (pid ("xOn"), "X On", true));
    l.add (std::make_unique<B> (pid ("yOn"), "Y On", true));
    l.add (std::make_unique<C> (pid ("xTarget1"), "X Target 1", xyTargetNames(), 0));
    l.add (std::make_unique<C> (pid ("xTarget2"), "X Target 2", xyTargetNames(), 0));
    l.add (std::make_unique<C> (pid ("yTarget1"), "Y Target 1", xyTargetNames(), 0));
    l.add (std::make_unique<C> (pid ("yTarget2"), "Y Target 2", xyTargetNames(), 0));
    bipct ("xDepth", "X Depth", 100.0f);
    bipct ("yDepth", "Y Depth", 100.0f);
    return l;
}

//==============================================================================
StepFxProcessor::StepFxProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "StepFluxState", createLayout())
{
    for (int m = 0; m < kNumMods; ++m)
    {
        const juce::String p = "m" + juce::String (m + 1);
        mp[(size_t) m] = { apvts.getRawParameterValue (p + "On"), apvts.getRawParameterValue (p + "Target"),
                           apvts.getRawParameterValue (p + "Depth"), apvts.getRawParameterValue (p + "Rate"),
                           apvts.getRawParameterValue (p + "Swing") };
        patternNames[(size_t) m] = "Default";
        currentStep[(size_t) m].store (-1);
    }
    for (int t = 0; t < kNumTargets; ++t)
    {
        effectiveNorm[(size_t) t].store (0.0f);
        if (auto* id = targetParamId (t))
        {
            targetParam[(size_t) t] = apvts.getParameter (id);
            targetRaw[(size_t) t] = apvts.getRawParameterValue (id);
            jassert (targetParam[(size_t) t] != nullptr);
        }
    }
    for (int b = 0; b < kNumBlocks; ++b) order[(size_t) b].store (b);

    auto raw = [this] (const char* id) { auto* p = apvts.getRawParameterValue (id); jassert (p != nullptr); return p; };
    pFilterOn = raw ("filterOn"); pFilterType = raw ("filterType"); pModOn = raw ("modOn"); pDelayOn = raw ("delayOn");
    pDelaySync = raw ("delaySync"); pDelayTimeL = raw ("delayTimeL"); pDelayTimeR = raw ("delayTimeR");
    pDelayDivL = raw ("delayDivL"); pDelayDivR = raw ("delayDivR"); pDelayFilter = raw ("delayFilter");
    pDistOn = raw ("distOn"); pReverbOn = raw ("reverbOn"); pReverbType = raw ("reverbType");
    pXyX = raw ("xyX"); pXyY = raw ("xyY"); pXOn = raw ("xOn"); pYOn = raw ("yOn");
    pXDepth = raw ("xDepth"); pYDepth = raw ("yDepth");
    pXyTarget = { raw ("xTarget1"), raw ("xTarget2"), raw ("yTarget1"), raw ("yTarget2") };
}

bool StepFxProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet(), out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo()) return false;
    return in == out || (in == juce::AudioChannelSet::mono() && out == juce::AudioChannelSet::stereo());
}

void StepFxProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sampleRateHz = sampleRate > 0.0 ? sampleRate : 44100.0;
    work.setSize (4, juce::jmax (16, samplesPerBlock), false, true, true);
    filter.prepare (sampleRateHz);
    modFx.prepare (sampleRateHz);
    delay.prepare (sampleRateHz);
    exciter.prepare (sampleRateHz);
    dirt.prepare (sampleRateHz);
    reverb.prepare (sampleRateHz);
    reset();
}

void StepFxProcessor::reset()
{
    filter.reset(); modFx.reset(); delay.reset(); exciter.reset(); dirt.reset(); reverb.reset();
    smInit = false;
    gateSm = 1.0f; panSm = 0.0f;
    internalPpq = 0.0;
}

void StepFxProcessor::setPattern (int mod, const PatternData& p, const juce::String& name)
{
    auto copy = p;
    copy.sanitise();
    patterns[(size_t) mod].store (copy);
    patternNames[(size_t) mod] = name;
}

std::array<int, kNumBlocks> StepFxProcessor::getOrder() const
{
    std::array<int, kNumBlocks> o {};
    for (int i = 0; i < kNumBlocks; ++i) o[(size_t) i] = order[(size_t) i].load();
    return o;
}

void StepFxProcessor::setOrder (const std::array<int, kNumBlocks>& newOrder)
{
    // only accept valid permutations
    std::array<bool, kNumBlocks> seen {};
    for (int v : newOrder)
    {
        if (v < 0 || v >= kNumBlocks || seen[(size_t) v]) return;
        seen[(size_t) v] = true;
    }
    for (int i = 0; i < kNumBlocks; ++i) order[(size_t) i].store (newOrder[(size_t) i]);
}

//==============================================================================
void StepFxProcessor::updateEffects (float sr, double bpm, const float* eff)
{
    juce::ignoreUnused (sr);
    auto val = [&] (int t) { return targetParam[(size_t) t]->convertFrom0to1 (juce::jlimit (0.0f, 1.0f, eff[t])); };

    filter.setParams ((int) pFilterType->load(), val (tFilterCutoff), eff[tFilterRes], eff[tFilterDrive]);
    modFx.setParams (val (tModDelay), val (tModRate), eff[tModDepth], eff[tModFeedback], eff[tModMix]);

    float secL, secR;
    if (pDelaySync->load() > 0.5f)
    {
        const double spb = 60.0 / juce::jmax (20.0, bpm);
        secL = (float) (delayDivBeats ((int) pDelayDivL->load()) * spb);
        secR = (float) (delayDivBeats ((int) pDelayDivR->load()) * spb);
    }
    else
    {
        secL = pDelayTimeL->load() * 0.001f;
        secR = pDelayTimeR->load() * 0.001f;
    }
    delay.setParams (secL, secR, eff[tDelayFeedback], (int) pDelayFilter->load(), val (tDelayCutoff), eff[tDelayMix]);
    exciter.setAmount (eff[tExciter]);
    dirt.setAmount (eff[tDirt]);
    reverb.setParams ((int) pReverbType->load(), val (tReverbTime), eff[tReverbMix]);
}

void StepFxProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numIn = getTotalNumInputChannels(), numOut = getTotalNumOutputChannels();
    const int n = buffer.getNumSamples();
    if (n == 0 || numOut == 0) return;
    for (int c = numIn; c < numOut; ++c) buffer.clear (c, 0, n);
    if (work.getNumSamples() < n) work.setSize (4, n, false, true, true);

    const float sr = (float) sampleRateHz;

    // transport
    double bpm = 120.0, ppq = internalPpq;
    bool playing = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) bpm = *b;
            playing = pos->getIsPlaying();
            if (playing) if (auto q = pos->getPpqPosition()) ppq = *q;
        }
    if (! std::isfinite (bpm) || bpm < 20.0 || bpm > 999.0) bpm = 120.0;
    if (! std::isfinite (ppq)) ppq = internalPpq;
    const double ppqPerSample = bpm / 60.0 / sampleRateHz;

    // patterns
    std::array<bool, kNumMods> modOn {};
    std::array<int, kNumMods> modTarget {};
    std::array<double, kNumMods> modBeats {};
    for (int m = 0; m < kNumMods; ++m)
    {
        const auto mi = (size_t) m;
        modTarget[mi] = juce::jlimit (0, kNumModTargets - 1, (int) mp[mi].target->load());
        modOn[mi] = mp[mi].on->load() > 0.5f && modTarget[mi] != tNone;
        modBeats[mi] = rateBeats ((int) mp[mi].rate->load());
        playback[mi].prepare (patterns[mi].load(), mp[mi].swing->load() * 0.01f);
        if (! modOn[mi]) currentStep[mi].store (-1, std::memory_order_relaxed);
    }

    // copy input to work buffers: 0/1 = wet, 2/3 = dry
    const float* inL = buffer.getReadPointer (0);
    const float* inR = buffer.getReadPointer (numIn > 1 ? 1 : 0);
    float* wl = work.getWritePointer (0); float* wr = work.getWritePointer (1);
    float* dl = work.getWritePointer (2); float* dr = work.getWritePointer (3);
    for (int i = 0; i < n; ++i)
    {
        dl[i] = std::isfinite (inL[i]) ? inL[i] : 0.0f;
        dr[i] = std::isfinite (inR[i]) ? inR[i] : 0.0f;
    }

    const bool filterOn = pFilterOn->load() > 0.5f, modFxOn = pModOn->load() > 0.5f, delayOn = pDelayOn->load() > 0.5f,
               distOn = pDistOn->load() > 0.5f, reverbOn = pReverbOn->load() > 0.5f;
    const auto ord = getOrder();

    constexpr int sub = 16;
    const float ctlCoef = 1.0f - std::exp (-(float) sub / (sr * 0.004f));
    const float gateCoef = 1.0f - std::exp (-1.0f / (sr * 0.0012f));
    float gateArr[sub], panArr[sub];
    float eff[kNumTargets];

    for (int pos = 0; pos < n; pos += sub)
    {
        const int len = juce::jmin (sub, n - pos);
        float offs[kNumTargets] = {};

        // XY pad
        {
            const float x = pXyX->load(), y = pXyY->load();
            const float xd = pXOn->load() > 0.5f ? pXDepth->load() * 0.01f : 0.0f;
            const float yd = pYOn->load() > 0.5f ? pYDepth->load() * 0.01f : 0.0f;
            for (int k = 0; k < 4; ++k)
            {
                const int t = juce::jlimit (0, kNumTargets - 1, xyChoiceToTarget ((int) pXyTarget[(size_t) k]->load()));
                if (t != tNone) offs[t] += k < 2 ? x * xd : y * yd;
            }
        }

        // step modulators
        for (int i = 0; i < len; ++i)
        {
            const double q = ppq + (double) (pos + i) * ppqPerSample;
            float gMix = 1.0f, gAdd = 0.0f, pan = offs[tPan];
            bool anyMix = false, anyAdd = false;
            for (int m = 0; m < kNumMods; ++m)
            {
                const auto mi = (size_t) m;
                if (! modOn[mi]) continue;
                const int steps = playback[mi].p.numSteps;
                double x = std::fmod (q / modBeats[mi], (double) steps);
                if (x < 0.0) x += (double) steps;
                if (x >= (double) steps) x = 0.0;
                const auto v = playback[mi].evaluate ((float) x);
                const float depth = juce::jlimit (-1.0f, 1.0f, mp[mi].depth->load() * 0.01f + offs[tMod1Depth + m]);
                switch (modTarget[mi])
                {
                    case tGateMix:
                    {
                        const float vv = depth < 0.0f ? 1.0f - v.value : v.value;
                        gMix *= 1.0f - std::abs (depth) * (1.0f - vv);
                        anyMix = true;
                        break;
                    }
                    case tGateAdd:
                    {
                        const float vv = depth < 0.0f ? 1.0f - v.value : v.value;
                        gAdd += std::abs (depth) * vv;
                        anyAdd = true;
                        break;
                    }
                    case tPan: pan += depth * (2.0f * v.level - 1.0f) * v.env; break;
                    default:   if (i == 0) offs[modTarget[mi]] += depth * v.value; break;
                }
                if (i == len - 1) currentStep[mi].store (v.step, std::memory_order_relaxed);
            }
            float g = anyAdd ? (anyMix ? gMix : 0.0f) + gAdd : gMix;
            g = juce::jlimit (0.0f, 1.0f, g);
            gateSm += gateCoef * (g - gateSm);
            panSm += gateCoef * (juce::jlimit (-1.0f, 1.0f, pan) - panSm);
            gateArr[i] = gateSm; panArr[i] = panSm;
        }

        // effective (modulated) parameter values, normalised
        for (int t = 0; t < kNumTargets; ++t)
        {
            float e = 0.0f;
            if (targetParam[(size_t) t] != nullptr)
                e = juce::jlimit (0.0f, 1.0f, targetParam[(size_t) t]->convertTo0to1 (targetRaw[(size_t) t]->load()) + offs[t]);
            if (! smInit) effSm[(size_t) t] = e;
            effSm[(size_t) t] += ctlCoef * (e - effSm[(size_t) t]);
            eff[t] = effSm[(size_t) t];
        }
        smInit = true;
        updateEffects (sr, bpm, eff);

        auto val = [&] (int t) { return targetParam[(size_t) t]->convertFrom0to1 (juce::jlimit (0.0f, 1.0f, eff[t])); };
        const float inGain = juce::Decibels::decibelsToGain (val (tMasterInput));
        const float outGain = juce::Decibels::decibelsToGain (val (tMasterOutput));
        const float mix = eff[tMasterMix];

        float* l = wl + pos; float* r = wr + pos;
        for (int i = 0; i < len; ++i) { l[i] = dl[pos + i] * inGain; r[i] = dr[pos + i] * inGain; }

        for (int b = 0; b < kNumBlocks; ++b)
        {
            switch (ord[(size_t) b])
            {
                case bGate:   for (int i = 0; i < len; ++i) { l[i] *= gateArr[i]; r[i] *= gateArr[i]; } break;
                case bPan:
                    for (int i = 0; i < len; ++i)
                    {
                        l[i] *= juce::jmin (1.0f, 1.0f - panArr[i]);
                        r[i] *= juce::jmin (1.0f, 1.0f + panArr[i]);
                    }
                    break;
                case bModFx:   if (modFxOn) modFx.process (l, r, len); break;
                case bDelay:   if (delayOn) delay.process (l, r, len); break;
                case bFilter:  if (filterOn) filter.process (l, r, len); break;
                case bExciter: if (distOn) exciter.process (l, r, len); break;
                case bDirt:    if (distOn) dirt.process (l, r, len); break;
                case bReverb:  if (reverbOn) reverb.process (l, r, len); break;
                default: break;
            }
        }

        for (int i = 0; i < len; ++i)
        {
            l[i] = sanitize ((dl[pos + i] * (1.0f - mix) + l[i] * mix) * outGain);
            r[i] = sanitize ((dr[pos + i] * (1.0f - mix) + r[i] * mix) * outGain);
        }
    }

    internalPpq = ppq + (double) n * ppqPerSample;
    if (internalPpq > 1.0e7) internalPpq = std::fmod (internalPpq, 3840.0 * 16.0);

    for (int t = 0; t < kNumTargets; ++t) effectiveNorm[(size_t) t].store (eff[t], std::memory_order_relaxed);
    gateDisplay.store (gateSm, std::memory_order_relaxed);
    panDisplay.store (panSm, std::memory_order_relaxed);

    if (numOut >= 2)
    {
        buffer.copyFrom (0, 0, wl, n);
        buffer.copyFrom (1, 0, wr, n);
    }
    else
    {
        float* o = buffer.getWritePointer (0);
        for (int i = 0; i < n; ++i) o[i] = 0.5f * (wl[i] + wr[i]);
    }
}

//==============================================================================
void StepFxProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    while (state.getChildWithName ("Extra").isValid())
        state.removeChild (state.getChildWithName ("Extra"), nullptr);

    juce::ValueTree extra ("Extra");
    juce::StringArray o;
    for (int v : getOrder()) o.add (juce::String (v));
    extra.setProperty ("order", o.joinIntoString (" "), nullptr);
    for (int m = 0; m < kNumMods; ++m)
    {
        auto t = patterns[(size_t) m].load().toValueTree ("Pattern");
        t.setProperty ("name", patternNames[(size_t) m], nullptr);
        extra.appendChild (t, nullptr);
    }
    state.appendChild (extra, nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void StepFxProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return;
    auto state = juce::ValueTree::fromXml (*xml);
    if (! state.isValid()) return;

    auto extra = state.getChildWithName ("Extra");
    if (extra.isValid())
    {
        auto tokens = juce::StringArray::fromTokens (extra.getProperty ("order").toString(), " ", "");
        if (tokens.size() == kNumBlocks)
        {
            std::array<int, kNumBlocks> o {};
            for (int i = 0; i < kNumBlocks; ++i) o[(size_t) i] = tokens[i].getIntValue();
            setOrder (o);
        }
        int m = 0;
        for (auto child : extra)
        {
            if (! child.hasType ("Pattern") || m >= kNumMods) continue;
            setPattern (m, PatternData::fromValueTree (child), child.getProperty ("name", "Custom").toString());
            ++m;
        }
        state.removeChild (extra, nullptr);
    }
    apvts.replaceState (state);
    stateVersion.fetch_add (1);
}

juce::AudioProcessorEditor* StepFxProcessor::createEditor() { return new StepFxEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new StepFxProcessor(); }
