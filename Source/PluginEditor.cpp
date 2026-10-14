#include "PluginEditor.h"

using namespace juce;

namespace vcui
{
Font uiFont (float size, bool bold) { return Font { FontOptions (size, bold ? Font::bold : Font::plain) }; }

static void handle (Graphics& g, Point<float> c, float r)
{
    g.setColour (Colours::black.withAlpha (0.25f));
    g.fillEllipse (c.x - r + 1, c.y - r + 2, r * 2, r * 2);
    g.setGradientFill (ColourGradient (Colours::white, c.x, c.y - r, Colour (0xffe4e4e0), c.x, c.y + r, false));
    g.fillEllipse (c.x - r, c.y - r, r * 2, r * 2);
    g.setColour (col::orange);
    g.drawEllipse (c.x - r + 1.5f, c.y - r + 1.5f, r * 2 - 3, r * 2 - 3, 3.0f);
}

//==============================================================================
ParamDrag::ParamDrag (RangedAudioParameter& p, bool v) : param (p), vertical (v)
{
    normToAxis = [v] (float n) { return v ? 1.0f - n : n; };
    axisToNorm = [v] (float a) { return v ? 1.0f - a : a; };
    att = std::make_unique<ParameterAttachment> (p, [this] (float val) { norm = param.convertTo0to1 (val); repaint(); });
    att->sendInitialUpdate();
    setRepaintsOnMouseActivity (false);
}
void ParamDrag::setFromNorm (float n)
{
    n = jlimit (0.0f, 1.0f, n);
    const float oldV = param.convertFrom0to1 (norm);
    const float newV = param.convertFrom0to1 (n);
    att->setValueAsPartOfGesture (newV);
    if (onUserChange) onUserChange (oldV, newV);
}
void ParamDrag::mouseDown (const MouseEvent&) { startAxis = axis(); att->beginGesture(); }
void ParamDrag::mouseDrag (const MouseEvent& e)
{
    const auto t = travel();
    const float len = jmax (20.0f, vertical ? t.getHeight() : t.getWidth());
    const float delta = (vertical ? (float) e.getDistanceFromDragStartY() : (float) e.getDistanceFromDragStartX()) / len;
    const float a = startAxis + delta * (e.mods.isShiftDown() ? 0.2f : 1.0f);
    setFromNorm (axisToNorm (jlimit (0.0f, 1.0f, a)));
}
void ParamDrag::mouseUp (const MouseEvent&) { att->endGesture(); }
void ParamDrag::mouseDoubleClick (const MouseEvent&)
{
    att->beginGesture();
    setFromNorm (param.getDefaultValue());
    att->endGesture();
}
void ParamDrag::mouseWheelMove (const MouseEvent&, const MouseWheelDetails& w)
{
    const float step = (w.deltaY > 0 ? 1.0f : -1.0f) * 0.01f;
    att->beginGesture();
    setFromNorm (axisToNorm (jlimit (0.0f, 1.0f, axis() - step)));
    att->endGesture();
}

//==============================================================================
MeterFader::MeterFader (RangedAudioParameter& p, float rangeDb, int b, bool s) : ParamDrag (p, true), range (rangeDb), bars (b), scale (s) {}
Rectangle<float> MeterFader::barArea() const
{
    const float bw = bars == 2 ? 20.0f : 14.0f;
    return Rectangle<float> (getWidth() * 0.5f - bw * 0.5f + (scale ? 10.0f : 0.0f), 14.0f, bw, (float) getHeight() - 28.0f);
}
Rectangle<float> MeterFader::travel() const { return barArea(); }
void MeterFader::paint (Graphics& g)
{
    const auto b = barArea();
    // scale (dB below full scale)
    if (scale)
    {
        g.setFont (uiFont (12.5f, false));
        g.setColour (col::label);
        for (int db = 12; db < (int) range; db += 12)
        {
            const float y = b.getY() + b.getHeight() * (float) db / range;
            g.drawText (String (db), Rectangle<float> (b.getX() - 34, y - 8, 24, 16), Justification::centredRight, false);
            g.fillRect (b.getX() - 8, y - 0.5f, 5.0f, 1.0f);
        }
    }
    // bars
    const float gap = bars == 2 ? 4.0f : 0.0f;
    const float w = (b.getWidth() - gap) / (float) bars;
    for (int i = 0; i < bars; ++i)
    {
        auto r = Rectangle<float> (b.getX() + (float) i * (w + gap), b.getY(), w, b.getHeight());
        g.setColour (col::navy);
        g.fillRoundedRectangle (r, 2.5f);
        const float frac = jlimit (0.0f, 1.0f, 1.0f + lv[i] / range);
        if (frac > 0.002f)
        {
            auto f = r.withTop (r.getBottom() - r.getHeight() * frac);
            g.setGradientFill (ColourGradient (col::orangeHi, 0, f.getY(), col::orange, 0, f.getBottom(), false));
            g.fillRoundedRectangle (f, 2.5f);
        }
    }
    // handle
    const float y = b.getY() + b.getHeight() * axis();
    g.setColour (col::orange.withAlpha (0.9f));
    g.fillRect (b.getX() - 6, y - 1.0f, b.getWidth() + 12, 2.0f);
    handle (g, { b.getCentreX(), y }, 12.0f);
    g.setColour (col::orange);
    g.fillRect (b.getCentreX() - 6.0f, y - 2.5f, 12.0f, 1.5f);
    g.fillRect (b.getCentreX() - 6.0f, y + 1.0f, 12.0f, 1.5f);
}

//==============================================================================
BoxFader::BoxFader (RangedAudioParameter& p, Mode m, const String& t) : ParamDrag (p, true), mode (m), title (t)
{
    if (mode == compMode)
    {
        normToAxis = [] (float n) { return 0.05f + 0.9f * n; };
        axisToNorm = [] (float a) { return (a - 0.05f) / 0.9f; };
    }
    else
    {
        normToAxis = [] (float n) { return 0.9f * (1.0f - n); };
        axisToNorm = [] (float a) { return 1.0f - a / 0.9f; };
    }
}
Rectangle<float> BoxFader::screenArea() const
{
    auto f = getLocalBounds().toFloat().reduced (4);
    return Rectangle<float> (f.getX() + 52, f.getY() + 44, f.getWidth() - 52 - 26, f.getHeight() - 44 - 52);
}
float BoxFader::dbToAxis (float db) const
{
    if (mode == voxMode) return jlimit (0.0f, 1.0f, db / 40.0f);
    static const float pts[][2] = { { 9, 0.0f }, { 6, 0.1f }, { 3, 0.25f }, { 0, 0.4f }, { -3, 0.55f }, { -6, 0.7f }, { -12, 0.85f }, { -24, 1.0f } };
    if (db >= pts[0][0]) return 0.0f;
    for (size_t i = 1; i < std::size (pts); ++i)
        if (db >= pts[i][0]) return pts[i - 1][1] + (db - pts[i - 1][0]) / (pts[i][0] - pts[i - 1][0]) * (pts[i][1] - pts[i - 1][1]);
    return 1.0f;
}
void BoxFader::mouseDown (const MouseEvent& e) { peakClick = (float) e.y > screenArea().getBottom() + 6; if (! peakClick) ParamDrag::mouseDown (e); }
void BoxFader::mouseDrag (const MouseEvent& e) { if (! peakClick) ParamDrag::mouseDrag (e); }
void BoxFader::mouseUp (const MouseEvent& e)
{
    if (peakClick) { if (onPeakClick) onPeakClick(); peakClick = false; return; }
    ParamDrag::mouseUp (e);
}

void BoxFader::paint (Graphics& g)
{
    auto frame = getLocalBounds().toFloat().reduced (4);
    // outer bevelled frame
    {
        Path p; p.addRoundedRectangle (frame, 34.0f);
        DropShadow (Colours::black.withAlpha (0.35f), 12, { 0, 5 }).drawForPath (g, p);
        g.setGradientFill (ColourGradient (Colour (0xfffbfbfa), 0, frame.getY(), Colour (0xffc9c7c2), 0, frame.getBottom(), false));
        g.fillPath (p);
        g.setColour (Colours::white);
        g.drawRoundedRectangle (frame.reduced (1.5f), 33.0f, 1.5f);
        g.setColour (Colour (0xffa9a7a2));
        g.drawRoundedRectangle (frame, 34.0f, 1.0f);
    }
    const auto s = screenArea();
    // title
    g.setColour (col::label);
    g.setFont (uiFont (16.5f));
    g.drawText (title, Rectangle<float> (frame.getX(), frame.getY() + 10, frame.getWidth(), 24), Justification::centred, false);
    // bezel + screen
    {
        auto bez = s.expanded (9);
        g.setGradientFill (ColourGradient (Colour (0xff0b1424), 0, bez.getY(), Colour (0xff15233a), 0, bez.getBottom(), false));
        g.fillRoundedRectangle (bez, 20.0f);
        g.setGradientFill (ColourGradient (Colour (0xff22364f), 0, s.getY(), Colour (0xff1a2a40), 0, s.getBottom(), false));
        g.fillRoundedRectangle (s, 12.0f);
    }
    {
    Graphics::ScopedSaveState ss (g);
    // grid + scale
    struct Tick { float db; const char* txt; };
    const Tick compTicks[] = { { 6, "6" }, { 3, "3" }, { 0, "0" }, { -3, "-3" }, { -6, "-6" }, { -12, "-12" } };
    const Tick voxTicks[] = { { 6, "6" }, { 12, "12" }, { 18, "18" }, { 24, "24" }, { 30, "30" }, { 36, "36" } };
    const Tick* ticks = mode == compMode ? compTicks : voxTicks;
    g.setFont (uiFont (13.5f, false));
    for (int i = 0; i < 6; ++i)
    {
        const float y = s.getY() + s.getHeight() * dbToAxis (ticks[i].db);
        g.setColour (col::grid);
        g.fillRect (s.getX() + 6, y - 0.5f, s.getWidth() - 12, 1.0f);
        g.setColour (col::label);
        g.drawText (ticks[i].txt, Rectangle<float> (frame.getX() + 6, y - 9, 30, 18), Justification::centredRight, false);
        g.fillRect (frame.getX() + 39, y - 0.5f, 5.0f, 1.0f);
    }
    // gain change fill
    {
        auto inner = s.reduced (6, 4);
        g.reduceClipRegion (s.toNearestInt());
        if (mode == compMode)
        {
            const float y0 = s.getY() + s.getHeight() * dbToAxis (0), y1 = s.getY() + s.getHeight() * dbToAxis (change);
            if (std::abs (y1 - y0) > 0.5f)
            {
                g.setColour (change > 0 ? col::blue : col::gold);
                g.fillRect (Rectangle<float>::leftTopRightBottom (inner.getX(), jmin (y0, y1), inner.getRight(), jmax (y0, y1)));
            }
        }
        else
        {
            const float y1 = s.getY() + s.getHeight() * dbToAxis (change);
            if (y1 - s.getY() > 0.5f)
            {
                g.setColour (col::red);
                g.fillRect (Rectangle<float>::leftTopRightBottom (inner.getX(), s.getY(), inner.getRight(), y1));
            }
        }
    }
    }
    // handle line + value pill
    {
        const float y = s.getY() + s.getHeight() * axis();
        g.setColour (col::orange);
        g.fillRect (frame.getX() + 30, y - 1.5f, frame.getWidth() - 44, 3.0f);
        auto pill = Rectangle<float> (92, 32).withCentre ({ s.getCentreX(), y });
        DropShadow (Colours::black.withAlpha (0.5f), 6, { 0, 2 }).drawForRectangle (g, pill.toNearestInt());
        g.setColour (Colour (0xff22364f));
        g.fillRoundedRectangle (pill, 16.0f);
        g.setColour (col::orange);
        g.drawRoundedRectangle (pill.reduced (1.5f), 14.5f, 3.0f);
        g.setColour (Colour (0xfffff3c4));
        g.drawRoundedRectangle (pill.reduced (4.5f), 11.5f, 1.0f);
        g.setColour (Colours::white);
        g.setFont (uiFont (16.0f));
        g.drawText (getText(), pill, Justification::centred, false);
    }
    // peak readout
    g.setColour (col::value);
    g.setFont (uiFont (18.0f));
    const String pk = mode == compMode ? (std::abs (peak) < 0.05f ? String ("0.0") : (peak > 0 ? "+" : "") + String (peak, 1))
                                       : (peak < 0.05f ? String ("0.0") : "-" + String (peak, 1));
    g.drawText (pk, Rectangle<float> (frame.getX(), s.getBottom() + 14, frame.getWidth(), 26), Justification::centred, false);
}

//==============================================================================
HSlider::HSlider (RangedAudioParameter& p, const String& l) : ParamDrag (p, false), label (l) {}
Rectangle<float> HSlider::travel() const { return Rectangle<float> (92, 0, (float) getWidth() - 92 - 72, (float) getHeight()); }
void HSlider::paint (Graphics& g)
{
    const auto t = travel();
    const float cy = getHeight() * 0.5f;
    g.setColour (col::label);
    g.setFont (uiFont (15.5f));
    g.drawText (label, Rectangle<float> (0, 0, 84, (float) getHeight()), Justification::centredRight, false);
    g.setColour (Colour (0xff9d9b96));
    g.fillRoundedRectangle (t.getX(), cy - 1.5f, t.getWidth(), 3.0f, 1.5f);
    g.setColour (Colours::white.withAlpha (0.8f));
    g.fillRect (t.getX(), cy + 1.5f, t.getWidth(), 1.0f);
    handle (g, { t.getX() + t.getWidth() * axis(), cy }, 12.0f);
    g.setColour (col::value);
    g.setFont (uiFont (17.0f));
    g.drawText (getText(), Rectangle<float> ((float) getWidth() - 66, 0, 66, (float) getHeight()), Justification::centredLeft, false);
}

//==============================================================================
Knob::Knob (RangedAudioParameter& p, const String& l, const String& mn, const String& mx) : ParamDrag (p, true), label (l), minT (mn), maxT (mx) {}
void Knob::paint (Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour (col::label);
    g.setFont (uiFont (15.0f));
    g.drawText (label + "  " + getText(), b.removeFromTop (20), Justification::centred, false);
    const float r = 22.0f;
    const Point<float> c { b.getCentreX(), b.getY() + 6 + r + 4 };
    const float sa = MathConstants<float>::pi * 1.25f, ea = MathConstants<float>::pi * 2.75f;
    g.setColour (col::label.withAlpha (0.8f));
    for (int i = 0; i <= 4; ++i)
    {
        const float a = sa + (ea - sa) * (float) i / 4.0f;
        g.drawLine ({ c.getPointOnCircumference (r + 4, a), c.getPointOnCircumference (r + 9, a) }, 1.4f);
    }
    g.setColour (Colours::black.withAlpha (0.3f));
    g.fillEllipse (c.x - r + 1, c.y - r + 3, r * 2, r * 2);
    g.setGradientFill (ColourGradient (Colours::white, c.x - r * 0.4f, c.y - r, Colour (0xffcfcdc8), c.x + r * 0.5f, c.y + r, false));
    g.fillEllipse (c.x - r, c.y - r, r * 2, r * 2);
    g.setColour (Colour (0xffa9a7a2));
    g.drawEllipse (c.x - r, c.y - r, r * 2, r * 2, 1.0f);
    const float a = sa + (ea - sa) * getNorm();
    const auto d = c.getPointOnCircumference (r * 0.62f, a);
    g.setColour (Colour (0xff2a2a2a));
    g.fillEllipse (d.x - 3.5f, d.y - 3.5f, 7, 7);
    g.setColour (col::label);
    g.setFont (uiFont (12.5f, false));
    g.drawText (minT, Rectangle<float> (c.x - r - 30, c.y + r - 4, 30, 16), Justification::centredRight, false);
    g.drawText (maxT, Rectangle<float> (c.x + r, c.y + r - 4, 34, 16), Justification::centredLeft, false);
}

//==============================================================================
LookAndFeel::LookAndFeel()
{
    setColour (PopupMenu::backgroundColourId, Colour (0xfff4f3f0));
    setColour (PopupMenu::textColourId, col::value);
    setColour (PopupMenu::highlightedBackgroundColourId, col::orange);
    setColour (PopupMenu::highlightedTextColourId, Colours::white);
    setColour (ComboBox::textColourId, col::value);
}
void LookAndFeel::drawButtonBackground (Graphics& g, Button& b, const Colour&, bool hi, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (2.0f);
    const float rad = r.getHeight() * 0.5f;
    const bool on = b.getToggleState();
    g.setColour (Colours::black.withAlpha (0.22f));
    g.fillRoundedRectangle (r.translated (0, down ? 0.5f : 1.5f), rad);
    g.setGradientFill (ColourGradient (Colour (hi ? 0xffffffff : 0xfffbfbfa), 0, r.getY(), Colour (down ? 0xffd4d2cc : 0xffdedcd7), 0, r.getBottom(), false));
    g.fillRoundedRectangle (r, rad);
    g.setColour (on ? col::orange : Colour (0xff8e8c88));
    g.drawRoundedRectangle (r.reduced (0.75f), rad, on ? 2.2f : 1.2f);
}
void LookAndFeel::drawButtonText (Graphics& g, TextButton& b, bool, bool)
{
    g.setFont (uiFont (jmin (15.0f, b.getHeight() * 0.5f)));
    g.setColour (b.getToggleState() ? Colour (0xff222222) : Colour (0xff6c6a66));
    g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (6, 2), Justification::centred, 1, 0.8f);
}
void LookAndFeel::drawComboBox (Graphics& g, int w, int h, bool, int, int, int, int, ComboBox&)
{
    auto r = Rectangle<float> (0, 0, (float) w, (float) h).reduced (1.0f);
    g.setGradientFill (ColourGradient (Colours::white, 0, 0, Colour (0xffe2e0db), 0, (float) h, false));
    g.fillRoundedRectangle (r, 8.0f);
    g.setColour (Colour (0xff9a9893));
    g.drawRoundedRectangle (r, 8.0f, 1.2f);
    Path a; const float ax = (float) w - 18.0f, ay = (float) h * 0.5f;
    a.addTriangle (ax - 5, ay - 2.5f, ax + 5, ay - 2.5f, ax, ay + 3.5f);
    g.setColour (col::orange); g.fillPath (a);
}
Font LookAndFeel::getComboBoxFont (ComboBox&) { return uiFont (15.5f); }
Font LookAndFeel::getPopupMenuFont() { return uiFont (15.0f, false); }
void LookAndFeel::positionComboBoxText (ComboBox& b, Label& l) { l.setBounds (12, 1, b.getWidth() - 36, b.getHeight() - 2); l.setFont (getComboBoxFont (b)); }

