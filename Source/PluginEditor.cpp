#include "PluginEditor.h"

using namespace sfx;
using namespace ui;

namespace
{
constexpr int kW = 1040, kH = 664;
constexpr float kStart = juce::MathConstants<float>::pi * 1.25f, kEnd = juce::MathConstants<float>::pi * 2.75f;
PatternData gClipboard;
bool gClipboardValid = false;

juce::Font fontOf (float size, bool bold = false)
{
    return juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain));
}
}

//==============================================================================
Laf::Laf()
{
    setColour (juce::ResizableWindow::backgroundColourId, col::bg);
    setColour (juce::Label::textColourId, col::text);
    setColour (juce::Slider::textBoxTextColourId, col::text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxHighlightColourId, col::accentDim);
    setColour (juce::ComboBox::backgroundColourId, col::lane);
    setColour (juce::ComboBox::outlineColourId, col::border);
    setColour (juce::ComboBox::textColourId, col::text);
    setColour (juce::ComboBox::arrowColourId, col::dim);
    setColour (juce::PopupMenu::backgroundColourId, col::panel);
    setColour (juce::PopupMenu::textColourId, col::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, col::accentDim);
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
    setColour (juce::TextButton::buttonColourId, col::lane);
    setColour (juce::TextButton::buttonOnColourId, col::accentDim);
    setColour (juce::TextButton::textColourOffId, col::text);
    setColour (juce::TextButton::textColourOnId, juce::Colours::white);
    setColour (juce::TextEditor::backgroundColourId, col::lane);
    setColour (juce::TextEditor::textColourId, col::text);
    setColour (juce::TextEditor::outlineColourId, col::border);
    setColour (juce::TextEditor::focusedOutlineColourId, col::accent);
    setColour (juce::TextEditor::highlightColourId, col::accentDim);
    setColour (juce::CaretComponent::caretColourId, col::accent);
    setColour (juce::ScrollBar::thumbColourId, col::border.brighter (0.3f));
    setColour (juce::ToggleButton::textColourId, col::text);
}

void Laf::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider& s)
{
    const auto b = juce::Rectangle<int> (x, y, w, h).toFloat();
    const float radius = juce::jmin (b.getWidth(), b.getHeight()) * 0.5f - 6.0f;
    const auto c = b.getCentre();
    const bool on = s.isEnabled();
    auto arc = [&] (float from, float to, float r, float thickness, juce::Colour colour)
    {
        juce::Path p;
        p.addCentredArc (c.x, c.y, r, r, 0.0f, from, to, true);
        g.setColour (colour);
        g.strokePath (p, juce::PathStrokeType (thickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    };
    const float angle = a0 + pos * (a1 - a0);
    arc (a0, a1, radius, 3.0f, col::lane.brighter (0.25f));
    if (on) arc (a0, angle, radius, 3.0f, col::accent);

    g.setColour (on ? col::panelHi.brighter (0.25f) : col::panelHi);
    g.fillEllipse (c.x - radius + 4.0f, c.y - radius + 4.0f, (radius - 4.0f) * 2.0f, (radius - 4.0f) * 2.0f);
    juce::Path ptr;
    ptr.addRoundedRectangle (-1.25f, -(radius - 5.0f), 2.5f, radius * 0.55f, 1.0f);
    g.setColour (on ? col::text : col::dim);
    g.fillPath (ptr, juce::AffineTransform::rotation (angle).translated (c.x, c.y));

    if ((bool) s.getProperties().getWithDefault ("mon", false))
    {
        const float lo = (float) s.getProperties()["mlo"], hi = (float) s.getProperties()["mhi"], cur = (float) s.getProperties()["mcur"];
        arc (a0 + lo * (a1 - a0), a0 + hi * (a1 - a0), radius + 4.0f, 2.0f, col::ring.withAlpha (0.75f));
        const float ca = a0 + cur * (a1 - a0) - juce::MathConstants<float>::halfPi;
        g.setColour (col::ring);
        g.fillEllipse (c.x + std::cos (ca) * (radius + 4.0f) - 2.5f, c.y + std::sin (ca) * (radius + 4.0f) - 2.5f, 5.0f, 5.0f);
    }
}

void Laf::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool)
{
    const auto r = b.getLocalBounds().toFloat();
    const bool on = b.getToggleState();
    const float d = juce::jmin (12.0f, r.getHeight() - 4.0f);
    juce::Rectangle<float> led (r.getX() + 2.0f, r.getCentreY() - d * 0.5f, d, d);
    g.setColour (on ? col::accent : col::lane);
    g.fillEllipse (led);
    g.setColour (highlighted ? col::text : col::border.brighter (0.2f));
    g.drawEllipse (led, 1.0f);
    if (b.getButtonText().isNotEmpty())
    {
        g.setColour (on ? col::text : col::dim);
        g.setFont (fontOf (12.0f, true));
        g.drawText (b.getButtonText(), r.withTrimmedLeft (d + 7.0f), juce::Justification::centredLeft, false);
    }
}

void Laf::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour& base, bool highlighted, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    auto c = b.getToggleState() ? col::accentDim : base;
    if (down) c = c.brighter (0.2f); else if (highlighted) c = c.brighter (0.1f);
    g.setColour (c);
    g.fillRoundedRectangle (r, 3.0f);
    g.setColour (b.getToggleState() ? col::accent : col::border);
    g.drawRoundedRectangle (r, 3.0f, 1.0f);
}

