#include "PluginEditor.h"
#include "Presets.h"
#include "Samples.h"
#include "Format.h"

using namespace gs;

int ui::textW (const juce::Font& f, const juce::String& s) { return juce::GlyphArrangement::getStringWidthInt (f, s); }

juce::Font ui::mono (float size, bool bold)
{
    return juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), size, bold ? juce::Font::bold : juce::Font::plain));
}

// =============== Look and feel ===============
GSLookAndFeel::GSLookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, ui::bg);
    setColour (juce::Label::textColourId, ui::ink);
    setColour (juce::Slider::textBoxTextColourId, ui::ink);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxHighlightColourId, ui::blue.withAlpha (0.3f));
    setColour (juce::TextEditor::textColourId, ui::ink);
    setColour (juce::TextEditor::backgroundColourId, ui::bg);
    setColour (juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
    setColour (juce::TextEditor::focusedOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::ComboBox::backgroundColourId, ui::bg);
    setColour (juce::ComboBox::textColourId, ui::ink);
    setColour (juce::ComboBox::outlineColourId, ui::ink);
    setColour (juce::ComboBox::arrowColourId, ui::ink);
    setColour (juce::PopupMenu::backgroundColourId, ui::bg);
    setColour (juce::PopupMenu::textColourId, ui::ink);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, ui::ink);
    setColour (juce::PopupMenu::highlightedTextColourId, ui::bg);
    setColour (juce::TextButton::buttonColourId, ui::bg);
    setColour (juce::TextButton::buttonOnColourId, ui::ink);
    setColour (juce::TextButton::textColourOffId, ui::ink);
    setColour (juce::TextButton::textColourOnId, ui::bg);
    setColour (juce::ScrollBar::thumbColourId, ui::ink);
}

void GSLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float start, float end, juce::Slider& s)
{
    const float r = juce::jmin (w, h) * 0.5f - 3.0f;
    const auto c = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).getCentre();
    const float ang = start + pos * (end - start);
    const bool bip = (bool) s.getProperties().getWithDefault ("bip", false);
    const float a0 = bip ? (start + end) * 0.5f : start;
    juce::Path track; track.addCentredArc (c.x, c.y, r, r, 0, start, end, true);
    g.setColour (ui::grey); g.strokePath (track, juce::PathStrokeType (3.0f));
    if (std::abs (ang - a0) > 0.01f)
    {
        juce::Path v; v.addCentredArc (c.x, c.y, r, r, 0, juce::jmin (a0, ang), juce::jmax (a0, ang), true);
        g.setColour (s.findColour (juce::Slider::rotarySliderFillColourId)); g.strokePath (v, juce::PathStrokeType (3.0f));
    }
    const float cr = r * 0.68f;
    g.setColour (ui::bg); g.fillEllipse (c.x - cr, c.y - cr, cr * 2, cr * 2);
    g.setColour (ui::ink); g.drawEllipse (c.x - cr, c.y - cr, cr * 2, cr * 2, 1.5f);
    const juce::Point<float> d (std::sin (ang), -std::cos (ang));
    g.drawLine (juce::Line<float> (c + d * (cr * 0.25f), c + d * (cr * 0.92f)), 2.2f);
}

void GSLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    juce::Colour fill = b.getToggleState() ? b.findColour (juce::TextButton::buttonOnColourId) : b.findColour (juce::TextButton::buttonColourId);
    if (down || (over && ! b.getToggleState())) fill = down ? ui::ink : ui::faint;
    g.setColour (fill); g.fillRect (r);
    g.setColour (b.getToggleState() ? b.findColour (juce::TextButton::buttonOnColourId) : ui::ink); g.drawRect (r, 1.0f);
}

void GSLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool down)
{
    g.setFont (ui::mono (b.getProperties().getWithDefault ("big", false) ? 13.0f : 10.5f, (bool) b.getProperties().getWithDefault ("big", false)));
    g.setColour (b.getToggleState() || down ? b.findColour (juce::TextButton::textColourOnId) : b.findColour (juce::TextButton::textColourOffId));
    g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (3, 1), juce::Justification::centred, 1);
}

void GSLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox&)
{
    g.setColour (ui::bg); g.fillRect (0, 0, w, h);
    g.setColour (ui::ink); g.drawRect (0, 0, w, h, 1);
    juce::Path p; const float ax = (float) w - 12, ay = (float) h * 0.5f; p.addTriangle (ax - 4, ay - 2, ax + 4, ay - 2, ax, ay + 3); g.fillPath (p);
}

void GSLookAndFeel::drawScrollbar (juce::Graphics& g, juce::ScrollBar&, int x, int y, int w, int h, bool vertical, int thumbStart, int thumbSize, bool, bool)
{
    g.setColour (ui::faint); g.fillRect (x, y, w, h);
    g.setColour (ui::ink);
    if (vertical) g.fillRect (x + 2, thumbStart, w - 4, thumbSize); else g.fillRect (thumbStart, y + 2, thumbSize, h - 4);
}

// =============== Knob ===============
Knob::Knob (GSProcessor& p, const juce::String& paramId, const juce::String& t) : proc (p), id (paramId), title (t.toUpperCase())
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 72, 14);
    slider.setColour (juce::Slider::rotarySliderFillColourId, ui::blue);
    // set the value-box colours on the slider itself so they apply before the editor's look-and-feel reaches it
    slider.setColour (juce::Slider::textBoxTextColourId, ui::ink);
    slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    slider.setColour (juce::Slider::textBoxHighlightColourId, ui::blue.withAlpha (0.3f));
    slider.setMouseDragSensitivity (200);
    slider.setPopupMenuEnabled (false);
    if (auto* rp = p.apvts.getParameter (paramId))
        slider.getProperties().set ("bip", rp->convertFrom0to1 (0.0f) < 0.0f);
    addAndMakeVisible (slider);
    label.setText (title, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centredBottom);
    label.setColour (juce::Label::textColourId, juce::Colour (0xff555555));
    label.setFont (ui::mono (9.5f));
    label.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (label);
    att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (p.apvts, paramId, slider);
    if (auto* rp = p.apvts.getParameter (paramId)) slider.setDoubleClickReturnValue (true, rp->convertFrom0to1 (rp->getDefaultValue()));
    slider.onRightClick = [this]
    {
        juce::PopupMenu m;
        const int cc = proc.getCCFor (id);
        m.addSectionHeader (cc >= 0 ? "MIDI CC " + juce::String (cc) : "NO MIDI CC");
        m.addItem (1, "MIDI LEARN (move a controller)");
        m.addItem (2, "CLEAR MIDI CC", cc >= 0);
        m.addItem (3, "RESET TO DEFAULT");
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&slider), [this] (int r)
        {
            if (r == 1) { proc.startLearn (id); startTimerHz (10); }
            else if (r == 2) proc.clearCC (id);
            else if (r == 3) if (auto* rp = proc.apvts.getParameter (id)) { rp->beginChangeGesture(); rp->setValueNotifyingHost (rp->getDefaultValue()); rp->endChangeGesture(); }
        });
    };
}

