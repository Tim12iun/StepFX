#include "PluginEditor.h"

using namespace sfx;
using namespace ui;

namespace
{
constexpr int kW = 1100, kH = 718;
// zone geometry (x, y, w, h)
constexpr int kRow1H = 262, kRow2Y = 262, kRow2H = 158, kRow3Y = 420, kRow3H = 111, kStepY = 531, kStepH = 148, kStripY = 679;
constexpr int kColA = 296, kColB = 556, kColC = 816; // column borders of the two processor rows
constexpr int kTargetW = 218, kTargetRowH = 37;
constexpr float kStart = juce::MathConstants<float>::pi * 1.25f, kEnd = juce::MathConstants<float>::pi * 2.75f;
PatternData gClipboard;
bool gClipboardValid = false;

juce::Font fontOf (float size, bool bold = false)
{
    return juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain));
}

float textWidth (const juce::Font& f, const juce::String& t)
{
    juce::GlyphArrangement ga;
    ga.addLineOfText (f, t, 0.0f, 0.0f);
    return ga.getBoundingBox (0, -1, true).getWidth();
}

void drawChevrons (juce::Graphics& g, float cx, float cy, juce::Colour c)
{
    juce::Path p;
    p.startNewSubPath (cx - 3.0f, cy - 2.0f); p.lineTo (cx, cy - 5.0f); p.lineTo (cx + 3.0f, cy - 2.0f);
    p.startNewSubPath (cx - 3.0f, cy + 2.0f); p.lineTo (cx, cy + 5.0f); p.lineTo (cx + 3.0f, cy + 2.0f);
    g.setColour (c);
    g.strokePath (p, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void fillZone (juce::Graphics& g, juce::Rectangle<int> r, juce::Colour top, juce::Colour bottom)
{
    g.setGradientFill (juce::ColourGradient (top, 0.0f, (float) r.getY(), bottom, 0.0f, (float) r.getBottom(), false));
    g.fillRect (r);
}
}

//==============================================================================
Laf::Laf()
{
    setColour (juce::ResizableWindow::backgroundColourId, col::rowB);
    setColour (juce::Label::textColourId, col::text);
    setColour (juce::ComboBox::textColourId, col::amber);
    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff2a1a18));
    setColour (juce::PopupMenu::textColourId, col::text);
    setColour (juce::PopupMenu::headerTextColourId, col::dim);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, col::blue);
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
    setColour (juce::TextButton::buttonColourId, juce::Colour (0xff3a2320));
    setColour (juce::TextButton::textColourOffId, col::text);
    setColour (juce::TextButton::textColourOnId, col::text);
    setColour (juce::TextEditor::backgroundColourId, col::lane);
    setColour (juce::TextEditor::textColourId, col::text);
    setColour (juce::TextEditor::outlineColourId, col::sep);
    setColour (juce::TextEditor::focusedOutlineColourId, col::amber);
    setColour (juce::TextEditor::highlightColourId, col::blue.withAlpha (0.5f));
    setColour (juce::CaretComponent::caretColourId, col::amber);
    setColour (juce::ScrollBar::thumbColourId, juce::Colour (0xff3a4560));
}

void Laf::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider& s)
{
    const auto b = juce::Rectangle<int> (x, y, w, h).toFloat();
    const float radius = juce::jmin (b.getWidth(), b.getHeight()) * 0.5f - 5.5f;
    const auto c = b.getCentre();
    auto arc = [&] (float from, float to, float r, float thickness, juce::Colour colour)
    {
        if (std::abs (to - from) < 0.01f) return;
        juce::Path p;
        p.addCentredArc (c.x, c.y, r, r, 0.0f, juce::jmin (from, to), juce::jmax (from, to), true);
        g.setColour (colour);
        g.strokePath (p, juce::PathStrokeType (thickness, juce::PathStrokeType::curved, juce::PathStrokeType::butt));
    };
    const float angle = a0 + pos * (a1 - a0);
    const float bodyR = radius - 2.0f;
    g.setGradientFill (juce::ColourGradient (col::knobA, c.x, c.y - bodyR, col::knobB, c.x, c.y + bodyR, false));
    g.fillEllipse (c.x - bodyR, c.y - bodyR, bodyR * 2.0f, bodyR * 2.0f);
    g.setColour (juce::Colour (0xff1c100f));
    g.drawEllipse (c.x - bodyR, c.y - bodyR, bodyR * 2.0f, bodyR * 2.0f, 1.2f);

    // value arc: bipolar parameters grow from the centre position
    const bool bipolar = s.getMinimum() < 0.0 && s.getMaximum() > 0.0;
    const float from = bipolar ? a0 + (float) s.valueToProportionOfLength (0.0) * (a1 - a0) : a0;
    arc (from, angle, radius, 4.6f, col::amber);

    const float pa = angle - juce::MathConstants<float>::halfPi;
    g.setColour (col::amber);
    g.drawLine (c.x, c.y, c.x + std::cos (pa) * (bodyR - 1.0f), c.y + std::sin (pa) * (bodyR - 1.0f), 1.6f);

    if ((bool) s.getProperties().getWithDefault ("mon", false))
    {
        const float lo = (float) s.getProperties()["mlo"], hi = (float) s.getProperties()["mhi"], cur = (float) s.getProperties()["mcur"];
        arc (a0 + lo * (a1 - a0), a0 + hi * (a1 - a0), radius + 4.5f, 2.0f, col::blueLine);
        const float ca = a0 + cur * (a1 - a0) - juce::MathConstants<float>::halfPi;
        g.setColour (juce::Colours::white);
        g.fillEllipse (c.x + std::cos (ca) * (radius + 5.0f) - 3.0f, c.y + std::sin (ca) * (radius + 5.0f) - 3.0f, 6.0f, 6.0f);
    }
}

void Laf::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool)
{
    const auto r = b.getLocalBounds().toFloat();
    const bool on = b.getToggleState();
    const auto c = r.getCentre();
    if (b.getProperties()["icon"].toString() == "note")
    {
        g.setColour (juce::Colour (0xff2e1b19));
        g.fillRoundedRectangle (r.reduced (0.5f), 4.0f);
        g.setColour (on ? col::amber : col::dim.withAlpha (0.6f));
        // eighth note
        g.fillEllipse (c.x - 6.0f, c.y + 2.0f, 7.5f, 5.5f);
        g.fillRect (c.x + 0.4f, c.y - 8.0f, 1.6f, 12.5f);
        juce::Path flag;
        flag.startNewSubPath (c.x + 2.0f, c.y - 8.0f);
        flag.quadraticTo (c.x + 7.5f, c.y - 5.5f, c.x + 5.5f, c.y);
        g.strokePath (flag, juce::PathStrokeType (1.8f));
        return;
    }
    // power symbol
    const float rad = juce::jmin (r.getWidth(), r.getHeight()) * 0.5f - 4.5f;
    juce::Path p;
    p.addCentredArc (c.x, c.y + 0.5f, rad, rad, 0.0f, 0.55f, juce::MathConstants<float>::twoPi - 0.55f, true);
    p.startNewSubPath (c.x, c.y - rad - 2.0f);
    p.lineTo (c.x, c.y - 0.5f);
    g.setColour (on ? (highlighted ? col::amber.brighter (0.2f) : col::amber) : col::dim.withAlpha (highlighted ? 0.9f : 0.65f));
    g.strokePath (p, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void Laf::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour& base, bool highlighted, bool down)
{
    const auto style = b.getProperties()["style"].toString();
    if (style == "flat") return;
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    auto c = base;
    if (down) c = c.brighter (0.25f); else if (highlighted) c = c.brighter (0.12f);
    g.setColour (c);
    juce::Path p;
    const bool left = style != "next", right = style != "prev";
    p.addRoundedRectangle (r.getX(), r.getY(), r.getWidth(), r.getHeight(), 4.0f, 4.0f, left, right, left, right);
    g.fillPath (p);
}

void Laf::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool highlighted, bool)
{
    const auto style = b.getProperties()["style"].toString();
    const auto r = b.getLocalBounds().toFloat();
    const auto c = r.getCentre();
    const auto icon = col::text.withAlpha (highlighted ? 1.0f : 0.75f);
    if (style == "flat")
    {
        const auto f = fontOf (15.0f);
        g.setFont (f);
        g.setColour (highlighted ? col::amber.brighter (0.2f) : col::amber);
        const auto text = b.getButtonText();
        const float tw = juce::jmin (textWidth (f, text), r.getWidth() - 16.0f);
        const float x0 = c.x - (tw + 14.0f) * 0.5f;
        g.drawText (text, juce::Rectangle<float> (x0, r.getY(), tw + 2.0f, r.getHeight()), juce::Justification::centredLeft, true);
        drawChevrons (g, x0 + tw + 10.0f, c.y, col::amber);
    }
    else if (style == "prev" || style == "next")
    {
        const float d = style == "prev" ? 1.0f : -1.0f;
        juce::Path p;
        p.startNewSubPath (c.x + 2.5f * d, c.y - 5.0f); p.lineTo (c.x - 2.5f * d, c.y); p.lineTo (c.x + 2.5f * d, c.y + 5.0f);
        g.setColour (icon);
        g.strokePath (p, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    else if (style == "gear")
    {
        juce::Path p;
        p.addStar (c, 8, 5.2f, 8.2f);
        p = p.createPathWithRoundedCorners (1.5f);
        g.setColour (icon);
        g.fillPath (p);
        g.fillEllipse (c.x - 5.8f, c.y - 5.8f, 11.6f, 11.6f);
        g.setColour (b.findColour (juce::TextButton::buttonColourId));
        g.fillEllipse (c.x - 2.4f, c.y - 2.4f, 4.8f, 4.8f);
    }
    else
    {
        g.setFont (fontOf (13.0f));
        g.setColour (col::text);
        g.drawText (b.getButtonText(), r, juce::Justification::centred, false);
    }
}

juce::Font Laf::getComboBoxFont (juce::ComboBox& box)
{
    return fontOf ((float) box.getProperties().getWithDefault ("fontSize", 15.0));
}

void Laf::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    const auto colour = box.findColour (juce::ComboBox::textColourId);
    float cx = (float) w - 9.0f;
    if (! (bool) box.getProperties().getWithDefault ("chevRight", false))
        cx = juce::jmin (cx, 2.0f + textWidth (getComboBoxFont (box), box.getText()) + 10.0f);
    drawChevrons (g, cx, (float) h * 0.5f, colour.withAlpha (box.isEnabled() ? 1.0f : 0.5f));
}

void Laf::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (0, 0, box.getWidth() - 16, box.getHeight());
    label.setFont (getComboBoxFont (box));
    label.setMinimumHorizontalScale (0.75f);
    label.setBorderSize (juce::BorderSize<int> (0, 2, 0, 0));
}