//==============================================================================
ChoicePill::ChoicePill (RangedAudioParameter& p) : param (p)
{
    setToggleState (true, dontSendNotification);
    att = std::make_unique<ParameterAttachment> (p, [this] (float v)
    {
        const int i = roundToInt (v);
        setButtonText (param.getAllValueStrings()[i]);
        repaint();
    });
    att->sendInitialUpdate();
    onClick = [this]
    {
        const int n = param.getAllValueStrings().size();
        const int cur = roundToInt (param.convertFrom0to1 (param.getValue()));
        att->setValueAsCompleteGesture ((float) ((cur + 1) % n));
    };
}
OnPill::OnPill (APVTS& s, const String& id, const String& text) : TextButton (text)
{
    setClickingTogglesState (true);
    att = std::make_unique<APVTS::ButtonAttachment> (s, id, *this);
}

//==============================================================================
namespace
{
    const Rectangle<int> compP { 16, 66, 486, 618 };
    const Rectangle<int> voxP  { 518, 66, 486, 618 };
    constexpr int colW = 80, col1 = 16, col3 = 390, meterTop = 112, meterH = 268, boxX = 108, boxY = 54, boxW = 270, boxH = 376;
    String fmt (float db) { return db <= -99.0f ? String ("-inf") : String (db, 1); }
}

