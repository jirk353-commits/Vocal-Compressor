#include "PluginEditor.h"

using namespace juce;

namespace vcui
{
Font uiFont (float size, bool bold, float kerning)
{
    Font f { FontOptions (size, bold ? Font::bold : Font::plain) };
    return kerning > 0.0f ? f.withExtraKerningFactor (kerning) : f;
}

void embossed (Graphics& g, const String& t, Rectangle<float> r, Font f, Colour c, Justification j)
{
    g.setFont (f);
    g.setColour (Colours::black.withAlpha (0.75f));
    g.drawText (t, r.translated (0, -1.0f), j, false);
    g.setColour (Colours::white.withAlpha (0.16f));
    g.drawText (t, r.translated (0, 1.2f), j, false);
    g.setColour (c);
    g.drawText (t, r, j, false);
}

static void drawLcd (Graphics& g, Rectangle<float> lcd, const String& text, Colour c, float fontSize)
{
    g.setColour (col::lcd);
    g.fillRoundedRectangle (lcd, 5.0f);
    g.setColour (Colours::black);
    g.drawRoundedRectangle (lcd, 5.0f, 1.2f);
    g.setColour (Colours::white.withAlpha (0.12f));
    g.drawLine (lcd.getX() + 4, lcd.getBottom() + 0.8f, lcd.getRight() - 4, lcd.getBottom() + 0.8f, 1.0f);
    g.setColour (c);
    g.setFont (uiFont (fontSize));
    g.drawFittedText (text, lcd.toNearestInt(), Justification::centred, 1, 0.8f);
}

//==============================================================================
LookAndFeel::LookAndFeel()
{
    setColour (PopupMenu::backgroundColourId, Colour (0xff1c1f24));
    setColour (PopupMenu::textColourId, col::text);
    setColour (PopupMenu::highlightedBackgroundColourId, col::gold);
    setColour (PopupMenu::highlightedTextColourId, Colours::black);
    setColour (ComboBox::textColourId, col::gold);
}

void LookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos, float sa, float ea, Slider& s)
{
    const auto accent = s.findColour (Slider::rotarySliderFillColourId);
    const float arcFrom = (float) s.getProperties().getWithDefault ("arcFrom", 0.0f);
    const auto area = Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    const float size = jmin (area.getWidth(), area.getHeight());
    const auto c = area.getCentre();
    const float ringR = size * 0.5f - 6.0f, bodyR = size * 0.36f;
    const float angle = sa + pos * (ea - sa);
    const float ringW = jmax (4.0f, size * 0.045f);

    Path track; track.addCentredArc (c.x, c.y, ringR, ringR, 0, sa, ea, true);
    g.setColour (Colour (0xff07080a));
    g.strokePath (track, PathStrokeType (ringW + 3, PathStrokeType::curved, PathStrokeType::rounded));
    g.setColour (Colour (0xff2c3138));
    g.strokePath (track, PathStrokeType (ringW, PathStrokeType::curved, PathStrokeType::rounded));
    {
        const float from = sa + arcFrom * (ea - sa);
        Path arc; arc.addCentredArc (c.x, c.y, ringR, ringR, 0, jmin (from, angle), jmax (from, angle), true);
        g.setColour (accent.withAlpha (0.25f));
        g.strokePath (arc, PathStrokeType (ringW + 7, PathStrokeType::curved, PathStrokeType::rounded));
        g.setColour (accent);
        g.strokePath (arc, PathStrokeType (ringW, PathStrokeType::curved, PathStrokeType::rounded));
    }
    for (int i = 0; i <= 10; ++i)
    {
        const float a = sa + (float) i / 10.0f * (ea - sa);
        g.setColour (col::textDim.withAlpha (i % 5 == 0 ? 0.95f : 0.5f));
        const auto pt = c.getPointOnCircumference (ringR + ringW + 4.0f, a);
        const float d = i % 5 == 0 ? 3.4f : 2.2f;
        g.fillEllipse (pt.x - d * 0.5f, pt.y - d * 0.5f, d, d);
    }
    Path body; body.addEllipse (c.x - bodyR, c.y - bodyR, bodyR * 2, bodyR * 2);
    DropShadow (Colours::black.withAlpha (0.9f), (int) (size * 0.12f), { 0, (int) (size * 0.05f) }).drawForPath (g, body);
    g.setGradientFill (ColourGradient (Colour (0xffc9ced5), c.x - bodyR, c.y - bodyR, Colour (0xff4c525a), c.x + bodyR, c.y + bodyR, false));
    g.fillPath (body);
    for (int i = 0; i < 56; ++i)
    {
        const float a = (float) i / 56.0f * MathConstants<float>::twoPi;
        g.setColour (i % 2 ? Colours::black.withAlpha (0.22f) : Colours::white.withAlpha (0.16f));
        g.drawLine ({ c.getPointOnCircumference (bodyR * 0.8f, a), c.getPointOnCircumference (bodyR - 1, a) }, 1.0f);
    }
    const float innerR = bodyR * 0.78f;
    g.setGradientFill (ColourGradient (Colour (0xff3a3f47), c.x, c.y - innerR, Colour (0xff131518), c.x, c.y + innerR, false));
    g.fillEllipse (c.x - innerR, c.y - innerR, innerR * 2, innerR * 2);
    g.setColour (Colours::white.withAlpha (0.10f));
    g.drawEllipse (c.x - innerR, c.y - innerR, innerR * 2, innerR * 2, 1.0f);
    const auto p1 = c.getPointOnCircumference (innerR * 0.3f, angle), p2 = c.getPointOnCircumference (innerR - 3, angle);
    g.setColour (accent.withAlpha (0.35f));
    g.drawLine ({ p1, p2 }, jmax (5.0f, size * 0.06f));
    g.setColour (accent.brighter (0.4f));
    g.drawLine ({ p1, p2 }, jmax (2.6f, size * 0.03f));
}

