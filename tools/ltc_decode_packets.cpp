#include <ltc.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace
{
constexpr std::uint16_t waveFormatPcm = 0x0001;
constexpr std::uint16_t waveFormatFloat = 0x0003;
constexpr std::uint16_t waveFormatExtensible = 0xfffe;
constexpr std::array<std::uint8_t, 14> waveSubFormatTail {
    0x00, 0x00, 0x10, 0x00,
    0x80, 0x00, 0x00, 0xaa,
    0x00, 0x38, 0x9b, 0x71,
    0x00, 0x00
};

enum class SampleEncoding
{
    unsigned8,
    signed16,
    signed24,
    signed32,
    float32,
    float64,
};

struct Options
{
    std::string inputPath;
    std::optional<int> apv;
    std::optional<double> fps;
    std::optional<int> rawSampleRate;
    bool header = true;
    bool rawU8 = false;
    int channel = 0;
    int rawChannels = 1;
};

struct AudioInput
{
    std::string path;
    SampleEncoding encoding = SampleEncoding::unsigned8;
    std::uint64_t dataOffset = 0;
    std::uint64_t dataSize = 0;
    std::uint32_t sampleRate = 0;
    std::uint16_t channels = 0;
    std::uint16_t bitsPerSample = 0;
    std::uint16_t blockAlign = 0;
};

enum class ParseResult
{
    ok,
    help,
    error,
};

std::uint16_t readLe16 (const std::uint8_t* bytes)
{
    return static_cast<std::uint16_t> (bytes[0])
         | static_cast<std::uint16_t> (bytes[1] << 8);
}

std::uint32_t readLe32 (const std::uint8_t* bytes)
{
    return static_cast<std::uint32_t> (bytes[0])
         | (static_cast<std::uint32_t> (bytes[1]) << 8)
         | (static_cast<std::uint32_t> (bytes[2]) << 16)
         | (static_cast<std::uint32_t> (bytes[3]) << 24);
}

bool readExact (std::istream& stream, void* destination, std::size_t byteCount)
{
    stream.read (static_cast<char*> (destination), static_cast<std::streamsize> (byteCount));
    return stream.good() || stream.gcount() == static_cast<std::streamsize> (byteCount);
}

void printUsage (const char* executable)
{
    std::cerr
        << "Usage:\n"
        << "  " << executable << " <input.wav> [--channel N] [--fps RATE | --apv COUNT] [--no-header]\n"
        << "  " << executable << " <input.raw> --raw-u8 --sample-rate HZ [--channels N] [--channel N]"
           " [--fps RATE | --apv COUNT] [--no-header]\n\n"
        << "Output columns:\n"
        << "  packet,start_sample,end_sample,reverse,drop_frame,timecode,raw_ltc_hex,volume_dbfs,sample_min,sample_max\n";
}

std::optional<int> parsePositiveInt (const std::string& text)
{
    try
    {
        const auto value = std::stoi (text);
        if (value > 0)
            return value;
    }
    catch (...)
    {
    }

    return std::nullopt;
}

std::optional<int> parseNonNegativeInt (const std::string& text)
{
    try
    {
        const auto value = std::stoi (text);
        if (value >= 0)
            return value;
    }
    catch (...)
    {
    }

    return std::nullopt;
}

std::optional<double> parsePositiveDouble (const std::string& text)
{
    try
    {
        const auto value = std::stod (text);
        if (std::isfinite (value) && value > 0.0)
            return value;
    }
    catch (...)
    {
    }

    return std::nullopt;
}

ParseResult parseArguments (int argc, char** argv, Options& options)
{
    for (auto i = 1; i < argc; ++i)
    {
        const std::string argument { argv[i] };

        if (argument == "--help" || argument == "-h")
            return ParseResult::help;

        if (argument == "--no-header")
        {
            options.header = false;
            continue;
        }

        if (argument == "--raw-u8")
        {
            options.rawU8 = true;
            continue;
        }

        if (argument == "--channel" && i + 1 < argc)
        {
            options.channel = parseNonNegativeInt (argv[++i]).value_or (-1);
            continue;
        }

        if (argument == "--channels" && i + 1 < argc)
        {
            options.rawChannels = parsePositiveInt (argv[++i]).value_or (-1);
            continue;
        }

        if (argument == "--sample-rate" && i + 1 < argc)
        {
            options.rawSampleRate = parsePositiveInt (argv[++i]);
            continue;
        }

        if (argument == "--apv" && i + 1 < argc)
        {
            options.apv = parsePositiveInt (argv[++i]);
            continue;
        }

        if (argument == "--fps" && i + 1 < argc)
        {
            options.fps = parsePositiveDouble (argv[++i]);
            continue;
        }

        if (! argument.empty() && argument.front() == '-')
            return ParseResult::error;

        if (! options.inputPath.empty())
            return ParseResult::error;

        options.inputPath = argument;
    }

    if (options.inputPath.empty())
        return ParseResult::error;

    if (options.apv.has_value() && options.fps.has_value())
        return ParseResult::error;

    if (options.channel < 0 || options.rawChannels <= 0)
        return ParseResult::error;

    if (options.rawU8)
    {
        if (! options.rawSampleRate.has_value())
            return ParseResult::error;
    }
    else if (options.rawSampleRate.has_value())
    {
        return ParseResult::error;
    }

    return ParseResult::ok;
}

std::optional<SampleEncoding> toEncoding (std::uint16_t formatTag, std::uint16_t bitsPerSample)
{
    if (formatTag == waveFormatPcm)
    {
        switch (bitsPerSample)
        {
            case 8:  return SampleEncoding::unsigned8;
            case 16: return SampleEncoding::signed16;
            case 24: return SampleEncoding::signed24;
            case 32: return SampleEncoding::signed32;
            default: return std::nullopt;
        }
    }

    if (formatTag == waveFormatFloat)
    {
        switch (bitsPerSample)
        {
            case 32: return SampleEncoding::float32;
            case 64: return SampleEncoding::float64;
            default: return std::nullopt;
        }
    }

    return std::nullopt;
}

bool loadRawInput (const Options& options, AudioInput& input, std::string& error)
{
    std::ifstream stream (options.inputPath, std::ios::binary);
    if (! stream)
    {
        error = "Unable to open input file: " + options.inputPath;
        return false;
    }

    stream.seekg (0, std::ios::end);
    const auto fileSize = stream.tellg();

    if (fileSize < 0)
    {
        error = "Unable to determine raw input size: " + options.inputPath;
        return false;
    }

    input.path = options.inputPath;
    input.encoding = SampleEncoding::unsigned8;
    input.dataOffset = 0;
    input.dataSize = static_cast<std::uint64_t> (fileSize);
    input.sampleRate = static_cast<std::uint32_t> (*options.rawSampleRate);
    input.channels = static_cast<std::uint16_t> (options.rawChannels);
    input.bitsPerSample = 8;
    input.blockAlign = static_cast<std::uint16_t> (options.rawChannels);
    return true;
}

bool loadWaveInput (const Options& options, AudioInput& input, std::string& error)
{
    std::ifstream stream (options.inputPath, std::ios::binary);
    if (! stream)
    {
        error = "Unable to open input file: " + options.inputPath;
        return false;
    }

    std::array<std::uint8_t, 12> header {};
    if (! readExact (stream, header.data(), header.size()))
    {
        error = "Input is too short to be a RIFF/WAVE file: " + options.inputPath;
        return false;
    }

    if (std::memcmp (header.data(), "RIFF", 4) != 0 || std::memcmp (header.data() + 8, "WAVE", 4) != 0)
    {
        error = "Only RIFF/WAVE inputs are supported: " + options.inputPath;
        return false;
    }

    bool foundFmt = false;
    bool foundData = false;
    std::uint16_t formatTag = 0;
    std::uint16_t channels = 0;
    std::uint32_t sampleRate = 0;
    std::uint16_t bitsPerSample = 0;
    std::uint16_t blockAlign = 0;

    while (stream.good() && ! (foundFmt && foundData))
    {
        std::array<std::uint8_t, 8> chunkHeader {};
        if (! readExact (stream, chunkHeader.data(), chunkHeader.size()))
            break;

        const auto chunkSize = readLe32 (chunkHeader.data() + 4);
        const auto chunkDataOffset = stream.tellg();
        if (chunkDataOffset < 0)
        {
            error = "Failed while parsing wave chunks.";
            return false;
        }

        if (std::memcmp (chunkHeader.data(), "fmt ", 4) == 0)
        {
            if (chunkSize < 16)
            {
                error = "Unsupported WAVE fmt chunk size.";
                return false;
            }

            std::vector<std::uint8_t> fmtChunk (chunkSize);
            if (! readExact (stream, fmtChunk.data(), fmtChunk.size()))
            {
                error = "Failed to read WAVE fmt chunk.";
                return false;
            }

            formatTag = readLe16 (fmtChunk.data());
            channels = readLe16 (fmtChunk.data() + 2);
            sampleRate = readLe32 (fmtChunk.data() + 4);
            blockAlign = readLe16 (fmtChunk.data() + 12);
            bitsPerSample = readLe16 (fmtChunk.data() + 14);

            if (formatTag == waveFormatExtensible)
            {
                if (chunkSize < 40)
                {
                    error = "Unsupported WAVE_FORMAT_EXTENSIBLE fmt chunk.";
                    return false;
                }

                const auto* subFormat = fmtChunk.data() + 24;
                if (std::memcmp (subFormat + 2, waveSubFormatTail.data(), waveSubFormatTail.size()) != 0)
                {
                    error = "Unsupported WAVE extensible sub-format.";
                    return false;
                }

                formatTag = readLe16 (subFormat);
            }

            foundFmt = true;
        }
        else if (std::memcmp (chunkHeader.data(), "data", 4) == 0)
        {
            input.dataOffset = static_cast<std::uint64_t> (chunkDataOffset);
            input.dataSize = chunkSize;
            foundData = true;
        }

        const auto nextChunk = static_cast<std::uint64_t> (chunkDataOffset)
                             + static_cast<std::uint64_t> (chunkSize)
                             + static_cast<std::uint64_t> (chunkSize & 1u);
        stream.seekg (static_cast<std::streamoff> (nextChunk), std::ios::beg);
    }

    if (! foundFmt || ! foundData)
    {
        error = "WAVE input must contain both fmt and data chunks.";
        return false;
    }

    const auto encoding = toEncoding (formatTag, bitsPerSample);
    if (! encoding.has_value())
    {
        error = "Unsupported WAVE sample format for LTC decode.";
        return false;
    }

    input.path = options.inputPath;
    input.encoding = *encoding;
    input.sampleRate = sampleRate;
    input.channels = channels;
    input.bitsPerSample = bitsPerSample;
    input.blockAlign = blockAlign;
    return true;
}

bool loadInput (const Options& options, AudioInput& input, std::string& error)
{
    if (options.rawU8)
        return loadRawInput (options, input, error);

    return loadWaveInput (options, input, error);
}

float decodeSample (const std::uint8_t* sampleBytes, SampleEncoding encoding)
{
    switch (encoding)
    {
        case SampleEncoding::unsigned8:
            return (static_cast<float> (sampleBytes[0]) - 128.0f) / 128.0f;

        case SampleEncoding::signed16:
        {
            const auto value = static_cast<std::int16_t> (
                static_cast<std::uint16_t> (sampleBytes[0])
              | static_cast<std::uint16_t> (sampleBytes[1] << 8));
            return static_cast<float> (static_cast<double> (value) / 32768.0);
        }

        case SampleEncoding::signed24:
        {
            auto value = static_cast<std::int32_t> (sampleBytes[0])
                       | (static_cast<std::int32_t> (sampleBytes[1]) << 8)
                       | (static_cast<std::int32_t> (sampleBytes[2]) << 16);

            if ((value & 0x00800000) != 0)
                value |= ~0x00ffffff;

            return static_cast<float> (static_cast<double> (value) / 8388608.0);
        }

        case SampleEncoding::signed32:
        {
            const auto value = static_cast<std::int32_t> (
                static_cast<std::uint32_t> (sampleBytes[0])
              | (static_cast<std::uint32_t> (sampleBytes[1]) << 8)
              | (static_cast<std::uint32_t> (sampleBytes[2]) << 16)
              | (static_cast<std::uint32_t> (sampleBytes[3]) << 24));
            return static_cast<float> (static_cast<double> (value) / 2147483648.0);
        }

        case SampleEncoding::float32:
        {
            float value = 0.0f;
            std::memcpy (&value, sampleBytes, sizeof (value));
            return std::isfinite (value) ? std::clamp (value, -1.0f, 1.0f) : 0.0f;
        }

        case SampleEncoding::float64:
        {
            double value = 0.0;
            std::memcpy (&value, sampleBytes, sizeof (value));
            if (! std::isfinite (value))
                return 0.0f;

            return static_cast<float> (std::clamp (value, -1.0, 1.0));
        }
    }

    return 0.0f;
}

void convertChannelToFloat (const std::vector<std::uint8_t>& interleaved,
                            std::size_t frameCount,
                            const AudioInput& input,
                            int channel,
                            std::vector<float>& output)
{
    const auto bytesPerSample = static_cast<std::size_t> (input.bitsPerSample / 8);

    for (std::size_t frameIndex = 0; frameIndex < frameCount; ++frameIndex)
    {
        const auto sampleOffset = frameIndex * input.blockAlign
                                + static_cast<std::size_t> (channel) * bytesPerSample;
        output[frameIndex] = decodeSample (interleaved.data() + sampleOffset, input.encoding);
    }
}

std::string formatTimecode (const LTCFrameExt& frame)
{
    SMPTETimecode timecode {};
    auto rawFrame = frame.ltc;
    ltc_frame_to_time (&timecode, &rawFrame, 0);

    std::ostringstream stream;
    stream << std::setfill ('0')
           << std::setw (2) << static_cast<int> (timecode.hours) << ':'
           << std::setw (2) << static_cast<int> (timecode.mins) << ':'
           << std::setw (2) << static_cast<int> (timecode.secs)
           << (frame.ltc.dfbit != 0 ? ';' : ':')
           << std::setw (2) << static_cast<int> (timecode.frame);
    return stream.str();
}

std::string formatRawFrameHex (const LTCFrameExt& frame)
{
    const std::array<std::uint8_t, 10> bytes {
        static_cast<std::uint8_t> ((frame.ltc.frame_units & 0x0f) | ((frame.ltc.user1 & 0x0f) << 4)),
        static_cast<std::uint8_t> ((frame.ltc.frame_tens & 0x03)
                                 | ((frame.ltc.dfbit & 0x01) << 2)
                                 | ((frame.ltc.col_frame & 0x01) << 3)
                                 | ((frame.ltc.user2 & 0x0f) << 4)),
        static_cast<std::uint8_t> ((frame.ltc.secs_units & 0x0f) | ((frame.ltc.user3 & 0x0f) << 4)),
        static_cast<std::uint8_t> ((frame.ltc.secs_tens & 0x07)
                                 | ((frame.ltc.biphase_mark_phase_correction & 0x01) << 3)
                                 | ((frame.ltc.user4 & 0x0f) << 4)),
        static_cast<std::uint8_t> ((frame.ltc.mins_units & 0x0f) | ((frame.ltc.user5 & 0x0f) << 4)),
        static_cast<std::uint8_t> ((frame.ltc.mins_tens & 0x07)
                                 | ((frame.ltc.binary_group_flag_bit0 & 0x01) << 3)
                                 | ((frame.ltc.user6 & 0x0f) << 4)),
        static_cast<std::uint8_t> ((frame.ltc.hours_units & 0x0f) | ((frame.ltc.user7 & 0x0f) << 4)),
        static_cast<std::uint8_t> ((frame.ltc.hours_tens & 0x03)
                                 | ((frame.ltc.binary_group_flag_bit1 & 0x01) << 2)
                                 | ((frame.ltc.binary_group_flag_bit2 & 0x01) << 3)
                                 | ((frame.ltc.user8 & 0x0f) << 4)),
        static_cast<std::uint8_t> (frame.ltc.sync_word & 0x00ff),
        static_cast<std::uint8_t> ((frame.ltc.sync_word >> 8) & 0x00ff),
    };

    std::ostringstream stream;
    stream << std::hex << std::setfill ('0');

    for (const auto byte : bytes)
        stream << std::setw (2) << static_cast<unsigned int> (byte);

    return stream.str();
}

int computeApv (const Options& options, const AudioInput& input)
{
    if (options.apv.has_value())
        return *options.apv;

    const auto fps = options.fps.value_or (25.0);
    return std::max (1, static_cast<int> (std::lround (static_cast<double> (input.sampleRate) / fps)));
}

bool dumpPackets (const Options& options, const AudioInput& input, std::string& error)
{
    if (input.channels == 0 || input.sampleRate == 0 || input.blockAlign == 0)
    {
        error = "Input format is incomplete.";
        return false;
    }

    if (options.channel >= input.channels)
    {
        std::ostringstream message;
        message << "Requested channel " << options.channel
                << " but input only has " << input.channels << " channel(s).";
        error = message.str();
        return false;
    }

    std::ifstream stream (input.path, std::ios::binary);
    if (! stream)
    {
        error = "Unable to reopen input file: " + input.path;
        return false;
    }

    stream.seekg (static_cast<std::streamoff> (input.dataOffset), std::ios::beg);

    auto* decoder = ltc_decoder_create (computeApv (options, input), 256);
    if (decoder == nullptr)
    {
        error = "Failed to create libltc decoder.";
        return false;
    }

    if (options.header)
        std::cout << "packet,start_sample,end_sample,reverse,drop_frame,timecode,raw_ltc_hex,volume_dbfs,sample_min,sample_max\n";

    constexpr std::size_t blockFrames = 4096;
    const auto byteCapacity = blockFrames * input.blockAlign;
    std::vector<std::uint8_t> interleaved (byteCapacity);
    std::vector<float> channelSamples (blockFrames);

    const auto totalFrames = input.dataSize / input.blockAlign;
    const auto trailingBytes = input.dataSize % input.blockAlign;
    std::uint64_t framesConsumed = 0;
    std::size_t packetIndex = 0;

    while (framesConsumed < totalFrames)
    {
        const auto framesThisPass = static_cast<std::size_t> (
            std::min<std::uint64_t> (blockFrames, totalFrames - framesConsumed));
        const auto bytesThisPass = framesThisPass * input.blockAlign;

        if (! readExact (stream, interleaved.data(), bytesThisPass))
        {
            error = "Failed while reading audio data.";
            ltc_decoder_free (decoder);
            return false;
        }

        convertChannelToFloat (interleaved, framesThisPass, input, options.channel, channelSamples);
        ltc_decoder_write_float (decoder, channelSamples.data(), framesThisPass, static_cast<ltc_off_t> (framesConsumed));

        LTCFrameExt frame {};
        while (ltc_decoder_read (decoder, &frame) != 0)
        {
            std::cout << packetIndex++ << ','
                      << frame.off_start << ','
                      << frame.off_end << ','
                      << frame.reverse << ','
                      << frame.ltc.dfbit << ','
                      << formatTimecode (frame) << ','
                      << formatRawFrameHex (frame) << ','
                      << std::fixed << std::setprecision (3) << frame.volume << ','
                      << static_cast<unsigned int> (frame.sample_min) << ','
                      << static_cast<unsigned int> (frame.sample_max) << '\n';
        }

        framesConsumed += framesThisPass;
    }

    LTCFrameExt frame {};
    while (ltc_decoder_read (decoder, &frame) != 0)
    {
        std::cout << packetIndex++ << ','
                  << frame.off_start << ','
                  << frame.off_end << ','
                  << frame.reverse << ','
                  << frame.ltc.dfbit << ','
                  << formatTimecode (frame) << ','
                  << formatRawFrameHex (frame) << ','
                  << std::fixed << std::setprecision (3) << frame.volume << ','
                  << static_cast<unsigned int> (frame.sample_min) << ','
                  << static_cast<unsigned int> (frame.sample_max) << '\n';
    }

    ltc_decoder_free (decoder);

    if (packetIndex == 0)
    {
        std::ostringstream message;
        message << "No LTC packets decoded.";

        if (! options.apv.has_value() && ! options.fps.has_value())
            message << " Try --fps 23.976, 24, 25, 29.97, or 30 if the first packets are missing.";

        error = message.str();
        return false;
    }

    if (trailingBytes != 0)
    {
        std::cerr << "Warning: ignored " << trailingBytes << " trailing byte(s) after the final whole sample frame.\n";
    }

    return true;
}
}

int main (int argc, char** argv)
{
    Options options;
    const auto parseResult = parseArguments (argc, argv, options);

    if (parseResult == ParseResult::help)
    {
        printUsage (argv[0]);
        return 0;
    }

    if (parseResult == ParseResult::error)
    {
        printUsage (argv[0]);
        return 1;
    }

    AudioInput input;
    std::string error;

    if (! loadInput (options, input, error))
    {
        std::cerr << error << '\n';
        return 1;
    }

    if (! dumpPackets (options, input, error))
    {
        std::cerr << error << '\n';
        return 1;
    }

    return 0;
}