MainPanel::MainPanel (VocalCompProcessor& p)
    : proc (p),
      cThresh (*p.apvts.getParameter ("c_thresh"), 60.0f, 2, false),
      cGain (*p.apvts.getParameter ("c_gain"), 60.0f, 2, false),
      vGate (*p.apvts.getParameter ("v_gate"), 80.0f, 1, true),
      vGain (*p.apvts.getParameter ("v_gain"), 36.0f, 2, false),
      cRatio (*p.apvts.getParameter ("c_ratio"), BoxFader::compMode, "Ratio"),
      vComp (*p.apvts.getParameter ("v_comp"), BoxFader::voxMode, "Comp"),
      cAttack (*p.apvts.getParameter ("c_attack"), "Attack"),
      cRelease (*p.apvts.getParameter ("c_release"), "Release"),
      cMix (*p.apvts.getParameter ("c_mix"), "Mix", "0", "100"),
      cTrim (*p.apvts.getParameter ("c_trim"), "Trim", "-18", "+18"),
      cArc (*p.apvts.getParameter ("c_arc")), cBehav (*p.apvts.getParameter ("c_behav")), cChar (*p.apvts.getParameter ("c_char")),
      order (*p.apvts.getParameter ("order")),
      cOn (p.apvts, "c_on", "ON"), vOn (p.apvts, "v_on", "ON"), vLink (p.apvts, "v_link", "LINK")
{
    setLookAndFeel (&lnf);
    setOpaque (true);
    for (Component* c : std::initializer_list<Component*> { &cThresh, &cGain, &vGate, &vGain, &cRatio, &vComp, &cAttack, &cRelease, &cMix, &cTrim,
                                                            &cArc, &cBehav, &cChar, &order, &cOn, &vOn, &vLink, &presetBox })
        addAndMakeVisible (c);

    // LINK: lowering COMP also lowers the GAIN ceiling by the make-up amount, so the level stays even
    vComp.onUserChange = [this] (float oldV, float newV)
    {
        if (proc.value ("v_link") < 0.5f) return;
        if (auto* gp = proc.apvts.getParameter ("v_gain"))
        {
            const float g = jlimit (-36.0f, 0.0f, proc.value ("v_gain") + 0.75f * (newV - oldV));
            gp->beginChangeGesture(); gp->setValueNotifyingHost (gp->convertTo0to1 (g)); gp->endChangeGesture();
        }
    };
    cRatio.onPeakClick = [this] { cPeak = 0; cPeakT = 0; };
    vComp.onPeakClick = [this] { hVPeak.reset(); };
    cOn.onClick = [this] { repaint(); };
    vOn.onClick = [this] { repaint(); };

    for (int i = 0; i < proc.getNumPrograms(); ++i) presetBox.addItem (proc.getProgramName (i), i + 1);
    presetBox.setSelectedId (proc.getCurrentProgram() + 1, dontSendNotification);
    presetBox.onChange = [this] { const int id = presetBox.getSelectedId(); if (id > 0 && id - 1 != proc.getCurrentProgram()) proc.setCurrentProgram (id - 1); };
    startTimerHz (30);
}

