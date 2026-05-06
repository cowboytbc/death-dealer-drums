#include "DrumPad.h"

#include <cmath>

//==============================================================================
DrumTrack::DrumTrack (const juce::String& nameIn, int /*midiNote*/)
    : name (nameIn)
{
    for (auto& t : lastTierVariation) t.store (-1);
}

//==============================================================================
bool DrumTrack::loadSampleAndGenerateVariations (const juce::File& file,
                                                  double targetSampleRate,
                                                  juce::AudioFormatManager& formatManager)
{
    auto reader = std::unique_ptr<juce::AudioFormatReader> (
        formatManager.createReaderFor (file));
    if (reader == nullptr) return false;

    const int numCh  = juce::jmin (2, (int) reader->numChannels);
    const int numSmp = (int) reader->lengthInSamples;
    if (numSmp <= 0) return false;

    juce::AudioBuffer<float> raw (numCh, numSmp);
    reader->read (&raw, 0, numSmp, 0, true, numCh > 1);

    // Ensure stereo
    if (raw.getNumChannels() == 1)
    {
        juce::AudioBuffer<float> st (2, numSmp);
        st.copyFrom (0, 0, raw, 0, 0, numSmp);
        st.copyFrom (1, 0, raw, 0, 0, numSmp);
        raw = std::move (st);
    }

    // Resample to target rate if needed
    juce::AudioBuffer<float> source;
    if (std::abs (reader->sampleRate - targetSampleRate) > 1.0)
    {
        const double ratio  = targetSampleRate / reader->sampleRate;
        const int    newLen = juce::jmax (1, (int) std::ceil (numSmp * ratio));
        juce::AudioBuffer<float> res (2, newLen);
        for (int ch = 0; ch < 2; ++ch)
        {
            const float* s = raw.getReadPointer (ch);
            float*       d = res.getWritePointer (ch);
            for (int i = 0; i < newLen; ++i)
            {
                const double idx = i / ratio;
                const int    i0  = (int) idx;
                const int    i1  = i0 + 1;
                const double f   = idx - i0;
                const float  s0  = (i0 < numSmp) ? s[i0] : 0.0f;
                const float  s1  = (i1 < numSmp) ? s[i1] : 0.0f;
                d[i] = (float) (s0 + f * (s1 - s0));
            }
        }
        source = std::move (res);
    }
    else
    {
        source = std::move (raw);
    }


    {
        juce::ScopedWriteLock wl (lock);
        generateVariations (source, targetSampleRate);
        hasCustomSample = true;
        sampleFilePath  = file.getFullPathName();
        // Store raw bytes so the sample can be embedded in preset files
        file.loadFileAsData (rawSampleData);
    }
    return true;
}

bool DrumTrack::loadSampleAndGenerateVariations (const void*                data,
                                                  int                        dataSizeBytes,
                                                  const juce::String&        displayPath,
                                                  double                     targetSampleRate,
                                                  juce::AudioFormatManager&  formatManager)
{
    auto stream  = std::make_unique<juce::MemoryInputStream> (data, (size_t) dataSizeBytes, false);
    auto reader  = std::unique_ptr<juce::AudioFormatReader> (
        formatManager.createReaderFor (std::move (stream)));
    if (reader == nullptr) return false;

    const int numCh  = juce::jmin (2, (int) reader->numChannels);
    const int numSmp = (int) reader->lengthInSamples;
    if (numSmp <= 0) return false;

    juce::AudioBuffer<float> raw (numCh, numSmp);
    reader->read (&raw, 0, numSmp, 0, true, numCh > 1);

    if (raw.getNumChannels() == 1)
    {
        juce::AudioBuffer<float> st (2, numSmp);
        st.copyFrom (0, 0, raw, 0, 0, numSmp);
        st.copyFrom (1, 0, raw, 0, 0, numSmp);
        raw = std::move (st);
    }

    juce::AudioBuffer<float> source;
    if (std::abs (reader->sampleRate - targetSampleRate) > 1.0)
    {
        const double ratio  = targetSampleRate / reader->sampleRate;
        const int    newLen = juce::jmax (1, (int) std::ceil (numSmp * ratio));
        juce::AudioBuffer<float> res (2, newLen);
        for (int ch = 0; ch < 2; ++ch)
        {
            const float* s = raw.getReadPointer (ch);
            float*       d = res.getWritePointer (ch);
            for (int i = 0; i < newLen; ++i)
            {
                const double idx = i / ratio;
                const int    i0  = (int) idx;
                const int    i1  = i0 + 1;
                const double f   = idx - i0;
                const float  s0  = (i0 < numSmp) ? s[i0] : 0.0f;
                const float  s1  = (i1 < numSmp) ? s[i1] : 0.0f;
                d[i] = (float) (s0 + f * (s1 - s0));
            }
        }
        source = std::move (res);
    }
    else
    {
        source = std::move (raw);
    }

    // Built-in CRASH 1 has intentional user-defined pre-roll removal.
    // Requested start point: 0:00:078 (78 ms).
    if (displayPath == "{builtin}/CRASH 1.wav")
    {
        const int offsetSamples = juce::jmax (0, (int) std::llround (targetSampleRate * 0.078));
        if (offsetSamples > 0 && offsetSamples < source.getNumSamples())
            source = trimStart (source, offsetSamples);
    }

    {
        juce::ScopedWriteLock wl (lock);
        generateVariations (source, targetSampleRate);
        hasCustomSample = true;
        sampleFilePath  = displayPath;
    }
    return true;
}

