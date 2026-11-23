#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Pattern.h"
#include "Dsp.h"

namespace sfx
{
enum Target
{
    tNone = 0, tGateMix, tGateAdd, tPan,
    tFilterCutoff, tFilterRes, tFilterDrive, tFilterMix,
    tModDelay, tModRate, tModDepth, tModFeedback, tModMix,
    tDelayFeedback, tDelayCutoff, tDelayMix,
    tExciter, tDirt,
    tReverbTime, tReverbMix,
    tMasterInput, tMasterMix, tMasterOutput,
    tMod1Depth, tMod2Depth, tMod3Depth,
    kNumTargets
};

constexpr int kNumModTargets = tMasterOutput + 1; // step modulators can not target modulator depths
const char* targetName (int t);
const char* targetParamId (int t); // nullptr when the target has no knob
juce::StringArray modTargetNames();
juce::StringArray xyTargetNames();
inline int xyChoiceToTarget (int choice) { return choice <= 0 ? (int) tNone : choice + 2; }

enum Block { bGate = 0, bPan, bModFx, bDelay, bFilter, bExciter, bDirt, bReverb, kNumBlocks };
const char* blockName (int b);

juce::StringArray rateNames();
double rateBeats (int index);
juce::StringArray delayDivNames();
double delayDivBeats (int index);
}

class StepFxProcessor : public juce::AudioProcessor
{
public:
    StepFxProcessor();
    ~StepFxProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void reset() override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 12.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==========================================================================
    juce::AudioProcessorValueTreeState apvts;

    // pattern access (message thread writes, audio thread reads)
    sfx::PatternData getPattern (int mod) const { return patterns[(size_t) mod].load(); }
    void setPattern (int mod, const sfx::PatternData& p, const juce::String& name);
    juce::String getPatternName (int mod) const { return patternNames[(size_t) mod]; }

    std::array<int, sfx::kNumBlocks> getOrder() const;
    void setOrder (const std::array<int, sfx::kNumBlocks>& newOrder);

    // display values written by the audio thread
    std::array<std::atomic<int>, sfx::kNumMods> currentStep;
    std::array<std::atomic<float>, sfx::kNumTargets> effectiveNorm;
    std::atomic<float> gateDisplay { 1.0f }, panDisplay { 0.0f };
    std::atomic<int> stateVersion { 0 }; // bumped whenever patterns/order are replaced from outside the editor

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

private:
    void updateEffects (float sr, double bpm, const float* eff);

    std::array<sfx::SharedPattern, sfx::kNumMods> patterns;
    std::array<juce::String, sfx::kNumMods> patternNames;
    std::array<std::atomic<int>, sfx::kNumBlocks> order;

    // cached parameter pointers
    struct ModParams { std::atomic<float>* on, *target, *depth, *rate, *swing; };
    std::array<ModParams, sfx::kNumMods> mp {};
    std::array<juce::RangedAudioParameter*, sfx::kNumTargets> targetParam {};
    std::array<std::atomic<float>*, sfx::kNumTargets> targetRaw {};
    std::atomic<float>* pFilterOn = nullptr, *pFilterType = nullptr, *pModOn = nullptr, *pDelayOn = nullptr,
        *pDelaySync = nullptr, *pDelayTimeL = nullptr, *pDelayTimeR = nullptr, *pDelayDivL = nullptr, *pDelayDivR = nullptr,
        *pDelayFilter = nullptr, *pDistOn = nullptr, *pReverbOn = nullptr, *pReverbType = nullptr,
        *pXyX = nullptr, *pXyY = nullptr, *pXOn = nullptr, *pYOn = nullptr;
    std::array<std::atomic<float>*, 4> pXyTarget {}, pXyDepth {};

    // dsp
    sfx::MultiFilter filter;
    sfx::ModFx modFx;
    sfx::StereoDelay delay;
    sfx::Exciter exciter;
    sfx::Dirt dirt;
    sfx::FdnReverb reverb;

    std::array<sfx::PlaybackPattern, sfx::kNumMods> playback;
    juce::AudioBuffer<float> work;
    std::array<float, sfx::kNumTargets> effSm {};
    bool smInit = false;
    float gateSm = 1.0f, panSm = 0.0f;
    double internalPpq = 0.0;
    double sampleRateHz = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StepFxProcessor)
};