MainPanel::~MainPanel() { stopTimer(); setLookAndFeel (nullptr); }

void MainPanel::resized()
{
    renderTexture();
    presetBox.setBounds (W - 16 - 300, 14, 300, 36);
    for (auto [panel, left, box, right] : { std::tuple<Rectangle<int>, Component*, Component*, Component*> { compP, &cThresh, &cRatio, &cGain },
                                            std::tuple<Rectangle<int>, Component*, Component*, Component*> { voxP, &vGate, &vComp, &vGain } })
    {
        left->setBounds (panel.getX() + col1, panel.getY() + meterTop - 14, colW, meterH + 28);
        right->setBounds (panel.getX() + col3, panel.getY() + meterTop - 14, colW, meterH + 28);
        box->setBounds (panel.getX() + boxX, panel.getY() + boxY, boxW, boxH);
    }
    cOn.setBounds (compP.getRight() - 78, compP.getY() + 12, 64, 30);
    vOn.setBounds (voxP.getRight() - 78, voxP.getY() + 12, 64, 30);

    const int bx = compP.getX(), by = compP.getY();
    cAttack.setBounds  (bx + 10, by + 448, 340, 34);
    cRelease.setBounds (bx + 10, by + 490, 340, 34);
    cArc.setBounds   (bx + 30,  by + 552, 100, 34);
    cBehav.setBounds (bx + 136, by + 552, 100, 34);
    cChar.setBounds  (bx + 242, by + 552, 100, 34);
    cMix.setBounds  (bx + 366, by + 444, 114, 86);
    cTrim.setBounds (bx + 366, by + 528, 114, 86);

    const int vx = voxP.getX(), vy = voxP.getY();
    order.setBounds (vx + 30, vy + 476, 190, 34);
    vLink.setBounds (vx + 236, vy + 476, 110, 34);
}