void Knob::timerCallback()
{
    const bool learning = proc.getLearnTarget() == id;
    label.setColour (juce::Label::textColourId, learning ? ui::blue : modulated ? ui::green : juce::Colour (0xff555555));
    label.setText (learning ? "LEARNING..." : label.getText(), juce::dontSendNotification);
    if (! learning)
    {
        stopTimer();
        label.setText (title, juce::dontSendNotification);
        setModulated (modulated);
    }
}

void Knob::setModulated (bool m)
{
    modulated = m;
    slider.setColour (juce::Slider::rotarySliderFillColourId, m ? ui::green : ui::blue);
    slider.setColour (juce::Slider::textBoxTextColourId, m ? ui::green : ui::ink);
    label.setColour (juce::Label::textColourId, m ? ui::green : juce::Colour (0xff555555));
}

void Knob::resized()
{
    auto r = getLocalBounds();
    label.setBounds (r.removeFromTop (16));
    slider.setBounds (r);
}

// =============== ChoiceButtons / LabeledCombo ===============
ChoiceButtons::ChoiceButtons (GSProcessor& p, const juce::String& paramId, const juce::String& title, int labelWidth) : labelW (labelWidth)
{
    label.setText (title.toUpperCase(), juce::dontSendNotification); label.setFont (ui::mono (9.5f));
    label.setColour (juce::Label::textColourId, juce::Colour (0xff555555)); addAndMakeVisible (label);
    auto* rp = p.apvts.getParameter (paramId);
    juce::StringArray items;
    if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (rp)) items = c->choices;
    else items = { "OFF", "ON" };
    for (int i = 0; i < items.size(); ++i)
    {
        auto* b = buttons.add (new juce::TextButton (items[i]));
        b->setClickingTogglesState (false);
        b->onClick = [this, i] { if (att) att->setValueAsCompleteGesture ((float) i); };
        addAndMakeVisible (b);
    }
    att = std::make_unique<juce::ParameterAttachment> (*rp, [this] (float v) { refresh (v); });
    att->sendInitialUpdate();
}
void ChoiceButtons::refresh (float v) { for (int i = 0; i < buttons.size(); ++i) buttons[i]->setToggleState (juce::roundToInt (v) == i, juce::dontSendNotification); }
int ChoiceButtons::preferredWidth() const
{
    int w = labelW; for (auto* b : buttons) w += juce::jmax (34, ui::textW (ui::mono (10.5f), b->getButtonText()) + 14) + 3; return w;
}
void ChoiceButtons::resized()
{
    auto r = getLocalBounds();
    label.setBounds (r.removeFromLeft (labelW));
    int x = r.getX(), y = r.getY(); const int h = 20;
    for (auto* b : buttons)
    {
        const int w = juce::jmax (34, ui::textW (ui::mono (10.5f), b->getButtonText()) + 14);
        if (x + w > getWidth() && x > r.getX()) { x = r.getX(); y += h + 3; }
        b->setBounds (x, y + 1, w, h); x += w + 3;
    }
}

LabeledCombo::LabeledCombo (GSProcessor& p, const juce::String& paramId, const juce::String& title, int labelWidth) : labelW (labelWidth)
{
    label.setText (title.toUpperCase(), juce::dontSendNotification); label.setFont (ui::mono (9.5f));
    label.setColour (juce::Label::textColourId, juce::Colour (0xff555555)); addAndMakeVisible (label);
    auto* rp = p.apvts.getParameter (paramId);
    if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (rp)) box.addItemList (c->choices, 1);
    else if (auto* ip = dynamic_cast<juce::AudioParameterInt*> (rp)) for (int i = ip->getRange().getStart(); i <= ip->getRange().getEnd(); ++i) box.addItem (juce::String (i), i - ip->getRange().getStart() + 1);
    addAndMakeVisible (box);
    att = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (p.apvts, paramId, box);
}
void LabeledCombo::resized() { auto r = getLocalBounds(); label.setBounds (r.removeFromLeft (labelW)); box.setBounds (r.withSizeKeepingCentre (r.getWidth(), 22)); }

// =============== Waveform ===============
WaveView::WaveView (GSProcessor& p) : proc (p) { startTimerHz (30); }

void WaveView::timerCallback()
{
    if (proc.sampleVersion.load() != peaksVer || getWidth() != peaksW)
    {
        peaksVer = proc.sampleVersion.load(); peaksW = getWidth();
        peaks.assign ((size_t) juce::jmax (1, peaksW) * 2, 0.f);
        if (auto s = proc.getSample())
        {
            const size_t n = s->length();
            for (int c = 0; c < peaksW && n > 0; ++c)
            {
                const size_t a = (size_t) ((double) c / peaksW * (double) n), b = (size_t) ((double) (c + 1) / peaksW * (double) n);
                const size_t stp = juce::jmax ((size_t) 1, (b - a) / 128); float mn = 0, mx = 0;
                for (size_t i = a; i < b && i < n; i += stp) { const float v = (s->L[i] + s->R[i]) * 0.5f; mn = juce::jmin (mn, v); mx = juce::jmax (mx, v); }
                peaks[(size_t) c * 2] = mn; peaks[(size_t) c * 2 + 1] = mx;
            }
        }
    }
    repaint();
}