void Laf::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float, juce::Slider::SliderStyle style, juce::Slider& s)
{
    if (style == juce::Slider::LinearBar) return; // value fields: only the text is shown, drag to change
    LookAndFeel_V4::drawLinearSlider (g, x, y, w, h, pos, 0, 0, style, s);
}

juce::Label* Laf::createSliderTextBox (juce::Slider& s)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (s);
    const bool blue = (bool) s.getProperties().getWithDefault ("blue", false);
    l->setFont (fontOf (14.0f));
    l->setJustificationType (blue ? juce::Justification::centredLeft : juce::Justification::centredRight);
    l->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
    l->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    l->setColour (juce::Label::textColourId, blue ? col::blueLine : col::amber);
    l->setColour (juce::Label::outlineWhenEditingColourId, col::amber);
    l->setColour (juce::Label::backgroundWhenEditingColourId, col::lane);
    l->setColour (juce::TextEditor::textColourId, col::text);
    l->setColour (juce::TextEditor::backgroundColourId, col::lane);
    return l;
}

//==============================================================================
Knob::Knob (juce::AudioProcessorValueTreeState& s, const juce::String& paramId, const juce::String& captionText, int targetIndex)
    : target (targetIndex), state (s), name (captionText)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setRotaryParameters (kStart, kEnd, true);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.addMouseListener (this, false);
    slider.onValueChange = [this] { updateCaption(); };
    slider.onDragEnd = [this] { updateCaption(); };
    addAndMakeVisible (slider);
    caption.setText (captionText, juce::dontSendNotification);
    caption.setJustificationType (juce::Justification::centred);
    caption.setFont (fontOf (14.0f));
    caption.setColour (juce::Label::textColourId, col::text);
    caption.setInterceptsMouseClicks (false, false);
    caption.setMinimumHorizontalScale (0.8f);
    addAndMakeVisible (caption);
    if (paramId.isNotEmpty()) attach (paramId);
}

Knob::~Knob() { slider.removeMouseListener (this); }

void Knob::attach (const juce::String& paramId)
{
    att.reset();
    param = state.getParameter (paramId);
    att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, paramId, slider);
    if (param != nullptr)
        slider.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));
    updateCaption();
}

void Knob::resized()
{
    auto r = getLocalBounds();
    caption.setBounds (r.removeFromTop (19));
    slider.setBounds (r.withSizeKeepingCentre (juce::jmin (r.getWidth(), r.getHeight()), r.getHeight()));
}

void Knob::setCaption (const juce::String& t)
{
    name = t;
    slider.setEnabled (t.isNotEmpty());
    setVisible (t.isNotEmpty());
    updateCaption();
}

void Knob::updateCaption()
{
    const bool showValue = param != nullptr && (hover || slider.isMouseButtonDown());
    caption.setText (showValue ? param->getCurrentValueAsText() : name, juce::dontSendNotification);
    caption.setColour (juce::Label::textColourId, showValue ? col::amber : col::text);
}

//==============================================================================
XYPad::XYPad (juce::AudioProcessorValueTreeState& s) : px (s.getParameter ("xyX")), py (s.getParameter ("xyY")) {}

void XYPad::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (juce::Colour (0xff3a4258));
    for (int i = 0; i <= 4; ++i)
    {
        const float x = r.getX() + r.getWidth() * (float) i / 4.0f, y = r.getY() + r.getHeight() * (float) i / 4.0f;
        g.fillRect (x - 0.5f, r.getY(), 1.0f, r.getHeight());
        g.fillRect (r.getX(), y - 0.5f, r.getWidth(), 1.0f);
    }
    auto inner = r.reduced (9.0f);
    const float x = inner.getX() + px->getValue() * inner.getWidth();
    const float y = inner.getBottom() - py->getValue() * inner.getHeight();
    g.setColour (col::blueLine.withAlpha (0.28f));
    g.fillEllipse (x - 11.0f, y - 11.0f, 22.0f, 22.0f);
    g.setColour (col::blueLine);
    g.drawEllipse (x - 11.0f, y - 11.0f, 22.0f, 22.0f, 1.0f);
    g.setColour (col::blueHi);
    g.fillEllipse (x - 5.0f, y - 5.0f, 10.0f, 10.0f);
}

void XYPad::setFromMouse (const juce::MouseEvent& e)
{
    auto inner = getLocalBounds().toFloat().reduced (9.5f);
    px->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, (e.position.x - inner.getX()) / inner.getWidth()));
    py->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, (inner.getBottom() - e.position.y) / inner.getHeight()));
    repaint();
}

void XYPad::mouseDown (const juce::MouseEvent& e) { px->beginChangeGesture(); py->beginChangeGesture(); setFromMouse (e); }
void XYPad::mouseDrag (const juce::MouseEvent& e) { setFromMouse (e); }
void XYPad::mouseUp (const juce::MouseEvent&) { px->endChangeGesture(); py->endChangeGesture(); }

//==============================================================================
namespace
{
constexpr float kStepPad = 16.0f, kHandleSpace = 44.0f;
}

StepEditor::StepEditor()
{
    addChildComponent (scroll);
    scroll.addListener (this);
    scroll.setAutoHide (false);
}

void StepEditor::setPattern (const PatternData& p)
{
    pat = p;
    updateScroll();
    repaint();
}

void StepEditor::setPlayStep (int s)
{
    if (s != playStep) { playStep = s; repaint(); }
}

juce::Rectangle<int> StepEditor::barsArea() const { return { 0, 7, getWidth(), juce::roundToInt ((float) getHeight() * 0.61f) }; }
juce::Rectangle<int> StepEditor::numbersArea() const { return { 0, barsArea().getBottom() + 10, getWidth(), 30 }; }

float StepEditor::stepWidth() const
{
    const float fit = ((float) getWidth() - kStepPad - kHandleSpace) / (float) juce::jmax (1, pat.numSteps);
    return juce::jlimit (20.0f, 80.0f, fit);
}

float StepEditor::stepX (int step) const { return kStepPad + (float) step * stepWidth() - scrollX; }

int StepEditor::stepAtX (float x) const { return (int) std::floor ((x - kStepPad + scrollX) / stepWidth()); }

