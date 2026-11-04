#include "PluginEditor.h"
#include "BinaryData.h"

using namespace unit808ui;

namespace
{
const float kPi = juce::MathConstants<float>::pi;
const juce::Colour kOrange { 0xfff47c39 }, kCream { 0xffd8d5c7 }, kPanel { 0xff303332 };

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
    setScrollWheelEnabled (true);
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

void Selector::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w)
{
    const int dir = (w.deltaY + w.deltaX) > 0 ? 1 : -1;
    const int n = juce::jlimit (0, ctrl.nDetents - 1, index + dir);
    if (n != index) att.setValueAsCompleteGesture ((float) n);
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
    if (led) { ledOff = image (led->a[0]); ledOnImg = image (led->a[1]); }
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
        const bool on = ledOn && ledOn();
        // the LED sits on the key cap and moves down with it
        drawAt (g, on ? ledOnImg : ledOff, led->a[on ? 1 : 0], o, juce::AffineTransform::translation (0, d ? 2.0f : 0.0f));
    }
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
        { "swing-fine", "swing-fine", "Fine swing: a little less or more swing", -135, 135 } };
    for (auto& k : kd)
    {
        const Ctrl* c = find (k.id);
        if (c == nullptr) continue;
        auto s = std::make_unique<Knob> (*c, k.minA, k.maxA);
        s->setTooltip (k.tip);
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
        s->setTooltip (tip);
        content.addAndMakeVisible (*s);
        selectors.push_back (std::move (s));
    }
    // switches and levers
    struct SD { const char* id; bool horiz; std::vector<int> map; const char* tip; };
    const SD sd[] = {
        { "lt-select", false, { 1, 0 }, "Low Tom (down) / Low Conga (up)" }, { "mt-select", false, { 1, 0 }, "Mid Tom (down) / Mid Conga (up)" },
        { "ht-select", false, { 1, 0 }, "Hi Tom (down) / Hi Conga (up)" }, { "rs-select", false, { 1, 0 }, "Rim Shot (down) / Claves (up)" },
        { "cp-select", false, { 1, 0 }, "Hand Clap (down) / Maracas (up)" },
        { "pre-scale", false, { 0, 1, 2, 3 }, "PRE-SCALE: 1 = 16th triplets, 2 = 32nd notes, 3 = 16th notes (normal), 4 = 8th triplets" },
        { "basic-variation", true, { 0, 1, 2 }, "BASIC VARIATION: A, A then B, or B" },
        { "if-variation", true, { 0, 1 }, "I/F VARIATION: which variation of the intro/fill pattern plays" },
        { "power", false, { 0, 1 }, "Power: off mutes the output" } };
    for (auto& s : sd)
    {
        auto w = std::make_unique<Switch> (*find (s.id), *proc.apvts.getParameter (s.id), s.horiz, s.map);
        w->setTooltip (s.tip);
        content.addAndMakeVisible (*w);
        switches.push_back (std::move (w));
    }
    // the 16 step keys, each with its LED
    for (int s = 0; s < 16; ++s)
    {
        const juce::String id = juce::String ("step-") + (s < 9 ? "0" : "") + juce::String (s + 1);
        auto k = std::make_unique<Key> (*find (id.toRawUTF8()), find ((id + "-led").toRawUTF8()));
        k->ledOn = [this, s] { return stepLedOn (s); };
        k->onPress = [this, s] (juce::ModifierKeys m)
        {
            const bool write = proc.getMode() == Unit808Processor::FirstPart || proc.getMode() == Unit808Processor::SecondPart;
            const bool stepNumber = m.isShiftDown() || proc.isStepNumberArmed();
            proc.stepKey (s, m);
            paintValue = (write && ! stepNumber) ? (proc.stepIsSet (s) ? 1 : 0) : -1;   // drag on to paint the same state
            paintedKeys = 1u << s;
            content.repaint();
        };
        k->onDrag = [this] (juce::Point<float> pos) { stepKeyDragTo (pos); };
        k->onRelease = [this] { paintValue = -1; paintedKeys = 0; };
        k->setTooltip (s < 12 ? "Step key. Manual Play: choose basic rhythm " + juce::String (s + 1) + ". 1st/2nd Part: set the step. Shift-click: step number (last step)"
                              : "Step key. Manual Play: choose intro/fill pattern " + juce::String (s - 11) + ". 1st/2nd Part: set the step");
        content.addAndMakeVisible (*k);
        stepKeys.push_back (std::move (k));
    }
    startStop = std::make_unique<Key> (*find ("start-stop"));
    startStop->heldDown = [this] { return proc.isTransportRunning(); };
    startStop->onPress = [this] (juce::ModifierKeys m) { if (m.isPopupMenu()) startStopMenu(); else proc.startStopKey(); };
    startStop->setTooltip (proc.isStandalone() ? "START/STOP. Right-click: tempo" : "START/STOP shows your DAW's transport: UNIT808 plays when the song plays");
    tap = std::make_unique<Key> (*find ("tap"));
    tap->onPress = [this] (juce::ModifierKeys) { proc.tapKey(); content.repaint(); };
    tap->setTooltip ("TAP: fill in trigger (plays the intro/fill next), tap-write in 1st/2nd Part, add the rhythm to the track in Compose");
    clear = std::make_unique<Key> (*find ("track-clear-led"));
    clear->heldDown = [this] { return proc.isStepNumberArmed(); };
    clear->onPress = [this] (juce::ModifierKeys m) { proc.clearKey (m); content.repaint(); };
    clear->setTooltip ("CLEAR. Pattern Clear: clears the rhythm (variation by the lever). Compose: clears the track. "
                       "1st/2nd Part: then a step key = step number; Alt-click clears the selected instrument");
    for (auto* k : { startStop.get(), tap.get(), clear.get() }) content.addAndMakeVisible (*k);

    auto led = [] (const char* id) { const Ctrl* c = find (id); return Led { c, image (c->a[0]), image (c->a[1]) }; };
    varA = led ("var-a-led"); varB = led ("var-b-led"); part1 = led ("part1-led"); part2 = led ("part2-led");

    content.addChildComponent (about);
    about.setBounds (0, 0, (int) kPanelW, (int) kPanelH);

    setResizable (true, true);
    setResizeLimits ((int) (kPanelW * 0.4f), (int) (kPanelH * 0.4f), (int) kPanelW, (int) kPanelH);
    getConstrainer()->setFixedAspectRatio ((double) kPanelW / kPanelH);
    setSize ((int) (kPanelW * 0.6f), (int) (kPanelH * 0.6f));
    startTimerHz (30);
}