void Laf::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    auto r = juce::Rectangle<int> (0, 0, w, h).toFloat().reduced (0.5f);
    const bool selected = (bool) box.getProperties().getWithDefault ("selected", false);
    g.setColour (selected ? col::accentDim.withAlpha (0.55f) : col::lane);
    g.fillRoundedRectangle (r, 3.0f);
    g.setColour (selected ? col::accent : col::border);
    g.drawRoundedRectangle (r, 3.0f, 1.0f);
    juce::Path p;
    const float cx = (float) w - 9.0f, cy = (float) h * 0.5f;
    p.addTriangle (cx - 3.5f, cy - 2.0f, cx + 3.5f, cy - 2.0f, cx, cy + 2.5f);
    g.setColour (col::dim);
    g.fillPath (p);
}

juce::Label* Laf::createSliderTextBox (juce::Slider& s)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (s);
    l->setFont (fontOf (s.getSliderStyle() == juce::Slider::LinearBar ? 11.5f : 11.0f));
    l->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
    l->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    l->setColour (juce::Label::textColourId, col::text);
    l->setColour (juce::Label::outlineWhenEditingColourId, col::accent);
    l->setColour (juce::Label::backgroundWhenEditingColourId, col::lane);
    l->setColour (juce::TextEditor::textColourId, col::text);
    l->setColour (juce::TextEditor::backgroundColourId, col::lane);
    return l;
}

void Laf::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (4, 1, box.getWidth() - 18, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
    label.setMinimumHorizontalScale (0.7f);
}

void Laf::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float, juce::Slider::SliderStyle style, juce::Slider& s)
{
    if (style != juce::Slider::LinearBar) { LookAndFeel_V4::drawLinearSlider (g, x, y, w, h, pos, 0, 0, style, s); return; }
    auto r = juce::Rectangle<int> (x, y, w, h).toFloat();
    g.setColour (col::lane);
    g.fillRoundedRectangle (r, 3.0f);
    const double range = s.getMaximum() - s.getMinimum();
    const float zero = s.getMinimum() < 0.0 ? r.getX() + r.getWidth() * (float) (-s.getMinimum() / range) : r.getX();
    g.setColour (col::accentDim);
    g.fillRect (juce::Rectangle<float>::leftTopRightBottom (juce::jmin (zero, pos), r.getY() + 1.0f, juce::jmax (zero, pos), r.getBottom() - 1.0f));
    g.setColour (col::border);
    g.drawRoundedRectangle (r.reduced (0.5f), 3.0f, 1.0f);
}

//==============================================================================
Knob::Knob (juce::AudioProcessorValueTreeState& s, const juce::String& paramId, const juce::String& captionText, int targetIndex)
    : target (targetIndex), state (s)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setRotaryParameters (kStart, kEnd, true);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 14);
    addAndMakeVisible (slider);
    caption.setText (captionText, juce::dontSendNotification);
    caption.setJustificationType (juce::Justification::centred);
    caption.setFont (fontOf (11.0f));
    caption.setColour (juce::Label::textColourId, col::dim);
    caption.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (caption);
    if (paramId.isNotEmpty()) attach (paramId);
}

void Knob::attach (const juce::String& paramId)
{
    att.reset();
    att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, paramId, slider);
    if (auto* p = state.getParameter (paramId))
        slider.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
}

void Knob::resized()
{
    auto r = getLocalBounds();
    caption.setBounds (r.removeFromTop (13));
    slider.setBounds (r);
}

void Knob::setCaption (const juce::String& t)
{
    caption.setText (t, juce::dontSendNotification);
    slider.setEnabled (t.isNotEmpty());
    slider.setAlpha (t.isNotEmpty() ? 1.0f : 0.35f);
}

//==============================================================================
XYPad::XYPad (juce::AudioProcessorValueTreeState& s) : px (s.getParameter ("xyX")), py (s.getParameter ("xyY")) {}

void XYPad::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (col::lane);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (col::border.withAlpha (0.6f));
    for (int i = 1; i < 4; ++i)
    {
        g.drawVerticalLine (juce::roundToInt (r.getX() + r.getWidth() * (float) i / 4.0f), r.getY(), r.getBottom());
        g.drawHorizontalLine (juce::roundToInt (r.getY() + r.getHeight() * (float) i / 4.0f), r.getX(), r.getRight());
    }
    g.setColour (col::border);
    g.drawRoundedRectangle (r, 4.0f, 1.0f);
    auto inner = r.reduced (8.0f);
    const float x = inner.getX() + px->getValue() * inner.getWidth();
    const float y = inner.getBottom() - py->getValue() * inner.getHeight();
    g.setColour (col::accent.withAlpha (0.25f));
    g.drawVerticalLine (juce::roundToInt (x), r.getY(), r.getBottom());
    g.drawHorizontalLine (juce::roundToInt (y), r.getX(), r.getRight());
    g.setColour (col::accent.withAlpha (0.3f));
    g.fillEllipse (x - 9.0f, y - 9.0f, 18.0f, 18.0f);
    g.setColour (col::accent);
    g.fillEllipse (x - 5.0f, y - 5.0f, 10.0f, 10.0f);
}