void StepEditor::updateScroll()
{
    const float total = kStepPad + (float) pat.numSteps * stepWidth() + kHandleSpace;
    const float visible = (float) getWidth();
    scrollX = juce::jlimit (0.0f, juce::jmax (0.0f, total - visible), scrollX);
    scroll.setRangeLimits (0.0, (double) juce::jmax (total, visible), juce::dontSendNotification);
    scroll.setCurrentRange ((double) scrollX, (double) visible, juce::dontSendNotification);
    scroll.setVisible (total > visible + 0.5f);
}

void StepEditor::scrollBarMoved (juce::ScrollBar*, double start) { scrollX = (float) start; repaint(); }

void StepEditor::resized()
{
    scroll.setBounds (getLocalBounds().removeFromBottom (8).reduced (10, 0));
    updateScroll();
}

void StepEditor::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w)
{
    if (! scroll.isVisible()) return;
    const float d = std::abs (w.deltaX) > std::abs (w.deltaY) ? w.deltaX : w.deltaY;
    scrollX -= d * 240.0f;
    updateScroll();
    repaint();
}

void StepEditor::noteRange (int step, int& first, int& last) const
{
    first = last = step;
    if (! pat.on[(size_t) step]) return;
    while (first > 0 && pat.on[(size_t) (first - 1)] && pat.tie[(size_t) (first - 1)]) --first;
    while (last + 1 < pat.numSteps && pat.tie[(size_t) last] && pat.on[(size_t) (last + 1)]) ++last;
}

float StepEditor::noteEndX (int last) const { return stepX (last) + pat.length[(size_t) last] * stepWidth(); }

void StepEditor::paint (juce::Graphics& g)
{
    const auto bars = barsArea().toFloat();
    const auto nums = numbersArea().toFloat();
    const float sw = stepWidth();
    const float gap = juce::jlimit (2.0f, 5.0f, sw * 0.07f);
    g.fillAll (col::lane);
    g.saveState();
    g.reduceClipRegion (getLocalBounds().withTrimmedBottom (8));

    for (int i = 0; i < pat.numSteps; ++i)
    {
        const auto ii = (size_t) i;
        const float x = stepX (i);
        if (! pat.on[ii])
        {
            if (x + sw >= 0.0f && x <= (float) getWidth())
            {
                g.setColour (col::stepOff);
                g.fillRect (x, bars.getY(), sw - gap, bars.getHeight());
            }
            continue;
        }
        int first, last;
        noteRange (i, first, last);
        if (first != i) continue;
        const float x1 = noteEndX (last) - gap;
        if (x1 >= 0.0f && x <= (float) getWidth())
        {
            const bool playing = playStep >= first && playStep <= last;
            const float lh = juce::jmax (2.0f, pat.level[ii] * bars.getHeight());
            const float w = juce::jmax (2.0f, x1 - x);
            if (mode == Mode::length && dragStep == first)
            {
                g.setColour (col::blue.withAlpha (0.22f)); // shaded bar showing the step length while editing
                g.fillRect (x, bars.getY(), w, bars.getHeight());
            }
            g.setColour (playing ? col::blueHi : col::blue);
            g.fillRect (x, bars.getBottom() - lh, w, lh);
        }
        i = last;
    }

    // step numbers and tie markers
    g.setFont (fontOf (sw < 26.0f ? 10.0f : (sw < 40.0f ? 12.0f : 14.5f)));
    for (int i = 0; i < pat.numSteps; ++i)
    {
        const auto ii = (size_t) i;
        const float x = stepX (i);
        if (x + sw * 2.0f < 0.0f || x > (float) getWidth()) continue;
        const bool on = pat.on[ii];
        g.setColour (on ? (i == playStep ? juce::Colours::white : col::num) : col::numOff);
        g.drawText (juce::String (i + 1), juce::Rectangle<float> (x, nums.getY(), sw - gap, nums.getHeight()), juce::Justification::centred, false);
        if (on && i + 1 < pat.numSteps && pat.tie[ii] && pat.on[ii + 1])
        {
            const float cy = nums.getY() - 2.0f;
            const float xa = x + (sw - gap) * 0.5f, xb = xa + sw;
            g.setColour (col::blueLine);
            g.drawLine (xa, cy, xb, cy, 1.6f);
            g.fillEllipse (xa - 3.0f, cy - 3.0f, 6.0f, 6.0f);
            g.fillEllipse (xb - 3.0f, cy - 3.0f, 6.0f, 6.0f);
            g.setColour (col::num);
        }
    }

    // pattern length handle: a tab at the right end of the last step
    const float hx = stepX (pat.numSteps) + 1.0f;
    juce::Path tab;
    tab.addRoundedRectangle (hx, bars.getY(), 17.0f, bars.getHeight(), 9.0f, 9.0f, false, true, false, true);
    g.setColour (mode == Mode::handle ? juce::Colour (0xff4f77b8) : juce::Colour (0xff3a5788));
    g.fillPath (tab);
    g.setColour (col::lane);
    g.fillRoundedRectangle (hx + 6.5f, bars.getCentreY() - 12.0f, 3.0f, 24.0f, 1.5f);
    if (mode == Mode::handle)
    {
        g.setColour (col::num);
        g.setFont (fontOf (12.0f));
        g.drawText (juce::String (pat.numSteps), juce::Rectangle<float> (hx - 6.0f, nums.getY(), 30.0f, nums.getHeight()), juce::Justification::centred, false);
    }
    g.restoreState();
}

void StepEditor::changed()
{
    repaint();
    if (onChange) onChange();
}

void StepEditor::setLevelAt (int step, float y)
{
    if (step < 0 || step >= pat.numSteps) return;
    const auto bars = barsArea().toFloat();
    int first = step, last = step;
    noteRange (step, first, last);
    pat.level[(size_t) first] = juce::jlimit (0.0f, 1.0f, 1.0f - (y - bars.getY()) / bars.getHeight());
}

void StepEditor::mouseMove (const juce::MouseEvent& e)
{
    auto cursor = juce::MouseCursor::NormalCursor;
    const float hx = stepX (pat.numSteps);
    if (e.position.x >= hx - 2.0f && e.position.x <= hx + 20.0f && e.y < numbersArea().getY()) cursor = juce::MouseCursor::LeftRightResizeCursor;
    else if (e.y < numbersArea().getY() - 4)
    {
        const int s = stepAtX (e.position.x);
        if (s >= 0 && s < pat.numSteps && pat.on[(size_t) s])
        {
            int first, last;
            noteRange (s, first, last);
            if (std::abs (e.position.x - noteEndX (last)) <= 5.0f) cursor = juce::MouseCursor::LeftRightResizeCursor;
        }
    }
    setMouseCursor (cursor);
}

void StepEditor::mouseDown (const juce::MouseEvent& e)
{
    mode = Mode::none;
    const float x = e.position.x;
    const float hx = stepX (pat.numSteps);
    if (e.y >= getHeight() - 8) return;
    const bool inNumbers = e.y >= numbersArea().getY() - 4;
    if (! inNumbers && x >= hx - 2.0f && x <= hx + 20.0f) { mode = Mode::handle; repaint(); return; }

    const int s = stepAtX (x);
    if (s < 0 || s >= pat.numSteps) return;
    const float sw = stepWidth();

    if (inNumbers)
    {
        // click between two step numbers: create or remove a tie
        const float rel = x - stepX (s);
        int left = -1;
        if (rel <= 4.0f && s > 0) left = s - 1;
        else if (rel >= sw - 6.0f && s + 1 < pat.numSteps) left = s;
        if (left >= 0)
        {
            const auto li = (size_t) left;
            const bool tied = pat.tie[li] && pat.on[li] && pat.on[li + 1];
            if (tied) pat.tie[li] = false;
            else { pat.tie[li] = true; pat.on[li] = true; pat.on[li + 1] = true; pat.length[li] = 1.0f; }
            changed();
            return;
        }
        pat.on[(size_t) s] = ! pat.on[(size_t) s];
        mode = Mode::toggle;
        lastStep = s;
        changed();
        return;
    }

    int first = s, last = s;
    noteRange (s, first, last);
    const bool nearEdge = pat.on[(size_t) s] && std::abs (x - noteEndX (last)) <= 5.0f;
    // also catch the edge when the pointer is just inside the next step
    if (! nearEdge && s > 0 && pat.on[(size_t) (s - 1)] && x - stepX (s) <= 4.0f && ! e.mods.isAltDown())
    {
        int f2, l2;
        noteRange (s - 1, f2, l2);
        if (l2 == s - 1 && std::abs (x - noteEndX (l2)) <= 5.0f)
        {
            mode = Mode::length; dragStep = f2; dragChainEnd = l2;
            repaint();
            return;
        }
    }
    if (pat.on[(size_t) s] && (nearEdge || e.mods.isAltDown()))
    {
        mode = Mode::length; dragStep = first; dragChainEnd = last;
        repaint();
        return;
    }
    mode = Mode::level;
    lastStep = s;
    setLevelAt (s, e.position.y);
    changed();
}