void WaveView::paint (juce::Graphics& g)
{
    const float W = (float) getWidth(), H = (float) getHeight(), top = (float) strip, h = H - top, mid = top + h / 2;
    g.fillAll (ui::bg);
    g.setColour (juce::Colour (0xfff2f2f0)); g.fillRect (0.f, 0.f, W, top);
    g.setColour (ui::ink); g.fillRect (0.f, top - 1, W, 1.f);
    for (int i = 0; i <= 16; ++i) g.fillRect ((float) juce::roundToInt (i / 16.f * W), top, 1.f, 4.f);
    const float base = proc.vizBase.load(), spray = proc.getParam ("spray") * 0.5f;
    g.setColour (ui::green.withAlpha (0.12f));
    { const float x0 = (base - spray) * W, x1 = (base + spray) * W; g.fillRect (x0, top, x1 - x0, h); if (x0 < 0) g.fillRect (W + x0, top, -x0, h); if (x1 > W) g.fillRect (0.f, top, x1 - W, h); }
    const int sl = juce::roundToInt (proc.getParam ("shuffle"));
    if (sl >= 2) { g.setColour (ui::blue.withAlpha (0.25f)); for (int i = 0; i < sl; ++i) g.fillRect ((float) juce::roundToInt (i / (float) sl * W), top, 1.f, h); }
    g.setColour (juce::Colour (0xffd8d8d4)); g.fillRect (0.f, mid, W, 1.f);
    g.setColour (ui::ink);
    for (int c = 0; c < (int) peaks.size() / 2; ++c)
    {
        const float y0 = mid - peaks[(size_t) c * 2 + 1] * h * 0.46f, y1 = mid - peaks[(size_t) c * 2] * h * 0.46f;
        g.fillRect ((float) c, y0, 1.f, juce::jmax (1.f, y1 - y0));
    }
    // markers
    const auto markers = proc.getMarkers(); const auto steps = proc.getSteps();
    const int cur = proc.vizStep.load(); const int curM = (cur >= 0 && steps[(size_t) cur].on) ? steps[(size_t) cur].m : -2;
    g.setFont (ui::mono (10, true));
    for (int i = 0; i < (int) markers.size(); ++i)
    {
        const float x = (float) juce::roundToInt (markers[(size_t) i] * W); const bool act = i == curM;
        g.setColour (act ? ui::green : ui::blue.withAlpha (0.8f)); g.fillRect (x, top, act ? 2.f : 1.f, h);
        const juce::String lbl (i + 1); const float w = (float) juce::jmax (16, ui::textW (ui::mono (10, true), lbl) + 8);
        g.setColour (act ? ui::green : (i == hover || i == dragMarker) ? juce::Colour (0xff5b74ff) : ui::blue); g.fillRect (x, 3.f, w, top - 6);
        g.setColour (juce::Colours::white); g.drawText (lbl, juce::Rectangle<float> (x + 4, 3, w - 4, top - 6), juce::Justification::centredLeft);
    }
    g.setColour (ui::green); g.fillRect (base * W - 1, top, 2.f, h); g.fillRect (base * W - 5, top, 10.f, 3.f);
    const int n = proc.vizCount.load();
    for (int i = 0; i < n; ++i)
    {
        const float p = proc.vizPos[(size_t) i].load (std::memory_order_relaxed), a = proc.vizAmp[(size_t) i].load (std::memory_order_relaxed);
        float hh = std::sin ((float) (i + 1) * 127.1f + std::floor (p * 50) * 311.7f) * 43758.5453f; hh -= std::floor (hh);
        const float x = p * W, y = top + h * (0.1f + 0.8f * hh), r = 1 + juce::jmin (1.f, a * 3) * 2.5f;
        g.setColour (ui::blue.withAlpha (0.25f + juce::jmin (0.75f, a * 3))); g.fillRect (x - r, y - r, r * 2, r * 2);
    }
    g.setColour (ui::ink); g.drawRect (getLocalBounds(), 1);
    if (dropping)
    {
        g.setColour (juce::Colours::white.withAlpha (0.85f)); g.fillRect (getLocalBounds());
        g.setColour (ui::blue); g.drawRect (getLocalBounds().reduced (2), 2); g.setFont (ui::mono (16, true));
        g.drawText ("[ DROP AUDIO FILE ]", getLocalBounds(), juce::Justification::centred);
    }
}

int WaveView::markerAt (float x) const
{
    const auto m = proc.getMarkers(); int best = -1; float bd = 1e9f;
    for (int i = 0; i < (int) m.size(); ++i) { const float mx = m[(size_t) i] * (float) getWidth(); if (x >= mx - 3 && x <= mx + 20) { const float d = std::abs (x - mx - 6); if (d < bd) { bd = d; best = i; } } }
    return best;
}
void WaveView::setPos (float x)
{
    if (auto* p = proc.apvts.getParameter ("pos")) p->setValueNotifyingHost (juce::jlimit (0.f, 1.f, x / (float) getWidth()));
    proc.resetScan();
}
void WaveView::mouseDown (const juce::MouseEvent& e)
{
    if (e.y < strip)
    {
        const int i = markerAt ((float) e.x);
        if (e.mods.isPopupMenu()) { if (i >= 0) proc.deleteMarker (i); return; }
        if (i >= 0) { dragMarker = i; return; }
        auto m = proc.getMarkers(); if (m.size() >= 32) return;
        m.push_back (juce::jlimit (0.f, 0.9999f, (float) e.x / (float) getWidth())); dragMarker = (int) m.size() - 1; proc.setMarkers (m);
        return;
    }
    posDrag = true;
    if (auto* p = proc.apvts.getParameter ("pos")) p->beginChangeGesture();
    setPos ((float) e.x);
}
void WaveView::mouseDrag (const juce::MouseEvent& e)
{
    if (dragMarker >= 0) { auto m = proc.getMarkers(); if (dragMarker < (int) m.size()) { m[(size_t) dragMarker] = juce::jlimit (0.f, 0.9999f, (float) e.x / (float) getWidth()); proc.setMarkers (m); } }
    else if (posDrag) setPos ((float) e.x);
}
void WaveView::mouseUp (const juce::MouseEvent&)
{
    if (posDrag) if (auto* p = proc.apvts.getParameter ("pos")) p->endChangeGesture();
    posDrag = false; dragMarker = -1;
}
void WaveView::mouseDoubleClick (const juce::MouseEvent& e) { if (e.y < strip) { const int i = markerAt ((float) e.x); if (i >= 0) proc.deleteMarker (i); } }
void WaveView::mouseMove (const juce::MouseEvent& e)
{
    hover = e.y < strip ? markerAt ((float) e.x) : -1;
    setMouseCursor (e.y < strip ? (hover >= 0 ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::CopyingCursor) : juce::MouseCursor::CrosshairCursor);
}
bool WaveView::isInterestedInFileDrag (const juce::StringArray& f) { for (auto& s : f) if (juce::File (s).hasFileExtension ("wav;aif;aiff;mp3;flac;ogg;m4a;caf;aac")) return true; return false; }
void WaveView::filesDropped (const juce::StringArray& f, int, int) { dropping = false; for (auto& s : f) if (proc.loadSampleFile (juce::File (s))) break; repaint(); }

// =============== previews ===============
void WindowPreview::paint (juce::Graphics& g)
{
    g.fillAll (ui::bg);
    float A = juce::jmax (0.002f, proc.getParam ("gatk")), R = juce::jmax (0.002f, proc.getParam ("grel")); if (A + R > 1) { const float s = A + R; A /= s; R /= s; }
    const float seg = juce::jlimit (0.f, 0.9999f, proc.getParam ("gcurve")) * 4; const int si = (int) seg; const float fr = seg - (float) si;
    auto f = [] (int i, float x) { switch (i) { case 0: return x * x * x * x; case 1: return x; case 2: return 0.5f - 0.5f * std::cos (kPi * x); case 3: return std::sin (kPi * x / 2); default: return std::sqrt (std::sin (kPi * x / 2)); } };
    const float w = (float) getWidth(), h = (float) getHeight(); juce::Path p; p.startNewSubPath (0, h);
    for (int i = 0; i <= (int) w; ++i) { const float t = i / w; float x = t < A ? t / A : t > 1 - R ? (1 - t) / R : 1; x = juce::jlimit (0.f, 1.f, x); p.lineTo ((float) i, h - 3 - (f (si, x) * (1 - fr) + f (si + 1, x) * fr) * (h - 6)); }
    p.lineTo (w, h); g.setColour (ui::blue.withAlpha (0.15f)); g.fillPath (p); g.setColour (ui::blue); g.strokePath (p, juce::PathStrokeType (1.5f));
    g.setColour (ui::ink); g.drawRect (getLocalBounds(), 1);
}
void EnvPreview::paint (juce::Graphics& g)
{
    g.fillAll (ui::bg);
    const float a = proc.getParam ("fatk"), d = proc.getParam ("fdec"), s = proc.getParam ("fsus"), r = proc.getParam ("frel"), amt = proc.getParam ("fenv");
    const float tot = a + d + 0.35f + r, w = (float) getWidth(), h = (float) getHeight();
    auto X = [&] (float t) { return t / tot * w; }; auto Y = [&] (float v) { return h - 3 - v * (h - 6); };
    juce::Path p; p.startNewSubPath (0, Y (0)); p.lineTo (X (a), Y (1));
    for (int i = 1; i <= 30; ++i) { const float t = i / 30.f; p.lineTo (X (a + d * t), Y (s + (1 - s) * std::exp (-3 * t))); }
    p.lineTo (X (a + d + 0.35f), Y (s));
    for (int i = 1; i <= 30; ++i) { const float t = i / 30.f; p.lineTo (X (a + d + 0.35f + r * t), Y (s * std::exp (-3 * t))); }
    g.setColour (amt == 0 ? ui::mid : ui::blue); g.strokePath (p, juce::PathStrokeType (1.5f));
    const float fe = proc.vizEnv.load(); if (fe > 0) { g.setColour (ui::green); g.fillRect (0.f, Y (fe) - 1, w, 2.f); }
    g.setColour (ui::ink); g.drawRect (getLocalBounds(), 1);
}

