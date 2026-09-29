#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>

// SustainTail — MIDI effect that delays each note-off by an adjustable amount,
// giving every note a "release tail" (auto-sustain) after you lift the key.
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

    double currentSampleRate = 44100.0;
    long long now = 0;

    // Absolute sample time at which each note's delayed note-off should fire,
    // or -1 when nothing is pending. Indexed by (channel-1) * 128 + note.
    std::array<long long, 16 * 128> pendingRelease;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SustainTailProcessor)
};