void MainPanel::renderTexture()
{
    texture = Image (Image::RGB, W, H, true);
    Graphics g (texture);
    g.setGradientFill (ColourGradient (Colour (0xffd7d5d0), 0, 0, Colour (0xffc4c2bd), 0, (float) H, false));
    g.fillAll();
    // header strip
    g.setGradientFill (ColourGradient (Colour (0xfff7f6f3), 0, 0, Colour (0xffe3e1dc), 0, 60, false));
    g.fillRect (0, 0, W, 60);
    g.setColour (Colour (0xffb3b1ac));
    g.fillRect (0, 60, W, 1);
}

void MainPanel::drawPanel (Graphics& g, Rectangle<int> r, const String& title, const String& sub, Colour titleCol, bool on)
{
    auto rf = r.toFloat();
    DropShadow (Colours::black.withAlpha (0.28f), 14, { 0, 4 }).drawForRectangle (g, r);
    g.setGradientFill (ColourGradient (col::panelTop, 0, rf.getY(), col::panelBot, 0, rf.getBottom(), false));
    g.fillRoundedRectangle (rf, 10.0f);
    g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.55f), rf.getX(), rf.getY(), Colours::transparentWhite, rf.getX() + rf.getWidth() * 0.6f, rf.getY() + rf.getHeight() * 0.5f, false));
    g.fillRoundedRectangle (rf, 10.0f);
    g.setColour (Colours::white);
    g.drawRoundedRectangle (rf.reduced (1.0f), 10.0f, 1.0f);
    g.setColour (Colour (0xffa6a49f));
    g.drawRoundedRectangle (rf, 10.0f, 1.0f);
    // title
    g.setColour (on ? titleCol : titleCol.withSaturation (0.0f).withAlpha (0.6f));
    g.setFont (uiFont (26.0f));
    g.drawText (title, Rectangle<float> (rf.getX(), rf.getY() + 8, rf.getWidth(), 32), Justification::centred, false);
    g.setColour (col::label);
    g.setFont (uiFont (11.5f));
    g.drawText (sub, Rectangle<float> (rf.getX(), rf.getY() + 37, rf.getWidth(), 14), Justification::centred, false);
    // separator above the lower section
    g.setColour (Colour (0xffbebcb7));
    g.fillRect (rf.getX() + 16, rf.getY() + 438, rf.getWidth() - 32, 1.0f);
    g.setColour (Colours::white.withAlpha (0.8f));
    g.fillRect (rf.getX() + 16, rf.getY() + 439, rf.getWidth() - 32, 1.0f);
}