void XYPad::setFromMouse (const juce::MouseEvent& e)
{
    auto inner = getLocalBounds().toFloat().reduced (8.5f);
    px->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, (e.position.x - inner.getX()) / inner.getWidth()));
    py->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, (inner.getBottom() - e.position.y) / inner.getHeight()));
    repaint();
}

void XYPad::mouseDown (const juce::MouseEvent& e) { px->beginChangeGesture(); py->beginChangeGesture(); setFromMouse (e); }
void XYPad::mouseDrag (const juce::MouseEvent& e) { setFromMouse (e); }
void XYPad::mouseUp (const juce::MouseEvent&) { px->endChangeGesture(); py->endChangeGesture(); }

//==============================================================================
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

juce::Rectangle<int> StepEditor::barsArea() const { return getLocalBounds().withTrimmedBottom (32); }
juce::Rectangle<int> StepEditor::numbersArea() const { return getLocalBounds().withTrimmedBottom (10).removeFromBottom (22); }

float StepEditor::stepWidth() const
{
    const float fit = ((float) getWidth() - 16.0f) / (float) juce::jmax (1, pat.numSteps);
    return juce::jlimit (18.0f, 46.0f, fit);
}

float StepEditor::stepX (int step) const { return (float) step * stepWidth() - scrollX; }

int StepEditor::stepAtX (float x) const { return (int) std::floor ((x + scrollX) / stepWidth()); }

void StepEditor::updateScroll()
{
    const float total = (float) pat.numSteps * stepWidth() + 16.0f;
    const float visible = (float) getWidth();
    scrollX = juce::jlimit (0.0f, juce::jmax (0.0f, total - visible), scrollX);
    scroll.setRangeLimits (0.0, (double) juce::jmax (total, visible), juce::dontSendNotification);
    scroll.setCurrentRange ((double) scrollX, (double) visible, juce::dontSendNotification);
    scroll.setVisible (total > visible + 0.5f);
}

void StepEditor::scrollBarMoved (juce::ScrollBar*, double start) { scrollX = (float) start; repaint(); }

void StepEditor::resized()
{
    scroll.setBounds (getLocalBounds().removeFromBottom (9));
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

float StepEditor::noteEndX (int first, int last) const
{
    juce::ignoreUnused (first);
    return stepX (last) + pat.length[(size_t) last] * stepWidth();
}

void StepEditor::paint (juce::Graphics& g)
{
    const auto bars = barsArea().toFloat();
    const auto nums = numbersArea().toFloat();
    const float sw = stepWidth();
    g.setColour (col::lane);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 4.0f);
    g.saveState();
    g.reduceClipRegion (getLocalBounds().withTrimmedBottom (9));

    // grid
    for (int i = 0; i < pat.numSteps; ++i)
    {
        const float x = stepX (i);
        if (x + sw < 0.0f || x > (float) getWidth()) continue;
        if ((i / 4) % 2 == 0)
        {
            g.setColour (juce::Colours::white.withAlpha (0.025f));
            g.fillRect (x, bars.getY(), sw, bars.getHeight());
        }
        g.setColour (col::border.withAlpha (i % 4 == 0 ? 0.9f : 0.35f));
        g.drawVerticalLine (juce::roundToInt (x), bars.getY(), nums.getBottom());
    }
    for (int q = 1; q < 4; ++q)
    {
        g.setColour (col::border.withAlpha (0.3f));
        g.drawHorizontalLine (juce::roundToInt (bars.getY() + bars.getHeight() * (float) q / 4.0f), 0.0f,
                              juce::jmin ((float) getWidth(), stepX (pat.numSteps)));
    }

    // notes
    const float top = bars.getY() + 4.0f, h = bars.getHeight() - 6.0f;
    for (int i = 0; i < pat.numSteps; ++i)
    {
        const auto ii = (size_t) i;
        if (! pat.on[ii])
        {
            const float lh = pat.level[ii] * h;
            g.setColour (col::dim.withAlpha (0.18f));
            g.fillRect (stepX (i) + 1.5f, top + h - lh, pat.length[ii] * sw - 3.0f, lh);
            continue;
        }
        int first, last;
        noteRange (i, first, last);
        if (first != i) continue;
        const float x0 = stepX (first) + 1.5f, x1 = noteEndX (first, last) - 1.5f;
        if (x1 < 0.0f || x0 > (float) getWidth()) { i = last; continue; }
        const bool playing = playStep >= first && playStep <= last;
        const float lh = juce::jmax (2.0f, pat.level[ii] * h);
        // shaded gate-length bar over the full height, bright level bar on top
        g.setColour (col::accent.withAlpha (playing ? 0.22f : 0.10f));
        g.fillRoundedRectangle (x0, top, juce::jmax (2.0f, x1 - x0), h, 2.0f);
        g.setColour (playing ? col::accent.brighter (0.5f) : col::accent.withAlpha (0.85f));
        g.fillRoundedRectangle (x0, top + h - lh, juce::jmax (2.0f, x1 - x0), lh, 2.0f);
        g.setColour (juce::Colours::white.withAlpha (0.55f));
        g.fillRect (x1 - 2.0f, top + h - lh, 2.0f, lh); // length grip
        i = last;
    }

    // step number buttons with tie markers
    g.setFont (fontOf (sw < 22.0f ? 9.0f : 10.5f));
    for (int i = 0; i < pat.numSteps; ++i)
    {
        const auto ii = (size_t) i;
        const float x = stepX (i);
        if (x + sw < 0.0f || x > (float) getWidth()) continue;
        juce::Rectangle<float> b (x + 1.5f, nums.getY() + 2.0f, sw - 3.0f, nums.getHeight() - 4.0f);
        const bool on = pat.on[ii];
        g.setColour (i == playStep ? col::accent : (on ? col::panelHi.brighter (0.15f) : col::panel.darker (0.2f)));
        g.fillRoundedRectangle (b, 3.0f);
        g.setColour (i == playStep ? col::bg : (on ? col::text : col::dim.withAlpha (0.6f)));
        g.drawText (juce::String (i + 1), b, juce::Justification::centred, false);
        if (on && i + 1 < pat.numSteps && pat.tie[ii] && pat.on[ii + 1])
        {
            const float cy = nums.getY() + 1.0f;
            const float xa = x + sw * 0.5f, xb = x + sw * 1.5f;
            g.setColour (col::ring);
            g.drawLine (xa, cy, xb, cy, 2.0f);
            g.fillEllipse (xa - 3.0f, cy - 3.0f, 6.0f, 6.0f);
            g.fillEllipse (xb - 3.0f, cy - 3.0f, 6.0f, 6.0f);
        }
    }

    // pattern length handle
    const float hx = stepX (pat.numSteps);
    g.setColour (mode == Mode::handle ? col::ring.brighter (0.3f) : col::ring);
    g.fillRoundedRectangle (hx + 1.0f, bars.getY(), 5.0f, nums.getBottom() - bars.getY(), 2.0f);
    juce::Path tri;
    tri.addTriangle (hx + 7.0f, bars.getCentreY() - 6.0f, hx + 13.0f, bars.getCentreY(), hx + 7.0f, bars.getCentreY() + 6.0f);
    g.fillPath (tri);
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
    const float top = bars.getY() + 4.0f, h = bars.getHeight() - 6.0f;
    int first = step, last = step;
    noteRange (step, first, last);
    pat.level[(size_t) first] = juce::jlimit (0.0f, 1.0f, 1.0f - (y - top) / h);
}

