#pragma once

#include <JuceHeader.h>

#include <atomic>

#include "LtcGenerator.h"

class TimecodeLtcAudioProcessor final : public juce::AudioProcessor
{
public:
    TimecodeLtcAudioProcessor();
    ~TimecodeLtcAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void reset() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlock (juce::AudioBuffer<double>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    ltc::FrameRate getActiveFrameRate (const juce::AudioPlayHead::PositionInfo& positionInfo) const;

    juce::AudioProcessorValueTreeState state;
    std::atomic<float>* frameRateChoice = nullptr;
    std::atomic<float>* followHostRate = nullptr;
    std::atomic<float>* outputLevelDb = nullptr;
    ltc::Generator generator;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TimecodeLtcAudioProcessor)
};
