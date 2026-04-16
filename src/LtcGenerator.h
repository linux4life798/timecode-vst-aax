#pragma once

#include <JuceHeader.h>

#include <array>
#include <cmath>
#include <cstdint>
#include <optional>

namespace ltc
{
enum class FrameRate
{
    fps23976,
    fps24,
    fps25,
    fps2997,
    fps2997Drop,
    fps30,
};

struct Timecode
{
    int hours = 0;
    int minutes = 0;
    int seconds = 0;
    int frames = 0;
};

inline double effectiveRate (FrameRate rate)
{
    switch (rate)
    {
        case FrameRate::fps23976:   return 24000.0 / 1001.0;
        case FrameRate::fps24:      return 24.0;
        case FrameRate::fps25:      return 25.0;
        case FrameRate::fps2997:
        case FrameRate::fps2997Drop:return 30000.0 / 1001.0;
        case FrameRate::fps30:      return 30.0;
    }

    return 30.0;
}

inline int displayFramesPerSecond (FrameRate rate)
{
    switch (rate)
    {
        case FrameRate::fps23976:
        case FrameRate::fps24:      return 24;
        case FrameRate::fps25:      return 25;
        case FrameRate::fps2997:
        case FrameRate::fps2997Drop:
        case FrameRate::fps30:      return 30;
    }

    return 30;
}

inline bool isDropFrame (FrameRate rate)
{
    return rate == FrameRate::fps2997Drop;
}

inline FrameRate fromChoiceIndex (int index)
{
    switch (juce::jlimit (0, 5, index))
    {
        case 0: return FrameRate::fps23976;
        case 1: return FrameRate::fps24;
        case 2: return FrameRate::fps25;
        case 3: return FrameRate::fps2997;
        case 4: return FrameRate::fps2997Drop;
        case 5: return FrameRate::fps30;
    }

    return FrameRate::fps25;
}

inline std::optional<FrameRate> fromJuceFrameRate (const juce::AudioPlayHead::FrameRate& rate)
{
    switch (rate.getType())
    {
        case juce::AudioPlayHead::fps23976:      return std::optional<FrameRate> { FrameRate::fps23976 };
        case juce::AudioPlayHead::fps24:         return std::optional<FrameRate> { FrameRate::fps24 };
        case juce::AudioPlayHead::fps25:         return std::optional<FrameRate> { FrameRate::fps25 };
        case juce::AudioPlayHead::fps2997:       return std::optional<FrameRate> { FrameRate::fps2997 };
        case juce::AudioPlayHead::fps2997drop:   return std::optional<FrameRate> { FrameRate::fps2997Drop };
        case juce::AudioPlayHead::fps30:         return std::optional<FrameRate> { FrameRate::fps30 };
        case juce::AudioPlayHead::fps30drop:
        case juce::AudioPlayHead::fps60:
        case juce::AudioPlayHead::fps60drop:
        case juce::AudioPlayHead::fpsUnknown:    return std::nullopt;
    }

    return std::nullopt;
}

inline int64_t continuousFramesPerDay (FrameRate rate)
{
    if (rate == FrameRate::fps2997Drop)
        return (24LL * 60LL * 60LL * 30LL) - (2LL * (24LL * 60LL - 24LL * 6LL));

    return 24LL * 60LL * 60LL * displayFramesPerSecond (rate);
}

inline int64_t continuousFrameIndex (const Timecode& tc, FrameRate rate)
{
    const auto baseFrames = static_cast<int64_t> (displayFramesPerSecond (rate));
    auto frameNumber = ((static_cast<int64_t> (tc.hours) * 3600LL)
                      + (static_cast<int64_t> (tc.minutes) * 60LL)
                      +  static_cast<int64_t> (tc.seconds)) * baseFrames
                      +  static_cast<int64_t> (tc.frames);

    if (rate == FrameRate::fps2997Drop)
    {
        const auto totalMinutes = static_cast<int64_t> (tc.hours) * 60LL + static_cast<int64_t> (tc.minutes);
        const auto dropped = 2LL * (totalMinutes - (totalMinutes / 10LL));
        frameNumber -= dropped;
    }

    return frameNumber;
}

inline int64_t frameDistance (const Timecode& expected, const Timecode& incoming, FrameRate rate)
{
    const auto dayFrames = continuousFramesPerDay (rate);
    auto diff = continuousFrameIndex (incoming, rate) - continuousFrameIndex (expected, rate);
    diff = ((diff % dayFrames) + dayFrames) % dayFrames;

    if (diff > dayFrames / 2)
        diff = dayFrames - diff;

    return diff;
}

inline Timecode incrementFrame (Timecode tc, FrameRate rate)
{
    const auto maxFrames = displayFramesPerSecond (rate);

    tc.frames++;

    if (tc.frames >= maxFrames)
    {
        tc.frames = 0;
        tc.seconds++;
    }

    if (tc.seconds >= 60)
    {
        tc.seconds = 0;
        tc.minutes++;
    }

    if (tc.minutes >= 60)
    {
        tc.minutes = 0;
        tc.hours++;
    }

    if (tc.hours >= 24)
        tc.hours = 0;

    if (rate == FrameRate::fps2997Drop
        && tc.frames == 0
        && tc.seconds == 0
        && (tc.minutes % 10) != 0)
    {
        tc.frames = 2;
    }

    return tc;
}

inline Timecode fromSeconds (double secondsSinceMidnight, FrameRate rate)
{
    constexpr auto secondsPerDay = 24.0 * 60.0 * 60.0;

    auto wrappedSeconds = std::fmod (secondsSinceMidnight, secondsPerDay);
    if (wrappedSeconds < 0.0)
        wrappedSeconds += secondsPerDay;

    if (rate == FrameRate::fps2997Drop)
    {
        constexpr auto exactFps = 30000.0 / 1001.0;
        const auto totalFrames = static_cast<int64_t> (wrappedSeconds * exactFps + 1.0e-9);
        constexpr int64_t framesPerTenMinutes = 17982;
        constexpr int64_t framesPerMinute = 1798;

        const auto tenMinuteBlocks = totalFrames / framesPerTenMinutes;
        const auto remainder = totalFrames % framesPerTenMinutes;
        const auto minutesSinceBlock = remainder < 1800 ? 0LL : 1LL + ((remainder - 1800) / framesPerMinute);
        const auto displayFrameNumber = totalFrames + (18LL * tenMinuteBlocks) + (2LL * minutesSinceBlock);

        return Timecode {
            static_cast<int> ((displayFrameNumber / 108000LL) % 24LL),
            static_cast<int> ((displayFrameNumber / 1800LL) % 60LL),
            static_cast<int> ((displayFrameNumber / 30LL) % 60LL),
            static_cast<int> (displayFrameNumber % 30LL)
        };
    }

    const auto wholeSeconds = static_cast<int64_t> (wrappedSeconds);
    const auto fractionalSeconds = wrappedSeconds - static_cast<double> (wholeSeconds);

    return Timecode {
        static_cast<int> ((wholeSeconds / 3600LL) % 24LL),
        static_cast<int> ((wholeSeconds / 60LL) % 60LL),
        static_cast<int> (wholeSeconds % 60LL),
        static_cast<int> ((fractionalSeconds * effectiveRate (rate)) + 1.0e-9) % displayFramesPerSecond (rate)
    };
}

inline void packFrameBits (const Timecode& tc, FrameRate rate, std::array<uint8_t, 80>& bits)
{
    bits.fill (0);

    const auto frameUnits = tc.frames % 10;
    const auto frameTens = tc.frames / 10;
    const auto secondUnits = tc.seconds % 10;
    const auto secondTens = tc.seconds / 10;
    const auto minuteUnits = tc.minutes % 10;
    const auto minuteTens = tc.minutes / 10;
    const auto hourUnits = tc.hours % 10;
    const auto hourTens = tc.hours / 10;

    bits[0] = static_cast<uint8_t> ((frameUnits >> 0) & 1);
    bits[1] = static_cast<uint8_t> ((frameUnits >> 1) & 1);
    bits[2] = static_cast<uint8_t> ((frameUnits >> 2) & 1);
    bits[3] = static_cast<uint8_t> ((frameUnits >> 3) & 1);
    bits[8] = static_cast<uint8_t> ((frameTens >> 0) & 1);
    bits[9] = static_cast<uint8_t> ((frameTens >> 1) & 1);
    bits[10] = static_cast<uint8_t> (isDropFrame (rate) ? 1 : 0);

    bits[16] = static_cast<uint8_t> ((secondUnits >> 0) & 1);
    bits[17] = static_cast<uint8_t> ((secondUnits >> 1) & 1);
    bits[18] = static_cast<uint8_t> ((secondUnits >> 2) & 1);
    bits[19] = static_cast<uint8_t> ((secondUnits >> 3) & 1);
    bits[24] = static_cast<uint8_t> ((secondTens >> 0) & 1);
    bits[25] = static_cast<uint8_t> ((secondTens >> 1) & 1);
    bits[26] = static_cast<uint8_t> ((secondTens >> 2) & 1);

    bits[32] = static_cast<uint8_t> ((minuteUnits >> 0) & 1);
    bits[33] = static_cast<uint8_t> ((minuteUnits >> 1) & 1);
    bits[34] = static_cast<uint8_t> ((minuteUnits >> 2) & 1);
    bits[35] = static_cast<uint8_t> ((minuteUnits >> 3) & 1);
    bits[40] = static_cast<uint8_t> ((minuteTens >> 0) & 1);
    bits[41] = static_cast<uint8_t> ((minuteTens >> 1) & 1);
    bits[42] = static_cast<uint8_t> ((minuteTens >> 2) & 1);

    bits[48] = static_cast<uint8_t> ((hourUnits >> 0) & 1);
    bits[49] = static_cast<uint8_t> ((hourUnits >> 1) & 1);
    bits[50] = static_cast<uint8_t> ((hourUnits >> 2) & 1);
    bits[51] = static_cast<uint8_t> ((hourUnits >> 3) & 1);
    bits[56] = static_cast<uint8_t> ((hourTens >> 0) & 1);
    bits[57] = static_cast<uint8_t> ((hourTens >> 1) & 1);

    bits[64] = 0; bits[65] = 0; bits[66] = 1; bits[67] = 1;
    bits[68] = 1; bits[69] = 1; bits[70] = 1; bits[71] = 1;
    bits[72] = 1; bits[73] = 1; bits[74] = 1; bits[75] = 1;
    bits[76] = 1; bits[77] = 1; bits[78] = 0; bits[79] = 1;

    auto parityLow = 0;
    for (auto i = 0; i < 27; ++i)
        parityLow += bits[static_cast<size_t> (i)];
    bits[27] = static_cast<uint8_t> ((parityLow & 1) ? 1 : 0);

    auto parityHigh = 0;
    for (auto i = 32; i < 59; ++i)
        parityHigh += bits[static_cast<size_t> (i)];
    bits[59] = static_cast<uint8_t> ((parityHigh & 1) ? 1 : 0);
}

class Generator
{
public:
    void prepare (double newSampleRate)
    {
        sampleRate = juce::jmax (1.0, newSampleRate);
        reset();
    }

