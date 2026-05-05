#include "PluginEditor.h"
#include <BinaryData.h>

#include <cmath>

namespace
{
    // Match common DAW labeling where MIDI note 60 is shown as C3.
    constexpr int kOctaveNumForMiddleC = 3;
}

//==============================================================================
// -- InfernoLookAndFeel --------------------------------------------------------

InfernoLookAndFeel::InfernoLookAndFeel()
{
    // Global colour overrides
    setColour(juce::Slider::backgroundColourId,        juce::Colour(0xff1a1a1f));
    setColour(juce::Slider::thumbColourId,             accentBright());
    setColour(juce::Slider::trackColourId,             accentRed());
    setColour(juce::Slider::rotarySliderFillColourId,  accentRed());
    setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xff2a2a30));
    setColour(juce::Label::textColourId,               textColour());
    setColour(juce::TextButton::buttonColourId,        juce::Colour(0xff1e1e24));
    setColour(juce::TextButton::buttonOnColourId,      accentRed());
    setColour(juce::TextButton::textColourOffId,       textColour());
    setColour(juce::TextButton::textColourOnId,        juce::Colours::white);
    setColour(juce::ComboBox::backgroundColourId,      juce::Colour(0xff1e1e24));
    setColour(juce::ComboBox::textColourId,            textColour());
    setColour(juce::ComboBox::outlineColourId,         juce::Colour(0xff333340));
}

void InfernoLookAndFeel::drawRotarySlider(juce::Graphics& g,
                                          int x, int y, int w, int h,
                                          float sliderPos,
                                          float startAngle, float endAngle,
                                          juce::Slider&)
{
    const float cx     = x + w * 0.5f;
    const float cy     = y + h * 0.5f;
    const float radius = juce::jmin(w, h) * 0.5f - 3.0f;

    // Track circle
    {
        juce::Path track;
        track.addCentredArc(cx, cy, radius, radius, 0.0f, startAngle, endAngle, true);
        g.setColour(juce::Colour(0xff2a2a33));
        g.strokePath(track, juce::PathStrokeType(3.0f, juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));
    }

    // Fill arc
    {
        const float angle = startAngle + sliderPos * (endAngle - startAngle);
        juce::Path fill;
        fill.addCentredArc(cx, cy, radius, radius, 0.0f, startAngle, angle, true);
        juce::ColourGradient grad(accentBright(), cx, cy - radius, accentRed(), cx, cy + radius, false);
        g.setGradientFill(grad);
        g.strokePath(fill, juce::PathStrokeType(3.0f, juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded));
    }

    // Centre dot
    g.setColour(juce::Colour(0xff1a1a1f));
    g.fillEllipse(cx - radius * 0.55f, cy - radius * 0.55f, radius * 1.1f, radius * 1.1f);

    // Pointer line
    {
        const float angle     = startAngle + sliderPos * (endAngle - startAngle);
        const float ptrLen    = radius * 0.6f;
        const float ptrThick  = 2.5f;
        const float x2 = cx + std::sin(angle) * ptrLen;
        const float y2 = cy - std::cos(angle) * ptrLen;
        g.setColour(accentBright());
        g.drawLine(cx, cy, x2, y2, ptrThick);
    }

    // Outer ring
    g.setColour(juce::Colour(0xff333340));
    g.drawEllipse(cx - radius - 2.0f, cy - radius - 2.0f,
                  (radius + 2.0f) * 2.0f, (radius + 2.0f) * 2.0f, 1.0f);
}

void InfernoLookAndFeel::drawButtonBackground(juce::Graphics& g,
                                               juce::Button& btn,
                                               const juce::Colour& backgroundColour,
                                               bool highlighted,
                                               bool down)
{
    const auto bounds = btn.getLocalBounds().toFloat().reduced(1.0f);
    const float corner = 4.0f;

    // Use the passed backgroundColour as base (set via buttonColourId on the button)
    const bool hasCustom = (backgroundColour.getAlpha() > 0
                            && backgroundColour != juce::Colour(0xff1e1e24));
    const juce::Colour base = hasCustom ? backgroundColour : juce::Colour(0xff1e1e24);

    // Respect per-button buttonOnColourId if set; fall back to accentRed
    const juce::Colour onColour = btn.findColour (juce::TextButton::buttonOnColourId);

    juce::Colour bg = btn.getToggleState() ? onColour
                    : down                 ? base.brighter(0.5f)
                    : highlighted          ? base.brighter(0.18f)
                                           : base;

    g.setColour(bg);
    g.fillRoundedRectangle(bounds, corner);

    const juce::Colour outline = btn.getToggleState() ? onColour.brighter(0.4f)
                                 : hasCustom          ? backgroundColour.brighter(0.4f)
                                                      : juce::Colour(0xff383840);
    g.setColour(outline);
    g.drawRoundedRectangle(bounds, corner, 1.0f);
}

void InfernoLookAndFeel::drawButtonText(juce::Graphics& g,
                                         juce::TextButton& btn,
                                         bool, bool)
{
    const juce::Colour col = btn.getToggleState()
                             ? juce::Colours::white
                             : textColour();
    g.setColour(col);
    g.setFont(juce::Font(juce::FontOptions("Arial", 11.0f, juce::Font::bold)));

    // For the BLEED button, draw the label left and a tick/cross icon on the right
    if (btn.getButtonText().startsWith ("BLEED"))
    {
        const auto b  = btn.getLocalBounds().toFloat();
        // Square icon: 8x8px fits neatly in a 20px-tall button
        const float iW = 8.0f;
        const float iX = b.getRight() - iW - 5.0f;
        const float cY = b.getCentreY();
        const float iT = cY - 4.0f;  // icon top
        const float iB = cY + 4.0f;  // icon bottom

        // Label left of icon
        g.drawFittedText ("BLEED",
                          btn.getLocalBounds().withTrimmedRight ((int)(iW + 8)),
                          juce::Justification::centred, 1);

        g.setColour (col);
        juce::PathStrokeType stroke (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);

        if (btn.getToggleState())
        {
            juce::Path tick;
            tick.startNewSubPath (iX,          cY + 1.5f);
            tick.lineTo          (iX + 2.5f,   iB);
            tick.lineTo          (iX + iW,     iT);
            g.strokePath (tick, stroke);
        }
        else
        {
            juce::Path cross;
            cross.startNewSubPath (iX,       iT);
            cross.lineTo          (iX + iW,  iB);
            cross.startNewSubPath (iX + iW,  iT);
            cross.lineTo          (iX,       iB);
            g.strokePath (cross, stroke);
        }
        return;
    }

    g.drawFittedText(btn.getButtonText(), btn.getLocalBounds(), juce::Justification::centred, 1);
}

juce::Font InfernoLookAndFeel::getLabelFont(juce::Label&)
{
    return juce::Font(juce::FontOptions("Arial", 10.5f, juce::Font::plain));
}

void InfernoLookAndFeel::drawComboBox(juce::Graphics& g, int w, int h,
                                       bool, int bx, int by, int bw, int bh,
                                       juce::ComboBox& box)
{
    const auto bounds = juce::Rectangle<float>(0, 0, (float)w, (float)h);
    g.setColour(box.findColour (juce::ComboBox::backgroundColourId));
    g.fillRoundedRectangle(bounds, 4.0f);
    g.setColour(juce::Colour(0xff383840));
    g.drawRoundedRectangle(bounds, 4.0f, 1.0f);

    // Arrow
    juce::Path arrow;
    const float ax = bx + bw * 0.5f;
    const float ay = by + bh * 0.5f;
    arrow.addTriangle(ax - 4.0f, ay - 2.0f, ax + 4.0f, ay - 2.0f, ax, ay + 3.0f);
    g.setColour(InfernoLookAndFeel::dimText());
    g.fillPath(arrow);
    (void) box;
}

//==============================================================================
// WaveformDisplay

void WaveformDisplay::loadFrom (const juce::AudioBuffer<float>* buf)
{
    peaks.clear();
    if (!buf || buf->getNumSamples() == 0) { repaint(); return; }

    const int numSamples = buf->getNumSamples();
    const int numCols    = MAX_PEAKS;
    peaks.resize ((size_t) numCols);

    const float* ch0 = buf->getReadPointer (0);
    const float* ch1 = (buf->getNumChannels() > 1) ? buf->getReadPointer (1) : nullptr;

    float globalPeak = 0.f;
    for (int col = 0; col < numCols; ++col)
    {
        const int start = (int) ((int64_t) col       * numSamples / numCols);
        const int end   = (int) ((int64_t) (col + 1) * numSamples / numCols);
        float pk = 0.f;
        for (int s = start; s < end; ++s)
        {
            pk = juce::jmax (pk, std::abs (ch0[s]));
            if (ch1) pk = juce::jmax (pk, std::abs (ch1[s]));
        }
        peaks[(size_t) col] = pk;
        globalPeak = juce::jmax (globalPeak, pk);
    }
    if (globalPeak > 1e-6f)
        for (auto& p : peaks) p /= globalPeak;
    repaint();
}

void WaveformDisplay::setTrimRange (float startNorm, float endNorm)
{
    trimStartNorm = juce::jlimit (0.0f, 1.0f, startNorm);
    trimEndNorm   = juce::jlimit (0.0f, 1.0f, endNorm);
    if (trimEndNorm < trimStartNorm + 0.001f)
        trimEndNorm = juce::jmin (1.0f, trimStartNorm + 0.001f);
    repaint();
}

void WaveformDisplay::paint (juce::Graphics& g)
{
    const auto  bounds = getLocalBounds();
    const float fw = (float) bounds.getWidth();
    const float fh = (float) bounds.getHeight();
    const float cx = (float) bounds.getX();
    const float cy = (float) bounds.getY() + fh * 0.5f;

    g.setColour (juce::Colour (0xff06080f));
    g.fillRoundedRectangle (bounds.toFloat(), 4.f);
    g.setColour (juce::Colour (0xff1a1a2c));
    g.drawRoundedRectangle (bounds.toFloat().reduced (0.5f), 4.f, 1.f);

    if (peaks.empty())
    {
        g.setColour (InfernoLookAndFeel::dimText().withAlpha (0.4f));
        g.setFont (juce::Font (juce::FontOptions ("Arial", 9.f, juce::Font::plain)));
        g.drawText ("NO SAMPLE", bounds, juce::Justification::centred, false);
        return;
    }

    g.setColour (juce::Colour (0xff252535));
    g.drawHorizontalLine ((int) cy, cx, cx + fw);

    const int   n    = (int) peaks.size();
    const float colW = fw / (float) n;

    // Temporary drag zoom window (for precise trim handle placement)
    const float viewSpan  = dragZoomActive ? dragZoomViewSpanNorm : 1.0f;
    const float viewStart = dragZoomActive ? dragZoomViewStartNorm : 0.0f;

    auto normToX = [&] (float norm) -> float
    {
        if (!dragZoomActive)
            return cx + juce::jlimit (0.0f, 1.0f, norm) * fw;

        const float t = (norm - viewStart) / viewSpan;
        return cx + t * fw;
    };
    juce::Path  wfPath;
    for (int i = 0; i < n; ++i)
    {
        const float u     = (n > 1) ? ((float) i / (float) (n - 1)) : 0.0f;
        const float norm  = viewStart + u * viewSpan;
        const int srcIdx  = juce::jlimit (0, n - 1, (int) std::round (norm * (float) (n - 1)));
        const float pk    = peaks[(size_t) srcIdx];
        const float x     = cx + (float) i * colW;
        const float halfH = pk * (fh * 0.5f - 1.f);
        if (halfH < 0.5f) continue;
        wfPath.addRectangle (x, cy - halfH, juce::jmax (1.f, colW - 0.5f), halfH * 2.f);
    }
    g.setGradientFill (juce::ColourGradient (
        juce::Colour (0xffdd2200), cx + fw * 0.5f, cy,
        juce::Colour (0xff661100).withAlpha (0.6f), cx, cy, true));
    g.fillPath (wfPath);
    g.setColour (juce::Colour (0xffff4422).withAlpha (0.45f));
    g.strokePath (wfPath, juce::PathStrokeType (0.5f));

    // Trim region overlay + draggable handles
    const float sx = normToX (trimStartNorm);
    const float ex = normToX (trimEndNorm);

    g.setColour (juce::Colours::black.withAlpha (0.38f));
    g.fillRect (juce::Rectangle<float> (cx, bounds.getY(), juce::jmax (0.0f, sx - cx), fh));
    g.fillRect (juce::Rectangle<float> (juce::jmax (cx, ex), bounds.getY(), juce::jmax (0.0f, (cx + fw) - ex), fh));

    const float topY = (float) bounds.getY();
    const float bottomY = (float) bounds.getBottom();
    const float r = HANDLE_RADIUS;

    g.setColour (InfernoLookAndFeel::accentBright().withAlpha (0.85f));
    g.drawLine (sx, topY + r * 2.0f, sx, bottomY, 2.0f);
    g.drawLine (ex, topY + r * 2.0f, ex, bottomY, 2.0f);

    // Grab handles (top circles) cap the trim lines (no line poke-through)
    g.setColour (InfernoLookAndFeel::accentBright());
    g.fillEllipse (sx - r, topY, r * 2.0f, r * 2.0f);
    g.fillEllipse (ex - r, topY, r * 2.0f, r * 2.0f);
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.drawEllipse (sx - r, topY, r * 2.0f, r * 2.0f, 1.0f);
    g.drawEllipse (ex - r, topY, r * 2.0f, r * 2.0f, 1.0f);
}

void WaveformDisplay::mouseDown (const juce::MouseEvent& e)
{
    const auto b = getLocalBounds().toFloat();
    const float x = e.position.x;
    const float sx = b.getX() + trimStartNorm * b.getWidth();
    const float ex = b.getX() + trimEndNorm   * b.getWidth();

    const float ds = std::abs (x - sx);
    const float de = std::abs (x - ex);

    // Only start drag if click is near one of the handles/lines.
    if (juce::jmin (ds, de) > HANDLE_HIT_RADIUS)
    {
        dragHandle = DragHandle::None;
        return;
    }

    dragHandle = (ds <= de) ? DragHandle::Start : DragHandle::End;

    // Stable zoom window for this drag (prevents jitter/shifting under cursor).
    dragZoomActive = true;
    dragZoomViewSpanNorm = 1.0f / DRAG_ZOOM_FACTOR;
    const float focusNorm = (dragHandle == DragHandle::Start) ? trimStartNorm : trimEndNorm;
    dragZoomViewStartNorm = juce::jlimit (0.0f, 1.0f - dragZoomViewSpanNorm,
                                          focusNorm - dragZoomViewSpanNorm * 0.5f);
    mouseDrag (e);
}

void WaveformDisplay::mouseDrag (const juce::MouseEvent& e)
{
    if (dragHandle == DragHandle::None) return;

    const auto b = getLocalBounds().toFloat();
    if (b.getWidth() <= 1.0f) return;

    float norm = 0.0f;
    if (dragZoomActive)
    {
        const float t = juce::jlimit (0.0f, 1.0f, (e.position.x - b.getX()) / b.getWidth());
        norm = juce::jlimit (0.0f, 1.0f, dragZoomViewStartNorm + t * dragZoomViewSpanNorm);
    }
    else
    {
        norm = juce::jlimit (0.0f, 1.0f, (e.position.x - b.getX()) / b.getWidth());
    }

    if (dragHandle == DragHandle::Start)
    {
        trimStartNorm = juce::jmin (norm, trimEndNorm - 0.001f);
    }
    else
    {
        trimEndNorm = juce::jmax (norm, trimStartNorm + 0.001f);
    }

    repaint();
    if (onTrimChanged) onTrimChanged (trimStartNorm, trimEndNorm);
}

void WaveformDisplay::mouseUp (const juce::MouseEvent&)
{
    dragHandle = DragHandle::None;
    dragZoomActive = false;
    dragZoomViewStartNorm = 0.0f;
    dragZoomViewSpanNorm  = 1.0f;
    repaint();
}


//==============================================================================
// -- TrackRow -----------------------------------------------------------------

static void setupSmallKnob (juce::Slider& s, InfernoLookAndFeel& laf)
{
    s.setLookAndFeel (&laf);
    s.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    s.setPopupDisplayEnabled (true, false, nullptr);
}

TrackRow::TrackRow (int idx, DeathDealerDrumsAudioProcessor& p, InfernoLookAndFeel& l)
    : slotIndex (idx), proc (p), laf (l)
{
    // Name label � double-click to rename
    nameLabel.setFont (juce::Font (juce::FontOptions ("Arial", 11.0f, juce::Font::bold)));
    nameLabel.setColour (juce::Label::textColourId, InfernoLookAndFeel::textColour());
    nameLabel.setJustificationType (juce::Justification::centredLeft);
    nameLabel.setEditable (false, true, false);
    nameLabel.onTextChange = [this]
    {
        proc.renameTrack (slotIndex, nameLabel.getText());
    };
    addAndMakeVisible (nameLabel);

    // MIDI note label ? double-click to type a note name or number
    midiLabel.setFont (juce::Font (juce::FontOptions ("Arial", 9.5f, juce::Font::bold)));
    midiLabel.setColour (juce::Label::textColourId, InfernoLookAndFeel::dimText());
    midiLabel.setColour (juce::Label::backgroundWhenEditingColourId, juce::Colour (0xff1a1a24));
    midiLabel.setColour (juce::Label::textWhenEditingColourId, InfernoLookAndFeel::textColour());
    midiLabel.setJustificationType (juce::Justification::centred);
    midiLabel.setEditable (false, true, false);
    midiLabel.onTextChange = [this]
    {
        const juce::String txt = midiLabel.getText().trim();
        int noteNum = -1;

        // Try parsing as integer first
        if (txt.containsOnly ("0123456789"))
        {
            noteNum = txt.getIntValue();
        }
        else
        {
            // Try as note name e.g. "C3", "D#4", "Eb2"
            for (int n = 0; n < 128; ++n)
            {
                if (juce::MidiMessage::getMidiNoteName (n, true, true, kOctaveNumForMiddleC)
                        .equalsIgnoreCase (txt))
                { noteNum = n; break; }
            }
        }

        noteNum = juce::jlimit (0, 127, noteNum < 0 ? 60 : noteNum);
        if (auto* p = proc.getAPVTS().getParameter (
                DeathDealerDrumsAudioProcessor::trackParamID (slotIndex, "midi_note")))
            p->setValueNotifyingHost (p->convertTo0to1 ((float) noteNum));
        updateMidiLabel();
    };
    addAndMakeVisible (midiLabel);

    // LOAD button
    loadBtn.setLookAndFeel (&laf);
    loadBtn.addListener (this);
    loadBtn.setTooltip ("Load samples for this track");
    addAndMakeVisible (loadBtn);

    // Remove button
    removeBtn.setLookAndFeel (&laf);
    removeBtn.addListener (this);
    removeBtn.setTooltip ("Remove this track from the kit");
    addAndMakeVisible (removeBtn);

    // Knobs
    setupSmallKnob (volKnob, laf);
    setupSmallKnob (panKnob, laf);
    volKnob.setTooltip ("Volume");
    panKnob.setTooltip ("Pan");

    panKnob.textFromValueFunction = [] (double v) -> juce::String
    {
        const int pct = juce::roundToInt (std::abs (v) * 100.0);
        if (pct == 0) return "CENTER";
        return juce::String (v < 0.0 ? "L " : "R ") + juce::String (pct);
    };
    panKnob.valueFromTextFunction = [] (const juce::String& t) -> double
    {
        const juce::String s = t.trim();
        if (s.startsWithIgnoreCase ("C")) return 0.0;
        const bool left = s.startsWithIgnoreCase ("L");
        const double pct = s.fromFirstOccurrenceOf (" ", false, false).getDoubleValue();
        return left ? -(pct / 100.0) : (pct / 100.0);
    };
    addAndMakeVisible (volKnob);
    addAndMakeVisible (panKnob);

    for (auto* lb : { &volLabel, &panLabel })
    {
        lb->setFont (juce::Font (juce::FontOptions ("Arial", 9.0f, juce::Font::plain)));
        lb->setColour (juce::Label::textColourId, InfernoLookAndFeel::dimText());
        lb->setJustificationType (juce::Justification::centred);
        addAndMakeVisible (lb);
    }
    volLabel.setText ("VOL", juce::dontSendNotification);
    panLabel.setText ("PAN", juce::dontSendNotification);

    // File label
    fileLabel.setFont (juce::Font (juce::FontOptions ("Arial", 9.0f, juce::Font::italic)));
    fileLabel.setColour (juce::Label::textColourId, InfernoLookAndFeel::dimText());
    fileLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (fileLabel);

    // APVTS attachments
    volAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        proc.getAPVTS(), DeathDealerDrumsAudioProcessor::trackParamID (slotIndex, "volume"), volKnob);
    panAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        proc.getAPVTS(), DeathDealerDrumsAudioProcessor::trackParamID (slotIndex, "pan"), panKnob);

    // Solo / Mute buttons
    for (auto* btn : { &soloBtn, &muteBtn })
    {
        btn->setLookAndFeel (&laf);
        btn->setClickingTogglesState (true);
        addAndMakeVisible (btn);
    }
    soloBtn.setTooltip ("Solo this track � all other tracks are silenced");
    muteBtn.setTooltip ("Mute this track");
    soloBtn.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xffcc2200));
    muteBtn.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xffcc2200));
    soloAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        proc.getAPVTS(), DeathDealerDrumsAudioProcessor::trackParamID (slotIndex, "solo"), soloBtn);
    muteAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        proc.getAPVTS(), DeathDealerDrumsAudioProcessor::trackParamID (slotIndex, "mute"), muteBtn);

    // Enforce mutual exclusivity: a track cannot be SOLO and MUTE at the same time.
    soloBtn.onClick = [this]
    {
        if (!soloBtn.getToggleState())
            return;

        if (auto* muteParam = proc.getAPVTS().getParameter (
                DeathDealerDrumsAudioProcessor::trackParamID (slotIndex, "mute")))
            muteParam->setValueNotifyingHost (0.0f);
    };

    muteBtn.onClick = [this]
    {
        if (!muteBtn.getToggleState())
            return;

        if (auto* soloParam = proc.getAPVTS().getParameter (
                DeathDealerDrumsAudioProcessor::trackParamID (slotIndex, "solo")))
            soloParam->setValueNotifyingHost (0.0f);
    };

    // Phase invert button
    phaseBtn.setLookAndFeel (&laf);
    phaseBtn.setClickingTogglesState (true);
    phaseBtn.setTooltip ("Phase Invert");
    phaseBtn.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff0055cc));
    addAndMakeVisible (phaseBtn);
    phaseAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        proc.getAPVTS(), DeathDealerDrumsAudioProcessor::trackParamID (slotIndex, "phase"), phaseBtn);

    // Mic-bleed enable checkbox
    bleedBtn.setLookAndFeel (&laf);
    bleedBtn.setClickingTogglesState (true);
    bleedBtn.setTooltip ("Enable mic bleed for this track");
    bleedBtn.setColour (juce::TextButton::buttonColourId,   juce::Colour (0xffcc2200)); // OFF = red
    bleedBtn.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff00aa44)); // ON  = green
    bleedBtn.setButtonText ("BLEED");
    bleedBtn.onStateChange = [this] { bleedBtn.repaint(); };
    addAndMakeVisible (bleedBtn);
    bleedAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        proc.getAPVTS(), DeathDealerDrumsAudioProcessor::trackParamID (slotIndex, "bleed_enable"), bleedBtn);

    // Bleed send knob (how much this track contributes to the bleed bus)
    setupSmallKnob (bleedSendKnob, laf);
    bleedSendKnob.setTooltip ("Bleed Send Level");
    addAndMakeVisible (bleedSendKnob);
    bleedSendAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        proc.getAPVTS(), DeathDealerDrumsAudioProcessor::trackParamID (slotIndex, "bleed_send"), bleedSendKnob);

    // Output combo
    outputCombo.addItem ("OUT 1  (MAIN)", 1);
    for (int i = 2; i <= 16; ++i)
        outputCombo.addItem ("OUT " + juce::String (i), i);
    outputCombo.setSelectedId (1, juce::dontSendNotification);
    outputCombo.setLookAndFeel (&laf);
    outputCombo.onChange = [this]
    {
        const int val = outputCombo.getSelectedId() - 1;
        if (auto* p = proc.getAPVTS().getParameter (
                DeathDealerDrumsAudioProcessor::trackParamID (slotIndex, "output")))
            p->setValueNotifyingHost (p->convertTo0to1 ((float) val));
    };
    outputCombo.setTooltip ("Route this track to a specific audio output channel");
    addAndMakeVisible (outputCombo);

    // Output mode combo
    outputModeCombo.addItem ("STEREO", 1);
    outputModeCombo.addItem ("MONO",   2);
    outputModeCombo.setSelectedId (1, juce::dontSendNotification);
    outputModeCombo.setLookAndFeel (&laf);
    outputModeCombo.onChange = [this]
    {
        const int val = outputModeCombo.getSelectedId() - 1;
        if (auto* p = proc.getAPVTS().getParameter (
                DeathDealerDrumsAudioProcessor::trackParamID (slotIndex, "output_mode")))
            p->setValueNotifyingHost (p->convertTo0to1 ((float) val));
    };
    outputModeCombo.setTooltip ("Set this track's output to stereo or mono");
    addAndMakeVisible (outputModeCombo);

    // Slave-link routing (same-note phase/humanization lock)
    slaveCombo.setLookAndFeel (&laf);
    slaveCombo.setTooltip ("Lock/Unlock same-note layer timing");
    slaveCombo.setColour (juce::ComboBox::textColourId, InfernoLookAndFeel::textColour());
    slaveCombo.setColour (juce::ComboBox::outlineColourId, juce::Colour (0xff383840));
    slaveCombo.onChange = [this]
    {
        if (updatingSlaveCombo) return;
        const int target = slaveCombo.getSelectedId() - 2; // id1=FREE, id2=track0
        if (auto* p = proc.getAPVTS().getParameter (
                DeathDealerDrumsAudioProcessor::trackParamID (slotIndex, "slave_to")))
            p->setValueNotifyingHost (p->convertTo0to1 ((float) target));
        updateSlaveComboAppearance();
    };
    addAndMakeVisible (slaveCombo);

    // Ensure clicks on any visible row child still select this row.
    for (int i = 0; i < getNumChildComponents(); ++i)
        if (auto* child = getChildComponent (i))
            child->addMouseListener (this, true);

    refresh();
}

