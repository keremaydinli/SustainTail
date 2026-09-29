#include "PluginProcessor.h"

SustainTailProcessor::SustainTailProcessor()
    : AudioProcessor (BusesProperties()),   // no audio buses -> pure MIDI effect
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    pendingRelease.fill (-1);
}

juce::AudioProcessorValueTreeState::ParameterLayout SustainTailProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "release", 1 },
        "Release tail (ms)",
        juce::NormalisableRange<float> (0.0f, 10000.0f, 1.0f),
        2000.0f));
    return layout;
}

void SustainTailProcessor::prepareToPlay (double sampleRate, int)
{
    currentSampleRate = sampleRate;
    now = 0;
    pendingRelease.fill (-1);
}

void SustainTailProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    const int numSamples      = buffer.getNumSamples();
    const long long blockStart = now;
    const long long blockEnd   = now + numSamples;
    const float releaseMs      = apvts.getRawParameterValue ("release")->load();
    const long long delaySamples = (long long) (releaseMs * 0.001 * currentSampleRate);

    juce::MidiBuffer output;

    for (const auto metadata : midi)
    {
        const auto msg = metadata.getMessage();
        const int  pos = metadata.samplePosition;

        if (msg.isNoteOn())
        {
            const int idx = (msg.getChannel() - 1) * 128 + msg.getNoteNumber();
            // If this note is still ringing on a delayed release, cut it first, then retrigger.
            if (idx >= 0 && idx < (int) pendingRelease.size() && pendingRelease[idx] >= 0)
            {
                output.addEvent (juce::MidiMessage::noteOff (msg.getChannel(), msg.getNoteNumber()), pos);
                pendingRelease[idx] = -1;
            }
            output.addEvent (msg, pos);
        }
        else if (msg.isNoteOff())
        {
            const int idx = (msg.getChannel() - 1) * 128 + msg.getNoteNumber();
            // Hold the note-off: schedule it for delaySamples later instead of sending now.
            if (idx >= 0 && idx < (int) pendingRelease.size())
                pendingRelease[idx] = blockStart + pos + delaySamples;
        }
        else
        {
            output.addEvent (msg, pos); // everything else passes through untouched
        }
    }

    // Fire any scheduled note-offs that land within this block.
    for (int idx = 0; idx < (int) pendingRelease.size(); ++idx)
    {
        if (pendingRelease[idx] >= 0 && pendingRelease[idx] < blockEnd)
        {
            int off = (int) (pendingRelease[idx] - blockStart);
            if (off < 0) off = 0;
            if (off >= numSamples) off = (numSamples > 0 ? numSamples - 1 : 0);

            const int channel = idx / 128 + 1;
            const int note    = idx % 128;
            output.addEvent (juce::MidiMessage::noteOff (channel, note), off);
            pendingRelease[idx] = -1;
        }
    }

    midi.swapWith (output);
    now = blockEnd;
}

juce::AudioProcessorEditor* SustainTailProcessor::createEditor()
{
    // Auto-generated UI with a single "Release tail (ms)" slider.
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
