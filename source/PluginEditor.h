#pragma once

#include <JuceHeader.h>
#include <vector>
#include <memory>
#include "PluginProcessor.h"

//==============================================================================
class InfernoLookAndFeel : public juce::LookAndFeel_V4
{
public:
    InfernoLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h,
                           float sliderPos, float startAngle, float endAngle,
                           juce::Slider&) override;

    void drawButtonBackground (juce::Graphics&, juce::Button&,
                                const juce::Colour&,
                                bool highlighted, bool down) override;

    void drawButtonText (juce::Graphics&, juce::TextButton&,
                         bool highlighted, bool down) override;

    juce::Font getLabelFont (juce::Label&) override;

    void drawComboBox (juce::Graphics&, int w, int h, bool,
                       int bx, int by, int bw, int bh,
                       juce::ComboBox&) override;

    static juce::Colour accentRed()    noexcept { return juce::Colour (0xffcc1100); }
    static juce::Colour accentBright() noexcept { return juce::Colour (0xffff3300); }
    static juce::Colour panelBg()      noexcept { return juce::Colour (0xff111115); }
    static juce::Colour windowBg()     noexcept { return juce::Colour (0xff0a0a0f); }
    static juce::Colour textColour()   noexcept { return juce::Colour (0xffe8e8e8); }
    static juce::Colour dimText()      noexcept { return juce::Colour (0xff888888); }
};

//==============================================================================
/** Industry-standard log-frequency spectrum analyzer with EQ curve overlay. */
class SpectrumDisplay : public juce::Component
{
public:
    SpectrumDisplay() = default;
    void setData     (const float* bins, int numBins, float sampleRateHz);
    /** Pass 8 biquad coefficients + per-band freq/gain + per-band passes count for HPF/LPF slope. */
    void setEQCoeffs (const BiquadCoeffs* coeffs8,
                      const float*        freqs8,
                      const float*        gains8,
                      const int*          passes8,
                      float               sampleRateHz);
    void paint     (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp   (const juce::MouseEvent& e) override;

    /** Called when the user drags a band dot: (bandIndex, newFreq, newGain). */
    std::function<void(int, float, float)> onBandDragged;

    /** Freq / gain ranges for clamping drag values. */
    float freqMin { 20.f }, freqMax { 22000.f };
    float gainMin { -18.f }, gainMax { 18.f };

private:
    std::vector<float> magnitudesDb;
    float sr { 44100.f };
    // EQ curve
    BiquadCoeffs eqBands[8] {};
    float        eqFreqs[8] {};
    float        eqGains[8] {};
    int          eqPasses[8] {};
    float        eqSr  { 44100.f };
    bool         hasEQ { false };
    int          dragBand { -1 };
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpectrumDisplay)
};

//==============================================================================
/** 8-band parametric EQ panel with spectrum analyzer, wired to APVTS. */
class TrackEQPanel : public juce::Component
{
public:
    static constexpr int NUM_BANDS = 8;

    TrackEQPanel (DeathDealerDrumsAudioProcessor& proc, InfernoLookAndFeel& laf);

    void setTrack        (int slotIndex);
    /** Call every timer tick — updates spectrum FFT data AND EQ curve overlay. */
    void timerTick       (DrumEngine* engine, float sampleRate);
    void paint   (juce::Graphics& g) override;
    void resized () override;

private:
    int currentSlot { -1 };
    DeathDealerDrumsAudioProcessor& proc;
    InfernoLookAndFeel& laf;

    SpectrumDisplay spectrumDisplay;

    juce::TextButton eqEnableBtn { "ON" };

    juce::Slider freqKnob[NUM_BANDS];
    juce::Slider gainKnob[NUM_BANDS];
    juce::Slider qKnob   [NUM_BANDS];

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>
        freqAtt[NUM_BANDS], gainAtt[NUM_BANDS], qAtt[NUM_BANDS];
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>
        eqEnableAtt;

    void rebuildAttachments();
    static const char* bandName (int b) noexcept;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrackEQPanel)
};

//==============================================================================
/** Per-track feed-forward compressor panel with drum presets. */
class TrackCompPanel : public juce::Component
{
public:
    TrackCompPanel (DeathDealerDrumsAudioProcessor& proc, InfernoLookAndFeel& laf);

    void setTrack    (int slotIndex);
    void updateMeter (DrumEngine* engine);  ///< Call from timer tick
    void paint   (juce::Graphics& g) override;
    void resized () override;

private:
    int   currentSlot { -1 };
    float grMeter     { 0.0f };  ///< UI-side smoothed GR value