void StepEditor::mouseDrag (const juce::MouseEvent& e)
{
    const float sw = stepWidth();
    switch (mode)
    {
        case Mode::handle:
        {
            if (e.position.x > (float) getWidth() - 12.0f) scrollX += 8.0f;
            else if (e.position.x < 12.0f) scrollX -= 8.0f;
            pat.numSteps = juce::jlimit (1, kMaxSteps, juce::roundToInt ((e.position.x - kStepPad + scrollX) / sw));
            updateScroll();
            changed();
            break;
        }
        case Mode::toggle:
        {
            const int s = stepAtX (e.position.x);
            if (s >= 0 && s < pat.numSteps && s != lastStep)
            {
                pat.on[(size_t) s] = ! pat.on[(size_t) s];
                lastStep = s;
                changed();
            }
            break;
        }
        case Mode::level:
        {
            const int s = juce::jlimit (0, pat.numSteps - 1, stepAtX (e.position.x));
            const int from = juce::jmin (s, lastStep), to = juce::jmax (s, lastStep);
            for (int i = from; i <= to; ++i) setLevelAt (i, e.position.y);
            lastStep = s;
            changed();
            break;
        }
        case Mode::length:
        {
            float total = (e.position.x - stepX (dragStep)) / sw; // in steps
            if (! e.mods.isShiftDown()) total = std::round (total * 4.0f) / 4.0f;
            total = juce::jlimit (e.mods.isShiftDown() ? 0.05f : 0.25f, (float) (pat.numSteps - dragStep), total);
            const int full = juce::jmax (1, (int) std::ceil (total - 0.0001f));
            const int last = dragStep + full - 1;
            for (int k = dragStep; k <= juce::jmax (last, dragChainEnd) && k < pat.numSteps; ++k)
            {
                const auto ki = (size_t) k;
                if (k < last) { pat.tie[ki] = true; pat.on[ki] = true; pat.on[ki + 1] = true; pat.length[ki] = 1.0f; }
                else if (k == last) { pat.tie[ki] = false; pat.length[ki] = juce::jlimit (0.05f, 1.0f, total - (float) (full - 1)); }
                else if (k < dragChainEnd) pat.tie[ki] = false;
            }
            dragChainEnd = juce::jmax (dragChainEnd, last);
            changed();
            break;
        }
        case Mode::none: break;
    }
}

void StepEditor::mouseUp (const juce::MouseEvent&)
{
    mode = Mode::none;
    repaint();
}

//==============================================================================
void EnvelopeDisplay::setAttack (PatternData& p, float a)
{
    p.attack = juce::jlimit (0.0f, 1.0f, a);
    float over = p.attack + p.hold + p.release - 1.0f;
    if (over > 0.0f) { const float d = juce::jmin (over, p.hold); p.hold -= d; over -= d; }
    if (over > 0.0f) p.release = juce::jmax (0.0f, p.release - over);
}

void EnvelopeDisplay::setHold (PatternData& p, float h)
{
    p.hold = juce::jlimit (0.0f, 1.0f, h);
    float over = p.attack + p.hold + p.release - 1.0f;
    if (over > 0.0f) { const float d = juce::jmin (over, p.release); p.release -= d; over -= d; }
    if (over > 0.0f) p.attack = juce::jmax (0.0f, p.attack - over);
}

void EnvelopeDisplay::setRelease (PatternData& p, float r)
{
    p.release = juce::jlimit (0.0f, 1.0f, r);
    float over = p.attack + p.hold + p.release - 1.0f;
    if (over > 0.0f) { const float d = juce::jmin (over, p.hold); p.hold -= d; over -= d; }
    if (over > 0.0f) p.attack = juce::jmax (0.0f, p.attack - over);
}

juce::Point<float> EnvelopeDisplay::dot (int index) const
{
    const auto r = plot();
    const float a = pat.attack, h = pat.hold, rl = pat.release;
    auto X = [&] (float t) { return r.getX() + t * r.getWidth(); };
    PlaybackPattern pp;
    pp.p = pat;
    auto Y = [&] (float t) { return r.getBottom() - pp.envelope (juce::jlimit (0.0f, 0.9999f, t)) * r.getHeight(); };
    switch (index)
    {
        case 0: return { X (a), r.getY() };                          // attack
        case 1: return { X (a + h), r.getY() };                      // hold
        case 2: return { X (a + h + rl), r.getBottom() };            // release
        case 3: return { X (a * 0.5f), a > 0.001f ? Y (a * 0.5f) : r.getCentreY() };                       // attack curve
        case 4: return { X (a + h + rl * 0.5f), rl > 0.001f ? Y (a + h + rl * 0.5f) : r.getCentreY() };    // release curve
        default: break;
    }
    return {};
}

void EnvelopeDisplay::paint (juce::Graphics& g)
{
    g.fillAll (col::lane);
    const auto r = plot();
    PlaybackPattern pp;
    pp.p = pat;
    juce::Path path;
    path.startNewSubPath (r.getX(), r.getBottom());
    const int n = juce::jmax (8, (int) r.getWidth());
    for (int i = 0; i <= n; ++i)
    {
        const float t = (float) i / (float) n;
        path.lineTo (r.getX() + t * r.getWidth(), r.getBottom() - pp.envelope (juce::jmin (t, 0.9999f)) * r.getHeight());
    }
    path.lineTo (r.getRight(), r.getBottom());
    path.lineTo (r.getRight() + 10.0f, r.getBottom());
    g.setColour (col::blueLine);
    g.strokePath (path, juce::PathStrokeType (1.6f));
    for (int i = 4; i >= 0; --i)
    {
        const auto p = dot (i);
        const bool curveDot = i >= 3;
        if (curveDot && ((i == 3 && pat.attack < 0.02f) || (i == 4 && pat.release < 0.02f))) continue;
        g.setColour (curveDot ? juce::Colour (0xff3d5f95) : col::blueLine);
        const float d = curveDot ? 7.0f : 8.5f;
        g.fillEllipse (p.x - d * 0.5f, p.y - d * 0.5f, d, d);
    }
    // captions of the three value fields (the fields themselves are sliders placed by the editor)
    g.setFont (fontOf (14.0f));
    g.setColour (col::dim.interpolatedWith (col::num, 0.4f));
    const float fw = (float) getWidth() / 3.0f;
    const char* names[3] = { "A:", "H:", "R:" };
    for (int i = 0; i < 3; ++i)
        g.drawText (names[i], juce::Rectangle<float> ((float) i * fw + 8.0f, (float) getHeight() - 25.0f, 22.0f, 20.0f), juce::Justification::centredLeft, false);
}

void EnvelopeDisplay::mouseDown (const juce::MouseEvent& e)
{
    drag = -1;
    float best = 14.0f;
    for (int i : { 3, 4, 0, 1, 2 })
    {
        if ((i == 3 && pat.attack < 0.02f) || (i == 4 && pat.release < 0.02f)) continue;
        const float d = dot (i).getDistanceFrom (e.position);
        if (d <= best) { best = d; drag = i; }
    }
    if (drag == 1 && pat.hold < 0.02f && e.position.x < dot (0).x) drag = 0;
    dragStartCurve = drag == 3 ? pat.attackCurve : pat.releaseCurve;
}

void EnvelopeDisplay::mouseDrag (const juce::MouseEvent& e)
{
    if (drag < 0) return;
    const auto r = plot();
    const float t = juce::jlimit (0.0f, 1.0f, (e.position.x - r.getX()) / r.getWidth());
    switch (drag)
    {
        case 0: setAttack (pat, t); break;
        case 1: setHold (pat, juce::jmax (0.0f, t - pat.attack)); break;
        case 2: setRelease (pat, juce::jmax (0.0f, t - pat.attack - pat.hold)); break;
        case 3: pat.attackCurve = juce::jlimit (-1.0f, 1.0f, dragStartCurve - (float) e.getDistanceFromDragStartY() / 40.0f); break;
        case 4: pat.releaseCurve = juce::jlimit (-1.0f, 1.0f, dragStartCurve - (float) e.getDistanceFromDragStartY() / 40.0f); break;
        default: break;
    }
    repaint();
    if (onChange) onChange();
}

