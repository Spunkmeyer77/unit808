#include "PluginEditor.h"
#include "BinaryData.h"

using namespace unit808ui;

namespace
{
const float kPi = juce::MathConstants<float>::pi;
const juce::Colour kOrange { 0xfff47c39 }, kCream { 0xffd8d5c7 }, kPanel { 0xff303332 };

juce::PopupMenu::Options menuAt (juce::Component* c) { return juce::PopupMenu::Options().withTargetComponent (c); }

juce::Font font (float size, bool bold = false) { return juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain)); }

// angle convention of the artwork: degrees, 0 = straight up, clockwise positive
float angleOf (juce::Point<float> c, juce::Point<float> p) { return std::atan2 (p.x - c.x, c.y - p.y) * 180.0f / kPi; }
} // namespace

namespace unit808ui
{
juce::Image image (const Art& a)
{
    if (a.res == nullptr) return {};
    int size = 0;
    if (auto* data = BinaryData::getNamedResource (a.res, size))
        return juce::ImageCache::getFromMemory (data, size);
    jassertfalse;
    return {};
}

const Ctrl* find (const char* id)
{
    for (auto& c : kCtrls) if (juce::String (c.id) == id) return &c;
    jassertfalse;
    return nullptr;
}

// draws a piece of artwork at its place, inside a component at `origin` on the panel
static void drawAt (juce::Graphics& g, const juce::Image& img, const Art& a, juce::Point<float> origin, const juce::AffineTransform& t = {})
{
    if (! img.isValid()) return;
    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
    const auto place = juce::AffineTransform::scale (a.w / (float) img.getWidth(), a.h / (float) img.getHeight())
                           .translated (a.x - origin.x, a.y - origin.y);
    g.drawImageTransformed (img, place.followedBy (t));
}

static juce::Rectangle<int> boundsOf (const Art& a) { return juce::Rectangle<float> (a.x, a.y, a.w, a.h).getSmallestIntegerContainer(); }

// ---------------------------------------------------------------------------------------------- Knob
Knob::Knob (const Ctrl& c, float minAngle, float maxAngle)
    : juce::Slider (juce::Slider::RotaryVerticalDrag, juce::Slider::NoTextBox), ctrl (c), minA (minAngle), maxA (maxAngle)
{
    base = image (c.a[0]); cap = image (c.a[1]);
    setBounds (boundsOf (c.a[0]));
    setMouseDragSensitivity (c.r > 50 ? 320 : 220);
    setVelocityBasedMode (false);
    setScrollWheelEnabled (false);   // knobs are turned by dragging only
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void Knob::paint (juce::Graphics& g)
{
    const juce::Point<float> o ((float) getX(), (float) getY());
    drawAt (g, base, ctrl.a[0], o);
    const float p = (float) valueToProportionOfLength (getValue());
    const float a = (minA + p * (maxA - minA)) * kPi / 180.0f;
    drawAt (g, cap, ctrl.a[1], o, juce::AffineTransform::rotation (a, ctrl.cx - o.x, ctrl.cy - o.y));
}

bool Knob::hitTest (int x, int y)
{
    return juce::Point<float> ((float) x + (float) getX(), (float) y + (float) getY()).getDistanceFrom ({ ctrl.cx, ctrl.cy }) < ctrl.r * 1.45f;
}

// ---------------------------------------------------------------------------------------------- Selector
Selector::Selector (const Ctrl& c, juce::RangedAudioParameter& p)
    : ctrl (c), param (p),
      att (p, [this] (float v) { index = juce::jlimit (0, ctrl.nDetents - 1, juce::roundToInt (v)); repaint(); })
{
    base = image (c.a[0]); cap = image (c.a[1]);
    // the clickable area includes the printed labels around the knob
    const float R = c.r * 2.6f;
    setBounds (juce::Rectangle<float> (R * 2, R * 2).withCentre ({ c.cx, c.cy }).getSmallestIntegerContainer());
    att.sendInitialUpdate();
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void Selector::paint (juce::Graphics& g)
{
    const juce::Point<float> o ((float) getX(), (float) getY());
    // mute / solo on the printed labels: dimmed = muted, red glow = soloed
    if (labelState)
        for (int i = 0; i < (int) labels.size(); ++i)
        {
            const int st = labelState (i);
            if (st == 0) continue;
            auto r = labels[(size_t) i] - o;
            if (st == 1)
            {
                g.setColour (juce::Colour (0xff1c1e1e).withAlpha (0.72f));
                g.fillRoundedRectangle (r, 2.0f);
            }
            else
            {
                const juce::Colour red (0xffff2a10);
                g.setGradientFill (juce::ColourGradient (red.withAlpha (0.55f), r.getCentre(), red.withAlpha (0.0f), r.getCentre().translated (r.getWidth() * 0.9f, 0), true));
                g.fillEllipse (r.expanded (r.getWidth() * 0.45f, r.getHeight() * 0.9f));
                g.setColour (red.withAlpha (0.35f));
                g.fillRoundedRectangle (r, 2.0f);
                g.setColour (red.brighter (0.2f));
                g.drawRoundedRectangle (r.expanded (0.5f), 2.0f, 1.6f);
            }
        }
    drawAt (g, base, ctrl.a[0], o);
    const float a = ctrl.detents[index] * kPi / 180.0f;
    drawAt (g, cap, ctrl.a[1], o, juce::AffineTransform::rotation (a, ctrl.cx - o.x, ctrl.cy - o.y));
}

bool Selector::hitTest (int x, int y)
{
    return getLocalBounds().toFloat().getCentre().getDistanceFrom ({ (float) x, (float) y }) < (float) getWidth() * 0.5f;
}

void Selector::pickAt (juce::Point<float> p)
{
    const juce::Point<float> c (ctrl.cx - (float) getX(), ctrl.cy - (float) getY());
    if (p.getDistanceFrom (c) < ctrl.r * 0.25f) return;
    const float a = angleOf (c, p);
    int best = index;
    float bestD = 1e9f;
    for (int i = 0; i < ctrl.nDetents; ++i)
    {
        float d = std::abs (a - ctrl.detents[i]);
        d = juce::jmin (d, 360.0f - d);
        if (d < bestD) { bestD = d; best = i; }
    }
    if (best != index) att.setValueAsCompleteGesture ((float) best);
}

void Selector::mouseDown (const juce::MouseEvent& e)
{
    labelClick = false;
    const auto m = e.mods;
    if (onLabel && (m.isAltDown() || m.isShiftDown() || m.isPopupMenu()))
    {
        const auto p = e.position + getPosition().toFloat();
        for (int i = 0; i < (int) labels.size(); ++i)
            if (labels[(size_t) i].expanded (3.0f).contains (p)) { labelClick = true; onLabel (i, m); return; }
        if (m.isPopupMenu()) return;
    }
    pickAt (e.position);
}

void ChannelLabel::paint (juce::Graphics& g)
{
    // the selected instrument's name window gets a thin orange frame
    if (isSelected && isSelected (instrument))
    {
        g.setColour (juce::Colour (0xfff47c39));
        g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (1.0f), 4.0f, 2.2f);
    }
}

// ---------------------------------------------------------------------------------------------- Switch
Switch::Switch (const Ctrl& c, juce::RangedAudioParameter& p, bool horiz, std::vector<int> imageForValue)
    : ctrl (c), att (p, [this] (float v) { value = juce::jlimit (0, (int) imageFor.size() - 1, juce::roundToInt (v)); repaint(); }),
      param (p), imageFor (std::move (imageForValue)), horizontal (horiz)
{
    for (int i = 0; i < 4; ++i) imgs.push_back (image (c.a[i]));
    auto r = juce::Rectangle<float> (c.x, c.y, c.w, c.h);
    for (int i = 0; i < 4; ++i) if (c.a[i].res) r = r.getUnion ({ c.a[i].x, c.a[i].y, c.a[i].w, c.a[i].h });
    setBounds (r.getSmallestIntegerContainer());
    att.sendInitialUpdate();
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void Switch::paint (juce::Graphics& g)
{
    const int img = imageFor[(size_t) value];
    drawAt (g, imgs[(size_t) img], ctrl.a[img], { (float) getX(), (float) getY() });
}

void Switch::setValue (int v)
{
    v = juce::jlimit (0, (int) imageFor.size() - 1, v);
    if (v != value) att.setValueAsCompleteGesture ((float) v);
}

void Switch::mouseDown (const juce::MouseEvent& e)
{
    const int n = (int) imageFor.size();
    if (n == 2) { setValue (1 - value); return; }
    mouseDrag (e);
}

void Switch::mouseDrag (const juce::MouseEvent& e)
{
    const int n = (int) imageFor.size();
    if (n == 2) return;
    // positions run left to right (levers) or top to bottom (PRE-SCALE)
    const float t = horizontal ? e.position.x / (float) getWidth() : e.position.y / (float) getHeight();
    const int pos = juce::jlimit (0, n - 1, (int) (t * (float) n));
    for (int v = 0; v < n; ++v) if (imageFor[(size_t) v] == pos) setValue (v);
}

// ---------------------------------------------------------------------------------------------- Key
Key::Key (const Ctrl& c, const Ctrl* l) : ctrl (c), led (l)
{
    up = image (c.a[0]); dn = image (c.a[1]);
    if (led)
    {
        ledOff = image (led->a[0]); ledOnImg = image (led->a[1]);
        // the flam colour: the lit red LED with its red and blue swapped (same lens, same glow)
        ledBlue = ledOnImg.createCopy();
        juce::Image::BitmapData bd (ledBlue, juce::Image::BitmapData::readWrite);
        for (int y = 0; y < bd.height; ++y)
            for (int x = 0; x < bd.width; ++x)
            {
                const auto c = bd.getPixelColour (x, y);
                bd.setPixelColour (x, y, juce::Colour (c.getBlue(), (juce::uint8) juce::jmin (255, c.getGreen() + c.getRed() / 4), c.getRed(), c.getAlpha()));
            }
    }
    setBounds (boundsOf (c.a[0]).getUnion (boundsOf (c.a[1])));
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void Key::paint (juce::Graphics& g)
{
    const juce::Point<float> o ((float) getX(), (float) getY());
    const bool d = down || (heldDown && heldDown());
    drawAt (g, d ? dn : up, ctrl.a[d ? 1 : 0], o);
    if (led)
    {
        const int st = ledState ? ledState() : 0;
        // the LED sits on the key cap and moves down with it
        drawAt (g, st == 2 ? ledBlue : (st == 1 ? ledOnImg : ledOff), led->a[st > 0 ? 1 : 0], o, juce::AffineTransform::translation (0, d ? 2.0f : 0.0f));
    }
}

// ---------------------------------------------------------------------------------------------- Trimmers (as Unit 606)
TrimmerLookAndFeel::TrimmerLookAndFeel()
{
    setColour (juce::BubbleComponent::backgroundColourId, juce::Colour (0xff1a1712));
    setColour (juce::TooltipWindow::textColourId, juce::Colours::white);
}

void TrimmerLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&)
{
    const auto area = juce::Rectangle<int> (x, y, w, h).toFloat();
    const auto c = area.getCentre();
    const float s = juce::jmin (area.getWidth(), area.getHeight());
    auto body = juce::Rectangle<float> (s * 0.82f, s * 0.82f).withCentre (c);
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillRoundedRectangle (body.translated (s * 0.05f, s * 0.07f), 2.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff4f7fc2), body.getX(), body.getY(), juce::Colour (0xff1d3b6b), body.getRight(), body.getBottom(), false));
    g.fillRoundedRectangle (body, 2.0f);
    g.setColour (juce::Colour (0xff0f1d33));
    g.drawRoundedRectangle (body, 2.0f, 1.0f);
    g.setColour (juce::Colours::white.withAlpha (0.35f));
    g.drawLine (body.getX() + 2, body.getY() + 1.2f, body.getRight() - 2, body.getY() + 1.2f, 1.0f);
    const float r = s * 0.3f;
    juce::ColourGradient rotor (juce::Colour (0xfffbf0c8), c.x - r * 0.5f, c.y - r * 0.6f, juce::Colour (0xff8a6a26), c.x + r, c.y + r, true);
    rotor.addColour (0.45, juce::Colour (0xffd8b866));
    g.setGradientFill (rotor);
    g.fillEllipse (juce::Rectangle<float> (r * 2, r * 2).withCentre (c));
    g.setColour (juce::Colour (0xff4a3810));
    g.drawEllipse (juce::Rectangle<float> (r * 2, r * 2).withCentre (c), 1.0f);
    const float a = start + pos * (end - start);
    juce::Path slot;
    slot.addRoundedRectangle (-r * 0.85f, -r * 0.13f, r * 1.7f, r * 0.26f, r * 0.1f);
    g.setColour (juce::Colour (0xff2a1e08));
    g.fillPath (slot, juce::AffineTransform::rotation (a - juce::MathConstants<float>::halfPi).translated (c));
    g.setColour (juce::Colours::white.withAlpha (0.85f));
    g.fillEllipse (juce::Rectangle<float> (r * 0.3f, r * 0.3f).withCentre (c.getPointOnCircumference (r * 0.62f, a)));
}

void TrimmerLookAndFeel::drawBubble (juce::Graphics& g, juce::BubbleComponent&, const juce::Point<float>&, const juce::Rectangle<float>& body)
{
    g.setColour (juce::Colour (0xff1a1712));
    g.fillRoundedRectangle (body, 3.0f);
    g.setColour (juce::Colours::black);
    g.drawRoundedRectangle (body.reduced (0.5f), 3.0f, 1.0f);
}

juce::Font TrimmerLookAndFeel::getSliderPopupFont (juce::Slider&)
{
    return juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 16.0f, juce::Font::bold));
}

