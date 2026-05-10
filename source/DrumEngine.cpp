#include "DrumEngine.h"
#include "PluginProcessor.h"

#include <cmath>
#include <limits>

//==============================================================================
DrumEngine::DrumEngine()
{
    for (auto& v : voices) v = DrumVoice{};
    for (auto& c : lowShelfCoeffs)  c = makeBypass();
    for (auto& c : peakMidCoeffs)   c = makeBypass();
    for (auto& c : highShelfCoeffs) c = makeBypass();
    for (auto& ta : eq8Coeffs)  for (auto& c : ta) c = makeBypass();
    for (auto& ta : eq8Passes)   for (auto& p : ta) p = 1;
    for (auto& v : lastEqLow)    v = std::numeric_limits<float>::quiet_NaN();
    for (auto& v : lastEqMid)    v = std::numeric_limits<float>::quiet_NaN();
    for (auto& v : lastEqHigh)   v = std::numeric_limits<float>::quiet_NaN();
    for (auto& ta : lastEq8Freq) for (auto& v : ta) v = std::numeric_limits<float>::quiet_NaN();
    for (auto& ta : lastEq8Gain) for (auto& v : ta) v = std::numeric_limits<float>::quiet_NaN();
    for (auto& ta : lastEq8Q)    for (auto& v : ta) v = std::numeric_limits<float>::quiet_NaN();
    silentBlockCount.fill (0);
    for (auto& a : trackPeakLin)    a.store (0.0f);
    for (auto& a : trackCompGrDb)   a.store (0.0f);
    for (auto& a : lastHitTier)     a.store (-1);
    for (auto& a : lastHitSlot)     a.store (-1);
    lastTriggerSample.fill (-1000000);  // ensure no suppression on first hit
}

void DrumEngine::killAllVoices() noexcept
{
    for (auto& v : voices)
        v.active = false;
}

void DrumEngine::prepare (double sampleRate, int blockSize)
{
    currentSampleRate = sampleRate;
    currentBlockSize  = blockSize;
    for (auto& buf : padBuffers)
        buf.setSize (2, blockSize, false, true, false);
    for (auto& dsp : padDSP)
        dsp.reset();

    // Reverb bus
    reverbBus.setSize (2, blockSize, false, true, false);

    // Parallel compression bus
    // Industry-standard metal "smash" settings:
    //   Attack  5 ms  — lets initial transient punch through for snare crack/kick click
    //   Release 60 ms — fast enough to pump with the groove, adds aggression
    //   Ratio   10:1  — heavy limiting character, classic NY parallel comp sound
    compBus.setSize (2, blockSize, false, true, false);
    compAttackCoef = 1.0f - std::exp (-1.0f / (0.005f  * (float) sampleRate));
    compRelCoef    = 1.0f - std::exp (-1.0f / (0.060f  * (float) sampleRate));
    compEnvL = compEnvR = 0.0f;

    // Tape saturation bus
    satBus.setSize (2, blockSize, false, true, false);
    satHfStateL = satHfStateR = 0.0f;

    // Mic bleed scratch buffer
    bleedBuf.setSize (2, blockSize, false, true, false);
    bleedLpfStateL = bleedLpfStateR = 0.0f;

    // Spectrum analysis FFT (order 10 = 1024 points)
    spectrumFFT = std::make_unique<juce::dsp::FFT> (SPECTRUM_FFT_ORDER);
    std::fill (spectrumFftBuf.begin(), spectrumFftBuf.end(), 0.f);
    spectrumFillPos = 0;
    spectrumReady.store (false);
    // Pre-compute Hann window
    for (int k = 0; k < SPECTRUM_FFT_SIZE; ++k)
        spectrumWindow[k] = 0.5f * (1.f - std::cos (2.f * juce::MathConstants<float>::pi
                                                     * k / (SPECTRUM_FFT_SIZE - 1)));

    // Metal room reverb: large live room, concrete/stone walls, minimal HF absorption.
    // roomSize 0.72 → ~1.8s RT60. damping 0.15 → very bright (stone doesn’t absorb much).
    // Full stereo width to fill the mix.
    reverbParams.roomSize   = 0.72f;
    reverbParams.damping    = 0.15f;
    reverbParams.wetLevel   = 1.0f;
    reverbParams.dryLevel   = 0.0f;
    reverbParams.width      = 1.0f;
    reverbParams.freezeMode = 0.0f;
    reverb.setParameters (reverbParams);
    reverb.reset();

    // Pre-delay: 22 ms keeps the transient attack dry; room arrives just behind the hit.
    // This is the single most important drum-room trick in professional mix engineering.
    preDelaySamples  = juce::jmax (1, (int) (0.022 * sampleRate));
    preDelayWritePos = 0;
    preDelayBuf.setSize (2, REVERB_PREDELAY_MAX, false, true, false);

    // Reverb bus HPF: removes sub/low-mid mud (kick body, etc.) from the reverb tail
    rvbHpfCoeffs   = makeHighPass  ((float) sampleRate, 120.f, 0.707f);
    rvbHpfStateL   = rvbHpfStateR  = BiquadState{};
    // Gentle high-shelf cut: tames harsh upper reverb tail without killing air
    rvbShelfCoeffs = makeHighShelf ((float) sampleRate, 7000.f, -4.0f);
    rvbShelfStateL = rvbShelfStateR = BiquadState{};
}

void DrumEngine::releaseResources()
{
    for (auto& v : voices) v.active = false;
}

void DrumEngine::handleTrackRemoved (int removedIndex)
{
    for (auto& v : voices)
    {
        if (!v.active) continue;

        if (v.trackIndex == removedIndex)
        {
            // Removed track: kill any active voice immediately to avoid dangling sample pointers.
            v = DrumVoice{};
            continue;
        }

        if (v.trackIndex > removedIndex)
            --v.trackIndex; // downstream tracks shift down by one slot
    }

    // Shift per-track DSP/runtime state so slot-compacted tracks keep their own history/state.
    if (removedIndex >= 0 && removedIndex < MAX_TRACKS)
    {
        for (int i = removedIndex; i < MAX_TRACKS - 1; ++i)
        {
            const size_t d = (size_t) i;
            const size_t s = (size_t) (i + 1);

            padDSP[d] = padDSP[s];
            lowShelfCoeffs[d]  = lowShelfCoeffs[s];
            peakMidCoeffs[d]   = peakMidCoeffs[s];
            highShelfCoeffs[d] = highShelfCoeffs[s];
            eq8Coeffs[d]       = eq8Coeffs[s];
            eq8Passes[d]       = eq8Passes[s];
            lastEqLow[d]       = lastEqLow[s];
            lastEqMid[d]       = lastEqMid[s];
            lastEqHigh[d]      = lastEqHigh[s];
            lastEq8Freq[d]     = lastEq8Freq[s];
            lastEq8Gain[d]     = lastEq8Gain[s];
            lastEq8Q[d]        = lastEq8Q[s];
            silentBlockCount[d]= silentBlockCount[s];

            lastTriggerSample[d] = lastTriggerSample[s];

            trackPeakLin[i].store (trackPeakLin[i + 1].load());
            trackCompGrDb[i].store (trackCompGrDb[i + 1].load());
            lastHitTier[i].store (lastHitTier[i + 1].load());
            lastHitSlot[i].store (lastHitSlot[i + 1].load());
        }

        const int tail = MAX_TRACKS - 1;
        const size_t t = (size_t) tail;
        padDSP[t] = PadDSPState{};
        lowShelfCoeffs[t]  = makeBypass();
        peakMidCoeffs[t]   = makeBypass();
        highShelfCoeffs[t] = makeBypass();
        for (auto& c : eq8Coeffs[t]) c = makeBypass();
        for (auto& p : eq8Passes[t]) p = 1;
        lastEqLow[t]   = std::numeric_limits<float>::quiet_NaN();
        lastEqMid[t]   = std::numeric_limits<float>::quiet_NaN();
        lastEqHigh[t]  = std::numeric_limits<float>::quiet_NaN();
        for (auto& v : lastEq8Freq[t]) v = std::numeric_limits<float>::quiet_NaN();
        for (auto& v : lastEq8Gain[t]) v = std::numeric_limits<float>::quiet_NaN();
        for (auto& v : lastEq8Q[t])    v = std::numeric_limits<float>::quiet_NaN();
        silentBlockCount[t] = 0;
        lastTriggerSample[t] = -1000000;
        trackPeakLin[tail].store (0.0f);
        trackCompGrDb[tail].store (0.0f);
        lastHitTier[tail].store (-1);
        lastHitSlot[tail].store (-1);
    }

    // Hard safety reset: after slot compaction, clear all per-track runtime histories
    // so no previous track's DSP memory can colour a different surviving track.
    for (int i = 0; i < MAX_TRACKS; ++i)
    {
        const size_t si = (size_t) i;
        padDSP[si].reset();
        trackPeakLin[i].store (0.0f);
        trackCompGrDb[i].store (0.0f);
        lastHitTier[i].store (-1);
        lastHitSlot[i].store (-1);
    }
    lastTriggerSample.fill (-1000000);
    silentBlockCount.fill (0);
}

//==============================================================================
// Biquad helpers (Audio EQ Cookbook formulas)

namespace {
    inline void normBiq (float b0, float b1, float b2,
                         float a0, float a1, float a2,
                         BiquadCoeffs& out) noexcept
    {
        const float inv = 1.0f / a0;
        out.b0 = b0*inv; out.b1 = b1*inv; out.b2 = b2*inv;
        out.a1 = a1*inv; out.a2 = a2*inv;
    }

    inline float readMonoSample (const juce::AudioBuffer<float>& b, int idx) noexcept
    {
        const float l = b.getSample (0, idx);
        if (b.getNumChannels() > 1)
            return 0.5f * (l + b.getSample (1, idx));
        return l;
    }

    int detectTransientOnsetSample (const juce::AudioBuffer<float>& buffer) noexcept
    {
        const int len = buffer.getNumSamples();
        if (len <= 0)
            return 0;

        const int scanLen = juce::jmin (8192, len);
        float peakAbs = 0.0f;
        for (int i = 0; i < scanLen; ++i)
            peakAbs = juce::jmax (peakAbs, std::abs (readMonoSample (buffer, i)));

        if (peakAbs < 1.0e-6f)
            return 0;

        const float threshold = juce::jmax (peakAbs * 0.12f, 1.0e-4f);
        for (int i = 0; i < scanLen; ++i)
            if (std::abs (readMonoSample (buffer, i)) >= threshold)
                return i;

        return 0;
    }
}

BiquadCoeffs DrumEngine::makeLowShelf (float fs, float freq, float gainDb) noexcept
{
    const float A = std::pow (10.f, gainDb/40.f);
    const float w = 2.f * juce::MathConstants<float>::pi * freq / fs;
    const float s = std::sin(w), c = std::cos(w);
    const float a = s/2.f * std::sqrt ((A + 1.f/A) * (1.f/1.f - 1.f) + 2.f);
    const float t = 2.f * std::sqrt(A) * a;
    BiquadCoeffs q;
    normBiq ( A*((A+1)-(A-1)*c+t),  2*A*((A-1)-(A+1)*c),  A*((A+1)-(A-1)*c-t),
                (A+1)+(A-1)*c+t,   -2*((A-1)+(A+1)*c),    (A+1)+(A-1)*c-t, q);
    return q;
}