TrackRow::~TrackRow()
{
    for (int i = 0; i < getNumChildComponents(); ++i)
        if (auto* child = getChildComponent (i))
            child->removeMouseListener (this);

    volAtt.reset(); panAtt.reset(); bleedSendAtt.reset(); soloAtt.reset(); muteAtt.reset(); phaseAtt.reset(); bleedAtt.reset();
    loadBtn.removeListener (this);
    removeBtn.removeListener (this);
    loadBtn.setLookAndFeel (nullptr);
    removeBtn.setLookAndFeel (nullptr);
    volKnob.setLookAndFeel (nullptr);
    panKnob.setLookAndFeel (nullptr);
    soloBtn.setLookAndFeel (nullptr);
    muteBtn.setLookAndFeel (nullptr);
    phaseBtn.setLookAndFeel (nullptr);
    bleedBtn.setLookAndFeel (nullptr);
    bleedSendKnob.setLookAndFeel (nullptr);
    outputCombo.setLookAndFeel (nullptr);
    outputModeCombo.setLookAndFeel (nullptr);
    slaveCombo.setLookAndFeel (nullptr);
}

void TrackRow::rebuildSlaveCombo ()
{
    updatingSlaveCombo = true;
    slaveCombo.clear (juce::dontSendNotification);
    slaveCombo.addItem ("UNLOCK", 1);

    const int n = proc.getNumActiveTracks();
    for (int i = 0; i < n; ++i)
    {
        if (i == slotIndex) continue;
        const auto* t = proc.getTrack (i);
        const juce::String name = t ? t->getName() : ("TRACK " + juce::String (i + 1));
        slaveCombo.addItem ("LOCK/" + name, i + 2);
    }

    int target = -1;
    if (auto* raw = proc.getAPVTS().getRawParameterValue (
            DeathDealerDrumsAudioProcessor::trackParamID (slotIndex, "slave_to")))
        target = juce::roundToInt (raw->load());

    if (target == slotIndex)
        target = -1;
    if (target < 0 || target >= n)
        slaveCombo.setSelectedId (1, juce::dontSendNotification);
    else
        slaveCombo.setSelectedId (target + 2, juce::dontSendNotification);

    updatingSlaveCombo = false;
    updateSlaveComboAppearance();
}

void TrackRow::updateSlaveComboAppearance ()
{
    const bool linked = (slaveCombo.getSelectedId() > 1);
    slaveCombo.setColour (juce::ComboBox::backgroundColourId,
        linked ? juce::Colour (0xff12301f) : juce::Colour (0xff1e1e24));
    slaveCombo.repaint();
}

void TrackRow::refresh ()
{
    DrumTrack* t = proc.getTrack (slotIndex);
    if (t)
    {
        nameLabel.setText (t->getName(), juce::dontSendNotification);
        const juce::String path = t->getSamplePath();
        if (path.isNotEmpty())
        {
            fileLabel.setText (juce::File (path).getFileNameWithoutExtension(),
                               juce::dontSendNotification);
            fileLabel.setColour (juce::Label::textColourId, InfernoLookAndFeel::accentBright());
        }
        else
        {
            fileLabel.setText ("(no sample)", juce::dontSendNotification);
            fileLabel.setColour (juce::Label::textColourId, InfernoLookAndFeel::dimText());
        }
    }

    // Sync output bus and output mode combos from current APVTS state so they
    // correctly reflect the parameter after preset load or editor reopen.
    {
        const int outVal  = juce::roundToInt (
            proc.getAPVTS().getRawParameterValue (
                DeathDealerDrumsAudioProcessor::trackParamID (slotIndex, "output"))->load());
        outputCombo.setSelectedId (outVal + 1, juce::dontSendNotification);
    }
    {
        const int modeVal = juce::roundToInt (
            proc.getAPVTS().getRawParameterValue (
                DeathDealerDrumsAudioProcessor::trackParamID (slotIndex, "output_mode"))->load());
        outputModeCombo.setSelectedId (modeVal + 1, juce::dontSendNotification);
    }

    updateMidiLabel();
    rebuildSlaveCombo();
}

void TrackRow::updateMidiLabel ()
{
    const float note = *proc.getAPVTS().getRawParameterValue (
        DeathDealerDrumsAudioProcessor::trackParamID (slotIndex, "midi_note"));
    const int noteInt = juce::roundToInt (note);
    const juce::String noteName = juce::MidiMessage::getMidiNoteName (noteInt, true, true, kOctaveNumForMiddleC);
    midiLabel.setText (noteName, juce::dontSendNotification);
}

void TrackRow::updateMeter (DrumEngine* engine)
{
    if (!engine || slotIndex >= DrumEngine::MAX_TRACKS) return;
    const float raw  = engine->trackPeakLin[slotIndex].load();
    // UI-side peak hold with faster decay (0.78 per 20Hz tick ? -14 dB/sec display fallback)
    meterLevel = juce::jmax (raw, meterLevel * 0.50f);
    repaint();
}

void TrackRow::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();

    // Background
    const juce::Colour bg = selected ? juce::Colour (0xff1c1c26) : InfernoLookAndFeel::panelBg();
    g.setColour (bg);
    g.fillRoundedRectangle (b.reduced (1.0f), 4.0f);

    // Border
    const juce::Colour border = selected ? InfernoLookAndFeel::accentRed()
                                          : juce::Colour (0xff282830);
    g.setColour (border);
    g.drawRoundedRectangle (b.reduced (1.0f), 4.0f, selected ? 1.5f : 0.5f);

    // Left accent bar when selected
    if (selected)
    {
        g.setColour (InfernoLookAndFeel::accentBright());
        g.fillRoundedRectangle (b.withWidth (4.0f), 2.0f);
    }

    // Volume meter ? sits left of the X button, skips the top row so it doesn't overlap
    {
        constexpr float minDb = -60.0f, maxDb = 0.0f;
        const float db = (meterLevel > 1e-7f)
                           ? juce::jlimit (minDb, maxDb, juce::Decibels::gainToDecibels (meterLevel))
                           : minDb;
        const float norm = (db - minDb) / (maxDb - minDb); // 0 = silent, 1 = 0 dBFS

        // Start below the top row (name/X/LOAD) ? approx y=34
        const int mW = 6;
        const int mX = getWidth() - mW - 2; // just inside right edge, clears X button
        const int mY = 34;
        const int mH = getHeight() - mY - 6;

        // Track background
        g.setColour (juce::Colour (0xff111118));
        g.fillRect (mX, mY, mW, mH);

        if (norm > 0.0f)
        {
            const int fillH = juce::roundToInt (norm * (float) mH);
            const int fillY = mY + mH - fillH;

            // Colour: green ? yellow (above -12 dB) ? red (above -3 dB)
            juce::Colour col;
            if (db > -3.0f)        col = juce::Colour (0xffff2200);
            else if (db > -12.0f)  col = juce::Colour (0xffffff00);
            else                   col = juce::Colour (0xff00cc44);

            g.setColour (col);
            g.fillRect (mX, fillY, mW, fillH);
        }

        // dB label just above the meter track
        if (db > -60.0f)
        {
            g.setColour (InfernoLookAndFeel::dimText().withAlpha (0.7f));
            g.setFont (juce::Font (juce::FontOptions ("Arial", 7.5f, juce::Font::plain)));
            g.drawText (juce::String (juce::roundToInt (db)) + "dB",
                        mX - 20, mY - 1, 22, 9, juce::Justification::centredRight, false);
        }
    }
}

void TrackRow::resized ()
{
    auto r = getLocalBounds().reduced (6, 4);
    constexpr int meterReserve = 26; // keep right side clear for meter + dB text

    // Top row: name | midi | LOAD | REMOVE
    auto top = r.removeFromTop (26);
    nameLabel.setBounds (top.removeFromLeft  (120));
    top.removeFromLeft (4);
    midiLabel.setBounds (top.removeFromLeft  (52));
    top.removeFromLeft (4);
    removeBtn.setBounds (top.removeFromRight (22));
    top.removeFromRight (2);
    loadBtn.setBounds   (top.removeFromRight (50));

    r.removeFromTop (2);

    // Middle row: VOL label+knob  PAN label+knob  file label
    auto mid = r.removeFromTop (28);
    const int knobW = 36;
    const int lblW  = 24;

    volLabel.setBounds  (mid.removeFromLeft (lblW));
    volKnob.setBounds   (mid.removeFromLeft (knobW));
    mid.removeFromLeft (4);
    panLabel.setBounds  (mid.removeFromLeft (lblW));
    panKnob.setBounds   (mid.removeFromLeft (knobW));
    mid.removeFromLeft (8);
    fileLabel.setBounds (mid);

    r.removeFromTop (2);

    // Bottom row 1 (performance controls): SOLO | MUTE | � | BLEED | BLEED SEND
    auto bot1 = r.removeFromTop (20);
    auto bot1Safe = bot1.withTrimmedRight (meterReserve);
    soloBtn.setBounds         (bot1Safe.removeFromLeft (28));
    bot1Safe.removeFromLeft (3);
    muteBtn.setBounds         (bot1Safe.removeFromLeft (28));
    bot1Safe.removeFromLeft (3);
    phaseBtn.setBounds        (bot1Safe.removeFromLeft (24));
    bot1Safe.removeFromLeft (4);
    bleedBtn.setBounds        (bot1Safe.removeFromLeft (64));
    bot1Safe.removeFromLeft (4);
    bleedSendKnob.setBounds   (bot1Safe.removeFromLeft (28));

    r.removeFromTop (2);

    // Bottom row 2 (routing/lock): OUTPUT | MODE | PHASE LOCK
    auto bot2 = r.removeFromTop (20);
    auto bot2Safe = bot2.withTrimmedRight (meterReserve);
    const int outputW    = 88;
    const int outputModeW= 62;
    const int gap        = 4;
    const int fixedW     = outputW + gap + outputModeW + gap;
    const int slaveW     = juce::jmax (80, bot2Safe.getWidth() - fixedW);

    outputCombo.setBounds   (bot2Safe.removeFromLeft (outputW));
    bot2Safe.removeFromLeft (gap);
    outputModeCombo.setBounds (bot2Safe.removeFromLeft (outputModeW));
    bot2Safe.removeFromLeft (gap);
    slaveCombo.setBounds    (bot2Safe.removeFromLeft (slaveW));
}

void TrackRow::mouseDown (const juce::MouseEvent&)
{
    if (onSelected) onSelected (slotIndex);
}

void TrackRow::buttonClicked (juce::Button* btn)
{
    if (btn == &removeBtn)
    {
        if (onRemove) onRemove (slotIndex);
        return;
    }

    if (btn == &loadBtn)
    {
        DrumTrack* t = proc.getTrack (slotIndex);
        const juce::String title = "Load Sample"
            + (t ? " \u2014 " + t->getName() : "");

        juce::FileChooser chooser (title,
            juce::File::getSpecialLocation (juce::File::userMusicDirectory),
            "*.wav;*.aif;*.aiff;*.flac;*.ogg");

        if (chooser.browseForFileToOpen())
        {
            proc.loadSampleForTrack (slotIndex, chooser.getResult());
            fileLabel.setText ("Loading\xe2\x80\xa6", juce::dontSendNotification);
        }
    }
}

//==============================================================================
// -- TrackDetailPanel ---------------------------------------------------------

static void setupDetailKnob (juce::Slider& s, InfernoLookAndFeel& laf)
{
    s.setLookAndFeel (&laf);
    s.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 55, 14);
    s.setPopupDisplayEnabled (false, false, nullptr);
    s.setColour (juce::Slider::textBoxTextColourId,       InfernoLookAndFeel::dimText());
    s.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    s.setColour (juce::Slider::textBoxOutlineColourId,    juce::Colours::transparentBlack);
}

static void setupDetailLabel (juce::Label& l)
{
    l.setFont (juce::Font (juce::FontOptions ("Arial", 10.0f, juce::Font::bold)));
    l.setColour (juce::Label::textColourId, InfernoLookAndFeel::dimText());
    l.setJustificationType (juce::Justification::centred);
}

//==============================================================================
// SpectrumDisplay
//==============================================================================
void SpectrumDisplay::setData (const float* bins, int numBins, float sampleRateHz)
{
    sr = sampleRateHz;
    magnitudesDb.resize ((size_t) numBins);
    for (int i = 0; i < numBins; ++i)
    {
        const float mag = bins[i] > 0.f ? bins[i] : 1e-10f;
        magnitudesDb[(size_t) i] = juce::jmax (-90.f, juce::Decibels::gainToDecibels (mag));
    }
    repaint();
}

void SpectrumDisplay::setEQCoeffs (const BiquadCoeffs* c8, const float* f8,
                                    const float* g8, const int* p8, float sampleRateHz)
{
    for (int b = 0; b < 8; ++b) { eqBands[b] = c8[b]; eqFreqs[b] = f8[b]; eqGains[b] = g8[b]; eqPasses[b] = p8[b]; }
    eqSr  = sampleRateHz;
    hasEQ = true;
    repaint();
}