// ---------------------------------------------------------------------------------------------- Menus (double size)
MenuLookAndFeel::MenuLookAndFeel()
{
    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff2b2e2e));
    setColour (juce::PopupMenu::textColourId, kCream);
    setColour (juce::PopupMenu::headerTextColourId, kOrange);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, kOrange);
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colour (0xff1c1e1e));
}

juce::Font MenuLookAndFeel::getPopupMenuFont() { return juce::Font (juce::FontOptions (30.0f)); }

void MenuLookAndFeel::getIdealPopupMenuItemSize (const juce::String& text, bool isSeparator, int standardMenuItemHeight, int& idealWidth, int& idealHeight)
{
    LookAndFeel_V4::getIdealPopupMenuItemSize (text, isSeparator, standardMenuItemHeight, idealWidth, idealHeight);
    if (! isSeparator) idealHeight = juce::jmax (idealHeight, 48);
    idealWidth += 40;
}

void MenuLookAndFeel::drawPopupMenuSectionHeader (juce::Graphics& g, const juce::Rectangle<int>& area, const juce::String& name)
{
    g.setFont (juce::Font (juce::FontOptions (30.0f, juce::Font::bold)));
    g.setColour (findColour (juce::PopupMenu::headerTextColourId));
    g.drawFittedText (name, area.getX() + 24, area.getY(), area.getWidth() - 32, area.getHeight(), juce::Justification::bottomLeft, 1);
}

// ---------------------------------------------------------------------------------------------- Board inside
BoardView::BoardView (Unit808Processor& p)
{
    board = image (kBoard);
    setOpaque (true);
    for (auto& pad : kPads)
    {
        int idx = -1;
        for (int i = 0; i < Unit808Processor::kNumTrims; ++i) if (juce::String (Unit808Processor::kTrims[i].pad) == pad.id) idx = i;
        if (idx < 0) continue;
        auto s = std::make_unique<juce::Slider> (juce::Slider::RotaryVerticalDrag, juce::Slider::NoTextBox);
        s->setLookAndFeel (&lnf);
        s->setRotaryParameters (kPi * 1.25f, kPi * 2.75f, true);
        s->setMouseDragSensitivity (260);
        s->setScrollWheelEnabled (false);
        s->setPopupDisplayEnabled (true, true, this, 900);
        s->setMouseCursor (juce::MouseCursor::PointingHandCursor);
        s->setBounds (juce::Rectangle<float> (pad.size + 10.0f, pad.size + 10.0f).withCentre ({ pad.cx, pad.cy }).toNearestInt());
        addAndMakeVisible (*s);
        const auto id = Unit808Processor::trimId (idx);
        atts.push_back (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (p.apvts, id, *s));
        auto* prm = p.apvts.getParameter (id);
        s->setDoubleClickReturnValue (true, (double) prm->convertFrom0to1 (prm->getDefaultValue()));
        trims.push_back (std::move (s));
    }
}