BiquadCoeffs DrumEngine::makeHighShelf (float fs, float freq, float gainDb) noexcept
{
    const float A = std::pow (10.f, gainDb/40.f);
    const float w = 2.f * juce::MathConstants<float>::pi * freq / fs;
    const float s = std::sin(w), c = std::cos(w);
    const float a = s/2.f * std::sqrt ((A + 1.f/A) * (1.f/1.f - 1.f) + 2.f);
    const float t = 2.f * std::sqrt(A) * a;
    BiquadCoeffs q;
    normBiq ( A*((A+1)+(A-1)*c+t), -2*A*((A-1)+(A+1)*c),  A*((A+1)+(A-1)*c-t),
                (A+1)-(A-1)*c+t,    2*((A-1)-(A+1)*c),     (A+1)-(A-1)*c-t, q);
    return q;
}

BiquadCoeffs DrumEngine::makeHighPass (float fs, float freq, float q) noexcept
{
    // 2nd-order Butterworth HPF (Audio EQ Cookbook)
    const float w  = 2.f * juce::MathConstants<float>::pi * freq / fs;
    const float s  = std::sin(w), c = std::cos(w);
    const float al = s / (2.f * q);
    BiquadCoeffs co;
    normBiq ( (1.f+c)*0.5f, -(1.f+c), (1.f+c)*0.5f,
               1.f+al,       -2.f*c,   1.f-al, co);
    return co;
}

BiquadCoeffs DrumEngine::makePeakEQ (float fs, float freq, float q, float gainDb) noexcept
{
    const float A = std::pow (10.f, gainDb/40.f);
    const float w = 2.f * juce::MathConstants<float>::pi * freq / fs;
    const float s = std::sin(w), c = std::cos(w);
    const float al = s / (2.f * q);
    BiquadCoeffs co;
    normBiq (1+al*A, -2*c, 1-al*A, 1+al/A, -2*c, 1-al/A, co);
    return co;
}

BiquadCoeffs DrumEngine::makeBypass () noexcept
{
    BiquadCoeffs c; c.b0=1; c.b1=0; c.b2=0; c.a1=0; c.a2=0; return c;
}

float DrumEngine::processBiquad (float x, const BiquadCoeffs& c, BiquadState& s) noexcept
{
    const float y = c.b0*x + c.b1*s.x1 + c.b2*s.x2 - c.a1*s.y1 - c.a2*s.y2;
    s.x2=s.x1; s.x1=x; s.y2=s.y1; s.y1=y;
    return y;
}

void DrumEngine::rebuildEQCoeffs (int slot, float low, float mid, float high)
{
    const float fs = (float) currentSampleRate;
    const auto  si = (size_t) slot;
    lowShelfCoeffs [si] = (std::abs(low)  > 0.05f) ? makeLowShelf  (fs, 100.f,  low)          : makeBypass();
    peakMidCoeffs  [si] = (std::abs(mid)  > 0.05f) ? makePeakEQ    (fs, 1000.f, 1.2f, mid)    : makeBypass();
    highShelfCoeffs[si] = (std::abs(high) > 0.05f) ? makeHighShelf (fs, 8000.f, high)          : makeBypass();
}

BiquadCoeffs DrumEngine::makeLowPass (float fs, float freq, float q) noexcept
{
    const float w  = 2.f * juce::MathConstants<float>::pi * freq / fs;
    const float s  = std::sin(w), c = std::cos(w);
    const float al = s / (2.f * q);
    BiquadCoeffs co;
    normBiq ((1.f-c)*0.5f, (1.f-c), (1.f-c)*0.5f,
              1.f+al,       -2.f*c,   1.f-al, co);
    return co;
}

void DrumEngine::rebuildEQ8Coeffs (int slot,
                                   const std::array<float, EQ8_BANDS>& freqs,
                                   const std::array<float, EQ8_BANDS>& gains,
                                   const std::array<float, EQ8_BANDS>& qVals)
{
    const float fs = (float) currentSampleRate;
    const auto  si = (size_t) slot;

    // Fixed band types
    enum class BT { HPF, LowShelf, Peak, HighShelf, LPF };
    static constexpr BT types[EQ8_BANDS] = {
        BT::HPF, BT::LowShelf, BT::Peak, BT::Peak, BT::Peak, BT::Peak, BT::HighShelf, BT::LPF
    };

    for (int b = 0; b < EQ8_BANDS; ++b)
    {
        const float freq = freqs[(size_t) b];
        const float gain = gains[(size_t) b];
        const float q    = qVals[(size_t) b];
        const float safeQ = juce::jmax (0.1f, q);

        switch (types[b])
        {
            case BT::HPF:
            {
                // Gain knob controls slope: map -18..+18 dB → 1..4 cascaded stages
                // -18 to -6 → 12 dB/oct (1 stage), -6 to 6 → 24 dB/oct (2), 6 to 12 → 36 dB/oct (3), 12+ → 48 dB/oct (4)
                const int passes = (gain < -6.f) ? 1 : (gain < 6.f) ? 2 : (gain < 12.f) ? 3 : 4;
                eq8Passes[si][b] = passes;
                eq8Coeffs[si][b] = (freq > 25.f) ? makeHighPass (fs, freq, safeQ) : makeBypass();
                break;
            }
            case BT::LowShelf:
                eq8Passes[si][b] = 1;
                eq8Coeffs[si][b] = (std::abs(gain) > 0.05f) ? makeLowShelf (fs, freq, gain) : makeBypass();
                break;
            case BT::Peak:
                eq8Passes[si][b] = 1;
                eq8Coeffs[si][b] = (std::abs(gain) > 0.05f) ? makePeakEQ (fs, freq, safeQ, gain) : makeBypass();
                break;
            case BT::HighShelf:
                eq8Passes[si][b] = 1;
                eq8Coeffs[si][b] = (std::abs(gain) > 0.05f) ? makeHighShelf (fs, freq, gain) : makeBypass();
                break;
            case BT::LPF:
            {
                const int passes = (gain < -6.f) ? 1 : (gain < 6.f) ? 2 : (gain < 12.f) ? 3 : 4;
                eq8Passes[si][b] = passes;
                eq8Coeffs[si][b] = (freq < 21000.f) ? makeLowPass (fs, freq, safeQ) : makeBypass();
                break;
            }
        }
    }
}

//==============================================================================
// Voice management

DrumVoice* DrumEngine::allocateVoice (int trackIndex)
{
    for (auto& v : voices) if (!v.active) return &v;

    DrumVoice* oldest = nullptr;
    double     maxPos = -1.0;
    for (auto& v : voices)
    {
        if (v.trackIndex == trackIndex)
        {
            const double rel = v.audioData ? v.playbackPosition / v.audioData->getNumSamples() : 0.0;
            if (rel > maxPos) { maxPos = rel; oldest = &v; }
        }
    }
    if (oldest) return oldest;
    return &voices[0];
}

size_t DrumEngine::phaseCacheIndex (int masterTrack, int slaveTrack, int tier, int slot) const noexcept
{
    const size_t m = (size_t) juce::jlimit (0, MAX_TRACKS - 1, masterTrack);
    const size_t s = (size_t) juce::jlimit (0, MAX_TRACKS - 1, slaveTrack);
    const size_t t = (size_t) juce::jlimit (0, NUM_VEL_TIERS - 1, tier);
    const size_t v = (size_t) juce::jlimit (0, VARS_PER_TIER - 1, slot);
    return (((m * MAX_TRACKS + s) * NUM_VEL_TIERS + t) * VARS_PER_TIER + v);
}

DrumEngine::PhaseAlignment DrumEngine::analysePhaseAlignment (const juce::AudioBuffer<float>& master,
                                                              const juce::AudioBuffer<float>& slave) const
{
    PhaseAlignment best;

    const int mLen = master.getNumSamples();
    const int sLen = slave.getNumSamples();
    if (mLen <= 4 || sLen <= 4)
        return best;

    const int window = juce::jmin (4096, mLen, sLen);
    const int maxShift = juce::jmin (1024, window / 2);

    auto readMono = [] (const juce::AudioBuffer<float>& b, int idx)
    {
        const float l = b.getSample (0, idx);
        if (b.getNumChannels() > 1)
            return 0.5f * (l + b.getSample (1, idx));
        return l;
    };

    float bestScore = -1.0f;
    for (int offset = -maxShift; offset <= maxShift; ++offset)
    {
        double dot = 0.0;
        double eM = 0.0;
        double eS = 0.0;

        for (int i = 0; i < window; ++i)
        {
            const int j = i + offset;
            if (j < 0 || j >= window)
                continue;

            const float m = readMono (master, i);
            const float s = readMono (slave, j);
            dot += (double) m * (double) s;
            eM  += (double) m * (double) m;
            eS  += (double) s * (double) s;
        }

        if (eM <= 1e-12 || eS <= 1e-12)
            continue;

        const float corr = (float) (dot / std::sqrt (eM * eS)); // -1..1
        const float absCorr = std::abs (corr);

        if (absCorr > bestScore)
        {
            bestScore = absCorr;
            best.bestOffsetSamples = offset;
            best.invertPolarity    = (corr < 0.0f);
        }
    }

    return best;
}

DrumEngine::PhaseAlignment DrumEngine::getCachedPhaseAlignment (int masterTrack, int slaveTrack,
                                                                int tier, int slot,
                                                                const DrumVariation& masterVar,
                                                                const DrumVariation& slaveVar)
{
    auto& entry = phaseCache[phaseCacheIndex (masterTrack, slaveTrack, tier, slot)];

    const float* mPtr = masterVar.buffer.getReadPointer (0);
    const float* sPtr = slaveVar.buffer.getReadPointer (0);
    const int mLen = masterVar.buffer.getNumSamples();
    const int sLen = slaveVar.buffer.getNumSamples();

    const bool cacheHit = entry.valid
                       && entry.masterPtr == mPtr
                       && entry.slavePtr  == sPtr
                       && entry.masterLen == mLen
                       && entry.slaveLen  == sLen;

    if (cacheHit)
        return entry.alignment;

    entry.alignment = analysePhaseAlignment (masterVar.buffer, slaveVar.buffer);
    entry.masterPtr = mPtr;
    entry.slavePtr  = sPtr;
    entry.masterLen = mLen;
    entry.slaveLen  = sLen;
    entry.valid     = true;
    return entry.alignment;
}