void SpectrumDisplay::paint (juce::Graphics& g)
{
    const auto  bounds = getLocalBounds();
    const int   w = bounds.getWidth(),  h = bounds.getHeight();
    const float fw = (float) w,         fh = (float) h;

    // -- Background gradient -------------------------------------------
    g.setGradientFill (juce::ColourGradient (
        juce::Colour (0xff06080f), 0.f, 0.f,
        juce::Colour (0xff0a0c14), 0.f, fh, false));
    g.fillRoundedRectangle (bounds.toFloat(), 4.f);

    // -- Helpers -------------------------------------------------------
    const float dBTop    =  24.f;
    const float dBBottom = -24.f;
    const float dBRange  = dBTop - dBBottom;
    const float logMin   = std::log10 (20.f);
    const float logRange = std::log10 (20000.f) - logMin;

    auto dBToY = [&] (float db) -> float
    {
        return juce::jlimit (0.f, fh, (dBTop - db) / dBRange * fh);
    };
    auto freqToX = [&] (float f) -> float
    {
        return juce::jlimit (0.f, fw, (std::log10 (f) - logMin) / logRange * fw);
    };

    // -- dB grid -------------------------------------------------------
    static const float dBGrid[] = { 24.f, 18.f, 12.f, 6.f, 0.f, -6.f, -12.f, -18.f, -24.f };
    for (float db : dBGrid)
    {
        const float y   = dBToY (db);
        const bool zero = (db == 0.f);
        g.setColour (zero ? juce::Colour (0xff2a2a44) : juce::Colour (0xff131320));
        g.drawHorizontalLine ((int) y, 0.f, fw);
    }

    // -- Frequency grid ------------------------------------------------
    static const int freqGrid[] = { 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000 };
    g.setColour (juce::Colour (0xff131320));
    for (int f : freqGrid)
        g.drawVerticalLine ((int) freqToX ((float) f), 0.f, fh);

    // -- Spectrum fill -------------------------------------------------
    if (!magnitudesDb.empty())
    {
        juce::Path specLine;
        const int   n   = (int) magnitudesDb.size();
        const float nyq = sr * 0.5f;
        bool started = false;

        for (int px = 0; px < w; ++px)
        {
            const float freq = 20.f * std::pow (1000.f, (float) px / fw);
            const int   bin  = juce::jlimit (0, n - 1, (int) (freq / nyq * (float) n));
            const float y    = dBToY (magnitudesDb[(size_t) bin]);

            if (!started) { specLine.startNewSubPath ((float) px, y); started = true; }
            else          specLine.lineTo ((float) px, y);
        }

        juce::Path fill = specLine;
        fill.lineTo (fw, fh);
        fill.lineTo (0.f, fh);
        fill.closeSubPath();

        g.setGradientFill (juce::ColourGradient (
            InfernoLookAndFeel::accentRed().withAlpha (0.55f), 0.f, 0.f,
            InfernoLookAndFeel::accentRed().withAlpha (0.04f), 0.f, fh, false));
        g.fillPath (fill);

        g.setColour (InfernoLookAndFeel::accentBright().withAlpha (0.80f));
        g.strokePath (specLine, juce::PathStrokeType (1.4f));
    }

    // -- EQ curve overlay ----------------------------------------------
    if (hasEQ)
    {
        // Combined EQ magnitude response
        juce::Path eqPath;
        bool started = false;
        for (int px = 0; px < w; ++px)
        {
            const float freq  = 20.f * std::pow (1000.f, (float) px / fw);
            const float omega = juce::MathConstants<float>::twoPi * freq / eqSr;
            const float cosW  = std::cos (omega);
            const float sinW  = std::sin (omega);
            const float cos2W = 2.f * cosW * cosW - 1.f; // double-angle identity (faster)
            const float sin2W = 2.f * sinW * cosW;

            float totalDb = 0.f;
            for (int b = 0; b < 8; ++b)
            {
                const auto& c = eqBands[b];
                const float nr = c.b0 + c.b1 * cosW + c.b2 * cos2W;
                const float ni = -(c.b1 * sinW + c.b2 * sin2W);
                const float dr = 1.f + c.a1 * cosW + c.a2 * cos2W;
                const float di = -(c.a1 * sinW + c.a2 * sin2W);
                const float magSq = (nr*nr + ni*ni) / juce::jmax (1e-30f, dr*dr + di*di);
                // Multiply contribution by pass count (cascaded stages for HPF/LPF slope)
                totalDb += (float) eqPasses[b] * 10.f * std::log10 (juce::jmax (1e-12f, magSq));
            }

            const float y = dBToY (totalDb);
            if (!started) { eqPath.startNewSubPath ((float) px, y); started = true; }
            else          eqPath.lineTo ((float) px, y);
        }
        // Subtle glow fill under curve
        juce::Path eqFill = eqPath;
        eqFill.lineTo (fw, dBToY (0.f));
        eqFill.lineTo (0.f, dBToY (0.f));
        eqFill.closeSubPath();
        g.setColour (juce::Colour (0x18ffee44));
        g.fillPath (eqFill);

        // EQ curve line
        g.setColour (juce::Colour (0xffffe040));
        g.strokePath (eqPath, juce::PathStrokeType (2.0f,
            juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // -- Band indicator dots ---------------------------------------
        static const juce::Colour bandDotCols[8] = {
            juce::Colour (0xff00ccff), // HPF    - cyan
            juce::Colour (0xffdd2200), // LowShelf - red
            juce::Colour (0xffffff44), // Peak   - yellow
            juce::Colour (0xffffff44),
            juce::Colour (0xffffff44),
            juce::Colour (0xffffff44),
            juce::Colour (0xffdd2200), // HighShelf - red
            juce::Colour (0xff00ccff), // LPF    - cyan
        };
        for (int b = 0; b < 8; ++b)
        {
            const float f = eqFreqs[b];
            if (f < 18.f || f > 21000.f) continue;
            const float dotX  = freqToX (f);
            // HPF (0) and LPF (7) gain controls slope ? dot always at 0 dB
            const float dotY  = (b == 0 || b == 7) ? dBToY (0.f) : dBToY (eqGains[b]);
            const auto  col   = bandDotCols[b];

            // Outer glow ring
            g.setColour (col.withAlpha (0.35f));
            g.fillEllipse (dotX - 7.f, dotY - 7.f, 14.f, 14.f);
            // Ring
            g.setColour (col.withAlpha (0.85f));
            g.drawEllipse (dotX - 5.f, dotY - 5.f, 10.f, 10.f, 1.5f);
            // Centre fill
            g.setColour (col.withAlpha (0.95f));
            g.fillEllipse (dotX - 3.f, dotY - 3.f, 6.f, 6.f);
        }
    }

    // -- dB labels (left edge) -----------------------------------------
    g.setFont (juce::Font (juce::FontOptions ("Arial", 7.5f, juce::Font::bold)));
    static const float dBLabels[] = { 24.f, 12.f, 0.f, -12.f, -24.f };
    for (float db : dBLabels)
    {
        const float y = dBToY (db);
        if (y > fh - 8.f) continue;
        const bool isKey = (db == 24.f || db == -24.f || db == 0.f);
        g.setColour (isKey ? juce::Colour (0xff9090cc)
                           : juce::Colour (0xff404060));
        g.drawText (juce::String ((int) db), 2, (int) y - 5, 22, 10,
                    juce::Justification::centredLeft, false);
    }

    // -- Frequency labels (bottom) -------------------------------------
    static const int freqLabels[] = { 50, 100, 200, 500, 1000, 2000, 5000, 10000 };
    g.setFont (juce::Font (juce::FontOptions ("Arial", 7.5f, juce::Font::plain)));
    g.setColour (juce::Colour (0xff404060));
    for (int f : freqLabels)
    {
        const float px = freqToX ((float) f);
        juce::String lbl = (f >= 1000) ? (juce::String (f / 1000) + "k") : juce::String (f);
        g.drawText (lbl, (int) px - 14, h - 11, 28, 10, juce::Justification::centred, false);
    }

    // -- 0 dB label ----------------------------------------------------
    g.setColour (juce::Colour (0xff6060a0));
    g.setFont   (juce::Font (juce::FontOptions ("Arial", 7.5f, juce::Font::bold)));
    g.drawText  ("0 dB", w - 28, (int) dBToY (0.f) - 5, 26, 10,
                 juce::Justification::centredRight, false);

    // -- Border --------------------------------------------------------
    g.setColour (juce::Colour (0xff1a1a2c));
    g.drawRoundedRectangle (bounds.toFloat().reduced (0.5f), 4.f, 1.f);
}

void SpectrumDisplay::mouseDown (const juce::MouseEvent& e)
{
    if (!hasEQ) return;
    const float fw = (float) getWidth(), fh = (float) getHeight();
    const float dBTop = 24.f, dBRange = 48.f;
    const float logMin = std::log10 (20.f), logRange = std::log10 (20000.f) - logMin;

    dragBand = -1;
    float bestDist2 = 14.f * 14.f;   // 14 px threshold
    for (int b = 0; b < 8; ++b)
    {
        const float f = eqFreqs[b];
        if (f < 18.f || f > 21000.f) continue;
        const float dotX = (std::log10 (f) - logMin) / logRange * fw;
        // HPF/LPF dot is always at 0 dB line
        const float dotY = (b == 0 || b == 7) ? (dBTop / dBRange * fh)
                                               : ((dBTop - eqGains[b]) / dBRange * fh);
        const float dx   = (float) e.x - dotX;
        const float dy   = (float) e.y - dotY;
        const float d2   = dx * dx + dy * dy;
        if (d2 < bestDist2) { bestDist2 = d2; dragBand = b; }
    }
    if (dragBand >= 0) setMouseCursor (juce::MouseCursor::CrosshairCursor);
}

void SpectrumDisplay::mouseDrag (const juce::MouseEvent& e)
{
    if (dragBand < 0 || !onBandDragged) return;
    const float fw = (float) getWidth(), fh = (float) getHeight();
    const float dBTop = 24.f, dBRange = 48.f;
    const float logMin = std::log10 (20.f), logRange = std::log10 (20000.f) - logMin;

    const float t    = juce::jlimit (0.f, 1.f, (float) e.x / fw);
    const float freq = 20.f * std::pow (1000.f, t);
    const float gain = dBTop - ((float) e.y / fh) * dBRange;

    onBandDragged (dragBand,
                   juce::jlimit (freqMin, freqMax, freq),
                   juce::jlimit (gainMin, gainMax, gain));
}

void SpectrumDisplay::mouseUp (const juce::MouseEvent&)
{
    dragBand = -1;
    setMouseCursor (juce::MouseCursor::NormalCursor);
}

//==============================================================================
// TrackEQPanel
//==============================================================================
static void setupEQKnob (juce::Slider& s, InfernoLookAndFeel& laf)
{
    s.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    s.setLookAndFeel (&laf);
}

const char* TrackEQPanel::bandName (int b) noexcept
{
    static const char* names[] = { "HPF", "Lo S", "Lo-M", "Mid", "Hi-M", "Pres", "Hi S", "LPF" };
    return (b >= 0 && b < NUM_BANDS) ? names[b] : "";
}

TrackEQPanel::TrackEQPanel (DeathDealerDrumsAudioProcessor& p, InfernoLookAndFeel& l)
    : proc (p), laf (l)
{
    addAndMakeVisible (spectrumDisplay);

    // EQ on/off button
    eqEnableBtn.setLookAndFeel (&laf);
    eqEnableBtn.setColour (juce::TextButton::buttonColourId,   juce::Colour (0xff1e1e28));
    eqEnableBtn.setColour (juce::TextButton::buttonOnColourId, InfernoLookAndFeel::accentRed());
    eqEnableBtn.setColour (juce::TextButton::textColourOffId,  InfernoLookAndFeel::dimText());
    eqEnableBtn.setColour (juce::TextButton::textColourOnId,   juce::Colours::white);
    eqEnableBtn.setClickingTogglesState (true);
    eqEnableBtn.setTooltip ("Enable/disable per-track EQ");
    addAndMakeVisible (eqEnableBtn);

    // Wire drag callback ? updates APVTS params when user drags a dot
    spectrumDisplay.onBandDragged = [this] (int band, float freq, float gain)
    {
        if (currentSlot < 0) return;
        auto& av = proc.getAPVTS();
        const juce::String bTag = "eq8_b" + juce::String (band);
        const int sl = currentSlot;
        if (auto* pf = av.getParameter (
                DeathDealerDrumsAudioProcessor::trackParamID (sl, bTag + "_freq")))
            pf->setValueNotifyingHost (pf->convertTo0to1 (freq));
        // HPF (0) and LPF (7): Y-drag maps to slope steps ? update gain param to adjust passes
        // Other bands: Y-drag updates gain directly
        if (auto* pg = av.getParameter (
                DeathDealerDrumsAudioProcessor::trackParamID (sl, bTag + "_gain")))
            pg->setValueNotifyingHost (pg->convertTo0to1 (gain));
    };

    for (int b = 0; b < NUM_BANDS; ++b)
    {
        setupEQKnob (freqKnob[b], laf);
        setupEQKnob (gainKnob[b], laf);
        setupEQKnob (qKnob[b],   laf);

        // Freq and gain knobs show value text box below
        freqKnob[b].setTextBoxStyle (juce::Slider::TextBoxBelow, false, 38, 10);
        gainKnob[b].setTextBoxStyle (juce::Slider::TextBoxBelow, false, 38, 10);

        // Custom text formatting
        freqKnob[b].textFromValueFunction = [] (double v)
        {
            return (v >= 1000.0) ? (juce::String (v / 1000.0, 1) + "k")
                                 : (juce::String ((int) v) + "Hz");
        };
        gainKnob[b].textFromValueFunction = [] (double v)
        {
            return (v >= 0.0 ? "+" : "") + juce::String (v, 1) + "dB";
        };
        qKnob[b].textFromValueFunction = [] (double v)
        {
            return juce::String (v, 2);
        };

        // Make text box uneditable
        freqKnob[b].setColour (juce::Slider::textBoxTextColourId,    juce::Colour (0xff7777aa));
        freqKnob[b].setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        freqKnob[b].setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        gainKnob[b].setColour (juce::Slider::textBoxTextColourId,    juce::Colour (0xff7777aa));
        gainKnob[b].setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        gainKnob[b].setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);

        const juce::String bn (bandName (b));
        freqKnob[b].setTooltip (bn + " � center frequency");
        gainKnob[b].setTooltip (bn + " � gain (dB)");
        qKnob[b].setTooltip    (bn + " � Q / bandwidth");
        addAndMakeVisible (freqKnob[b]);
        addAndMakeVisible (gainKnob[b]);
        addAndMakeVisible (qKnob[b]);
    }

    // HPF (band 0) and LPF (band 7): gain knob shows slope in dB/oct
    auto slopeText = [] (double v) -> juce::String
    {
        const int p = (v < -6.0) ? 1 : (v < 6.0) ? 2 : (v < 12.0) ? 3 : 4;
        return juce::String (p * 12) + " dB/oct";
    };
    gainKnob[0].textFromValueFunction = slopeText;
    gainKnob[7].textFromValueFunction = slopeText;
}

void TrackEQPanel::setTrack (int slotIndex)
{
    currentSlot = slotIndex;
    rebuildAttachments();
    repaint();
}

void TrackEQPanel::rebuildAttachments()
{
    for (int b = 0; b < NUM_BANDS; ++b)
    {
        freqAtt[b].reset(); gainAtt[b].reset(); qAtt[b].reset();
    }
    eqEnableAtt.reset();
    if (currentSlot < 0) return;

    auto& av = proc.getAPVTS();
    const int sl = currentSlot;

    eqEnableAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        av, DeathDealerDrumsAudioProcessor::trackParamID (sl, "eq8_on"), eqEnableBtn);

    for (int b = 0; b < NUM_BANDS; ++b)
    {
        const juce::String bTag = "eq8_b" + juce::String (b);
        freqAtt[b] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            av, DeathDealerDrumsAudioProcessor::trackParamID (sl, bTag + "_freq"), freqKnob[b]);
        gainAtt[b] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            av, DeathDealerDrumsAudioProcessor::trackParamID (sl, bTag + "_gain"), gainKnob[b]);
        qAtt[b]    = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            av, DeathDealerDrumsAudioProcessor::trackParamID (sl, bTag + "_q"),    qKnob[b]);
    }
}

void TrackEQPanel::timerTick (DrumEngine* engine, float sampleRate)
{
    if (!engine || currentSlot < 0) return;

    // Always update the EQ curve so it responds instantly to knob changes
    BiquadCoeffs coeffs[NUM_BANDS];
    float freqs[NUM_BANDS], gains[NUM_BANDS];
    int   passes[NUM_BANDS];
    auto& av = proc.getAPVTS();
    const int sl = currentSlot;
    for (int b = 0; b < NUM_BANDS; ++b)
    {
        coeffs[b] = engine->getEQ8Coeff (sl, b);
        passes[b] = engine->getEQ8Passes (sl, b);
        const juce::String bTag = "eq8_b" + juce::String (b);
        freqs[b] = *av.getRawParameterValue (
            DeathDealerDrumsAudioProcessor::trackParamID (sl, bTag + "_freq"));
        gains[b] = *av.getRawParameterValue (
            DeathDealerDrumsAudioProcessor::trackParamID (sl, bTag + "_gain"));
    }
    spectrumDisplay.setEQCoeffs (coeffs, freqs, gains, passes, sampleRate);

    if (engine->spectrumReady.exchange (false))
        spectrumDisplay.setData (engine->spectrumData, DrumEngine::SPECTRUM_BINS, sampleRate);
}

void TrackEQPanel::paint (juce::Graphics& g)
{
    g.setColour (InfernoLookAndFeel::panelBg().brighter (0.04f));
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 4.f);

    if (currentSlot < 0) return;

    // Compute same geometry as resized() so labels align with knobs
    const auto area        = getLocalBounds().reduced (6, 6);
    const int  enableH     = 22 + 4;
    const int  bandLabelH  = 12;
    const int  knobH       = 34;
    const int  textH       = 10;   // text box on freq + gain rows
    const int  rowLabelH   = 10;
    const int  rowGap      = 3;
    const int  rowLabelW   = 30;
    const int  knobbingH   = enableH + bandLabelH
                             + (rowLabelH + knobH + textH + rowGap)   // freq
                             + (rowLabelH + knobH + textH + rowGap)   // gain
                             + (rowLabelH + knobH);                   // q
    const int  specH       = juce::jmax (60, area.getHeight() - knobbingH - 8);
    const int  bandW       = (area.getWidth() - rowLabelW) / NUM_BANDS;
    const int  gridX       = area.getX() + rowLabelW;

    int y = area.getY() + specH + 8 + enableH;

    // Band name labels
    g.setFont (juce::Font (juce::FontOptions ("Arial", 8.5f, juce::Font::bold)));
    for (int b = 0; b < NUM_BANDS; ++b)
    {
        const bool isFilter = (b == 0 || b == 7);
        g.setColour (isFilter ? InfernoLookAndFeel::accentRed().withAlpha (0.75f)
                              : InfernoLookAndFeel::dimText().withAlpha (0.9f));
        g.drawText (bandName (b), gridX + b * bandW, y, bandW, bandLabelH,
                    juce::Justification::centred, false);
    }
    y += bandLabelH;

    // Row labels: FREQ / GAIN / Q
    const char* rowNames[] = { "FREQ", "GAIN", "Q" };
    g.setFont   (juce::Font (juce::FontOptions ("Arial", 8.f, juce::Font::bold)));
    g.setColour (InfernoLookAndFeel::dimText().withAlpha (0.65f));
    const int rowExtraH[3] = { textH, textH, 0 };
    for (int row = 0; row < 3; ++row)
    {
        g.drawText (rowNames[row], area.getX(), y, rowLabelW, rowLabelH,
                    juce::Justification::centredLeft, false);
        y += rowLabelH + knobH + rowExtraH[row] + rowGap;
    }
}

void TrackEQPanel::resized()
{
    const auto area       = getLocalBounds().reduced (6, 6);
    const int  enableH    = 22 + 4;
    const int  bandLabelH = 12;
    const int  knobH      = 34;
    const int  textH      = 10;
    const int  rowLabelH  = 10;
    const int  rowGap     = 3;
    const int  rowLabelW  = 30;
    const int  knobbingH  = enableH + bandLabelH
                            + (rowLabelH + knobH + textH + rowGap)
                            + (rowLabelH + knobH + textH + rowGap)
                            + (rowLabelH + knobH);
    const int  specH      = juce::jmax (60, area.getHeight() - knobbingH - 8);

    spectrumDisplay.setBounds (area.getX(), area.getY(), area.getWidth(), specH);

    // EQ on/off button (above band labels, below spectrum)
    eqEnableBtn.setBounds (area.getX(), area.getY() + specH + 8, 36, 22);

    const int bandW = (area.getWidth() - rowLabelW) / NUM_BANDS;
    const int gridX = area.getX() + rowLabelW;

    int y = area.getY() + specH + 8 + enableH + bandLabelH;

    juce::Slider* rows[3][NUM_BANDS] {};
    for (int b = 0; b < NUM_BANDS; ++b)
    {
        rows[0][b] = &freqKnob[b];
        rows[1][b] = &gainKnob[b];
        rows[2][b] = &qKnob[b];
    }
    const int rowTextH[3] = { textH, textH, 0 };
    for (int row = 0; row < 3; ++row)
    {
        y += rowLabelH;
        for (int b = 0; b < NUM_BANDS; ++b)
        {
            const int cx = gridX + b * bandW + (bandW - knobH) / 2;
            rows[row][b]->setBounds (cx, y, knobH, knobH + rowTextH[row]);
        }
        y += knobH + rowTextH[row] + rowGap;
    }
}

//==============================================================================
// TrackCompPanel
//==============================================================================
struct CompPreset { float thr, rat, atk, rel, mkp; };
static const CompPreset kCompPresets[] =
{
    //  thr      rat     atk      rel    mkp
    { -24.f,   6.f,  15.f,   80.f,  3.f },  //  1  KICK PUNCH  � longer atk lets the click transient through; 80ms rel avoids pumping
    { -30.f,   8.f,  20.f,  250.f,  7.f },  //  2  KICK DEEP   � slow atk for big transient, long rel creates sustained low-end boom
    { -18.f,   4.f,   2.f,   40.f,  3.f },  //  3  SNARE SNAP  � very fast atk tightens the snap; slightly longer rel than original
    { -24.f,   6.f,  18.f,   70.f,  4.f },  //  4  SNARE FAT   � slower atk lets crack pass through, compresses body for thickness
    { -22.f,   5.f,  15.f,  100.f,  3.f },  //  5  RACK TOM    � longer rel follows natural tom decay without pumping
    { -22.f,   5.f,  20.f,  180.f,  4.f },  //  6  FLOOR TOM   � long rel lets the deep thud sustain naturally
    { -16.f,   3.f,   5.f,   25.f,  1.f },  //  7  HI-HAT      � gentle; 5ms atk preserves the chick transient
    { -18.f,   3.f,  12.f,  100.f,  2.f },  //  8  CYMBAL      � slower atk preserves initial shimmer; long rel for wash tail
    { -16.f,   2.5f, 25.f,  180.f,  2.f },  //  9  OVERHEAD    � gentle glue: low ratio, slow atk/rel, minimal GR
};