BoardView::~BoardView() { for (auto& t : trims) t->setLookAndFeel (nullptr); }

static void ledGlow (juce::Graphics& g, juce::Point<float> c, juce::Colour col, float r)
{
    g.setGradientFill (juce::ColourGradient (col.withAlpha (0.55f), c, col.withAlpha (0.0f), c.translated (r * 2.6f, 0), true));
    g.fillEllipse (juce::Rectangle<float> (r * 5.2f, r * 5.2f).withCentre (c));
    g.setGradientFill (juce::ColourGradient (col.brighter (0.9f), c.translated (-r * 0.3f, -r * 0.35f), col, c.translated (r, r), true));
    g.fillEllipse (juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c));
    g.setColour (juce::Colours::white.withAlpha (0.75f));
    g.fillEllipse (juce::Rectangle<float> (r * 0.7f, r * 0.55f).withCentre (c.translated (-r * 0.3f, -r * 0.4f)));
}

void PhotoKey::paint (juce::Graphics& g)
{
    if (down)   // the cap goes down a little
    {
        g.setColour (juce::Colours::black.withAlpha (0.22f));
        g.fillRect (juce::Rectangle<float> (6.0f, 9.0f, 50.0f, 79.0f));
    }
    const int st = ledState ? ledState() : 0;
    if (st > 0) ledGlow (g, { 30.5f, down ? 27.0f : 25.0f }, st == 2 ? juce::Colour (0xff3d8bff) : juce::Colour (0xffff2a1c), 8.5f);
}

bool BoardView::closesBoard (juce::Point<float> p)
{
    return juce::Rectangle<float> (110.0f, 68.0f, 78.0f, 84.0f).contains (p)          // the black POWER switch
        || juce::Rectangle<float> (50.0f, 232.0f, 362.0f, 500.0f).contains (p)        // the power supply: transformer and its board
        || juce::Rectangle<float> (kPlateX, kPlateY, kPlateW, kPlateH).contains (p);  // the Rhythm Composer lettering
}

void BoardView::paint (juce::Graphics& g)
{
    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
    g.drawImage (board, getLocalBounds().toFloat());
    const int m = partLeds ? partLeds() : 0;
    if (m & 1) ledGlow (g, { 340.0f, 908.0f }, juce::Colour (0xffff2a1c), 8.5f);
    if (m & 2)
    {
        const auto r = juce::Rectangle<float> (18.0f, 18.0f).withCentre ({ 340.0f, 955.0f });
        g.setGradientFill (juce::ColourGradient (juce::Colour (0x88ffd23a), r.getCentre(), juce::Colour (0x00ffd23a), r.getCentre().translated (24.0f, 0), true));
        g.fillEllipse (r.expanded (16.0f));
        g.setGradientFill (juce::ColourGradient::vertical (juce::Colour (0xfffff1a0), r.getY(), juce::Colour (0xffffb800), r.getBottom()));
        g.fillRect (r);
    }
}

void BoardView::mouseMove (const juce::MouseEvent& e)
{
    setMouseCursor (closesBoard (e.position) ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
}

void BoardView::mouseUp (const juce::MouseEvent& e)
{
    if (closesBoard (e.position) && onClose) onClose();
}

PowerSwitch::PowerSwitch (const Ctrl& c) : ctrl (c)
{
    off = image (c.a[0]); on = image (c.a[1]);
    auto r = juce::Rectangle<float> (c.x, c.y, c.w, c.h);
    for (int i = 0; i < 2; ++i) r = r.getUnion ({ c.a[i].x, c.a[i].y, c.a[i].w, c.a[i].h });
    setBounds (r.getSmallestIntegerContainer());
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void PowerSwitch::paint (juce::Graphics& g)
{
    const int i = showingBoard ? 0 : 1;
    drawAt (g, i == 0 ? off : on, ctrl.a[i], { (float) getX(), (float) getY() });
}

// ---------------------------------------------------------------------------------------------- About
AboutOverlay::AboutOverlay()
{
    setWantsKeyboardFocus (true);
    logo = image (kBrand);
    manualBtn.onClick = [] { if (auto f = writeManual(); f.existsAsFile()) f.startAsProcess(); };
    manualBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (0xffe9d49b));
    manualBtn.setColour (juce::TextButton::textColourOffId, juce::Colour (0xff3a3422));
    addAndMakeVisible (manualBtn);
    for (auto* h : { &mail, &shop })
    {
        h->setFont (font (22.0f, true), false, juce::Justification::centredLeft);
        h->setColour (juce::HyperlinkButton::textColourId, kOrange);
        addAndMakeVisible (*h);
    }
}

juce::File AboutOverlay::writeManual()
{
    auto f = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("UNIT808 Manual " JucePlugin_VersionString ".pdf");
    const auto size = (juce::int64) BinaryData::UNIT808_Manual_pdfSize;
    if (! f.existsAsFile() || f.getSize() != size)
        if (! f.replaceWithData (BinaryData::UNIT808_Manual_pdf, (size_t) size)) return {};
    return f;
}

juce::Rectangle<float> AboutOverlay::card() const { return juce::Rectangle<float> (1060.0f, 620.0f).withCentre (getLocalBounds().toFloat().getCentre()); }

void AboutOverlay::resized()
{
    const auto c = card();
    mail.setBounds (juce::Rectangle<float> (c.getX() + 300.0f, c.getY() + 380.0f, 420.0f, 34.0f).toNearestInt());
    shop.setBounds (juce::Rectangle<float> (c.getX() + 300.0f, c.getY() + 420.0f, 420.0f, 34.0f).toNearestInt());
    manualBtn.setBounds (juce::Rectangle<float> (c.getRight() - 320.0f, c.getBottom() - 92.0f, 270.0f, 50.0f).toNearestInt());
}

void AboutOverlay::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black.withAlpha (0.62f));
    const auto c = card();
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillRect (c.translated (0, 8.0f));
    {
        // the card is a piece of the panel: dark body, a pale rule and the orange line under "Rhythm Composer"
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (c.toNearestInt());
        g.setGradientFill (juce::ColourGradient::vertical (juce::Colour (0xff3d4040), c.getY(), juce::Colour (0xff262a2a), c.getBottom()));
        g.fillRect (c);
        g.setColour (juce::Colour (0xffb8b9b2));
        g.fillRect (c.getX() + 40.0f, c.getY() + 176.0f, c.getWidth() - 80.0f, 4.0f);
    }
    g.setColour (juce::Colour (0xff565a5a));
    g.drawRect (c, 2.0f);

    const float x0 = c.getX() + 48.0f;
    if (logo.isValid())
    {
        // the brand artwork, sized to the card
        const float h = 92.0f, w = h * (float) logo.getWidth() / (float) logo.getHeight();
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        g.drawImage (logo, juce::Rectangle<float> (x0 - 20.0f, c.getY() + 40.0f, w, h));
    }
    g.setColour (kOrange);
    g.setFont (font (58.0f));
    g.drawText ("Rhythm Composer", juce::Rectangle<float> (x0, c.getY() + 200.0f, 640.0f, 70.0f), juce::Justification::centredLeft);
    g.setFont (font (38.0f));
    g.drawText ("UNIT808", juce::Rectangle<float> (c.getRight() - 300.0f, c.getY() + 212.0f, 250.0f, 56.0f), juce::Justification::centredRight);
    g.fillRect (x0, c.getY() + 278.0f, c.getWidth() - 96.0f, 3.0f);

    auto line = [&] (const juce::String& label, const juce::String& value, float y)
    {
        g.setColour (juce::Colour (0xff9fa29a));
        g.setFont (font (17.0f, true));
        g.drawText (label, juce::Rectangle<float> (x0, y, 240.0f, 34.0f), juce::Justification::centredLeft);
        g.setColour (kCream);
        g.setFont (font (22.0f, true));
        g.drawText (value, juce::Rectangle<float> (x0 + 252.0f, y, 700.0f, 34.0f), juce::Justification::centredLeft, true);
    };
    juce::String format = juce::AudioProcessor::getWrapperTypeDescription (juce::PluginHostType::getPluginLoadedAs());
    if (format == "Undefined") format = "Test build";
    const juce::String host = juce::PluginHostType().getHostDescription();
    if (host.isNotEmpty() && host != "Unknown") format << " in " << host;
    line ("VERSION", "v" JucePlugin_VersionString "   (built " U808_BUILD_DATE ")", c.getY() + 300.0f);
    line ("RUNNING AS", format, c.getY() + 340.0f);
    line ("CONTACT", {}, c.getY() + 380.0f);
    line ("STORE", {}, c.getY() + 420.0f);
    g.setColour (juce::Colour (0xff9fa29a));
    g.setFont (font (15.0f));
    g.drawText (juce::String (juce::CharPointer_UTF8 ("Ticco Ross \xc2\xb7 \xc2\xa9 2026 \xc2\xb7 Made with JUCE. VST is a trademark of Steinberg Media Technologies GmbH. "
                                                      "Audio Units is a trademark of Apple Inc.")),
                juce::Rectangle<float> (x0, c.getBottom() - 46.0f, 640.0f, 26.0f), juce::Justification::centredLeft, true);
    g.drawText ("CLICK ANYWHERE TO CLOSE", juce::Rectangle<float> (x0, c.getBottom() - 84.0f, 400.0f, 26.0f), juce::Justification::centredLeft);
}

