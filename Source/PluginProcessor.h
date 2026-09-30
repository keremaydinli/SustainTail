#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>

// SustainTail — MIDI effect that gives every note a sustain tail after the key
// is released: it holds each note at full level for "Release tail" ms, then fades
// it out over "Fade time" ms (via MIDI Expression, CC11) before sending note-off.
class SustainTailProcessor : public juce::AudioProcessor
{
public:
    SustainTailProcessor();
    ~SustainTailProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "SustainTail"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return true; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    static constexpr int kNumChannels = 16;
    static constexpr int kNumNotes    = 128;
    static constexpr int kNumSlots    = kNumChannels * kNumNotes;

    double currentSampleRate = 44100.0;
    long long now = 0;

    // Absolute sample time at which each note's fade should BEGIN (full-level
    // sustain runs until then, the fade runs for fadeSamples after). -1 = inactive.
    // The actual note-off is sent at fadeStart + fadeSamples. Indexed by
    // (channel-1) * 128 + note.
    std::array<long long, kNumSlots> fadeStart;

    // Whether the physical key is currently held down (note-on received, note-off not yet).
    std::array<bool, kNumSlots> keyDown;

    // Last Expression (CC11) value sent per channel, to avoid redundant messages.
    std::array<int, kNumChannels> lastExpr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SustainTailProcessor)
};