TrackCompPanel::TrackCompPanel (DeathDealerDrumsAudioProcessor& p, InfernoLookAndFeel& l)
    : proc (p), laf (l)
{
    // Enable toggle
    enableBtn.setLookAndFeel (&laf);
    enableBtn.setColour (juce::TextButton::buttonColourId,   juce::Colour (0xff1e1e28));
    enableBtn.setColour (juce::TextButton::buttonOnColourId, InfernoLookAndFeel::accentRed());
    enableBtn.setColour (juce::TextButton::textColourOffId,  InfernoLookAndFeel::dimText());
    enableBtn.setColour (juce::TextButton::textColourOnId,   juce::Colours::white);
    enableBtn.setClickingTogglesState (true);
    enableBtn.setTooltip ("Enable/disable per-track compressor");
    addAndMakeVisible (enableBtn);

    // Preset combo
    presetLabel.setText ("PRESET", juce::dontSendNotification);
    presetLabel.setFont (juce::Font (juce::FontOptions ("Arial", 9.f, juce::Font::bold)));
    presetLabel.setColour (juce::Label::textColourId, InfernoLookAndFeel::dimText());
    presetLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (presetLabel);

    presetCombo.setLookAndFeel (&laf);
    presetCombo.addItem ("KICK PUNCH",  1);
    presetCombo.addItem ("KICK DEEP",   2);
    presetCombo.addItem ("SNARE SNAP",  3);
    presetCombo.addItem ("SNARE FAT",   4);
    presetCombo.addItem ("RACK TOM",    5);
    presetCombo.addItem ("FLOOR TOM",   6);
    presetCombo.addItem ("HI-HAT",      7);
    presetCombo.addItem ("CYMBAL",      8);
    presetCombo.addItem ("OVERHEAD",    9);
    presetCombo.addSeparator();
    presetCombo.addItem ("CUSTOM",     10);
    presetCombo.setSelectedId (10, juce::dontSendNotification);
    presetCombo.onChange = [this] { applyPreset (presetCombo.getSelectedId()); };
    presetCombo.setTooltip ("Load a compressor preset tuned for a specific drum type");
    addAndMakeVisible (presetCombo);

    // Knobs + labels
    struct KnobSetup { juce::Slider& knob; juce::Label& label; const char* txt; };
    KnobSetup ks[] = {
        { thrKnob, thrLabel, "THRESH" },
        { ratKnob, ratLabel, "RATIO"  },
        { atkKnob, atkLabel, "ATTACK" },
        { relKnob, relLabel, "RELEASE"},
        { mkpKnob, mkpLabel, "MAKEUP" },
    };
    for (auto& k : ks)
    {
        k.knob.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        k.knob.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 14);
        k.knob.setLookAndFeel (&laf);
        k.knob.setColour (juce::Slider::textBoxTextColourId,    InfernoLookAndFeel::dimText());
        k.knob.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        addAndMakeVisible (k.knob);

        k.label.setText (k.txt, juce::dontSendNotification);
        k.label.setFont (juce::Font (juce::FontOptions ("Arial", 9.f, juce::Font::bold)));
        k.label.setColour (juce::Label::textColourId, InfernoLookAndFeel::dimText());
        k.label.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (k.label);
    }
    // Custom text formatters
    thrKnob.textFromValueFunction = [] (double v) { return juce::String (v, 1) + " dB"; };
    ratKnob.textFromValueFunction = [] (double v) { return juce::String (v, 1) + ":1"; };
    atkKnob.textFromValueFunction = [] (double v) { return juce::String (v, 1) + " ms"; };
    relKnob.textFromValueFunction = [] (double v) { return juce::String (v, 0) + " ms"; };
    mkpKnob.textFromValueFunction = [] (double v) { return "+" + juce::String (v, 1) + " dB"; };
    thrKnob.setTooltip ("Threshold � compression kicks in below this level");
    ratKnob.setTooltip ("Ratio � higher = more squash");
    atkKnob.setTooltip ("Attack � how fast the compressor clamps down (ms)");
    relKnob.setTooltip ("Release � how fast the compressor lets go (ms)");
    mkpKnob.setTooltip ("Makeup gain � compensate for volume lost to compression");
}

void TrackCompPanel::setTrack (int slotIndex)
{
    currentSlot = slotIndex;
    rebuildAttachments();
    syncPresetComboToTrack();
    repaint();
}

void TrackCompPanel::rebuildAttachments()
{
    thrAtt.reset(); ratAtt.reset(); atkAtt.reset(); relAtt.reset(); mkpAtt.reset();
    enableAtt.reset();
    if (currentSlot < 0) return;

    auto& av  = proc.getAPVTS();
    const int sl = currentSlot;
    thrAtt    = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
                    av, DeathDealerDrumsAudioProcessor::trackParamID (sl, "trk_comp_thr"), thrKnob);
    ratAtt    = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
                    av, DeathDealerDrumsAudioProcessor::trackParamID (sl, "trk_comp_rat"), ratKnob);
    atkAtt    = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
                    av, DeathDealerDrumsAudioProcessor::trackParamID (sl, "trk_comp_atk"), atkKnob);
    relAtt    = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
                    av, DeathDealerDrumsAudioProcessor::trackParamID (sl, "trk_comp_rel"), relKnob);
    mkpAtt    = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
                    av, DeathDealerDrumsAudioProcessor::trackParamID (sl, "trk_comp_mkp"), mkpKnob);
    enableAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
                    av, DeathDealerDrumsAudioProcessor::trackParamID (sl, "trk_comp_on"), enableBtn);
}

void TrackCompPanel::syncPresetComboToTrack()
{
    if (currentSlot < 0)
    {
        presetCombo.setSelectedId (10, juce::dontSendNotification);
        return;
    }

    auto& av = proc.getAPVTS();
    const int sl = currentSlot;

    auto raw = [&] (const char* name) -> const std::atomic<float>*
    {
        return av.getRawParameterValue (DeathDealerDrumsAudioProcessor::trackParamID (sl, name));
    };

    const auto* thr = raw ("trk_comp_thr");
    const auto* rat = raw ("trk_comp_rat");
    const auto* atk = raw ("trk_comp_atk");
    const auto* rel = raw ("trk_comp_rel");
    const auto* mkp = raw ("trk_comp_mkp");

    if (!thr || !rat || !atk || !rel || !mkp)
    {
        presetCombo.setSelectedId (10, juce::dontSendNotification);
        return;
    }

    const float t = thr->load();
    const float r = rat->load();
    const float a = atk->load();
    const float l = rel->load();
    const float m = mkp->load();

    constexpr float tol = 0.051f;
    auto near = [tol] (float x, float y) { return std::abs (x - y) <= tol; };

    int matchedId = 10; // CUSTOM
    for (int i = 0; i < 9; ++i)
    {
        const auto& ps = kCompPresets[i];
        if (near (t, ps.thr) && near (r, ps.rat) && near (a, ps.atk)
            && near (l, ps.rel) && near (m, ps.mkp))
        {
            matchedId = i + 1;
            break;
        }
    }

    presetCombo.setSelectedId (matchedId, juce::dontSendNotification);
}

void TrackCompPanel::applyPreset (int id)
{
    if (currentSlot < 0 || id < 1 || id > 9) return;
    const auto& ps = kCompPresets[id - 1];
    auto& av = proc.getAPVTS();
    const int sl = currentSlot;

    auto set = [&] (const char* name, float val)
    {
        if (auto* p = av.getParameter (DeathDealerDrumsAudioProcessor::trackParamID (sl, name)))
            p->setValueNotifyingHost (p->convertTo0to1 (val));
    };
    set ("trk_comp_thr", ps.thr);
    set ("trk_comp_rat", ps.rat);
    set ("trk_comp_atk", ps.atk);
    set ("trk_comp_rel", ps.rel);
    set ("trk_comp_mkp", ps.mkp);

    // All named presets enable the compressor
    if (auto* p = av.getParameter (DeathDealerDrumsAudioProcessor::trackParamID (sl, "trk_comp_on")))
        p->setValueNotifyingHost (1.f);
}

void TrackCompPanel::updateMeter (DrumEngine* engine)
{
    if (!engine || currentSlot < 0) return;
    const float raw = juce::jmin (0.f, engine->trackCompGrDb[(size_t) currentSlot].load());
    // Snap to new raw if it's more negative (more GR), otherwise decay toward 0
    grMeter = juce::jmin (raw, grMeter * 0.7f);
    repaint();
}

void TrackCompPanel::paint (juce::Graphics& g)
{
    g.setColour (InfernoLookAndFeel::panelBg().brighter (0.04f));
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 4.f);

    if (currentSlot < 0)
    {
        g.setColour (InfernoLookAndFeel::dimText());
        g.setFont (juce::Font (juce::FontOptions ("Arial", 11.f, juce::Font::plain)));
        g.drawText ("No track selected", getLocalBounds(), juce::Justification::centred, false);
        return;
    }

    // -- GR meter (right edge) -----------------------------------------
    const int mW    = 16;
    const int mX    = getWidth() - mW - 6;
    const int mTopY = 30;
    const int mBotY = getHeight() - 24;  // leave room for readout below
    const int mH    = mBotY - mTopY;

    // Track background
    g.setColour (juce::Colour (0xff080810));
    g.fillRoundedRectangle ((float) mX, (float) mTopY, (float) mW, (float) mH, 3.f);

    // dB tick marks on the meter
    g.setFont (juce::Font (juce::FontOptions ("Arial", 6.5f, juce::Font::plain)));
    static const float meterTicks[] = { 0.f, -3.f, -6.f, -12.f, -20.f, -30.f, -40.f };
    for (float db : meterTicks)
    {
        const float ratio = juce::jlimit (0.f, 1.f, -db / 40.f);
        const int   ty    = mTopY + (int) (ratio * mH);
        g.setColour (juce::Colour (0xff222233));
        g.drawHorizontalLine (ty, (float) mX, (float) (mX + mW));
        g.setColour (juce::Colour (0xff3a3a55));
        const juce::String tickTxt = (db == 0.f) ? "0" : juce::String ((int) db);
        g.drawText (tickTxt, mX - 20, ty - 4, 18, 9, juce::Justification::centredRight, false);
    }

    // GR fill ? green ? amber ? red
    const float grDb  = juce::jlimit (-40.f, 0.f, grMeter);
    const float ratio = -grDb / 40.f;
    if (ratio > 0.001f)
    {
        const int fillH = juce::roundToInt (ratio * (float) mH);
        const juce::Colour col = (ratio < 0.30f) ? juce::Colour (0xff22dd55)
                               : (ratio < 0.65f) ? juce::Colour (0xffdd2200)
                                                  : juce::Colour (0xffdd2200);
        g.setGradientFill (juce::ColourGradient (
            col, (float) mX, (float) mTopY,
            col.withAlpha (0.6f), (float) mX, (float) (mTopY + fillH), false));
        g.fillRoundedRectangle ((float) mX, (float) mTopY,
                                (float) mW, (float) fillH, 3.f);
    }

    // GR label above meter
    g.setColour (InfernoLookAndFeel::dimText());
    g.setFont (juce::Font (juce::FontOptions ("Arial", 7.5f, juce::Font::bold)));
    g.drawText ("GR", mX - 2, mTopY - 14, mW + 4, 12, juce::Justification::centred, false);

    // GR dB readout below meter
    juce::String readout = (grDb < -0.05f) ? (juce::String (grDb, 1) + " dB") : "0.0 dB";
    g.setColour (ratio > 0.05f ? juce::Colours::white.withAlpha (0.9f)
                               : InfernoLookAndFeel::dimText().withAlpha (0.6f));
    g.setFont (juce::Font (juce::FontOptions ("Arial", 8.f, juce::Font::bold)));
    g.drawText (readout, mX - 12, mBotY + 4, mW + 24, 12,
                juce::Justification::centred, false);

    // Section label
    g.setColour (InfernoLookAndFeel::dimText());
    g.setFont   (juce::Font (juce::FontOptions ("Arial", 9.f, juce::Font::bold)));
    g.drawText  ("COMPRESSOR", 8, 8, 120, 14, juce::Justification::centredLeft, false);
}

void TrackCompPanel::resized()
{
    // Right edge reserved for GR meter (width 16px) + tick labels (20px) + padding
    const int meterReserve = 16 + 20 + 10;
    auto area = getLocalBounds().reduced (8, 8);
    area.removeFromRight (meterReserve);

    const int topRowH     = 22;
    const int gapAfterTop = 10;
    const int labelH      = 12;
    const int knobH       = 56 + 14;  // knob + text box

    // Top row stays at the top: "COMPRESSOR" label | ON button | PRESET combo
    auto topRow = area.removeFromTop (topRowH);
    topRow.removeFromLeft (86); // space for painted label
    enableBtn.setBounds (topRow.removeFromLeft (34));
    topRow.removeFromLeft (8);
    presetLabel.setBounds (topRow.removeFromLeft (44));
    presetCombo.setBounds (topRow.removeFromLeft (130));

    area.removeFromTop (gapAfterTop);

    // Centre just the knob block in the remaining space
    const int knobBlockH = labelH + knobH;
    const int leftover   = juce::jmax (0, area.getHeight() - knobBlockH);
    area.removeFromTop (leftover / 2);

    // Knob row: 5 equal columns
    const int colW = area.getWidth() / 5;

    juce::Slider* knobs[]  = { &thrKnob,  &ratKnob,  &atkKnob,  &relKnob,  &mkpKnob  };
    juce::Label*  labels[] = { &thrLabel, &ratLabel, &atkLabel, &relLabel, &mkpLabel };

    for (int c = 0; c < 5; ++c)
    {
        const int x = area.getX() + c * colW;
        labels[c]->setBounds (x, area.getY(),           colW, labelH);
        knobs [c]->setBounds (x, area.getY() + labelH,  colW, knobH);
    }
}

//==============================================================================
// TrackTransPanel implementation
//==============================================================================
TrackTransPanel::TrackTransPanel (DeathDealerDrumsAudioProcessor& p, InfernoLookAndFeel& l)
    : proc (p), laf (l)
{
    enableBtn.setClickingTogglesState (true);
    enableBtn.setLookAndFeel (&laf);
    enableBtn.setColour (juce::TextButton::buttonColourId,   juce::Colour (0xff1e1e2a));
    enableBtn.setColour (juce::TextButton::buttonOnColourId, InfernoLookAndFeel::accentRed());
    enableBtn.setColour (juce::TextButton::textColourOffId,  InfernoLookAndFeel::dimText());
    enableBtn.setColour (juce::TextButton::textColourOnId,   juce::Colours::white);
    enableBtn.setTooltip ("Enable/disable per-track transient designer");
    addAndMakeVisible (enableBtn);

    struct KnobSetup { juce::Slider& knob; juce::Label& label; const char* txt; };
    KnobSetup ks[] = {
        { atkKnob, atkLabel, "ATTACK" },
        { susKnob, susLabel, "SUSTAIN" },
    };
    for (auto& k : ks)
    {
        k.knob.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        k.knob.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 14);
        k.knob.setLookAndFeel (&laf);
        k.knob.setColour (juce::Slider::textBoxTextColourId,    InfernoLookAndFeel::dimText());
        k.knob.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        addAndMakeVisible (k.knob);

        k.label.setText (k.txt, juce::dontSendNotification);
        k.label.setFont (juce::Font (juce::FontOptions ("Arial", 9.f, juce::Font::bold)));
        k.label.setColour (juce::Label::textColourId, InfernoLookAndFeel::dimText());
        k.label.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (k.label);
    }
    atkKnob.textFromValueFunction = [] (double v) { return juce::String (v, 1) + " dB"; };
    susKnob.textFromValueFunction = [] (double v) { return juce::String (v, 1) + " dB"; };
    atkKnob.setTooltip ("Attack boost/cut � positive punches up the transient click, negative softens it");
    susKnob.setTooltip ("Sustain boost/cut � positive fattens the body, negative tightens it");
}

void TrackTransPanel::setTrack (int slotIndex)
{
    currentSlot = slotIndex;
    rebuildAttachments();
    repaint();
}

void TrackTransPanel::rebuildAttachments()
{
    atkAtt.reset(); susAtt.reset(); enableAtt.reset();
    if (currentSlot < 0) return;

    auto& av  = proc.getAPVTS();
    const int sl = currentSlot;
    atkAtt    = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
                    av, DeathDealerDrumsAudioProcessor::trackParamID (sl, "trk_trans_atk"), atkKnob);
    susAtt    = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
                    av, DeathDealerDrumsAudioProcessor::trackParamID (sl, "trk_trans_sus"), susKnob);
    enableAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
                    av, DeathDealerDrumsAudioProcessor::trackParamID (sl, "trk_trans_on"), enableBtn);
}

void TrackTransPanel::paint (juce::Graphics& g)
{
    g.setColour (InfernoLookAndFeel::panelBg().brighter (0.04f));
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 4.f);

    if (currentSlot < 0)
    {
        g.setColour (InfernoLookAndFeel::dimText());
        g.setFont (juce::Font (juce::FontOptions ("Arial", 11.f, juce::Font::plain)));
        g.drawText ("No track selected", getLocalBounds(), juce::Justification::centred, false);
        return;
    }

    // Section header
    g.setFont (juce::Font (juce::FontOptions ("Arial", 11.f, juce::Font::bold)));
    g.setColour (InfernoLookAndFeel::accentBright());
    g.drawText ("TRANSIENT DESIGNER", getLocalBounds().reduced (8, 6).removeFromTop (16),
                juce::Justification::left, false);
}

void TrackTransPanel::resized()
{
    auto area = getLocalBounds().reduced (8, 8);

    const int topRowH     = 22;
    const int gapAfterTop = 10;
    const int labelH      = 12;
    const int knobH       = 56 + 14;

    auto topRow = area.removeFromTop (topRowH);
    topRow.removeFromLeft (136); // space for painted label
    enableBtn.setBounds (topRow.removeFromLeft (34));

    area.removeFromTop (gapAfterTop);

    const int knobBlockH = labelH + knobH;
    const int leftover   = juce::jmax (0, area.getHeight() - knobBlockH);
    area.removeFromTop (leftover / 2);

    const int colW = area.getWidth() / 2;
    juce::Slider* knobs[]  = { &atkKnob,  &susKnob  };
    juce::Label*  labels[] = { &atkLabel, &susLabel };

    for (int c = 0; c < 2; ++c)
    {
        const int x = area.getX() + c * colW;
        labels[c]->setBounds (x, area.getY(),           colW, labelH);
        knobs [c]->setBounds (x, area.getY() + labelH,  colW, knobH);
    }
}