void StepEditor::mouseMove (const juce::MouseEvent& e)
{
    auto cursor = juce::MouseCursor::NormalCursor;
    const float hx = stepX (pat.numSteps);
    if (e.position.x >= hx - 2.0f && e.position.x <= hx + 14.0f) cursor = juce::MouseCursor::LeftRightResizeCursor;
    else if (barsArea().contains (e.getPosition()))
    {
        const int s = stepAtX (e.position.x);
        if (s >= 0 && s < pat.numSteps && pat.on[(size_t) s])
        {
            int first, last;
            noteRange (s, first, last);
            if (std::abs (e.position.x - noteEndX (first, last)) <= 5.0f) cursor = juce::MouseCursor::LeftRightResizeCursor;
        }
    }
    setMouseCursor (cursor);
}

void StepEditor::mouseDown (const juce::MouseEvent& e)
{
    mode = Mode::none;
    const float x = e.position.x;
    const float hx = stepX (pat.numSteps);
    if (e.y >= getHeight() - 9) return;
    if (x >= hx - 2.0f && x <= hx + 14.0f) { mode = Mode::handle; repaint(); return; }

    const int s = stepAtX (x);
    if (s < 0 || s >= pat.numSteps) return;
    const float sw = stepWidth();

    if (e.y >= numbersArea().getY())
    {
        // click between two step numbers: create or remove a tie
        const float rel = x - stepX (s);
        int left = -1;
        if (rel <= 4.0f && s > 0) left = s - 1;
        else if (rel >= sw - 4.0f && s + 1 < pat.numSteps) left = s;
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
    const bool nearEdge = pat.on[(size_t) s] && std::abs (x - noteEndX (first, last)) <= 5.0f;
    // also catch the edge when the pointer is just inside the next step
    if (! nearEdge && s > 0 && pat.on[(size_t) (s - 1)] && x - stepX (s) <= 4.0f && ! e.mods.isAltDown())
    {
        int f2, l2;
        noteRange (s - 1, f2, l2);
        if (l2 == s - 1 && std::abs (x - noteEndX (f2, l2)) <= 5.0f)
        {
            mode = Mode::length; dragStep = f2; dragChainEnd = l2;
            return;
        }
    }
    if (pat.on[(size_t) s] && (nearEdge || e.mods.isAltDown()))
    {
        mode = Mode::length; dragStep = first; dragChainEnd = last;
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
            if (e.position.x > (float) getWidth() - 12.0f) scrollX += 6.0f;
            else if (e.position.x < 12.0f) scrollX -= 6.0f;
            pat.numSteps = juce::jlimit (1, kMaxSteps, juce::roundToInt ((e.position.x + scrollX) / sw));
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
    g.setColour (col::lane);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 4.0f);
    g.setColour (col::border);
    g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 4.0f, 1.0f);
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
    auto fill = path;
    fill.closeSubPath();
    g.setColour (col::accent.withAlpha (0.16f));
    g.fillPath (fill);
    g.setColour (col::accent);
    g.strokePath (path, juce::PathStrokeType (1.6f));
    for (int i = 4; i >= 0; --i)
    {
        const auto p = dot (i);
        const bool curveDot = i >= 3;
        if (curveDot && ((i == 3 && pat.attack < 0.02f) || (i == 4 && pat.release < 0.02f))) continue;
        g.setColour (curveDot ? col::dim : col::text);
        const float d = curveDot ? 7.0f : 9.0f;
        g.fillEllipse (p.x - d * 0.5f, p.y - d * 0.5f, d, d);
        g.setColour (col::bg);
        g.drawEllipse (p.x - d * 0.5f, p.y - d * 0.5f, d, d, 1.0f);
    }
}