DrumEngine::TriggerHumanization DrumEngine::makeTriggerHumanization (float velocity, float humanErrorScatter)
{
    juce::ignoreUnused (velocity);

    TriggerHumanization h;

    const float he = juce::jlimit (0.05f, 0.20f, humanErrorScatter);
    const float heNorm = (he - 0.05f) / 0.15f; // 0..1

    // Hit timing
    const float maxHitDelayMs = juce::jmap (heNorm, 4.0f, 30.0f);
    const float onTimeChance  = juce::jmap (heNorm, 0.40f, 0.05f);
    const float hitDelayMs    = (rng.nextFloat() < onTimeChance) ? 0.0f : rng.nextFloat() * maxHitDelayMs;
    h.pendingDelaySamples     = (int) (hitDelayMs * 0.001 * currentSampleRate);

    // Sample start offset — capped at 3 ms max so the transient is never skipped
    const float maxStartOffsetMs = juce::jmap (heNorm, 1.0f, 3.0f);
    const float zeroStartChance  = juce::jmap (heNorm, 0.50f, 0.10f);
    h.startOffsetMs = (rng.nextFloat() < zeroStartChance) ? 0.0f : rng.nextFloat() * maxStartOffsetMs;

    // Micro pitch / amplitude
    // humanErrorScatter == 0 → pitch-lock mode: no detuning at all
    if (humanErrorScatter <= 0.0f)
    {
        h.pitchMicroOffset = 1.0f;
    }
    else
    {
        const float maxPitchCents = juce::jmap (heNorm, 10.0f, 35.0f);
        const float cents         = rng.nextFloat() * (maxPitchCents * 2.0f) - maxPitchCents;
        h.pitchMicroOffset        = std::pow (2.0f, cents / 1200.0f);
    }

    const float maxAmpDb      = juce::jmap (heNorm, 2.0f, 6.0f);
    const float dB            = rng.nextFloat() * (maxAmpDb * 2.0f) - maxAmpDb;
    h.amplitudeTrim           = std::pow (10.0f, dB / 20.0f);
    h.doubleStrikeGain        = 1.0f;

    // Velocity scatter
    const float scatterPct    = juce::jlimit (0.05f, 0.20f, humanErrorScatter);
    h.velocityScatter         = 1.0f + (rng.nextFloat() * (scatterPct * 2.0f) - scatterPct);

    // Per-hit noise burst — frequency scales with velocity (harder hit = brighter stick noise)
    h.noiseState              = (juce::uint32) rng.nextInt();
    h.noiseSamplesLeft        = (int) (currentSampleRate * (0.003f + rng.nextFloat() * 0.009f));
    h.noiseEnv                = 0.010f + rng.nextFloat() * 0.025f;
    h.noiseDecay              = std::exp (-5.0f / (float) juce::jmax (1, h.noiseSamplesLeft));
    h.noiseFreq               = 2000.0f + velocity * 3000.0f + rng.nextFloat() * 500.0f;

    // Transient position micro-jitter: 0-5 samples (~0-0.1 ms)
    // Imperceptible as latency but adds microscopic timing irregularity
    h.transientJitter         = (int) (rng.nextFloat() * 5.0f);

    // Random tonal color
    h.bodyFreq                = 80.0f  + rng.nextFloat() * 170.0f;
    h.bodyDb                  = rng.nextFloat() * 8.0f - 4.0f;
    h.presFreq                = 2000.0f + rng.nextFloat() * 3000.0f;
    h.presDb                  = rng.nextFloat() * 5.0f - 2.5f;

    return h;
}

void DrumEngine::startVoiceFromVariation (int trackIndex, const DrumVariation& var,
                                          float velocity, float humanErrorScatter,
                                          int reportedTier, int reportedSlot,
                                          const TriggerHumanization* sharedHumanization,
                                          const PhaseAlignment* forcedPhaseAlignment,
                                          int strictStartOverrideSamples,
                                          bool strictLockedFollower,
                                          int extraDelaySamples)
{
    if (trackIndex < 0 || trackIndex >= MAX_TRACKS || !var.valid)
        return;

    // Expose trigger feedback to UI (tier tab + pad flash)
    if (reportedTier >= 0 && reportedSlot >= 0)
    {
        lastHitTier[(size_t) trackIndex].store (reportedTier);
        lastHitSlot[(size_t) trackIndex].store (reportedSlot);
    }

    DrumVoice* v = allocateVoice (trackIndex);
    if (!v) return;

    v->trackIndex       = trackIndex;
    v->audioData        = &var.buffer;
    v->sourceSampleRate = var.sampleRate;
    v->amplitude        = velocity;
    v->decayEnv         = 1.0f;
    v->decayRate        = 0.0f;
    v->chokeGain        = 1.0f;
    v->chokeStep        = 0.0f;
    v->beingChoked      = false;
    v->active           = true;
    v->forceInvertPolarity = false;

    const auto h = sharedHumanization ? *sharedHumanization
                                      : makeTriggerHumanization (velocity, humanErrorScatter);

    int pendingDelay = h.pendingDelaySamples + extraDelaySamples;
    double playbackPos = h.startOffsetMs * 0.001 * var.sampleRate;

    if (sharedHumanization != nullptr && strictStartOverrideSamples >= 0)
        playbackPos = (double) strictStartOverrideSamples;

    if (forcedPhaseAlignment != nullptr)
    {
        // For locked/slaved hits, preserve strict onset unison across tracks.
        // We keep polarity alignment, but do not apply per-follower timing offsets,
        // because those can create audible flam at high human-error settings.
        const bool strictSlaveTiming = (sharedHumanization != nullptr);
        const bool hardStrictLock    = strictSlaveTiming && (strictStartOverrideSamples >= 0);

        if (!strictSlaveTiming || !hardStrictLock)
        {
            // +offset means slave waveform is later in the file and should start earlier.
            if (forcedPhaseAlignment->bestOffsetSamples > 0)
                playbackPos += (double) forcedPhaseAlignment->bestOffsetSamples;
            else if (forcedPhaseAlignment->bestOffsetSamples < 0)
                pendingDelay += -forcedPhaseAlignment->bestOffsetSamples;
        }
        else
        {
            // Hard strict lock mode: keep absolute onset lock between root/follower.
            // Do not add any relative timing offset here; onset alignment is handled
            // once per hit group before voices are started.
        }

        // In strict slave lock mode, avoid automatic polarity inversion. Even with
        // sample-aligned timing, opposite polarity layers can psychoacoustically
        // smear the attack and be perceived as flam.
        if (!hardStrictLock)
            v->forceInvertPolarity = forcedPhaseAlignment->invertPolarity;
        else
            // Hard lock keeps timing fixed, but we can still correct polarity
            // to avoid out-of-phase low-end cancellation between locked layers.
            v->forceInvertPolarity = forcedPhaseAlignment->invertPolarity;
    }

    v->pendingDelaySamples = juce::jmax (0, pendingDelay);
    v->playbackPosition    = playbackPos + (double) h.transientJitter;
    v->pitchMicroOffset    = h.pitchMicroOffset;
    v->amplitudeTrim       = h.amplitudeTrim;
    v->noiseState          = h.noiseState;
    v->noiseSamplesLeft    = h.noiseSamplesLeft;
    v->noiseEnv            = h.noiseEnv * v->amplitude * v->amplitudeTrim;
    v->noiseDecay          = h.noiseDecay;

    // Velocity-scaled noise brightness — retune the body-rand filter to act as noise bandpass
    // We repurpose presenceRandCoeffs initial centre to track noise freq (set before use below)
    // Actually drive it via a dedicated approach: store in bodyRandCoeffs with noise freq
    // NOTE: noiseFreq is used in the noise bandpass in the render loop via presenceRandCoeffs
    // We pass it through by overwriting presFreq here before the coeffs are computed:
    const float effectivePresFreq = h.noiseFreq;

    // Perceptual anti-flam: in strict lock mode, keep root transient fully intact,
    // but gently fade in locked followers over a couple of milliseconds so we
    // don't hear a doubled click/transient even when sample starts are aligned.
    if (sharedHumanization != nullptr && strictLockedFollower && strictStartOverrideSamples >= 0)
    {
        v->transientBlendSamples = juce::jmax (1, (int) std::round (currentSampleRate * 0.0025));
        v->transientBlendPos     = 0;
    }
    else
    {
        v->transientBlendSamples = 0;
        v->transientBlendPos     = 0;
    }

    // Double-strike suppression — models stick bounce / press-roll physics.
    // For linked/slaved same-note hits, this is precomputed once at the root and
    // shared to all followers so they do not feel independently humanized.
    if (sharedHumanization != nullptr)
    {
        v->amplitude *= h.doubleStrikeGain;
    }
    else
    {
        const juce::int64 gapSamples = sampleCounter - lastTriggerSample[(size_t) trackIndex];
        const float       gapMs      = (float) gapSamples / (float) currentSampleRate * 1000.f;
        if (gapMs < 40.f)
        {
            const float suppressDb = 8.0f * juce::jmax (0.f, 1.f - gapMs / 40.f);
            v->amplitude *= std::pow (10.f, -suppressDb / 20.f);
        }
        lastTriggerSample[(size_t) trackIndex] = sampleCounter;
    }

    v->amplitude *= h.velocityScatter;

    // Velocity-sensitive brightness: high-shelf scales with velocity
    const float velGain       = (velocity - 0.3f) / 0.7f;
    const float shelfDb       = juce::jlimit (-2.0f, 4.0f, velGain * 6.0f - 2.0f);
    v->velBrightCoeffs        = makeHighShelf ((float) currentSampleRate, 5000.f, shelfDb);
    v->velBrightStateL        = BiquadState{};
    v->velBrightStateR        = BiquadState{};

    v->bodyRandCoeffs         = makePeakEQ ((float) currentSampleRate, h.bodyFreq, 0.8f, h.bodyDb);
    v->bodyRandStateL         = v->bodyRandStateR = BiquadState{};
    v->presenceRandCoeffs     = makePeakEQ ((float) currentSampleRate, effectivePresFreq, 1.5f, h.presDb);
    v->presenceRandStateL     = v->presenceRandStateR = BiquadState{};

    // Consecutive-hit fatigue: drift amplitude ±1% based on recent history.
    // Smoothed with a 0.92 coefficient so it recovers naturally after pauses.
    if (trackIndex >= 0 && trackIndex < MAX_TRACKS && sharedHumanization == nullptr)
    {
        const float drift      = (rng.nextFloat() * 0.02f - 0.01f);  // ±1%
        hitFatigue[(size_t) trackIndex] = hitFatigue[(size_t) trackIndex] * 0.92f + drift * 0.08f;
        v->amplitude *= (1.0f + hitFatigue[(size_t) trackIndex]);
    }
}

void DrumEngine::handleNoteOn (int trackIndex, float velocity, float humanErrorScatter,
                                const std::vector<std::unique_ptr<DrumTrack>>& tracks,
                                int extraDelaySamples)
{
    if (trackIndex < 0 || trackIndex >= (int) tracks.size()) return;
    const DrumVariation* var = tracks[(size_t) trackIndex]->getNextVariationForVelocity (velocity);
    if (var == nullptr || !var->valid) return;

    startVoiceFromVariation (trackIndex, *var, velocity, humanErrorScatter,
                             tracks[(size_t) trackIndex]->lastUsedTier.load(),
                             tracks[(size_t) trackIndex]->lastUsedSlot.load(),
                             nullptr,
                             nullptr,
                             -1,
                             false,
                             extraDelaySamples);
}

//==============================================================================
void DrumEngine::triggerVariationDirect (int trackIndex, int varIndex, float velocity,
                                          float humanErrorScatter,
                                          const std::vector<std::unique_ptr<DrumTrack>>& tracks)
{
    if (trackIndex < 0 || trackIndex >= (int) tracks.size()) return;
    const DrumVariation* var = tracks[(size_t) trackIndex]->getVariation (varIndex);
    if (var == nullptr || !var->valid) return;

    const int tier = juce::jlimit (0, NUM_VEL_TIERS - 1, varIndex / VARS_PER_TIER);
    const int slot = juce::jlimit (0, VARS_PER_TIER - 1, varIndex % VARS_PER_TIER);
    startVoiceFromVariation (trackIndex, *var, velocity, humanErrorScatter,
                             tier, slot,
                             nullptr,
                             nullptr,
                             -1,
                             false);

    // Choke trigger: fire secondary sample on every hit when ON CHOKE: is enabled
    if (trackIndex < MAX_TRACKS)
    {
        const auto& ctc = trackParamCache[(size_t) trackIndex];
        if (ctc.chokeTrigOn && ctc.chokeTrigOn->load() > 0.5f && ctc.chokeTrigSlot)
        {
            const int numT    = (int) tracks.size();
            const int dstSlot = juce::jlimit (0, numT - 1,
                                    juce::roundToInt (ctc.chokeTrigSlot->load()));
            const int delay   = ctc.chokeTrigDelay
                ? juce::jmax (0, juce::roundToInt (ctc.chokeTrigDelay->load() * 0.001f * (float) currentSampleRate))
                : 0;
            handleNoteOn (dstSlot, velocity, humanErrorScatter, tracks, delay);
        }
    }
}

