#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "DrumEngine.h"

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout
DeathDealerDrumsAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // Pre-register params for all 32 track slots
    for (int i = 0; i < MAX_TRACKS; ++i)
    {
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { trackParamID (i, "volume"),  1 }, trackParamID (i, "volume"),
            juce::NormalisableRange<float> (0.0f, 2.0f, 0.01f), 1.0f));

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { trackParamID (i, "pan"),     1 }, trackParamID (i, "pan"),
            juce::NormalisableRange<float> (-1.0f, 1.0f, 0.01f), 0.0f));

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { trackParamID (i, "tune"),    1 }, trackParamID (i, "tune"),
            juce::NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f));

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { trackParamID (i, "decay"),   1 }, trackParamID (i, "decay"),
            juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 1.0f));

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { trackParamID (i, "attack"),  1 }, trackParamID (i, "attack"),
            juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.0f));

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { trackParamID (i, "eq_low"),  1 }, trackParamID (i, "eq_low"),
            juce::NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f));

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { trackParamID (i, "eq_mid"),  1 }, trackParamID (i, "eq_mid"),
            juce::NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f));

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { trackParamID (i, "eq_high"), 1 }, trackParamID (i, "eq_high"),
            juce::NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f));

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { trackParamID (i, "midi_note"), 1 }, trackParamID (i, "midi_note"),
            juce::NormalisableRange<float> (0.0f, 127.0f, 1.0f), 60.0f));

        // Slave-link target for same-note layering.
        // -1 = free (no slave), otherwise 0..MAX_TRACKS-1 = follow that track's hit context.
        layout.add (std::make_unique<juce::AudioParameterInt> (
            juce::ParameterID { trackParamID (i, "slave_to"), 1 }, trackParamID (i, "slave_to"),
            -1, MAX_TRACKS - 1, -1));

        // Per-track sample trim (normalized 0..1 over the loaded sample)
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { trackParamID (i, "sample_start"), 1 }, trackParamID (i, "sample_start"),
            juce::NormalisableRange<float> (0.0f, 1.0f, 0.0001f), 0.0f));

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { trackParamID (i, "sample_end"), 1 }, trackParamID (i, "sample_end"),
            juce::NormalisableRange<float> (0.0f, 1.0f, 0.0001f), 1.0f));

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { trackParamID (i, "reverb_send"), 1 }, trackParamID (i, "reverb_send"),
            juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.0f));

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { trackParamID (i, "comp_send"), 1 }, trackParamID (i, "comp_send"),
            juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.0f));
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { trackParamID (i, "sat_send"), 1 }, trackParamID (i, "sat_send"),
            juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.0f));

        layout.add (std::make_unique<juce::AudioParameterBool> (
            juce::ParameterID { trackParamID (i, "choke"), 1 }, trackParamID (i, "choke"), false));

        layout.add (std::make_unique<juce::AudioParameterBool> (
            juce::ParameterID { trackParamID (i, "mute"), 1 }, trackParamID (i, "mute"), false));
        layout.add (std::make_unique<juce::AudioParameterBool> (
            juce::ParameterID { trackParamID (i, "solo"), 1 }, trackParamID (i, "solo"), false));
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { trackParamID (i, "output"), 1 }, trackParamID (i, "output"),
            juce::NormalisableRange<float> (0.0f, 15.0f, 1.0f), 0.0f));
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { trackParamID (i, "output_mode"), 1 }, trackParamID (i, "output_mode"),
            juce::NormalisableRange<float> (0.0f, 1.0f, 1.0f), 0.0f));
        layout.add (std::make_unique<juce::AudioParameterBool> (
            juce::ParameterID { trackParamID (i, "phase"), 1 }, trackParamID (i, "phase"), false));

        // ── Per-track 8-band parametric EQ ──────────────────────────────────
        // Bands: 0=HPF  1=LowShelf  2-5=Peak  6=HighShelf  7=LPF
        {
            static const float defFreq[8] = { 20.f, 80.f, 200.f, 800.f, 2500.f, 5000.f, 10000.f, 22000.f };
            static const float defQ   [8] = { 0.707f, 0.707f, 1.0f, 1.0f, 1.0f, 1.0f, 0.707f, 0.707f };
            for (int b = 0; b < 8; ++b)
            {
                const juce::String bTag = "eq8_b" + juce::String (b);
                layout.add (std::make_unique<juce::AudioParameterFloat> (
                    juce::ParameterID { trackParamID (i, bTag + "_freq"), 1 },
                    trackParamID (i, bTag + "_freq"),
                    juce::NormalisableRange<float> (20.f, 22000.f, 0.f, 0.3f), defFreq[b]));
                layout.add (std::make_unique<juce::AudioParameterFloat> (
                    juce::ParameterID { trackParamID (i, bTag + "_gain"), 1 },
                    trackParamID (i, bTag + "_gain"),
                    juce::NormalisableRange<float> (-18.f, 18.f, 0.1f), 0.0f));
                layout.add (std::make_unique<juce::AudioParameterFloat> (
                    juce::ParameterID { trackParamID (i, bTag + "_q"), 1 },
                    trackParamID (i, bTag + "_q"),
                    juce::NormalisableRange<float> (0.1f, 10.0f, 0.01f, 0.5f), defQ[b]));
            }
        }
        // ── Per-track EQ on/off ──────────────────────────────────────────────
        layout.add (std::make_unique<juce::AudioParameterBool> (
            juce::ParameterID { trackParamID (i, "eq8_on"), 1 },
            trackParamID (i, "eq8_on"), true));   // on by default
        // ── Per-track compressor ─────────────────────────────────────────────
        layout.add (std::make_unique<juce::AudioParameterBool> (
            juce::ParameterID { trackParamID (i, "trk_comp_on"),  1 }, trackParamID (i, "trk_comp_on"),  false));
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { trackParamID (i, "trk_comp_thr"), 1 }, trackParamID (i, "trk_comp_thr"),
            juce::NormalisableRange<float> (-60.f, 0.f, 0.1f), -20.f));
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { trackParamID (i, "trk_comp_rat"), 1 }, trackParamID (i, "trk_comp_rat"),
            juce::NormalisableRange<float> (1.f, 20.f, 0.1f), 4.f));
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { trackParamID (i, "trk_comp_atk"), 1 }, trackParamID (i, "trk_comp_atk"),
            juce::NormalisableRange<float> (0.1f, 100.f, 0.1f), 5.f));
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { trackParamID (i, "trk_comp_rel"), 1 }, trackParamID (i, "trk_comp_rel"),
            juce::NormalisableRange<float> (10.f, 1000.f, 0.1f), 60.f));
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { trackParamID (i, "trk_comp_mkp"), 1 }, trackParamID (i, "trk_comp_mkp"),
            juce::NormalisableRange<float> (0.f, 24.f, 0.1f), 0.f));
        // Per-track mic-bleed enable (default off)
        layout.add (std::make_unique<juce::AudioParameterBool> (
            juce::ParameterID { trackParamID (i, "bleed_enable"), 1 },
            trackParamID (i, "bleed_enable"), false));
        // Per-track bleed send level (0=silent, 1=full contribution to bus)
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { trackParamID (i, "bleed_send"), 1 },
            trackParamID (i, "bleed_send"),
            juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 1.0f));
    }

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { PARAM_MASTER_VOLUME, 1 }, PARAM_MASTER_VOLUME,
        juce::NormalisableRange<float> (0.0f, 1.5f, 0.01f), 1.0f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { PARAM_BLEED_AMOUNT, 1 }, PARAM_BLEED_AMOUNT,
        juce::NormalisableRange<float> (0.0f, 10.0f, 0.1f), 0.0f));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { PARAM_BLEED_SOLO, 1 }, PARAM_BLEED_SOLO, false));

    // Global humanization amount for per-hit velocity scatter.
    // 0.05 = ±5% (current behavior, default/min), 0.20 = ±20%.
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { PARAM_HUMAN_ERROR, 1 }, PARAM_HUMAN_ERROR,
        juce::NormalisableRange<float> (0.05f, 0.20f, 0.001f), 0.05f));

    // Room bus global controls
    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "room_mute",   1 }, "room_mute",   false));
    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "room_solo",   1 }, "room_solo",   false));
    layout.add (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { "room_output", 1 }, "room_output", 0, 15, 0));
    layout.add (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { "room_output_mode", 1 }, "room_output_mode", 0, 1, 0)); // 0=Stereo 1=Mono
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "room_gain",   1 }, "room_gain",
        juce::NormalisableRange<float> (-24.0f, 6.0f, 0.1f), 0.0f));

    // Parallel compression bus
    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "comp_mute",       1 }, "comp_mute",       false));
    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "comp_solo",       1 }, "comp_solo",       false));
    layout.add (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { "comp_output",     1 }, "comp_output",     0, 15, 0));
    layout.add (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { "comp_output_mode",1 }, "comp_output_mode",0,  1, 0)); // 0=Stereo 1=Mono
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "comp_threshold",  1 }, "comp_threshold",
        juce::NormalisableRange<float> (-60.0f, 0.0f, 0.1f), -20.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "comp_makeup",     1 }, "comp_makeup",
        juce::NormalisableRange<float> (0.0f, 30.0f, 0.1f), 10.0f));

    // Tape saturation bus
    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "sat_mute",       1 }, "sat_mute",       false));
    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "sat_solo",       1 }, "sat_solo",       false));
    layout.add (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { "sat_output",     1 }, "sat_output",     0, 15, 0));
    layout.add (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { "sat_output_mode",1 }, "sat_output_mode",0,  1, 0));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "sat_drive",      1 }, "sat_drive",
        juce::NormalisableRange<float> (0.0f, 24.0f, 0.1f), 6.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "sat_gain",       1 }, "sat_gain",
        juce::NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f));

    return layout;
}

