#include "PluginProcessor.h"

namespace
{
constexpr auto frameRateParamId = "frameRate";
constexpr auto followHostRateParamId = "followHostRate";
constexpr auto outputLevelDbParamId = "outputLevelDb";

template <typename SampleType>
void processBlockTyped (TimecodeLtcAudioProcessor& processor,
                        ltc::Generator& generator,
                        std::atomic<float>& frameRateChoice,
                        std::atomic<float>& followHostRate,
                        std::atomic<float>& outputLevelDb,
                        juce::AudioBuffer<SampleType>& buffer,
                        juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    midiMessages.clear();
    buffer.clear();

    const auto sampleRate = processor.getSampleRate();
    if (sampleRate <= 0.0)
    {
        generator.reset();
        return;
    }

    auto* hostPlayHead = processor.getPlayHead();
    if (hostPlayHead == nullptr)
    {
        generator.reset();
        return;
    }

    const auto positionInfo = hostPlayHead->getPosition();
    if (! positionInfo.hasValue() || ! positionInfo->getIsPlaying())
    {
        generator.reset();
        return;
    }

    const auto timeInSamples = positionInfo->getTimeInSamples();
    if (! timeInSamples.hasValue())
    {
        generator.reset();
        return;
    }

    const auto manualRate = ltc::fromChoiceIndex (static_cast<int> (std::lround (frameRateChoice.load (std::memory_order_relaxed))));
    auto rate = manualRate;

    if (followHostRate.load (std::memory_order_relaxed) > 0.5f)
        if (const auto hostRate = positionInfo->getFrameRate())
            rate = ltc::fromJuceFrameRate (*hostRate).value_or (manualRate);

    const auto absoluteSeconds = positionInfo->getEditOriginTime().orFallback (0.0)
                               + (static_cast<double> (*timeInSamples) / sampleRate);

    generator.setFrameRate (rate);
    generator.setOutputLevelDb (outputLevelDb.load (std::memory_order_relaxed));
    generator.setPendingTimecode (ltc::fromSeconds (absoluteSeconds, rate));
    generator.render (buffer.getWritePointer (0), buffer.getNumSamples());
}
}

TimecodeLtcAudioProcessor::TimecodeLtcAudioProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::mono(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::mono(), true)),
      state (*this, nullptr, "state", createParameterLayout())
{
    frameRateChoice = state.getRawParameterValue (frameRateParamId);
    followHostRate = state.getRawParameterValue (followHostRateParamId);
    outputLevelDb = state.getRawParameterValue (outputLevelDbParamId);

    jassert (frameRateChoice != nullptr);
    jassert (followHostRate != nullptr);
    jassert (outputLevelDb != nullptr);
}

void TimecodeLtcAudioProcessor::prepareToPlay (double sampleRate, int)
{
    generator.prepare (sampleRate);
    generator.setOutputLevelDb (outputLevelDb->load (std::memory_order_relaxed));
}

void TimecodeLtcAudioProcessor::releaseResources()
{
}

void TimecodeLtcAudioProcessor::reset()
{
    generator.reset();
}

bool TimecodeLtcAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& output = layouts.getMainOutputChannelSet();
    const auto& input = layouts.getMainInputChannelSet();

    if (output != juce::AudioChannelSet::mono())
        return false;

    return input.isDisabled() || input == juce::AudioChannelSet::mono();
}

void TimecodeLtcAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    processBlockTyped (*this,
                       generator,
                       *frameRateChoice,
                       *followHostRate,
                       *outputLevelDb,
                       buffer,
                       midiMessages);
}

void TimecodeLtcAudioProcessor::processBlock (juce::AudioBuffer<double>& buffer, juce::MidiBuffer& midiMessages)
{
    processBlockTyped (*this,
                       generator,
                       *frameRateChoice,
                       *followHostRate,
                       *outputLevelDb,
                       buffer,
                       midiMessages);
}

juce::AudioProcessorEditor* TimecodeLtcAudioProcessor::createEditor()
{
    return new juce::GenericAudioProcessorEditor (*this);
}

bool TimecodeLtcAudioProcessor::hasEditor() const
{
    return true;
}

const juce::String TimecodeLtcAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool TimecodeLtcAudioProcessor::acceptsMidi() const
{
    return false;
}

bool TimecodeLtcAudioProcessor::producesMidi() const
{
    return false;
}

bool TimecodeLtcAudioProcessor::isMidiEffect() const
{
    return false;
}

double TimecodeLtcAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int TimecodeLtcAudioProcessor::getNumPrograms()
{
    return 1;
}

int TimecodeLtcAudioProcessor::getCurrentProgram()
{
    return 0;
}

void TimecodeLtcAudioProcessor::setCurrentProgram (int)
{
}

const juce::String TimecodeLtcAudioProcessor::getProgramName (int)
{
    return {};
}

void TimecodeLtcAudioProcessor::changeProgramName (int, const juce::String&)
{
}

void TimecodeLtcAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = state.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void TimecodeLtcAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        state.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorValueTreeState::ParameterLayout TimecodeLtcAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { frameRateParamId, 1 },
        "Frame Rate",
        juce::StringArray { "23.976", "24", "25", "29.97", "29.97 DF", "30" },
        2));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { followHostRateParamId, 1 },
        "Use Host Rate",
        true));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { outputLevelDbParamId, 1 },
        "Output Level dBFS",
        juce::NormalisableRange<float> (-36.0f, 0.0f, 0.1f),
        -18.0f));

    return layout;
}

ltc::FrameRate TimecodeLtcAudioProcessor::getActiveFrameRate (const juce::AudioPlayHead::PositionInfo& positionInfo) const
{
    const auto manualRate = ltc::fromChoiceIndex (static_cast<int> (std::lround (frameRateChoice->load (std::memory_order_relaxed))));

    if (followHostRate->load (std::memory_order_relaxed) <= 0.5f)
        return manualRate;

    const auto hostRate = positionInfo.getFrameRate();
    if (! hostRate.hasValue())
        return manualRate;

    return ltc::fromJuceFrameRate (*hostRate).value_or (manualRate);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TimecodeLtcAudioProcessor();
}