//==============================================================================
juce::Rectangle<float> OrderStrip::slot (int index) const
{
    const auto ord = proc.getOrder();
    const auto f = fontOf (14.0f);
    float widths[kNumBlocks], total = 0.0f;
    for (int i = 0; i < kNumBlocks; ++i)
    {
        widths[i] = textWidth (f, blockName (ord[(size_t) i])) + 46.0f;
        total += widths[i] + 3.0f;
    }
    float x = ((float) getWidth() - total) * 0.5f;
    for (int i = 0; i < index; ++i) x += widths[i] + 3.0f;
    return { x, 7.0f, widths[index], (float) getHeight() - 14.0f };
}

int OrderStrip::slotAt (float x) const
{
    for (int i = 0; i < kNumBlocks; ++i)
        if (x < slot (i).getRight() + 1.5f) return i;
    return kNumBlocks - 1;
}

void OrderStrip::paint (juce::Graphics& g)
{
    g.fillAll (col::strip);
    const auto ord = proc.getOrder();
    g.setFont (fontOf (14.0f));
    for (int i = 0; i < kNumBlocks; ++i)
    {
        const auto r = slot (i);
        const int b = ord[(size_t) i];
        const bool on = active[(size_t) b];
        g.setColour (i == dragging ? col::stripBtn.brighter (0.25f) : (on ? col::stripBtn : col::stripBtn.withAlpha (0.45f)));
        g.fillRect (r);
        g.setColour (on ? col::text : col::dim.withAlpha (0.75f));
        g.drawText (blockName (b), r, juce::Justification::centred, false);
    }
}

void OrderStrip::mouseDown (const juce::MouseEvent& e)
{
    dragging = (e.position.x >= slot (0).getX() && e.position.x <= slot (kNumBlocks - 1).getRight()) ? slotAt (e.position.x) : -1;
    repaint();
}

void OrderStrip::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging < 0) return;
    const int to = slotAt (e.position.x);
    if (to == dragging) return;
    // only swap once the pointer is over the middle part of the neighbour, so slots of different width do not flicker
    const auto target = slot (to);
    if (std::abs (e.position.x - target.getCentreX()) > target.getWidth() * 0.35f && std::abs (to - dragging) == 1) return;
    auto ord = proc.getOrder();
    const int moved = ord[(size_t) dragging];
    if (to > dragging) for (int i = dragging; i < to; ++i) ord[(size_t) i] = ord[(size_t) (i + 1)];
    else               for (int i = dragging; i > to; --i) ord[(size_t) i] = ord[(size_t) (i - 1)];
    ord[(size_t) to] = moved;
    proc.setOrder (ord);
    dragging = to;
    repaint();
}

//==============================================================================
Knob* StepFxEditor::addKnob (const char* id, const char* caption, int target, int cx, int cy)
{
    auto* k = knobs.add (new Knob (proc.apvts, id, caption, target));
    addAndMakeVisible (k);
    k->setBounds (cx - 44, cy - 51, 88, 85);
    return k;
}

StepFxEditor::StepFxEditor (StepFxProcessor& p)
    : AudioProcessorEditor (&p), proc (p), xyPad (p.apvts),
      depthKnob (p.apvts, "m1Depth", "Depth", tMod1Depth), rateKnob (p.apvts, "m1Rate", "Rate"), swingKnob (p.apvts, "m1Swing", "Swing"),
      orderStrip (p)
{
    setLookAndFeel (&laf);
    auto& s = proc.apvts;

    auto attachButton = [&] (juce::ToggleButton& b, const char* id, juce::Rectangle<int> bounds)
    {
        addAndMakeVisible (b);
        b.setBounds (bounds);
        buttonAtts.add (new APVTS::ButtonAttachment (s, id, b));
    };
    auto attachCombo = [&] (juce::ComboBox& c, const juce::String& id, juce::Rectangle<int> bounds)
    {
        if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (s.getParameter (id)))
            c.addItemList (choice->choices, 1);
        addAndMakeVisible (c);
        c.setBounds (bounds);
        comboAtts.add (new APVTS::ComboBoxAttachment (s, id, c));
    };

    // ---- row 1 ----
    const int k1 = 113, k2 = 217; // knob centre lines of the upper panels
    groupMod = { addKnob ("modDelay", "Delay", tModDelay, 59, k1), addKnob ("modRate", "Rate", tModRate, 150, k1),
                 addKnob ("modDepth", "Depth", tModDepth, 238, k1), addKnob ("modFeedback", "Feedback", tModFeedback, 102, k2),
                 addKnob ("modMix", "Mix", tModMix, 193, k2) };
    attachButton (modOn, "modOn", { 18, 10, 24, 24 });

    addAndMakeVisible (xyPad);
    xyPad.setBounds (kColA + 21, 22, 218, 218);

    attachButton (xOn, "xOn", { kColB + 14, 10, 24, 24 });
    attachButton (yOn, "yOn", { kColB + 14, 142, 24, 24 });
    const char* xyIds[4] = { "xTarget1", "xTarget2", "yTarget1", "yTarget2" };
    const char* xyDepthIds[4] = { "xDepth1", "xDepth2", "yDepth1", "yDepth2" };
    const int xyRowY[4] = { 50, 82, 182, 214 };
    for (int i = 0; i < 4; ++i)
    {
        attachCombo (xyTarget[i], xyIds[i], { kColB + 16, xyRowY[i], 172, 26 });
        auto& d = xyDepth[i];
        d.setSliderStyle (juce::Slider::LinearBar);
        d.setTextBoxIsEditable (true);
        d.setMouseDragSensitivity (220);
        addAndMakeVisible (d);
        d.setBounds (kColB + 190, xyRowY[i], 58, 26);
        sliderAtts.add (new APVTS::SliderAttachment (s, xyDepthIds[i], d));
    }

    const int dx = kColC;
    groupDelay = { addKnob ("delayFeedback", "Feedback", tDelayFeedback, dx + 49, k1), addKnob ("delayCutoff", "Cutoff", tDelayCutoff, dx + 142, k1),
                   addKnob ("delayMix", "Mix", tDelayMix, dx + 228, k1) };
    kDelayMsL = addKnob ("delayTimeL", "Time L", -1, dx + 142, k2); kDelayMsR = addKnob ("delayTimeR", "Time R", -1, dx + 228, k2);
    kDelayDivL = addKnob ("delayDivL", "Time L", -1, dx + 142, k2); kDelayDivR = addKnob ("delayDivR", "Time R", -1, dx + 228, k2);
    for (auto* k : { kDelayMsL, kDelayMsR, kDelayDivL, kDelayDivR }) groupDelay.push_back (k);
    attachButton (delayOn, "delayOn", { dx + 14, 10, 24, 24 });
    delaySync.getProperties().set ("icon", "note");
    attachButton (delaySync, "delaySync", { dx + 30, k2 - 16, 40, 28 });
    attachCombo (delayFilter, "delayFilter", { dx + 170, 9, 104, 26 });

    // ---- row 2 ----
    const int k3 = kRow2Y + 109;
    kFilter[0] = addKnob ("filterCutoff", "Cutoff", tFilterCutoff, 59, k3);
    kFilter[1] = addKnob ("filterRes", "Res", tFilterRes, 150, k3);
    kFilter[2] = addKnob ("filterDrive", "Drive", tFilterDrive, 238, k3);
    kFilterMix = addKnob ("filterMix", "Mix", tFilterMix, 238, k3);
    groupFilter = { kFilter[0], kFilter[1], kFilter[2], kFilterMix };
    attachButton (filterOn, "filterOn", { 18, kRow2Y + 10, 24, 24 });
    attachCombo (filterType, "filterType", { 132, kRow2Y + 9, 160, 26 });

    groupDist = { addKnob ("exciter", "Exciter", tExciter, kColA + 69, k3), addKnob ("dirt", "Dirt", tDirt, kColA + 186, k3) };
    attachButton (distOn, "distOn", { kColA + 14, kRow2Y + 10, 24, 24 });

    groupReverb = { addKnob ("reverbTime", "Time", tReverbTime, kColB + 70, k3), addKnob ("reverbMix", "Mix", tReverbMix, kColB + 187, k3) };
    attachButton (reverbOn, "reverbOn", { kColB + 14, kRow2Y + 10, 24, 24 });
    attachCombo (reverbType, "reverbType", { kColB + 148, kRow2Y + 9, 108, 26 });

    addKnob ("masterInput", "Input", tMasterInput, dx + 52, k3);
    addKnob ("masterMix", "Mix", tMasterMix, dx + 140, k3);
    addKnob ("masterOutput", "Output", tMasterOutput, dx + 228, k3);

    // ---- row 3: step modulator ----
    for (int m = 0; m < kNumMods; ++m)
    {
        const juce::String pre = "m" + juce::String (m + 1);
        const int y = kRow3Y + m * kTargetRowH;
        addAndMakeVisible (modOnBtn[m]);
        modOnBtn[m].setBounds (14, y + 7, 24, 24);
        buttonAtts.add (new APVTS::ButtonAttachment (s, pre + "On", modOnBtn[m]));
        modTarget[m].addItemList (modTargetNames(), 1);
        modTarget[m].setColour (juce::ComboBox::textColourId, col::text);
        modTarget[m].getProperties().set ("chevRight", true);
        addAndMakeVisible (modTarget[m]);
        modTarget[m].setBounds (44, y + 5, kTargetW - 50, 27);
        comboAtts.add (new APVTS::ComboBoxAttachment (s, pre + "Target", modTarget[m]));
        modTarget[m].addMouseListener (this, true);
    }

    presetBtn.getProperties().set ("style", "flat");
    prevBtn.getProperties().set ("style", "prev");
    nextBtn.getProperties().set ("style", "next");
    actionBtn.getProperties().set ("style", "gear");
    for (auto* b : { &prevBtn, &nextBtn, &presetBtn, &actionBtn, &saveBtn, &cancelBtn }) addAndMakeVisible (b);
    saveBtn.setVisible (false); cancelBtn.setVisible (false);
    addChildComponent (nameEditor);
    nameEditor.setFont (fontOf (14.0f));
    nameEditor.onReturnKey = [this] { endSaveAs (true); };
    nameEditor.onEscapeKey = [this] { endSaveAs (false); };
    saveBtn.onClick = [this] { endSaveAs (true); };
    cancelBtn.onClick = [this] { endSaveAs (false); };
    prevBtn.onClick = [this] { stepPreset (-1); };
    nextBtn.onClick = [this] { stepPreset (1); };
    presetBtn.onClick = [this] { showPresetMenu(); };
    actionBtn.onClick = [this] { showActionMenu(); };
    presetBtn.setBounds (244, kRow3Y + 40, 190, 26);
    prevBtn.setBounds (288, kRow3Y + 75, 27, 25);
    nextBtn.setBounds (316, kRow3Y + 75, 27, 25);
    actionBtn.setBounds (349, kRow3Y + 75, 40, 25);
    nameEditor.setBounds (240, kRow3Y + 41, 150, 24);
    saveBtn.setBounds (394, kRow3Y + 41, 50, 24);
    cancelBtn.setBounds (447, kRow3Y + 41, 60, 24);

    addAndMakeVisible (depthKnob); addAndMakeVisible (rateKnob); addAndMakeVisible (swingKnob);
    depthKnob.setBounds (565 - 44, kRow3Y + 70 - 51, 88, 85);
    rateKnob.setBounds (653 - 44, kRow3Y + 70 - 51, 88, 85);
    swingKnob.setBounds (742 - 44, kRow3Y + 70 - 51, 88, 85);

    addAndMakeVisible (envDisplay);
    envDisplay.setBounds (821, kRow3Y + 8, 271, 97);
    envDisplay.onChange = [this]
    {
        auto pat = stepEditor.getPattern();
        const auto& e = envDisplay.getPattern();
        pat.attack = e.attack; pat.hold = e.hold; pat.release = e.release;
        pat.attackCurve = e.attackCurve; pat.releaseCurve = e.releaseCurve;
        pushPattern (pat, "Custom");
    };
    juce::Slider* envSliders[3] = { &envA, &envH, &envR };
    for (int i = 0; i < 3; ++i)
    {
        auto* sl = envSliders[i];
        sl->getProperties().set ("blue", true);
        sl->setSliderStyle (juce::Slider::LinearBar);
        sl->setRange (0.0, 100.0, 0.1);
        sl->setNumDecimalPlacesToDisplay (1);
        sl->setTextValueSuffix (" %");
        sl->setMouseDragSensitivity (220);
        sl->onValueChange = [this, i, sl]
        {
            auto pat = stepEditor.getPattern();
            const float v = (float) sl->getValue() * 0.01f;
            if (i == 0) EnvelopeDisplay::setAttack (pat, v);
            else if (i == 1) EnvelopeDisplay::setHold (pat, v);
            else EnvelopeDisplay::setRelease (pat, v);
            pushPattern (pat, "Custom");
        };
        addAndMakeVisible (sl);
        sl->setBounds (821 + i * 90 + 30, kRow3Y + 8 + 97 - 25, 58, 20);
    }

    addAndMakeVisible (stepEditor);
    stepEditor.setBounds (0, kStepY, kW, kStepH);
    stepEditor.onChange = [this] { pushPattern (stepEditor.getPattern(), "Custom"); };
    addAndMakeVisible (orderStrip);
    orderStrip.setBounds (0, kStripY, kW, kH - kStripY);
    addAndMakeVisible (globalPresetBtn);
    globalPresetBtn.getProperties().set ("style", "flat");
    globalPresetBtn.setTooltip ("Presets (complete settings)");
    globalPresetBtn.setButtonText (proc.getPresetName());
    globalPresetBtn.onClick = [this] { showGlobalPresetMenu(); };
    globalPresetBtn.setBounds (6, kStripY + 6, 190, kH - kStripY - 12);

    sendLookAndFeelChange(); // children were created before the look and feel was set
    setSize (kW, kH);
    seenStateVersion = proc.stateVersion.load();
    selectMod (0);
    updateDynamicLayout();
    updateRings();
    startTimerHz (30);
}