// ---------------------------------------------------------------------------------------------- MIDI export
// buttons of the export window drawn like the 808's own keys: a cream cap, a red LED on the toggles
struct ExportKeyLnf : public juce::LookAndFeel_V4
{
    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down) override
    {
        const bool toggle = b.getClickingTogglesState();
        auto cap = toggle ? juce::Rectangle<float> (4.0f, 4.0f, 44.0f, (float) b.getHeight() - 8.0f) : b.getLocalBounds().toFloat().reduced (2.0f);
        g.setColour (juce::Colour (0xff1c1e1e));
        g.fillRect (cap.expanded (2.0f).translated (0, 2.0f));
        if (down) cap = cap.translated (0, 2.0f);
        g.setGradientFill (juce::ColourGradient::vertical (juce::Colour (over ? 0xffebe8dc : 0xffe2dfd3), cap.getY(), juce::Colour (0xffbdb9ab), cap.getBottom()));
        g.fillRect (cap);
        g.setColour (juce::Colours::white.withAlpha (0.5f));
        g.fillRect (cap.getX() + 3.0f, cap.getY() + 3.0f, cap.getWidth() - 6.0f, 2.0f);
        if (toggle)
        {
            const auto c = cap.getCentre();
            const bool on = b.getToggleState();
            g.setColour (on ? juce::Colour (0xffff2a1c) : juce::Colour (0xff5a1a14));
            g.fillEllipse (juce::Rectangle<float> (12.0f, 12.0f).withCentre (c));
            if (on) { g.setColour (juce::Colour (0x55ff2a1c)); g.fillEllipse (juce::Rectangle<float> (24.0f, 24.0f).withCentre (c)); }
        }
    }
    void drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool down) override
    {
        const bool toggle = b.getClickingTogglesState();
        g.setColour (juce::Colour (0xff1c1e1e));
        g.setFont (font (toggle ? 16.0f : 20.0f, ! toggle));
        auto r = b.getLocalBounds().toFloat();
        if (toggle) g.drawFittedText (b.getButtonText(), r.withTrimmedLeft (60.0f).toNearestInt(), juce::Justification::centredLeft, 2);
        else g.drawText (b.getButtonText(), r.translated (0, down ? 2.0f : 0.0f), juce::Justification::centred);
    }
};

ExportPanel::ExportPanel (Unit808Processor& p) : proc (p), lnf (std::make_unique<ExportKeyLnf>())
{
    setWantsKeyboardFocus (true);
    for (auto* b : { &saveBtn, &swingBtn, &muteBtn })
    {
        b->setLookAndFeel (lnf.get());
        b->setMouseCursor (juce::MouseCursor::PointingHandCursor);
        addAndMakeVisible (*b);
    }
    swingBtn.setClickingTogglesState (true);
    muteBtn.setClickingTogglesState (true);
    swingBtn.onClick = [this] { proc.exportSwing = swingBtn.getToggleState(); refresh(); };
    muteBtn.onClick  = [this] { proc.exportMuteSolo = muteBtn.getToggleState(); refresh(); };
    saveBtn.onClick = [this]
    {
        auto start = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile (juce::File::createLegalFileName (name) + ".mid");
        chooser = std::make_unique<juce::FileChooser> ("Save MIDI file", start, "*.mid");
        ExportPanel* self = this;   // (MSVC: no `this` inside the nested init-capture)
        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
                              [safe = juce::Component::SafePointer<ExportPanel> (self)] (const juce::FileChooser& fc)
                              {
                                  if (safe == nullptr) return;
                                  auto f = fc.getResult();
                                  if (f == juce::File()) return;
                                  f = f.withFileExtension ("mid");
                                  safe->status = safe->proc.writeMidi (safe->spec, f) ? "SAVED: " + f.getFileName() : "COULD NOT WRITE " + f.getFileName();
                                  safe->repaint();
                              });
    };
}

ExportPanel::~ExportPanel() { for (auto* b : { &saveBtn, &swingBtn, &muteBtn }) b->setLookAndFeel (nullptr); }

juce::Rectangle<float> ExportPanel::card() const { return juce::Rectangle<float> (860.0f, 560.0f).withCentre ({ 935.0f, 545.0f }); }
juce::Rectangle<float> ExportPanel::tile() const { const auto c = card(); return { c.getX() + 40.0f, c.getY() + 130.0f, c.getWidth() - 80.0f, 170.0f }; }
juce::File ExportPanel::tempFile() const
{
    return juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("UNIT808 MIDI").getChildFile (juce::File::createLegalFileName (name) + ".mid");
}

void ExportPanel::show (const Unit808Processor::ExportSpec& x)
{
    spec = x; status = {};
    swingBtn.setToggleState (proc.exportSwing, juce::dontSendNotification);
    muteBtn.setToggleState (proc.exportMuteSolo, juce::dontSendNotification);
    refresh();
    toFront (false);
    setVisible (true);
    if (isShowing()) grabKeyboardFocus();
}

void ExportPanel::refresh()
{
    name = proc.exportName (spec);
    proc.buildMidi (spec, &notes, &bars);
    // the first bar of the clip, shown on 16 little step keys
    int slot = spec.slot, var = spec.var, part = (spec.kind == Unit808Processor::ExPart || spec.kind == Unit808Processor::ExRow) ? spec.part : 0;
    if (spec.kind == Unit808Processor::ExRhythm) var = 0;
    if (spec.kind == Unit808Processor::ExTrack)
    {
        if (proc.memory.trackLen[spec.track].load() > 0) { const auto e = proc.memory.entry (spec.track, 0); slot = e.slot; var = e.var == Unit808Processor::VarB ? 1 : 0; }
    }
    const auto& pa = proc.memory.rhythm[slot].part[var][part];
    lit = 0;
    for (int st = 0; st < 16; ++st)
        for (int r = 1; r < unit808::kNumRows; ++r)
            if ((spec.kind != Unit808Processor::ExRow || r == spec.row) && pa.cell (r, st)) lit |= (uint16_t) (1u << st);
    repaint();
}

void ExportPanel::resized()
{
    const auto c = card();
    swingBtn.setBounds (juce::Rectangle<float> (c.getX() + 40.0f, c.getY() + 326.0f, 370.0f, 50.0f).toNearestInt());
    muteBtn.setBounds (juce::Rectangle<float> (c.getX() + 440.0f, c.getY() + 326.0f, 380.0f, 50.0f).toNearestInt());
    saveBtn.setBounds (juce::Rectangle<float> (c.getX() + 40.0f, c.getY() + 400.0f, c.getWidth() - 80.0f, 62.0f).toNearestInt());
}

