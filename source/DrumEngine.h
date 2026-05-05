#pragma once

#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <vector>
#include <memory>
#include "DrumPad.h"

// Forward declaration â€” DrumEngine.cpp includes PluginProcessor.h for trackParamID
class DeathDealerDrumsAudioProcessor;

//==============================================================================
struct BiquadCoeffs
{
    float b0 { 1.0f }, b1 { 0.0f }, b2 { 0.0f };
    float a1 { 0.0f }, a2 { 0.0f };
};

struct BiquadState
{
    float x1 { 0.0f }, x2 { 0.0f };
    float y1 { 0.0f }, y2 { 0.0f };
    void reset() noexcept { x1 = x2 = y1 = y2 = 0.0f; }
};

//==============================================================================
/** Active voice â€” one concurrently playing drum hit. */
struct DrumVoice
{
    static constexpr float CHOKE_FADE_SECS = 0.005f;

    int    trackIndex       { -1 };
    const juce::AudioBuffer<float>* audioData { nullptr };
    double sourceSampleRate { 44100.0 };
    double playbackPosition { 0.0 };
    double playbackRate     { 1.0 };
    float  amplitude        { 1.0f };
    float  decayEnv         { 1.0f };
    float  decayRate        { 0.0f };
    float  chokeGain        { 1.0f };
    float  chokeStep        { 0.0f };
    bool   beingChoked      { false };
    bool   active           { false };
    bool   forceInvertPolarity { false }; ///< Optional per-voice polarity flip from phase alignment
    int    pendingDelaySamples { 0 };  ///< Hit-timing humanization: samples to wait before outputting
    int    transientBlendSamples { 0 }; ///< For strict locked followers: fade-in window to avoid double-attack flam perception
    int    transientBlendPos     { 0 };

    // Per-trigger micro-randomization — makes every hit subtly unique
    float        pitchMicroOffset { 1.0f };   ///< ±4 cents tuning drift applied to playbackRate
    float        amplitudeTrim    { 1.0f };   ///< ±1.5 dB amplitude variation
    juce::uint32 noiseState       { 0 };      ///< LCG state for unique stick-contact noise burst
    int          noiseSamplesLeft { 0 };      ///< Samples remaining in noise burst
    float        noiseEnv         { 0.0f };   ///< Current noise amplitude
    float        noiseDecay       { 0.0f };   ///< Per-sample noise decay coefficient
    // Velocity-sensitive brightness — high-shelf per voice, scaled by velocity
    BiquadCoeffs velBrightCoeffs;             ///< High-shelf coeffs computed at note-on
    BiquadState  velBrightStateL, velBrightStateR; ///< Stereo biquad state

    // Per-hit random tonal color — body resonance (80–250 Hz) + presence snap (2–5 kHz)
    // These make every hit tonally distinct even at the same velocity and variation.
    BiquadCoeffs bodyRandCoeffs;              ///< Random bell ±4 dB, 80–250 Hz
    BiquadState  bodyRandStateL, bodyRandStateR;
    BiquadCoeffs presenceRandCoeffs;          ///< Random bell ±2.5 dB, 2–5 kHz
    BiquadState  presenceRandStateL, presenceRandStateR;
};

//==============================================================================
/** Per-track EQ state (3 biquad filters, stereo). */
static constexpr int EQ8_BANDS = 8;

struct PadCompState
{
    float envL { 0.f }, envR { 0.f };
    void reset() noexcept { envL = envR = 0.f; }
};

struct PadDSPState
{
    std::array<BiquadState, 2> lowShelfState;
    std::array<BiquadState, 2> peakMidState;
    std::array<BiquadState, 2> highShelfState;

    // 8-band EQ state: [band][stage 0-3][L=0 / R=1]
    std::array<std::array<std::array<BiquadState, 2>, 4>, EQ8_BANDS> eq8State;

    PadCompState comp;

    struct TransientState
    {
        float envFast { 0.f }, envSlow { 0.f };
        void reset() noexcept { envFast = envSlow = 0.f; }
    } trans;

    void reset() noexcept
    {
        for (auto& s : lowShelfState)  s.reset();
        for (auto& s : peakMidState)   s.reset();
        for (auto& s : highShelfState) s.reset();
        for (auto& band : eq8State)    for (auto& stg : band) for (auto& s : stg) s.reset();
        comp.reset();
        trans.reset();
    }
};

//==============================================================================
class DrumEngine
{
public:
    static constexpr int MAX_VOICES = 64;

    /// Gain reduction in dB — updated each process block, read by the UI meter (atomic).
    std::atomic<float> compGrDb { 0.0f };