void LookAndFeel::drawButtonBackground (Graphics& g, Button& b, const Colour&, bool hi, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.5f);
    DropShadow (Colours::black.withAlpha (0.7f), 4, { 0, down ? 0 : 2 }).drawForRectangle (g, r.toNearestInt());
    g.setGradientFill (ColourGradient (Colour (down ? 0xff23272d : (hi ? 0xff545a63 : 0xff4a5059)), 0, r.getY(), Colour (0xff1c1f24), 0, r.getBottom(), false));
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (Colours::white.withAlpha (hi ? 0.32f : 0.18f));
    g.drawLine (r.getX() + 3, r.getY() + 1, r.getRight() - 3, r.getY() + 1, 1.0f);
    g.setColour (Colours::black);
    g.drawRoundedRectangle (r, 5.0f, 1.0f);
    const auto led = b.findColour (TextButton::buttonOnColourId);
    const float d = jmin (9.0f, r.getHeight() * 0.3f);
    auto l = Rectangle<float> (r.getX() + 9, r.getCentreY() - d * 0.5f, d, d);
    if (b.getToggleState()) { g.setColour (led.withAlpha (0.35f)); g.fillEllipse (l.expanded (4)); g.setColour (led); }
    else g.setColour (Colour (0xff101214));
    g.fillEllipse (l);
    g.setColour (Colours::black.withAlpha (0.6f));
    g.drawEllipse (l, 0.8f);
}

void LookAndFeel::drawButtonText (Graphics& g, TextButton& b, bool, bool)
{
    auto r = b.getLocalBounds().reduced (2).withTrimmedLeft (18);
    g.setFont (uiFont (jmin (15.0f, b.getHeight() * 0.46f), true, 0.05f));
    g.setColour (b.getToggleState() ? col::text : col::textDim.withAlpha (0.8f));
    g.drawFittedText (b.getButtonText(), r, Justification::centred, 1, 0.75f);
}

void LookAndFeel::drawComboBox (Graphics& g, int w, int h, bool, int, int, int, int, ComboBox&)
{
    auto r = Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
    g.setColour (col::lcd);
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (Colours::black);
    g.drawRoundedRectangle (r, 5.0f, 1.5f);
    g.setColour (Colours::white.withAlpha (0.12f));
    g.drawLine (r.getX() + 4, r.getBottom() + 0.5f, r.getRight() - 4, r.getBottom() + 0.5f, 1.0f);
    Path a; const float ax = (float) w - 18.0f, ay = (float) h * 0.5f;
    a.addTriangle (ax - 5, ay - 2.5f, ax + 5, ay - 2.5f, ax, ay + 3.5f);
    g.setColour (col::gold); g.fillPath (a);
}
Font LookAndFeel::getComboBoxFont (ComboBox&) { return uiFont (15.5f); }
Font LookAndFeel::getPopupMenuFont() { return uiFont (15.0f, false); }
void LookAndFeel::positionComboBoxText (ComboBox& b, Label& l) { l.setBounds (12, 1, b.getWidth() - 36, b.getHeight() - 2); l.setFont (getComboBoxFont (b)); }