void MainPanel::column (Graphics& g, int x, int w, int y0, const String& top, const String& topValue, const String& bottom, const String& bottomValues)
{
    g.setColour (col::label);
    g.setFont (uiFont (15.5f));
    g.drawText (top, Rectangle<int> (x - 10, y0 + 56, w + 20, 20), Justification::centred, false);
    g.setColour (col::value);
    g.setFont (uiFont (19.0f));
    g.drawText (topValue, Rectangle<int> (x - 10, y0 + 76, w + 20, 22), Justification::centred, false);
    g.setColour (col::label);
    g.setFont (uiFont (15.5f));
    g.drawText (bottom, Rectangle<int> (x - 10, y0 + meterTop + meterH + 18, w + 20, 20), Justification::centred, false);
    g.setColour (col::value);
    g.setFont (uiFont (14.5f));
    g.drawText (bottomValues, Rectangle<int> (x - 16, y0 + meterTop + meterH + 38, w + 32, 18), Justification::centred, false);
}

void MainPanel::paint (Graphics& g)
{
    g.drawImage (texture, getLocalBounds().toFloat());
    // header
    g.setColour (Colour (0xff2b2d31));
    g.setFont (uiFont (25.0f));
    g.drawText ("VOCAL COMPRESSOR", Rectangle<float> (22, 10, 400, 30), Justification::centredLeft, false);
    g.setColour (col::compTitle);
    g.setFont (uiFont (12.5f));
    g.drawText ("ROHIT JI   |   COMP + VOX  IN ONE", Rectangle<float> (24, 38, 400, 16), Justification::centredLeft, false);
    g.setColour (col::label);
    g.setFont (uiFont (14.0f));
    g.drawText ("PRESET", Rectangle<float> ((float) W - 16 - 300 - 80, 14, 72, 36), Justification::centredRight, false);

    const bool cOnS = proc.value ("c_on") > 0.5f, vOnS = proc.value ("v_on") > 0.5f;
    drawPanel (g, compP, "COMP", "COMPRESSOR  /  UPWARD EXPANDER  /  MODELS", col::compTitle, cOnS);
    drawPanel (g, voxP, "VOX", "GATE  /  UPWARD GAIN  /  CEILING LIMITER", col::voxTitle, vOnS);

    column (g, compP.getX() + col1, colW, compP.getY(), "Thresh", cThresh.getText(), "Input", readout[0]);
    column (g, compP.getX() + col3, colW, compP.getY(), "Gain", cGain.getText(), "Output", readout[1]);
    column (g, voxP.getX() + col1, colW, voxP.getY(), "Gate", vGate.getText(), "Energy", readout[2]);
    column (g, voxP.getX() + col3, colW, voxP.getY(), "Gain", vGain.getText(), "Output", readout[3]);

    // COMP limiter activity light (yellow = limiting, red = 6 dB or more)
    auto led = [&] (Rectangle<float> r, float lim)
    {
        const Colour c = lim > 6.0f ? Colour (0xffe8402a) : (lim > 0.1f ? Colour (0xfff6d21e) : Colour (0xff2a2c30));
        if (lim > 0.1f) { g.setColour (c.withAlpha (0.35f)); g.fillRoundedRectangle (r.expanded (3), 6); }
        g.setColour (c); g.fillRoundedRectangle (r, 4);
        g.setColour (Colours::black.withAlpha (0.6f)); g.drawRoundedRectangle (r, 4, 1.0f);
    };
    led (Rectangle<float> ((float) compP.getX() + col3 + colW * 0.5f + 24, (float) compP.getY() + 61, 16, 10), proc.meters.cLimit.load());
    led (Rectangle<float> ((float) voxP.getX() + col3 + colW * 0.5f + 24, (float) voxP.getY() + 61, 16, 10), proc.meters.vLimit.load());

    // COMP lower labels
    g.setColour (col::label);
    g.setFont (uiFont (12.0f));
    g.drawText ("RELEASE MODE      BEHAVIOR      CHARACTER", Rectangle<float> ((float) compP.getX() + 30, (float) compP.getY() + 532, 312, 16), Justification::centred, false);

    // VOX lower section
    {
        const float x = (float) voxP.getX(), y = (float) voxP.getY();
        g.setColour (col::label);
        g.setFont (uiFont (12.0f));
        g.drawText ("SIGNAL ORDER", Rectangle<float> (x + 30, y + 456, 190, 16), Justification::centred, false);
        g.drawText ("COMP + GAIN", Rectangle<float> (x + 236, y + 456, 110, 16), Justification::centred, false);
        g.setFont (uiFont (13.5f, false));
        g.setColour (col::value);
        const float ty = y + 526;
        g.drawText (String::fromUTF8 ("▲ Lower COMP = more compression AND more level (upward gain)"), Rectangle<float> (x + 30, ty, 440, 20), Justification::centredLeft, false);
        g.drawText (String::fromUTF8 ("▲ GAIN = output ceiling, the limiter never lets it clip"), Rectangle<float> (x + 30, ty + 22, 440, 20), Justification::centredLeft, false);
        g.drawText (String::fromUTF8 ("▲ GATE: raise it above the noise to clean the pauses"), Rectangle<float> (x + 30, ty + 44, 440, 20), Justification::centredLeft, false);
        g.setColour (col::label);
        g.drawText ("LINK keeps the level even while you set COMP", Rectangle<float> (x + 30, ty + 66, 440, 20), Justification::centredLeft, false);
    }
    g.setColour (col::label);
    g.setFont (uiFont (11.0f));
    g.drawText ("v1.0  |  ROHIT JI", Rectangle<float> ((float) W - 216, (float) H - 15, 200, 13), Justification::centredRight, false);
}