    DeathDealerDrumsAudioProcessor& proc;
    InfernoLookAndFeel& laf;

    juce::TextButton enableBtn { "ON" };
    juce::ComboBox   presetCombo;
    juce::Label      presetLabel;

    juce::Slider thrKnob, ratKnob, atkKnob, relKnob, mkpKnob;
    juce::Label  thrLabel, ratLabel, atkLabel, relLabel, mkpLabel;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>
        thrAtt, ratAtt, atkAtt, relAtt, mkpAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> enableAtt;

    void rebuildAttachments();
    void syncPresetComboToTrack();
    void applyPreset (int id);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrackCompPanel)
};

//==============================================================================
/** Gain-reduction meter — reads an atomic<float> from the engine (GR in dB, 0 = no reduction). */
class CompGrMeter : public juce::Component
{
public:
    explicit CompGrMeter (std::atomic<float>& grSource) : src (grSource) {}

    void paint (juce::Graphics& g) override
    {
        const float grDb = juce::jlimit (-40.0f, 0.0f, src.load());

        // Background
        g.fillAll (juce::Colour (0xff0a0a0f));

        // Scale: 0 dB at top, -40 dB at bottom
        const float ratio    = (-grDb) / 40.0f;   // 0..1, 1 = most reduction
        const int   barH     = juce::roundToInt (ratio * (float) getHeight());
        const juce::Rectangle<int> bar (0, getHeight() - barH, getWidth(), barH);

        // Colour: green → yellow → red as GR increases
        const juce::Colour col = (ratio < 0.4f) ? juce::Colour (0xff22cc44)
                               : (ratio < 0.75f) ? juce::Colour (0xffdd2200)
                                                  : juce::Colour (0xffcc1100);
        g.setColour (col);
        g.fillRect (bar);

        // Border
        g.setColour (juce::Colour (0xff282830));
        g.drawRect (getLocalBounds());

        // Label "GR"
        g.setColour (juce::Colour (0xff888888));
        g.setFont (juce::Font (juce::FontOptions ("Arial", 8.0f, juce::Font::bold)));
        g.drawText ("GR", getLocalBounds().removeFromTop (10), juce::Justification::centred, false);
    }

private:
    std::atomic<float>& src;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CompGrMeter)
};

//==============================================================================
/** Lightweight waveform thumbnail drawn from a downsampled peak buffer. */
class WaveformDisplay : public juce::Component
{
public:
    WaveformDisplay() = default;

    /** Load waveform data from an AudioBuffer (channel 0, downsampled to peaks).
        Pass nullptr to clear. */
    void loadFrom (const juce::AudioBuffer<float>* buf);
    void setTrimRange (float startNorm, float endNorm);

    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp   (const juce::MouseEvent& e) override;

    std::function<void(float, float)> onTrimChanged;

private:
    static constexpr int MAX_PEAKS = 512;
    static constexpr float HANDLE_RADIUS = 5.0f;
    static constexpr float HANDLE_HIT_RADIUS = 9.0f;
    std::vector<float> peaks;   ///< Per-column peak absolute value, 0..1
    float trimStartNorm { 0.0f };
    float trimEndNorm   { 1.0f };
    enum class DragHandle { None, Start, End };
    DragHandle dragHandle { DragHandle::None };
    bool  dragZoomActive { false };
    float dragZoomViewStartNorm { 0.0f };
    float dragZoomViewSpanNorm  { 1.0f };
    static constexpr float DRAG_ZOOM_FACTOR = 2.2f;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WaveformDisplay)
};

//==============================================================================
/**
 *  One row in the scrollable track list.
 *  Slot index maps directly to APVTS parameter names ("track_i_volume" etc.)
 */