//==============================================================================
void DrumTrack::generateVariations (const juce::AudioBuffer<float>& source,
                                     double sampleRate)
{
    // 24 variations = 3 velocity tiers (SOFT/MID/HARD) × 8 round-robin slots.
    // All generated from the single source sample.
    // Tier offsets: SOFT=[0-7], MID=[8-15], HARD=[16-23]
    //
    // Tier shaping:
    //   SOFT  — quieter (-6 dB), rounded transient, darker HF, more body warmth
    //   MID   — neutral (0 dB), natural tonal variations (the original 8 recipes)
    //   HARD  — louder (+3 dB), sharpened transient, brighter HF, extra presence snap

    struct Recipe
    {
        double pitchSt, gainDb;
        double hiFreq, hiGain, loFreq, loGain;
        double pkFreq, pkGain, pkQ;
        double resFreq, resDecayMs, resAmp;
        double noiseMs, noiseAmp;
        int    noiseSeed;
        double noiseBand, noiseQ;
        double tMs, tScale;
        double dfDelayMs, dfOpenHz, dfCloseHz, dfSweepMs;
        double msWidth;
        double apFreqHz; int apStages;
    };

    // 8 base recipes — same as before (MID tier). SOFT and HARD are derived from these.
    static const Recipe R[VARS_PER_TIER] =
    {// pitchSt  gainDb  hiF    hiG    loF    loG    pkF    pkG   pkQ  resF  rDms  rAmp  noMs  noAmp seed  noBnd  noQ   tMs  tSc   dfDly  dfOpen  dfClose  dfSwp  msW    apF    apN
     {  0.000,  0.00,    0.0,  0.00,   0.0,  0.00,    0.0,  0.0, 1.0,   0.0,  0.0, 0.00, 0.0, 0.00, 0,    0.0, 1.0,  0.0, 1.00,  4.0, 20000.0,  9000.0, 120.0, 1.00,    0.0, 0 },
     { -0.050,  0.30, 6000.0, -0.80,  80.0,  1.50, 2500.0,  1.5, 1.4,   0.0,  0.0, 0.00, 0.0, 0.00, 0,    0.0, 1.0, 12.0, 1.00,  5.0, 20000.0,  9000.0, 120.0, 1.05,    0.0, 0 },
     {  0.060, -0.20, 8000.0,  2.50, 100.0, -1.00, 3500.0,  2.0, 1.6,   0.0,  0.0, 0.00, 3.0, 0.02, 2, 3500.0, 1.4,  7.0, 1.16,  4.0, 20000.0,  8000.0,  80.0, 0.95, 5000.0, 1 },
     { -0.080,  0.40, 5500.0, -1.50,  70.0,  2.00,  200.0,  2.5, 1.3,  80.0, 25.0, 0.02, 0.0, 0.00, 0,    0.0, 1.0, 14.0, 1.00,  6.0, 18000.0,  7000.0, 200.0, 1.12,  400.0, 1 },
     {  0.040, -0.30, 9500.0,  1.50, 120.0, -1.50, 4000.0,  3.0, 1.8,   0.0,  0.0, 0.00, 2.0, 0.025,4, 4000.0, 1.5,  5.0, 1.22,  3.0, 20000.0, 10000.0,  60.0, 0.90, 6000.0, 1 },
     {  0.070,  0.10, 7000.0,  1.00,  90.0,  0.80, 1500.0,  1.5, 1.2,  90.0, 38.0, 0.025,0.0, 0.00, 0,    0.0, 1.0, 10.0, 1.08,  8.0, 20000.0, 12000.0, 280.0, 1.08,  800.0, 1 },
     { -0.060, -0.20, 5000.0, -2.50, 150.0, -1.00, 2000.0, -1.5, 1.5,   0.0,  0.0, 0.00, 0.0, 0.00, 0,    0.0, 1.0, 18.0, 1.00,  3.0, 16000.0,  6000.0,  40.0, 0.85, 1200.0, 2 },
     {  0.080,  0.60, 9000.0,  3.00,  80.0,  1.80, 3000.0,  3.5, 1.6,   0.0,  0.0, 0.00, 2.5, 0.02, 7, 3500.0, 1.4,  6.0, 1.30,  5.0, 20000.0,  8000.0, 100.0, 1.00,    0.0, 0 },
    };

    // Tier gain offsets (dB): SOFT quieter, HARD louder
    static const double tierGainDb[NUM_VEL_TIERS] = { -6.0, 0.0, +3.0 };

    // Per-slot transient micro-variation (dB) — applied only to the preserved attack
    // region (first ~11 ms). Keeps sharpness intact; just nudges the punch slightly.
    static const double transientTrimDb[VARS_PER_TIER] =
    {  0.0, +1.2, -0.8, +0.6, -1.4, +1.0, -0.5, +1.5 };

    auto buildVariation = [&] (const Recipe& r, double extraGainDb,
                                double transientMult, double hfExtra,
                                float transientBlend) -> juce::AudioBuffer<float>
    {
        juce::AudioBuffer<float> buf = pitchShiftOLA (source, r.pitchSt);

        // Restore original attack transient after OLA smearing.
        // SOFT tier: skip (ghost notes should stay soft). MID/HARD: full blend.
        if (transientBlend > 0.0f && r.pitchSt != 0.0)
        {
            const int blendLen = juce::jmin (buf.getNumSamples(), 512);
            for (int ch = 0; ch < buf.getNumChannels(); ++ch)
            {
                const float* s = source.getReadPointer (ch < source.getNumChannels() ? ch : 0);
                float*       d = buf.getWritePointer (ch);
                for (int i = 0; i < blendLen; ++i)
                {
                    const float t = (float) i / (float) blendLen;
                    d[i] = s[i] * transientBlend * (1.0f - t) + d[i] * t;
                }
            }
        }

        if (r.hiFreq > 0.0 && std::abs (r.hiGain + hfExtra) > 0.01)
            applyHighShelf (buf, sampleRate, r.hiFreq, r.hiGain + hfExtra);
        else if (std::abs (hfExtra) > 0.01)
            applyHighShelf (buf, sampleRate, 8000.0, hfExtra);

        if (r.loFreq > 0.0 && std::abs (r.loGain) > 0.01)
            applyLowShelf (buf, sampleRate, r.loFreq, r.loGain);
        if (r.pkFreq > 0.0 && std::abs (r.pkGain) > 0.01)
            applyPeakEQ (buf, sampleRate, r.pkFreq, r.pkGain, r.pkQ);

        const double effTScale = 1.0 + (r.tScale - 1.0) * transientMult;
        if (r.tMs > 0.0 && std::abs (effTScale - 1.0) > 0.01)
            scaleTransient (buf, sampleRate, r.tMs, effTScale);
        if (r.resAmp > 0.0)
            addModalResonance (buf, sampleRate, r.resFreq, r.resDecayMs, (float) r.resAmp);
        if (r.noiseAmp > 0.0)
            addTransientNoise (buf, sampleRate, r.noiseMs, (float) r.noiseAmp,
                               r.noiseSeed, r.noiseBand, r.noiseQ);
        if (r.dfOpenHz > 0.0)
            applyDecayFilter (buf, sampleRate, r.dfDelayMs,
                              r.dfOpenHz, r.dfCloseHz, r.dfSweepMs);
        if (std::abs (r.msWidth - 1.0) > 0.01)
            applyMSWidth (buf, r.msWidth);
        if (r.apFreqHz > 0.0 && r.apStages > 0)
            applyAllPass (buf, sampleRate, r.apFreqHz, r.apStages);

        applyGainDb (buf, r.gainDb + extraGainDb);

        // Short fade-in (0.3 ms) to prevent DC-offset clicks without killing the transient
        const int fadeLen = juce::jmin (buf.getNumSamples(), (int)(sampleRate * 0.0003));
        for (int ch = 0; ch < buf.getNumChannels(); ++ch)
        {
            float* d = buf.getWritePointer (ch);
            for (int s = 0; s < fadeLen; ++s)
                d[s] *= (float)s / (float)fadeLen;
        }
        return buf;
    };

    for (int tier = 0; tier < NUM_VEL_TIERS; ++tier)
    {
        // SOFT: rounded transient (0.5×), darker (-2 dB HF)
        // MID:  neutral
        // HARD: sharper transient (1.5×), brighter (+2 dB HF)
        const double transientMult  = (tier == 0) ? 0.5  : (tier == 2) ? 1.5  : 1.0;
        const double hfExtra        = (tier == 0) ? -2.0 : (tier == 2) ? 2.0  : 0.0;
        // Velocity-continuous transient blend: SOFT=0 (ghost notes stay soft),
        // MID=0.6 (partial), HARD=1.0 (full sharp attack). Matches real drum physics
        // where harder hits produce a sharper, more prominent stick crack.
        const float  transientBlend = (tier == 0) ? 0.0f : (tier == 1) ? 0.6f : 1.0f;

        for (int slot = 0; slot < VARS_PER_TIER; ++slot)
        {
            const int idx = tier * VARS_PER_TIER + slot;
            auto buf = buildVariation (R[slot], tierGainDb[tier], transientMult, hfExtra, transientBlend);

            // Apply per-slot transient micro-variation to the preserved attack region only.
            // Uniform scalar over first H samples — no shape change, no softening.
            {
                const float trimGain = (float) std::pow (10.0, transientTrimDb[slot] / 20.0);
                const int   tLen     = juce::jmin (buf.getNumSamples(), 512);
                for (int ch = 0; ch < buf.getNumChannels(); ++ch)
                {
                    float* d = buf.getWritePointer (ch);
                    for (int s = 0; s < tLen; ++s)
                        d[s] *= trimGain;
                }
            }

            variations[idx].buffer     = std::move (buf);
            variations[idx].sampleRate = sampleRate;
            variations[idx].valid      = true;
        }
    }
}

