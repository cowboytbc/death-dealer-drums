#pragma once

#include <JuceHeader.h>

static constexpr int MAX_TRACKS    = 20;
static constexpr int NUM_VEL_TIERS  = 3;   ///< SOFT / MID / HARD
static constexpr int VARS_PER_TIER  = 8;   ///< Round-robin variations per tier
static constexpr int NUM_VARIATIONS = NUM_VEL_TIERS * VARS_PER_TIER; // 24 total

//==============================================================================
/** One pre-generated sample variation used for round-robin playback. */
struct DrumVariation
{
    juce::AudioBuffer<float> buffer;
    double sampleRate { 44100.0 };
    bool   valid      { false  };
};

//==============================================================================
/**
 *  DrumTrack — one named drum sound with 8 auto-generated subtle variations.
 *
 *  Loading one sample generates 8 variations offline:
 *    pitch-shift ±1-5 cents, high-shelf EQ tweaks ±1.5-3 dB,
 *    subtle gain changes ±1.5 dB, start-offset trimming 0-3 ms.
 *  The audio thread picks variations in round-robin order —
 *  zero machine-gunning, zero extra run-time DSP cost.
 */
class DrumTrack
{
public:
    DrumTrack (const juce::String& name = "DRUM", int midiNote = 60);

    // ── Identity ──────────────────────────────────────────────────────────────
    void         setName (const juce::String& n) { name = n; }
    juce::String getName () const                { return name; }

    // ── Sample management (background thread) ────────────────────────────────
    bool loadSampleAndGenerateVariations (const juce::File&          file,
                                          double                      targetSampleRate,
                                          juce::AudioFormatManager&   formatManager);

    /** Load from in-memory data (e.g. BinaryData built-in samples). */
    bool loadSampleAndGenerateVariations (const void*                 data,
                                          int                         dataSizeBytes,
                                          const juce::String&         displayPath,
                                          double                      targetSampleRate,
                                          juce::AudioFormatManager&   formatManager);

    bool         hasSample    () const { return hasCustomSample; }
    juce::String getSamplePath() const { return sampleFilePath;  }

    /** Raw WAV bytes of the loaded sample — populated when loading from file.
        Used to embed samples in preset .ddd files so presets are self-contained. */
    const juce::MemoryBlock& getRawSampleData() const { return rawSampleData; }

    // ── Round-robin (audio-thread safe) ──────────────────────────────────────
    /** Velocity-aware round-robin: picks tier by velocity then round-robins within it.
        vel 0.00–0.33 = SOFT (slots 0–7), 0.34–0.66 = MID (8–15), 0.67–1.0 = HARD (16–23). */
    const DrumVariation* getNextVariationForVelocity (float velocity);
    /** Returns next variation using legacy tier-agnostic round-robin (Single/Multi mode). */
    const DrumVariation* getNextVariation ();
    void resetRoundRobin () { lastVariation.store (-1);
                              for (auto& t : lastTierVariation) t.store (-1); }

    /// Set by getNextVariationForVelocity — readable by the UI for trigger feedback.
    std::atomic<int> lastUsedTier { -1 };
    std::atomic<int> lastUsedSlot { -1 };

    /** Get a specific variation by absolute index 0-23 (for pad preview triggers). */
    const DrumVariation* getVariation (int index) const;
    /** Get a variation by tier (0=SOFT,1=MID,2=HARD) and slot 0-7 (for pad preview). */
    const DrumVariation* getVariationForTier (int tier, int slot) const;

    // ── Sample mode ───────────────────────────────────────────────────────────
    enum class SampleMode { Single = 0, Multi = 1 };
    void       setSampleMode (SampleMode m, int numVars = 1);
    SampleMode getSampleMode ()  const { return sampleMode; }
    int        getNumVariants () const { return numVariants; }

private:
    juce::String     name;
    bool             hasCustomSample { false };
    juce::String     sampleFilePath;
    juce::MemoryBlock rawSampleData;  ///< Original WAV bytes (empty for built-in samples)

    DrumVariation    variations[NUM_VARIATIONS];  ///< 24 total: [tier*8 + slot]
    std::atomic<int> lastVariation { -1 };         ///< Legacy round-robin index
    std::atomic<int> lastTierVariation[NUM_VEL_TIERS];  ///< Per-tier last-played slot
    juce::Random     rng;
    juce::ReadWriteLock lock;

    SampleMode sampleMode  { SampleMode::Single };
    int        numVariants { 1 };

    void generateVariations (const juce::AudioBuffer<float>& source, double sampleRate);

    static juce::AudioBuffer<float> pitchShift      (const juce::AudioBuffer<float>& src, double cents);
    static juce::AudioBuffer<float> pitchShiftOLA   (const juce::AudioBuffer<float>& src, double semitones);
    static void                     applyHighShelf  (juce::AudioBuffer<float>& buf, double fs,
                                                      double freq, double gainDb);
    static void                     applyLowShelf   (juce::AudioBuffer<float>& buf, double fs,
                                                      double freq, double gainDb);
    static void                     applyPeakEQ     (juce::AudioBuffer<float>& buf, double sr,
                                                      double freqHz, double gainDb, double Q);
    static void                     applyGainDb     (juce::AudioBuffer<float>& buf, double dB);
    static void                     scaleTransient  (juce::AudioBuffer<float>& buf, double fs,
                                                      double durationMs, double gainFactor);
    static void                     applySaturation (juce::AudioBuffer<float>& buf, double amount);
    static void                     addModalResonance(juce::AudioBuffer<float>& buf, double sr,
                                                      double freqHz, double decayMs, float amplitude);
    static void                     addTransientNoise(juce::AudioBuffer<float>& buf, double sr,
                                                      double durationMs, float amplitude, int seed,
                                                      double bandHz, double Q);
    static void                     applyDecayFilter(juce::AudioBuffer<float>& buf, double sr,
                                                      double delayMs, double openHz,
                                                      double closeHz, double sweepMs);
    static void                     applyMSWidth    (juce::AudioBuffer<float>& buf,
                                                      double widthFactor);
    static void                     applyAllPass    (juce::AudioBuffer<float>& buf, double sr,
                                                      double freqHz, int numStages);
    static juce::AudioBuffer<float> trimStart       (const juce::AudioBuffer<float>& src,
                                                      int offsetSamples);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DrumTrack)
};