//==============================================================================
Knob::Knob (APVTS& s, const String& id, const String& t, Colour accent, bool b, float arcFrom, const String& suf) : title (t), suffix (suf), big (b)
{
    slider.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
    slider.setRotaryParameters (MathConstants<float>::pi * 1.25f, MathConstants<float>::pi * 2.75f, true);
    slider.setMouseDragSensitivity (big ? 280 : 220);
    slider.setColour (Slider::rotarySliderFillColourId, accent);
    slider.getProperties().set ("arcFrom", arcFrom);
    addAndMakeVisible (slider);
    att = std::make_unique<APVTS::SliderAttachment> (s, id, slider);
    if (auto* p = s.getParameter (id)) slider.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
    slider.onValueChange = [this]
    {
        if (dynamicAccent) slider.setColour (Slider::rotarySliderFillColourId, dynamicAccent());
        repaint();
    };
}
void Knob::resized()
{
    auto r = getLocalBounds();
    r.removeFromTop (26);
    r.removeFromBottom (34);
    const int sz = jmin (r.getWidth(), r.getHeight());
    slider.setBounds (r.withSizeKeepingCentre (sz, sz));
    if (dynamicAccent) slider.setColour (Slider::rotarySliderFillColourId, dynamicAccent());
}
void Knob::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    embossed (g, title, r.removeFromTop (26), uiFont (big ? 16.5f : 14.5f, true, 0.12f), col::text, Justification::centred);
    auto lcd = r.removeFromBottom (34).withSizeKeepingCentre (jmin ((float) getWidth() - 8, big ? 112.0f : 96.0f), 26.0f);
    drawLcd (g, lcd, slider.getTextFromValue (slider.getValue()) + suffix, slider.findColour (Slider::rotarySliderFillColourId).brighter (0.35f), big ? 16.0f : 14.5f);
}

//==============================================================================
ChoiceButtons::ChoiceButtons (RangedAudioParameter& param, const StringArray& labels, Colour led)
{
    for (int i = 0; i < labels.size(); ++i)
    {
        auto* b = buttons.add (new TextButton (labels[i]));
        b->setColour (TextButton::buttonOnColourId, led);
        b->onClick = [this, i] { if (att) att->setValueAsCompleteGesture ((float) i); };
        addAndMakeVisible (b);
    }
    att = std::make_unique<ParameterAttachment> (param, [this] (float v) { update (roundToInt (v)); });
    att->sendInitialUpdate();
}
void ChoiceButtons::update (int index) { for (int i = 0; i < buttons.size(); ++i) buttons[i]->setToggleState (i == index, dontSendNotification); }
void ChoiceButtons::resized()
{
    const int n = buttons.size();
    const float w = (float) getWidth() / (float) n;
    for (int i = 0; i < n; ++i)
    {
        const int a = roundToInt ((float) i * w), b = roundToInt ((float) (i + 1) * w);
        buttons[i]->setBounds (a + 2, 0, b - a - 4, getHeight());
    }
}

Toggle::Toggle (APVTS& s, const String& id, const String& text, Colour led) : TextButton (text)
{
    setClickingTogglesState (true);
    setColour (TextButton::buttonOnColourId, led);
    att = std::make_unique<APVTS::ButtonAttachment> (s, id, *this);
}