void DrumEngine::triggerTrackDirect (int trackIndex, float velocity, float humanErrorScatter,
                                      const std::vector<std::unique_ptr<DrumTrack>>& tracks)
{
    handleNoteOn (trackIndex, velocity, humanErrorScatter, tracks);
}

void DrumEngine::ensureTrackParamCache (juce::AudioProcessorValueTreeState& apvts)
{
    if (trackParamCacheReady)
        return;

    for (int i = 0; i < MAX_TRACKS; ++i)
    {
        auto& c = trackParamCache[(size_t) i];
        c.vol        = apvts.getRawParameterValue (DeathDealerDrumsAudioProcessor::trackParamID (i, "volume"));
        c.pan        = apvts.getRawParameterValue (DeathDealerDrumsAudioProcessor::trackParamID (i, "pan"));
        c.tune       = apvts.getRawParameterValue (DeathDealerDrumsAudioProcessor::trackParamID (i, "tune"));
        c.decay      = apvts.getRawParameterValue (DeathDealerDrumsAudioProcessor::trackParamID (i, "decay"));
        c.attack     = apvts.getRawParameterValue (DeathDealerDrumsAudioProcessor::trackParamID (i, "attack"));
        c.eqL        = apvts.getRawParameterValue (DeathDealerDrumsAudioProcessor::trackParamID (i, "eq_low"));
        c.eqM        = apvts.getRawParameterValue (DeathDealerDrumsAudioProcessor::trackParamID (i, "eq_mid"));
        c.eqH        = apvts.getRawParameterValue (DeathDealerDrumsAudioProcessor::trackParamID (i, "eq_high"));
        c.startTrim  = apvts.getRawParameterValue (DeathDealerDrumsAudioProcessor::trackParamID (i, "sample_start"));
        c.endTrim    = apvts.getRawParameterValue (DeathDealerDrumsAudioProcessor::trackParamID (i, "sample_end"));
        c.reverbSend = apvts.getRawParameterValue (DeathDealerDrumsAudioProcessor::trackParamID (i, "reverb_send"));
        c.compSend   = apvts.getRawParameterValue (DeathDealerDrumsAudioProcessor::trackParamID (i, "comp_send"));
        c.satSend    = apvts.getRawParameterValue (DeathDealerDrumsAudioProcessor::trackParamID (i, "sat_send"));
        c.choke      = apvts.getRawParameterValue (DeathDealerDrumsAudioProcessor::trackParamID (i, "choke"));
        c.chokeTrigOn    = apvts.getRawParameterValue (DeathDealerDrumsAudioProcessor::trackParamID (i, "choke_trig_on"));
        c.chokeTrigSlot  = apvts.getRawParameterValue (DeathDealerDrumsAudioProcessor::trackParamID (i, "choke_trig_slot"));
        c.chokeTrigDelay = apvts.getRawParameterValue (DeathDealerDrumsAudioProcessor::trackParamID (i, "choke_trig_delay"));
        c.mute       = apvts.getRawParameterValue (DeathDealerDrumsAudioProcessor::trackParamID (i, "mute"));
        c.solo       = apvts.getRawParameterValue (DeathDealerDrumsAudioProcessor::trackParamID (i, "solo"));
        c.phase      = apvts.getRawParameterValue (DeathDealerDrumsAudioProcessor::trackParamID (i, "phase"));
        c.output     = apvts.getRawParameterValue (DeathDealerDrumsAudioProcessor::trackParamID (i, "output"));
        c.outputMode = apvts.getRawParameterValue (DeathDealerDrumsAudioProcessor::trackParamID (i, "output_mode"));
        c.eq8On      = apvts.getRawParameterValue (DeathDealerDrumsAudioProcessor::trackParamID (i, "eq8_on"));
        c.trkCompOn  = apvts.getRawParameterValue (DeathDealerDrumsAudioProcessor::trackParamID (i, "trk_comp_on"));
        c.trkCompThr = apvts.getRawParameterValue (DeathDealerDrumsAudioProcessor::trackParamID (i, "trk_comp_thr"));
        c.trkCompRat = apvts.getRawParameterValue (DeathDealerDrumsAudioProcessor::trackParamID (i, "trk_comp_rat"));
        c.trkCompAtk = apvts.getRawParameterValue (DeathDealerDrumsAudioProcessor::trackParamID (i, "trk_comp_atk"));
        c.trkCompRel = apvts.getRawParameterValue (DeathDealerDrumsAudioProcessor::trackParamID (i, "trk_comp_rel"));
        c.trkCompMkp = apvts.getRawParameterValue (DeathDealerDrumsAudioProcessor::trackParamID (i, "trk_comp_mkp"));
        c.bleedSend  = apvts.getRawParameterValue (DeathDealerDrumsAudioProcessor::trackParamID (i, "bleed_send"));
        c.bleedEnable= apvts.getRawParameterValue (DeathDealerDrumsAudioProcessor::trackParamID (i, "bleed_enable"));
        c.trkTransOn  = apvts.getRawParameterValue (DeathDealerDrumsAudioProcessor::trackParamID (i, "trk_trans_on"));
        c.trkTransAtk = apvts.getRawParameterValue (DeathDealerDrumsAudioProcessor::trackParamID (i, "trk_trans_atk"));
        c.trkTransSus = apvts.getRawParameterValue (DeathDealerDrumsAudioProcessor::trackParamID (i, "trk_trans_sus"));

        for (int b = 0; b < EQ8_BANDS; ++b)
        {
            const juce::String prefix = DeathDealerDrumsAudioProcessor::trackParamID (i, "eq8_b" + juce::String (b));
            c.eq8Freq[(size_t) b] = apvts.getRawParameterValue (prefix + "_freq");
            c.eq8Gain[(size_t) b] = apvts.getRawParameterValue (prefix + "_gain");
            c.eq8Q[(size_t) b]    = apvts.getRawParameterValue (prefix + "_q");
        }
    }

    trackParamCacheReady = true;
}