//==============================================================================
// TrackDetailPanel � constructor update to include EQ/comp panels + tabs
//==============================================================================
TrackDetailPanel::TrackDetailPanel (DeathDealerDrumsAudioProcessor& p, InfernoLookAndFeel& l)
    : proc (p), laf (l), eqPanel (p, l), compPanel (p, l), transPanel (p, l)
{
    // Editable track name (no static header above it)
    nameEditLabel.setFont (juce::Font (juce::FontOptions ("Arial", 15.0f, juce::Font::bold)));
    nameEditLabel.setColour (juce::Label::textColourId, InfernoLookAndFeel::accentBright());
    nameEditLabel.setColour (juce::Label::backgroundWhenEditingColourId, juce::Colour (0xff1a0000));
    nameEditLabel.setJustificationType (juce::Justification::centred);
    nameEditLabel.setEditable (false, true, false);
    nameEditLabel.onTextChange = [this]
    {
        if (currentSlot < 0) return;
        const juce::String newName = nameEditLabel.getText().trim();
        if (newName.isEmpty()) return;
        proc.renameTrack (currentSlot, newName);
        if (onNameChanged)
            onNameChanged (currentSlot);
    };
    addAndMakeVisible (nameEditLabel);

    for (auto* s : { &tuneKnob, &decayKnob, &attackKnob })
        { setupDetailKnob (*s, laf); addAndMakeVisible (s); }
    for (auto* lb : { &tuneLabel, &decayLabel, &attackLabel })
        { setupDetailLabel (*lb); addAndMakeVisible (lb); }
    tuneLabel.setText  ("TUNE",   juce::dontSendNotification);
    decayLabel.setText ("DECAY",  juce::dontSendNotification);
    attackLabel.setText("ATTACK", juce::dontSendNotification);
    tuneKnob.setTooltip   ("Tune � pitch offset in semitones");
    decayKnob.setTooltip  ("Decay � how long the sample tail plays out");
    attackKnob.setTooltip ("Attack sensitivity � controls how strongly quiet hits respond");

    setupDetailKnob  (reverbSendKnob, laf);
    setupDetailLabel (reverbSendLabel);
    reverbSendLabel.setText ("ROOM", juce::dontSendNotification);
    reverbSendKnob.setTooltip ("Room reverb bus send level for this track");
    addAndMakeVisible (reverbSendKnob);
    addAndMakeVisible (reverbSendLabel);

    setupDetailKnob  (compSendKnob, laf);
    setupDetailLabel (compSendLabel);
    compSendLabel.setText ("SMASH", juce::dontSendNotification);
    compSendKnob.setTooltip ("Parallel compression (SMASH) bus send level for this track");
    addAndMakeVisible (compSendKnob);
    addAndMakeVisible (compSendLabel);

    setupDetailKnob  (satSendKnob, laf);
    setupDetailLabel (satSendLabel);
    satSendLabel.setText ("TAPE", juce::dontSendNotification);
    satSendKnob.setTooltip ("Tape saturation bus send level for this track");
    addAndMakeVisible (satSendKnob);
    addAndMakeVisible (satSendLabel);

    chokeButton.setButtonText ("CHOKE");
    chokeButton.setToggleState (false, juce::dontSendNotification);
    chokeButton.setColour (juce::ToggleButton::textColourId,      InfernoLookAndFeel::textColour());
    chokeButton.setColour (juce::ToggleButton::tickColourId,      InfernoLookAndFeel::accentRed());
    chokeButton.setColour (juce::ToggleButton::tickDisabledColourId, InfernoLookAndFeel::textColour().withAlpha (0.4f));
    chokeButton.setTooltip ("Choke � retriggering this note cuts off the current hit (like a closed hi-hat)");
    addAndMakeVisible (chokeButton);

    // Choke trigger � fire a secondary sample when choke fires
    chokeTrigOnBtn.setButtonText ("ON CHOKE:");
    chokeTrigOnBtn.setToggleState (false, juce::dontSendNotification);
    chokeTrigOnBtn.setColour (juce::ToggleButton::textColourId,         InfernoLookAndFeel::textColour());
    chokeTrigOnBtn.setColour (juce::ToggleButton::tickColourId,         InfernoLookAndFeel::accentRed());
    chokeTrigOnBtn.setColour (juce::ToggleButton::tickDisabledColourId, InfernoLookAndFeel::textColour().withAlpha (0.4f));
    chokeTrigOnBtn.setTooltip ("When checked, triggers the selected track every time this track is choked");
    addAndMakeVisible (chokeTrigOnBtn);

    chokeTrigCombo.setTextWhenNothingSelected ("-- pick track --");
    chokeTrigCombo.setTextWhenNoChoicesAvailable ("(no tracks)");
    chokeTrigCombo.setTooltip ("Track to trigger when this track gets choked");
    addAndMakeVisible (chokeTrigCombo);

    chokeTrigDelayKnob.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    chokeTrigDelayKnob.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    chokeTrigDelayKnob.setRange (0.0, 200.0, 1.0);
    chokeTrigDelayKnob.setValue (0.0, juce::dontSendNotification);
    chokeTrigDelayKnob.setTooltip ("Choke Trigger Delay");
    chokeTrigDelayKnob.setColour (juce::Slider::rotarySliderFillColourId, InfernoLookAndFeel::accentRed());
    addAndMakeVisible (chokeTrigDelayKnob);

    // MIDI note ? editable, type a note name ("C3", "D#4") or number
    midiNoteLabel.setFont (juce::Font (juce::FontOptions ("Arial", 13.0f, juce::Font::bold)));
    midiNoteLabel.setColour (juce::Label::textColourId,             InfernoLookAndFeel::textColour());
    midiNoteLabel.setColour (juce::Label::backgroundWhenEditingColourId, juce::Colour (0xff1a0000));
    midiNoteLabel.setJustificationType (juce::Justification::centred);
    midiNoteLabel.setEditable (false, true, false);
    midiNoteLabel.onTextChange = [this]
    {
        if (currentSlot < 0) return;
        const juce::String txt = midiNoteLabel.getText().trim();
        int noteNum = -1;
        if (txt.containsOnly ("0123456789"))
            noteNum = txt.getIntValue();
        else
            for (int n = 0; n < 128; ++n)
                if (juce::MidiMessage::getMidiNoteName (n, true, true, kOctaveNumForMiddleC)
                        .equalsIgnoreCase (txt))
                { noteNum = n; break; }
        noteNum = juce::jlimit (0, 127, noteNum < 0 ? 60 : noteNum);
        if (auto* p = proc.getAPVTS().getParameter (
                DeathDealerDrumsAudioProcessor::trackParamID (currentSlot, "midi_note")))
            p->setValueNotifyingHost (p->convertTo0to1 ((float) noteNum));
        updateMidiNoteLabel();
    };
    addAndMakeVisible (midiNoteLabel);

    midiDownBtn.setLookAndFeel (&laf);
    midiUpBtn.setLookAndFeel   (&laf);
    midiDownBtn.setTooltip ("Shift MIDI trigger note down 1 semitone");
    midiUpBtn.setTooltip   ("Shift MIDI trigger note up 1 semitone");
    addAndMakeVisible (midiDownBtn);
    addAndMakeVisible (midiUpBtn);

    midiDownBtn.onClick = [this]
    {
        if (currentSlot < 0) return;
        auto* p = proc.getAPVTS().getParameter (
            DeathDealerDrumsAudioProcessor::trackParamID (currentSlot, "midi_note"));
        if (!p) return;
        const float cur = *proc.getAPVTS().getRawParameterValue (
            DeathDealerDrumsAudioProcessor::trackParamID (currentSlot, "midi_note"));
        const float nv  = juce::jlimit (0.0f, 127.0f, cur - 1.0f);
        p->setValueNotifyingHost (p->convertTo0to1 (nv));
        updateMidiNoteLabel();
    };

    midiUpBtn.onClick = [this]
    {
        if (currentSlot < 0) return;
        auto* p = proc.getAPVTS().getParameter (
            DeathDealerDrumsAudioProcessor::trackParamID (currentSlot, "midi_note"));
        if (!p) return;
        const float cur = *proc.getAPVTS().getRawParameterValue (
            DeathDealerDrumsAudioProcessor::trackParamID (currentSlot, "midi_note"));
        const float nv  = juce::jlimit (0.0f, 127.0f, cur + 1.0f);
        p->setValueNotifyingHost (p->convertTo0to1 (nv));
        updateMidiNoteLabel();
    };

    // Load sample button
    loadSampleBtn.setLookAndFeel (&laf);
    loadSampleBtn.setTooltip ("Load a new audio sample file for this pad");
    addAndMakeVisible (loadSampleBtn);
    loadSampleBtn.onClick = [this]
    {
        if (currentSlot < 0) return;
        DrumTrack* t = proc.getTrack (currentSlot);
        const juce::String title = "Load Sample"
            + (t ? " \u2014 " + t->getName() : "");
        juce::FileChooser chooser (title,
            juce::File::getSpecialLocation (juce::File::userMusicDirectory),
            "*.wav;*.aif;*.aiff;*.flac;*.ogg");
        if (chooser.browseForFileToOpen())
            proc.loadSampleForTrack (currentSlot, chooser.getResult());
    };

    samplePathLabel.setFont (juce::Font (juce::FontOptions ("Arial", 9.0f, juce::Font::italic)));
    samplePathLabel.setColour (juce::Label::textColourId, InfernoLookAndFeel::dimText());
    samplePathLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (samplePathLabel);

    // Sample mode selector
    for (auto* lb : { &sampleModeLabel })
    {
        lb->setFont (juce::Font (juce::FontOptions ("Arial", 10.0f, juce::Font::bold)));
        lb->setColour (juce::Label::textColourId, InfernoLookAndFeel::dimText());
        addAndMakeVisible (lb);
    }
    sampleModeLabel.setText   ("MODE:",     juce::dontSendNotification);

    padAreaLabel.setFont (juce::Font (juce::FontOptions ("Arial", 12.0f, juce::Font::bold)));
    padAreaLabel.setColour (juce::Label::textColourId, InfernoLookAndFeel::accentBright());
    padAreaLabel.setJustificationType (juce::Justification::centred);
    padAreaLabel.setText ("SAMPLE PADS", juce::dontSendNotification);
    addAndMakeVisible (padAreaLabel);

    sampleModeCombo.addItem ("SINGLE", 1);
    sampleModeCombo.addItem ("MULTI",  2);
    sampleModeCombo.setSelectedId (1, juce::dontSendNotification);
    sampleModeCombo.setLookAndFeel (&laf);
    sampleModeCombo.onChange = [this] { rebuildPads(); };
    sampleModeCombo.setTooltip ("Single: one sample for all velocities.  Multi: separate sample pads per velocity tier");
    addAndMakeVisible (sampleModeCombo);

    addAndMakeVisible (waveformDisplay);
    waveformDisplay.onTrimChanged = [this] (float startNorm, float endNorm)
    {
        if (currentSlot < 0) return;
        auto& av = proc.getAPVTS();

        if (auto* p = av.getParameter (
                DeathDealerDrumsAudioProcessor::trackParamID (currentSlot, "sample_start")))
            p->setValueNotifyingHost (p->convertTo0to1 (startNorm));

        if (auto* p = av.getParameter (
                DeathDealerDrumsAudioProcessor::trackParamID (currentSlot, "sample_end")))
            p->setValueNotifyingHost (p->convertTo0to1 (endNorm));
    };

    // Velocity tier selector ? compact inline checkboxes: SOFT / MID / HARD
    static const char* tierNames[NUM_VEL_TIERS] = { "SOFT", "MID", "HARD" };
    for (int t = 0; t < NUM_VEL_TIERS; ++t)
    {
        tierBtn[t].setButtonText (tierNames[t]);
        tierBtn[t].setLookAndFeel (&laf);
        tierBtn[t].setColour (juce::TextButton::buttonColourId,   juce::Colour (0xff161620));
        tierBtn[t].setColour (juce::TextButton::buttonOnColourId, InfernoLookAndFeel::accentRed());
        tierBtn[t].setColour (juce::TextButton::textColourOffId,  InfernoLookAndFeel::dimText());
        tierBtn[t].setColour (juce::TextButton::textColourOnId,   juce::Colours::white);
        tierBtn[t].setClickingTogglesState (false);
        tierBtn[t].setToggleState (t == activeTier, juce::dontSendNotification);
        const int tierIdx = t;

        tierBtn[t].onClick = [this, tierIdx]
        {
            activeTier = tierIdx;
            for (int i = 0; i < NUM_VEL_TIERS; ++i)
                tierBtn[i].setToggleState (i == activeTier, juce::dontSendNotification);
            rebuildPads();
        };
        static const char* tierTips[NUM_VEL_TIERS] = {
            "Edit samples for the SOFT velocity layer (quiet hits)",
            "Edit samples for the MID velocity layer",
            "Edit samples for the HARD velocity layer (loud hits)"
        };
        tierBtn[t].setTooltip (tierTips[t]);
        addAndMakeVisible (tierBtn[t]);
    }
    // Default: HARD selected
    tierBtn[2].setToggleState (true, juce::dontSendNotification);

    setTrack (-1);

    // EQ / COMP tab buttons
    auto setupTab = [this] (juce::TextButton& btn, int tabId)
    {
        btn.setLookAndFeel (&laf);
        btn.setColour (juce::TextButton::buttonColourId,   juce::Colour (0xff1e1e2a));
        btn.setColour (juce::TextButton::buttonOnColourId, InfernoLookAndFeel::accentRed());
        btn.setColour (juce::TextButton::textColourOffId,  InfernoLookAndFeel::dimText());
        btn.setColour (juce::TextButton::textColourOnId,   juce::Colours::white);
        btn.setClickingTogglesState (false);
        btn.onClick = [this, tabId]
        {
            activeDetailTab = tabId;
            eqPanel.setVisible    (tabId == 0);
            compPanel.setVisible  (tabId == 1);
            transPanel.setVisible (tabId == 2);
        };
        addAndMakeVisible (btn);
    };
    setupTab (eqTabBtn,    0);
    setupTab (compTabBtn,  1);
    setupTab (transTabBtn, 2);
    eqTabBtn.setTooltip    ("Per-track parametric EQ (8 bands)");
    compTabBtn.setTooltip  ("Per-track compressor");
    transTabBtn.setTooltip ("Per-track transient designer");

    addAndMakeVisible (eqPanel);
    compPanel.setVisible (false);
    addAndMakeVisible (compPanel);
    transPanel.setVisible (false);
    addAndMakeVisible (transPanel);
}

TrackDetailPanel::~TrackDetailPanel()
{
    for (auto* s : { &tuneKnob, &decayKnob, &attackKnob })
        s->setLookAndFeel (nullptr);
    midiDownBtn.setLookAndFeel (nullptr);
    midiUpBtn.setLookAndFeel   (nullptr);
    loadSampleBtn.setLookAndFeel (nullptr);
    sampleModeCombo.setLookAndFeel (nullptr);
    for (auto& btn : padButtons)
        btn->setLookAndFeel (nullptr);
    for (auto& btn : tierBtn)
        btn.setLookAndFeel (nullptr);
}

void TrackDetailPanel::setTrack (int slotIndex)
{
    currentSlot = slotIndex;

    const bool hasTrack = (slotIndex >= 0);

    nameEditLabel.setVisible   (hasTrack);
    for (auto* s : { &tuneKnob, &decayKnob, &attackKnob,
                     &reverbSendKnob, &compSendKnob, &satSendKnob })
        s->setVisible (hasTrack);
    for (auto* lb : { &tuneLabel, &decayLabel, &attackLabel,
                      &midiNoteLabel, &reverbSendLabel, &compSendLabel, &satSendLabel })
        lb->setVisible (hasTrack);
    for (auto* btn : { &midiDownBtn, &midiUpBtn, &loadSampleBtn })
        btn->setVisible (hasTrack);
    chokeButton.setVisible (hasTrack);
    chokeTrigOnBtn.setVisible (hasTrack);
    chokeTrigCombo.setVisible (hasTrack);
    chokeTrigDelayKnob.setVisible (hasTrack);
    samplePathLabel.setVisible (hasTrack);
    sampleModeLabel.setVisible (hasTrack);
    sampleModeCombo.setVisible (hasTrack);
    padAreaLabel.setVisible    (hasTrack);
    eqTabBtn .setVisible (hasTrack);
    compTabBtn.setVisible (hasTrack);
    transTabBtn.setVisible (hasTrack);
    eqPanel  .setVisible (hasTrack && activeDetailTab == 0);
    compPanel.setVisible (hasTrack && activeDetailTab == 1);
    transPanel.setVisible (hasTrack && activeDetailTab == 2);
    for (auto& btn : tierBtn)
        btn.setVisible (false);

    if (!hasTrack)
    {
        waveformDisplay.loadFrom (nullptr);
        rebuildPads();
        repaint();
        return;
    }

    DrumTrack* t = proc.getTrack (slotIndex);
    nameEditLabel.setText (t ? t->getName() : "", juce::dontSendNotification);

    rebuildAttachments();
    updateMidiNoteLabel();
    syncWaveTrimFromParams();

    // Populate choke trigger track combo with current track names
    {
        chokeTrigCombo.clear (juce::dontSendNotification);
        const int n = proc.getNumActiveTracks();
        for (int i = 0; i < n; ++i)
        {
            if (i == slotIndex) continue;   // skip self
            DrumTrack* tr = proc.getTrack (i);
            const juce::String name = tr ? tr->getName() : ("Track " + juce::String (i + 1));
            chokeTrigCombo.addItem (name, i + 1);   // item ID = track index + 1
        }
        // Restore selection from param
        auto* p = proc.getAPVTS().getRawParameterValue (
            DeathDealerDrumsAudioProcessor::trackParamID (slotIndex, "choke_trig_slot"));
        if (p)
            chokeTrigCombo.setSelectedId (juce::roundToInt (p->load()) + 1,
                                          juce::dontSendNotification);
    }

    // Update sample path label
    const juce::String path = t ? t->getSamplePath() : "";
    if (path.isNotEmpty())
        samplePathLabel.setText (juce::File (path).getFileName(), juce::dontSendNotification);
    else
        samplePathLabel.setText ("No sample loaded", juce::dontSendNotification);

    // Sync mode combo from track's current state
    if (t)
    {
        const bool isMulti = (t->getSampleMode() == DrumTrack::SampleMode::Multi);
        sampleModeCombo.setSelectedId  (isMulti ? 2 : 1,            juce::dontSendNotification);
    }
    rebuildPads();
    eqPanel.setTrack    (slotIndex);
    compPanel.setTrack  (slotIndex);
    transPanel.setTrack (slotIndex);

    // Load waveform from variation 0
    {
        const DrumVariation* v = t ? t->getVariation (0) : nullptr;
        waveformDisplay.loadFrom (v && v->valid ? &v->buffer : nullptr);
    }

    if (auto* engine = proc.getEngine())
        engine->spectrumTrack.store (slotIndex);
}

void TrackDetailPanel::timerUpdate (DrumEngine* engine, float sampleRate)
{
    syncWaveTrimFromParams();
    eqPanel.timerTick  (engine, sampleRate);
    compPanel.updateMeter (engine);

    // Trigger feedback: read last-hit tier + slot from the engine, flash pad + switch tier tab
    if (engine && currentSlot >= 0 && currentSlot < DrumEngine::MAX_TRACKS)
    {
        const int hitTier = engine->lastHitTier[(size_t) currentSlot].load();
        const int hitSlot = engine->lastHitSlot[(size_t) currentSlot].load();

        if (hitTier >= 0 && hitSlot >= 0)
        {
            // Clear the atomic so we only react once per hit
            engine->lastHitTier[(size_t) currentSlot].store (-1);
            engine->lastHitSlot[(size_t) currentSlot].store (-1);

            // Switch the active tier tab to match the velocity tier that fired
            if (hitTier != activeTier)
            {
                activeTier = hitTier;
                for (int t = 0; t < NUM_VEL_TIERS; ++t)
                    tierBtn[t].setToggleState (t == activeTier, juce::dontSendNotification);
                rebuildPads();
            }

            // Flash the pad button for the slot that fired
            if (hitSlot < (int) padButtons.size())
            {
                padFlashCountdown[hitSlot] = kFlashFrames;
                padButtons[(size_t) hitSlot]->setColour (
                    juce::TextButton::buttonColourId, InfernoLookAndFeel::accentRed());
                padButtons[(size_t) hitSlot]->repaint();
            }
        }

        // Decay flash countdowns � restore normal colour when they expire
        for (int s = 0; s < (int) padButtons.size(); ++s)
        {
            if (padFlashCountdown[s] > 0)
            {
                --padFlashCountdown[s];
                if (padFlashCountdown[s] == 0)
                {
                    padButtons[(size_t) s]->setColour (
                        juce::TextButton::buttonColourId, juce::Colour (0xff3a0e0e));
                    padButtons[(size_t) s]->repaint();
                }
            }
        }
    }

}


void TrackDetailPanel::rebuildAttachments ()
{
    tuneAtt.reset(); decayAtt.reset(); attackAtt.reset();
    reverbSendAtt.reset(); compSendAtt.reset(); satSendAtt.reset();
    chokeTrigDelayAtt.reset();
    chokeAtt.reset(); chokeTrigOnAtt.reset();
    chokeTrigCombo.onChange = nullptr;
    if (currentSlot < 0) return;

    auto& av = proc.getAPVTS();
    const int sl = currentSlot;
    tuneAtt   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
                    av, DeathDealerDrumsAudioProcessor::trackParamID (sl, "tune"),   tuneKnob);
    decayAtt  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
                    av, DeathDealerDrumsAudioProcessor::trackParamID (sl, "decay"),  decayKnob);
    attackAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
                    av, DeathDealerDrumsAudioProcessor::trackParamID (sl, "attack"), attackKnob);
    reverbSendAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
                    av, DeathDealerDrumsAudioProcessor::trackParamID (sl, "reverb_send"), reverbSendKnob);
    compSendAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
                    av, DeathDealerDrumsAudioProcessor::trackParamID (sl, "comp_send"), compSendKnob);
    satSendAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
                    av, DeathDealerDrumsAudioProcessor::trackParamID (sl, "sat_send"), satSendKnob);
    chokeAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
                    av, DeathDealerDrumsAudioProcessor::trackParamID (sl, "choke"), chokeButton);
    chokeTrigOnAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
                    av, DeathDealerDrumsAudioProcessor::trackParamID (sl, "choke_trig_on"), chokeTrigOnBtn);
    chokeTrigDelayAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
                    av, DeathDealerDrumsAudioProcessor::trackParamID (sl, "choke_trig_delay"), chokeTrigDelayKnob);

    // ComboBox ? param (no built-in ComboBoxAttachment for plain float params)
    chokeTrigCombo.onChange = [this, sl, &av]
    {
        const int id = chokeTrigCombo.getSelectedId();   // track index + 1
        if (id > 0)
            if (auto* p = av.getParameter (DeathDealerDrumsAudioProcessor::trackParamID (sl, "choke_trig_slot")))
                p->setValueNotifyingHost (p->convertTo0to1 ((float)(id - 1)));
    };
}

void TrackDetailPanel::syncWaveTrimFromParams ()
{
    if (currentSlot < 0) return;

    auto& av = proc.getAPVTS();
    const auto sid = DeathDealerDrumsAudioProcessor::trackParamID (currentSlot, "sample_start");
    const auto eid = DeathDealerDrumsAudioProcessor::trackParamID (currentSlot, "sample_end");
    auto* s = av.getRawParameterValue (sid);
    auto* e = av.getRawParameterValue (eid);
    if (!s || !e) return;

    waveformDisplay.setTrimRange (s->load(), e->load());
}

void TrackDetailPanel::updateMidiNoteLabel ()
{
    if (currentSlot < 0) return;
    const int note = juce::roundToInt (
        proc.getAPVTS().getRawParameterValue (
            DeathDealerDrumsAudioProcessor::trackParamID (currentSlot, "midi_note"))->load());
    const juce::String noteName = juce::MidiMessage::getMidiNoteName (note, true, true, kOctaveNumForMiddleC);
    midiNoteLabel.setText ("MIDI: " + noteName + " (" + juce::String (note) + ")",
                            juce::dontSendNotification);
}

void TrackDetailPanel::paint (juce::Graphics& g)
{
    g.setColour (InfernoLookAndFeel::panelBg());
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 6.0f);

    auto borderColour = dragHovering ? InfernoLookAndFeel::accentBright()
                                     : InfernoLookAndFeel::accentRed().withAlpha (0.6f);
    g.setColour (borderColour);
    g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (1.0f), 6.0f,
                             dragHovering ? 2.0f : 1.0f);

    if (currentSlot < 0)
    {
        g.setColour (InfernoLookAndFeel::dimText().withAlpha (0.45f));
        g.setFont (juce::Font (juce::FontOptions ("Arial", 14.f, juce::Font::bold)));
        g.drawText ("SELECT A TRACK", getLocalBounds(), juce::Justification::centred, false);
        return;
    }

    if (dragHovering)
    {
        g.setColour (InfernoLookAndFeel::accentBright().withAlpha (0.08f));
        g.fillRoundedRectangle (getLocalBounds().toFloat().reduced (2.0f), 5.0f);
        g.setColour (InfernoLookAndFeel::accentBright());
        g.setFont (juce::Font (juce::FontOptions ("Arial", 13.0f, juce::Font::bold)));
        g.drawText ("DROP SAMPLE HERE",
                    getLocalBounds().removeFromBottom (30),
                    juce::Justification::centred, false);
    }
}