    /// Per-track peak level (linear, 0..∞). Updated each block. Read by TrackRow meters.
    static constexpr int MAX_TRACKS = 20;
    std::atomic<float> trackPeakLin[MAX_TRACKS];

    /// Per-track compressor GR in dB (0 = no reduction). Updated each block.
    std::atomic<float> trackCompGrDb[MAX_TRACKS];

    /// Per-track trigger feedback: tier (0=SOFT,1=MID,2=HARD) and slot (0-7) of last hit.
    /// Set on each note-on. -1 = never triggered. Read by UI timerCallback (lock-free).
    std::atomic<int> lastHitTier[MAX_TRACKS];
    std::atomic<int> lastHitSlot[MAX_TRACKS];

    /// Bus output peak levels (linear) for the three parallel buses.
    std::atomic<float> roomPeakLin   { 0.0f };
    std::atomic<float> compPeakLin   { 0.0f };
    std::atomic<float> satPeakLin    { 0.0f };
    /// Master output peak (main mix buffer after all processing).
    std::atomic<float> masterPeakLin { 0.0f };

    // ── Spectrum analysis ─────────────────────────────────────────────────────
    static constexpr int SPECTRUM_FFT_ORDER = 10;                  // 1024-point FFT
    static constexpr int SPECTRUM_FFT_SIZE  = 1 << SPECTRUM_FFT_ORDER;
    static constexpr int SPECTRUM_BINS      = SPECTRUM_FFT_SIZE / 2;

    /// Set by UI to select which track's padBuffer to analyse (-1 = off).
    std::atomic<int>  spectrumTrack { -1 };
    /// Set by DSP when a new spectrum frame is ready; cleared by UI after reading.
    std::atomic<bool> spectrumReady { false };
    /// Magnitude bins (linear), written by DSP, read by UI.  Benign data race — worst
    /// case is a single visually glitched frame, which is harmless.
    float spectrumData[SPECTRUM_BINS] {};

    /** Read a single EQ8 biquad coefficient set — safe for UI reads (benign data race). */
    const BiquadCoeffs& getEQ8Coeff (int slot, int band) const noexcept
    {
        return eq8Coeffs[(size_t) slot][(size_t) band];
    }
    /** Number of cascaded biquad stages for HPF/LPF slope control. */
    int getEQ8Passes (int slot, int band) const noexcept
    {
        return eq8Passes[(size_t) slot][(size_t) band];
    }

    DrumEngine();
    ~DrumEngine() = default;

    void prepare         (double sampleRate, int blockSize);
    void releaseResources();
    /** Kill all currently active voices immediately (call before replacing the track list). */
    void killAllVoices   () noexcept;
    /** Called by processor when a track slot is removed; remaps active voice indices safely. */
    void handleTrackRemoved (int removedIndex);

    /** Main process call.  Caller must hold at least a read lock on `tracks`. */
    void process (juce::AudioBuffer<float>&    buffer,
                  juce::MidiBuffer&            midi,
                  juce::AudioProcessorValueTreeState& apvts,
                  const std::vector<std::unique_ptr<DrumTrack>>& tracks,
                  const int* busChannelMap);

    /** Trigger a specific variation directly (for UI preview pads). Call from audio thread. */
    void triggerVariationDirect (int trackIndex, int varIndex, float velocity,
                                 float humanErrorScatter,
                                 const std::vector<std::unique_ptr<DrumTrack>>& tracks);

    /** Trigger a track directly (uses normal track variation selection/humanization). */
    void triggerTrackDirect (int trackIndex, float velocity,
                             float humanErrorScatter,
                             const std::vector<std::unique_ptr<DrumTrack>>& tracks);

private:
    struct TrackParamCache
    {
        std::atomic<float>* vol { nullptr };
        std::atomic<float>* pan { nullptr };
        std::atomic<float>* tune { nullptr };
        std::atomic<float>* decay { nullptr };
        std::atomic<float>* attack { nullptr };
        std::atomic<float>* eqL { nullptr };
        std::atomic<float>* eqM { nullptr };
        std::atomic<float>* eqH { nullptr };
        std::atomic<float>* startTrim { nullptr };
        std::atomic<float>* endTrim { nullptr };
        std::atomic<float>* reverbSend { nullptr };
        std::atomic<float>* compSend { nullptr };
        std::atomic<float>* satSend { nullptr };
        std::atomic<float>* choke { nullptr };
        std::atomic<float>* chokeTrigOn    { nullptr };
        std::atomic<float>* chokeTrigSlot  { nullptr };
        std::atomic<float>* chokeTrigDelay { nullptr };
        std::atomic<float>* mute { nullptr };
        std::atomic<float>* solo { nullptr };
        std::atomic<float>* phase { nullptr };
        std::atomic<float>* output { nullptr };
        std::atomic<float>* outputMode { nullptr };
        std::atomic<float>* eq8On { nullptr };
        std::atomic<float>* trkCompOn { nullptr };
        std::atomic<float>* trkCompThr { nullptr };
        std::atomic<float>* trkCompRat { nullptr };
        std::atomic<float>* trkCompAtk { nullptr };
        std::atomic<float>* trkCompRel { nullptr };
        std::atomic<float>* trkCompMkp { nullptr };
        std::atomic<float>* bleedSend { nullptr };
        std::atomic<float>* bleedEnable { nullptr };
        std::array<std::atomic<float>*, EQ8_BANDS> eq8Freq {};
        std::array<std::atomic<float>*, EQ8_BANDS> eq8Gain {};
        std::array<std::atomic<float>*, EQ8_BANDS> eq8Q {};
        std::atomic<float>* trkTransOn  { nullptr };
        std::atomic<float>* trkTransAtk { nullptr };
        std::atomic<float>* trkTransSus { nullptr };
    };