//==============================================================================
DeathDealerDrumsAudioProcessor::DeathDealerDrumsAudioProcessor()
    : AudioProcessor ([&]
      {
          auto p = BusesProperties()
                       .withOutput ("Main Output", juce::AudioChannelSet::stereo(), true);
          for (int i = 2; i <= 16; ++i)
          {
              p = p.withOutput ("OUT " + juce::String (i),
                                juce::AudioChannelSet::stereo(), false);
          }
          return p;
      }()),
      engine (std::make_unique<DrumEngine>()),
      apvts  (*this, nullptr, "DDDParams", createParameterLayout())
{
    for (int i = 0; i < MAX_TRACKS; ++i)
    {
        apvts.addParameterListener (trackParamID (i, "solo"), this);
        apvts.addParameterListener (trackParamID (i, "mute"), this);
    }
    apvts.addParameterListener ("room_solo", this);
    apvts.addParameterListener ("room_mute", this);
    apvts.addParameterListener ("comp_solo", this);
    apvts.addParameterListener ("comp_mute", this);
    apvts.addParameterListener ("sat_solo", this);
    apvts.addParameterListener ("sat_mute", this);

    formatManager.registerBasicFormats();

    // Load the cooked-in default preset.
    // DAWs call setStateInformation() after construction to restore project state,
    // which correctly overrides this.
    {
        auto xml = juce::XmlDocument::parse (
            juce::String::fromUTF8 (BinaryData::DEFAULT_ddd,
                                    BinaryData::DEFAULT_dddSize));
        if (xml)
        {
            juce::MemoryBlock mb;
            copyXmlToBinary (*xml, mb);
            setStateInformation (mb.getData(), (int) mb.getSize());
            currentPresetName = "DEFAULT";
            activePresetFilePath.clear();
        }
    }

    // Parse embedded DEMO.mid into demoSequence
    {
        juce::MemoryInputStream mis (BinaryData::DEMO_mid,
                                     BinaryData::DEMO_midSize, false);
        juce::MidiFile mf;
        if (mf.readFrom (mis))
        {
            mf.convertTimestampTicksToSeconds();
            demoSequence = *mf.getTrack (0);
            demoSequence.updateMatchedPairs();
        }
    }
}