void EnvelopeDisplay::mouseDown (const juce::MouseEvent& e)
{
    drag = -1;
    float best = 14.0f;
    // main dots win over curve dots when they overlap; release dot first because hold/attack may sit on it
    for (int i : { 3, 4, 0, 1, 2 })
    {
        if ((i == 3 && pat.attack < 0.02f) || (i == 4 && pat.release < 0.02f)) continue;
        const float d = dot (i).getDistanceFrom (e.position);
        if (d <= best) { best = d; drag = i; }
    }
    // overlapping dots: pick by drag direction later; simple rule here
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
    const float labelW = 92.0f;
    const float w = ((float) getWidth() - labelW) / (float) kNumBlocks;
    return { labelW + (float) index * w + 3.0f, 5.0f, w - 6.0f, (float) getHeight() - 10.0f };
}

int OrderStrip::slotAt (float x) const
{
    const float labelW = 92.0f;
    const float w = ((float) getWidth() - labelW) / (float) kNumBlocks;
    return juce::jlimit (0, kNumBlocks - 1, (int) std::floor ((x - labelW) / w));
}

void OrderStrip::paint (juce::Graphics& g)
{
    g.setColour (col::dim);
    g.setFont (fontOf (11.0f, true));
    g.drawText ("EFFECT ORDER", 8, 0, 84, getHeight(), juce::Justification::centredLeft, false);
    const auto ord = proc.getOrder();
    for (int i = 0; i < kNumBlocks; ++i)
    {
        const auto r = slot (i);
        const int b = ord[(size_t) i];
        const bool on = active[(size_t) b];
        g.setColour (i == dragging ? col::accentDim : (on ? col::panelHi.brighter (0.1f) : col::lane));
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (on ? col::accent : col::border);
        g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.0f);
        g.setColour (on ? col::text : col::dim.withAlpha (0.7f));
        g.setFont (fontOf (12.0f, on));
        g.drawText (blockName (b), r, juce::Justification::centred, false);
        if (i + 1 < kNumBlocks)
        {
            g.setColour (col::dim.withAlpha (0.6f));
            const float ax = r.getRight() + 3.0f, ay = r.getCentreY();
            juce::Path p;
            p.addTriangle (ax - 2.0f, ay - 3.0f, ax + 2.0f, ay, ax - 2.0f, ay + 3.0f);
            g.fillPath (p);
        }
    }
}

void OrderStrip::mouseDown (const juce::MouseEvent& e)
{
    dragging = e.position.x >= 92.0f ? slotAt (e.position.x) : -1;
    setMouseCursor (juce::MouseCursor::DraggingHandCursor);
    repaint();
}