StepFxEditor::~StepFxEditor()
{
    stopTimer();
    for (auto& c : modTarget) c.removeMouseListener (this);
    // attachments must go before the widgets they are attached to
    sliderAtts.clear(); comboAtts.clear(); buttonAtts.clear();
    setLookAndFeel (nullptr);
}

void StepFxEditor::resized() {}

//==============================================================================
void StepFxEditor::paint (juce::Graphics& g)
{
    // processor panels
    const juce::Rectangle<int> modFx (0, 0, kColA, kRow1H), xy (kColA, 0, kColB - kColA, kRow1H), xyAssign (kColB, 0, kColC - kColB, kRow1H),
        delay (kColC, 0, kW - kColC, kRow1H), filter (0, kRow2Y, kColA, kRow2H), dist (kColA, kRow2Y, kColB - kColA, kRow2H),
        reverb (kColB, kRow2Y, kColC - kColB, kRow2H), master (kColC, kRow2Y, kW - kColC, kRow2H);
    for (auto& r : { modFx, delay, filter, dist, reverb, master }) fillZone (g, r, col::panelA, col::panelB);
    fillZone (g, xyAssign, col::panelDark, col::panelDark.darker (0.12f));
    g.setColour (col::lane);
    g.fillRect (xy);

    g.setColour (col::sep);
    g.fillRect (0, kRow2Y - 1, kW, 2);
    for (int x : { kColA, kColB, kColC }) g.fillRect (x - 1, kRow2Y, 2, kRow2H);
    g.fillRect (kColC - 1, 0, 2, kRow1H);
    g.fillRect (kColB, 131, kColC - kColB, 1);

    auto title = [&] (const char* text, int x, int y, bool on)
    {
        g.setFont (fontOf (20.0f));
        g.setColour (on ? col::text : col::dim);
        g.drawText (text, x, y, 220, 26, juce::Justification::centredLeft, false);
    };
    title ("MOD FX", 50, 9, modOn.getToggleState());
    title ("X", kColB + 46, 9, xOn.getToggleState());
    title ("Y", kColB + 46, 141, yOn.getToggleState());
    title ("DELAY", kColC + 46, 9, delayOn.getToggleState());
    title ("FILTER", 50, kRow2Y + 9, filterOn.getToggleState());
    title ("DISTORTION", kColA + 46, kRow2Y + 9, distOn.getToggleState());
    title ("REVERB", kColB + 46, kRow2Y + 9, reverbOn.getToggleState());
    title ("MASTER", kColC + 22, kRow2Y + 9, true);
    g.setFont (fontOf (15.0f));
    g.setColour (col::text.withAlpha (0.85f));
    g.drawText ("Depth", kColB + 180, 11, 68, 22, juce::Justification::centredRight, false);
    g.drawText ("Depth", kColB + 180, 143, 68, 22, juce::Justification::centredRight, false);
    g.drawText ("Filter", kColC + 122, 11, 46, 22, juce::Justification::centredRight, false);

    // step modulator header row
    const juce::Rectangle<int> row3 (0, kRow3Y, kW, kRow3H);
    fillZone (g, row3, col::rowA, col::rowB);
    g.setColour (col::sep);
    g.fillRect (0, kRow3Y - 1, kW, 2);
    for (int m = 0; m < kNumMods; ++m)
    {
        const juce::Rectangle<int> r (0, kRow3Y + m * kTargetRowH, kTargetW, kTargetRowH);
        g.setColour (m == sel ? col::rowSel : col::rowA.darker (0.08f));
        g.fillRect (r);
        g.setColour (col::sep);
        g.fillRect (r.withTop (r.getBottom() - 1));
        g.fillRect (kTargetW - 32, kRow3Y + m * kTargetRowH, 1, kTargetRowH);
    }
    g.fillRect (kTargetW, kRow3Y, 1, kRow3H);
    title ("STEP MODULATOR", 244, kRow3Y + 10, true);
}