// =============== Sequencer ===============
SeqView::SeqView (GSProcessor& p)
    : proc (p), slen (p, "slen", "Steps", 50), div (p, "div", "Interval", 70), asgA (p, "assignA", "A ->", 34), asgB (p, "assignB", "B ->", 34),
      gate (p, "gate", "Gate"), swing (p, "swing", "Swing")
{
    const char* names[] = { "VELOCITY", "MARKER", "NOTE", "LENGTH", "GRAIN SIZE", "ASSIGN_A", "ASSIGN_B" };
    for (int i = 0; i < 7; ++i)
    {
        auto* b = tabs.add (new juce::TextButton (names[i]));
        b->setColour (juce::TextButton::buttonOnColourId, ui::blue);
        b->onClick = [this, i] { lane = i; for (int j = 0; j < tabs.size(); ++j) tabs[j]->setToggleState (j == lane, juce::dontSendNotification); repaint(); };
        b->setToggleState (i == lane, juce::dontSendNotification);
        addAndMakeVisible (b);
    }
    randomBtn.onClick = [this]
    {
        const int nm = (int) proc.getMarkers().size();
        for (auto& s : steps)
        {
            s.on = rng.nextFloat() < 0.55f; s.m = nm && rng.nextFloat() < 0.9f ? rng.nextInt (nm) : -1;
            const int ns[] = { -12, -5, 7, 12, 3 }; s.n = rng.nextFloat() < 0.7f ? 0 : ns[rng.nextInt (5)];
            const float ls[] = { 0.25f, 0.5f, 1, 1, 1, 2 }; s.len = ls[rng.nextInt (6)];
            const float zs[] = { 0.25f, 0.5f, 2, 3 }; s.sz = rng.nextFloat() < 0.7f ? 1 : zs[rng.nextInt (4)];
            s.v = 0.5f + rng.nextFloat() * 0.5f; s.a = rng.nextFloat() < 0.6f ? -1 : rng.nextFloat(); s.b = rng.nextFloat() < 0.6f ? -1 : rng.nextFloat();
        }
        push(); repaint();
    };
    orderBtn.onClick = [this] { const int nm = (int) proc.getMarkers().size(); for (int j = 0; j < 16; ++j) { steps[(size_t) j].on = j < nm; steps[(size_t) j].m = nm ? j % nm : -1; steps[(size_t) j].len = 1; } push(); repaint(); };
    clearBtn.onClick = [this] { steps = {}; push(); repaint(); };
    resetBtn.onClick = [this]
    {
        for (auto& s : steps)
            switch (lane) { case 0: s.v = 0.9f; break; case 1: s.m = -1; break; case 2: s.n = 0; break; case 3: s.len = 1; break; case 4: s.sz = 1; break; case 5: s.a = -1; break; default: s.b = -1; break; }
        push(); repaint();
    };
    for (auto* c : std::initializer_list<juce::Component*> { &randomBtn, &orderBtn, &clearBtn, &resetBtn, &slen, &div, &asgA, &asgB, &gate, &swing }) addAndMakeVisible (c);
    steps = proc.getSteps();
    startTimerHz (30);
}

void SeqView::timerCallback() { if (! dragging) steps = proc.getSteps(); repaint(); }

void SeqView::resized()
{
    auto r = getLocalBounds();
    auto top = r.removeFromTop (58);
    auto bl = top.removeFromLeft (330);
    randomBtn.setBounds (bl.getX(), bl.getY() + 18, 80, 24); orderBtn.setBounds (bl.getX() + 86, bl.getY() + 18, 88, 24); clearBtn.setBounds (bl.getX() + 180, bl.getY() + 18, 70, 24);
    slen.setBounds (top.removeFromLeft (120).withSizeKeepingCentre (120, 24)); top.removeFromLeft (10);
    div.setBounds (top.removeFromLeft (150).withSizeKeepingCentre (150, 24)); top.removeFromLeft (10);
    gate.setBounds (top.removeFromLeft (64)); swing.setBounds (top.removeFromLeft (64));
    r.removeFromTop (4);
    gateArea = r.removeFromTop (26);
    r.removeFromTop (8);
    auto t = r.removeFromTop (24); int x = t.getX();
    for (int i = 0; i < 7; ++i)
    {
        const int w = ui::textW (ui::mono (10.5f), tabs[i]->getButtonText()) + 16;
        tabs[i]->setBounds (x, t.getY(), w, 22); x += w + 3;
        if (i == 5) { asgA.setBounds (x, t.getY(), 200, 22); x += 206; }
        if (i == 6) { asgB.setBounds (x, t.getY(), 200, 22); x += 206; }
    }
    resetBtn.setBounds (t.getRight() - 96, t.getY(), 96, 22);
    r.removeFromTop (6);
    laneArea = r;
}