void TrackDetailPanel::resized ()
{
    if (currentSlot < 0) return;

    const int totalW  = getWidth();
    const int totalH  = getHeight();
    const int padX    = 6;
    const int padY    = 4;
    const int innerW  = totalW - padX * 2;

    // -- Bottom: EQ / COMP tabs + panels ----------------------------------
    // Measure top controls height so bottom gets the rest
    // Fixed heights for top controls:
    const int nameH       = 22;
    const int knobRowH    = 74;   // label(10) + knob(48) + choke(16)
    const int chokeTrigH  = 22;   // choke-trigger checkbox + combo row
    const int midiH       = 22;
    const int loadH       = 36;   // button(22) + path(14)
    const int modeH       = 22;
    const int padLabelH   = 16;
    const int padBtnH     = 52;
    const int topRowGaps  = 4 + 4 + 4 + 4 + 4 + 4 + 2; // gaps between rows
    const int topH        = padY + nameH + topRowGaps + knobRowH + chokeTrigH + midiH + loadH + modeH
                            + padLabelH + padBtnH + 4;

    const int tabH    = 28;
    const int tabY    = topH;
    const int thirdW  = innerW / 3;
    eqTabBtn  .setBounds (padX,              tabY, thirdW,          tabH);
    compTabBtn.setBounds (padX + thirdW,     tabY, thirdW,          tabH);
    transTabBtn.setBounds (padX + thirdW * 2, tabY, innerW - thirdW * 2, tabH);

    const juce::Rectangle<int> panelR (padX, tabY + tabH, innerW, totalH - tabY - tabH - 4);
    eqPanel   .setBounds (panelR);
    compPanel .setBounds (panelR);
    transPanel.setBounds (panelR);

    // -- Top controls -----------------------------------------------------
    int y = padY;

    // Name
    nameEditLabel.setBounds (padX, y, innerW, nameH);
    y += nameH + 4;

    // Knob row ? Tune/Decay+Choke/Attack on left, Room/Smash/Tape on right
    {
        const int kW    = 48;
        const int lH    = 10;
        const int chkH  = 16;
        const int colW  = kW + 6;  // knob + small gap

        // Left group
        const int lx0 = padX + 4;
        auto placeKnob = [&] (juce::Slider& k, juce::Label& lb, int x, bool choke = false)
        {
            lb.setBounds (x, y,       kW, lH);
            k .setBounds (x, y + lH,  kW, kW);
            if (choke)
                chokeButton.setBounds (x - 2, y + lH + kW + 2, kW + 4, chkH);
        };
        placeKnob (tuneKnob,   tuneLabel,       lx0,             false);
        placeKnob (decayKnob,  decayLabel,      lx0 + colW,      true);
        placeKnob (attackKnob, attackLabel,     lx0 + colW * 2,  false);

        // Right group
        const int rx0 = padX + innerW - 3 * colW - 4;
        placeKnob (reverbSendKnob, reverbSendLabel, rx0,            false);
        placeKnob (compSendKnob,   compSendLabel,   rx0 + colW,     false);
        placeKnob (satSendKnob,    satSendLabel,    rx0 + colW * 2, false);

        // Waveform display in the middle gap
        const int wfX = lx0 + 3 * colW + 4;
        const int wfW = rx0 - wfX - 4;
        if (wfW > 20)
            waveformDisplay.setBounds (wfX, y + lH, wfW, kW);
        else
            waveformDisplay.setBounds (0, 0, 0, 0);

        y += lH + kW + chkH + 2 + 4;
    }

    // Choke trigger row � "ON CHOKE:" checkbox + track selector combo + delay knob
    {
        const int cbH   = 16;   // same height as CHOKE checkbox
        const int cbOff = (chokeTrigH - cbH) / 2;   // vertical centering offset
        const int kW    = chokeTrigH;                // knob is square, fills row height
        const int btnW  = 60;                        // "ON CHOKE:" toggle
        const int comboW = juce::jmin (130, innerW - btnW - kW - 12); // fixed narrow combo
        const int gap   = 4;
        chokeTrigOnBtn    .setBounds (padX,                      y + cbOff, btnW,   cbH);
        chokeTrigCombo    .setBounds (padX + btnW + gap,         y + cbOff, comboW, cbH);
        chokeTrigDelayKnob.setBounds (padX + btnW + gap + comboW + gap, y, kW,    kW);
        y += chokeTrigH + 4;
    }

    // MIDI row
    {
        auto midiRow = juce::Rectangle<int> (padX, y, innerW, midiH);
        midiDownBtn.setBounds (midiRow.removeFromLeft  (24));
        midiUpBtn  .setBounds (midiRow.removeFromRight (24));
        midiNoteLabel.setBounds (midiRow);
        y += midiH + 4;
    }

    // Load sample + path
    loadSampleBtn  .setBounds (padX, y, innerW, 22);  y += 22 + 2;
    samplePathLabel.setBounds (padX, y, innerW, 14);  y += 14 + 4;

    // Mode row (single dropdown only)
    {
        auto modeRow = juce::Rectangle<int> (padX, y, innerW, modeH);
        sampleModeLabel.setBounds (modeRow.removeFromLeft (50));
        sampleModeCombo.setBounds (modeRow.removeFromLeft (90));
        y += modeH + 4;
    }

    // Pad area label + tier checkboxes on the same row (top-right), then pad buttons
    {
        const int labelW   = innerW;
        const int chkW     = 42;
        const int chkGap   = 3;
        const int totalChk = NUM_VEL_TIERS * chkW + (NUM_VEL_TIERS - 1) * chkGap;
        padAreaLabel.setBounds (padX, y, labelW, padLabelH);

        // Tier checkboxes right-aligned, raised up by the mode row height
        const int chkY = y + (padLabelH - 16) / 2 - (modeH + 4);
        for (int t = 0; t < NUM_VEL_TIERS; ++t)
        {
            const int chkX = padX + innerW - totalChk + t * (chkW + chkGap);
            tierBtn[t].setBounds (chkX, chkY, chkW, 16);
        }
        y += padLabelH + 2;
    }

    if (!padButtons.empty())
    {
        const int n      = (int) padButtons.size();
        const int gap    = 6;
        const int maxW   = 80;
        const int btnW   = juce::jmin (maxW, juce::jmax (20, (innerW - (n - 1) * gap) / n));
        const int totalBtnW = n * btnW + (n - 1) * gap;
        int bx = padX + (innerW - totalBtnW) / 2;
        for (auto& btn : padButtons)
        {
            btn->setBounds (bx, y, btnW, padBtnH);
            bx += btnW + gap;
        }
    }
}

void TrackDetailPanel::setupKnob (juce::Slider& s, bool /*bipolar*/)
{
    setupDetailKnob (s, laf);
}

void TrackDetailPanel::setupLabel (juce::Label& l)
{
    setupDetailLabel (l);
}

//==============================================================================
// FileDragAndDropTarget

static bool isAudioFile (const juce::String& path)
{
    const juce::StringArray exts { ".wav", ".aif", ".aiff", ".mp3", ".flac", ".ogg", ".bwf" };
    const juce::String lower = path.toLowerCase();
    for (auto& e : exts)
        if (lower.endsWith (e)) return true;
    return false;
}

bool TrackDetailPanel::isInterestedInFileDrag (const juce::StringArray& files)
{
    if (currentSlot < 0) return false;
    for (auto& f : files)
        if (isAudioFile (f)) return true;
    return false;
}

void TrackDetailPanel::fileDragEnter (const juce::StringArray& files, int, int)
{
    if (isInterestedInFileDrag (files)) { dragHovering = true; repaint(); }
}

void TrackDetailPanel::fileDragExit (const juce::StringArray&)
{
    dragHovering = false;
    repaint();
}

void TrackDetailPanel::filesDropped (const juce::StringArray& files, int, int)
{
    dragHovering = false;
    repaint();
    if (currentSlot < 0) return;
    for (auto& f : files)
    {
        if (isAudioFile (f))
        {
            proc.loadSampleForTrack (currentSlot, juce::File (f));
            break;
        }
    }
}

void TrackDetailPanel::setLockedForPreset (bool locked)
{
    // Disable editing controls but leave pad buttons and tier buttons interactive
    nameEditLabel  .setEnabled (!locked);
    tuneKnob       .setEnabled (!locked);
    decayKnob      .setEnabled (!locked);
    attackKnob     .setEnabled (!locked);
    midiDownBtn    .setEnabled (!locked);
    midiUpBtn      .setEnabled (!locked);
    loadSampleBtn  .setEnabled (!locked);
    sampleModeCombo.setEnabled (!locked);
    reverbSendKnob .setEnabled (!locked);
    compSendKnob   .setEnabled (!locked);
    satSendKnob    .setEnabled (!locked);
    chokeButton    .setEnabled (!locked);
    chokeTrigOnBtn .setEnabled (!locked);
    chokeTrigCombo .setEnabled (!locked);
    chokeTrigDelayKnob.setEnabled (!locked);
    eqTabBtn       .setEnabled (!locked);
    compTabBtn     .setEnabled (!locked);
    transTabBtn    .setEnabled (!locked);
    eqPanel        .setEnabled (!locked);
    compPanel      .setEnabled (!locked);
    transPanel     .setEnabled (!locked);
    waveformDisplay.setEnabled (!locked);
    // Pad buttons and tier buttons always stay enabled
}

void TrackDetailPanel::rebuildPads ()
{
    // Remove existing pad buttons
    for (auto& btn : padButtons)
    {
        btn->setLookAndFeel (nullptr);
        removeChildComponent (btn.get());
    }
    padButtons.clear();

    if (currentSlot < 0)
        return;

    const bool isMulti = (sampleModeCombo.getSelectedId() == 2);
    const int  count   = isMulti ? VARS_PER_TIER : 1;

    // Tier tabs visible only in multi mode
    for (auto& btn : tierBtn)
        btn.setVisible (isMulti);

    // Tell the track about its new mode so MIDI round-robin works correctly
    if (DrumTrack* t = proc.getTrack (currentSlot))
        t->setSampleMode (isMulti ? DrumTrack::SampleMode::Multi
                                  : DrumTrack::SampleMode::Single, count);

    // Build pad buttons for the active tier (each triggers a specific slot within that tier)
    for (int i = 0; i < count; ++i)
    {
        const juce::String label = isMulti ? juce::String (i + 1) : juce::String ("PLAY");
        auto btn = std::make_unique<juce::TextButton> (label);
        btn->setLookAndFeel (&laf);
        btn->setColour (juce::TextButton::buttonColourId,   juce::Colour (0xff3a0e0e));
        btn->setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xffcc2200));
        const int varIdx  = activeTier * VARS_PER_TIER + i;
        const int tierIdx = activeTier;
        const int slotIdx = i;
        btn->onClick = [this, varIdx, tierIdx, slotIdx]
        {
            if (currentSlot >= 0)
                proc.triggerPreview (currentSlot, varIdx);
            juce::ignoreUnused (tierIdx, slotIdx);
        };
        addAndMakeVisible (*btn);
        padButtons.push_back (std::move (btn));
    }

    resized();
}

//==============================================================================
// -- Main Editor ---------------------------------------------------------------

