#include "PluginProcessor.h"
#include <cmath>

namespace
{
    // MIDI Channel Volume controller (CC7). Almost universally respected by
    // instruments and reliably attenuates all the way to true silence, so the
    // fade reaches zero before the note-off — no sharp cutoff at the end.
    constexpr int kExprCC = 7;
}

SustainTailProcessor::SustainTailProcessor()
    : AudioProcessor (BusesProperties()),   // no audio buses -> pure MIDI effect
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    fadeStart.fill (-1);
    keyDown.fill (false);
    lastExpr.fill (127);
}

juce::AudioProcessorValueTreeState::ParameterLayout SustainTailProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "release", 1 },
        "Release tail (ms)",
        juce::NormalisableRange<float> (0.0f, 10000.0f, 1.0f),
        2000.0f));

    layout.add (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "fade", 1 },
        "Fade time (ms)",
        juce::NormalisableRange<float> (0.0f, 10000.0f, 1.0f),
        1000.0f));

    return layout;
}

void SustainTailProcessor::prepareToPlay (double sampleRate, int)
{
    currentSampleRate = sampleRate;
    now = 0;
    fadeStart.fill (-1);
    keyDown.fill (false);
    lastExpr.fill (127);
}

void SustainTailProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    const int numSamples       = buffer.getNumSamples();
    const long long blockStart = now;
    const long long blockEnd   = now + numSamples;

    const float releaseMs = apvts.getRawParameterValue ("release")->load();
    const float fadeMs    = apvts.getRawParameterValue ("fade")->load();
    const long long releaseSamples = (long long) (releaseMs * 0.001 * currentSampleRate);
    const long long fadeSamples    = juce::jmax ((long long) 1,
                                                 (long long) (fadeMs * 0.001 * currentSampleRate));

    juce::MidiBuffer output;

    for (const auto metadata : midi)
    {
        const auto msg = metadata.getMessage();
        const int  pos = metadata.samplePosition;

        if (msg.isNoteOn())
        {
            const int ch  = msg.getChannel();                    // 1..16
            const int idx = (ch - 1) * kNumNotes + msg.getNoteNumber();
            if (idx >= 0 && idx < kNumSlots)
            {
                // If this note is still ringing/fading, cut it first, then retrigger.
                if (fadeStart[idx] >= 0)
                {
                    output.addEvent (juce::MidiMessage::noteOff (ch, msg.getNoteNumber()), pos);
                    fadeStart[idx] = -1;
                }
                keyDown[idx] = true;

                // Make sure the new note starts at full level.
                if (lastExpr[ch - 1] != 127)
                {
                    output.addEvent (juce::MidiMessage::controllerEvent (ch, kExprCC, 127), pos);
                    lastExpr[ch - 1] = 127;
                }
            }
            output.addEvent (msg, pos);
        }
        else if (msg.isNoteOff())
        {
            const int ch  = msg.getChannel();
            const int idx = (ch - 1) * kNumNotes + msg.getNoteNumber();
            if (idx >= 0 && idx < kNumSlots)
            {
                keyDown[idx] = false;
                // Begin the tail: full-level sustain for releaseSamples, then fade.
                fadeStart[idx] = blockStart + pos + releaseSamples;
            }
        }
        else
        {
            output.addEvent (msg, pos); // everything else passes through untouched
        }
    }

    // Send scheduled note-offs whose fade has fully completed within this block.
    for (int idx = 0; idx < kNumSlots; ++idx)
    {
        if (fadeStart[idx] >= 0)
        {
            const long long noteOffAt = fadeStart[idx] + fadeSamples;
            if (noteOffAt < blockEnd)
            {
                int off = (int) (noteOffAt - blockStart);
                if (off < 0) off = 0;
                if (off >= numSamples) off = (numSamples > 0 ? numSamples - 1 : 0);

                const int ch   = idx / kNumNotes + 1;
                const int note = idx % kNumNotes;
                output.addEvent (juce::MidiMessage::noteOff (ch, note), off);
                fadeStart[idx] = -1;
            }
        }
    }

    // Per-channel Expression: full while a key is held or still sustaining,
    // ramping down while notes are fading, back to full when idle.
    for (int ch = 0; ch < kNumChannels; ++ch)
    {
        bool needFull   = false;
        double maxLevel = -1.0;   // highest remaining fade level among fading notes

        for (int note = 0; note < kNumNotes; ++note)
        {
            const int idx = ch * kNumNotes + note;

            if (keyDown[idx]) { needFull = true; break; }          // key physically down

            if (fadeStart[idx] >= 0)
            {
                if (blockStart < fadeStart[idx]) { needFull = true; break; }  // sustaining (pre-fade)

                const double level = 1.0 - (double) (blockStart - fadeStart[idx]) / (double) fadeSamples;
                maxLevel = juce::jmax (maxLevel, juce::jlimit (0.0, 1.0, level));
            }
        }

        int target;
        if (needFull)
        {
            target = 127;
        }
        else if (maxLevel >= 0)
        {
            // Exponential fade curve: drops quickly at first, then lingers quietly
            // as it tails off — sounds far more natural than a linear ramp for a
            // decaying note. maxLevel is linear (1 -> 0); shape it before sending.
            constexpr double k = 5.0;
            const double shaped = (std::exp (k * maxLevel) - 1.0) / (std::exp (k) - 1.0);
            target = (int) std::round (shaped * 127.0);
        }
        else
        {
            target = 127;   // idle default
        }

        if (target != lastExpr[ch])
        {
            // When raising the level, send it at the end of the block so it lands
            // after any note-offs earlier in the block (avoids a full-volume blip).
            const int sendOff = (target > lastExpr[ch] && numSamples > 0) ? numSamples - 1 : 0;
            output.addEvent (juce::MidiMessage::controllerEvent (ch + 1, kExprCC, target), sendOff);
            lastExpr[ch] = target;
        }
    }

    midi.swapWith (output);
    now = blockEnd;
}

juce::AudioProcessorEditor* SustainTailProcessor::createEditor()
{
    // Auto-generated UI with "Release tail (ms)" and "Fade time (ms)" sliders.
    return new juce::GenericAudioProcessorEditor (*this);
}

void SustainTailProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState(); state.isValid())
    {
        std::unique_ptr<juce::XmlElement> xml (state.createXml());
        copyXmlToBinary (*xml, destData);
    }
}

void SustainTailProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml != nullptr && xml->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

// JUCE entry point.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SustainTailProcessor();
}