DeathDealerDrumsAudioProcessor::~DeathDealerDrumsAudioProcessor()
{
    for (int i = 0; i < MAX_TRACKS; ++i)
    {
        apvts.removeParameterListener (trackParamID (i, "solo"), this);
        apvts.removeParameterListener (trackParamID (i, "mute"), this);
    }
    apvts.removeParameterListener ("room_solo", this);
    apvts.removeParameterListener ("room_mute", this);
    apvts.removeParameterListener ("comp_solo", this);
    apvts.removeParameterListener ("comp_mute", this);
    apvts.removeParameterListener ("sat_solo", this);
    apvts.removeParameterListener ("sat_mute", this);

    // Ensure no loader job is still touching DrumTrack objects while we're
    // tearing down processor state.
    backgroundLoader.removeAllJobs (true, 10000);

    juce::ScopedWriteLock wl (tracksLock);
    tracks.clear();
}

void DeathDealerDrumsAudioProcessor::parameterChanged (const juce::String& parameterID, float newValue)
{
    if (newValue <= 0.5f)
        return;

    if (soloMuteGuardActive.exchange (true))
        return;

    const auto clearParam = [this] (const juce::String& id)
    {
        if (auto* p = apvts.getParameter (id))
            p->setValueNotifyingHost (0.0f);
    };

    if (parameterID.startsWith ("track_") && parameterID.endsWith ("_solo"))
    {
        clearParam (parameterID.upToLastOccurrenceOf ("_solo", false, false) + "_mute");
    }
    else if (parameterID.startsWith ("track_") && parameterID.endsWith ("_mute"))
    {
        clearParam (parameterID.upToLastOccurrenceOf ("_mute", false, false) + "_solo");
    }
    else if (parameterID == "room_solo") clearParam ("room_mute");
    else if (parameterID == "room_mute") clearParam ("room_solo");
    else if (parameterID == "comp_solo") clearParam ("comp_mute");
    else if (parameterID == "comp_mute") clearParam ("comp_solo");
    else if (parameterID == "sat_solo")  clearParam ("sat_mute");
    else if (parameterID == "sat_mute")  clearParam ("sat_solo");

    soloMuteGuardActive.store (false);
}

void DeathDealerDrumsAudioProcessor::captureDemoLaneMapping()
{
    // Build a stable lane map from the CURRENT track layout at demo start.
    // Each lane tracks a source note from DEMO.mid and points to a current slot.
    juce::ScopedReadLock rl (tracksLock);
    const int n = juce::jlimit (0, MAX_TRACKS, (int) tracks.size());
    demoLaneCount = n;

    for (int i = 0; i < MAX_TRACKS; ++i)
    {
        demoLaneSourceNotes[(size_t) i]   = 60;
        demoLaneToCurrentSlot[(size_t) i] = -1;
    }

    for (int i = 0; i < n; ++i)
    {
        int note = 60;
        if (auto* raw = apvts.getRawParameterValue (trackParamID (i, "midi_note")))
            note = juce::jlimit (0, 127, juce::roundToInt (raw->load()));

        demoLaneSourceNotes[(size_t) i]   = note;
        demoLaneToCurrentSlot[(size_t) i] = i;
    }
}

void DeathDealerDrumsAudioProcessor::updateDemoLaneMappingAfterTrackRemoval (int removedSlot) noexcept
{
    if (removedSlot < 0) return;

    for (int i = 0; i < MAX_TRACKS; ++i)
    {
        int& cur = demoLaneToCurrentSlot[(size_t) i];
        if (cur < 0) continue;
        if (cur == removedSlot) cur = -1;
        else if (cur > removedSlot) --cur;
    }
}

