#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

namespace ui
{
namespace col
{
    const juce::Colour panelA { 0xff633a34 }, panelB { 0xff57302b }, panelDark { 0xff4a2a26 }, sep { 0xff3a211e },
        rowA { 0xff4b2622 }, rowB { 0xff3c1d1b }, rowSel { 0xff5d342e }, strip { 0xff4a1e1b }, stripBtn { 0xff6b3d38 },
        lane { 0xff1b1d26 }, text { 0xffe9e1dd }, dim { 0xff9c8580 }, amber { 0xfff5b041 }, amberDim { 0xff9a6a35 },
        knobA { 0xff5b3b35 }, knobB { 0xff3f2420 }, blue { 0xff5b8fd6 }, blueHi { 0xff68a3ff }, blueLine { 0xff5b9cf0 },
        stepOff { 0xff222938 }, numOff { 0xff35415c }, num { 0xffd3d7df };
}

class Laf : public juce::LookAndFeel_V4
{
public:
    Laf();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float startAngle, float endAngle, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool highlighted, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool highlighted, bool down) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float minPos, float maxPos, juce::Slider::SliderStyle, juce::Slider&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override { return juce::Font (juce::FontOptions (13.5f)); }
    juce::Font getLabelFont (juce::Label& l) override { return l.getFont(); }
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    juce::Label* createSliderTextBox (juce::Slider&) override;
};

//==============================================================================
/** Rotary knob with a caption above; the caption shows the value while the pointer is over the knob. */
class Knob : public juce::Component
{
public:
    Knob (juce::AudioProcessorValueTreeState& s, const juce::String& paramId, const juce::String& captionText, int targetIndex = -1);
    ~Knob() override;
    void resized() override;
    void setCaption (const juce::String& t);
    void attach (const juce::String& paramId);
    void mouseEnter (const juce::MouseEvent&) override { hover = true; updateCaption(); }
    void mouseExit (const juce::MouseEvent&) override { hover = false; updateCaption(); }
    void updateCaption();

    juce::Slider slider;
    juce::Label caption;
    int target = -1;

private:
    juce::AudioProcessorValueTreeState& state;
    juce::RangedAudioParameter* param = nullptr;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
    juce::String name;
    bool hover = false;
};

//==============================================================================
class XYPad : public juce::Component
{
public:
    explicit XYPad (juce::AudioProcessorValueTreeState& s);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    void setFromMouse (const juce::MouseEvent&);
    juce::RangedAudioParameter* px, *py;
};

//==============================================================================
class StepEditor : public juce::Component, private juce::ScrollBar::Listener
{
public:
    StepEditor();
    void setPattern (const sfx::PatternData& p);
    const sfx::PatternData& getPattern() const { return pat; }
    void setPlayStep (int s);
    std::function<void()> onChange;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    // geometry helpers (public for tests)
    float stepWidth() const;
    juce::Rectangle<int> barsArea() const;
    juce::Rectangle<int> numbersArea() const;
    float stepX (int step) const; // component x of the left edge of a step

private:
    enum class Mode { none, level, length, toggle, handle };
    void scrollBarMoved (juce::ScrollBar*, double) override;
    void updateScroll();
    int stepAtX (float x) const;
    void noteRange (int step, int& first, int& last) const;
    float noteEndX (int last) const;
    void setLevelAt (int step, float y);
    void changed();

    sfx::PatternData pat;
    juce::ScrollBar scroll { false };
    float scrollX = 0.0f;
    int playStep = -1;
    Mode mode = Mode::none;
    int dragStep = -1, dragChainEnd = -1, lastStep = -1;
};

//==============================================================================
class EnvelopeDisplay : public juce::Component
{
public:
    void setPattern (const sfx::PatternData& p) { pat = p; repaint(); }
    const sfx::PatternData& getPattern() const { return pat; }
    std::function<void()> onChange;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override { drag = -1; }

    static void setAttack (sfx::PatternData&, float);
    static void setHold (sfx::PatternData&, float);
    static void setRelease (sfx::PatternData&, float);

private:
    juce::Rectangle<float> plot() const { return getLocalBounds().toFloat().withTrimmedBottom (26.0f).reduced (14.0f, 10.0f); }
    juce::Point<float> dot (int index) const;
    sfx::PatternData pat;
    int drag = -1;
    float dragStartCurve = 0.0f;
};

