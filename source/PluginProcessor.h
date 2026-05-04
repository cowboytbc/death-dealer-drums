#pragma once

#include <JuceHeader.h>
#include <vector>
#include <memory>
#include "DrumEngine.h"   // also pulls in DrumPad.h

//==============================================================================
/** AudioParameterBool that declares itself as a VST3 meta-parameter.
    Required whenever setting this parameter causes other parameters to change
    their values (e.g. Solo clears Mute, Mute clears Solo).
    Without this flag the VST3 validator emits:
      "parameter values are different since last set" */
struct MetaBoolParam : public juce::AudioParameterBool
{
    using juce::AudioParameterBool::AudioParameterBool;
    bool isMetaParameter() const override { return true; }
};

//==============================================================================
class DeathDealerDrumsAudioProcessor : public juce::AudioProcessor,
                                      private juce::AudioProcessorValueTreeState::Listener
{
public:
    //==========================================================================
    static constexpr int MAX_TRACKS = 20;

    /** Builds a parameter ID like "track_2_volume". */
    static juce::String trackParamID (int slot, const char* name)
    {
        return "track_" + juce::String (slot) + "_" + name;
    }
    static juce::String trackParamID (int slot, const juce::String& name)
    {
        return "track_" + juce::String (slot) + "_" + name;
    }

    static constexpr const char* PARAM_MASTER_VOLUME = "master_volume";
    static constexpr const char* PARAM_BLEED_AMOUNT   = "bleed_amount";
    static constexpr const char* PARAM_BLEED_SOLO     = "bleed_solo";
    static constexpr const char* PARAM_HUMAN_ERROR    = "human_error";

    //==========================================================================
    DeathDealerDrumsAudioProcessor();
    ~DeathDealerDrumsAudioProcessor() override;

    //==========================================================================
    void prepareToPlay  (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock   (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi()  const override { return true;  }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 1.5; }

    int  getNumPrograms()                             override { return 1; }
    int  getCurrentProgram()                          override { return 0; }
    void setCurrentProgram (int)                      override {}
    const juce::String getProgramName (int)           override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int)   override;

    //==========================================================================
    juce::AudioProcessorValueTreeState& getAPVTS() { return apvts; }
    juce::AudioFormatManager& getFormatManager()   { return formatManager; }
    DrumEngine* getEngine() noexcept               { return engine.get(); }

    //==========================================================================
    // Track management (message-thread only)
    int        getNumActiveTracks() const;
    void       addTrack    (const juce::String& name = "DRUM", int midiNote = 60);
    void       removeTrack (int index);
    void       renameTrack (int index, const juce::String& name);
    DrumTrack* getTrack    (int index);

    /** Loads sample on background thread; fires onSampleLoaded(trackIndex) on message thread. */
    void loadSampleForTrack (int trackIndex, const juce::File& file);
    /** Thread-safe: queue a pad preview trigger (message thread → audio thread). */
    void triggerPreview (int trackIndex, int varIndex, float velocity = 1.0f);

    // Preset management — .ddd files are plain XML state snapshots
    static juce::File getPresetsFolder();
    void savePreset (const juce::File& file, bool setAsCurrent = true);
    bool loadPreset (const juce::File& file);

    juce::String currentPresetName;  // set to the active preset's file stem (no extension)
    juce::String getActivePresetFilePath() const noexcept { return activePresetFilePath; }

    // Demo playback — triggered by the DEMO button in the footer
    void startDemo() noexcept;
    void stopDemo()  noexcept;
    bool isDemoPlaying() const noexcept { return demoPlaying.load(); }
    bool isHostPlaying() const noexcept { return hostPlaying.load(); }

    // Expose copyXmlToBinary for the editor (base class method is protected)
    using juce::AudioProcessor::copyXmlToBinary;

    // Callbacks — set by editor
    std::function<void()>    onTracksChanged;
    std::function<void(int)> onSampleLoaded;
    std::function<void()>    onDemoStopped;   // called (message thread) when demo finishes

private:
    void parameterChanged (const juce::String& parameterID, float newValue) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    juce::AudioFormatManager    formatManager;
    std::unique_ptr<DrumEngine> engine;
    juce::AudioProcessorValueTreeState apvts;
    std::atomic<bool> soloMuteGuardActive { false };

    mutable juce::ReadWriteLock            tracksLock;
    std::vector<std::unique_ptr<DrumTrack>> tracks;

    juce::ThreadPool backgroundLoader { 1 };
    double currentSampleRate { 44100.0 };

    // Demo player (audio-thread state)
    juce::MidiMessageSequence demoSequence;
    std::atomic<bool>         demoPlaying { false };
    std::atomic<bool>         hostPlaying { false };
    juce::int64               demoSamplePos { 0 };
    int                       demoTotalSamples { 0 };

    // Demo lane mapping: captures note lanes at demo start and tracks them through slot deletions.
    std::array<int, MAX_TRACKS> demoLaneSourceNotes {}; // lane -> source MIDI note used by DEMO.mid at start
    std::array<int, MAX_TRACKS> demoLaneToCurrentSlot {}; // lane -> current track slot, -1 when removed
    int demoLaneCount { 0 };
    juce::String activePresetFilePath; // full path of currently loaded/saved preset, empty for built-in DEFAULT
    void captureDemoLaneMapping();
    void updateDemoLaneMappingAfterTrackRemoval (int removedSlot) noexcept;

    // Lock-free preview trigger queue (UI pads → audio thread)
    struct PreviewItem { int trackIndex, varIndex; float velocity; };
    static constexpr int PREVIEW_FIFO_SIZE = 32;
    juce::AbstractFifo previewFifo { PREVIEW_FIFO_SIZE };
    std::array<PreviewItem, PREVIEW_FIFO_SIZE> previewBuf;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DeathDealerDrumsAudioProcessor)
};