//==============================================================================
void DrumTrack::setSampleMode (SampleMode m, int numVars)
{
    sampleMode  = m;
    numVariants = juce::jlimit (1, VARS_PER_TIER, numVars);
    resetRoundRobin();
}

const DrumVariation* DrumTrack::getVariation (int index) const
{
    juce::ScopedReadLock rl (lock);
    if (index < 0 || index >= NUM_VARIATIONS) return nullptr;
    return variations[index].valid ? &variations[index] : nullptr;
}

const DrumVariation* DrumTrack::getVariationForTier (int tier, int slot) const
{
    juce::ScopedReadLock rl (lock);
    if (tier < 0 || tier >= NUM_VEL_TIERS || slot < 0 || slot >= VARS_PER_TIER) return nullptr;
    const int idx = tier * VARS_PER_TIER + slot;
    return variations[idx].valid ? &variations[idx] : nullptr;
}

//==============================================================================
const DrumVariation* DrumTrack::getNextVariationForVelocity (float velocity)
{
    juce::ScopedReadLock rl (lock);

    if (sampleMode == SampleMode::Single)
    {
        // Single mode: always use MID tier slot 0 (index 8)
        const int idx = 1 * VARS_PER_TIER;
        return variations[idx].valid ? &variations[idx] : nullptr;
    }

    // Determine tier from velocity: 0-0.33=SOFT, 0.34-0.66=MID, 0.67-1.0=HARD
    const int tier = (velocity < 0.34f) ? 0 : (velocity < 0.67f) ? 1 : 2;
    const int base = tier * VARS_PER_TIER;

    // Per-tier round-robin: never repeat the same slot
    const int last = lastTierVariation[tier].load();
    int pick = rng.nextInt (VARS_PER_TIER);
    if (pick == last)
        pick = (pick + 1) % VARS_PER_TIER;

    for (int a = 0; a < VARS_PER_TIER; ++a)
    {
        const int slot = (pick + a) % VARS_PER_TIER;
        if (variations[base + slot].valid)
        {
            lastTierVariation[tier].store (slot);
            lastUsedTier.store (tier);
            lastUsedSlot.store (slot);
            return &variations[base + slot];
        }
    }
    return nullptr;
}