void ExportPanel::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black.withAlpha (0.6f));
    const auto c = card();
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillRect (c.translated (0, 8.0f));
    // like the lower half of the panel: light grey, with a dark strip on top
    g.setColour (juce::Colour (0xffc6c3b7));
    g.fillRect (c);
    const auto top = c.withHeight (96.0f);
    g.setColour (kPanel);
    g.fillRect (top);
    g.setColour (kOrange);
    g.fillRect (c.getX(), top.getBottom(), c.getWidth(), 4.0f);
    g.setFont (font (38.0f));
    g.drawText ("EXPORT MIDI", top.withTrimmedLeft (40.0f), juce::Justification::centredLeft);
    g.setColour (kCream);
    g.setFont (font (19.0f));
    g.drawText (name.fromFirstOccurrenceOf ("UNIT808 ", false, false).toUpperCase(), top.withTrimmedRight (40.0f).withTrimmedLeft (330.0f), juce::Justification::centredRight, true);

    // the clip: a dark strip with 16 little step keys; grab it and drop it on a DAW track
    const auto t = tile();
    g.setColour (pressedInTile ? juce::Colour (0xff3a3d3c) : juce::Colour (0xff2b2e2d));
    g.fillRect (t);
    g.setColour (juce::Colour (0xff1c1e1e));
    g.drawRect (t, 2.0f);
    const juce::Colour keyCol[4] = { juce::Colour (0xffb5422c), juce::Colour (0xffd47a33), juce::Colour (0xffd8b546), juce::Colour (0xffe2dfd3) };
    const float kw = 38.0f, gap = (t.getWidth() - 40.0f - 16.0f * kw) / 15.0f;
    for (int i = 0; i < 16; ++i)
    {
        const auto k = juce::Rectangle<float> (t.getX() + 20.0f + (float) i * (kw + gap), t.getY() + 22.0f, kw, 56.0f);
        g.setColour (juce::Colour (0xff1c1e1e)); g.fillRect (k.expanded (1.5f).translated (0, 1.5f));
        g.setColour (keyCol[i / 4]); g.fillRect (k);
        const bool on = (lit >> i) & 1;
        const auto led = juce::Rectangle<float> (10.0f, 10.0f).withCentre ({ k.getCentreX(), k.getY() + 13.0f });
        if (on) { g.setColour (juce::Colour (0x66ff2a1c)); g.fillEllipse (led.expanded (6.0f)); }
        g.setColour (on ? juce::Colour (0xffff2a1c) : juce::Colour (0xff5a1a14));
        g.fillEllipse (led);
        g.setColour (juce::Colour (0xff9a988e));
        g.setFont (font (13.0f));
        g.drawText (juce::String (i + 1), k.withY (k.getBottom() + 3.0f).withHeight (16.0f), juce::Justification::centred);
    }
    g.setColour (notes > 0 ? kCream : juce::Colour (0xff8a8a82));
    g.setFont (font (22.0f));
    g.drawText (notes > 0 ? "DRAG THIS INTO YOUR DAW" : "NOTHING TO EXPORT", juce::Rectangle<float> (t.getX() + 20.0f, t.getBottom() - 54.0f, 420.0f, 40.0f), juce::Justification::centredLeft);
    g.setColour (juce::Colour (0xff9a988e));
    g.setFont (font (15.0f));
    g.drawText (juce::String (notes) + " NOTES   " + juce::String (bars, bars == std::floor (bars) ? 0 : 2) + (bars == 1.0 ? " BAR   " : " BARS   ")
                + juce::String (proc.getHostBpm(), 1) + " BPM   CH 10", juce::Rectangle<float> (t.getRight() - 420.0f, t.getBottom() - 54.0f, 400.0f, 40.0f),
                juce::Justification::centredRight);

    g.setColour (juce::Colour (0xff4a4c4a));
    g.setFont (font (14.0f));
    g.drawText (status.isNotEmpty() ? status : "ESC OR CLICK OUTSIDE TO CLOSE", juce::Rectangle<float> (c.getX() + 40.0f, c.getBottom() - 70.0f, c.getWidth() - 80.0f, 24.0f),
                juce::Justification::centredLeft, true);
}

void ExportPanel::mouseDown (const juce::MouseEvent& e)
{
    pressedInTile = tile().contains (e.position) && notes > 0;
    dragging = false;
    repaint();
}

void ExportPanel::mouseDrag (const juce::MouseEvent& e)
{
    if (! pressedInTile || dragging || e.getDistanceFromDragStart() < 6) return;
    const auto f = tempFile();
    if (! proc.writeMidi (spec, f)) { status = "COULD NOT WRITE THE MIDI FILE"; repaint(); return; }
    dragging = true;
    juce::DragAndDropContainer::performExternalDragDropOfFiles ({ f.getFullPathName() }, false, this,
        [safe = juce::Component::SafePointer<ExportPanel> (this)] { if (safe != nullptr) { safe->dragging = false; safe->pressedInTile = false; safe->repaint(); } });
}

void ExportPanel::mouseUp (const juce::MouseEvent& e)
{
    const bool wasTile = pressedInTile;
    pressedInTile = false;
    repaint();
    if (! wasTile && ! card().contains (e.position)) setVisible (false);
}
} // namespace unit808ui