Unit808Editor::~Unit808Editor() { stopTimer(); }

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

bool Unit808Editor::stepLedOn (int s) const
{
    if (! proc.isPowerOn()) return false;
    const int play = proc.getPlayStep();
    const bool blink = (juce::Time::getMillisecondCounter() / 180) % 2 == 0;
    switch (proc.getMode())
    {
        case Unit808Processor::FirstPart:
        case Unit808Processor::SecondPart:
        {
            const auto& part = proc.editedPart();
            const int len = part.length.load() == 0 ? 16 : part.length.load();
            if (s >= len) return false;
            const bool on = part.cell (proc.getInstrument(), s);
            const bool here = play >= 0 && proc.getPlaySlot() == proc.getSelectedSlot() && proc.getPlayPart() == proc.editPart();
            return (here && s == play) ? ! on : on;   // the playing step shows inverted
        }
        default:
        {
            if (s == play) return true;                                     // the running light
            if (s == proc.getPendingSlot()) return blink;                   // queued for the next measure
            if (s >= unit808::kFirstFill && s == proc.getSelectedFill()) return proc.isFillRequested() ? blink : true;
            return s == proc.getSelectedSlot();
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
    const bool blinking = proc.getPendingSlot() >= 0 || proc.isFillRequested();
    if (v != lastUi || blinking)
    {
        lastUi = v;
        for (auto& k : stepKeys) k->repaint();
        startStop->repaint(); clear->repaint();
        content.repaint (juce::Rectangle<int> (140, 830, 240, 150));
    }
}

void Unit808Editor::stepKeyDragTo (juce::Point<float> pos)
{
    if (paintValue < 0) return;
    for (int s = 0; s < (int) stepKeys.size(); ++s)
        if (stepKeys[(size_t) s]->getBounds().toFloat().contains (pos.x, stepKeys[(size_t) s]->getBounds().toFloat().getCentreY())
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
    m.addSectionHeader ("Tempo (standalone app)");
    auto* prm = proc.apvts.getParameter ("standalone-bpm");
    const float cur = prm->convertFrom0to1 (prm->getValue());
    for (int bpm : { 90, 100, 110, 115, 120, 125, 128, 130, 135, 140, 150, 160, 174 })
        m.addItem (bpm, juce::String (bpm) + " BPM", true, std::abs (cur - (float) bpm) < 0.05f);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (startStop.get()), [prm] (int r)
    {
        if (r <= 0) return;
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (prm->convertTo0to1 ((float) r));
        prm->endChangeGesture();
    });
}