void SeqView::paint (juce::Graphics& g)
{
    const int len = juce::roundToInt (proc.getParam ("slen")); const int cur = proc.vizStep.load();
    const int nm = (int) proc.getMarkers().size();
    const float cw = (float) laneArea.getWidth() / 16.f;
    const int aA = juce::roundToInt (proc.getParam ("assignA")), aB = juce::roundToInt (proc.getParam ("assignB"));
    for (int j = 0; j < 16; ++j)
    {
        const auto& s = steps[(size_t) j]; const float alpha = j < len ? 1.f : 0.25f;
        auto gb = juce::Rectangle<float> ((float) gateArea.getX() + j * cw + 1, (float) gateArea.getY(), cw - 2, (float) gateArea.getHeight());
        g.setColour ((s.on ? ui::ink : ui::bg).withMultipliedAlpha (alpha)); g.fillRect (gb);
        g.setColour ((s.on ? ui::ink : juce::Colour (0xffbbbbbb)).withMultipliedAlpha (alpha)); g.drawRect (gb, 1.f);
        g.setColour ((s.on ? ui::bg : ui::mid).withMultipliedAlpha (alpha)); g.setFont (ui::mono (10)); g.drawText (juce::String (j + 1).paddedLeft ('0', 2), gb, juce::Justification::centred);
        if (j == cur) { g.setColour (ui::green); g.drawRect (gb.reduced (1), 2.f); }

        auto cell = juce::Rectangle<float> ((float) laneArea.getX() + j * cw + 1, (float) laneArea.getY(), cw - 2, (float) laneArea.getHeight());
        g.setColour (ui::bg); g.fillRect (cell);
        g.setColour ((j % 4 == 0 ? ui::mid : juce::Colour (0xffd0d0cc)).withMultipliedAlpha (alpha)); g.drawRect (cell, 1.f);
        const juce::Colour fill = (s.on ? ui::ink : juce::Colour (0xffbdbdb8)).withMultipliedAlpha (alpha);
        float h = 0; juce::String t; juce::Rectangle<float> bar;
        auto assignLane = [&] (float u, int asg)
        {
            const FloatDef* d = asg > 0 ? findFloat (kAssignIds[asg]) : nullptr;
            if (! d) return;
            if (u < 0)
            {
                const float y = cell.getBottom() - toNorm (*d, proc.getParam (d->id)) * cell.getHeight();
                g.setColour (ui::blue.withAlpha (0.5f * alpha)); g.fillRect (cell.getX() + 2, y - 1, cell.getWidth() - 4, 2.f); t = "-";
            }
            else { h = u; t = fmtValue (*d, fromNorm (*d, u)); }
        };
        switch (lane)
        {
            case 0: h = s.v; t = juce::String (juce::roundToInt (s.v * 100)); break;
            case 1: if (s.m >= 0 && nm) { h = (float) (s.m + 1) / (float) nm; t = "M" + juce::String (s.m + 1); } else t = "-"; break;
            case 2:
            {
                const float v = (float) s.n / 24.f, half = cell.getHeight() / 2;
                bar = v >= 0 ? juce::Rectangle<float> (cell.getX(), cell.getCentreY() - v * half, cell.getWidth(), v * half) : juce::Rectangle<float> (cell.getX(), cell.getCentreY(), cell.getWidth(), -v * half);
                g.setColour (juce::Colour (0xffbbbbbb)); g.fillRect (cell.getX(), cell.getCentreY(), cell.getWidth(), 1.f);
                t = (s.n > 0 ? "+" : "") + juce::String (s.n); break;
            }
            case 3: h = std::log2 (s.len / 0.25f) / 4; t = s.len < 1 ? juce::String (s.len, 2) : juce::String (s.len, 1); break;
            case 4: h = (std::log2 (s.sz) + 2) / 4; t = "x" + juce::String (s.sz, s.sz < 1 ? 2 : 1); break;
            case 5: assignLane (s.a, aA); break;
            default: assignLane (s.b, aB); break;
        }
        if (lane != 2) bar = cell.withTrimmedTop (cell.getHeight() * (1 - juce::jlimit (0.f, 1.f, h)));
        g.setColour (fill); g.fillRect (bar);
        g.setFont (ui::mono (9.5f));
        g.setColour (bar.getHeight() > 14 ? ui::bg : ui::ink); g.drawText (t, cell.withTrimmedBottom (2).removeFromBottom (13), juce::Justification::centred);
        if (j == cur) { g.setColour (ui::green); g.drawRect (cell.reduced (1), 2.f); }
    }
}

void SeqView::edit (juce::Point<int> p, bool first)
{
    if (first) inGates = gateArea.expanded (0, 3).contains (p);
    if (inGates)
    {
        const int j = juce::jlimit (0, 15, (int) ((float) (p.x - gateArea.getX()) / (float) gateArea.getWidth() * 16));
        if (first) togTo = ! steps[(size_t) j].on;
        auto& s = steps[(size_t) j]; s.on = togTo;
        const int nm = (int) proc.getMarkers().size(); if (s.on && s.m < 0 && nm) s.m = j % nm;
        return;
    }
    if (first && ! laneArea.contains (p)) { dragging = false; return; }
    const int j = juce::jlimit (0, 15, (int) ((float) (p.x - laneArea.getX()) / (float) laneArea.getWidth() * 16));
    const float y = juce::jlimit (0.f, 1.f, 1 - (float) (p.y - laneArea.getY()) / (float) laneArea.getHeight());
    auto& s = steps[(size_t) j]; const int nm = (int) proc.getMarkers().size();
    switch (lane)
    {
        case 0: s.v = juce::jmax (0.05f, y); break;
        case 1: s.m = (! nm || y < 0.04f) ? -1 : juce::jmin (nm - 1, (int) ((y - 0.04f) / 0.96f * (float) nm)); break;
        case 2: s.n = juce::roundToInt ((y * 2 - 1) * 24); break;
        case 3: { const float L[] = { 0.25f, 0.5f, 0.75f, 1, 1.5f, 2, 3, 4 }; s.len = L[juce::jmin (7, (int) (y * 8))]; break; }
        case 4: s.sz = std::exp2 ((float) juce::roundToInt ((y * 4 - 2) * 2) / 2.f); break;
        case 5: s.a = y < 0.04f ? -1 : (y - 0.04f) / 0.96f; break;
        default: s.b = y < 0.04f ? -1 : (y - 0.04f) / 0.96f; break;
    }
}
void SeqView::mouseDown (const juce::MouseEvent& e) { dragging = true; edit (e.getPosition(), true); push(); repaint(); }
void SeqView::mouseDrag (const juce::MouseEvent& e) { if (! dragging || ! e.mods.isLeftButtonDown()) return; edit (e.getPosition(), false); push(); repaint(); }
void SeqView::mouseUp (const juce::MouseEvent&) { if (dragging) push(); dragging = false; }

// =============== Section box ===============
void Section::paint (juce::Graphics& g)
{
    auto r = getLocalBounds();
    g.setColour (ui::bg); g.fillRect (r);
    g.setColour (ui::ink); g.drawRect (r, 1);
    g.fillRect (r.getX(), r.getY() + 24, r.getWidth(), 1);
    g.setFont (ui::mono (11, true));
    const int nw = ui::textW (ui::mono (11, true), n);
    g.setColour (ui::blue); g.drawText (n, r.getX() + 8, r.getY(), nw + 2, 24, juce::Justification::centredLeft);
    g.setColour (ui::ink); g.drawText (t, r.getX() + 8 + nw, r.getY(), r.getWidth() - 16, 24, juce::Justification::centredLeft);
}