// ================================================================================================== Editor
Unit808Editor::Unit808Editor (Unit808Processor& p) : AudioProcessorEditor (p), proc (p)
{
    faceplate = image (kFaceplate);
    addAndMakeVisible (content);
    content.setBounds (0, 0, (int) kPanelW, (int) kPanelH);

    // knobs: every continuous knob of the layout, attached to the parameter of the same id
    struct KD { const char* id; const char* param; const char* tip; float minA, maxA; };
    const KD kd[] = {
        { "ac-level", "ac-level", "Accent: how much louder accented steps play", -135, 135 },
        { "bd-level", "bd-level", "Bass Drum level", -135, 135 }, { "bd-tone", "bd-tone", "Bass Drum tone", -135, 135 },
        { "bd-decay", "bd-decay", "Bass Drum decay", -135, 135 },
        { "sd-level", "sd-level", "Snare Drum level", -135, 135 }, { "sd-tone", "sd-tone", "Snare Drum tone", -135, 135 },
        { "sd-snappy", "sd-snappy", "Snare Drum snappy (noise)", -135, 135 },
        { "lt-level", "lt-level", "Low Tom / Low Conga level", -135, 135 }, { "lt-tuning", "lt-tuning", "Low Tom / Low Conga tuning", -135, 135 },
        { "mt-level", "mt-level", "Mid Tom / Mid Conga level", -135, 135 }, { "mt-tuning", "mt-tuning", "Mid Tom / Mid Conga tuning", -135, 135 },
        { "ht-level", "ht-level", "Hi Tom / Hi Conga level", -135, 135 }, { "ht-tuning", "ht-tuning", "Hi Tom / Hi Conga tuning", -135, 135 },
        { "rs-level", "rs-level", "Rim Shot / Claves level", -135, 135 }, { "cp-level", "cp-level", "Hand Clap / Maracas level", -135, 135 },
        { "cb-level", "cb-level", "Cow Bell level", -135, 135 },
        { "cy-level", "cy-level", "Cymbal level", -135, 135 }, { "cy-tone", "cy-tone", "Cymbal tone", -135, 135 },
        { "cy-decay", "cy-decay", "Cymbal decay", -135, 135 },
        { "oh-level", "oh-level", "Open Hi-hat level", -135, 135 }, { "oh-decay", "oh-decay", "Open Hi-hat decay", -135, 135 },
        { "ch-level", "ch-level", "Closed Hi-hat level", -135, 135 },
        { "master-volume", "master-volume", "Master volume", -135, 135 },
        { "swing", "swing", "Swing: delays every second step (0 = straight). Tempo always follows your DAW", -130, 130 },
        { "swing-fine", "flam", "", -135, 135 } };
    for (auto& k : kd)
    {
        const Ctrl* c = find (k.id);
        if (c == nullptr) continue;
        auto s = std::make_unique<Knob> (*c, k.minA, k.maxA);
        s->setPopupDisplayEnabled (true, true, &content, 900);
        content.addAndMakeVisible (*s);
        knobAtts.push_back (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, k.param, *s));
        auto* prm = proc.apvts.getParameter (k.param);
        s->setDoubleClickReturnValue (true, (double) prm->convertFrom0to1 (prm->getDefaultValue()));
        knobs.push_back (std::move (s));
    }
    // selector knobs
    for (auto [id, tip] : { std::pair<const char*, const char*> { "mode", "MODE: Pattern Clear, 1st / 2nd Part (write), Manual Play, Play, Compose" },
                            { "instrument-select", "INSTRUMENT-SELECT: the instrument the step keys write. In PLAY / COMPOSE: the rhythm track 1-12" },
                            { "auto-fill", "MEASURES AUTO FILL IN: play the intro/fill pattern every 16, 12, 8, 4 or 2 measures (Manual = off)" } })
    {
        auto s = std::make_unique<Selector> (*find (id), *proc.apvts.getParameter (id));
        (void) tip;
        if (juce::String (id) == "instrument-select")
        {
            // the printed AC..CH label windows around INSTRUMENT-SELECT: Alt-click mute, Shift-click solo, right-click menu
            static const float lx[12] = { 305, 269, 242.65f, 233, 242.65f, 269, 305, 341, 367.35f, 377, 367.35f, 341 };
            static const float ly[12] = { 348, 339.02f, 314.5f, 281, 247.5f, 231, 231, 231, 247.5f, 281, 314.5f, 339.02f };
            for (int i = 0; i < 12; ++i) s->labels.push_back ({ lx[i], ly[i], 30.0f, 16.0f });
            s->onLabel = [this] (int i, juce::ModifierKeys m) { instrumentLabelClick (i, m); };
            s->labelState = [this] (int i) { return proc.isSoloed (i) ? 2 : (proc.isMuted (i) ? 1 : 0); };
        }
        content.addAndMakeVisible (*s);
        selectors.push_back (std::move (s));
    }
    // the instrument name windows on the channels select the instrument for writing (as INSTRUMENT-SELECT)
    {
        auto select = [this] (int inst)
        {
            auto* prm = proc.apvts.getParameter ("instrument-select");
            prm->beginChangeGesture();
            prm->setValueNotifyingHost (prm->convertTo0to1 ((float) inst));
            prm->endChangeGesture();
        };
        for (int i = 0; i < 12; ++i)   // ACCENT, BASS DRUM ... CLS'D HIHAT
        {
            channelLabels.push_back (std::make_unique<ChannelLabel> (juce::Rectangle<float> (419.5f + 110.0f * (float) i, 553.0f, 101.0f, 34.0f), i));
            if (i >= 3 && i <= 7)      // LOW CONGA, MID CONGA, HI CONGA, CLAVES, MARACAS above
                channelLabels.push_back (std::make_unique<ChannelLabel> (juce::Rectangle<float> (419.5f + 110.0f * (float) i, 457.0f, 101.0f, 34.0f), i));
        }
        for (auto& l : channelLabels)
        {
            l->onClick = select;
            l->onMuteSolo = [this] (int i, juce::ModifierKeys m) { instrumentLabelClick (i, m); };
            l->isSelected = [this] (int i) { return proc.getInstrument() == i; };
            content.addAndMakeVisible (*l);
        }
    }
    // switches and levers
    struct SD { const char* id; bool horiz; std::vector<int> map; const char* tip; };
    const SD sd[] = {
        { "lt-select", false, { 1, 0 }, "Low Tom (down) / Low Conga (up)" }, { "mt-select", false, { 1, 0 }, "Mid Tom (down) / Mid Conga (up)" },
        { "ht-select", false, { 1, 0 }, "Hi Tom (down) / Hi Conga (up)" }, { "rs-select", false, { 1, 0 }, "Rim Shot (down) / Claves (up)" },
        { "cp-select", false, { 1, 0 }, "Hand Clap (down) / Maracas (up)" },
        { "pre-scale", false, { 0, 1, 2, 3 }, "PRE-SCALE: 1 = 16th triplets, 2 = 32nd notes, 3 = 16th notes (normal), 4 = 8th triplets" },
        { "basic-variation", true, { 0, 1, 2 }, "BASIC VARIATION: A, A then B, or B" },
        { "if-variation", true, { 0, 1 }, "I/F VARIATION: which variation of the intro/fill pattern plays" } };
    for (auto& s : sd)
    {
        auto w = std::make_unique<Switch> (*find (s.id), *proc.apvts.getParameter (s.id), s.horiz, s.map);
        content.addAndMakeVisible (*w);
        switches.push_back (std::move (w));
    }
    // the 16 step keys, each with its LED
    for (int s = 0; s < 16; ++s)
    {
        const juce::String id = juce::String ("step-") + (s < 9 ? "0" : "") + juce::String (s + 1);
        auto k = std::make_unique<Key> (*find (id.toRawUTF8()), find ((id + "-led").toRawUTF8()));
        k->ledState = [this, s] { return stepLed (s); };
        k->onPress = [this, s] (juce::ModifierKeys m) { stepKeyPress (s, m); };
        k->onDrag = [this] (juce::Point<float> pos) { stepKeyDragTo (pos); };
        k->onRelease = [this] { paintValue = -1; paintedKeys = 0; };
        content.addAndMakeVisible (*k);
        stepKeys.push_back (std::move (k));
    }
    startStop = std::make_unique<Key> (*find ("start-stop"));
    startStop->heldDown = [this] { return proc.isTransportRunning(); };
    startStop->onPress = [this] (juce::ModifierKeys m) { if (m.isPopupMenu()) startStopMenu(); else proc.startStopKey(); };
    tap = std::make_unique<Key> (*find ("tap"));
    tap->onPress = [this] (juce::ModifierKeys) { proc.tapKey(); content.repaint(); };
    clear = std::make_unique<Key> (*find ("track-clear-led"));
    clear->heldDown = [this] { return proc.isStepNumberArmed(); };
    clear->onPress = [this] (juce::ModifierKeys m) { if (m.isPopupMenu()) clearMenu(); else proc.clearKey (m); content.repaint(); };
    for (auto* k : { startStop.get(), tap.get(), clear.get() }) content.addAndMakeVisible (*k);

    auto led = [] (const char* id) { const Ctrl* c = find (id); return Led { c, image (c->a[0]), image (c->a[1]) }; };
    varA = led ("var-a-led"); varB = led ("var-b-led"); part1 = led ("part1-led"); part2 = led ("part2-led");

    // POWER opens the circuit board inside (UNIT808 keeps playing); the plate on the board, or POWER again, closes it
    power = std::make_unique<PowerSwitch> (*find ("power"));
    power->onToggle = [this] { showBoard (! board->isVisible()); };
    content.addAndMakeVisible (*power);
    board = std::make_unique<BoardView> (proc);
    board->onClose = [this] { showBoard (false); };
    board->partLeds = [this] { return partLedMask(); };
    for (int s = 0; s < 16; ++s)   // the STEP keys on the board inside work like the ones on the panel
    {
        auto k = std::make_unique<PhotoKey>();
        k->setBounds (juce::Rectangle<float> (399.0f + (float) s * 72.9f, 900.0f, 62.0f, 90.0f).toNearestInt());
        k->setMouseCursor (juce::MouseCursor::PointingHandCursor);
        k->ledState = [this, s] { return stepLed (s); };
        k->onPress = [this, s] (juce::ModifierKeys m) { stepKeyPress (s, m); };
        k->onDrag = [this] (juce::Point<float> pos) { stepKeyDragTo (pos, true); };
        k->onRelease = [this] { paintValue = -1; paintedKeys = 0; };
        board->addAndMakeVisible (*k);
        board->keys[(size_t) s] = std::move (k);
    }
    content.addChildComponent (*board);
    board->setBounds (0, 0, (int) kPanelW, (int) kPanelH);

    content.addChildComponent (about);
    about.setBounds (0, 0, (int) kPanelW, (int) kPanelH);
    content.addChildComponent (exporter);
    exporter.setBounds (0, 0, (int) kPanelW, (int) kPanelH);

    setLookAndFeel (&menuLnf);   // right-click menus at double size
    setResizable (true, true);
    setResizeLimits ((int) (kPanelW * 0.4f), (int) (kPanelH * 0.4f), (int) kPanelW, (int) kPanelH);
    getConstrainer()->setFixedAspectRatio ((double) kPanelW / kPanelH);
    setSize ((int) (kPanelW * 0.6f), (int) (kPanelH * 0.6f));
    startTimerHz (30);
}