//==============================================================================
bool DeathDealerDrumsAudioProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    if (l.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    const auto stereo   = juce::AudioChannelSet::stereo();
    const auto disabled = juce::AudioChannelSet::disabled();
    for (int i = 1; i < l.outputBuses.size(); ++i)
        if (l.outputBuses[i] != stereo && l.outputBuses[i] != disabled)
            return false;
    return true;
}

void DeathDealerDrumsAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    engine->prepare (sampleRate, samplesPerBlock);
    demoTotalSamples = (int) ((demoSequence.getEndTime() + 0.5) * sampleRate);
}

void DeathDealerDrumsAudioProcessor::releaseResources()
{
    engine->releaseResources();
}

void DeathDealerDrumsAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                                    juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ScopedReadLock rl (tracksLock);

    // Track host transport state for UI animation decisions (editor thread reads atomically)
    bool hostIsPlayingNow = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            hostIsPlayingNow = pos->getIsPlaying();
    hostPlaying.store (hostIsPlayingNow, std::memory_order_relaxed);

    const float humanErrorScatter = juce::jlimit (
        0.05f, 0.20f,
        apvts.getRawParameterValue (PARAM_HUMAN_ERROR)->load());

    // Drain preview triggers queued by UI pad buttons
    {
        int start1, size1, start2, size2;
        previewFifo.prepareToRead (previewFifo.getNumReady(), start1, size1, start2, size2);
        for (int i = 0; i < size1; ++i)
        {
            const auto& item = previewBuf[(size_t)(start1 + i)];
            engine->triggerVariationDirect (item.trackIndex, item.varIndex, item.velocity,
                                            humanErrorScatter, tracks);
        }
        for (int i = 0; i < size2; ++i)
        {
            const auto& item = previewBuf[(size_t)(start2 + i)];
            engine->triggerVariationDirect (item.trackIndex, item.varIndex, item.velocity,
                                            humanErrorScatter, tracks);
        }
        previewFifo.finishedRead (size1 + size2);
    }

    // Map each output bus index to its first channel in the flat processBlock buffer.
    // JUCE only includes enabled buses in the buffer, so we must ask it for the real offset.
    int busChMap[16];
    for (int b = 0; b < 16; ++b)
        busChMap[b] = getChannelIndexInProcessBlockBuffer (false, b, 0); // -1 if bus disabled

    // Inject demo MIDI playback (sample-accurate looping)
    if (demoPlaying.load() && demoTotalSamples > 0)
    {
        int remaining = buffer.getNumSamples();
        int outOffsetBase = 0;

        while (remaining > 0)
        {
            if (demoSamplePos >= (juce::int64) demoTotalSamples)
                demoSamplePos = 0;

            const int samplesUntilWrap = juce::jmax (1, demoTotalSamples - (int) demoSamplePos);
            const int segSamples       = juce::jmin (remaining, samplesUntilWrap);

            const double secStart = (double) demoSamplePos / currentSampleRate;
            const double secEnd   = secStart + (double) segSamples / currentSampleRate;

            for (int i = 0; i < demoSequence.getNumEvents(); ++i)
            {
                const auto* ep = demoSequence.getEventPointer (i);
                const double t = ep->message.getTimeStamp();
                if (t >= secStart && t < secEnd)
                {
                    const int segOffset = juce::jlimit (0, segSamples - 1,
                        (int) ((t - secStart) * currentSampleRate));
                    juce::ignoreUnused (segOffset, outOffsetBase);

                    const auto msg = ep->message;
                    if (msg.isNoteOn() && msg.getVelocity() > 0)
                    {
                        const float vel = msg.getVelocity() / 127.0f;
                        const int note = juce::jlimit (0, 127, msg.getNoteNumber());
                        for (int lane = 0; lane < demoLaneCount; ++lane)
                        {
                            if (demoLaneSourceNotes[(size_t) lane] != note)
                                continue;

                            const int curSlot = demoLaneToCurrentSlot[(size_t) lane];
                            if (curSlot < 0 || curSlot >= (int) tracks.size())
                                continue;

                            // Route demo hits through the regular MIDI path so slave/lock
                            // grouping and anti-flam logic is applied identically to host MIDI.
                            int targetNote = 60;
                            if (auto* raw = apvts.getRawParameterValue (trackParamID (curSlot, "midi_note")))
                                targetNote = juce::roundToInt (raw->load());
                            targetNote = juce::jlimit (0, 127, targetNote);

                            const int outSamplePos = juce::jlimit (0, buffer.getNumSamples() - 1,
                                outOffsetBase + segOffset);
                            midi.addEvent (juce::MidiMessage::noteOn (1, targetNote, vel), outSamplePos);
                        }
                    }
                }
            }

            demoSamplePos += segSamples;
            remaining     -= segSamples;
            outOffsetBase += segSamples;

            if (demoSamplePos >= (juce::int64) demoTotalSamples)
                demoSamplePos = 0;
        }
    }

    engine->process (buffer, midi, apvts, tracks, busChMap);
}