DeathDealerDrumsAudioProcessorEditor::DeathDealerDrumsAudioProcessorEditor (
    DeathDealerDrumsAudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p), detailPanel (p, laf)
{
    setLookAndFeel (&laf);

    // Header logo: load and tint pure solid red (preserve source alpha)
    {
        const auto src = juce::ImageCache::getFromMemory (
            BinaryData::NEW_LOGO_png, (int) BinaryData::NEW_LOGO_pngSize);
        const juce::uint8 rR = 0xff, rG = 0x00, rB = 0x00; // pure red
        juce::Image red (juce::Image::ARGB, src.getWidth(), src.getHeight(), true);
        for (int y = 0; y < src.getHeight(); ++y)
            for (int x = 0; x < src.getWidth(); ++x)
                red.setPixelAt (x, y, juce::Colour (rR, rG, rB, src.getPixelAt (x, y).getAlpha()));
        // Store at full resolution; aspect ratio preserved at draw time
        logoTinted = red;
    }

    // INFERNO TONES logo for bottom-right footer
    infernoTonesImg = juce::ImageCache::getFromMemory (
        BinaryData::INFERNO_TONES_png, (int) BinaryData::INFERNO_TONES_pngSize);

    brandLabel.setText ("INFERNO TONES", juce::dontSendNotification);
    brandLabel.setFont (juce::Font (juce::FontOptions ("Arial", 11.0f, juce::Font::bold)));
    brandLabel.setColour (juce::Label::textColourId, InfernoLookAndFeel::dimText());
    brandLabel.setJustificationType (juce::Justification::centredRight);
    // brandLabel intentionally not added � text removed from header

    // DEMO button � plays DEMO.mid through the engine
    demoBtn.setLookAndFeel (&laf);
    demoBtn.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff00aa44));
    demoBtn.onClick = [this]
    {
        if (proc.isDemoPlaying())
        {
            proc.stopDemo();
            demoBtn.setButtonText ("DEMO");
            demoBtn.setToggleState (false, juce::dontSendNotification);
        }
        else
        {
            proc.startDemo();
            demoBtn.setButtonText ("STOP");
            demoBtn.setToggleState (true, juce::dontSendNotification);
        }
    };
    proc.onDemoStopped = [this]
    {
        juce::MessageManager::callAsync ([this]
        {
            demoBtn.setButtonText ("DEMO");
            demoBtn.setToggleState (false, juce::dontSendNotification);
        });
    };
    demoBtn.setTooltip ("Play a demo MIDI sequence through the current kit");
    addAndMakeVisible (demoBtn);

    // Preset bar (header center)
    presetLabel.setText ("PRESET:", juce::dontSendNotification);
    presetLabel.setFont (juce::Font (juce::FontOptions ("Arial", 10.0f, juce::Font::bold)));
    presetLabel.setColour (juce::Label::textColourId, InfernoLookAndFeel::dimText());
    presetLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (presetLabel);

    presetCombo.setLookAndFeel (&laf);
    presetCombo.setTextWhenNothingSelected ("-- NO PRESET --");
    presetCombo.onChange = [this]
    {
        const int id = presetCombo.getSelectedId();
        if (id <= 0) return;
        if (id == 1)
        {
            auto xml = juce::XmlDocument::parse (
                juce::String::fromUTF8 (BinaryData::DEFAULT_ddd, BinaryData::DEFAULT_dddSize));
            if (xml)
            {
                juce::MemoryBlock mb;
                proc.copyXmlToBinary (*xml, mb);
                proc.setStateInformation (mb.getData(), (int) mb.getSize());
                proc.currentPresetName = "DEFAULT";
                rebuildTrackList();
                selectTrack (-1);
                refreshPresetList();
            }
        }
        else if (id == 2)
        {
            auto xml = juce::XmlDocument::parse (
                juce::String::fromUTF8 (BinaryData::SCAVENGER_DRUMS_ddd, BinaryData::SCAVENGER_DRUMS_dddSize));
            if (xml)
            {
                juce::MemoryBlock mb;
                proc.copyXmlToBinary (*xml, mb);
                proc.setStateInformation (mb.getData(), (int) mb.getSize());
                proc.currentPresetName = "SCAVENGER DRUMS";
                rebuildTrackList();
                selectTrack (-1);
                refreshPresetList();
            }
        }
        else if (id == 3)
        {
            auto xml = juce::XmlDocument::parse (
                juce::String::fromUTF8 (BinaryData::SHORT_BASS_DROPS_ddd, BinaryData::SHORT_BASS_DROPS_dddSize));
            if (xml)
            {
                juce::MemoryBlock mb;
                proc.copyXmlToBinary (*xml, mb);
                proc.setStateInformation (mb.getData(), (int) mb.getSize());
                proc.currentPresetName = "SHORT BASS DROPS";
                rebuildTrackList();
                selectTrack (-1);
                refreshPresetList();
            }
        }
        else if (id == 4)
        {
            auto xml = juce::XmlDocument::parse (
                juce::String::fromUTF8 (BinaryData::MEDIUM_BASS_DROPS_ddd, BinaryData::MEDIUM_BASS_DROPS_dddSize));
            if (xml)
            {
                juce::MemoryBlock mb;
                proc.copyXmlToBinary (*xml, mb);
                proc.setStateInformation (mb.getData(), (int) mb.getSize());
                proc.currentPresetName = "MEDIUM BASS DROPS";
                rebuildTrackList();
                selectTrack (-1);
                refreshPresetList();
            }
        }
        else if (id == 5)
        {
            auto xml = juce::XmlDocument::parse (
                juce::String::fromUTF8 (BinaryData::LONG_BASS_DROPS_ddd, BinaryData::LONG_BASS_DROPS_dddSize));
            if (xml)
            {
                juce::MemoryBlock mb;
                proc.copyXmlToBinary (*xml, mb);
                proc.setStateInformation (mb.getData(), (int) mb.getSize());
                proc.currentPresetName = "LONG BASS DROPS";
                rebuildTrackList();
                selectTrack (-1);
                refreshPresetList();
            }
        }
        else
        {
            const juce::String name = presetCombo.getItemText (id - 1);
            const juce::File file = DeathDealerDrumsAudioProcessor::getPresetsFolder()
                                        .getChildFile (name + ".ddd");
            if (proc.loadPreset (file))
            {
                rebuildTrackList();
                selectTrack (-1);
                refreshPresetList();
            }
        }
    };
    presetCombo.setTooltip ("Switch between saved kit presets");
    addAndMakeVisible (presetCombo);

    loadPresetBtn.setLookAndFeel (&laf);
    loadPresetBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff1e1e2a));
    loadPresetBtn.onClick = [this]
    {
        juce::FileChooser chooser ("Load Preset",
            juce::File::getSpecialLocation (juce::File::userDocumentsDirectory),
            "*.ddd");

        if (chooser.browseForFileToOpen())
        {
            const juce::File srcFile = chooser.getResult();
            if (!srcFile.existsAsFile())
                return;

            // Mirror reference behavior: keep user presets centralized in the plugin preset folder.
            const auto presetFolder = DeathDealerDrumsAudioProcessor::getPresetsFolder();
            presetFolder.createDirectory();

            juce::File targetFile = srcFile;
            const juce::File canonical = presetFolder.getChildFile (srcFile.getFileName());
            if (srcFile.getFullPathName() != canonical.getFullPathName())
            {
                if (srcFile.copyFileTo (canonical))
                    targetFile = canonical;
            }

            if (proc.loadPreset (targetFile))
            {
                rebuildTrackList();
                selectTrack (-1);
                refreshPresetList();
            }
            else
            {
                juce::AlertWindow::showMessageBoxAsync (
                    juce::AlertWindow::WarningIcon,
                    "Preset Load Failed",
                    "Could not load that .ddd preset file.");
            }
        }
    };
    loadPresetBtn.setTooltip ("Import a .ddd preset file from disk");
    addAndMakeVisible (loadPresetBtn);

    savePresetBtn.setLookAndFeel (&laf);
    savePresetBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff1e1e2a));
    savePresetBtn.onClick = [this]
    {
        const auto folder = DeathDealerDrumsAudioProcessor::getPresetsFolder();
        folder.createDirectory();

        juce::AlertWindow namePrompt ("Save Preset",
                                      "Enter a preset name:",
                                      juce::AlertWindow::NoIcon);
        const juce::String suggested = proc.currentPresetName.equalsIgnoreCase ("DEFAULT")
                                     ? juce::String()
                                     : proc.currentPresetName;
        namePrompt.addTextEditor ("presetName", suggested, "Name:");
        namePrompt.addButton ("Save",   1, juce::KeyPress (juce::KeyPress::returnKey));
        namePrompt.addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

        if (namePrompt.runModalLoop() == 1)
        {
            juce::String name = namePrompt.getTextEditorContents ("presetName").trim();
            name = juce::File::createLegalFileName (name);
            if (name.isEmpty())
                return;

            const juce::File file = folder.getChildFile (name + ".ddd");

            if (file.existsAsFile())
            {
                const bool overwrite = juce::AlertWindow::showOkCancelBox (
                    juce::AlertWindow::WarningIcon,
                    "Overwrite Preset",
                    "A preset named \"" + name + "\" already exists. Overwrite it?",
                    "Overwrite",
                    "Cancel");

                if (!overwrite)
                    return;
            }

            proc.savePreset (file);
            refreshPresetList();
            const juce::String savedName = file.getFileNameWithoutExtension();
            for (int i = 0; i < presetCombo.getNumItems(); ++i)
                if (presetCombo.getItemText (i) == savedName)
                    { presetCombo.setSelectedId (i + 1, juce::dontSendNotification); break; }
        }
    };
    savePresetBtn.setTooltip ("Save the current kit as a named preset");
    addAndMakeVisible (savePresetBtn);

    exportPresetBtn.setLookAndFeel (&laf);
    exportPresetBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff1e1e2a));
    exportPresetBtn.onClick = [this]
    {
        juce::FileChooser chooser ("Export Preset",
            juce::File::getSpecialLocation (juce::File::userDocumentsDirectory),
            "*.ddd");
        if (chooser.browseForFileToSave (true))
        {
            const juce::File file = chooser.getResult().withFileExtension ("ddd");
            proc.savePreset (file, false);
        }
    };
    exportPresetBtn.setTooltip ("Export the current kit as a portable .ddd file");
    addAndMakeVisible (exportPresetBtn);

    refreshPresetList();

    // brandLabel removed from header

    // Left panel � viewport for scrollable track list
    trackViewport.setViewedComponent (&trackListContent, false);
    trackViewport.setScrollBarsShown (true, false);
    addAndMakeVisible (trackViewport);

    // ADD TRACK button
    addTrackBtn.setLookAndFeel (&laf);
    addTrackBtn.onClick = [this]
    {
        const int n = proc.getNumActiveTracks();
        // Suggest next GM-ish MIDI note
        const int defaultNotes[] = { 36, 38, 42, 46, 41, 45, 48, 49, 51, 37, 39, 40 };
        const int midiNote = defaultNotes[n % 12];
        proc.addTrack ("DRUM " + juce::String (n + 1), midiNote);
        selectTrack (proc.getNumActiveTracks() - 1);
    };
    addTrackBtn.setTooltip ("Add a new drum track to the kit");
    addAndMakeVisible (addTrackBtn);

    // Detail panel
    addAndMakeVisible (detailPanel);

    // Master volume
    masterVolKnob.setLookAndFeel (&laf);
    masterVolKnob.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    masterVolKnob.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 55, 14);
    masterVolKnob.setColour (juce::Slider::textBoxTextColourId,       InfernoLookAndFeel::dimText());
    masterVolKnob.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    masterVolKnob.setColour (juce::Slider::textBoxOutlineColourId,    juce::Colours::transparentBlack);
    masterVolKnob.setTooltip ("Master output volume for all tracks");
    addAndMakeVisible (masterVolKnob);

    masterVolLabel.setText ("MASTER", juce::dontSendNotification);
    masterVolLabel.setFont (juce::Font (juce::FontOptions ("Arial", 10.0f, juce::Font::bold)));
    masterVolLabel.setColour (juce::Label::textColourId, InfernoLookAndFeel::dimText());
    masterVolLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (masterVolLabel);

    masterVolAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        proc.getAPVTS(), DeathDealerDrumsAudioProcessor::PARAM_MASTER_VOLUME, masterVolKnob);

    // Mic bleed knob ? header, right of preset controls
    bleedKnob.setLookAndFeel (&laf);
    bleedKnob.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    bleedKnob.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 46, 13);
    bleedKnob.setColour (juce::Slider::textBoxTextColourId,       InfernoLookAndFeel::dimText());
    bleedKnob.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    bleedKnob.setColour (juce::Slider::textBoxOutlineColourId,    juce::Colours::transparentBlack);
    bleedKnob.setTooltip ("Global mic bleed amount � simulates crosstalk between microphones");
    addAndMakeVisible (bleedKnob);

    bleedLabel.setText ("MIC BLEED", juce::dontSendNotification);
    bleedLabel.setFont (juce::Font (juce::FontOptions ("Arial", 9.0f, juce::Font::bold)));
    bleedLabel.setColour (juce::Label::textColourId, InfernoLookAndFeel::dimText());
    bleedLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (bleedLabel);

    bleedAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        proc.getAPVTS(), DeathDealerDrumsAudioProcessor::PARAM_BLEED_AMOUNT, bleedKnob);

    // HUMAN ERROR knob � controls per-hit velocity scatter amount globally
    humanErrorKnob.setLookAndFeel (&laf);
    humanErrorKnob.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    humanErrorKnob.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 46, 13);
    humanErrorKnob.setColour (juce::Slider::textBoxTextColourId,       InfernoLookAndFeel::dimText());
    humanErrorKnob.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    humanErrorKnob.setColour (juce::Slider::textBoxOutlineColourId,    juce::Colours::transparentBlack);
    humanErrorKnob.textFromValueFunction = [] (double v)
    {
        return juce::String (juce::roundToInt (v * 100.0)) + "%";
    };
    humanErrorKnob.setTooltip ("Human Error � random timing and velocity scatter. Set to 0 for pitch-lock mode (no micro-detuning � ideal for pitched samples like bass drops)");
    addAndMakeVisible (humanErrorKnob);

    humanErrorLabel.setText ("HUMAN ERROR", juce::dontSendNotification);
    humanErrorLabel.setFont (juce::Font (juce::FontOptions ("Arial", 9.0f, juce::Font::bold)));
    humanErrorLabel.setColour (juce::Label::textColourId, InfernoLookAndFeel::dimText());
    humanErrorLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (humanErrorLabel);

    humanErrorAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        proc.getAPVTS(), DeathDealerDrumsAudioProcessor::PARAM_HUMAN_ERROR, humanErrorKnob);

    // Bleed solo button intentionally removed from header UI per design request.

    // Room Bus controls
    roomBusLabel.setText ("ROOM BUS", juce::dontSendNotification);
    roomBusLabel.setFont (juce::Font (juce::FontOptions ("Arial", 10.0f, juce::Font::bold)));
    roomBusLabel.setColour (juce::Label::textColourId, InfernoLookAndFeel::accentBright());
    roomBusLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (roomBusLabel);

    for (auto* btn : { &roomMuteBtn, &roomSoloBtn })
    {
        btn->setLookAndFeel (&laf);
        btn->setClickingTogglesState (true);
        addAndMakeVisible (btn);
    }
    roomMuteBtn.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xffcc2200));
    roomSoloBtn.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xffcc2200));
    roomMuteBtn.setTooltip ("Mute the room reverb bus");
    roomSoloBtn.setTooltip ("Solo the room reverb bus");

    roomMuteAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        proc.getAPVTS(), "room_mute", roomMuteBtn);
    roomSoloAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        proc.getAPVTS(), "room_solo", roomSoloBtn);

    roomSoloBtn.onClick = [this]
    {
        if (!roomSoloBtn.getToggleState())
            return;
        if (auto* p = proc.getAPVTS().getParameter ("room_mute"))
            p->setValueNotifyingHost (0.0f);
    };
    roomMuteBtn.onClick = [this]
    {
        if (!roomMuteBtn.getToggleState())
            return;
        if (auto* p = proc.getAPVTS().getParameter ("room_solo"))
            p->setValueNotifyingHost (0.0f);
    };

    // Room bus gain knob
    roomGainKnob.setLookAndFeel (&laf);
    roomGainKnob.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    roomGainKnob.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 44, 13);
    roomGainKnob.setColour (juce::Slider::textBoxTextColourId,       InfernoLookAndFeel::dimText());
    roomGainKnob.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    roomGainKnob.setColour (juce::Slider::textBoxOutlineColourId,    juce::Colours::transparentBlack);
    roomGainKnob.setTooltip ("Room reverb bus output gain");
    addAndMakeVisible (roomGainKnob);

    roomGainLabel.setText ("GAIN", juce::dontSendNotification);
    roomGainLabel.setFont (juce::Font (juce::FontOptions ("Arial", 10.0f, juce::Font::bold)));
    roomGainLabel.setColour (juce::Label::textColourId, InfernoLookAndFeel::dimText());
    roomGainLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (roomGainLabel);

    roomGainAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        proc.getAPVTS(), "room_gain", roomGainKnob);

    // Output routing ? 25 outputs: 20 instrument tracks + 5 bus tracks
    roomOutputLabel.setText ("OUTPUT", juce::dontSendNotification);
    roomOutputLabel.setFont (juce::Font (juce::FontOptions ("Arial", 9.5f, juce::Font::bold)));
    roomOutputLabel.setColour (juce::Label::textColourId, InfernoLookAndFeel::dimText());
    roomOutputLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (roomOutputLabel);

    roomOutputCombo.addItem ("OUT 1  (MAIN)", 1);
    for (int i = 2; i <= 16; ++i)
        roomOutputCombo.addItem ("OUT " + juce::String (i), i);
    roomOutputCombo.setSelectedId (1, juce::dontSendNotification);
    roomOutputCombo.setLookAndFeel (&laf);
    roomOutputCombo.onChange = [this]
    {
        const int val = roomOutputCombo.getSelectedId() - 1;
        if (auto* p = proc.getAPVTS().getParameter ("room_output"))
            p->setValueNotifyingHost (p->convertTo0to1 ((float) val));
    };
    roomOutputCombo.setTooltip ("Select the audio output channel for the room bus");
    addAndMakeVisible (roomOutputCombo);

    // Output mode: Stereo / Mono L / Mono R
    roomOutputModeLabel.setText ("MODE", juce::dontSendNotification);
    roomOutputModeLabel.setFont (juce::Font (juce::FontOptions ("Arial", 9.5f, juce::Font::bold)));
    roomOutputModeLabel.setColour (juce::Label::textColourId, InfernoLookAndFeel::dimText());
    roomOutputModeLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (roomOutputModeLabel);

    roomOutputModeCombo.addItem ("STEREO", 1);
    roomOutputModeCombo.addItem ("MONO",   2);
    roomOutputModeCombo.setSelectedId (1, juce::dontSendNotification);
    roomOutputModeCombo.setLookAndFeel (&laf);
    roomOutputModeCombo.onChange = [this]
    {
        const int val = roomOutputModeCombo.getSelectedId() - 1; // 0=stereo, 1=L, 2=R
        if (auto* p = proc.getAPVTS().getParameter ("room_output_mode"))
            p->setValueNotifyingHost (p->convertTo0to1 ((float) val));
    };
    roomOutputModeCombo.setTooltip ("Set the room bus output to stereo or mono");
    addAndMakeVisible (roomOutputModeCombo);

    // -- Parallel Compression Bus ----------------------------------------------
    compBusLabel.setText ("SMASH BUS", juce::dontSendNotification);
    compBusLabel.setFont (juce::Font (juce::FontOptions ("Arial", 10.0f, juce::Font::bold)));
    compBusLabel.setColour (juce::Label::textColourId, InfernoLookAndFeel::accentBright());
    compBusLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (compBusLabel);

    for (auto* btn : { &compMuteBtn, &compSoloBtn })
    {
        btn->setLookAndFeel (&laf);
        btn->setClickingTogglesState (true);
        addAndMakeVisible (btn);
    }
    compMuteBtn.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xffcc2200));
    compSoloBtn.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xffcc2200));
    compMuteBtn.setTooltip ("Mute the parallel compression (SMASH) bus");
    compSoloBtn.setTooltip ("Solo the parallel compression (SMASH) bus");

    compMuteAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        proc.getAPVTS(), "comp_mute", compMuteBtn);
    compSoloAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        proc.getAPVTS(), "comp_solo", compSoloBtn);

    compSoloBtn.onClick = [this]
    {
        if (!compSoloBtn.getToggleState())
            return;
        if (auto* p = proc.getAPVTS().getParameter ("comp_mute"))
            p->setValueNotifyingHost (0.0f);
    };
    compMuteBtn.onClick = [this]
    {
        if (!compMuteBtn.getToggleState())
            return;
        if (auto* p = proc.getAPVTS().getParameter ("comp_solo"))
            p->setValueNotifyingHost (0.0f);
    };

    compOutputLabel.setText ("OUTPUT", juce::dontSendNotification);
    compOutputLabel.setFont (juce::Font (juce::FontOptions ("Arial", 9.5f, juce::Font::bold)));
    compOutputLabel.setColour (juce::Label::textColourId, InfernoLookAndFeel::dimText());
    compOutputLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (compOutputLabel);

    compOutputCombo.addItem ("OUT 1  (MAIN)", 1);
    for (int i = 2; i <= 16; ++i)
        compOutputCombo.addItem ("OUT " + juce::String (i), i);
    compOutputCombo.setSelectedId (1, juce::dontSendNotification);
    compOutputCombo.setLookAndFeel (&laf);
    compOutputCombo.onChange = [this]
    {
        const int val = compOutputCombo.getSelectedId() - 1;
        if (auto* p = proc.getAPVTS().getParameter ("comp_output"))
            p->setValueNotifyingHost (p->convertTo0to1 ((float) val));
    };
    addAndMakeVisible (compOutputCombo);

    compOutputModeLabel.setText ("MODE", juce::dontSendNotification);
    compOutputModeLabel.setFont (juce::Font (juce::FontOptions ("Arial", 9.5f, juce::Font::bold)));
    compOutputModeLabel.setColour (juce::Label::textColourId, InfernoLookAndFeel::dimText());
    compOutputModeLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (compOutputModeLabel);

    compOutputModeCombo.addItem ("STEREO", 1);
    compOutputModeCombo.addItem ("MONO",   2);
    compOutputModeCombo.setSelectedId (1, juce::dontSendNotification);
    compOutputModeCombo.setLookAndFeel (&laf);
    compOutputModeCombo.onChange = [this]
    {
        const int val = compOutputModeCombo.getSelectedId() - 1;
        if (auto* p = proc.getAPVTS().getParameter ("comp_output_mode"))
            p->setValueNotifyingHost (p->convertTo0to1 ((float) val));
    };
    addAndMakeVisible (compOutputModeCombo);

    // Comp output gain knob
    auto setupCompKnob = [&] (juce::Slider& s, juce::Label& l, const char* name)
    {
        s.setLookAndFeel (&laf);
        s.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 48, 13);
        s.setColour (juce::Slider::textBoxTextColourId,       InfernoLookAndFeel::dimText());
        s.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        s.setColour (juce::Slider::textBoxOutlineColourId,    juce::Colours::transparentBlack);
        addAndMakeVisible (s);
        l.setText (name, juce::dontSendNotification);
        l.setFont (juce::Font (juce::FontOptions ("Arial", 10.0f, juce::Font::bold)));
        l.setColour (juce::Label::textColourId, InfernoLookAndFeel::dimText());
        l.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (l);
    };
    setupCompKnob (compThreshKnob, compThreshLabel, "THRESH");
    setupCompKnob (compMakeupKnob, compMakeupLabel, "GAIN");
    compThreshKnob.setTooltip ("Parallel compressor threshold (dB)");
    compMakeupKnob.setTooltip ("Parallel compressor makeup gain");

    compThreshAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        proc.getAPVTS(), "comp_threshold", compThreshKnob);
    compMakeupAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        proc.getAPVTS(), "comp_makeup",    compMakeupKnob);

    // GR meter
    compGrMeter = std::make_unique<CompGrMeter> (proc.getEngine()->compGrDb);
    addAndMakeVisible (*compGrMeter);

    // -- Tape Saturation Bus ---------------------------------------------------
    satBusLabel.setText ("TAPE SAT", juce::dontSendNotification);
    satBusLabel.setFont (juce::Font (juce::FontOptions ("Arial", 10.0f, juce::Font::bold)));
    satBusLabel.setColour (juce::Label::textColourId, InfernoLookAndFeel::accentBright());
    satBusLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (satBusLabel);

    for (auto* btn : { &satMuteBtn, &satSoloBtn })
    {
        btn->setLookAndFeel (&laf);
        btn->setClickingTogglesState (true);
        addAndMakeVisible (btn);
    }
    satMuteBtn.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xffcc2200));
    satSoloBtn.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xffcc2200));
    satMuteBtn.setTooltip ("Mute the tape saturation bus");
    satSoloBtn.setTooltip ("Solo the tape saturation bus");

    satMuteAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        proc.getAPVTS(), "sat_mute", satMuteBtn);
    satSoloAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        proc.getAPVTS(), "sat_solo", satSoloBtn);

    satSoloBtn.onClick = [this]
    {
        if (!satSoloBtn.getToggleState())
            return;
        if (auto* p = proc.getAPVTS().getParameter ("sat_mute"))
            p->setValueNotifyingHost (0.0f);
    };
    satMuteBtn.onClick = [this]
    {
        if (!satMuteBtn.getToggleState())
            return;
        if (auto* p = proc.getAPVTS().getParameter ("sat_solo"))
            p->setValueNotifyingHost (0.0f);
    };

    satOutputLabel.setText ("OUTPUT", juce::dontSendNotification);
    satOutputLabel.setFont (juce::Font (juce::FontOptions ("Arial", 9.5f, juce::Font::bold)));
    satOutputLabel.setColour (juce::Label::textColourId, InfernoLookAndFeel::dimText());
    satOutputLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (satOutputLabel);

    satOutputCombo.addItem ("OUT 1  (MAIN)", 1);
    for (int i = 2; i <= 16; ++i)
        satOutputCombo.addItem ("OUT " + juce::String (i), i);
    satOutputCombo.setSelectedId (1, juce::dontSendNotification);
    satOutputCombo.setLookAndFeel (&laf);
    satOutputCombo.onChange = [this]
    {
        const int val = satOutputCombo.getSelectedId() - 1;
        if (auto* p = proc.getAPVTS().getParameter ("sat_output"))
            p->setValueNotifyingHost (p->convertTo0to1 ((float) val));
    };
    satOutputCombo.setTooltip ("Select the audio output channel for the saturation bus");
    addAndMakeVisible (satOutputCombo);

    satOutputModeLabel.setText ("MODE", juce::dontSendNotification);
    satOutputModeLabel.setFont (juce::Font (juce::FontOptions ("Arial", 9.5f, juce::Font::bold)));
    satOutputModeLabel.setColour (juce::Label::textColourId, InfernoLookAndFeel::dimText());
    satOutputModeLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (satOutputModeLabel);

    satOutputModeCombo.addItem ("STEREO", 1);
    satOutputModeCombo.addItem ("MONO",   2);
    satOutputModeCombo.setSelectedId (1, juce::dontSendNotification);
    satOutputModeCombo.setLookAndFeel (&laf);
    satOutputModeCombo.onChange = [this]
    {
        const int val = satOutputModeCombo.getSelectedId() - 1;
        if (auto* p = proc.getAPVTS().getParameter ("sat_output_mode"))
            p->setValueNotifyingHost (p->convertTo0to1 ((float) val));
    };
    satOutputModeCombo.setTooltip ("Set the saturation bus output to stereo or mono");
    addAndMakeVisible (satOutputModeCombo);

    auto setupSatKnob = [&] (juce::Slider& s, juce::Label& l, const char* name)
    {
        s.setLookAndFeel (&laf);
        s.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 48, 13);
        s.setColour (juce::Slider::textBoxTextColourId,       InfernoLookAndFeel::dimText());
        s.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        s.setColour (juce::Slider::textBoxOutlineColourId,    juce::Colours::transparentBlack);
        addAndMakeVisible (s);
        l.setText (name, juce::dontSendNotification);
        l.setFont (juce::Font (juce::FontOptions ("Arial", 10.0f, juce::Font::bold)));
        l.setColour (juce::Label::textColourId, InfernoLookAndFeel::dimText());
        l.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (l);
    };
    setupSatKnob (satDriveKnob, satDriveLabel, "DRIVE");
    setupSatKnob (satGainKnob,  satGainLabel,  "GAIN");
    satDriveKnob.setTooltip ("Tape saturation drive amount � higher = warmer/crunchier harmonics");
    satGainKnob.setTooltip  ("Tape saturation output level");

    satDriveAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        proc.getAPVTS(), "sat_drive", satDriveKnob);
    satGainAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        proc.getAPVTS(), "sat_gain",  satGainKnob);

    // Register callbacks
    proc.onTracksChanged = [this] { rebuildTrackList(); };
    detailPanel.onNameChanged = [this] (int idx)
    {
        if (idx >= 0 && idx < (int) trackRows.size())
        {
            if (auto* row = trackRows[(size_t) idx].get())
            {
                row->refresh();
                row->repaint();
            }
        }
    };
    proc.onSampleLoaded  = [this] (int idx)
    {
        // Refresh the row that just finished loading
        if (idx >= 0 && idx < (int) trackRows.size())
            if (auto* row = trackRows[(size_t) idx].get())
                row->refresh();
        // If it's the selected track, refresh the detail panel path
        if (idx == selectedTrack)
            detailPanel.setTrack (idx);
    };

    rebuildTrackList();
    startTimerHz (20);

    // Lock overlay label (hidden by default; shown over preset controls when bass drop preset active)
    lockOverlayLabel.setVisible (false);
    addAndMakeVisible (lockOverlayLabel);

    // Help button ? bottom-left corner, opens website
    helpBtn.setLookAndFeel (&laf);
    helpBtn.setColour (juce::TextButton::buttonColourId,   juce::Colour (0xff1e1e28));
    helpBtn.setColour (juce::TextButton::textColourOffId,  InfernoLookAndFeel::dimText());
    helpBtn.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff1e1e28));
    helpBtn.onClick = [] { juce::URL ("https://myinferno.online/").launchInDefaultBrowser(); };
    addAndMakeVisible (helpBtn);

    setResizable (false, false);
    uiLayoutReady = true;
    setSize (W, H);
    resized();
}

DeathDealerDrumsAudioProcessorEditor::~DeathDealerDrumsAudioProcessorEditor()
{
    proc.onTracksChanged  = nullptr;
    proc.onSampleLoaded   = nullptr;
    proc.onDemoStopped    = nullptr;
    detailPanel.onNameChanged = nullptr;
    stopTimer();
    setLookAndFeel (nullptr);
    addTrackBtn.setLookAndFeel (nullptr);
    masterVolKnob.setLookAndFeel (nullptr);
    bleedKnob.setLookAndFeel (nullptr);
    humanErrorKnob.setLookAndFeel (nullptr);
    roomMuteBtn.setLookAndFeel (nullptr);
    roomSoloBtn.setLookAndFeel (nullptr);
    roomOutputCombo.setLookAndFeel (nullptr);
    roomOutputModeCombo.setLookAndFeel (nullptr);
    roomGainKnob.setLookAndFeel (nullptr);
    compMuteBtn.setLookAndFeel (nullptr);
    compSoloBtn.setLookAndFeel (nullptr);
    compOutputCombo.setLookAndFeel (nullptr);
    compOutputModeCombo.setLookAndFeel (nullptr);
    compThreshKnob.setLookAndFeel (nullptr);
    compMakeupKnob.setLookAndFeel (nullptr);
    presetCombo    .setLookAndFeel (nullptr);
    loadPresetBtn  .setLookAndFeel (nullptr);
    savePresetBtn  .setLookAndFeel (nullptr);
    exportPresetBtn.setLookAndFeel (nullptr);
}

//==============================================================================
void DeathDealerDrumsAudioProcessorEditor::rebuildTrackList ()
{
    // Detach all existing rows from the content component
    for (auto& row : trackRows)
        trackListContent.removeChildComponent (row.get());
    trackRows.clear();

    const int n = proc.getNumActiveTracks();
    for (int i = 0; i < n; ++i)
    {
        auto row = std::make_unique<TrackRow> (i, proc, laf);
        row->onSelected = [this] (int idx) { selectTrack (idx); };
        row->onRemove   = [this] (int idx)
        {
            proc.removeTrack (idx);
            // onTracksChanged fires -> rebuildTrackList
        };
        row->setSelected (i == selectedTrack);
        trackListContent.addAndMakeVisible (row.get());
        trackRows.push_back (std::move (row));
    }

    const int contentH = juce::jmax (1, n * TrackRow::ROW_H);
    trackListContent.setSize (trackViewport.getMaximumVisibleWidth(), contentH);

    // Layout rows inside content
    for (int i = 0; i < n; ++i)
        if (auto* row = trackRows[(size_t) i].get())
            row->setBounds (0, i * TrackRow::ROW_H,
                            trackListContent.getWidth(), TrackRow::ROW_H);

    // Clamp selected track
    if (selectedTrack >= n)
    {
        selectedTrack = n - 1;
        detailPanel.setTrack (selectedTrack);
    }
}