//==============================================================================
BarMeter::BarMeter (Kind k, const String& n, Colour c) : kind (k), name (n), colour (c) {}
float BarMeter::toPos (float v) const      // 0 = bottom, 1 = top
{
    switch (kind)
    {
        case level:      return jlimit (0.0f, 1.0f, (v + 48.0f) / 54.0f);
        case gainChange: return jlimit (0.0f, 1.0f, (v + 24.0f) / 36.0f);
        default:         return jlimit (0.0f, 1.0f, 1.0f - v / 24.0f);
    }
}
void BarMeter::setValue (float v)
{
    float smooth = kind == level ? (v > val ? v : val + (v - val) * 0.18f) : val + (v - val) * 0.4f;
    const float old = val;
    val = smooth;
    const float mag = kind == gainChange ? std::abs (v) : v;
    if (kind == level) { if (v >= peakLevel || ++peakT > 75) { peakLevel = v; peakT = 0; } }
    else if (mag >= std::abs (peak) || ++peakT > 75) { peak = v; peakT = 0; }
    if (std::abs (old - val) > 0.02f || peakT == 0) repaint();
}
void BarMeter::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    auto label = r.removeFromBottom (22);
    auto num = r.removeFromBottom (30);
    g.setFont (uiFont (13.5f, true, 0.06f));
    g.setColour (Colours::black.withAlpha (0.75f));
    g.drawFittedText (name, label.translated (0, -1).toNearestInt(), Justification::centred, 1, 0.7f);
    g.setColour (col::text);
    g.drawFittedText (name, label.toNearestInt(), Justification::centred, 1, 0.7f);

    String txt;
    if (kind == level) txt = peakLevel < -60 ? String ("-inf") : String (peakLevel, 1);
    else if (kind == gainChange) txt = std::abs (peak) < 0.05f ? String ("0.0") : (peak > 0 ? "+" : "") + String (peak, 1);
    else txt = peak < 0.05f ? String ("0.0") : "-" + String (peak, 1);
    const Colour numCol = kind == gainChange ? (peak > 0.05f ? col::blue : col::gold) : colour;
    drawLcd (g, num.reduced (3, 3), txt, numCol.brighter (0.25f), 14.0f);

    r.removeFromBottom (6);
    if (showLimiter)
    {
        auto lr = r.removeFromTop (20);
        const Colour c = limiter > 6.0f ? col::red : (limiter > 0.1f ? Colour (0xfff6d21e) : Colour (0xff15171a));
        auto led = Rectangle<float> (9, 9).withCentre ({ lr.getX() + 12, lr.getCentreY() });
        if (limiter > 0.1f) { g.setColour (c.withAlpha (0.35f)); g.fillEllipse (led.expanded (4)); }
        g.setColour (c); g.fillEllipse (led);
        g.setColour (Colours::black); g.drawEllipse (led, 0.8f);
        g.setColour (col::textDim); g.setFont (uiFont (11.5f, true, 0.06f));
        g.drawText ("LIMIT", lr.withTrimmedLeft (20), Justification::centredLeft, false);
        r.removeFromTop (4);
    }
    g.setColour (Colour (0xff050608));
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (Colours::white.withAlpha (0.08f));
    g.drawRoundedRectangle (r.expanded (1), 5.0f, 1.0f);
    auto in = r.reduced (5);
    const int segs = jlimit (16, 32, (int) (in.getHeight() / 11.0f));
    const float sh = in.getHeight() / (float) segs;
    const float vPos = toPos (val);
    const float zero = kind == gainChange ? toPos (0) : (kind == level ? 0.0f : 1.0f);
    const float hold = kind == level ? toPos (peakLevel) : -1.0f;
    for (int s = 0; s < segs; ++s)
    {
        const float pLo = (float) s / (float) segs, pHi = (float) (s + 1) / (float) segs, pMid = 0.5f * (pLo + pHi);
        auto seg = Rectangle<float> (in.getX(), in.getBottom() - pHi * in.getHeight() + 1.0f, in.getWidth(), sh - 2.0f);
        Colour sc = colour;
        bool lit = false;
        if (kind == level)
        {
            const float db = -48.0f + pMid * 54.0f;
            sc = db > 0 ? col::red : (db > -9 ? col::gold : col::mint);
            lit = pMid <= vPos || std::abs (pMid - hold) < 0.5f / segs;
        }
        else if (kind == gainChange)
        {
            sc = pMid > zero ? col::blue : col::gold;
            lit = (pMid > zero && pMid <= vPos) || (pMid < zero && pMid >= vPos);
        }
        else lit = pMid >= vPos;
        g.setColour (lit ? sc : sc.withAlpha (0.09f));
        g.fillRoundedRectangle (seg, 1.5f);
    }
    if (kind == gainChange)
    {
        const float y = in.getBottom() - zero * in.getHeight();
        g.setColour (col::text);
        g.fillRect (r.getX() - 3, y - 1.0f, r.getWidth() + 6, 2.0f);
    }
}