Unit808Editor::~Unit808Editor() { stopTimer(); setLookAndFeel (nullptr); }

void Unit808Editor::showBoard (bool show)
{
    // the board covers the whole panel (POWER included); its plate closes it again
    board->setVisible (show);
    if (show) { board->toFront (false); board->setWantsKeyboardFocus (true); if (board->isShowing()) board->grabKeyboardFocus(); }
    about.toFront (false);
    power->showingBoard = show;
    power->repaint();
}

void Unit808Editor::paint (juce::Graphics& g) { g.fillAll (kPanel); }

void Unit808Editor::resized() { content.setTransform (juce::AffineTransform::scale ((float) getWidth() / kPanelW)); }

void Unit808Editor::Content::mouseUp (const juce::MouseEvent& e)
{
    const Ctrl* a = find ("about");
    if (a && juce::Rectangle<float> (a->x, a->y, a->w, a->h).contains (e.position))
    {
        ed.about.toFront (false);
        ed.about.setVisible (true);
        if (ed.about.isShowing()) ed.about.grabKeyboardFocus();
    }
}

int Unit808Editor::stepLed (int s) const
{
    if (! proc.isPowerOn()) return 0;
    if (s == flashKey && juce::Time::getMillisecondCounter() < flashUntil)   // just pasted here
        return ((juce::Time::getMillisecondCounter() / 90) % 2) == 0 ? 1 : 0;
    const int play = proc.getPlayStep();
    const bool blink = (juce::Time::getMillisecondCounter() / 180) % 2 == 0;
    switch (proc.getMode())
    {
        case Unit808Processor::FirstPart:
        case Unit808Processor::SecondPart:
        {
            const auto& part = proc.editedPart();
            const int len = part.length.load() == 0 ? 16 : part.length.load();
            if (s >= len) return 0;
            const int inst = proc.getInstrument();
            const bool on = part.cell (inst, s);
            const bool here = play >= 0 && proc.getPlaySlot() == proc.getSelectedSlot() && proc.getPlayPart() == proc.editPart();
            if (here && s == play) return on ? 0 : 1;              // the playing step shows inverted
            return on ? (part.flam (inst, s) ? 2 : 1) : 0;         // blue = flam
        }
        default:
        {
            if (s == play) return 1;                                        // the running light
            if (s == proc.getPendingSlot()) return blink ? 1 : 0;           // queued for the next measure
            if (s >= unit808::kFirstFill && s == proc.getSelectedFill()) return (proc.isFillRequested() ? blink : true) ? 1 : 0;
            return s == proc.getSelectedSlot() ? 1 : 0;
        }
    }
}

void Unit808Editor::paintPanel (juce::Graphics& g)
{
    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
    g.drawImage (faceplate, juce::Rectangle<float> (kPanelW, kPanelH));

    // BASIC VARIATION A / B and 1st / 2nd PART LEDs
    const bool pw = proc.isPowerOn();
    const bool run = proc.isRunning();
    const int mode = proc.getMode();
    bool a, b, p1, p2;
    if (run) { a = proc.getPlayVar() == 0; b = ! a; p1 = proc.getPlayPart() == 0; p2 = ! p1; }
    else
    {
        const int v = proc.getBasicVar();
        a = v != Unit808Processor::VarB; b = v != Unit808Processor::VarA;
        p1 = mode != Unit808Processor::SecondPart; p2 = mode == Unit808Processor::SecondPart;
    }
    if (mode == Unit808Processor::FirstPart || mode == Unit808Processor::SecondPart) { p1 = mode == Unit808Processor::FirstPart; p2 = ! p1; }
    auto draw = [&] (const Led& l, bool on) { on = on && pw; drawAt (g, on ? l.on : l.off, l.c->a[on ? 1 : 0], {}); };
    draw (varA, a); draw (varB, b); draw (part1, p1); draw (part2, p2);
}

void Unit808Editor::timerCallback()
{
    const int v = proc.uiVersion.load() + proc.memory.version.load() * 7919 + (proc.isTransportRunning() ? 1 : 0) * 104729;
    const bool blinking = proc.getPendingSlot() >= 0 || proc.isFillRequested() || juce::Time::getMillisecondCounter() < flashUntil + 100;
    if (v != lastUi || blinking)
    {
        lastUi = v;
        for (auto& k : stepKeys) k->repaint();
        if (board->isVisible()) board->repaint (juce::Rectangle<int> (300, 880, 1240, 120));
        startStop->repaint(); clear->repaint();
        content.repaint (juce::Rectangle<int> (140, 830, 240, 150));
    }
    // mute / solo windows and the selected channel name (they can change from automation too)
    int ms = proc.getInstrument() << 24;
    for (int i = 0; i < 12; ++i) ms |= (proc.isMuted (i) ? 1 : 0) << i | (proc.isSoloed (i) ? 1 : 0) << (i + 12);
    if (ms != lastMuteSolo)
    {
        lastMuteSolo = ms;
        for (auto& sel : selectors) sel->repaint();
        for (auto& l : channelLabels) l->repaint();
    }
}

// Right-click a STEP key (where the keys choose rhythms): copy / paste the rhythm or one variation
void Unit808Editor::stepKeyMenu (int key)
{
    const int mode = proc.getMode();
    if (mode == Unit808Processor::FirstPart || mode == Unit808Processor::SecondPart) { clearMenu(); return; }
    const juce::String name = key < unit808::kFirstFill ? "Rhythm " + juce::String (key + 1) : "Intro/Fill " + juce::String (key - unit808::kFirstFill + 1);
    const int k = proc.clipKind();
    juce::PopupMenu m;
    m.setLookAndFeel (&menuLnf);
    m.addSectionHeader (name);
    m.addItem (1, "Copy " + name.toLowerCase());
    m.addItem (2, "Copy variation A");
    m.addItem (3, "Copy variation B");
    m.addSeparator();
    const juce::String from = proc.clipDescription().isNotEmpty() ? "  (" + proc.clipDescription() + ")" : juce::String();
    m.addItem (4, "Paste rhythm" + (k == Unit808Processor::ClipRhythm ? from : juce::String()), k == Unit808Processor::ClipRhythm);
    m.addItem (5, "Paste to variation A" + (k == Unit808Processor::ClipVariation ? from : juce::String()), k == Unit808Processor::ClipVariation);
    m.addItem (6, "Paste to variation B" + (k == Unit808Processor::ClipVariation ? from : juce::String()), k == Unit808Processor::ClipVariation);
    m.addSeparator();
    m.addItem (7, "Undo paste", proc.canUndoPaste());
    m.addSeparator();
    m.addItem (10, "Export " + name.toLowerCase() + " as MIDI...");
    m.addItem (11, "Export variation A as MIDI...");
    m.addItem (12, "Export variation B as MIDI...");
    const int track = proc.getTrack();
    if (mode == Unit808Processor::Play || mode == Unit808Processor::Compose)
        m.addItem (13, "Export rhythm track " + juce::String (track + 1) + " as MIDI...", proc.memory.trackLen[track].load() > 0);
    m.showMenuAsync (menuAt (board->isVisible() ? (juce::Component*) board->keys[(size_t) key].get() : stepKeys[(size_t) key].get()),
                     [safe = juce::Component::SafePointer<Unit808Editor> (this), key] (int r)
                     {
                         if (safe == nullptr || r <= 0) return;
                         auto& p = safe->proc;
                         bool pasted = false;
                         switch (r)
                         {
                             case 1: p.copyRhythm (key); break;
                             case 2: p.copyVariation (key, 0); break;
                             case 3: p.copyVariation (key, 1); break;
                             case 4: pasted = p.pasteRhythm (key); break;
                             case 5: pasted = p.pasteVariation (key, 0); break;
                             case 6: pasted = p.pasteVariation (key, 1); break;
                             case 7: p.undoPaste(); break;
                             case 10: case 11: case 12: case 13:
                             {
                                 Unit808Processor::ExportSpec x;
                                 x.slot = key; x.kind = r == 10 ? Unit808Processor::ExRhythm : r == 13 ? Unit808Processor::ExTrack : Unit808Processor::ExVariation;
                                 x.var = r == 12 ? 1 : 0; x.track = p.getTrack();
                                 safe->exportMidi (x);
                                 break;
                             }
                             default: break;
                         }
                         if (pasted) safe->flash (key);
                         safe->content.repaint();
                     });
}