void DeathDealerDrumsAudioProcessor::triggerPreview (int trackIndex, int varIndex, float velocity)
{
    int start1, size1, start2, size2;
    previewFifo.prepareToWrite (1, start1, size1, start2, size2);
    if (size1 > 0)
        previewBuf[(size_t) start1] = { trackIndex, varIndex, velocity };
    else if (size2 > 0)
        previewBuf[(size_t) start2] = { trackIndex, varIndex, velocity };
    previewFifo.finishedWrite (size1 > 0 ? 1 : (size2 > 0 ? 1 : 0));
}

juce::AudioProcessorEditor* DeathDealerDrumsAudioProcessor::createEditor()
{
    return new DeathDealerDrumsAudioProcessorEditor (*this);
}

//==============================================================================
// Track management
//==============================================================================

int DeathDealerDrumsAudioProcessor::getNumActiveTracks() const
{
    juce::ScopedReadLock rl (tracksLock);
    return (int) tracks.size();
}

void DeathDealerDrumsAudioProcessor::addTrack (const juce::String& name, int midiNote)
{
    {
        juce::ScopedWriteLock wl (tracksLock);
        if ((int) tracks.size() >= MAX_TRACKS) return;

        tracks.push_back (std::make_unique<DrumTrack> (name));

        // Reset all APVTS params for this slot to their defaults
        const int slot = (int) tracks.size() - 1;
        static const char* allParams[] = {
            "volume", "pan", "tune", "decay", "attack",
            "eq_low", "eq_mid", "eq_high", "midi_note", "sample_start", "sample_end",
            "reverb_send", "comp_send", "sat_send",
            "choke", "mute", "solo", "output", "output_mode", "phase", "slave_to"
        };
        for (auto* pn : allParams)
        {
            if (auto* p = apvts.getParameter (trackParamID (slot, pn)))
                p->setValueNotifyingHost (p->getDefaultValue());
        }

        // Reset EQ8 and per-track comp params
        for (int b = 0; b < 8; ++b)
        {
            const juce::String bTag = "eq8_b" + juce::String (b);
            for (const juce::String& suf : { juce::String ("_freq"), juce::String ("_gain"), juce::String ("_q") })
                if (auto* p = apvts.getParameter (trackParamID (slot, bTag + suf)))
                    p->setValueNotifyingHost (p->getDefaultValue());
        }
        {
            static const char* compPnames[] = { "trk_comp_on", "trk_comp_thr", "trk_comp_rat",
                                                 "trk_comp_atk", "trk_comp_rel", "trk_comp_mkp" };
            for (auto* pn : compPnames)
                if (auto* p = apvts.getParameter (trackParamID (slot, pn)))
                    p->setValueNotifyingHost (p->getDefaultValue());
        }

        // Override midi_note with the caller-supplied value
        if (auto* p = apvts.getParameter (trackParamID (slot, "midi_note")))
            p->setValueNotifyingHost (p->convertTo0to1 ((float) midiNote));
    }
    if (onTracksChanged) onTracksChanged();
}