//==============================================================================
namespace
{
    const Rectangle<int> compW { 24, 92, 690, 546 };
    const Rectangle<int> voxW  { 726, 92, 370, 546 };
}

MainPanel::MainPanel (VocalCompProcessor& p)
    : proc (p),
      cThresh (p.apvts, "c_thresh", "THRESHOLD", col::gold, true, 1.0f, " dB"),
      cRatio (p.apvts, "c_ratio", "RATIO", col::gold, true, 0.3889f, " : 1"),
      cGain (p.apvts, "c_gain", "GAIN", col::gold, false, 0.5f, " dB"),
      cAttack (p.apvts, "c_attack", "ATTACK", col::gold, false, 0.0f, " ms"),
      cRelease (p.apvts, "c_release", "RELEASE", col::gold, false, 0.0f, " ms"),
      cMix (p.apvts, "c_mix", "MIX", col::gold, false, 0.0f, " %"),
      cTrim (p.apvts, "c_trim", "TRIM", Colour (0xffe8e8e8), false, 0.5f, " dB"),
      vGate (p.apvts, "v_gate", "GATE", col::mint, false, 0.0f, ""),
      vComp (p.apvts, "v_comp", "COMP", col::red, true, 1.0f, " dB"),
      vGain (p.apvts, "v_gain", "GAIN", Colour (0xffe8e8e8), false, 1.0f, " dB"),
      cArc (*p.apvts.getParameter ("c_arc"), { "MANUAL", "ARC" }, col::gold),
      cBehav (*p.apvts.getParameter ("c_behav"), { "ELECTRO", "OPTO" }, col::gold),
      cChar (*p.apvts.getParameter ("c_char"), { "WARM", "SMOOTH" }, col::gold),
      order (*p.apvts.getParameter ("order"), { "COMP > VOX", "VOX > COMP" }, col::mint),
      cOn (p.apvts, "c_on", "ON", col::gold), vOn (p.apvts, "v_on", "ON", col::red), vLink (p.apvts, "v_link", "LINK", col::red),
      cIn (BarMeter::level, "IN", col::mint), cChange (BarMeter::gainChange, "GAIN +/-", col::gold), cOut (BarMeter::level, "OUT", col::mint),
      vIn (BarMeter::level, "ENERGY", col::mint), vGr (BarMeter::reduction, "TOTAL GR", col::red), vOut (BarMeter::level, "OUT", col::mint)
{
    setLookAndFeel (&lnf);
    setOpaque (true);
    for (Component* c : std::initializer_list<Component*> { &cThresh, &cRatio, &cGain, &cAttack, &cRelease, &cMix, &cTrim, &vGate, &vComp, &vGain,
                                                            &cArc, &cBehav, &cChar, &order, &cOn, &vOn, &vLink, &cIn, &cChange, &cOut, &vIn, &vGr, &vOut, &presetBox })
        addAndMakeVisible (c);
    cOut.showLimiter = vOut.showLimiter = true;

    // ratio below 1 = upward gain (blue), above 1 = compression (gold)
    cRatio.dynamicAccent = [this] { const float r = (float) cRatio.slider.getValue(); return r < 0.995f ? col::blue : (r > 1.005f ? col::gold : col::textDim); };
    cRatio.resized();

    // LINK: lowering COMP also lowers the GAIN ceiling by the make-up amount, so the level stays even
    lastVoxComp = (float) vComp.slider.getValue();
    vComp.slider.onValueChange = [this]
    {
        const float now = (float) vComp.slider.getValue();
        if (proc.value ("v_link") > 0.5f && vComp.slider.isMouseButtonDown())
            if (auto* gp = proc.apvts.getParameter ("v_gain"))
            {
                const float g = jlimit (-36.0f, 0.0f, proc.value ("v_gain") + 0.75f * (now - lastVoxComp));
                gp->beginChangeGesture(); gp->setValueNotifyingHost (gp->convertTo0to1 (g)); gp->endChangeGesture();
            }
        lastVoxComp = now;
        vComp.repaint();
    };
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
    presetBox.setBounds (W - 24 - 280, 24, 280, 34);
    order.setBounds (470, 38, 260, 32);

    const int cx = compW.getX(), cy = compW.getY();
    cOn.setBounds (compW.getRight() - 96, cy + 8, 80, 28);
    cThresh.setBounds (cx + 14,  cy + 46, 160, 232);
    cRatio.setBounds  (cx + 174, cy + 46, 160, 232);
    cGain.setBounds   (cx + 334, cy + 66, 130, 196);
    cAttack.setBounds  (cx + 14,  cy + 288, 112, 172);
    cRelease.setBounds (cx + 126, cy + 288, 112, 172);
    cMix.setBounds     (cx + 238, cy + 288, 112, 172);
    cTrim.setBounds    (cx + 350, cy + 288, 112, 172);
    {
        const int mx = cx + 480, mw = 64;
        cIn.setBounds     (mx,            cy + 46, mw, 420);
        cChange.setBounds (mx + mw + 4,   cy + 46, mw, 420);
        cOut.setBounds    (mx + 2 * (mw + 4), cy + 46, mw, 420);
    }
    {
        const int gw = (compW.getWidth() - 28) / 3;
        cArc.setBounds   (cx + 14,          cy + 496, gw - 12, 36);
        cBehav.setBounds (cx + 14 + gw,     cy + 496, gw - 12, 36);
        cChar.setBounds  (cx + 14 + 2 * gw, cy + 496, gw - 12, 36);
    }

    const int vx = voxW.getX(), vy = voxW.getY();
    vOn.setBounds (voxW.getRight() - 96, vy + 8, 80, 28);
    vGate.setBounds (vx + 10,  vy + 66, 110, 196);
    vComp.setBounds (vx + 120, vy + 46, 130, 232);
    vGain.setBounds (vx + 250, vy + 66, 110, 196);
    vIn.setBounds  (vx + 30,  vy + 290, 88, 196);
    vGr.setBounds  (vx + 141, vy + 290, 88, 196);
    vOut.setBounds (vx + 252, vy + 290, 88, 196);
    vLink.setBounds (vx + 16, vy + 498, 100, 34);
}