void StepFxEditor::mouseDown (const juce::MouseEvent& e)
{
    for (int m = 0; m < kNumMods; ++m)
        if (e.eventComponent == &modTarget[m] || modTarget[m].isParentOf (e.eventComponent))
        {
            selectMod (m);
            return;
        }
    if (e.eventComponent == this && e.x < kTargetW && e.y >= kRow3Y && e.y < kRow3Y + kNumMods * kTargetRowH)
        selectMod ((e.y - kRow3Y) / kTargetRowH);
}

//==============================================================================
void StepFxEditor::selectMod (int m)
{
    sel = juce::jlimit (0, kNumMods - 1, m);
    const juce::String pre = "m" + juce::String (sel + 1);
    depthKnob.attach (pre + "Depth");
    rateKnob.attach (pre + "Rate");
    swingKnob.attach (pre + "Swing");
    depthKnob.target = tMod1Depth + sel;
    refreshFromProcessor();
    repaint();
}

void StepFxEditor::refreshFromProcessor()
{
    const auto pat = proc.getPattern (sel);
    stepEditor.setPattern (pat);
    envDisplay.setPattern (pat);
    presetBtn.setButtonText (proc.getPatternName (sel));
    globalPresetBtn.setButtonText (proc.getPresetName());
    updateEnvFields();
}

void StepFxEditor::showGlobalPresetMenu()
{
    enum { idSave = 1, idDelete, idFactory = 100, idUser = 1000 };
    juce::StringArray users;
    for (auto& f : StepFxProcessor::userPresetDir().findChildFiles (juce::File::findFiles, false, "*.stepflux"))
        users.add (f.getFileNameWithoutExtension());
    users.sort (true);
    const auto current = proc.getPresetName();
    const auto fnames = StepFxProcessor::factoryPresetNames();
    juce::PopupMenu m, factory;
    m.addItem (idSave, "Save Preset As...");
    m.addItem (idDelete, "Delete User Preset", users.contains (current));
    m.addSeparator();
    for (int i = 0; i < fnames.size(); ++i) factory.addItem (idFactory + i, fnames[i], true, fnames[i] == current);
    m.addSubMenu ("Factory", factory);
    if (! users.isEmpty())
    {
        m.addSectionHeader ("User");
        for (int i = 0; i < users.size(); ++i) m.addItem (idUser + i, users[i], true, users[i] == current);
    }
    juce::Component::SafePointer<StepFxEditor> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (globalPresetBtn).withParentComponent (this), [safe, users, current] (int r)
    {
        if (safe == nullptr || r == 0) return;
        auto& e = *safe;
        const auto dir = StepFxProcessor::userPresetDir();
        if (r == idSave) e.saveGlobalPreset();
        else if (r == idDelete) dir.getChildFile (current + ".stepflux").deleteFile();
        else if (r >= idUser) e.proc.loadPresetFile (dir.getChildFile (users[r - idUser] + ".stepflux"));
        else if (r >= idFactory) e.proc.loadFactoryPreset (r - idFactory);
    });
}

void StepFxEditor::saveGlobalPreset()
{
    auto* w = new juce::AlertWindow ("Save Preset", "Name:", juce::MessageBoxIconType::NoIcon, this);
    const auto current = proc.getPresetName();
    w->addTextEditor ("name", StepFxProcessor::factoryPresetNames().contains (current) || current == "Custom" ? "My Preset" : current);
    w->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    juce::Component::SafePointer<StepFxEditor> safe (this);
    w->enterModalState (true, juce::ModalCallbackFunction::create ([safe, w] (int result)
    {
        const auto name = juce::File::createLegalFileName (w->getTextEditorContents ("name").trim());
        if (safe == nullptr || result != 1 || name.isEmpty()) return;
        if (safe->proc.savePresetFile (StepFxProcessor::userPresetDir().getChildFile (name + ".stepflux"), name))
            safe->globalPresetBtn.setButtonText (name);
    }), true);
}

void StepFxEditor::updateEnvFields()
{
    const auto& pat = stepEditor.getPattern();
    envA.setValue (pat.attack * 100.0, juce::dontSendNotification);
    envH.setValue (pat.hold * 100.0, juce::dontSendNotification);
    envR.setValue (pat.release * 100.0, juce::dontSendNotification);
}

void StepFxEditor::pushPattern (const PatternData& p, const juce::String& name)
{
    proc.setPattern (sel, p, name);
    const auto stored = proc.getPattern (sel);
    stepEditor.setPattern (stored);
    envDisplay.setPattern (stored);
    presetBtn.setButtonText (name);
    updateEnvFields();
}

//==============================================================================
juce::File StepFxEditor::userPatternDir()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("StepFlux").getChildFile ("Patterns");
}

juce::StringArray StepFxEditor::userPatternNames() const
{
    juce::StringArray names;
    for (auto& f : userPatternDir().findChildFiles (juce::File::findFiles, false, "*.xml"))
        names.add (f.getFileNameWithoutExtension());
    names.sort (true);
    return names;
}

void StepFxEditor::loadPresetByName (const juce::String& name)
{
    const int fi = FactoryPatterns::names().indexOf (name);
    if (fi >= 0) { pushPattern (FactoryPatterns::get (fi), name); return; }
    const auto file = userPatternDir().getChildFile (name + ".xml");
    if (auto xml = juce::parseXML (file))
        pushPattern (PatternData::fromValueTree (juce::ValueTree::fromXml (*xml)), name);
}

void StepFxEditor::stepPreset (int delta)
{
    auto all = FactoryPatterns::names();
    all.addArray (userPatternNames());
    if (all.isEmpty()) return;
    int idx = all.indexOf (proc.getPatternName (sel));
    idx = idx < 0 ? (delta > 0 ? 0 : all.size() - 1) : (idx + delta + all.size()) % all.size();
    loadPresetByName (all[idx]);
}

void StepFxEditor::showPresetMenu()
{
    enum { idSaveAs = 1, idDefault, idCopy, idPaste, idDelete, idFactory = 100, idUser = 1000 };
    const auto users = userPatternNames();
    const auto current = proc.getPatternName (sel);
    juce::PopupMenu m, factory;
    m.addItem (idSaveAs, "Save As...");
    m.addItem (idDefault, "Recall Default");
    m.addSeparator();
    m.addItem (idCopy, "Copy");
    m.addItem (idPaste, "Paste", gClipboardValid);
    m.addSeparator();
    m.addItem (idDelete, "Delete User Preset", users.contains (current));
    m.addSeparator();
    const auto fnames = FactoryPatterns::names();
    for (int i = 0; i < fnames.size(); ++i) factory.addItem (idFactory + i, fnames[i], true, fnames[i] == current);
    m.addSubMenu ("Factory", factory);
    if (! users.isEmpty())
    {
        m.addSectionHeader ("User");
        for (int i = 0; i < users.size(); ++i) m.addItem (idUser + i, users[i], true, users[i] == current);
    }
    juce::Component::SafePointer<StepFxEditor> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (presetBtn).withParentComponent (this), [safe, users, fnames, current] (int r)
    {
        if (safe == nullptr || r == 0) return;
        auto& e = *safe;
        if (r == idSaveAs) e.beginSaveAs();
        else if (r == idDefault) e.pushPattern (PatternData(), "Default");
        else if (r == idCopy) { gClipboard = e.stepEditor.getPattern(); gClipboardValid = true; }
        else if (r == idPaste) { if (gClipboardValid) e.pushPattern (gClipboard, "Custom"); }
        else if (r == idDelete) { userPatternDir().getChildFile (current + ".xml").deleteFile(); e.pushPattern (e.stepEditor.getPattern(), "Custom"); }
        else if (r >= idUser) e.loadPresetByName (users[r - idUser]);
        else if (r >= idFactory) e.loadPresetByName (fnames[r - idFactory]);
    });
}