// Right-click CLEAR (or a STEP key) in 1st / 2nd PART: copy / paste the part or the selected instrument
void Unit808Editor::clearMenu()
{
    const int mode = proc.getMode();
    if (mode != Unit808Processor::FirstPart && mode != Unit808Processor::SecondPart) return;
    const int slot = proc.getSelectedSlot(), var = proc.editVar(), part = proc.editPart(), inst = proc.getInstrument();
    const juce::String partName = juce::String (part == 0 ? "1st" : "2nd") + " part";
    const juce::String instName = Unit808Processor::kInstrumentNames[inst];
    const int k = proc.clipKind();
    const juce::String from = proc.clipDescription().isNotEmpty() ? "  (" + proc.clipDescription() + ")" : juce::String();
    juce::PopupMenu m;
    m.setLookAndFeel (&menuLnf);
    m.addSectionHeader ((slot < unit808::kFirstFill ? "Rhythm " + juce::String (slot + 1) : "Intro/Fill " + juce::String (slot - 11))
                        + " " + (var == 0 ? "A" : "B") + ", " + partName);
    m.addItem (1, "Copy this " + partName);
    m.addItem (2, "Copy " + instName);
    m.addSeparator();
    m.addItem (3, "Paste into this " + partName + (k == Unit808Processor::ClipPart ? from : juce::String()), k == Unit808Processor::ClipPart);
    m.addItem (4, "Paste into " + instName + (k == Unit808Processor::ClipRow ? from : juce::String()), k == Unit808Processor::ClipRow);
    m.addSeparator();
    m.addItem (5, "Undo paste", proc.canUndoPaste());
    m.addSeparator();
    m.addItem (10, "Export this " + partName + " as MIDI...");
    m.addItem (11, "Export " + instName + " as MIDI...", inst > 0);
    m.showMenuAsync (menuAt (clear.get()),
                     [safe = juce::Component::SafePointer<Unit808Editor> (this), slot, var, part, inst] (int r)
                     {
                         if (safe == nullptr || r <= 0) return;
                         auto& p = safe->proc;
                         switch (r)
                         {
                             case 1: p.copyPart (slot, var, part); break;
                             case 2: p.copyRow (slot, var, part, inst); break;
                             case 3: p.pastePart (slot, var, part); break;
                             case 4: p.pasteRow (slot, var, part, inst); break;
                             case 5: p.undoPaste(); break;
                             case 10: case 11:
                             {
                                 Unit808Processor::ExportSpec x;
                                 x.kind = r == 10 ? Unit808Processor::ExPart : Unit808Processor::ExRow;
                                 x.slot = slot; x.var = var; x.part = part; x.row = inst;
                                 safe->exportMidi (x);
                                 break;
                             }
                             default: break;
                         }
                         safe->content.repaint();
                     });
}

void Unit808Editor::toggleParam (const juce::String& id)
{
    if (auto* p = proc.apvts.getParameter (id))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->getValue() > 0.5f ? 0.0f : 1.0f);
        p->endChangeGesture();
    }
    for (auto& sel : selectors) sel->repaint();
}

// Option/Alt-click a label: mute. Shift-click: solo (several at once). Right-click: menu. AC can only be muted.
void Unit808Editor::instrumentLabelClick (int inst, juce::ModifierKeys m)
{
    static const char* names[12] = { "Accent", "Bass Drum", "Snare Drum", "Low Tom / Conga", "Mid Tom / Conga", "Hi Tom / Conga",
                                     "Rim Shot / Claves", "Hand Clap / Maracas", "Cow Bell", "Cymbal", "Open Hi-hat", "Closed Hi-hat" };
    if (m.isPopupMenu())
    {
        juce::PopupMenu menu;
        menu.setLookAndFeel (&menuLnf);
        menu.addSectionHeader (names[inst]);
        menu.addItem (1, "Mute", true, proc.isMuted (inst));
        if (inst > 0) menu.addItem (2, "Solo", true, proc.isSoloed (inst));
        menu.addSeparator();
        bool any = false;
        for (int i = 0; i < 12; ++i) any |= proc.isMuted (i) || proc.isSoloed (i);
        menu.addItem (3, "Clear all mutes and solos", any);
        menu.showMenuAsync (menuAt (this),
                            [safe = juce::Component::SafePointer<Unit808Editor> (this), inst] (int r)
                            {
                                if (safe == nullptr || r <= 0) return;
                                if (r == 1) safe->toggleParam (Unit808Processor::muteId (inst));
                                if (r == 2) safe->toggleParam (Unit808Processor::soloId (inst));
                                if (r == 3)
                                    for (int i = 0; i < 12; ++i)
                                    {
                                        if (safe->proc.isMuted (i)) safe->toggleParam (Unit808Processor::muteId (i));
                                        if (safe->proc.isSoloed (i)) safe->toggleParam (Unit808Processor::soloId (i));
                                    }
                            });
        return;
    }
    if (m.isShiftDown()) { if (inst > 0) toggleParam (Unit808Processor::soloId (inst)); }
    else if (m.isAltDown()) toggleParam (Unit808Processor::muteId (inst));
}

void Unit808Editor::stepKeyPress (int s, juce::ModifierKeys m)
{
    if (m.isPopupMenu()) { stepKeyMenu (s); return; }   // copy / paste / export
    const bool write = proc.getMode() == Unit808Processor::FirstPart || proc.getMode() == Unit808Processor::SecondPart;
    const bool stepNumber = m.isShiftDown() || m.isAltDown() || proc.isStepNumberArmed();
    proc.stepKey (s, m);
    paintValue = (write && ! stepNumber) ? (proc.stepIsSet (s) ? 1 : 0) : -1;   // drag on to paint the same state
    paintedKeys = 1u << s;
    content.repaint();
}

int Unit808Editor::partLedMask() const
{
    const int mode = proc.getMode();
    bool p1 = mode != Unit808Processor::SecondPart;
    if (proc.isRunning()) p1 = proc.getPlayPart() == 0;
    if (mode == Unit808Processor::FirstPart || mode == Unit808Processor::SecondPart) p1 = mode == Unit808Processor::FirstPart;
    return p1 ? 1 : 2;
}

void Unit808Editor::stepKeyDragTo (juce::Point<float> pos, bool inside)
{
    if (paintValue < 0) return;
    for (int s = 0; s < 16; ++s)
        if (const auto r = inside ? board->keys[(size_t) s]->getBounds().toFloat() : stepKeys[(size_t) s]->getBounds().toFloat();
            r.contains (pos.x, r.getCentreY())
            && (paintedKeys & (1u << s)) == 0)
        {
            paintedKeys |= 1u << s;
            proc.setStep (s, paintValue == 1);
        }
}

void Unit808Editor::startStopMenu()
{
    if (! proc.isStandalone()) return;
    juce::PopupMenu m;
    m.setLookAndFeel (&menuLnf);
    m.addSectionHeader ("Tempo (standalone app)");
    auto* prm = proc.apvts.getParameter ("standalone-bpm");
    const float cur = prm->convertFrom0to1 (prm->getValue());
    for (int bpm : { 90, 100, 110, 115, 120, 125, 128, 130, 135, 140, 150, 160, 174 })
        m.addItem (bpm, juce::String (bpm) + " BPM", true, std::abs (cur - (float) bpm) < 0.05f);
    m.showMenuAsync (menuAt (startStop.get()), [prm] (int r)
    {
        if (r <= 0) return;
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (prm->convertTo0to1 ((float) r));
        prm->endChangeGesture();
    });
}