//==============================================================================
const DrumVariation* DrumTrack::getNextVariation ()
{
    // Legacy path — used by pad preview triggers that don't pass velocity
    return getNextVariationForVelocity (0.7f); // default to MID/HARD boundary
}

//==============================================================================
juce::AudioBuffer<float> DrumTrack::pitchShift (const juce::AudioBuffer<float>& src,
                                                  double cents)
{
    if (src.getNumSamples() == 0) return {};
    const double ratio    = std::pow (2.0, cents / 1200.0);
    const int    numCh    = src.getNumChannels();
    const int    srcSamps = src.getNumSamples();
    const int    dstSamps = juce::jmax (1, (int) std::ceil (srcSamps / ratio));

    juce::AudioBuffer<float> dst (numCh, dstSamps);
    for (int ch = 0; ch < numCh; ++ch)
    {
        const float* s = src.getReadPointer (ch);
        float*       d = dst.getWritePointer (ch);
        for (int i = 0; i < dstSamps; ++i)
        {
            const double si = i * ratio;
            const int    i0 = (int) si;
            const int    i1 = i0 + 1;
            const double f  = si - i0;
            const float  s0 = (i0 < srcSamps) ? s[i0] : 0.0f;
            const float  s1 = (i1 < srcSamps) ? s[i1] : 0.0f;
            d[i] = (float) (s0 + f * (s1 - s0));
        }
    }
    return dst;
}