void DeathDealerDrumsAudioProcessor::removeTrack (int index)
{
    // Prevent audio-thread contention while structurally mutating tracks + parameter mapping.
    // This operation is internal housekeeping, not user automation, so we avoid host notifications.
    suspendProcessing (true);

    static const char* pNames[] = {
        "volume", "pan", "tune", "decay", "attack",
        "eq_low", "eq_mid", "eq_high", "midi_note", "sample_start", "sample_end",
        "reverb_send", "comp_send", "sat_send",
        "choke", "mute", "solo", "output", "output_mode", "phase",
        "bleed_enable", "bleed_send", "eq8_on", "slave_to"
    };
    static const char* compPs[] = {
        "trk_comp_on", "trk_comp_thr", "trk_comp_rat", "trk_comp_atk", "trk_comp_rel", "trk_comp_mkp"
    };

    struct SlotSnapshot
    {
        std::vector<std::pair<juce::String, float>> normValues;
    };

    int oldCount = 0;
    std::vector<SlotSnapshot> survivors;
    std::vector<float> survivorMidiNotes;
    std::vector<int>   survivorSlaveTargets;

    // Snapshot all surviving slot parameters by absolute name/value before erasing
    {
        juce::ScopedReadLock rl (tracksLock);
        oldCount = (int) tracks.size();
        if (index < 0 || index >= oldCount)
        {
            suspendProcessing (false);
            return;
        }

        survivors.reserve ((size_t) juce::jmax (0, oldCount - 1));
        survivorMidiNotes.reserve ((size_t) juce::jmax (0, oldCount - 1));
        survivorSlaveTargets.reserve ((size_t) juce::jmax (0, oldCount - 1));
        for (int srcSlot = 0; srcSlot < oldCount; ++srcSlot)
        {
            if (srcSlot == index) continue;
            SlotSnapshot snap;

            if (auto* raw = apvts.getRawParameterValue (trackParamID (srcSlot, "midi_note")))
                survivorMidiNotes.push_back (raw->load());
            else
                survivorMidiNotes.push_back (60.0f);

            if (auto* raw = apvts.getRawParameterValue (trackParamID (srcSlot, "slave_to")))
                survivorSlaveTargets.push_back (juce::roundToInt (raw->load()));
            else
                survivorSlaveTargets.push_back (-1);

            for (auto* pn : pNames)
                if (auto* p = apvts.getParameter (trackParamID (srcSlot, pn)))
                    snap.normValues.push_back ({ pn, p->getValue() });

            for (int b = 0; b < 8; ++b)
            {
                const juce::String bTag = "eq8_b" + juce::String (b);
                for (const juce::String& suf : { juce::String ("_freq"), juce::String ("_gain"), juce::String ("_q") })
                {
                    const juce::String full = bTag + suf;
                    if (auto* p = apvts.getParameter (trackParamID (srcSlot, full)))
                        snap.normValues.push_back ({ full, p->getValue() });
                }
            }

            for (auto* pn : compPs)
                if (auto* p = apvts.getParameter (trackParamID (srcSlot, pn)))
                    snap.normValues.push_back ({ pn, p->getValue() });

            survivors.push_back (std::move (snap));
        }
    }

    // Remove track structure and remap active voices
    {
        juce::ScopedWriteLock wl (tracksLock);
        tracks.erase (tracks.begin() + index);
    }
    if (engine)
        engine->handleTrackRemoved (index);

    updateDemoLaneMappingAfterTrackRemoval (index);

    // Restore surviving slots exactly from snapshot (no incremental shift ambiguity)
    for (int dstSlot = 0; dstSlot < (int) survivors.size(); ++dstSlot)
    {
        for (const auto& kv : survivors[(size_t) dstSlot].normValues)
            if (auto* p = apvts.getParameter (trackParamID (dstSlot, kv.first)))
                p->setValueNotifyingHost (kv.second);

        // Commit MIDI note explicitly (host-notified) so note identity follows the surviving track.
        if (dstSlot < (int) survivorMidiNotes.size())
            if (auto* p = apvts.getParameter (trackParamID (dstSlot, "midi_note")))
                p->setValueNotifyingHost (p->convertTo0to1 (survivorMidiNotes[(size_t) dstSlot]));

        // Remap slave target so links remain valid after slot compaction.
        if (dstSlot < (int) survivorSlaveTargets.size())
        {
            int target = survivorSlaveTargets[(size_t) dstSlot];
            if (target == index)      target = -1;      // linked-to track was deleted
            else if (target > index)  --target;         // downstream slot shifted left
            if (target == dstSlot)    target = -1;      // disallow self-slave

            if (auto* p = apvts.getParameter (trackParamID (dstSlot, "slave_to")))
                p->setValueNotifyingHost (p->convertTo0to1 ((float) target));
        }
    }

    // Reset old tail slot to defaults after compaction
    if (oldCount > 0)
    {
        const int tail = oldCount - 1;
        for (auto* pn : pNames)
            if (auto* p = apvts.getParameter (trackParamID (tail, pn)))
                p->setValueNotifyingHost (p->getDefaultValue());

        for (int b = 0; b < 8; ++b)
        {
            const juce::String bTag = "eq8_b" + juce::String (b);
            for (const juce::String& suf : { juce::String ("_freq"), juce::String ("_gain"), juce::String ("_q") })
                if (auto* p = apvts.getParameter (trackParamID (tail, bTag + suf)))
                    p->setValueNotifyingHost (p->getDefaultValue());
        }

        for (auto* pn : compPs)
            if (auto* p = apvts.getParameter (trackParamID (tail, pn)))
                p->setValueNotifyingHost (p->getDefaultValue());
    }

    suspendProcessing (false);
    if (onTracksChanged) onTracksChanged();
}

void DeathDealerDrumsAudioProcessor::renameTrack (int index, const juce::String& name)
{
    {
        juce::ScopedReadLock rl (tracksLock);
        if (index >= 0 && index < (int) tracks.size())
            tracks[(size_t) index]->setName (name);
    }
    // Do NOT call onTracksChanged here — a rename is not a structural change.
    // The UI handles the targeted label refresh via the onNameChanged callback.
}

DrumTrack* DeathDealerDrumsAudioProcessor::getTrack (int index)
{
    juce::ScopedReadLock rl (tracksLock);
    if (index < 0 || index >= (int) tracks.size()) return nullptr;
    return tracks[(size_t) index].get();
}

void DeathDealerDrumsAudioProcessor::loadSampleForTrack (int trackIndex,
                                                          const juce::File& file)
{
    backgroundLoader.addJob ([this, trackIndex, file]
    {
        DrumTrack* track = nullptr;
        {
            juce::ScopedReadLock rl (tracksLock);
            if (trackIndex >= 0 && trackIndex < (int) tracks.size())
                track = tracks[(size_t) trackIndex].get();
        }
        if (track)
            track->loadSampleAndGenerateVariations (file, currentSampleRate, formatManager);

        juce::MessageManager::callAsync ([this, trackIndex]
        {
            if (onSampleLoaded) onSampleLoaded (trackIndex);
        });
    });
}