// =============== Content (scrolling area) ===============
static const char* kInfo =
    "PLAY - The sequencer follows your DAW: press play in the host and it locks to tempo, bar position and loops. With the host stopped, RUN uses the internal tempo. "
    "Hold TEST (or the T key) to hear the position line; DRONE latches it on. Each has its own pitch knob.\n\n"
    "MIDI - Play notes from any MIDI track or controller. KEYS PLAY gives every note its own grain cloud, pitched from C3/C4 = 60 (velocity sets level). "
    "TRANSPOSE SEQ makes notes transpose the running pattern instead. Sustain pedal and HOLD latch notes. Pitch bend range and mod-wheel target are set here. "
    "Right-click any knob for MIDI LEARN, then move a controller. Every parameter can also be automated from the DAW.\n\n"
    "SAMPLE - LOAD or drop a file on the waveform. Click the waveform to move the green POSITION line. In the strip above it: click to add a numbered marker, drag to move, "
    "double-click or right-click to delete. SLICE 8/16 and DETECT HITS place markers for you.\n\n"
    "SEQUENCER - Toggle steps with the numbered buttons, then click-drag across a lane to draw. MARKER picks which marker a step plays from (- = position line). "
    "ASSIGN_A/B write any chosen parameter per step; empty steps use the knob. IN ORDER replays the markers in sequence.\n\n"
    "PITCH - Switch PITCH off to keep the sample's own pitch. DENS->PITCH turns the grain rate itself into the note for buzzy tones above ~30 grains/s.\n\n"
    "CLEAN PLAYBACK - NATURAL preset: SCAN 1x, SPRAY 0, CHAOS 0, HANN window, DENSITY = 2 / SIZE.\n\n"
    "KNOBS - Drag up/right; double-click to reset; right-click for MIDI learn. Green = driven by a sequencer lane.";

Knob* Content::K (const char* id, const char* title)
{
    auto* k = new Knob (proc, id, title); owned.add (k); knobs[id] = k; addAndMakeVisible (k); return k;
}

Content::Content (GSProcessor& p)
    : proc (p), seq (p), keyboard (p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard), winPrev (p), envPrev (p)
{
    for (auto* s : { &seqBox, &kbBox, &grains, &time, &filter, &pitch, &modfx, &delay, &reverb, &lfo1, &lfo2, &output, &info }) addAndMakeVisible (s);
    addAndMakeVisible (seq);
    keyboard.setAvailableRange (24, 108); keyboard.setLowestVisibleKey (48); keyboard.setKeyWidth (26);
    keyboard.setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId, ui::blue);
    keyboard.setColour (juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, ui::blue.withAlpha (0.25f));
    keyboard.setColour (juce::MidiKeyboardComponent::keySeparatorLineColourId, ui::ink);
    keyboard.setColour (juce::MidiKeyboardComponent::shadowColourId, juce::Colours::transparentBlack);
    addAndMakeVisible (keyboard);
    octDown.onClick = [this] { keyboard.setLowestVisibleKey (juce::jmax (24, keyboard.getLowestVisibleKey() - 12)); };
    octUp.onClick = [this] { keyboard.setLowestVisibleKey (juce::jmin (84, keyboard.getLowestVisibleKey() + 12)); };
    holdBtn.setClickingTogglesState (true); holdBtn.setColour (juce::TextButton::buttonOnColourId, ui::blue);
    holdAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, "hold", holdBtn);
    for (auto* c : std::initializer_list<juce::Component*> { &octDown, &octUp, &holdBtn }) addAndMakeVisible (c);
    auto add = [this] (juce::Component* c) { owned.add (c); addAndMakeVisible (c); return c; };
    add (new ChoiceButtons (p, "kmode", "Keys", 44))->setName ("kmode");
    add (new LabeledCombo (p, "mwDest", "Mod wheel ->", 96))->setName ("mwDest");
    K ("bendRange", "Bend range");

    // grains
    for (auto* id : { "pos", "spray", "size", "sizeJ", "dens", "chaos", "gatk", "grel", "gcurve" }) K (id, findFloat (id)->name);
    add (new ChoiceButtons (p, "dfollow", "Dens->pitch", 92))->setName ("dfollow");
    addAndMakeVisible (winPrev);
    for (const auto& w : windowShapes)
    {
        auto* b = winBtns.add (new juce::TextButton (w.name));
        b->onClick = [this, w] { proc.setParam ("gatk", w.a); proc.setParam ("grel", w.r); proc.setParam ("gcurve", w.c); };
        addAndMakeVisible (b);
    }
    for (auto* id : { "scan", "rev", "shuffle", "glitch", "width" }) K (id, findFloat (id)->name);
    add (new ChoiceButtons (p, "ftype", "Mode", 44))->setName ("ftype");
    for (auto* id : { "cut", "res", "drive", "crush" }) K (id, findFloat (id)->name);
    addAndMakeVisible (envPrev);
    K ("fenv", "Env amount"); K ("fatk", "Attack"); K ("fdec", "Decay"); K ("fsus", "Sustain"); K ("frel", "Release");
    add (new ChoiceButtons (p, "pitchOn", "Pitch", 92))->setName ("pitchOn");
    for (auto* id : { "pitch", "fine", "pspray" }) K (id, findFloat (id)->name);
    add (new ChoiceButtons (p, "pseq", "Seq notes", 92))->setName ("pseq");
    add (new ChoiceButtons (p, "pmode", "Random mode", 92))->setName ("pmode");
    add (new ChoiceButtons (p, "fxType", "Type", 40))->setName ("fxType");
    K ("fxRate", "Rate"); K ("fxDepth", "Depth"); K ("fxFb", "Feedback"); K ("fxMix", "Mix");
    add (new ChoiceButtons (p, "dmode", "Mode", 40))->setName ("dmode");
    add (new LabeledCombo (p, "dsync", "Sync", 40))->setName ("dsync");
    K ("dtime", "Time"); K ("dfb", "Feedback"); K ("dtone", "Tone"); K ("dmix", "Mix");
    K ("rsize", "Size"); K ("rdecay", "Decay"); K ("rdamp", "Damping"); K ("rhp", "High-pass"); K ("rmix", "Mix");
    K ("l1r", "Rate"); K ("l1d", "Depth"); add (new LabeledCombo (p, "l1s", "Shape", 50))->setName ("l1s"); add (new LabeledCombo (p, "l1t", "Target", 50))->setName ("l1t");
    K ("l2r", "Rate"); K ("l2d", "Depth"); add (new LabeledCombo (p, "l2s", "Shape", 50))->setName ("l2s"); add (new LabeledCombo (p, "l2t", "Target", 50))->setName ("l2t");
    K ("att", "Attack"); K ("rel", "Release"); K ("vol", "Volume");
    infoText.setMultiLine (true); infoText.setReadOnly (true); infoText.setScrollbarsShown (false); infoText.setCaretVisible (false);
    infoText.setColour (juce::TextEditor::textColourId, juce::Colour (0xff333333)); infoText.setColour (juce::TextEditor::backgroundColourId, ui::bg);
    infoText.setFont (ui::mono (10.5f)); infoText.setText (juce::String (juce::CharPointer_UTF8 (kInfo)), false);
    infoText.applyFontToAllText (ui::mono (10.5f)); infoText.applyColourToAllText (juce::Colour (0xff333333));
    addAndMakeVisible (infoText);
}