void OrderStrip::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging < 0) return;
    const int to = slotAt (e.position.x);
    if (to == dragging) return;
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
StepFxEditor::StepFxEditor (StepFxProcessor& p)
    : AudioProcessorEditor (&p), proc (p), xyPad (p.apvts),
      depthKnob (p.apvts, "m1Depth", "Depth", tMod1Depth), rateKnob (p.apvts, "m1Rate", "Rate"), swingKnob (p.apvts, "m1Swing", "Swing"),
      orderStrip (p)
{
    setLookAndFeel (&laf);
    auto& s = proc.apvts;

    auto addKnob = [&] (const char* id, const char* caption, int target) -> Knob*
    {
        auto* k = knobs.add (new Knob (s, id, caption, target));
        addAndMakeVisible (k);
        return k;
    };
    auto attachButton = [&] (juce::ToggleButton& b, const char* id)
    {
        addAndMakeVisible (b);
        buttonAtts.add (new APVTS::ButtonAttachment (s, id, b));
    };
    auto attachCombo = [&] (juce::ComboBox& c, const char* id)
    {
        if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (s.getParameter (id)))
            c.addItemList (choice->choices, 1);
        addAndMakeVisible (c);
        comboAtts.add (new APVTS::ComboBoxAttachment (s, id, c));
    };

    // order of creation = order used in layoutPanels()
    addKnob ("modDelay", "Delay", tModDelay); addKnob ("modRate", "Rate", tModRate); addKnob ("modDepth", "Depth", tModDepth);
    addKnob ("modFeedback", "Feedback", tModFeedback); addKnob ("modMix", "Mix", tModMix);                       // 0..4
    kDelayMsL = addKnob ("delayTimeL", "Time L", -1); kDelayMsR = addKnob ("delayTimeR", "Time R", -1);          // 5,6
    kDelayDivL = addKnob ("delayDivL", "Time L", -1); kDelayDivR = addKnob ("delayDivR", "Time R", -1);          // 7,8
    addKnob ("delayFeedback", "Feedback", tDelayFeedback); addKnob ("delayCutoff", "Cutoff", tDelayCutoff);
    addKnob ("delayMix", "Mix", tDelayMix);                                                                       // 9..11
    kFilter[0] = addKnob ("filterCutoff", "Cutoff", tFilterCutoff); kFilter[1] = addKnob ("filterRes", "Res", tFilterRes);
    kFilter[2] = addKnob ("filterDrive", "Drive", tFilterDrive);                                                  // 12..14
    addKnob ("exciter", "Exciter", tExciter); addKnob ("dirt", "Dirt", tDirt);                                    // 15,16
    addKnob ("reverbTime", "Time", tReverbTime); addKnob ("reverbMix", "Mix", tReverbMix);                        // 17,18
    addKnob ("masterInput", "Input", tMasterInput); addKnob ("masterMix", "Mix", tMasterMix);
    addKnob ("masterOutput", "Output", tMasterOutput);                                                            // 19..21

    attachButton (modOn, "modOn"); attachButton (delayOn, "delayOn"); attachButton (filterOn, "filterOn");
    attachButton (distOn, "distOn"); attachButton (reverbOn, "reverbOn"); attachButton (delaySync, "delaySync");
    attachCombo (filterType, "filterType"); attachCombo (delayFilter, "delayFilter"); attachCombo (reverbType, "reverbType");

    // xy pad
    addAndMakeVisible (xyPad);
    attachButton (xOn, "xOn"); attachButton (yOn, "yOn");
    const char* xyIds[4] = { "xTarget1", "xTarget2", "yTarget1", "yTarget2" };
    for (int i = 0; i < 4; ++i) attachCombo (xyTarget[i], xyIds[i]);
    for (auto* sl : { &xDepth, &yDepth })
    {
        sl->setSliderStyle (juce::Slider::LinearBar);
        sl->setTextBoxIsEditable (true);
        addAndMakeVisible (sl);
    }
    sliderAtts.add (new APVTS::SliderAttachment (s, "xDepth", xDepth));
    sliderAtts.add (new APVTS::SliderAttachment (s, "yDepth", yDepth));

    // modulator rows
    for (int m = 0; m < kNumMods; ++m)
    {
        const juce::String pre = "m" + juce::String (m + 1);
        addAndMakeVisible (modOnBtn[m]);
        buttonAtts.add (new APVTS::ButtonAttachment (s, pre + "On", modOnBtn[m]));
        modSelBtn[m].setButtonText (juce::String (m + 1));
        modSelBtn[m].setClickingTogglesState (false);
        modSelBtn[m].onClick = [this, m] { selectMod (m); };
        addAndMakeVisible (modSelBtn[m]);
        modTarget[m].addItemList (modTargetNames(), 1);
        addAndMakeVisible (modTarget[m]);
        comboAtts.add (new APVTS::ComboBoxAttachment (s, pre + "Target", modTarget[m]));
        modTarget[m].addMouseListener (this, true);
    }

    addAndMakeVisible (depthKnob); addAndMakeVisible (rateKnob); addAndMakeVisible (swingKnob);
    for (auto* b : { &prevBtn, &nextBtn, &presetBtn, &actionBtn, &saveBtn, &cancelBtn }) addAndMakeVisible (b);
    saveBtn.setVisible (false); cancelBtn.setVisible (false);
    addChildComponent (nameEditor);
    nameEditor.setFont (fontOf (13.0f));
    nameEditor.onReturnKey = [this] { endSaveAs (true); };
    nameEditor.onEscapeKey = [this] { endSaveAs (false); };
    saveBtn.onClick = [this] { endSaveAs (true); };
    cancelBtn.onClick = [this] { endSaveAs (false); };
    prevBtn.onClick = [this] { stepPreset (-1); };
    nextBtn.onClick = [this] { stepPreset (1); };
    presetBtn.onClick = [this] { showPresetMenu(); };
    actionBtn.onClick = [this] { showActionMenu(); };

    addAndMakeVisible (stepEditor);
    stepEditor.onChange = [this] { pushPattern (stepEditor.getPattern(), "Custom"); };
    addAndMakeVisible (envDisplay);
    envDisplay.onChange = [this]
    {
        auto pat = stepEditor.getPattern();
        const auto& e = envDisplay.getPattern();
        pat.attack = e.attack; pat.hold = e.hold; pat.release = e.release;
        pat.attackCurve = e.attackCurve; pat.releaseCurve = e.releaseCurve;
        pushPattern (pat, "Custom");
    };

    const char* envNames[3] = { "Attack", "Hold", "Release" };
    juce::Slider* envSliders[3] = { &envA, &envH, &envR };
    for (int i = 0; i < 3; ++i)
    {
        auto* sl = envSliders[i];
        sl->setSliderStyle (juce::Slider::LinearBar);
        sl->setRange (0.0, 100.0, 1.0);
        sl->setTextValueSuffix (" %");
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
        envLabels[i].setText (envNames[i], juce::dontSendNotification);
        envLabels[i].setFont (fontOf (11.0f));
        envLabels[i].setColour (juce::Label::textColourId, col::dim);
        envLabels[i].setJustificationType (juce::Justification::centred);
        addAndMakeVisible (envLabels[i]);
    }
    lengthLabel.setFont (fontOf (11.0f));
    lengthLabel.setColour (juce::Label::textColourId, col::dim);
    lengthLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (lengthLabel);

    addAndMakeVisible (orderStrip);

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