void MainPanel::mouseUp (const MouseEvent&)
{
    for (auto* h : { &hIn[0], &hIn[1], &hOut[0], &hOut[1], &hVOut[0], &hVOut[1], &hEnergy }) h->reset();
}

void MainPanel::timerCallback()
{
    auto& m = proc.meters;
    cThresh.setLevels (m.cInL.load(), m.cInR.load());
    cGain.setLevels (m.cOutL.load(), m.cOutR.load());
    vGate.setLevels (m.vEnergy.load(), m.vEnergy.load());
    vGain.setLevels (m.vOutL.load(), m.vOutR.load());

    const float ch = m.cChange.load();
    cChangeSm += (ch - cChangeSm) * 0.5f;
    if (std::abs (ch) >= std::abs (cPeak) || ++cPeakT > 90) { cPeak = ch; cPeakT = 0; }
    cRatio.setChange (cChangeSm, cPeak);
    const float vg = m.vGr.load();
    vGrSm += (vg - vGrSm) * 0.5f;
    vComp.setChange (vGrSm, hVPeak.update (vg, 0.0f));

    const String r0 = fmt (hIn[0].update (m.cInL.load())) + "   " + fmt (hIn[1].update (m.cInR.load()));
    const String r1 = fmt (hOut[0].update (m.cOutL.load())) + "   " + fmt (hOut[1].update (m.cOutR.load()));
    const String r2 = fmt (hEnergy.update (m.vEnergy.load()));
    const String r3 = fmt (hVOut[0].update (m.vOutL.load())) + "   " + fmt (hVOut[1].update (m.vOutR.load()));
    const String now[4] = { r0, r1, r2, r3 };
    for (int i = 0; i < 4; ++i)
        if (now[i] != readout[i])
        {
            readout[i] = now[i];
            const auto& pnl = i < 2 ? compP : voxP;
            const int x = pnl.getX() + (i % 2 == 0 ? col1 : col3);
            repaint (x - 18, pnl.getY() + meterTop + meterH + 36, colW + 36, 22);
        }
    // values above the faders + limiter lights
    const String tops[4] = { cThresh.getText(), cGain.getText(), vGate.getText(), vGain.getText() };
    for (int i = 0; i < 4; ++i)
        if (tops[i] != readout[4 + i])
        {
            readout[4 + i] = tops[i];
            const auto& pnl = i < 2 ? compP : voxP;
            const int x = pnl.getX() + (i % 2 == 0 ? col1 : col3);
            repaint (x - 12, pnl.getY() + 74, colW + 24, 26);
        }
    const float lims[2] = { m.cLimit.load(), m.vLimit.load() };
    for (int i = 0; i < 2; ++i)
    {
        const int state = lims[i] > 6.0f ? 2 : (lims[i] > 0.1f ? 1 : 0);
        if ((int) shown[i] != state) { shown[i] = (float) state; const auto& pnl = i == 0 ? compP : voxP; repaint (pnl.getX() + col3 + colW / 2 + 18, pnl.getY() + 54, 30, 24); }
    }
    if (presetBox.getSelectedId() != proc.getCurrentProgram() + 1) presetBox.setSelectedId (proc.getCurrentProgram() + 1, dontSendNotification);
}
} // namespace vcui

//==============================================================================
VocalCompEditor::VocalCompEditor (VocalCompProcessor& p) : AudioProcessorEditor (&p), panel (p)
{
    addAndMakeVisible (panel);
    setResizable (true, true);
    setResizeLimits (vcui::MainPanel::W * 3 / 5, vcui::MainPanel::H * 3 / 5, vcui::MainPanel::W * 2, vcui::MainPanel::H * 2);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) vcui::MainPanel::W / (double) vcui::MainPanel::H);
    setSize (vcui::MainPanel::W, vcui::MainPanel::H);
}
void VocalCompEditor::resized()
{
    panel.setBounds (0, 0, vcui::MainPanel::W, vcui::MainPanel::H);
    panel.setTransform (AffineTransform::scale ((float) getWidth() / (float) vcui::MainPanel::W));
}