void MainPanel::renderTexture()
{
    texture = Image (Image::RGB, W, H, true);
    Graphics g (texture);
    g.setGradientFill (ColourGradient (col::plateTop, 0, 0, col::plateBot, 0, (float) H, false));
    g.fillAll();
    Random rnd (2026);
    for (int y = 0; y < H; ++y)
        for (int k = 0; k < 6; ++k)
        {
            const float x0 = rnd.nextFloat() * W, len = 40.0f + rnd.nextFloat() * 260.0f;
            g.setColour ((rnd.nextBool() ? Colours::white : Colours::black).withAlpha (0.025f + rnd.nextFloat() * 0.05f));
            g.fillRect (x0, (float) y, len, 1.0f);
        }
    g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.09f), W * 0.3f, 0, Colours::transparentWhite, W * 0.6f, H * 0.6f, false));
    g.fillAll();
    // screws
    for (auto pt : { Point<float> (12, 12), Point<float> ((float) W - 12, 12), Point<float> (12, (float) H - 12), Point<float> ((float) W - 12, (float) H - 12) })
    {
        g.setGradientFill (ColourGradient (Colour (0xffb9bec5), pt.x - 5, pt.y - 5, Colour (0xff3a3f46), pt.x + 5, pt.y + 5, false));
        g.fillEllipse (pt.x - 6, pt.y - 6, 12, 12);
        g.setColour (Colours::black.withAlpha (0.7f));
        g.drawLine (pt.x - 4, pt.y + 2, pt.x + 4, pt.y - 2, 1.6f);
    }
}

void MainPanel::drawWell (Graphics& g, Rectangle<int> r, const String& title, Colour accent)
{
    auto rf = r.toFloat();
    g.setColour (Colours::white.withAlpha (0.10f));
    g.drawRoundedRectangle (rf.translated (0, 1.5f), 9.0f, 1.5f);
    g.setGradientFill (ColourGradient (Colour (0xff121518), 0, rf.getY(), Colour (0xff1d2126), 0, rf.getBottom(), false));
    g.fillRoundedRectangle (rf, 9.0f);
    g.setGradientFill (ColourGradient (Colours::black.withAlpha (0.55f), 0, rf.getY(), Colours::transparentBlack, 0, rf.getY() + 18, false));
    g.fillRoundedRectangle (rf, 9.0f);
    g.setColour (Colour (0xff0a0c0e));
    g.drawRoundedRectangle (rf.reduced (0.5f), 9.0f, 1.5f);
    g.setColour (accent);
    g.fillRoundedRectangle (rf.getX() + 16, rf.getY() + 13, 6, 22, 3);
    embossed (g, title, Rectangle<float> (rf.getX() + 30, rf.getY() + 8, rf.getWidth() - 140, 32), uiFont (17.5f, true, 0.16f), col::text, Justification::centredLeft);
}