//==============================================================================
void DrumTrack::applyHighShelf (juce::AudioBuffer<float>& buf,
                                  double fs, double freqHz, double gainDb)
{
    // Audio EQ Cookbook high shelf, S = 1
    const double A  = std::pow (10.0, gainDb / 40.0);
    const double w0 = 2.0 * juce::MathConstants<double>::pi * freqHz / fs;
    const double cw = std::cos (w0);
    const double sw = std::sin (w0);
    const double al = sw / std::sqrt (2.0);
    const double sq = std::sqrt (A);
    const double b0 =  A  * ((A+1) + (A-1)*cw + 2.0*sq*al);
    const double b1 = -2.0*A * ((A-1) + (A+1)*cw);
    const double b2 =  A  * ((A+1) + (A-1)*cw - 2.0*sq*al);
    const double a0 =       (A+1) - (A-1)*cw + 2.0*sq*al;
    const double a1 =  2.0 * ((A-1) - (A+1)*cw);
    const double a2 =       (A+1) - (A-1)*cw - 2.0*sq*al;
    const double B0 = b0/a0, B1 = b1/a0, B2 = b2/a0;
    const double A1 = a1/a0, A2 = a2/a0;

    for (int ch = 0; ch < buf.getNumChannels(); ++ch)
    {
        float* d = buf.getWritePointer (ch);
        double x1=0, x2=0, y1=0, y2=0;
        for (int i = 0; i < buf.getNumSamples(); ++i)
        {
            const double x0 = d[i];
            const double y0 = B0*x0 + B1*x1 + B2*x2 - A1*y1 - A2*y2;
            d[i] = (float) y0;
            x2 = x1; x1 = x0; y2 = y1; y1 = y0;
        }
    }
}

//==============================================================================
void DrumTrack::applyLowShelf (juce::AudioBuffer<float>& buf,
                                 double fs, double freqHz, double gainDb)
{
    // Audio EQ Cookbook low shelf, S = 1
    const double A  = std::pow (10.0, gainDb / 40.0);
    const double w0 = 2.0 * juce::MathConstants<double>::pi * freqHz / fs;
    const double cw = std::cos (w0);
    const double sw = std::sin (w0);
    const double al = sw / std::sqrt (2.0);
    const double sq = std::sqrt (A);
    const double b0 =  A  * ((A+1) - (A-1)*cw + 2.0*sq*al);
    const double b1 =  2.0*A * ((A-1) - (A+1)*cw);
    const double b2 =  A  * ((A+1) - (A-1)*cw - 2.0*sq*al);
    const double a0 =       (A+1) + (A-1)*cw + 2.0*sq*al;
    const double a1 = -2.0 * ((A-1) + (A+1)*cw);
    const double a2 =       (A+1) + (A-1)*cw - 2.0*sq*al;
    const double B0 = b0/a0, B1 = b1/a0, B2 = b2/a0;
    const double A1 = a1/a0, A2 = a2/a0;

    for (int ch = 0; ch < buf.getNumChannels(); ++ch)
    {
        float* d = buf.getWritePointer (ch);
        double x1=0, x2=0, y1=0, y2=0;
        for (int i = 0; i < buf.getNumSamples(); ++i)
        {
            const double x0 = d[i];
            const double y0 = B0*x0 + B1*x1 + B2*x2 - A1*y1 - A2*y2;
            d[i] = (float) y0;
            x2 = x1; x1 = x0; y2 = y1; y1 = y0;
        }
    }
}