//==============================================================================
class OrderStrip : public juce::Component
{
public:
    explicit OrderStrip (StepFxProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override { dragging = -1; repaint(); }
    std::array<bool, sfx::kNumBlocks> active {};

private:
    juce::Rectangle<float> slot (int index) const;
    int slotAt (float x) const;
    StepFxProcessor& proc;
    int dragging = -1;
};
} // namespace ui

//==============================================================================
class StepFxEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit StepFxEditor (StepFxProcessor&);
    ~StepFxEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

    // exposed for tests
    void selectMod (int m);
    void runAction (int actionId);
    ui::StepEditor& getStepEditor() { return stepEditor; }

    enum Action { aRandomFull = 1, aRandomDev, aShiftLeft, aShiftRight, aDouble, aReverse, aInvert, aAppendDup, aAppendRev };

private:
    using APVTS = juce::AudioProcessorValueTreeState;
    void timerCallback() override;
    void pushPattern (const sfx::PatternData& p, const juce::String& name);
    void refreshFromProcessor();
    void updateEnvFields();
    void showPresetMenu();
    void showActionMenu();
    void showGlobalPresetMenu();
    void saveGlobalPreset();
    void stepPreset (int delta);
    void loadPresetByName (const juce::String& name);
    void beginSaveAs();
    void endSaveAs (bool save);
    juce::StringArray userPatternNames() const;
    static juce::File userPatternDir();
    void updateRings();
    void updateDynamicLayout();
    ui::Knob* addKnob (const char* id, const char* caption, int target, int cx, int cy);

    StepFxProcessor& proc;
    ui::Laf laf;
    int sel = 0;
    int seenStateVersion = 0;
    int lastFilterType = -1, lastSync = -1, lastOnMask = -1, lastUpdateState = -1;
    juce::Random rng;

    // processors
    juce::OwnedArray<ui::Knob> knobs;
    ui::Knob* kFilter[3] {};
    ui::Knob* kFilterMix = nullptr;
    ui::Knob* kDelayMsL = nullptr, *kDelayMsR = nullptr, *kDelayDivL = nullptr, *kDelayDivR = nullptr;
    std::vector<ui::Knob*> groupMod, groupDelay, groupFilter, groupDist, groupReverb;
    juce::ToggleButton filterOn, modOn, delayOn, distOn, reverbOn, delaySync;
    juce::ComboBox filterType, delayFilter, reverbType;
    juce::OwnedArray<APVTS::ButtonAttachment> buttonAtts;
    juce::OwnedArray<APVTS::ComboBoxAttachment> comboAtts;
    juce::OwnedArray<APVTS::SliderAttachment> sliderAtts;

    // xy
    ui::XYPad xyPad;
    juce::ToggleButton xOn, yOn;
    juce::ComboBox xyTarget[4];
    juce::Slider xyDepth[4];

    // modulators
    juce::ToggleButton modOnBtn[sfx::kNumMods];
    juce::ComboBox modTarget[sfx::kNumMods];
    ui::Knob depthKnob, rateKnob, swingKnob;
    juce::TextButton prevBtn, nextBtn, presetBtn, actionBtn;
    juce::TextEditor nameEditor;
    juce::TextButton saveBtn { "Save" }, cancelBtn { "Cancel" };
    ui::StepEditor stepEditor;
    ui::EnvelopeDisplay envDisplay;
    juce::Slider envA, envH, envR;
    ui::OrderStrip orderStrip;
    juce::TextButton globalPresetBtn;

    // self update: asks GitHub for the newest published build and can install it in place
    struct Updater : juce::Thread
    {
        enum State { idle = 0, available, installing, installed, failed };
        Updater() : juce::Thread ("StepFlux updater") {}
        ~Updater() override { stopThread (15000); }
        void run() override;
        bool check();
        bool install();
        std::atomic<int> latest { 0 }, state { idle };
        std::atomic<bool> wantInstall { false };
    };
    Updater updater;
    juce::TextButton updateBtn;
    juce::Label buildLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StepFxEditor)
};