//==============================================================================
void DrumEngine::process (juce::AudioBuffer<float>& buffer,
                           juce::MidiBuffer& midiMessages,
                           juce::AudioProcessorValueTreeState& apvts,
                           const std::vector<std::unique_ptr<DrumTrack>>& tracks,
                           const int* busChannelMap)
{
    const int numSamples = buffer.getNumSamples();
    const int numTracks  = (int) tracks.size();

    ensureTrackParamCache (apvts);

    sampleCounter += numSamples;   // advance running position for double-strike detection

    const float masterVol =
        *apvts.getRawParameterValue (DeathDealerDrumsAudioProcessor::PARAM_MASTER_VOLUME);
    const bool bleedSolo =
        (*apvts.getRawParameterValue (DeathDealerDrumsAudioProcessor::PARAM_BLEED_SOLO) > 0.5f);
    const float humanErrorScatter = juce::jlimit (
        0.05f, 0.20f,
        apvts.getRawParameterValue (DeathDealerDrumsAudioProcessor::PARAM_HUMAN_ERROR)->load());

    for (int i = 0; i < numTracks; ++i)
        padBuffers[(size_t) i].clear (0, numSamples);
    buffer.clear();

    if (numTracks == 0) return;

    // Build MIDI-note → track-index table
    // Multiple tracks can share the same MIDI note — all are triggered together.
    std::vector<int> midiTable[128];
    int midiNoteForTrack[MAX_TRACKS] = {};
    int slaveToTrack[MAX_TRACKS] = {};
    bool trackIsFollower[MAX_TRACKS] = {};
    bool trackIsMaster[MAX_TRACKS] = {};
    for (int i = 0; i < numTracks; ++i)
    {
        const int note = juce::roundToInt (
            apvts.getRawParameterValue (
                DeathDealerDrumsAudioProcessor::trackParamID (i, "midi_note"))->load());
        midiNoteForTrack[i] = note;

        int slaveTo = -1;
        if (auto* raw = apvts.getRawParameterValue (
                DeathDealerDrumsAudioProcessor::trackParamID (i, "slave_to")))
            slaveTo = juce::roundToInt (raw->load());
        slaveToTrack[i] = slaveTo;

        if (slaveTo >= 0 && slaveTo < numTracks && slaveTo != i)
        {
            trackIsFollower[i] = true;
            trackIsMaster[slaveTo] = true;
        }

        if (note >= 0 && note < 128)
            midiTable[note].push_back (i);
    }

    // Process MIDI
    int lastEventSampleForRoot[MAX_TRACKS];
    for (int i = 0; i < MAX_TRACKS; ++i)
        lastEventSampleForRoot[i] = std::numeric_limits<int>::min();

    auto resolveSlaveRoot = [&] (int startTrack) -> int
    {
        int root = startTrack;
        bool seen[MAX_TRACKS] = {};

        while (true)
        {
            if (root < 0 || root >= numTracks)
                return startTrack;

            const int target = slaveToTrack[root];
            if (target < 0 || target >= numTracks || target == root)
                return root;
            if (seen[target])
                return root;

            seen[target] = true;
            root = target;
        }
    };

    for (const auto meta : midiMessages)
    {
        const auto msg = meta.getMessage();
        if (msg.isNoteOn() && msg.getVelocity() > 0)
        {
            const int eventSamplePos = meta.samplePosition;
            const int note = msg.getNoteNumber();
            const auto& directHitTracks = midiTable[note];
            if (directHitTracks.empty())
                continue;

            const float velNorm = msg.getVelocity() / 127.0f;

            bool rootActive[MAX_TRACKS] = {};
            int resolvedRoot[MAX_TRACKS];
            for (int i = 0; i < MAX_TRACKS; ++i)
                resolvedRoot[i] = -1;

            for (int i = 0; i < numTracks; ++i)
                resolvedRoot[i] = resolveSlaveRoot (i);

            for (const int idx : directHitTracks)
            {
                if (idx < 0 || idx >= numTracks)
                    continue;
                const int root = resolvedRoot[idx] >= 0 ? resolvedRoot[idx] : idx;
                // Strict slave behavior: only a root/master track can trigger the group.
                // If a follower's own MIDI note is played, ignore it to avoid duplicate
                // re-triggers (a common source of perceived flam in layered kicks).
                if (root >= 0 && root < numTracks && idx == root)
                    rootActive[root] = true;
            }

            std::vector<int> hitTracks;
            hitTracks.reserve ((size_t) numTracks);
            for (int i = 0; i < numTracks; ++i)
            {
                const int root = resolvedRoot[i];
                if (root >= 0 && root < numTracks && rootActive[root])
                    hitTracks.push_back (i);
            }

            if (hitTracks.empty())
                continue;

            int rootForTrack[MAX_TRACKS];
            for (int i = 0; i < MAX_TRACKS; ++i)
                rootForTrack[i] = -1;

            for (const int idx : hitTracks)
            {
                if (idx < 0 || idx >= numTracks)
                    continue;
                rootForTrack[idx] = resolvedRoot[idx] >= 0 ? resolvedRoot[idx] : idx;
            }

            bool rootTriggered[MAX_TRACKS] = {};
            TriggerHumanization rootHumanization[MAX_TRACKS];

            auto getTrimStartSamplesForTrack = [&] (int trackIdx, const DrumVariation& v) -> int
            {
                float startTrimNorm = 0.0f;
                if (auto* raw = apvts.getRawParameterValue (
                        DeathDealerDrumsAudioProcessor::trackParamID (trackIdx, "sample_start")))
                    startTrimNorm = juce::jlimit (0.0f, 1.0f, raw->load());

                const int len = v.buffer.getNumSamples();
                return juce::jlimit (0, juce::jmax (0, len - 1),
                    (int) std::floor (startTrimNorm * (float) len));
            };

            // Trigger each root once, then trigger all followers with shared context.
            for (const int idx : hitTracks)
            {
                const int root = rootForTrack[idx] >= 0 ? rootForTrack[idx] : idx;
                if (root < 0 || root >= numTracks || rootTriggered[root])
                    continue;

                // Prevent duplicate same-sample retriggers for the same slave root.
                if (lastEventSampleForRoot[root] == eventSamplePos)
                    continue;
                lastEventSampleForRoot[root] = eventSamplePos;

                const DrumVariation* masterVar = tracks[(size_t) root]->getNextVariationForVelocity (velNorm);
                if (masterVar == nullptr || !masterVar->valid)
                    continue;

                bool rootHasFollowers = false;
                for (const int follower : hitTracks)
                {
                    if (follower == root) continue;
                    if (rootForTrack[follower] == root)
                    {
                        rootHasFollowers = true;
                        break;
                    }
                }

                // Human Error knob should affect only independent tracks.
                // If a track participates in a slave relationship (master or follower),
                // remove all HE randomization for this grouped trigger.
                const bool rootIsSlaveOrMaster = (root >= 0 && root < numTracks)
                    ? (trackIsFollower[root] || trackIsMaster[root] || rootHasFollowers)
                    : rootHasFollowers;
                const float groupHumanErrorScatter = rootIsSlaveOrMaster ? 0.0f : humanErrorScatter;

                if (rootIsSlaveOrMaster)
                {
                    rootHumanization[root] = TriggerHumanization{};
                    rootHumanization[root].pendingDelaySamples = 0;
                    rootHumanization[root].startOffsetMs       = 0.0f;
                    rootHumanization[root].pitchMicroOffset    = 1.0f;
                    rootHumanization[root].amplitudeTrim       = 1.0f;
                    rootHumanization[root].doubleStrikeGain    = 1.0f;
                    rootHumanization[root].velocityScatter     = 1.0f;
                    rootHumanization[root].noiseSamplesLeft    = 0;
                    rootHumanization[root].noiseEnv            = 0.0f;
                    rootHumanization[root].noiseDecay          = 1.0f;
                    rootHumanization[root].bodyFreq            = 120.0f;
                    rootHumanization[root].bodyDb              = 0.0f;
                    rootHumanization[root].presFreq            = 3000.0f;
                    rootHumanization[root].presDb              = 0.0f;
                }
                else
                {
                    rootHumanization[root] = makeTriggerHumanization (velNorm, groupHumanErrorScatter);
                }

                // For grouped slave hits, always use hard anti-flam lock.
                // Independent tracks continue to use normal (non-strict) behavior.
                const bool useHardAntiFlamLock = rootHasFollowers;

                if (useHardAntiFlamLock)
                {
                    rootHumanization[root].pendingDelaySamples = 0;
                    rootHumanization[root].startOffsetMs = 0.0f;
                }

                // Shared double-strike suppression for root + followers.
                {
                    const juce::int64 gapSamples = sampleCounter - lastTriggerSample[(size_t) root];
                    const float       gapMs      = (float) gapSamples / (float) currentSampleRate * 1000.f;
                    float strikeGain = 1.0f;
                    if (gapMs < 40.f)
                    {
                        const float suppressDb = 8.0f * juce::jmax (0.f, 1.f - gapMs / 40.f);
                        strikeGain = std::pow (10.f, -suppressDb / 20.f);
                    }
                    rootHumanization[root].doubleStrikeGain = strikeGain;
                    lastTriggerSample[(size_t) root] = sampleCounter;
                }

                rootTriggered[root] = true;

                const int masterTier = tracks[(size_t) root]->lastUsedTier.load();
                const int masterSlot = tracks[(size_t) root]->lastUsedSlot.load();

                struct GroupVoice
                {
                    int track { -1 };
                    const DrumVariation* variation { nullptr };
                    int tier { -1 };
                    int slot { -1 };
                    bool hasAlignment { false };
                    PhaseAlignment alignment;
                    int onsetSample { 0 };
                };

                std::vector<GroupVoice> group;
                group.reserve ((size_t) hitTracks.size());

                {
                    GroupVoice gv;
                    gv.track = root;
                    gv.variation = masterVar;
                    gv.tier = masterTier;
                    gv.slot = masterSlot;
                    const int trimStart = getTrimStartSamplesForTrack (root, *masterVar);
                    gv.onsetSample = juce::jmax (detectTransientOnsetSample (masterVar->buffer), trimStart);
                    group.push_back (gv);
                }

                for (const int follower : hitTracks)
                {
                    if (follower == root) continue;
                    if (rootForTrack[follower] != root) continue;

                    int tier = masterTier;
                    int slot = masterSlot;
                    const DrumVariation* v = nullptr;

                    if (masterTier >= 0 && masterSlot >= 0)
                        v = tracks[(size_t) follower]->getVariationForTier (masterTier, masterSlot);

                    if (v == nullptr || !v->valid)
                    {
                        v = tracks[(size_t) follower]->getNextVariationForVelocity (velNorm);
                        tier = tracks[(size_t) follower]->lastUsedTier.load();
                        slot = tracks[(size_t) follower]->lastUsedSlot.load();
                    }

                    if (v == nullptr || !v->valid)
                        continue;

                    GroupVoice gv;
                    gv.track = follower;
                    gv.variation = v;
                    gv.tier = tier;
                    gv.slot = slot;
                    const int trimStart = getTrimStartSamplesForTrack (follower, *v);
                    gv.onsetSample = juce::jmax (detectTransientOnsetSample (v->buffer), trimStart);

                    const bool canAlign = (!useHardAntiFlamLock)
                                        && (masterTier >= 0 && masterTier < NUM_VEL_TIERS)
                                        && (masterSlot >= 0 && masterSlot < VARS_PER_TIER);
                    if (canAlign)
                    {
                        gv.alignment = getCachedPhaseAlignment (root, follower, masterTier, masterSlot,
                                                                *masterVar, *v);
                        gv.hasAlignment = true;
                    }

                    group.push_back (gv);
                }

                for (const auto& gv : group)
                {
                    const int strictStartOverride = useHardAntiFlamLock
                        ? juce::jlimit (0,
                                        juce::jmax (0, gv.variation->buffer.getNumSamples() - 1),
                                        gv.onsetSample)
                        : -1;
                    // In hard lock we ignore timing offsets, but still pass alignment
                    // so polarity can be corrected safely without adding flam.
                    const PhaseAlignment* alignPtr = gv.hasAlignment ? &gv.alignment : nullptr;

                    startVoiceFromVariation (gv.track, *gv.variation, velNorm, groupHumanErrorScatter,
                                             gv.tier, gv.slot,
                                             &rootHumanization[root],
                                             alignPtr,
                                             strictStartOverride,
                                             useHardAntiFlamLock && (gv.track != root));
                }

                // Choke-trigger secondary sample: fire at note-on time.
                // (Also fires from triggerVariationDirect for UI pad-button path.)
                {
                    const auto& ctc = trackParamCache[(size_t) root];
                    if (ctc.chokeTrigOn  && ctc.chokeTrigOn->load()  > 0.5f
                     && ctc.chokeTrigSlot)
                    {
                        const int dstSlot    = juce::jlimit (0, numTracks - 1,
                                                  juce::roundToInt (ctc.chokeTrigSlot->load()));
                        const int extraDelay = (ctc.chokeTrigDelay != nullptr)
                            ? juce::jmax (0, juce::roundToInt (ctc.chokeTrigDelay->load() * 0.001f * (float) currentSampleRate))
                            : 0;
                        handleNoteOn (dstSlot, velNorm, humanErrorScatter, tracks, extraDelay);
                    }
                }
            }
        }
    }

    // Read per-track params
    struct TP { float vol, pan, tune, decay, attack, eqL, eqM, eqH, startTrim, endTrim, reverbSend, compSend, satSend;
                bool choke, mute, solo, phase; int output, outputMode; };

    auto paramChanged = [] (float oldV, float newV, float eps = 1.0e-4f) noexcept
    {
        return !std::isfinite (oldV) || std::abs (oldV - newV) > eps;
    };

    auto loadParam = [] (std::atomic<float>* p, float fallback = 0.0f) noexcept
    {
        return p != nullptr ? p->load() : fallback;
    };

    TP tp[MAX_TRACKS];
    for (int i = 0; i < numTracks; ++i)
    {
        const size_t si = (size_t) i;
        const auto& pc   = trackParamCache[si];
        tp[si].vol        = loadParam (pc.vol, 1.0f);
        tp[si].pan        = loadParam (pc.pan, 0.0f);
        tp[si].tune       = loadParam (pc.tune, 0.0f);
        tp[si].decay      = loadParam (pc.decay, 1.0f);
        tp[si].attack     = loadParam (pc.attack, 0.0f);
        tp[si].eqL        = loadParam (pc.eqL, 0.0f);
        tp[si].eqM        = loadParam (pc.eqM, 0.0f);
        tp[si].eqH        = loadParam (pc.eqH, 0.0f);
        tp[si].startTrim  = loadParam (pc.startTrim, 0.0f);
        tp[si].endTrim    = loadParam (pc.endTrim, 1.0f);
        tp[si].reverbSend = loadParam (pc.reverbSend, 0.0f);
        tp[si].compSend   = loadParam (pc.compSend, 0.0f);
        tp[si].satSend    = loadParam (pc.satSend, 0.0f);
        tp[si].choke      = (loadParam (pc.choke, 0.0f) > 0.5f);
        tp[si].mute       = (loadParam (pc.mute, 0.0f) > 0.5f);
        tp[si].solo       = (loadParam (pc.solo, 0.0f) > 0.5f);
        tp[si].phase      = (loadParam (pc.phase, 0.0f) > 0.5f);
        tp[si].output     = juce::roundToInt (loadParam (pc.output, 0.0f));
        tp[si].outputMode = juce::roundToInt (loadParam (pc.outputMode, 0.0f));
        tp[si].output     = juce::jlimit (0, 15, tp[si].output);

        if (paramChanged (lastEqLow[si], tp[si].eqL)
         || paramChanged (lastEqMid[si], tp[si].eqM)
         || paramChanged (lastEqHigh[si], tp[si].eqH))
        {
            rebuildEQCoeffs (i, tp[si].eqL, tp[si].eqM, tp[si].eqH);
            lastEqLow[si]  = tp[si].eqL;
            lastEqMid[si]  = tp[si].eqM;
            lastEqHigh[si] = tp[si].eqH;
        }

        std::array<float, EQ8_BANDS> eq8Freqs {};
        std::array<float, EQ8_BANDS> eq8Gains {};
        std::array<float, EQ8_BANDS> eq8Qs {};
        bool eq8Dirty = false;
        for (int b = 0; b < EQ8_BANDS; ++b)
        {
            const float freq = loadParam (pc.eq8Freq[(size_t) b], 1000.0f);
            const float gain = loadParam (pc.eq8Gain[(size_t) b], 0.0f);
            const float qVal = loadParam (pc.eq8Q[(size_t) b], 1.0f);

            eq8Freqs[(size_t) b] = freq;
            eq8Gains[(size_t) b] = gain;
            eq8Qs[(size_t) b] = qVal;

            if (paramChanged (lastEq8Freq[si][(size_t) b], freq)
             || paramChanged (lastEq8Gain[si][(size_t) b], gain)
             || paramChanged (lastEq8Q[si][(size_t) b], qVal))
                eq8Dirty = true;
        }

        if (eq8Dirty)
        {
            rebuildEQ8Coeffs (i, eq8Freqs, eq8Gains, eq8Qs);
            lastEq8Freq[si] = eq8Freqs;
            lastEq8Gain[si] = eq8Gains;
            lastEq8Q[si]    = eq8Qs;
        }
    }

    // Pre-compute which output slots have at least one solo active
    bool outputHasSolo[16] = {};
    for (int i = 0; i < numTracks; ++i)
        if (tp[(size_t) i].solo)
            outputHasSolo[tp[(size_t) i].output] = true;

    // Per-track peak accumulator for this block
    float blockPeak[MAX_TRACKS] = {};

    // Render voices
    for (auto& voice : voices)
    {
        if (!voice.active) continue;
        const int ti = voice.trackIndex;
        if (ti < 0 || ti >= numTracks) { voice.active = false; continue; }

        const auto& p = tp[(size_t) ti];
        const double tuneRatio = std::pow (2.0, (double) p.tune / 12.0);
        voice.playbackRate = (voice.sourceSampleRate / currentSampleRate) * tuneRatio
                             * (double) voice.pitchMicroOffset;

        if (p.decay < 0.999f)
        {
            const float ms = 30.f + 1970.f * p.decay;
            voice.decayRate = 1.f - std::exp (-1.f / ((float) currentSampleRate * ms * 0.001f));
        }
        else voice.decayRate = 0.f;

        auto*       bufL = padBuffers[(size_t) ti].getWritePointer (0);
        auto*       bufR = padBuffers[(size_t) ti].getWritePointer (1);
        const auto* srcL = voice.audioData->getReadPointer (0);
        const auto* srcR = voice.audioData->getNumChannels() > 1
                         ? voice.audioData->getReadPointer (1)
                         : voice.audioData->getReadPointer (0);
        const int srcLen = voice.audioData->getNumSamples();
        const int trimStart = juce::jlimit (0, juce::jmax (0, srcLen - 1),
            (int) std::floor (juce::jlimit (0.0f, 1.0f, p.startTrim) * (float) srcLen));
        int trimEnd = juce::jlimit (trimStart + 1, srcLen,
            (int) std::ceil (juce::jlimit (0.0f, 1.0f, p.endTrim) * (float) srcLen));
        if (trimEnd <= trimStart) trimEnd = juce::jmin (srcLen, trimStart + 1);

        if (voice.playbackPosition < (double) trimStart)
            voice.playbackPosition = (double) trimStart;

        for (int s = 0; s < numSamples; ++s)
        {
            if (!voice.active) break;

            // Hit-timing humanization: count down before starting output
            if (voice.pendingDelaySamples > 0)
            {
                --voice.pendingDelaySamples;
                continue;
            }

            if (voice.playbackPosition >= (double) trimEnd)
            { voice.active = false; break; }

            const int maxInterpIndex = juce::jmax (trimStart, trimEnd - 1);
            const int maxNextIndex   = juce::jmax (trimStart, trimEnd - 1);
            const int   p0 = juce::jlimit (trimStart, maxInterpIndex, (int) voice.playbackPosition);
            const int   p1 = juce::jlimit (trimStart, maxNextIndex, p0 + 1);
            const float pf = (float) (voice.playbackPosition - (int) voice.playbackPosition);
            float sL = srcL[p0] + pf * (srcL[p1] - srcL[p0]);
            float sR = srcR[p0] + pf * (srcR[p1] - srcR[p0]);

            sL *= voice.amplitude * voice.amplitudeTrim;
            sR *= voice.amplitude * voice.amplitudeTrim;

            // Velocity-sensitive brightness — per-voice high-shelf
            sL = processBiquad (sL, voice.velBrightCoeffs, voice.velBrightStateL);
            sR = processBiquad (sR, voice.velBrightCoeffs, voice.velBrightStateR);

            // Per-hit random tonal color: body resonance (80–250 Hz) + presence (2–5 kHz)
            sL = processBiquad (sL, voice.bodyRandCoeffs, voice.bodyRandStateL);
            sR = processBiquad (sR, voice.bodyRandCoeffs, voice.bodyRandStateR);
            sL = processBiquad (sL, voice.presenceRandCoeffs, voice.presenceRandStateL);
            sR = processBiquad (sR, voice.presenceRandCoeffs, voice.presenceRandStateR);

            if (voice.forceInvertPolarity)
            {
                sL = -sL;
                sR = -sR;
            }

            if (voice.transientBlendSamples > 0 && voice.transientBlendPos < voice.transientBlendSamples)
            {
                const float t = (float) voice.transientBlendPos / (float) voice.transientBlendSamples;
                const float blend = t * t; // slightly softer early ramp
                sL *= blend;
                sR *= blend;
                ++voice.transientBlendPos;
            }

            // Unique per-trigger stick-contact noise burst (5 ms, different every hit)
            if (voice.noiseSamplesLeft > 0)
            {
                voice.noiseState = voice.noiseState * 1664525u + 1013904223u;
                const float n    = (float)(int)(voice.noiseState >> 16) / 32767.5f - 1.0f;
                sL += n * voice.noiseEnv;
                sR += n * voice.noiseEnv;
                voice.noiseEnv *= voice.noiseDecay;
                --voice.noiseSamplesLeft;
            }

            // Attack: extends the onset ramp-up time. 0 = instant (just the baked 2ms fade),
            // 1.0 = 30ms linear ramp from silence to full. Fully disengaged at default (0).
            if (p.attack > 0.001f)
            {
                const float attackMs  = p.attack * 30.f;
                const float atMs      = (float) voice.playbackPosition / (float) currentSampleRate * 1000.f;
                if (atMs < attackMs)
                {
                    const float ramp = atMs / attackMs;
                    sL *= ramp; sR *= ramp;
                }
            }

            // Decay envelope
            if (voice.decayRate > 0.f)
            {
                if (p.choke)
                {
                    // Choke mode: 8x faster decay, hard-cut below 0.05 for abrupt stop
                    const float cr = juce::jmin (1.0f, voice.decayRate * 8.0f);
                    voice.decayEnv += (0.f - voice.decayEnv) * cr;
                    if (voice.decayEnv < 0.05f)
                    {
                        voice.decayEnv = 0.f;
                        voice.active = false;
                    }
                }
                else
                {
                    voice.decayEnv += (0.f - voice.decayEnv) * voice.decayRate;
                }
                sL *= voice.decayEnv;
                sR *= voice.decayEnv;
            }

            // Anti-click: linear fade over last 512 source samples before trimEnd.
            // Prevents a hard pop when a sample ends without a natural zero-crossing.
            {
                constexpr int kEndFade = 512;
                if ((trimEnd - trimStart) > kEndFade)
                {
                    const double fadeStart = (double)(trimEnd - kEndFade);
                    if (voice.playbackPosition >= fadeStart)
                    {
                        const float fade = (float)(trimEnd - voice.playbackPosition) / (float)kEndFade;
                        sL *= juce::jmax (0.f, fade);
                        sR *= juce::jmax (0.f, fade);
                    }
                }
            }

            bufL[s] += sL;
            bufR[s] += sR;
            voice.playbackPosition += voice.playbackRate;

            if (voice.decayEnv < 0.0001f || voice.playbackPosition >= trimEnd)
            { voice.active = false; break; }
        }
    }

    // Sums all padBuffers, LPFs to body frequencies only (< 400 Hz),
    // then cross-feeds a scaled fraction into every OTHER track's padBuffer.
    // Knob 0-10 maps to 0-0.04 linear (≈ -28 dB at max), realistic bleed level.
    {
        const float bleedAmount = apvts.getRawParameterValue (
            DeathDealerDrumsAudioProcessor::PARAM_BLEED_AMOUNT)->load();
        if (numTracks > 1 && (bleedAmount > 0.001f || bleedSolo))
        {
            const float bleedGain = (bleedAmount / 10.0f) * 0.04f;
            const float lpfCoef   = 1.0f - std::exp (
                -2.0f * juce::MathConstants<float>::pi * 400.0f / (float) currentSampleRate);

            // Sum all pad voices into scratch bleed bus, scaled by each track's send level
            bleedBuf.clear (0, numSamples);
            for (int i = 0; i < numTracks; ++i)
            {
                const float sendLvl = loadParam (trackParamCache[(size_t) i].bleedSend, 0.0f);
                if (sendLvl < 0.001f) continue;
                bleedBuf.addFrom (0, 0, padBuffers[(size_t) i], 0, 0, numSamples, sendLvl);
                bleedBuf.addFrom (1, 0, padBuffers[(size_t) i], 1, 0, numSamples, sendLvl);
            }

            // One-pole LPF — keeps body thump, suppresses transient click bleed
            float* bL = bleedBuf.getWritePointer (0);
            float* bR = bleedBuf.getWritePointer (1);
            for (int n = 0; n < numSamples; ++n)
            {
                bleedLpfStateL += lpfCoef * (bL[n] - bleedLpfStateL);
                bleedLpfStateR += lpfCoef * (bR[n] - bleedLpfStateR);
                bL[n] = bleedLpfStateL;
                bR[n] = bleedLpfStateR;
            }

            // Add (bleed_bus - this_track) * bleedGain to each padBuffer (only enabled tracks)
            if (bleedAmount > 0.001f)
            {
                for (int i = 0; i < numTracks; ++i)
                {
                    const bool bleedOn = loadParam (trackParamCache[(size_t) i].bleedEnable, 0.0f) > 0.5f;
                    if (!bleedOn) continue;
                    float*       dL  = padBuffers[(size_t) i].getWritePointer (0);
                    float*       dR  = padBuffers[(size_t) i].getWritePointer (1);
                    const float* pL  = padBuffers[(size_t) i].getReadPointer  (0);
                    const float* pR  = padBuffers[(size_t) i].getReadPointer  (1);
                    for (int n = 0; n < numSamples; ++n)
                    {
                        dL[n] += (bL[n] - pL[n]) * bleedGain;
                        dR[n] += (bR[n] - pR[n]) * bleedGain;
                    }
                }
            }

            // Bleed solo: write raw bleed bus to main output for monitoring
            if (bleedSolo)
            {
                const int numCh = buffer.getNumChannels();
                for (int n = 0; n < numSamples; ++n)
                {
                    if (numCh > 0) buffer.setSample (0, n, bL[n]);
                    if (numCh > 1) buffer.setSample (1, n, bR[n]);
                }
            }
        }
    }

    // Per-track DSP + accumulate into master
    for (int i = 0; i < numTracks; ++i)
    {
        const auto  si  = (size_t) i;
        auto&       pb  = padBuffers[si];
        auto&       dsp = padDSP[si];
        const auto& p   = tp[si];

        const bool needsSpectrum = (i == spectrumTrack.load() && spectrumFFT);
        const float inMagL = pb.getMagnitude (0, 0, numSamples);
        const float inMagR = pb.getMagnitude (1, 0, numSamples);
        const bool hasInputSignal = juce::jmax (inMagL, inMagR) > 1.0e-6f;

        if (hasInputSignal)
        {
            silentBlockCount[si] = 0;
        }
        else
        {
            silentBlockCount[si] = juce::jmin (1000000, silentBlockCount[si] + 1);
            constexpr int kSilentBlocksBeforeSkip = 4;

            // Conservative fast path: only skip after multiple fully silent blocks,
            // and keep selected spectrum track active so analyser falls to silence.
            if (!needsSpectrum && silentBlockCount[si] >= kSilentBlocksBeforeSkip)
            {
                if (silentBlockCount[si] == kSilentBlocksBeforeSkip)
                    dsp.reset();
                trackCompGrDb[i].store (0.0f);
                continue;
            }
        }

        float* L = pb.getWritePointer (0);
        float* R = pb.getWritePointer (1);

        for (int n = 0; n < numSamples; ++n)
        {
            L[n] = processBiquad (L[n], lowShelfCoeffs[si],  dsp.lowShelfState[0]);
            R[n] = processBiquad (R[n], lowShelfCoeffs[si],  dsp.lowShelfState[1]);
            L[n] = processBiquad (L[n], peakMidCoeffs[si],   dsp.peakMidState[0]);
            R[n] = processBiquad (R[n], peakMidCoeffs[si],   dsp.peakMidState[1]);
            L[n] = processBiquad (L[n], highShelfCoeffs[si], dsp.highShelfState[0]);
            R[n] = processBiquad (R[n], highShelfCoeffs[si], dsp.highShelfState[1]);
        }

        // 8-band parametric EQ
        {
            const bool eq8On = loadParam (trackParamCache[si].eq8On, 0.0f) > 0.5f;
            if (eq8On)
            {
                for (int n = 0; n < numSamples; ++n)
                {
                    for (int b = 0; b < EQ8_BANDS; ++b)
                    {
                        const int passes = eq8Passes[si][b];
                        for (int p = 0; p < passes; ++p)
                        {
                            L[n] = processBiquad (L[n], eq8Coeffs[si][b], dsp.eq8State[b][p][0]);
                            R[n] = processBiquad (R[n], eq8Coeffs[si][b], dsp.eq8State[b][p][1]);
                        }
                    }
                }
            }
        }

        // Per-track feed-forward compressor (enabled by trk_comp_on)
        {
            const bool compOn = loadParam (trackParamCache[si].trkCompOn, 0.0f) > 0.5f;
            if (compOn)
            {
                const auto& pc = trackParamCache[si];
                const float thrDb   = loadParam (pc.trkCompThr, -18.0f);
                const float ratio   = loadParam (pc.trkCompRat, 4.0f);
                const float atkMs   = loadParam (pc.trkCompAtk, 10.0f);
                const float relMs   = loadParam (pc.trkCompRel, 100.0f);
                const float mkpDb   = loadParam (pc.trkCompMkp, 0.0f);

                const float thrLin  = juce::Decibels::decibelsToGain (thrDb);
                const float mkpLin  = juce::Decibels::decibelsToGain (mkpDb);
                const float invRat  = 1.0f / ratio;
                const float atkCoef = 1.0f - std::exp (-1.0f / (atkMs * 0.001f * (float) currentSampleRate));
                const float relCoef = 1.0f - std::exp (-1.0f / (relMs * 0.001f * (float) currentSampleRate));

                float blockGrDb = 0.0f;
                auto& comp = dsp.comp;
                for (int n = 0; n < numSamples; ++n)
                {
                    const float peak = juce::jmax (std::abs (L[n]), std::abs (R[n]));
                    const float coef = (peak > comp.envL) ? atkCoef : relCoef;
                    comp.envL += (peak - comp.envL) * coef;

                    float gr = 1.0f;
                    if (comp.envL > thrLin && comp.envL > 1e-9f)
                    {
                        const float inDb  = juce::Decibels::gainToDecibels (comp.envL);
                        const float outDb = thrDb + (inDb - thrDb) * invRat;
                        gr = juce::Decibels::decibelsToGain (outDb - inDb);
                    }
                    const float grDb = juce::Decibels::gainToDecibels (gr);
                    if (grDb < blockGrDb) blockGrDb = grDb;

                    L[n] *= gr * mkpLin;
                    R[n] *= gr * mkpLin;
                }
                trackCompGrDb[i].store (blockGrDb);
            }
            else
            {
                trackCompGrDb[i].store (0.0f);
                dsp.comp.reset();
            }
        }

        // Per-track transient designer (two-envelope method)
        {
            const auto& pc = trackParamCache[si];
            const bool transOn = loadParam (pc.trkTransOn, 0.f) > 0.5f;
            if (transOn)
            {
                const float atkDb = loadParam (pc.trkTransAtk, 0.f);
                const float susDb = loadParam (pc.trkTransSus, 0.f);
                // Fast envelope ~1ms attack, ~50ms release → tracks transient peaks
                const float atkCoef  = 1.f - std::exp (-1.f / (0.001f * (float) currentSampleRate));
                const float relCoef  = 1.f - std::exp (-1.f / (0.050f * (float) currentSampleRate));
                // Slow envelope ~200ms → tracks sustained body
                const float slowCoef = 1.f - std::exp (-1.f / (0.200f * (float) currentSampleRate));
                const float atkG = std::pow (10.f, atkDb / 20.f);
                const float susG = std::pow (10.f, susDb / 20.f);
                auto& ts = dsp.trans;
                for (int n = 0; n < numSamples; ++n)
                {
                    const float x = juce::jmax (std::abs (L[n]), std::abs (R[n]));
                    ts.envFast += (x > ts.envFast ? atkCoef : relCoef) * (x - ts.envFast);
                    ts.envSlow += slowCoef * (x - ts.envSlow);
                    if (ts.envFast > 1e-6f)
                    {
                        // tRat: how much of the fast envelope is above the slow (0..1)
                        // sRat: remainder — always sums to 1, so gain is bounded
                        const float tRat = juce::jmax (0.f, (ts.envFast - ts.envSlow) / ts.envFast);
                        const float sRat = 1.f - tRat;
                        const float gain = tRat * atkG + sRat * susG;
                        L[n] *= gain;
                        R[n] *= gain;
                    }
                }
            }
            else { dsp.trans.reset(); }
        }

        // Spectrum analysis — fill FFT buffer for the selected track
        if (needsSpectrum)
        {
            for (int n = 0; n < numSamples; ++n)
            {
                spectrumFftBuf[spectrumFillPos] = (L[n] + R[n]) * 0.5f;
                ++spectrumFillPos;
                if (spectrumFillPos >= SPECTRUM_FFT_SIZE)
                {
                    // Apply Hann window to first half
                    for (int k = 0; k < SPECTRUM_FFT_SIZE; ++k)
                        spectrumFftBuf[k] *= spectrumWindow[k];
                    // Zero imaginary / scratch half
                    std::fill (spectrumFftBuf.begin() + SPECTRUM_FFT_SIZE,
                               spectrumFftBuf.end(), 0.f);
                    // FFT: output magnitudes in first SPECTRUM_BINS floats
                    spectrumFFT->performFrequencyOnlyForwardTransform (spectrumFftBuf.data());
                    std::copy (spectrumFftBuf.begin(),
                               spectrumFftBuf.begin() + SPECTRUM_BINS,
                               std::begin (spectrumData));
                    spectrumReady.store (true);
                    spectrumFillPos = 0;
                    // Refill cleared positions
                    std::fill (spectrumFftBuf.begin(), spectrumFftBuf.end(), 0.f);
                }
            }
        }

        for (int n = 0; n < numSamples; ++n)
        {
            const float panA = (p.pan + 1.f) * 0.5f * juce::MathConstants<float>::halfPi;
            L[n] *= p.vol * std::cos (panA);
            R[n] *= p.vol * std::sin (panA);
        }

        // Phase inversion (polarity flip)
        if (p.phase)
        {
            for (int n = 0; n < numSamples; ++n) { L[n] = -L[n]; R[n] = -R[n]; }
        }

        // Route to assigned output bus, respecting mute and solo
        if (!bleedSolo && !p.mute && (!outputHasSolo[p.output] || p.solo))
        {
            int chL         = busChannelMap[p.output];
            if (chL < 0)
                chL = busChannelMap[0]; // fallback to main out if selected bus is disabled
            const int chR   = (chL >= 0) ? chL + 1 : -1;
            const int numCh = buffer.getNumChannels();
            if (p.outputMode == 1) // Mono: sum L+R
            {
                for (int n = 0; n < numSamples; ++n)
                {
                    const float m = (pb.getSample (0, n) + pb.getSample (1, n)) * 0.5f;
                    if (chL >= 0 && chL < numCh) buffer.addSample (chL, n, m);
                    if (chR >= 0 && chR < numCh) buffer.addSample (chR, n, m);
                }
            }
            else // Stereo
            {
                if (chL >= 0 && chL < numCh) buffer.addFrom (chL, 0, pb, 0, 0, numSamples);
                if (chR >= 0 && chR < numCh) buffer.addFrom (chR, 0, pb, 1, 0, numSamples);
            }
        }

        // Track peak for this track slot (L+R max)
        if (i < MAX_TRACKS)
        {
            float vPeak = 0.0f;
            for (int n = 0; n < numSamples; ++n)
                vPeak = juce::jmax (vPeak, std::abs (L[n]), std::abs (R[n]));
            if (vPeak > blockPeak[i]) blockPeak[i] = vPeak;
        }

        // Per-track reverb send
        if (p.reverbSend > 0.001f)
        {
            reverbBus.addFrom (0, 0, pb, 0, 0, numSamples, p.reverbSend);
            reverbBus.addFrom (1, 0, pb, 1, 0, numSamples, p.reverbSend);
        }

        // Per-track parallel comp send
        if (p.compSend > 0.001f)
        {
            compBus.addFrom (0, 0, pb, 0, 0, numSamples, p.compSend);
            compBus.addFrom (1, 0, pb, 1, 0, numSamples, p.compSend);
        }

        // Per-track tape saturation send
        if (p.satSend > 0.001f)
        {
            satBus.addFrom (0, 0, pb, 0, 0, numSamples, p.satSend);
            satBus.addFrom (1, 0, pb, 1, 0, numSamples, p.satSend);
        }
    }

    // Write raw per-block peak to atomics. UI timer handles hold/fall.
    for (int i = 0; i < MAX_TRACKS; ++i)
        trackPeakLin[i].store (blockPeak[i]);

    buffer.applyGain (0, numSamples, masterVol);

    // Process the reverb bus:
    //   1. Pre-delay (22 ms) — transient arrives dry, room follows
    //   2. HPF at 120 Hz     — removes muddy low-end buildup in the tail
    //   3. Freeverb           — large metal room
    //   4. High-shelf -4 dB  — smooths harsh upper tail
    reverbBus.applyGain (0, numSamples, masterVol);
    float* rL = reverbBus.getWritePointer (0);
    float* rR = reverbBus.getWritePointer (1);

    // 1. Pre-delay
    for (int n = 0; n < numSamples; ++n)
    {
        const int readPos = (preDelayWritePos - preDelaySamples + REVERB_PREDELAY_MAX) % REVERB_PREDELAY_MAX;
        const float dL = preDelayBuf.getSample (0, readPos);
        const float dR = preDelayBuf.getSample (1, readPos);
        preDelayBuf.setSample (0, preDelayWritePos, rL[n]);
        preDelayBuf.setSample (1, preDelayWritePos, rR[n]);
        preDelayWritePos = (preDelayWritePos + 1) % REVERB_PREDELAY_MAX;
        rL[n] = dL;
        rR[n] = dR;
    }

    // 2. HPF: kill low mud before Freeverb builds it up
    for (int n = 0; n < numSamples; ++n)
    {
        rL[n] = processBiquad (rL[n], rvbHpfCoeffs, rvbHpfStateL);
        rR[n] = processBiquad (rR[n], rvbHpfCoeffs, rvbHpfStateR);
    }

    // 3. Reverb
    reverb.processStereo (rL, rR, numSamples);

    // 4. High-shelf: tame harsh upper tail
    for (int n = 0; n < numSamples; ++n)
    {
        rL[n] = processBiquad (rL[n], rvbShelfCoeffs, rvbShelfStateL);
        rR[n] = processBiquad (rR[n], rvbShelfCoeffs, rvbShelfStateR);
    }

    // Room bus routing: mute / solo / gain / output bus selection
    const bool  roomMute   = (apvts.getRawParameterValue ("room_mute")->load()   > 0.5f);
    const bool  roomSolo   = (apvts.getRawParameterValue ("room_solo")->load()   > 0.5f);
    const int   roomOutput = juce::roundToInt (apvts.getRawParameterValue ("room_output")->load()); // 0-24
    const int   roomMode   = juce::roundToInt (apvts.getRawParameterValue ("room_output_mode")->load()); // 0=stereo, 1=mono
    const float roomGainDb = apvts.getRawParameterValue ("room_gain")->load();
    reverbBus.applyGain (juce::Decibels::decibelsToGain (roomGainDb));

    if (!bleedSolo && !roomMute)
    {
        if (roomSolo)
            buffer.clear();

        // Output 0 = Main (ch 0-1), Output N = DAW-assigned channel for bus N
        int chL         = busChannelMap[juce::jlimit (0, 15, roomOutput)];
        if (chL < 0)
            chL = busChannelMap[0]; // fallback to main out if selected bus is disabled
        const int chR   = (chL >= 0) ? chL + 1 : -1;
        const int numCh = buffer.getNumChannels();

        if (roomMode == 1) // Mono: sum L+R, write to both channels
        {
            for (int n = 0; n < numSamples; ++n)
            {
                const float m = (reverbBus.getSample (0, n) + reverbBus.getSample (1, n)) * 0.5f;
                reverbBus.setSample (0, n, m);
                reverbBus.setSample (1, n, m);
            }
            if (chL >= 0 && chL < numCh) buffer.addFrom (chL, 0, reverbBus, 0, 0, numSamples);
            if (chR >= 0 && chR < numCh) buffer.addFrom (chR, 0, reverbBus, 1, 0, numSamples);
        }
        else // Stereo
        {
            if (chL >= 0 && chL < numCh) buffer.addFrom (chL, 0, reverbBus, 0, 0, numSamples);
            if (chR >= 0 && chR < numCh) buffer.addFrom (chR, 0, reverbBus, 1, 0, numSamples);
        }
    }

    // Room bus peak measurement
    {
        const float rL = reverbBus.getMagnitude (0, 0, numSamples);
        const float rR = reverbBus.getMagnitude (1, 0, numSamples);
        const float rPeak = juce::jmax (rL, rR);
        roomPeakLin.store (rPeak);
    }

    reverbBus.clear (0, numSamples);

    // ── Parallel compression bus ──────────────────────────────────────────────
    // Metal "smash" parallel compression: 10:1, 5ms attack, 60ms release, hard knee.
    // The compressed bus is mixed back in alongside the dry signal (NY-style).
    {
        const bool  compMute   = (apvts.getRawParameterValue ("comp_mute")->load()        > 0.5f);
        const bool  compSolo   = (apvts.getRawParameterValue ("comp_solo")->load()        > 0.5f);
        const int   compOutput = juce::roundToInt (apvts.getRawParameterValue ("comp_output")->load());
        const int   compMode   = juce::roundToInt (apvts.getRawParameterValue ("comp_output_mode")->load());
        const float threshDb   = apvts.getRawParameterValue ("comp_threshold")->load();
        const float makeupDb   = apvts.getRawParameterValue ("comp_makeup")->load();

        // Apply master vol to the comp bus (matches loudness of main mix feed)
        compBus.applyGain (0, numSamples, masterVol);

        const float threshLin  = juce::Decibels::decibelsToGain (threshDb);
        const float makeupLin  = juce::Decibels::decibelsToGain (makeupDb);
        constexpr float ratio  = 10.0f; // 10:1 smash
        constexpr float invRat = 1.0f / ratio;

        float* cL = compBus.getWritePointer (0);
        float* cR = compBus.getWritePointer (1);

        float blockGrDb = 0.0f;  // track worst-case GR this block

        for (int n = 0; n < numSamples; ++n)
        {
            // Peak envelope follower (linked stereo)
            const float peak = juce::jmax (std::abs (cL[n]), std::abs (cR[n]));
            const float coef = (peak > compEnvL) ? compAttackCoef : compRelCoef;
            compEnvL += (peak - compEnvL) * coef;
            compEnvR = compEnvL;

            // Gain reduction: hard-knee feed-forward
            float gr = 1.0f;
            if (compEnvL > threshLin && compEnvL > 1e-9f)
            {
                const float inDb  = juce::Decibels::gainToDecibels (compEnvL);
                const float outDb = threshDb + (inDb - threshDb) * invRat;
                gr = juce::Decibels::decibelsToGain (outDb - inDb);
            }

            // Track max GR (most negative dB = most reduction)
            const float grDb = juce::Decibels::gainToDecibels (gr);
            if (grDb < blockGrDb) blockGrDb = grDb;

            cL[n] *= gr * makeupLin;
            cR[n] *= gr * makeupLin;
        }

        // Expose GR to UI (smooth toward new value)
        compGrDb.store (blockGrDb);

        if (!bleedSolo && !compMute)
        {
            if (compSolo)
                buffer.clear();

            int chL         = busChannelMap[juce::jlimit (0, 15, compOutput)];
            if (chL < 0)
                chL = busChannelMap[0]; // fallback to main out if selected bus is disabled
            const int chR   = (chL >= 0) ? chL + 1 : -1;
            const int numCh = buffer.getNumChannels();

            if (compMode == 1) // Mono
            {
                for (int n = 0; n < numSamples; ++n)
                {
                    const float m = (cL[n] + cR[n]) * 0.5f;
                    cL[n] = cR[n] = m;
                }
                if (chL >= 0 && chL < numCh) buffer.addFrom (chL, 0, compBus, 0, 0, numSamples);
                if (chR >= 0 && chR < numCh) buffer.addFrom (chR, 0, compBus, 1, 0, numSamples);
            }
            else // Stereo
            {
                if (chL >= 0 && chL < numCh) buffer.addFrom (chL, 0, compBus, 0, 0, numSamples);
                if (chR >= 0 && chR < numCh) buffer.addFrom (chR, 0, compBus, 1, 0, numSamples);
            }
        }

        // Comp bus peak measurement
        {
            const float cL = compBus.getMagnitude (0, 0, numSamples);
            const float cR = compBus.getMagnitude (1, 0, numSamples);
            const float cPeak = juce::jmax (cL, cR);
            compPeakLin.store (cPeak);
        }

        compBus.clear (0, numSamples);
    }

    // ── Tape saturation bus ───────────────────────────────────────────────────
    // Tanh waveshaper: asymmetric soft-clip with HF damping to emulate real tape.
    // Drive parameter (dB) converts to a linear gain fed into tanh; output is
    // normalised so 0 dBFS in → 0 dBFS out at any drive level (unity-gain when
    // sat_gain = 0 dB). A one-pole LPF mimics tape's HF rolloff at high drive.
    {
        const bool  satMute   = (apvts.getRawParameterValue ("sat_mute")->load()        > 0.5f);
        const bool  satSolo   = (apvts.getRawParameterValue ("sat_solo")->load()        > 0.5f);
        const int   satOutput = juce::roundToInt (apvts.getRawParameterValue ("sat_output")->load());
        const int   satMode   = juce::roundToInt (apvts.getRawParameterValue ("sat_output_mode")->load());
        const float driveDb   = apvts.getRawParameterValue ("sat_drive")->load();
        const float gainDb    = apvts.getRawParameterValue ("sat_gain")->load();

        const float driveLinear = juce::Decibels::decibelsToGain (driveDb);
        const float gainLin     = juce::Decibels::decibelsToGain (gainDb);
        // Normalisation factor: divide by tanh(drive) so unity-gain at low drive
        const float tanhDrive   = std::tanh (driveLinear);
        const float norm        = (tanhDrive > 1e-6f) ? 1.0f / tanhDrive : 1.0f;
        // HF damping coefficient: stronger at higher drive (tape loses top end when driven hard)
        // fc ≈ sampleRate / (2 * pi * drive * 400)  — empirically tuned for metal drums
        const float hfCoef = juce::jlimit (0.0f, 0.95f, 1.0f - (1.0f / (1.0f + driveLinear * 0.25f)));

        satBus.applyGain (0, numSamples, masterVol);

        float* sL = satBus.getWritePointer (0);
        float* sR = satBus.getWritePointer (1);

        for (int n = 0; n < numSamples; ++n)
        {
            // Tanh saturation (normalised)
            sL[n] = std::tanh (driveLinear * sL[n]) * norm;
            sR[n] = std::tanh (driveLinear * sR[n]) * norm;
            // One-pole HF rolloff (tape high-frequency loss at high drive)
            satHfStateL = satHfStateL * hfCoef + sL[n] * (1.0f - hfCoef);
            satHfStateR = satHfStateR * hfCoef + sR[n] * (1.0f - hfCoef);
            sL[n] = satHfStateL * gainLin;
            sR[n] = satHfStateR * gainLin;
        }

        if (!bleedSolo && !satMute)
        {
            if (satSolo)
                buffer.clear();

            int chL         = busChannelMap[juce::jlimit (0, 15, satOutput)];
            if (chL < 0)
                chL = busChannelMap[0]; // fallback to main out if selected bus is disabled
            const int chR   = (chL >= 0) ? chL + 1 : -1;
            const int numCh = buffer.getNumChannels();

            if (satMode == 1) // Mono
            {
                for (int n = 0; n < numSamples; ++n)
                {
                    const float m = (sL[n] + sR[n]) * 0.5f;
                    sL[n] = sR[n] = m;
                }
                if (chL >= 0 && chL < numCh) buffer.addFrom (chL, 0, satBus, 0, 0, numSamples);
                if (chR >= 0 && chR < numCh) buffer.addFrom (chR, 0, satBus, 1, 0, numSamples);
            }
            else // Stereo
            {
                if (chL >= 0 && chL < numCh) buffer.addFrom (chL, 0, satBus, 0, 0, numSamples);
                if (chR >= 0 && chR < numCh) buffer.addFrom (chR, 0, satBus, 1, 0, numSamples);
            }
        }

        // Sat bus peak measurement
        {
            const float sL = satBus.getMagnitude (0, 0, numSamples);
            const float sR = satBus.getMagnitude (1, 0, numSamples);
            const float sPeak = juce::jmax (sL, sR);
            satPeakLin.store (sPeak);
        }

        satBus.clear (0, numSamples);
    }

    // ── Master output peak ────────────────────────────────────────────────────
    {
        const int numCh = buffer.getNumChannels();
        float mPeak = 0.0f;
        for (int ch = 0; ch < numCh; ++ch)
            mPeak = juce::jmax (mPeak, buffer.getMagnitude (ch, 0, numSamples));
        masterPeakLin.store (mPeak);
    }
}

