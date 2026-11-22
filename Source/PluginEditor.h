#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

namespace ui
{
namespace col
{
    const juce::Colour bg { 0xff16181d }, panel { 0xff22262e }, panelHi { 0xff2b303a }, border { 0xff363c48 },
        text { 0xffd9dde6 }, dim { 0xff7f8796 }, accent { 0xff2fd6b5 }, accentDim { 0xff1b6f61 },
        ring { 0xffffb347 }, lane { 0xff1a1d23 }, warn { 0xffff6b6b };
}

class Laf : public juce::LookAndFeel_V4
{
public:
    Laf();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float startAngle, float endAngle, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool highlighted, bool down) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float minPos, float maxPos, juce::Slider::SliderStyle, juce::Slider&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override { return juce::Font (juce::FontOptions (12.0f)); }
    juce::Font getTextButtonFont (juce::TextButton&, int) override { return juce::Font (juce::FontOptions (12.0f)); }
    juce::Font getPopupMenuFont() override { return juce::Font (juce::FontOptions (13.0f)); }
    juce::Font getLabelFont (juce::Label& l) override { return l.getFont(); }
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    juce::Label* createSliderTextBox (juce::Slider&) override;
};

//==============================================================================
class Knob : public juce::Component
{
public:
    Knob (juce::AudioProcessorValueTreeState& s, const juce::String& paramId, const juce::String& captionText, int targetIndex = -1);
    void resized() override;
    void setCaption (const juce::String& t);
    void attach (const juce::String& paramId);

    juce::Slider slider;
    juce::Label caption;
    int target = -1;

private:
    juce::AudioProcessorValueTreeState& state;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
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
    float noteEndX (int first, int last) const;
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
    juce::Rectangle<float> plot() const { return getLocalBounds().toFloat().reduced (8.0f, 8.0f); }
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
    void stepPreset (int delta);
    void loadPresetByName (const juce::String& name);
    void beginSaveAs();
    void endSaveAs (bool save);
    juce::StringArray userPatternNames() const;
    static juce::File userPatternDir();
    void updateRings();
    void updateDynamicLayout();
    void layoutPanels();

    StepFxProcessor& proc;
    ui::Laf laf;
    int sel = 0;
    int seenStateVersion = 0;
    int lastFilterType = -1;
    bool lastSync = false, firstLayout = true;
    juce::Random rng;

    struct PanelRect { juce::Rectangle<int> r; juce::String title; };
    std::vector<PanelRect> panels;

    // processors
    juce::OwnedArray<ui::Knob> knobs;
    ui::Knob* kFilter[3] {};
    ui::Knob* kDelayMsL = nullptr, *kDelayMsR = nullptr, *kDelayDivL = nullptr, *kDelayDivR = nullptr;
    juce::ToggleButton filterOn, modOn, delayOn, distOn, reverbOn, delaySync { "Sync" };
    juce::ComboBox filterType, delayFilter, reverbType;
    juce::OwnedArray<APVTS::ButtonAttachment> buttonAtts;
    juce::OwnedArray<APVTS::ComboBoxAttachment> comboAtts;
    juce::OwnedArray<APVTS::SliderAttachment> sliderAtts;

    // xy
    ui::XYPad xyPad;
    juce::ToggleButton xOn { "X" }, yOn { "Y" };
    juce::ComboBox xyTarget[4];
    juce::Slider xDepth, yDepth;

    // modulators
    juce::ToggleButton modOnBtn[sfx::kNumMods];
    juce::TextButton modSelBtn[sfx::kNumMods];
    juce::ComboBox modTarget[sfx::kNumMods];
    ui::Knob depthKnob, rateKnob, swingKnob;
    juce::TextButton prevBtn { "<" }, nextBtn { ">" }, presetBtn, actionBtn { "Actions" };
    juce::TextEditor nameEditor;
    juce::TextButton saveBtn { "Save" }, cancelBtn { "Cancel" };
    ui::StepEditor stepEditor;
    ui::EnvelopeDisplay envDisplay;
    juce::Slider envA, envH, envR;
    juce::Label envLabels[3], lengthLabel;
    ui::OrderStrip orderStrip;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StepFxEditor)
};