//==============================================================================
void StepFxEditor::layoutPanels()
{
    panels.clear();
    const int topY = 40, topH = 252;
    auto panel = [&] (int x, int w, const char* title) { panels.push_back ({ { x, topY, w, topH }, title }); return juce::Rectangle<int> (x, topY, w, topH); };
    const int cellW = 66, cellH = 66;
    auto grid = [&] (juce::Rectangle<int> area, std::initializer_list<Knob*> ks)
    {
        int i = 0;
        const int x0 = area.getX() + (area.getWidth() - 2 * cellW) / 2;
        for (auto* k : ks)
        {
            if (k != nullptr) k->setBounds (x0 + (i % 2) * cellW, area.getY() + (i / 2) * cellH, cellW, cellH - 2);
            ++i;
        }
    };

    // XY pad
    {
        auto r = panel (10, 200, "XY PAD");
        auto in = r.reduced (8).withTrimmedTop (20);
        xyPad.setBounds (in.removeFromTop (128).withSizeKeepingCentre (184, 128));
        in.removeFromTop (5);
        auto row = [&] (juce::ToggleButton& on, juce::Slider& depth, juce::ComboBox& a, juce::ComboBox& b)
        {
            auto l1 = in.removeFromTop (20);
            on.setBounds (l1.removeFromLeft (38));
            depth.setBounds (l1.reduced (0, 1));
            in.removeFromTop (3);
            auto l2 = in.removeFromTop (20);
            a.setBounds (l2.removeFromLeft (l2.getWidth() / 2).withTrimmedRight (2));
            b.setBounds (l2.withTrimmedLeft (2));
            in.removeFromTop (4);
        };
        row (xOn, xDepth, xyTarget[0], xyTarget[1]);
        row (yOn, yDepth, xyTarget[2], xyTarget[3]);
    }

    const int pw = 138, gap = 6;
    int x = 216;
    auto header = [&] (juce::Rectangle<int> r, juce::ToggleButton& on) { on.setBounds (r.getRight() - 24, r.getY() + 5, 18, 16); };
    {   // Mod FX
        auto r = panel (x, pw, "MOD FX"); header (r, modOn);
        grid (r.withTrimmedTop (52), { knobs[0], knobs[1], knobs[2], knobs[3], knobs[4] });
        x += pw + gap;
    }
    {   // Delay
        auto r = panel (x, pw, "DELAY"); header (r, delayOn);
        auto line = r.withTrimmedTop (26).removeFromTop (22).reduced (6, 0);
        delaySync.setBounds (line.removeFromLeft (54));
        delayFilter.setBounds (line);
        const bool sync = proc.apvts.getRawParameterValue ("delaySync")->load() > 0.5f;
        grid (r.withTrimmedTop (52), { sync ? kDelayDivL : kDelayMsL, sync ? kDelayDivR : kDelayMsR, knobs[9], knobs[10], knobs[11] });
        kDelayMsL->setVisible (! sync); kDelayMsR->setVisible (! sync);
        kDelayDivL->setVisible (sync); kDelayDivR->setVisible (sync);
        x += pw + gap;
    }
    {   // Filter
        auto r = panel (x, pw, "FILTER"); header (r, filterOn);
        filterType.setBounds (r.withTrimmedTop (26).removeFromTop (22).reduced (6, 0));
        grid (r.withTrimmedTop (52), { kFilter[0], kFilter[1], kFilter[2] });
        x += pw + gap;
    }
    {   // Distortion
        auto r = panel (x, pw, "DISTORTION"); header (r, distOn);
        grid (r.withTrimmedTop (52), { knobs[15], knobs[16] });
        x += pw + gap;
    }
    {   // Reverb
        auto r = panel (x, pw, "REVERB"); header (r, reverbOn);
        reverbType.setBounds (r.withTrimmedTop (26).removeFromTop (22).reduced (6, 0));
        grid (r.withTrimmedTop (52), { knobs[17], knobs[18] });
        x += pw + gap;
    }
    {   // Master
        auto r = panel (x, kW - 10 - x, "MASTER");
        auto in = r.withTrimmedTop (30);
        for (int i = 0; i < 3; ++i)
            knobs[19 + i]->setBounds (in.getCentreX() - cellW / 2, in.getY() + i * (cellH + 6), cellW, cellH);
    }
}