//==============================================================================
void DrumTrack::scaleTransient (juce::AudioBuffer<float>& buf,
                                  double fs, double durationMs, double gainFactor)
{
    // Apply a gain ramp over the attack region: starts at gainFactor, tapers to 1.0
    // This makes hits feel punchier (>1) or softer/thuddy (<1)
    const int tSamples = juce::jmin ((int) (durationMs * 0.001 * fs),
                                     buf.getNumSamples());
    if (tSamples <= 0) return;

    for (int ch = 0; ch < buf.getNumChannels(); ++ch)
    {
        float* d = buf.getWritePointer (ch);
        for (int i = 0; i < tSamples; ++i)
        {
            const double t    = (double) i / tSamples;          // 0 -> 1
            const double gain = gainFactor + (1.0 - gainFactor) * t; // gainFactor -> 1.0
            d[i] *= (float) gain;
        }
    }
}

//==============================================================================
void DrumTrack::applySaturation (juce::AudioBuffer<float>& buf, double amount)
{
    // Soft-clip via tanh with makeup gain; amount 0-1, higher = more harmonic drive
    if (amount <= 0.0) return;
    const double drive  = 1.0 + amount * 4.0;   // 1x..5x drive
    const double makeup = 1.0 / std::tanh (drive); // keep peak level consistent

    for (int ch = 0; ch < buf.getNumChannels(); ++ch)
    {
        float* d = buf.getWritePointer (ch);
        for (int i = 0; i < buf.getNumSamples(); ++i)
            d[i] = (float) (std::tanh (drive * (double) d[i]) * makeup);
    }
}

//==============================================================================
void DrumTrack::applyGainDb (juce::AudioBuffer<float>& buf, double dB)
{
    const float g = (float) std::pow (10.0, dB / 20.0);
    for (int ch = 0; ch < buf.getNumChannels(); ++ch)
        juce::FloatVectorOperations::multiply (buf.getWritePointer (ch), g, buf.getNumSamples());
}

//==============================================================================
juce::AudioBuffer<float> DrumTrack::trimStart (const juce::AudioBuffer<float>& src,
                                                 int offsetSamples)
{
    const int n = juce::jmax (0, src.getNumSamples() - offsetSamples);
    juce::AudioBuffer<float> dst (src.getNumChannels(), n);
    for (int ch = 0; ch < src.getNumChannels(); ++ch)
        dst.copyFrom (ch, 0, src, ch, offsetSamples, n);
    return dst;
}