//==============================================================================
void DeathDealerDrumsAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::ValueTree root ("DDDState");
    root.setProperty ("currentPresetName", currentPresetName, nullptr);
    root.setProperty ("currentPresetFilePath", activePresetFilePath, nullptr);
    root.addChild (apvts.copyState(), -1, nullptr);

    juce::ValueTree tracksNode ("Tracks");
    {
        juce::ScopedReadLock rl (tracksLock);
        for (int i = 0; i < (int) tracks.size(); ++i)
        {
            juce::ValueTree t ("Track");
            t.setProperty ("name",        tracks[(size_t) i]->getName(),             nullptr);
            t.setProperty ("samplePath",  tracks[(size_t) i]->getSamplePath(),       nullptr);
            t.setProperty ("sampleMode",  (int) tracks[(size_t) i]->getSampleMode(), nullptr);
            t.setProperty ("numVariants", tracks[(size_t) i]->getNumVariants(),      nullptr);

            // Embed user sample data as base64 so the preset is self-contained.
            // Built-in ({builtin}/) and empty paths have no data to embed.
            const auto& raw = tracks[(size_t) i]->getRawSampleData();
            const auto  sp  = tracks[(size_t) i]->getSamplePath();
            if (raw.getSize() > 0 && !sp.startsWith ("{builtin}/"))
                t.setProperty ("sampleData",
                               juce::Base64::toBase64 (raw.getData(), raw.getSize()),
                               nullptr);

            tracksNode.addChild (t, -1, nullptr);
        }
    }
    root.addChild (tracksNode, -1, nullptr);

    std::unique_ptr<juce::XmlElement> xml (root.createXml());
    copyXmlToBinary (*xml, destData);
}

void DeathDealerDrumsAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // A previous preset load may still have async sample decode jobs running.
    // Wait for them before replacing the track list to avoid destroying
    // DrumTrack objects while worker code is still inside them.
    backgroundLoader.removeAllJobs (true, 10000);

    auto xml = std::unique_ptr<juce::XmlElement> (getXmlFromBinary (data, sizeInBytes));
    if (!xml) return;

    const auto root = juce::ValueTree::fromXml (*xml);
    if (!root.isValid()) return;

    // If the saved state has no Tracks node at all (legacy/invalid state),
    // fall back to the cooked-in default.
    // NOTE: an empty Tracks node (0 children) is now treated as a valid preset
    // state so users can intentionally save/load "empty kit" presets.
    const auto tracksNodeCheck = root.getChildWithName ("Tracks");
    if (!tracksNodeCheck.isValid())
    {
        auto defXml = juce::XmlDocument::parse (
            juce::String::fromUTF8 (BinaryData::DEFAULT_ddd, BinaryData::DEFAULT_dddSize));
        if (defXml)
        {
            juce::MemoryBlock mb;
            copyXmlToBinary (*defXml, mb);
            setStateInformation (mb.getData(), (int) mb.getSize());
            currentPresetName = "DEFAULT";
            activePresetFilePath.clear();
        }
        return;
    }

    currentPresetName = root.getProperty ("currentPresetName", "DEFAULT").toString();
    activePresetFilePath = root.getProperty ("currentPresetFilePath", "").toString();
    if (currentPresetName.isEmpty())
        currentPresetName = "DEFAULT";
    if (currentPresetName.equalsIgnoreCase ("DEFAULT"))
        activePresetFilePath.clear();

    // Restore APVTS
    const auto apvtsState = root.getChildWithName ("DDDParams");
    if (apvtsState.isValid()) apvts.replaceState (apvtsState);

    // Restore tracks
    const auto tracksNode = root.getChildWithName ("Tracks");
    if (tracksNode.isValid())
    {
        juce::ScopedWriteLock wl (tracksLock);
        tracks.clear();
        for (const auto& child : tracksNode)
        {
            juce::String name = child.getProperty ("name", "DRUM");
            tracks.push_back (std::make_unique<DrumTrack> (name));
            const int sMode = (int) child.getProperty ("sampleMode",  0);
            const int nVars = (int) child.getProperty ("numVariants", 1);
            tracks.back()->setSampleMode ((DrumTrack::SampleMode) sMode, nVars);
            const juce::String path = child.getProperty ("samplePath", "");
            if (path.isNotEmpty())
            {
                // Decode any embedded sample data (non-builtin user samples)
                juce::MemoryBlock embeddedData;
                const juce::String b64 = child.getProperty ("sampleData", "");
                if (b64.isNotEmpty())
                {
                    juce::MemoryOutputStream mos (embeddedData, false);
                    juce::Base64::convertFromBase64 (mos, b64);
                }

                const int slot = (int) tracks.size() - 1;
                backgroundLoader.addJob ([this, slot, path, embeddedData]
                {
                    DrumTrack* track = nullptr;
                    {
                        juce::ScopedReadLock rl (tracksLock);
                        if (slot < (int) tracks.size())
                            track = tracks[(size_t) slot].get();
                    }
                    if (track)
                    {
                        // Built-in samples are baked into BinaryData
                        if (path.startsWith ("{builtin}/"))
                        {
                            const juce::String name = path.fromFirstOccurrenceOf ("{builtin}/", false, false);
                            const void* data   = nullptr;
                            int         bytes  = 0;
                            if      (name == "KICK.wav")             { data = BinaryData::KICK_wav;             bytes = BinaryData::KICK_wavSize;             }
                            else if (name == "KICK 2.wav")           { data = BinaryData::KICK_2_wav;           bytes = BinaryData::KICK_2_wavSize;           }
                            else if (name == "SNARE.wav")            { data = BinaryData::SNARE_wav;            bytes = BinaryData::SNARE_wavSize;            }
                            else if (name == "RIM.wav")              { data = BinaryData::RIM_wav;              bytes = BinaryData::RIM_wavSize;              }
                            else if (name == "TOM 1.wav")            { data = BinaryData::TOM_1_wav;            bytes = BinaryData::TOM_1_wavSize;            }
                            else if (name == "TOM 2.wav")            { data = BinaryData::TOM_2_wav;            bytes = BinaryData::TOM_2_wavSize;            }
                            else if (name == "TOM 3.wav")            { data = BinaryData::TOM_3_wav;            bytes = BinaryData::TOM_3_wavSize;            }
                            else if (name == "TOM 4.wav")            { data = BinaryData::TOM_4_wav;            bytes = BinaryData::TOM_4_wavSize;            }
                            else if (name == "HIHAT CLOSED.wav")     { data = BinaryData::HIHAT_CLOSED_wav;     bytes = BinaryData::HIHAT_CLOSED_wavSize;     }
                            else if (name == "HIHAT HALF LOOSE.wav") { data = BinaryData::HIHAT_HALF_LOOSE_wav; bytes = BinaryData::HIHAT_HALF_LOOSE_wavSize; }
                            else if (name == "HIHAT LOOSE.wav")      { data = BinaryData::HIHAT_LOOSE_wav;      bytes = BinaryData::HIHAT_LOOSE_wavSize;      }
                            else if (name == "HIHAT OPEN.wav")       { data = BinaryData::HIHAT_OPEN_wav;       bytes = BinaryData::HIHAT_OPEN_wavSize;       }
                            else if (name == "CRASH 1.wav")          { data = BinaryData::CRASH_1_wav;          bytes = BinaryData::CRASH_1_wavSize;          }
                            else if (name == "CRASH 2.wav")          { data = BinaryData::CRASH_2_wav;          bytes = BinaryData::CRASH_2_wavSize;          }
                            else if (name == "RIDE.wav")             { data = BinaryData::RIDE_wav;             bytes = BinaryData::RIDE_wavSize;             }
                            else if (name == "CHINA.wav")            { data = BinaryData::CHINA_wav;            bytes = BinaryData::CHINA_wavSize;            }
                            else if (name == "SPLASH.wav")           { data = BinaryData::SPLASH_wav;           bytes = BinaryData::SPLASH_wavSize;           }
                            else if (name == "BELL.wav")             { data = BinaryData::BELL_wav;             bytes = BinaryData::BELL_wavSize;             }
                            if (data)
                                track->loadSampleAndGenerateVariations (data, bytes, path, currentSampleRate, formatManager);
                        }
                        else if (embeddedData.getSize() > 0)
                        {
                            // Sample was embedded in the preset — fully portable, no file needed
                            track->loadSampleAndGenerateVariations (embeddedData.getData(),
                                                                     (int) embeddedData.getSize(),
                                                                     path, currentSampleRate, formatManager);
                        }
                        else
                        {
                            track->loadSampleAndGenerateVariations (juce::File (path), currentSampleRate, formatManager);
                        }
                    }
                    juce::MessageManager::callAsync ([this, slot]
                    {
                        if (onSampleLoaded) onSampleLoaded (slot);
                    });
                });
            }
        }
    }
    // Notify UI that track list has changed (tracks vector is populated; samples still loading in background)
    if (onTracksChanged) juce::MessageManager::callAsync ([this] { if (onTracksChanged) onTracksChanged(); });
}