class TrackRow : public juce::Component,
                 public juce::Button::Listener
{
public:
    static constexpr int ROW_H = 116;

    TrackRow (int slotIndex,
              DeathDealerDrumsAudioProcessor& proc,
              InfernoLookAndFeel& laf);
    ~TrackRow() override;

    void paint   (juce::Graphics&) override;
    void resized () override;
    void mouseDown (const juce::MouseEvent&) override;
    void buttonClicked (juce::Button*) override;

    void setSelected (bool s) { selected = s; repaint(); }
    void refresh ();   ///< Pull track name + sample path from processor
    void updateMidiLabel ();
    void updateMeter (DrumEngine* engine);

    std::function<void(int)> onSelected;
    std::function<void(int)> onRemove;

private:
    int  slotIndex;
    bool selected { false };

    DeathDealerDrumsAudioProcessor& proc;
    InfernoLookAndFeel& laf;

    juce::Label      nameLabel;      ///< Double-click to rename
    juce::Label      midiLabel;      ///< Shows assigned note (read-only here)
    juce::TextButton loadBtn  { "LOAD" };
    juce::TextButton removeBtn{ "X"    };

    juce::Slider volKnob, panKnob;
    juce::Label  volLabel, panLabel;
    juce::Label  fileLabel;

    // Per-track routing
    juce::TextButton soloBtn { "S" };
    juce::TextButton muteBtn { "M" };
    juce::ComboBox   outputCombo;
    juce::ComboBox   outputModeCombo;
    juce::ComboBox   slaveCombo;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> volAtt, panAtt, bleedSendAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> soloAtt, muteAtt, phaseAtt, bleedAtt;

    // Phase invert
    juce::TextButton phaseBtn { juce::String::charToString (0x00F8) }; // ø symbol

    // Mic-bleed enable + send level
    juce::TextButton bleedBtn { "BLEED" };
    juce::Slider     bleedSendKnob;

    // Level meter
    float meterLevel { 0.0f };   ///< Current displayed peak (linear, decays in UI timer)
    bool  updatingSlaveCombo { false };

    void rebuildSlaveCombo ();
    void updateSlaveComboAppearance ();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrackRow)
};

//==============================================================================
/** Right-panel detail editor for the selected track. */
class TrackDetailPanel : public juce::Component,
                          public juce::FileDragAndDropTarget
{
public:
    TrackDetailPanel (DeathDealerDrumsAudioProcessor& proc, InfernoLookAndFeel& laf);
    ~TrackDetailPanel() override;

    /** Set to -1 to show "no track selected" state. */
    void setTrack (int slotIndex);

    /** Called from editor timer to refresh spectrum and comp GR. */
    void timerUpdate (DrumEngine* engine, float sampleRate);

    void paint   (juce::Graphics&) override;
    void resized () override;

    // FileDragAndDropTarget
    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped           (const juce::StringArray& files, int x, int y) override;
    void fileDragEnter          (const juce::StringArray& files, int x, int y) override;
    void fileDragExit           (const juce::StringArray& files) override;

    std::function<void(int)> onNameChanged; ///< Called with slot index when track is renamed

private:
    int currentSlot { -1 };

    DeathDealerDrumsAudioProcessor& proc;
    InfernoLookAndFeel& laf;

    juce::Label nameEditLabel;   ///< Double-click to rename the track

    juce::Slider tuneKnob, decayKnob, attackKnob;
    juce::Label  tuneLabel, decayLabel, attackLabel;

    juce::Label      midiNoteLabel;
    juce::TextButton midiDownBtn { "-" }, midiUpBtn { "+" };

    juce::TextButton loadSampleBtn { "LOAD SAMPLE" };
    juce::Label      samplePathLabel;

    // Sample mode + pad trigger area
    juce::Label    sampleModeLabel, padAreaLabel;
    juce::ComboBox sampleModeCombo;
    std::vector<std::unique_ptr<juce::TextButton>> padButtons;

    // Reverb send
    juce::Slider reverbSendKnob;
    juce::Label  reverbSendLabel;

    // Parallel comp send
    juce::Slider compSendKnob;
    juce::Label  compSendLabel;

    // Tape saturation send
    juce::Slider satSendKnob;
    juce::Label  satSendLabel;

    // Choke toggle
    juce::ToggleButton chokeButton;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>
        tuneAtt, decayAtt, attackAtt, reverbSendAtt, compSendAtt, satSendAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>
        chokeAtt;

    bool dragHovering { false };   ///< True while a valid file is hovering over the panel

    WaveformDisplay waveformDisplay;

    // Velocity tier selector for pad view: SOFT / MID / HARD
    juce::TextButton tierBtn[NUM_VEL_TIERS];  ///< "SOFT", "MID", "HARD"
    int              activeTier { 2 };         ///< 0=SOFT, 1=MID, 2=HARD

    /// Trigger feedback: counts down from kFlashFrames to 0, one per pad slot.
    static constexpr int kFlashFrames = 3;     ///< ~150ms at 20Hz timer
    int padFlashCountdown[VARS_PER_TIER] {};   ///< Per-slot flash counter

    void rebuildAttachments ();
    void updateMidiNoteLabel ();
    void syncWaveTrimFromParams ();
    void rebuildPads ();
    void setupKnob  (juce::Slider& s, bool bipolar = false);
    void setupLabel (juce::Label& l);

    // EQ + Compressor panels (right column)
    juce::TextButton eqTabBtn   { "EQ"   };
    juce::TextButton compTabBtn { "COMP" };
    int              activeDetailTab { 0 }; // 0 = EQ, 1 = COMP
    TrackEQPanel     eqPanel;
    TrackCompPanel   compPanel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrackDetailPanel)
};