    struct PhaseAlignment
    {
        int  bestOffsetSamples { 0 }; // +N => slave should start N samples earlier
        bool invertPolarity    { false };
    };

    struct PhaseCacheEntry
    {
        bool         valid       { false };
        const float* masterPtr   { nullptr };
        const float* slavePtr    { nullptr };
        int          masterLen   { 0 };
        int          slaveLen    { 0 };
        PhaseAlignment alignment;
    };

    struct TriggerHumanization
    {
        int          pendingDelaySamples { 0 };
        float        startOffsetMs       { 0.0f };
        float        pitchMicroOffset    { 1.0f };
        float        amplitudeTrim       { 1.0f };
        float        doubleStrikeGain    { 1.0f }; // shared gain from double-strike suppression
        float        velocityScatter     { 1.0f };
        juce::uint32 noiseState          { 0 };
        int          noiseSamplesLeft    { 0 };
        float        noiseEnv            { 0.0f };
        float        noiseDecay          { 0.0f };
        float        bodyFreq            { 120.0f };
        float        bodyDb              { 0.0f };
        float        presFreq            { 3000.0f };
        float        presDb              { 0.0f };
    };

    double       currentSampleRate { 44100.0 };
    juce::Random rng;   ///< Audio-thread RNG for humanization
    int    currentBlockSize  { 512 };

    // Shared room reverb bus — fed by per-track reverb_send amounts
    juce::Reverb                  reverb;
    juce::Reverb::Parameters      reverbParams;
    juce::AudioBuffer<float>      reverbBus;   ///< Stereo accumulation buffer for reverb

    // Parallel compression bus — "smash" metal parallel processing
    juce::AudioBuffer<float>      compBus;     ///< Stereo accumulation buffer fed from main mix
    // Compressor state (feed-forward RMS, 10:1 ratio, 5ms attack, 60ms release)
    float compEnvL       { 0.0f };
    float compEnvR       { 0.0f };
    float compAttackCoef { 0.0f };
    float compRelCoef    { 0.0f };

    // Tape saturation bus — parallel tanh waveshaper with HF softening
    juce::AudioBuffer<float>      satBus;
    // One-pole HF damping filter state (per channel), mimics tape HF rolloff at high drive
    float satHfStateL    { 0.0f };
    float satHfStateR    { 0.0f };

    // Mic bleed bus — scratch buffer for sympathetic resonance cross-feed
    juce::AudioBuffer<float>      bleedBuf;
    // One-pole LPF state for bleed bus (retains body thump, removes transients)
    float bleedLpfStateL { 0.0f };
    float bleedLpfStateR { 0.0f };

    // Pre-delay line (circular buffer, stereo) — 22ms keeps attack transient dry
    static constexpr int REVERB_PREDELAY_MAX = 4096; // >85ms at 48 kHz
    juce::AudioBuffer<float> preDelayBuf;
    int  preDelayWritePos { 0 };
    int  preDelaySamples  { 0 };

    // Reverb bus shaping filters
    BiquadCoeffs rvbHpfCoeffs;                     ///< HPF at 120 Hz — kills muddy low tail
    BiquadState  rvbHpfStateL,   rvbHpfStateR;
    BiquadCoeffs rvbShelfCoeffs;                   ///< High-shelf -4 dB @ 7 kHz — smooths harshness
    BiquadState  rvbShelfStateL, rvbShelfStateR;

    std::array<DrumVoice,   MAX_VOICES>  voices;
    std::array<PadDSPState, MAX_TRACKS>  padDSP;
    std::array<BiquadCoeffs, MAX_TRACKS> lowShelfCoeffs;