void MainPanel::paint (Graphics& g)
{
    g.drawImage (texture, getLocalBounds().toFloat());
    embossed (g, "VOCAL COMPRESSOR", Rectangle<float> (30, 12, 440, 40), uiFont (32.0f, true, 0.12f), Colour (0xfffff4dc), Justification::centredLeft);
    embossed (g, "ROHIT JI   |   COMP + VOX DYNAMICS", Rectangle<float> (32, 52, 440, 20), uiFont (13.5f, true, 0.2f), col::textDim, Justification::centredLeft);
    embossed (g, "SIGNAL ORDER", Rectangle<float> (470, 14, 260, 20), uiFont (12.5f, true, 0.2f), col::textDim, Justification::centred);
    embossed (g, "PRESET", Rectangle<float> ((float) W - 24 - 280 - 84, 24, 76, 34), uiFont (14.0f, true, 0.2f), col::textDim, Justification::centredRight);

    drawWell (g, compW, "COMP  -  COMPRESSOR / UPWARD", col::gold);
    drawWell (g, voxW, "VOX  -  LEVELER", col::red);

    // COMP group labels
    {
        const int gw = (compW.getWidth() - 28) / 3;
        const char* lbl[] = { "RELEASE MODE", "BEHAVIOR", "CHARACTER" };
        for (int i = 0; i < 3; ++i)
            embossed (g, lbl[i], Rectangle<float> ((float) (compW.getX() + 14 + i * gw), (float) compW.getY() + 474, (float) gw - 12, 18), uiFont (12.5f, true, 0.16f), col::textDim, Justification::centred);
        g.setColour (Colours::white.withAlpha (0.07f));
        g.fillRect ((float) compW.getX() + 14, (float) compW.getY() + 466, (float) compW.getWidth() - 28, 1.0f);
        g.fillRect ((float) compW.getX() + 472, (float) compW.getY() + 50, 1.0f, 410.0f);
    }
    // hints
    {
        g.setFont (uiFont (12.5f, false));
        g.setColour (col::textDim);
        const float x = (float) voxW.getX() + 126, y = (float) voxW.getY() + 498;
        g.drawText ("Lower COMP = more level (upward gain)", Rectangle<float> (x, y - 2, 236, 18), Justification::centredLeft, false);
        g.drawText ("GAIN = output ceiling, never clips", Rectangle<float> (x, y + 18, 236, 18), Justification::centredLeft, false);
    }
    if (proc.value ("c_on") < 0.5f) { g.setColour (Colours::black.withAlpha (0.35f)); g.fillRoundedRectangle (compW.toFloat().withTrimmedTop (44).reduced (2), 8); }
    if (proc.value ("v_on") < 0.5f) { g.setColour (Colours::black.withAlpha (0.35f)); g.fillRoundedRectangle (voxW.toFloat().withTrimmedTop (44).reduced (2), 8); }
    embossed (g, "v1.0  |  ROHIT JI", Rectangle<float> ((float) W - 330, (float) H - 18, 300, 14), uiFont (11.0f, true, 0.2f), col::textDim, Justification::centredRight);
}

void MainPanel::timerCallback()
{
    auto& m = proc.meters;
    cIn.setValue (jmax (m.cInL.load(), m.cInR.load()));
    cChange.setValue (m.cChange.load());
    cOut.setValue (jmax (m.cOutL.load(), m.cOutR.load()));
    cOut.limiter = m.cLimit.load();
    vIn.setValue (m.vEnergy.load());
    vGr.setValue (m.vGr.load());
    vOut.setValue (jmax (m.vOutL.load(), m.vOutR.load()));
    vOut.limiter = m.vLimit.load();
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