void StepFxEditor::resized()
{
    layoutPanels();
    const int midY = 298, midH = 316;
    panels.push_back ({ { 10, midY, kW - 20, midH }, "STEP MODULATOR" });

    // modulator rows (left)
    for (int m = 0; m < kNumMods; ++m)
    {
        juce::Rectangle<int> row (18, midY + 34 + m * 34, 184, 26);
        modOnBtn[m].setBounds (row.removeFromLeft (20).withSizeKeepingCentre (18, 16));
        modSelBtn[m].setBounds (row.removeFromLeft (26).reduced (0, 1));
        row.removeFromLeft (4);
        modTarget[m].setBounds (row.reduced (0, 1));
    }

    // toolbar + step editor (centre)
    juce::Rectangle<int> centre (216, midY + 8, 590, midH - 16);
    auto bar = centre.removeFromTop (26).withTrimmedLeft (140);
    prevBtn.setBounds (bar.removeFromLeft (24).reduced (0, 2));
    bar.removeFromLeft (3);
    auto actionArea = bar.removeFromRight (78).reduced (0, 2);
    bar.removeFromRight (4);
    auto lenArea = bar.removeFromRight (70);
    nextBtn.setBounds (bar.removeFromRight (24).reduced (0, 2));
    bar.removeFromRight (3);
    presetBtn.setBounds (bar.reduced (0, 2));
    actionBtn.setBounds (actionArea);
    lengthLabel.setBounds (lenArea);
    // save-as overlay shares the toolbar space
    {
        auto o = presetBtn.getBounds().getUnion (nextBtn.getBounds());
        cancelBtn.setBounds (o.removeFromRight (58));
        o.removeFromRight (3);
        saveBtn.setBounds (o.removeFromRight (50));
        o.removeFromRight (3);
        nameEditor.setBounds (o);
    }
    centre.removeFromTop (6);
    stepEditor.setBounds (centre);

    // playback controls + envelope (right)
    juce::Rectangle<int> right (814, midY + 30, 208, midH - 38);
    auto kr = right.removeFromTop (70);
    const int kw = kr.getWidth() / 3;
    depthKnob.setBounds (kr.removeFromLeft (kw));
    rateKnob.setBounds (kr.removeFromLeft (kw));
    swingKnob.setBounds (kr);
    right.removeFromTop (8);
    auto fields = right.removeFromBottom (36);
    right.removeFromBottom (6);
    envDisplay.setBounds (right);
    const int fw = fields.getWidth() / 3;
    juce::Slider* envSliders[3] = { &envA, &envH, &envR };
    for (int i = 0; i < 3; ++i)
    {
        auto f = fields.removeFromLeft (fw).reduced (2, 0);
        envLabels[i].setBounds (f.removeFromTop (14));
        envSliders[i]->setBounds (f.reduced (0, 1));
    }

    orderStrip.setBounds (10, kH - 44, kW - 20, 36);
    firstLayout = false;
}

void StepFxEditor::paint (juce::Graphics& g)
{
    g.fillAll (col::bg);
    g.setColour (col::text);
    g.setFont (fontOf (18.0f, true));
    g.drawText ("StepFlux", 14, 6, 200, 28, juce::Justification::centredLeft, false);
    g.setColour (col::dim);
    g.setFont (fontOf (11.0f));
    g.drawText ("step modulated multi effect", 100, 9, 300, 24, juce::Justification::centredLeft, false);

    for (auto& p : panels)
    {
        g.setColour (col::panel);
        g.fillRoundedRectangle (p.r.toFloat(), 6.0f);
        g.setColour (col::border);
        g.drawRoundedRectangle (p.r.toFloat().reduced (0.5f), 6.0f, 1.0f);
        g.setColour (col::dim);
        g.setFont (fontOf (11.0f, true));
        g.drawText (p.title, p.r.getX() + 9, p.r.getY() + 4, p.r.getWidth() - 34, 18, juce::Justification::centredLeft, false);
    }
    g.setColour (col::accentDim);
    g.fillRoundedRectangle (juce::Rectangle<int> (16, 298 + 32 + sel * 34, 188, 30).toFloat(), 4.0f);
    orderStrip.getBounds();
    g.setColour (col::panel);
    g.fillRoundedRectangle (orderStrip.getBounds().toFloat(), 6.0f);
    g.setColour (col::border);
    g.drawRoundedRectangle (orderStrip.getBounds().toFloat().reduced (0.5f), 6.0f, 1.0f);
}

void StepFxEditor::mouseDown (const juce::MouseEvent& e)
{
    for (int m = 0; m < kNumMods; ++m)
        if (e.eventComponent == &modTarget[m] || modTarget[m].isParentOf (e.eventComponent))
            selectMod (m);
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
    for (int i = 0; i < kNumMods; ++i)
    {
        modSelBtn[i].setToggleState (i == sel, juce::dontSendNotification);
        modTarget[i].getProperties().set ("selected", i == sel);
        modTarget[i].repaint();
    }
    refreshFromProcessor();
    repaint();
}

void StepFxEditor::refreshFromProcessor()
{
    const auto pat = proc.getPattern (sel);
    stepEditor.setPattern (pat);
    envDisplay.setPattern (pat);
    presetBtn.setButtonText (proc.getPatternName (sel));
    updateEnvFields();
}

void StepFxEditor::updateEnvFields()
{
    const auto& pat = stepEditor.getPattern();
    envA.setValue (pat.attack * 100.0, juce::dontSendNotification);
    envH.setValue (pat.hold * 100.0, juce::dontSendNotification);
    envR.setValue (pat.release * 100.0, juce::dontSendNotification);
    lengthLabel.setText (juce::String (pat.numSteps) + (pat.numSteps == 1 ? " step" : " steps"), juce::dontSendNotification);
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
    presetBtn.setVisible (false); nextBtn.setVisible (false);
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
    presetBtn.setVisible (true); nextBtn.setVisible (true);
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
        for (int i = 0; i < 3; ++i) kFilter[i]->setCaption (names[(size_t) i]);
    }
    const bool sync = proc.apvts.getRawParameterValue ("delaySync")->load() > 0.5f;
    if (sync != lastSync || firstLayout) { lastSync = sync; resized(); repaint(); }
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
    for (int k = 0; k < 4; ++k)
    {
        const bool isX = k < 2;
        if (s.getRawParameterValue (isX ? "xOn" : "yOn")->load() < 0.5f) continue;
        const int t = xyChoiceToTarget ((int) s.getRawParameterValue (ids[k])->load());
        if (t == tPan) panActive = true;
        add (t, s.getRawParameterValue (isX ? "xDepth" : "yDepth")->load() * 0.01f);
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