    /// Running sample position — incremented each block, used for double-strike detection.
    juce::int64 sampleCounter { 0 };

    /// Sample position at last trigger per track — used for double-strike detection.
    std::array<juce::int64, MAX_TRACKS> lastTriggerSample {};
    std::array<BiquadCoeffs, MAX_TRACKS> peakMidCoeffs;
    std::array<BiquadCoeffs, MAX_TRACKS> highShelfCoeffs;
    std::array<juce::AudioBuffer<float>, MAX_TRACKS> padBuffers;
    std::array<PhaseCacheEntry, MAX_TRACKS * MAX_TRACKS * NUM_VEL_TIERS * VARS_PER_TIER> phaseCache;

    void       handleNoteOn   (int trackIndex, float velocity, float humanErrorScatter,
                                const std::vector<std::unique_ptr<DrumTrack>>& tracks,
                                int extraDelaySamples = 0);
    size_t phaseCacheIndex (int masterTrack, int slaveTrack, int tier, int slot) const noexcept;
    PhaseAlignment analysePhaseAlignment (const juce::AudioBuffer<float>& master,
                                          const juce::AudioBuffer<float>& slave) const;
    PhaseAlignment getCachedPhaseAlignment (int masterTrack, int slaveTrack,
                                            int tier, int slot,
                                            const DrumVariation& masterVar,
                                            const DrumVariation& slaveVar);
    TriggerHumanization makeTriggerHumanization (float velocity, float humanErrorScatter);
    void startVoiceFromVariation (int trackIndex, const DrumVariation& var,
                                  float velocity, float humanErrorScatter,
                                  int reportedTier, int reportedSlot,
                                  const TriggerHumanization* sharedHumanization,
                                  const PhaseAlignment* forcedPhaseAlignment,
                                  int strictStartOverrideSamples = -1,
                                  bool strictLockedFollower = false,
                                  int extraDelaySamples = 0);
    DrumVoice* allocateVoice  (int trackIndex);

    static BiquadCoeffs makeLowShelf  (float fs, float freq, float gainDb) noexcept;
    static BiquadCoeffs makeHighShelf (float fs, float freq, float gainDb) noexcept;
    static BiquadCoeffs makeHighPass  (float fs, float freq, float q)      noexcept;
    static BiquadCoeffs makeLowPass   (float fs, float freq, float q)      noexcept;
    static BiquadCoeffs makePeakEQ    (float fs, float freq, float q, float gainDb) noexcept;
    static BiquadCoeffs makeBypass    () noexcept;
    static float        processBiquad (float x, const BiquadCoeffs&, BiquadState&) noexcept;

    void rebuildEQCoeffs  (int slot, float gainLow, float gainMid, float gainHigh);
    void rebuildEQ8Coeffs (int slot,
                           const std::array<float, EQ8_BANDS>& freqs,
                           const std::array<float, EQ8_BANDS>& gains,
                           const std::array<float, EQ8_BANDS>& qVals);

    // 8-band EQ coefficients per track
    std::array<std::array<BiquadCoeffs, EQ8_BANDS>, MAX_TRACKS> eq8Coeffs;
    std::array<std::array<int,          EQ8_BANDS>, MAX_TRACKS> eq8Passes;

    // Cached EQ parameters for dirty-checking (CPU optimization)
    std::array<float, MAX_TRACKS> lastEqLow;
    std::array<float, MAX_TRACKS> lastEqMid;
    std::array<float, MAX_TRACKS> lastEqHigh;
    std::array<std::array<float, EQ8_BANDS>, MAX_TRACKS> lastEq8Freq;
    std::array<std::array<float, EQ8_BANDS>, MAX_TRACKS> lastEq8Gain;
    std::array<std::array<float, EQ8_BANDS>, MAX_TRACKS> lastEq8Q;

    // Consecutive silent blocks per track (for conservative idle fast path)
    std::array<int, MAX_TRACKS> silentBlockCount;

    // Cached APVTS raw parameter pointers (reduces per-block string lookups)
    std::array<TrackParamCache, MAX_TRACKS> trackParamCache;
    bool trackParamCacheReady { false };
    void ensureTrackParamCache (juce::AudioProcessorValueTreeState& apvts);

    // Spectrum analysis
    std::unique_ptr<juce::dsp::FFT>         spectrumFFT;
    std::array<float, SPECTRUM_FFT_SIZE * 2> spectrumFftBuf {};
    std::array<float, SPECTRUM_FFT_SIZE>     spectrumWindow {};
    int spectrumFillPos { 0 };
};

//==============================================================================
// Forward declarations
class DeathDealerDrumsAudioProcessor;