//==============================================================================
// OLA overlap-add pitch shift — changes pitch without changing duration.
// Analysis hop Ha = H*ratio advances through source faster (pitch up) or
// slower (pitch down) while synthesis hop stays fixed at H.
juce::AudioBuffer<float> DrumTrack::pitchShiftOLA (const juce::AudioBuffer<float>& src,
                                                     double semitones)
{
    const int srcLen = src.getNumSamples();
    const int numCh  = src.getNumChannels();
    juce::AudioBuffer<float> dst (numCh, juce::jmax (1, srcLen));
    dst.clear();

    if (srcLen == 0) return dst;
    if (std::abs (semitones) < 0.005)
    {
        for (int ch = 0; ch < numCh; ++ch)
            dst.copyFrom (ch, 0, src, ch, 0, srcLen);
        return dst;
    }

    // We'll blend the original source transient back in after OLA (see below)

    const double ratio = std::pow (2.0, semitones / 12.0);
    const int    G     = 1024;       // grain size (~23 ms @ 44.1 kHz)
    const int    H     = G / 2;      // synthesis hop — 50 % overlap
    const double Ha    = H * ratio;  // analysis hop

    std::vector<float> win (G);
    for (int i = 0; i < G; ++i)
        win[i] = (float)(0.5 - 0.5 * std::cos (
                     2.0 * juce::MathConstants<double>::pi * i / G));

    for (int ch = 0; ch < numCh; ++ch)
    {
        const float* s = src.getReadPointer (ch);
        float*       d = dst.getWritePointer (ch);
        double analysisPos  = 0.0;
        int    synthesisPos = 0;

        while (synthesisPos < srcLen)
        {
            for (int i = 0; i < G; ++i)
            {
                const int outIdx = synthesisPos + i;
                if (outIdx >= srcLen) break;

                const double si  = analysisPos + i;
                const int    i0  = (int) si;
                if (i0 >= srcLen) break;

                const double frac = si - i0;
                const float  s0   = s[i0];
                const float  s1   = (i0 + 1 < srcLen) ? s[i0 + 1] : 0.0f;
                d[outIdx] += (s0 + (float)(frac * (s1 - s0))) * win[i];
            }
            synthesisPos += H;
            analysisPos  += Ha;
        }
    }

    // Preserve attack transient: blend original source over the first half-grain (H samples).
    // Moved into buildVariation (tier-aware). When semitones==0 the copy path is used so
    // the blend is a no-op; the caller applies it with the correct weight.
    return dst;
}

//==============================================================================
void DrumTrack::applyPeakEQ (juce::AudioBuffer<float>& buf,
                               double sr, double freqHz, double gainDb, double Q)
{
    // Audio EQ Cookbook peaking EQ
    const double A  = std::pow (10.0, gainDb / 40.0);
    const double w0 = 2.0 * juce::MathConstants<double>::pi * freqHz / sr;
    const double cw = std::cos (w0);
    const double al = std::sin (w0) / (2.0 * Q);
    const double b0 =  1.0 + al * A;
    const double b1 = -2.0 * cw;
    const double b2 =  1.0 - al * A;
    const double a0 =  1.0 + al / A;
    const double a1 = -2.0 * cw;
    const double a2 =  1.0 - al / A;
    const double B0 = b0/a0, B1 = b1/a0, B2 = b2/a0;
    const double A1 = a1/a0, A2 = a2/a0;

    for (int ch = 0; ch < buf.getNumChannels(); ++ch)
    {
        float* d = buf.getWritePointer (ch);
        double x1=0, x2=0, y1=0, y2=0;
        for (int i = 0; i < buf.getNumSamples(); ++i)
        {
            const double x0 = d[i];
            const double y0 = B0*x0 + B1*x1 + B2*x2 - A1*y1 - A2*y2;
            d[i] = (float) y0;
            x2=x1; x1=x0; y2=y1; y1=y0;
        }
    }
}

//==============================================================================
void DrumTrack::addModalResonance (juce::AudioBuffer<float>& buf,
                                    double sr, double freqHz,
                                    double decayMs, float amplitude)
{
    // Inject a decaying sinusoid to simulate a drum resonance mode being excited.
    // Energy drops ~60 dB over decayMs milliseconds.
    const double decayCoeff = std::exp (-6.908 / (decayMs * 0.001 * sr));
    const double angFreq    = 2.0 * juce::MathConstants<double>::pi * freqHz / sr;
    const int    nSamples   = juce::jmin (buf.getNumSamples(),
                                          (int)(decayMs * 0.001 * sr * 5.0));

    for (int ch = 0; ch < buf.getNumChannels(); ++ch)
    {
        float* d   = buf.getWritePointer (ch);
        double env = (double) amplitude;
        for (int i = 0; i < nSamples; ++i)
        {
            d[i] += (float)(env * std::sin (angFreq * i));
            env  *= decayCoeff;
        }
    }
}