//==============================================================================
class DeathDealerDrumsAudioProcessorEditor : public juce::AudioProcessorEditor,
                                              private juce::Timer
{
public:
    explicit DeathDealerDrumsAudioProcessorEditor (DeathDealerDrumsAudioProcessor&);
    ~DeathDealerDrumsAudioProcessorEditor() override;

    void paint   (juce::Graphics&) override;
    void resized () override;

private:
    void timerCallback    () override;
    void rebuildTrackList ();
    void selectTrack (int slotIndex);

    static constexpr int W = 1200;
    static constexpr int H = 860;

    DeathDealerDrumsAudioProcessor& proc;
    InfernoLookAndFeel              laf;

    juce::Image           logoTinted;
    juce::Image           infernoTonesImg;
    juce::Label           brandLabel;
    juce::TextButton      demoBtn { "DEMO" };

    // Header preset bar
    juce::Label           presetLabel;
    juce::ComboBox        presetCombo;
    juce::TextButton      loadPresetBtn  { "LOAD" };
    juce::TextButton      savePresetBtn  { "SAVE" };
    juce::TextButton      exportPresetBtn { "EXPORT" };
    void refreshPresetList();

    // Mic bleed intensity knob (header, right of preset bar)
    juce::Slider     bleedKnob;
    juce::Label      bleedLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> bleedAtt;

    // Global velocity scatter amount (header, right side)
    juce::Slider     humanErrorKnob;
    juce::Label      humanErrorLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> humanErrorAtt;

    // Left panel â€” scrollable track list
    juce::Viewport  trackViewport;
    juce::Component trackListContent;
    std::vector<std::unique_ptr<TrackRow>> trackRows;
    juce::TextButton addTrackBtn { "+ ADD TRACK" };

    // Right panel â€” detail
    TrackDetailPanel detailPanel;

    // Footer
    juce::Slider     masterVolKnob;
    juce::Label      masterVolLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> masterVolAtt;

    // Room Bus controls (footer)
    juce::Label      roomBusLabel;
    juce::TextButton roomMuteBtn  { "MUTE" };
    juce::TextButton roomSoloBtn  { "SOLO" };
    juce::ComboBox   roomOutputCombo;
    juce::Label      roomOutputLabel;
    juce::ComboBox   roomOutputModeCombo;
    juce::Label      roomOutputModeLabel;
    juce::Slider     roomGainKnob;
    juce::Label      roomGainLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>
        roomMuteAtt, roomSoloAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>
        roomGainAtt;

    // Parallel Compression Bus controls (footer)
    juce::Label      compBusLabel;
    juce::TextButton compMuteBtn  { "MUTE" };
    juce::TextButton compSoloBtn  { "SOLO" };
    juce::ComboBox   compOutputCombo;
    juce::Label      compOutputLabel;
    juce::ComboBox   compOutputModeCombo;
    juce::Label      compOutputModeLabel;
    juce::Slider     compThreshKnob;
    juce::Label      compThreshLabel;
    juce::Slider     compMakeupKnob;
    juce::Label      compMakeupLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>
        compMuteAtt, compSoloAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>
        compThreshAtt, compMakeupAtt;
    std::unique_ptr<CompGrMeter> compGrMeter;

    // Tape Saturation Bus controls (footer)
    juce::Label      satBusLabel;
    juce::TextButton satMuteBtn   { "MUTE" };
    juce::TextButton satSoloBtn   { "SOLO" };
    juce::ComboBox   satOutputCombo;
    juce::Label      satOutputLabel;
    juce::ComboBox   satOutputModeCombo;
    juce::Label      satOutputModeLabel;
    juce::Slider     satDriveKnob;
    juce::Label      satDriveLabel;
    juce::Slider     satGainKnob;
    juce::Label      satGainLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>
        satMuteAtt, satSoloAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>
        satDriveAtt, satGainAtt;

    int selectedTrack { -1 };

    juce::TextButton helpBtn { "?" };

    // Smoothed peak levels for bus meters (read in timerCallback, drawn in paint)
    float busPeakMaster { 0.0f };
    float busPeakRoom   { 0.0f };
    float busPeakComp   { 0.0f };
    float busPeakSat    { 0.0f };

    // Bottom-right brand logo spin angle (radians), animated while DEMO is playing
    float footerLogoSpinRadians { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DeathDealerDrumsAudioProcessorEditor)
};