void Content::paint (juce::Graphics& g) { g.fillAll (ui::bg); }

void Content::setModulated (int a, int b)
{
    if (a == lastA && b == lastB) return;
    lastA = a; lastB = b;
    for (auto& kv : knobs)
        kv.second->setModulated ((a > 0 && kv.first == kAssignIds[a]) || (b > 0 && kv.first == kAssignIds[b]));
}

int Content::heightFor (int) { return 2000; }

void Content::resized()
{
    const int W = getWidth() - 4, gap = 12, kw = 64, kh = 78;
    auto byName = [this] (const char* n) -> juce::Component* { for (auto* c : owned) if (c->getName() == n) return c; return nullptr; };
    auto rowKnobs = [&] (std::initializer_list<const char*> ids, int x, int y) { for (auto* id : ids) { knobs[id]->setBounds (x, y, kw, kh); x += kw + 2; } return x; };
    int y = 0;
    // sequencer
    seqBox.setBounds (0, y, W, 330); seq.setBounds (seqBox.getBounds().reduced (10).withTrimmedTop (24)); y += 330 + gap;
    // keyboard / MIDI
    kbBox.setBounds (0, y, W, 150);
    {
        auto b = kbBox.getBounds().reduced (10).withTrimmedTop (22);
        auto row = b.removeFromTop (30);
        octDown.setBounds (row.getX(), row.getY() + 4, 60, 22); octUp.setBounds (row.getX() + 64, row.getY() + 4, 60, 22); holdBtn.setBounds (row.getX() + 128, row.getY() + 4, 60, 22);
        auto* km = byName ("kmode"); km->setBounds (row.getX() + 200, row.getY() + 4, 240, 22);
        byName ("mwDest")->setBounds (row.getX() + 450, row.getY() + 4, 330, 22);
        knobs["bendRange"]->setBounds (W - 90, kbBox.getY() + 26, kw, kh);
        keyboard.setBounds (b.withTrimmedTop (4).withTrimmedRight (90));
    }
    y += 150 + gap;
    // row 1: grains | time
    const int w1 = (int) ((W - gap) * 0.59f), w2 = W - gap - w1;
    const int r1h = 300;
    grains.setBounds (0, y, w1, r1h); time.setBounds (w1 + gap, y, w2, r1h);
    {
        auto b = grains.body(); int x = rowKnobs ({ "pos", "spray", "size", "sizeJ", "dens", "chaos" }, b.getX(), b.getY());
        byName ("dfollow")->setBounds (x + 10, b.getY() + 30, 200, 22);
        const int wy = b.getY() + kh + 10;
        winPrev.setBounds (b.getX(), wy, 200, 50);
        int bx = b.getX() + 210, by = wy;
        for (auto* wb : winBtns) { const int ww = ui::textW (ui::mono (10.5f), wb->getButtonText()) + 14; if (bx + ww > b.getRight()) { bx = b.getX() + 210; by += 25; } wb->setBounds (bx, by, ww, 22); bx += ww + 3; }
        rowKnobs ({ "gatk", "grel", "gcurve" }, b.getX(), wy + 60);
        auto tb = time.body(); rowKnobs ({ "scan", "rev", "shuffle", "glitch", "width" }, tb.getX(), tb.getY());
    }
    y += r1h + gap;
    // row 2: filter | pitch
    const int r2h = 250;
    filter.setBounds (0, y, w1, r2h); pitch.setBounds (w1 + gap, y, w2, r2h);
    {
        auto b = filter.body(); const int half = b.getWidth() / 2 - 6;
        byName ("ftype")->setBounds (b.getX(), b.getY(), half, 22);
        rowKnobs ({ "cut", "res", "drive", "crush" }, b.getX(), b.getY() + 30);
        envPrev.setBounds (b.getX() + half + 12, b.getY(), half, 50);
        int x = b.getX() + half + 12; for (auto* id : { "fenv", "fatk", "fdec", "fsus", "frel" }) { knobs[id]->setBounds (x, b.getY() + 58, kw, kh); x += kw + 2; }
        auto pb = pitch.body();
        byName ("pitchOn")->setBounds (pb.getX(), pb.getY(), pb.getWidth(), 22);
        rowKnobs ({ "pitch", "fine", "pspray" }, pb.getX(), pb.getY() + 28);
        byName ("pseq")->setBounds (pb.getX(), pb.getY() + 28 + kh + 6, pb.getWidth(), 22);
        byName ("pmode")->setBounds (pb.getX(), pb.getY() + 28 + kh + 34, pb.getWidth(), 48);
    }
    y += r2h + gap;
    // rows 3–4: four columns
    const int cw = (W - 3 * gap) / 4, r3h = 260, r4h = 260;
    juce::Component* r3[] = { &modfx, &delay, &reverb, &lfo1 };
    for (int i = 0; i < 4; ++i) r3[i]->setBounds (i * (cw + gap), y, cw, r3h);
    {
        auto b = modfx.body(); byName ("fxType")->setBounds (b.getX(), b.getY(), b.getWidth(), 48);
        rowKnobs ({ "fxRate", "fxDepth" }, b.getX(), b.getY() + 54); rowKnobs ({ "fxFb", "fxMix" }, b.getX(), b.getY() + 54 + kh);
        b = delay.body(); byName ("dmode")->setBounds (b.getX(), b.getY(), b.getWidth(), 22); byName ("dsync")->setBounds (b.getX(), b.getY() + 28, b.getWidth(), 22);
        rowKnobs ({ "dtime", "dfb" }, b.getX(), b.getY() + 56); rowKnobs ({ "dtone", "dmix" }, b.getX(), b.getY() + 56 + kh);
        b = reverb.body(); rowKnobs ({ "rsize", "rdecay", "rdamp" }, b.getX(), b.getY()); rowKnobs ({ "rhp", "rmix" }, b.getX(), b.getY() + kh + 4);
        b = lfo1.body(); rowKnobs ({ "l1r", "l1d" }, b.getX(), b.getY()); byName ("l1s")->setBounds (b.getX(), b.getY() + kh + 6, b.getWidth(), 22); byName ("l1t")->setBounds (b.getX(), b.getY() + kh + 34, b.getWidth(), 22);
    }
    y += r3h + gap;
    lfo2.setBounds (0, y, cw, r4h); output.setBounds (cw + gap, y, cw, r4h); info.setBounds (2 * (cw + gap), y, 2 * cw + gap, r4h);
    {
        auto b = lfo2.body(); rowKnobs ({ "l2r", "l2d" }, b.getX(), b.getY()); byName ("l2s")->setBounds (b.getX(), b.getY() + kh + 6, b.getWidth(), 22); byName ("l2t")->setBounds (b.getX(), b.getY() + kh + 34, b.getWidth(), 22);
        b = output.body(); rowKnobs ({ "att", "rel", "vol" }, b.getX(), b.getY());
        infoText.setBounds (info.body());
    }
    y += r4h + 30;
    if (getHeight() != y) setSize (getWidth(), y);
}