//==============================================================================
juce::File DeathDealerDrumsAudioProcessor::getPresetsFolder()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
               .getChildFile ("Death Dealer Drums")
               .getChildFile ("Presets");
}

void DeathDealerDrumsAudioProcessor::savePreset (const juce::File& file, bool setAsCurrent)
{
    juce::MemoryBlock mb;
    getStateInformation (mb);
    auto xml = std::unique_ptr<juce::XmlElement> (
        getXmlFromBinary (mb.getData(), (int) mb.getSize()));
    if (xml)
    {
        file.getParentDirectory().createDirectory();
        xml->writeToFile (file, {});
        if (setAsCurrent)
        {
            currentPresetName = file.getFileNameWithoutExtension();
            activePresetFilePath = file.getFullPathName();
        }
    }
}

bool DeathDealerDrumsAudioProcessor::loadPreset (const juce::File& file)
{
    auto xml = juce::XmlDocument::parse (file);
    if (!xml) return false;
    juce::MemoryBlock mb;
    copyXmlToBinary (*xml, mb);
    setStateInformation (mb.getData(), (int) mb.getSize());
    currentPresetName = file.getFileNameWithoutExtension();
    activePresetFilePath = file.getFullPathName();
    return true;
}

void DeathDealerDrumsAudioProcessor::startDemo() noexcept
{
    captureDemoLaneMapping();
    demoSamplePos = 0;
    demoPlaying.store (true);
}

void DeathDealerDrumsAudioProcessor::stopDemo() noexcept
{
    demoPlaying.store (false);
    demoSamplePos = 0;
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DeathDealerDrumsAudioProcessor();
}