void StepFxEditor::beginSaveAs()
{
    presetBtn.setVisible (false);
    nameEditor.setVisible (true); saveBtn.setVisible (true); cancelBtn.setVisible (true);
    const auto current = proc.getPatternName (sel);
    nameEditor.setText (current == "Custom" || FactoryPatterns::names().contains (current) ? "My Pattern" : current, false);
    nameEditor.selectAll();
    nameEditor.grabKeyboardFocus();
}

void StepFxEditor::endSaveAs (bool save)
{
    const auto name = juce::File::createLegalFileName (nameEditor.getText().trim());
    nameEditor.setVisible (false); saveBtn.setVisible (false); cancelBtn.setVisible (false);
    presetBtn.setVisible (true);
    if (! save || name.isEmpty() || FactoryPatterns::names().contains (name)) return;
    const auto dir = userPatternDir();
    if (! dir.createDirectory().wasOk()) return;
    const auto pat = stepEditor.getPattern();
    if (auto xml = pat.toValueTree ("Pattern").createXml())
        if (xml->writeTo (dir.getChildFile (name + ".xml")))
            pushPattern (pat, name);
}

void StepFxEditor::showActionMenu()
{
    juce::PopupMenu m;
    m.addItem (aRandomFull, "Random Full");
    m.addItem (aRandomDev, "Random Deviation");
    m.addSeparator();
    m.addItem (aShiftLeft, "Shift Left");
    m.addItem (aShiftRight, "Shift Right");
    m.addSeparator();
    m.addItem (aDouble, "Double Note Length");
    m.addItem (aReverse, "Reverse");
    m.addItem (aInvert, "Invert");
    m.addSeparator();
    m.addItem (aAppendDup, "Append Duplicate");
    m.addItem (aAppendRev, "Append Reverse Duplicate");
    juce::Component::SafePointer<StepFxEditor> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (actionBtn).withParentComponent (this), [safe] (int r)
    {
        if (safe != nullptr && r != 0) safe->runAction (r);
    });
}

void StepFxEditor::runAction (int id)
{
    auto pat = stepEditor.getPattern();
    switch (id)
    {
        case aRandomFull: pat.randomFull (rng); break;
        case aRandomDev:  pat.randomDeviation (rng); break;
        case aShiftLeft:  pat.shift (-1); break;
        case aShiftRight: pat.shift (1); break;
        case aDouble:     pat.doubleNoteLength(); break;
        case aReverse:    pat.reverse(); break;
        case aInvert:     pat.invert(); break;
        case aAppendDup:  pat.appendDuplicate (false); break;
        case aAppendRev:  pat.appendDuplicate (true); break;
        default: return;
    }
    pushPattern (pat, "Custom");
}

//==============================================================================
void StepFxEditor::updateDynamicLayout()
{
    const int type = (int) proc.apvts.getRawParameterValue ("filterType")->load();
    if (type != lastFilterType)
    {
        lastFilterType = type;
        const auto names = FilterTypes::knobNames (type);
        const bool mixKnob = juce::String (names[2]) == "Mix";
        kFilter[0]->setCaption (names[0]);
        kFilter[1]->setCaption (names[1]);
        kFilter[2]->setCaption (mixKnob ? "" : names[2]);
        kFilterMix->setCaption (mixKnob ? "Mix" : "");
    }
    const int sync = proc.apvts.getRawParameterValue ("delaySync")->load() > 0.5f ? 1 : 0;
    if (sync != lastSync)
    {
        lastSync = sync;
        kDelayMsL->setVisible (sync == 0); kDelayMsR->setVisible (sync == 0);
        kDelayDivL->setVisible (sync == 1); kDelayDivR->setVisible (sync == 1);
    }

    // sections that are switched off are shown dimmed
    auto dim = [] (std::vector<Knob*>& group, juce::ComboBox* box, bool on)
    {
        const float a = on ? 1.0f : 0.45f;
        for (auto* k : group) if (std::abs (k->getAlpha() - a) > 0.01f) k->setAlpha (a);
        if (box != nullptr && std::abs (box->getAlpha() - a) > 0.01f) box->setAlpha (a);
    };
    const bool states[5] = { modOn.getToggleState(), delayOn.getToggleState(), filterOn.getToggleState(), distOn.getToggleState(), reverbOn.getToggleState() };
    dim (groupMod, nullptr, states[0]);
    dim (groupDelay, &delayFilter, states[1]);
    dim (groupFilter, &filterType, states[2]);
    dim (groupDist, nullptr, states[3]);
    dim (groupReverb, &reverbType, states[4]);
    int mask = 0;
    for (int i = 0; i < 5; ++i) mask |= states[i] ? (1 << i) : 0;
    mask |= (xOn.getToggleState() ? 32 : 0) | (yOn.getToggleState() ? 64 : 0);
    if (mask != lastOnMask) { lastOnMask = mask; repaint(); }
}

void StepFxEditor::updateRings()
{
    auto& s = proc.apvts;
    float lo[kNumTargets] = {}, hi[kNumTargets] = {};
    bool any[kNumTargets] = {};
    bool gateActive = false, panActive = false;
    auto add = [&] (int t, float d)
    {
        if (t <= tNone || t >= kNumTargets || std::abs (d) < 0.0005f) return;
        any[t] = true;
        if (d > 0.0f) hi[t] += d; else lo[t] += d;
    };
    for (int m = 0; m < kNumMods; ++m)
    {
        const juce::String pre = "m" + juce::String (m + 1);
        if (s.getRawParameterValue (pre + "On")->load() < 0.5f) continue;
        const int t = (int) s.getRawParameterValue (pre + "Target")->load();
        if (t == tGateMix || t == tGateAdd) gateActive = true;
        if (t == tPan) panActive = true;
        add (t, s.getRawParameterValue (pre + "Depth")->load() * 0.01f);
    }
    const char* ids[4] = { "xTarget1", "xTarget2", "yTarget1", "yTarget2" };
    const char* depthIds[4] = { "xDepth1", "xDepth2", "yDepth1", "yDepth2" };
    for (int k = 0; k < 4; ++k)
    {
        if (s.getRawParameterValue (k < 2 ? "xOn" : "yOn")->load() < 0.5f) continue;
        const int t = xyChoiceToTarget ((int) s.getRawParameterValue (ids[k])->load());
        if (t == tPan) panActive = true;
        add (t, s.getRawParameterValue (depthIds[k])->load() * 0.01f);
    }

    auto apply = [&] (Knob& k)
    {
        auto& props = k.slider.getProperties();
        const int t = k.target;
        const bool on = t > 0 && any[t];
        const float base = (float) k.slider.valueToProportionOfLength (k.slider.getValue());
        const float nlo = juce::jlimit (0.0f, 1.0f, base + (on ? lo[t] : 0.0f)), nhi = juce::jlimit (0.0f, 1.0f, base + (on ? hi[t] : 0.0f));
        const float cur = on ? juce::jlimit (0.0f, 1.0f, proc.effectiveNorm[(size_t) t].load()) : base;
        if ((bool) props.getWithDefault ("mon", false) != on || (on && (std::abs ((float) props["mcur"] - cur) > 0.002f
            || std::abs ((float) props["mlo"] - nlo) > 0.002f || std::abs ((float) props["mhi"] - nhi) > 0.002f)))
        {
            props.set ("mon", on); props.set ("mlo", nlo); props.set ("mhi", nhi); props.set ("mcur", cur);
            k.slider.repaint();
        }
    };
    for (auto* k : knobs) apply (*k);
    apply (depthKnob);

    std::array<bool, kNumBlocks> act {};
    act[bGate] = gateActive; act[bPan] = panActive;
    act[bModFx] = modOn.getToggleState(); act[bDelay] = delayOn.getToggleState(); act[bFilter] = filterOn.getToggleState();
    act[bExciter] = act[bDirt] = distOn.getToggleState(); act[bReverb] = reverbOn.getToggleState();
    if (act != orderStrip.active) { orderStrip.active = act; orderStrip.repaint(); }
}

void StepFxEditor::timerCallback()
{
    const int v = proc.stateVersion.load();
    if (v != seenStateVersion) { seenStateVersion = v; refreshFromProcessor(); orderStrip.repaint(); }
    updateDynamicLayout();
    updateRings();
    stepEditor.setPlayStep (proc.currentStep[(size_t) sel].load());
    xyPad.repaint();
}