void DeathDealerDrumsAudioProcessorEditor::selectTrack (int slotIndex)
{
    selectedTrack = slotIndex;
    for (int i = 0; i < (int) trackRows.size(); ++i)
        if (auto* row = trackRows[(size_t) i].get())
            row->setSelected (i == slotIndex);
    detailPanel.setTrack (slotIndex);
}

//==============================================================================
void DeathDealerDrumsAudioProcessorEditor::refreshPresetList()
{
    presetCombo.clear (juce::dontSendNotification);

    // ID 1 is always the cooked-in default � never depends on the file system
    presetCombo.addItem ("DEFAULT",             1);
    // ID 2 is the cooked-in SCAVENGER DRUMS factory preset
    presetCombo.addItem ("SCAVENGER DRUMS",     2);
    // IDs 3-5 are cooked-in bass drop factory presets
    presetCombo.addItem ("SHORT BASS DROPS",    3);
    presetCombo.addItem ("MEDIUM BASS DROPS",   4);
    presetCombo.addItem ("LONG BASS DROPS",     5);

    // Remaining IDs: user-saved presets from the presets folder
    const auto folder = DeathDealerDrumsAudioProcessor::getPresetsFolder();
    if (folder.exists())
    {
        juce::Array<juce::File> files;
        folder.findChildFiles (files, juce::File::findFiles, false, "*.ddd");
        files.sort();
        int id = 6;
        for (const auto& f : files)
        {
            if (f.getFileNameWithoutExtension().equalsIgnoreCase ("DEFAULT"))           continue;
            if (f.getFileNameWithoutExtension().equalsIgnoreCase ("SCAVENGER DRUMS"))   continue;
            if (f.getFileNameWithoutExtension().equalsIgnoreCase ("SHORT BASS DROPS"))  continue;
            if (f.getFileNameWithoutExtension().equalsIgnoreCase ("MEDIUM BASS DROPS")) continue;
            if (f.getFileNameWithoutExtension().equalsIgnoreCase ("LONG BASS DROPS"))   continue;
            presetCombo.addItem (f.getFileNameWithoutExtension(), id++);
        }
    }

    // Restore selection to match what the processor currently has loaded
    const juce::String& active = proc.currentPresetName;
    if (active.equalsIgnoreCase ("DEFAULT"))
    {
        presetCombo.setSelectedId (1, juce::dontSendNotification);
    }
    else if (active.equalsIgnoreCase ("SCAVENGER DRUMS"))
    {
        presetCombo.setSelectedId (2, juce::dontSendNotification);
    }
    else if (active.equalsIgnoreCase ("SHORT BASS DROPS"))
    {
        presetCombo.setSelectedId (3, juce::dontSendNotification);
    }
    else if (active.equalsIgnoreCase ("MEDIUM BASS DROPS"))
    {
        presetCombo.setSelectedId (4, juce::dontSendNotification);
    }
    else if (active.equalsIgnoreCase ("LONG BASS DROPS"))
    {
        presetCombo.setSelectedId (5, juce::dontSendNotification);
    }
    else if (active.isNotEmpty())
    {
        bool found = false;
        for (int i = 1; i < presetCombo.getNumItems(); ++i)
            if (presetCombo.getItemText (i) == active)
            {
                presetCombo.setSelectedId (i + 1, juce::dontSendNotification);
                found = true;
                break;
            }

        // If active preset is external/not yet copied, only show it if that file still exists.
        if (!found)
        {
            const juce::String activePath = proc.getActivePresetFilePath();
            const juce::File activeFile (activePath);
            const bool activeExternalExists = activePath.isNotEmpty()
                                              && activeFile.existsAsFile();

            if (activeExternalExists)
            {
                const int externalId = presetCombo.getNumItems() + 1;
                presetCombo.addItem (active, externalId);
                presetCombo.setSelectedId (externalId, juce::dontSendNotification);
            }
            else
            {
                // Stale active preset metadata (file deleted/moved): do not show ghost entries.
                proc.currentPresetName = "DEFAULT";
                presetCombo.setSelectedId (1, juce::dontSendNotification);
            }
        }
    }
    else
    {
        presetCombo.setSelectedId (1, juce::dontSendNotification);
    }

    // Lock controls when a bass drop factory preset is active
    const bool isBassDrop = active.equalsIgnoreCase ("SHORT BASS DROPS")
                         || active.equalsIgnoreCase ("MEDIUM BASS DROPS")
                         || active.equalsIgnoreCase ("LONG BASS DROPS");
    setPresetLocked (isBassDrop);
}

//==============================================================================
void DeathDealerDrumsAudioProcessorEditor::setPresetLocked (bool locked)
{
    // Global knobs � master vol stays enabled even when locked
    humanErrorKnob  .setEnabled (!locked);
    bleedKnob       .setEnabled (!locked);

    // Room bus
    roomMuteBtn     .setEnabled (!locked);
    roomSoloBtn     .setEnabled (!locked);
    roomGainKnob    .setEnabled (!locked);
    roomOutputCombo .setEnabled (!locked);
    roomOutputModeCombo.setEnabled (!locked);

    // Comp bus
    compMuteBtn     .setEnabled (!locked);
    compSoloBtn     .setEnabled (!locked);
    compThreshKnob  .setEnabled (!locked);
    compMakeupKnob  .setEnabled (!locked);
    compOutputCombo .setEnabled (!locked);
    compOutputModeCombo.setEnabled (!locked);

    // Sat bus
    satMuteBtn      .setEnabled (!locked);
    satSoloBtn      .setEnabled (!locked);
    satDriveKnob    .setEnabled (!locked);
    satGainKnob     .setEnabled (!locked);
    satOutputCombo  .setEnabled (!locked);
    satOutputModeCombo.setEnabled (!locked);

    // Save / Export (Load is fine � user can load a different preset)
    savePresetBtn   .setEnabled (!locked);
    exportPresetBtn .setEnabled (!locked);

    // Add track button
    addTrackBtn     .setEnabled (!locked);

    // Every track row
    for (auto& row : trackRows)
        if (row) row->setEnabled (!locked);

    // Detail panel � selectively disable editing controls, pads stay active
    detailPanel.setLockedForPreset (locked);

    // Lock overlay label
    if (locked)
    {
        lockOverlayLabel.setText ("PRESET LOCKED", juce::dontSendNotification);
        lockOverlayLabel.setFont (juce::Font (juce::FontOptions ("Arial", 22.0f, juce::Font::bold)));
        lockOverlayLabel.setColour (juce::Label::textColourId, juce::Colour (0xffff6600));
        lockOverlayLabel.setJustificationType (juce::Justification::centred);
        lockOverlayLabel.setVisible (true);
    }
    else
    {
        lockOverlayLabel.setVisible (false);
    }
}

//==============================================================================
void DeathDealerDrumsAudioProcessorEditor::timerCallback ()
{
    auto* engine = proc.getEngine();
    for (auto& row : trackRows)
    {
        if (row)
        {
            row->updateMidiLabel();
            row->updateMeter (engine);
        }
    }
    if (compGrMeter) compGrMeter->repaint();

    // Update bus level meters (UI-side peak hold + decay at 20 Hz)
    // kDecay=0.50 at 20Hz ? meter falls to near-zero in ~0.5s
    constexpr float kDecay = 0.50f;
    if (engine)
    {
        busPeakMaster = juce::jmax (engine->masterPeakLin.load(), busPeakMaster * kDecay);
        busPeakRoom   = juce::jmax (engine->roomPeakLin.load(),   busPeakRoom   * kDecay);
        busPeakComp   = juce::jmax (engine->compPeakLin.load(),   busPeakComp   * kDecay);
        busPeakSat    = juce::jmax (engine->satPeakLin.load(),    busPeakSat    * kDecay);
    }

    // Rotate footer brand logo like a CD while DEMO MIDI or host transport is playing
    if (proc.isDemoPlaying() || proc.isHostPlaying())
    {
        constexpr float rpm = 33.0f;
        constexpr float updatesPerSecond = 20.0f; // startTimerHz (20)
        footerLogoSpinRadians += juce::MathConstants<float>::twoPi * (rpm / 60.0f) / updatesPerSecond;
        if (footerLogoSpinRadians >= juce::MathConstants<float>::twoPi)
            footerLogoSpinRadians -= juce::MathConstants<float>::twoPi;
    }
    else
    {
        // Return to default upright state when playback stops
        footerLogoSpinRadians = 0.0f;
    }

    repaint (0, getHeight() - 90, getWidth(), 90); // repaint footer only

    // Update EQ spectrum + comp GR meter in detail panel
    detailPanel.timerUpdate (engine, (float) proc.getSampleRate());
}

void DeathDealerDrumsAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (InfernoLookAndFeel::windowBg());

    // Header ? same dark background as the rest of the window
    g.setColour (InfernoLookAndFeel::windowBg());
    g.fillRect (0, 0, getWidth(), 80);

    // Draw logo with high-quality resampling ? aspect ratio preserved
    if (logoTinted.isValid())
    {
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        g.drawImageWithin (logoTinted, 0, 2, 380, 76,
                           juce::RectanglePlacement::xLeft
                               | juce::RectanglePlacement::yMid
                               | juce::RectanglePlacement::onlyReduceInSize);
        g.setImageResamplingQuality (juce::Graphics::mediumResamplingQuality);
    }

    g.setColour (juce::Colour (0xff282830));
    g.drawHorizontalLine (79, 0.0f, (float) getWidth());

    // Footer
    const int footerY = getHeight() - 90;
    g.setColour (InfernoLookAndFeel::panelBg());
    g.fillRect (0, footerY, getWidth(), 90);
    g.setColour (InfernoLookAndFeel::accentRed().withAlpha (0.3f));
    g.drawHorizontalLine (footerY, 0.0f, (float) getWidth());

    g.setColour (InfernoLookAndFeel::dimText());
    g.setFont (juce::Font (juce::FontOptions ("Arial", 9.5f, juce::Font::bold)));
    g.drawText ("GLOBAL", 12, footerY + 4, 60, 12, juce::Justification::centredLeft, false);

    // Divider: Master | Room | Smash | Sat sections
    g.setColour (juce::Colour (0xff282830));
    g.drawVerticalLine (175,  (float)(footerY + 8), (float)(footerY + 82));
    g.drawVerticalLine (402,  (float)(footerY + 8), (float)(footerY + 82));
    g.drawVerticalLine (730,  (float)(footerY + 8), (float)(footerY + 82));
    g.drawVerticalLine (1040, (float)(footerY + 8), (float)(footerY + 82));

    // -- Bus level meters ------------------------------------------------------
    // Shared helper lambda: draws a vertical peak meter (16px wide, 74px tall)
    auto drawBusMeter = [&] (int x, float peakLin)
    {
        constexpr float minDb = -60.0f, maxDb = 0.0f;
        const float db = (peakLin > 1e-7f)
                           ? juce::jlimit (minDb, maxDb, juce::Decibels::gainToDecibels (peakLin))
                           : minDb;
        const float norm = (db - minDb) / (maxDb - minDb);
        const int mX = x, mY = footerY + 8, mW = 8, mH = 74;

        g.setColour (juce::Colour (0xff111118));
        g.fillRect (mX, mY, mW, mH);

        if (norm > 0.0f)
        {
            const int fillH = juce::roundToInt (norm * (float) mH);
            const int fillY = mY + mH - fillH;
            juce::Colour col;
            if (db > -3.0f)        col = juce::Colour (0xffff2200);
            else if (db > -12.0f)  col = juce::Colour (0xffffff00);
            else                   col = juce::Colour (0xff00cc44);
            g.setColour (col);
            g.fillRect (mX, fillY, mW, fillH);
        }

        if (db > -60.0f)
        {
            g.setColour (InfernoLookAndFeel::dimText().withAlpha (0.7f));
            g.setFont (juce::Font (juce::FontOptions ("Arial", 7.5f, juce::Font::plain)));
            g.drawText (juce::String (juce::roundToInt (db)) + "dB",
                        mX - 22, mY, 20, 9, juce::Justification::centredRight, false);
        }
    };

    // Master: far right of master section, before divider at 175
    drawBusMeter (162, busPeakMaster);
    // Room: far right of room section ? gain knob shifted left; ends ~380, meter before divider at 402
    drawBusMeter (388, busPeakRoom);
    // Smash: after GR meter (cbX+284=698, w=16 ends 714), before divider at 730
    drawBusMeter (718, busPeakComp);
    // Sat: after sat gain knob (sbX+219+60 = 738+279 = 1017), before divider at 1040
    drawBusMeter (1024, busPeakSat);

    // Divider between list and detail
    g.setColour (juce::Colour (0xff282830));
    g.drawVerticalLine (380, 80.0f, (float) footerY);

    // INFERNO TONES logo � bottom-right, silver disc (no frame/ring)
    if (infernoTonesImg.isValid())
    {
        const int logoX = 1046;
        const int logoY = footerY + 2;
        const int logoW = getWidth() - logoX - 4;
        const int logoH = 86;

        const auto placement = juce::RectanglePlacement::xRight
                             | juce::RectanglePlacement::yMid
                             | juce::RectanglePlacement::onlyReduceInSize;

        // Image is square ? work out exact rendered centre for glow
        const int   imgSide = logoH;
        const int   imgX    = logoX + logoW - imgSide;
        const float cx      = (float) imgX + imgSide * 0.5f;
        const float cy      = (float) logoY + imgSide * 0.5f;
        const float r       = imgSide * 0.5f;

        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);

        // Metallic silver disc (the disc itself, no decorative frame)
        g.setGradientFill (juce::ColourGradient (
            juce::Colour (0xfff2f2f2).withAlpha (0.5f), cx - r * 0.35f, cy - r * 0.45f,
            juce::Colour (0xff808080).withAlpha (0.5f), cx + r * 0.35f, cy + r * 0.55f, true));
        g.fillEllipse (cx - r, cy - r, r * 2.f, r * 2.f);

        // Tiny soft shadow only (keeps disc readable against footer)
        g.setColour (juce::Colour (0xff000000).withAlpha (0.18f));
        g.drawEllipse (cx - r + 0.5f, cy - r + 0.5f, r * 2.f - 1.f, r * 2.f - 1.f, 1.0f);

        // Draw badge in near-black using its alpha channel
        g.setColour (juce::Colour (0xff0e0e0e));
        const float srcW = (float) infernoTonesImg.getWidth();
        const float srcH = (float) infernoTonesImg.getHeight();

        // Render square image fitted to the disc, then rotate around disc centre
        juce::AffineTransform t = juce::AffineTransform::translation (-srcW * 0.5f, -srcH * 0.5f)
                                      .scaled ((float) imgSide / srcW, (float) imgSide / srcH)
                                      .rotated (footerLogoSpinRadians)
                                      .translated (cx, cy);
        g.drawImageTransformed (infernoTonesImg, t, true);

        g.setImageResamplingQuality (juce::Graphics::mediumResamplingQuality);
    }
}

void DeathDealerDrumsAudioProcessorEditor::resized ()
{
    if (!uiLayoutReady)
        return;

    const int headerH = 80;
    const int footerH = 90;
    const int listW   = 380;
    const int contentY = headerH + 2;
    const int contentH = getHeight() - headerH - footerH - 2;
    const int footerY  = getHeight() - footerH;

    // logo drawn directly in paint() ? no component bounds needed
    brandLabel.setBounds  (getWidth() - 160, 28, 150, 28);

    // Preset bar ? centered in header between logo and brand label
    {
        const int btnH  = 26;
        const int btnY  = (80 - btnH) / 2;   // vertically centered in 80px header
        presetLabel    .setBounds (318, btnY,      56,  btnH);
        presetCombo    .setBounds (378, btnY,      300, btnH);
        loadPresetBtn  .setBounds (686, btnY,      60,  btnH);
        savePresetBtn  .setBounds (752, btnY,      60,  btnH);
        exportPresetBtn.setBounds (818, btnY,      70,  btnH);

        // Lock overlay label � dead center of the plugin
        lockOverlayLabel.setBounds (W / 2 - 200, H / 2 - 30, 400, 60);

        // Mic bleed knob ? right of preset bar, before brand label
        bleedLabel     .setBounds (895, 2,  62, 11);
        bleedKnob      .setBounds (899, 12, 50, 60);
        humanErrorLabel.setBounds (952, 2,  98, 11);
        humanErrorKnob .setBounds (976, 12, 50, 60);
    }

    // Left: track list + add button
    const int addBtnH = 30;
    const int panelLabelH = 20;
    const int listLabelY = contentY;
    const int addBtnY  = footerY - addBtnH - 4;
    const int vpH      = addBtnY - listLabelY - panelLabelH - 2;

    // "TRACKS" label painted inline in paint() � just layout viewport + button
    trackViewport.setBounds (2, listLabelY + panelLabelH, listW - 4, vpH);
    trackListContent.setSize (juce::jmax (1, trackViewport.getMaximumVisibleWidth()),
        juce::jmax (1, (int) trackRows.size() * TrackRow::ROW_H));
    for (int i = 0; i < (int) trackRows.size(); ++i)
        if (auto* row = trackRows[(size_t) i].get())
            row->setBounds (0, i * TrackRow::ROW_H,
                            trackListContent.getWidth(), TrackRow::ROW_H);
    addTrackBtn.setBounds (2, addBtnY, listW - 4, addBtnH);

    // Right: detail panel
    detailPanel.setBounds (listW + 4, contentY, getWidth() - listW - 6, contentH);

    // Footer: master vol + room bus
    masterVolLabel.setBounds (80, footerY + 4,  70, 14);
    masterVolKnob.setBounds  (80, footerY + 18, 70, 60);

    // Help button ? bottom-left corner
    helpBtn.setBounds (4, footerY + 68, 22, 18);

    // Room Bus section: ROOM BUS label | SOLO+MUTE | OUTPUT+MODE side-by-side | GAIN knob
    const int rbX = 180;
    roomBusLabel.setBounds       (rbX,       footerY + 4,  164, 12);
    roomSoloBtn.setBounds        (rbX,       footerY + 18,  54, 20);
    roomMuteBtn.setBounds        (rbX + 58,  footerY + 18,  54, 20);
    roomOutputLabel.setBounds    (rbX,       footerY + 42,  80, 12);
    roomOutputCombo.setBounds    (rbX,       footerY + 55,  80, 18);
    roomOutputModeLabel.setBounds(rbX + 84,  footerY + 42,  64, 12);
    roomOutputModeCombo.setBounds(rbX + 84,  footerY + 55,  64, 18);
    roomGainLabel.setBounds      (rbX + 146, footerY + 4,   52, 12);
    roomGainKnob.setBounds       (rbX + 142, footerY + 16,  58, 66);

    // Smash Bus (parallel compression): mirrored layout, 3 knobs on the right
    // Section starts at x=414, after room bus right divider at 402 + 12px gap
    const int cbX = 414;
    compBusLabel.setBounds       (cbX,       footerY + 4,  164, 12);
    compSoloBtn.setBounds        (cbX,       footerY + 18,  54, 20);
    compMuteBtn.setBounds        (cbX + 58,  footerY + 18,  54, 20);
    compOutputLabel.setBounds    (cbX,       footerY + 42,  80, 12);
    compOutputCombo.setBounds    (cbX,       footerY + 55,  80, 18);
    compOutputModeLabel.setBounds(cbX + 84,  footerY + 42,  64, 12);
    compOutputModeCombo.setBounds(cbX + 84,  footerY + 55,  64, 18);
    // Two knobs: THRESH | MAKEUP GAIN, plus GR meter
    compThreshLabel.setBounds    (cbX + 160, footerY + 4,   56, 12);
    compThreshKnob.setBounds     (cbX + 157, footerY + 16,  60, 66);
    compMakeupLabel.setBounds    (cbX + 222, footerY + 4,   56, 12);
    compMakeupKnob.setBounds     (cbX + 219, footerY + 16,  60, 66);
    if (compGrMeter)
        compGrMeter->setBounds   (cbX + 284, footerY + 8,   16, 74);

    // Tape Saturation Bus: starts at x=738 (after SMASH right divider at 730)
    const int sbX = 738;
    satBusLabel.setBounds        (sbX,       footerY + 4,  164, 12);
    satSoloBtn.setBounds         (sbX,       footerY + 18,  54, 20);
    satMuteBtn.setBounds         (sbX + 58,  footerY + 18,  54, 20);
    satOutputLabel.setBounds     (sbX,       footerY + 42,  80, 12);
    satOutputCombo.setBounds     (sbX,       footerY + 55,  80, 18);
    satOutputModeLabel.setBounds (sbX + 84,  footerY + 42,  64, 12);
    satOutputModeCombo.setBounds (sbX + 84,  footerY + 55,  64, 18);
    satDriveLabel.setBounds      (sbX + 160, footerY + 4,   56, 12);
    satDriveKnob.setBounds       (sbX + 157, footerY + 16,  60, 66);
    satGainLabel.setBounds       (sbX + 222, footerY + 4,   56, 12);
    satGainKnob.setBounds        (sbX + 219, footerY + 16,  60, 66);

    // DEMO button � in footer gap between sat bus and INFERNO TONES logo
    demoBtn.setBounds (1044, footerY + 31, 58, 28);
}