//==============================================================================
void DrumTrack::addTransientNoise (juce::AudioBuffer<float>& buf,
                                    double sr, double durationMs,
                                    float amplitude, int seed,
                                    double bandHz, double Q)
{
    // Fixed-seed band-limited noise burst at the attack — reproducible stick
    // contact character.  Seed ensures same file always produces same variations.
    const int nSamples = juce::jmin (buf.getNumSamples(),
                                     (int)(durationMs * 0.001 * sr));
    if (nSamples <= 0) return;

    juce::Random rng ((juce::int64)(seed + 1) * 999983LL);
    const double decayCoeff = std::exp (-4.0 / nSamples);

    juce::AudioBuffer<float> noise (buf.getNumChannels(), nSamples);
    for (int ch = 0; ch < buf.getNumChannels(); ++ch)
    {
        float* n = noise.getWritePointer (ch);
        double env = (double) amplitude;
        for (int i = 0; i < nSamples; ++i)
        {
            n[i]  = (float)(env * (rng.nextFloat() * 2.0f - 1.0f));
            env  *= decayCoeff;
        }
    }
    applyPeakEQ (noise, sr, bandHz, 10.0, Q);

    for (int ch = 0; ch < buf.getNumChannels(); ++ch)
        juce::FloatVectorOperations::add (buf.getWritePointer (ch),
                                          noise.getReadPointer (ch),
                                          nSamples);
}

//==============================================================================
void DrumTrack::applyDecayFilter (juce::AudioBuffer<float>& buf,
                                   double sr,
                                   double delayMs,  double openHz,
                                   double closeHz,  double sweepMs)
{
    // One-pole LPF whose cutoff sweeps from openHz → closeHz after delayMs.
    // Models the drum head / shell absorbing highs as the hit decays.
    // Dead hit = fast narrow sweep; open/ringy hit = slow, nearly open.
    const int delaySamples = (int) (delayMs * 0.001 * sr);
    const int sweepSamples = juce::jmax (1, (int) (sweepMs * 0.001 * sr));
    const int n            = buf.getNumSamples();

    auto calcG = [&] (double hz) -> double {
        const double w = 2.0 * juce::MathConstants<double>::pi
                         * juce::jlimit (1.0, sr * 0.499, hz) / sr;
        const double c = std::cos (w);
        return 2.0 - c - std::sqrt ((2.0 - c) * (2.0 - c) - 1.0);
    };
    const double g0 = calcG (openHz);
    const double g1 = calcG (closeHz);

    for (int ch = 0; ch < buf.getNumChannels(); ++ch)
    {
        float*  d = buf.getWritePointer (ch);
        double  z = 0.0;
        for (int i = 0; i < n; ++i)
        {
            double g;
            if (i <= delaySamples)
                g = g0;
            else if (i < delaySamples + sweepSamples)
                g = g0 + (g1 - g0) * (double)(i - delaySamples) / sweepSamples;
            else
                g = g1;
            z    = z * (1.0 - g) + (double) d[i] * g;
            d[i] = (float) z;
        }
    }
}

//==============================================================================
void DrumTrack::applyMSWidth (juce::AudioBuffer<float>& buf, double widthFactor)
{
    // Scale the side channel to widen (>1) or narrow (<1) the stereo image.
    if (buf.getNumChannels() < 2) return;
    float* L = buf.getWritePointer (0);
    float* R = buf.getWritePointer (1);
    const float sf = (float) widthFactor;
    for (int i = 0; i < buf.getNumSamples(); ++i)
    {
        const float m = (L[i] + R[i]) * 0.5f;
        const float s = (L[i] - R[i]) * 0.5f * sf;
        L[i] = m + s;
        R[i] = m - s;
    }
}

//==============================================================================
void DrumTrack::applyAllPass (juce::AudioBuffer<float>& buf,
                               double sr, double freqHz, int numStages)
{
    // First-order all-pass: y[n] = -g·x[n] + x[n-1] + g·y[n-1]
    // Phase is rotated 180° at freqHz, spreading the timing of harmonics.
    // Doesn't change loudness or EQ — only changes the "character" of the
    // attack and decay by shifting phase relationships between partials.
    const double t  = std::tan (juce::MathConstants<double>::pi * freqHz / sr);
    const double g  = (t - 1.0) / (t + 1.0);

    for (int ch = 0; ch < buf.getNumChannels(); ++ch)
    {
        float* d = buf.getWritePointer (ch);
        for (int stage = 0; stage < numStages; ++stage)
        {
            double x1 = 0.0, y1 = 0.0;
            for (int i = 0; i < buf.getNumSamples(); ++i)
            {
                const double x0 = d[i];
                const double y0 = -g * x0 + x1 + g * y1;
                d[i] = (float) y0;
                x1 = x0;  y1 = y0;
            }
        }
    }
}