// =============== Editor ===============
GSEditor::GSEditor (GSProcessor& p) : AudioProcessorEditor (&p), proc (p), wave (p), content (p)
{
    setLookAndFeel (&lnf);
    runBtn.setClickingTogglesState (true); runBtn.getProperties().set ("big", true); runBtn.setColour (juce::TextButton::buttonOnColourId, ui::green);
    runAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, "run", runBtn);
    testBtn.getProperties().set ("big", true); testBtn.setColour (juce::TextButton::buttonOnColourId, ui::blue);
    testBtn.onStateChange = [this]
    {
        const bool down = testBtn.isDown();
        if (down != (proc.getParam ("test") > 0.5f)) proc.setParam ("test", down ? 1.f : 0.f);
        testBtn.setToggleState (down, juce::dontSendNotification);
    };
    droneBtn.setClickingTogglesState (true); droneBtn.getProperties().set ("big", true); droneBtn.setColour (juce::TextButton::buttonOnColourId, ui::green);
    droneAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, "drone", droneBtn);
    tpitch = std::make_unique<Knob> (p, "tpitch", "Pitch"); dpitch = std::make_unique<Knob> (p, "dpitch", "Pitch"); tempo = std::make_unique<Knob> (p, "bpm", "Tempo");

    presetBox.setTextWhenNothingSelected ("PRESETS...");
    for (size_t i = 0; i < presets().size(); ++i) presetBox.addItem (presets()[i].name, (int) i + 1);
    presetBox.onChange = [this] { const int id = presetBox.getSelectedId(); if (id > 0) proc.applyPreset (id - 1); presetBox.setSelectedId (0, juce::dontSendNotification); };
    mangleBtn.onClick = [this] { proc.mangle(); };

    loadBtn.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Load a sample", juce::File(), "*.wav;*.aif;*.aiff;*.mp3;*.flac;*.ogg;*.m4a;*.caf;*.aac");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this] (const juce::FileChooser& fc)
        {
            const auto f = fc.getResult();
            if (f.existsAsFile() && ! proc.loadSampleFile (f))
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "GRANULAR_SYSTEM", "Couldn't read that file.");
        });
    };
    demoBox.setTextWhenNothingSelected ("DEMO SOUNDS...");
    for (int i = 0; i < numDemos; ++i) demoBox.addItem (demoNames[i], i + 1);
    demoBox.onChange = [this] { if (demoBox.getSelectedId() > 0) proc.loadDemo (demoBox.getSelectedId() - 1); };
    slice8.onClick = [this] { proc.setMarkers (evenMarkers (8)); };
    slice16.onClick = [this] { proc.setMarkers (evenMarkers (16)); };
    detectBtn.onClick = [this] { proc.detectMarkers(); };
    clearBtn.onClick = [this] { proc.setMarkers ({}); auto s = proc.getSteps(); for (auto& st : s) st.m = -1; proc.setSteps (s); };
    sampleName.setFont (ui::mono (11)); markersLbl.setFont (ui::mono (10.5f)); markersLbl.setText ("MARKERS:", juce::dontSendNotification);

    for (auto* c : std::initializer_list<juce::Component*> { &runBtn, &testBtn, &droneBtn, tpitch.get(), dpitch.get(), tempo.get(), &presetBox, &mangleBtn,
                                                             &loadBtn, &demoBox, &sampleName, &markersLbl, &slice8, &slice16, &detectBtn, &clearBtn, &wave, &viewport })
        addAndMakeVisible (c);
    viewport.setViewedComponent (&content, false);
    viewport.setScrollBarsShown (true, false);
    viewport.setScrollBarThickness (10);
    setWantsKeyboardFocus (true);
    setResizable (true, true);
    setResizeLimits (1100, 700, 1800, 1400);
    setSize (1240, 900);
    startTimerHz (15);
}

GSEditor::~GSEditor() { stopTimer(); setLookAndFeel (nullptr); }

void GSEditor::timerCallback()
{
    sampleName.setText (proc.getSampleName(), juce::dontSendNotification);
    const int d = proc.getDemoIndex(); demoBox.setSelectedId (d >= 0 ? d + 1 : 0, juce::dontSendNotification);
    runBtn.setButtonText (proc.seqRunning.load() && proc.getParam ("run") < 0.5f ? "HOST" : proc.getParam ("run") > 0.5f ? "STOP" : "RUN");
    content.setModulated (proc.vizAssignA.load(), proc.vizAssignB.load());
    content.repaintPreviews();
}

bool GSEditor::keyStateChanged (bool)
{
    const bool t = juce::KeyPress::isKeyCurrentlyDown ('t') || juce::KeyPress::isKeyCurrentlyDown ('T');
    if (t != tDown) { tDown = t; proc.setParam ("test", t ? 1.f : 0.f); testBtn.setToggleState (t, juce::dontSendNotification); return true; }
    return false;
}

void GSEditor::paint (juce::Graphics& g)
{
    g.fillAll (ui::bg);
    g.setColour (ui::ink); g.setFont (ui::mono (22, true));
    g.drawText ("GRANULAR_SYSTEM", 16, 8, 400, 30, juce::Justification::centredLeft);
    auto tr = juce::Rectangle<int> (16, 42, getWidth() - 32, 84); g.drawRect (tr, 1);
    for (int x : { 130, 340, 560 }) g.fillRect (16 + x, 42, 1, 84);
    g.drawRect (16, 134, getWidth() - 32, 30, 1);
    g.fillRect (0, wave.getBottom() + 8, getWidth(), 2);
}

void GSEditor::resized()
{
    const int W = getWidth();
    runBtn.setBounds (30, 60, 100, 46);
    testBtn.setBounds (160, 60, 84, 46); tpitch->setBounds (250, 46, 64, 78);
    droneBtn.setBounds (370, 60, 84, 46); dpitch->setBounds (460, 46, 64, 78);
    tempo->setBounds (590, 46, 64, 78);
    presetBox.setBounds (W - 400, 58, 260, 24); mangleBtn.setBounds (W - 130, 58, 100, 24);
    loadBtn.setBounds (24, 138, 80, 22); demoBox.setBounds (110, 138, 160, 22); sampleName.setBounds (278, 138, 300, 22);
    clearBtn.setBounds (W - 92, 138, 70, 22); detectBtn.setBounds (W - 206, 138, 108, 22); slice16.setBounds (W - 290, 138, 78, 22); slice8.setBounds (W - 368, 138, 72, 22);
    markersLbl.setBounds (W - 450, 138, 80, 22);
    wave.setBounds (16, 168, W - 32, 210);
    viewport.setBounds (16, wave.getBottom() + 14, W - 26, getHeight() - wave.getBottom() - 14);
    content.setSize (viewport.getWidth() - viewport.getScrollBarThickness() - 4, content.getHeight() > 0 ? content.getHeight() : 1800);
    content.resized();
}