    void reset()
    {
        frameBits.fill (0);
        pendingTimecode = {};
        encodedTimecode = {};
        currentBitIndex = 0;
        halfCellIndex = 0;
        samplePositionInHalfBit = 0.0;
        currentLevel = 1.0f;
        needNewFrame = true;
        seeded = false;
        updateTiming();
    }

    void setFrameRate (FrameRate newRate)
    {
        if (currentRate == newRate)
            return;

        currentRate = newRate;
        needNewFrame = true;
        seeded = false;
        updateTiming();
    }

    void setOutputLevelDb (float newLevelDb)
    {
        levelDb = juce::jlimit (-36.0f, 0.0f, newLevelDb);
        amplitude = std::pow (10.0f, levelDb / 20.0f);
    }

    void setPendingTimecode (Timecode tc)
    {
        pendingTimecode = tc;
    }

    template <typename SampleType>
    void render (SampleType* output, int numSamples)
    {
        if (output == nullptr)
            return;

        for (auto i = 0; i < numSamples; ++i)
        {
            if (needNewFrame)
                seedFrame();

            output[i] = static_cast<SampleType> (currentLevel * amplitude);
            samplePositionInHalfBit += 1.0;

            if (samplePositionInHalfBit >= samplesPerHalfBit)
            {
                samplePositionInHalfBit -= samplesPerHalfBit;

                if (halfCellIndex == 0)
                {
                    halfCellIndex = 1;

                    if (frameBits[static_cast<size_t> (currentBitIndex)] != 0)
                        currentLevel = -currentLevel;
                }
                else
                {
                    halfCellIndex = 0;
                    ++currentBitIndex;
                    currentLevel = -currentLevel;

                    if (currentBitIndex >= static_cast<int> (frameBits.size()))
                        needNewFrame = true;
                }
            }
        }
    }

private:
    void updateTiming()
    {
        samplesPerHalfBit = sampleRate / (effectiveRate (currentRate) * 80.0 * 2.0);
    }

    void seedFrame()
    {
        updateTiming();

        if (! seeded)
        {
            encodedTimecode = pendingTimecode;
            seeded = true;
        }
        else
        {
            const auto next = incrementFrame (encodedTimecode, currentRate);
            encodedTimecode = frameDistance (next, pendingTimecode, currentRate) > 1 ? pendingTimecode : next;
        }

        packFrameBits (encodedTimecode, currentRate, frameBits);
        currentBitIndex = 0;
        halfCellIndex = 0;
        samplePositionInHalfBit = 0.0;
        needNewFrame = false;
    }

    std::array<uint8_t, 80> frameBits {};
    Timecode pendingTimecode;
    Timecode encodedTimecode;
    FrameRate currentRate = FrameRate::fps25;
    double sampleRate = 48000.0;
    double samplesPerHalfBit = 1.0;
    double samplePositionInHalfBit = 0.0;
    float levelDb = -18.0f;
    float amplitude = 0.12589254f;
    float currentLevel = 1.0f;
    int currentBitIndex = 0;
    int halfCellIndex = 0;
    bool needNewFrame = true;
    bool seeded = false;
};
}
