# Timecode LTC Research

## Scope Answers

- Initial format target: `AAX only`
- First platforms to support: `macOS`, `Windows`, and local development on `Linux`
- v1 behavior: `continuous LTC tone`
- Time source: `host transport`
- Stopped output: `silence`
- Output layout: `mono only`

## Chosen Stack

- Plugin framework: `JUCE`
- AAX path: JUCE's `AAX` wrapper support
- LTC generation: small internal encoder in project code
- State/parameters: `juce::AudioProcessorValueTreeState`
- Editor: `juce::GenericAudioProcessorEditor`

## Why This Stack

### JUCE over iPlug2

JUCE is the best fit for this repository because it gives the shortest path to a maintainable AAX plugin with a very small amount of project code.

Reference points used:

- `references/JUCE/examples/CMake/AudioPlugin/CMakeLists.txt`
- `references/JUCE/docs/CMake API.md`
- `references/JUCE/extras/Build/CMake/JUCEUtils.cmake`
- `references/JUCE/extras/Build/CMake/JUCEModuleSupport.cmake`

The exact JUCE interfaces/config we are using are:

- `juce_add_plugin(... FORMATS AAX ...)`
- `AAX_CATEGORY SWGenerators`
- `juce::AudioProcessor`
- `juce::AudioProcessorValueTreeState`
- `juce::AudioParameterChoice`
- `juce::AudioParameterBool`
- `juce::AudioParameterFloat`
- `juce::GenericAudioProcessorEditor`
- `juce::AudioPlayHead::getPosition()`
- `juce::AudioPlayHead::PositionInfo::getTimeInSamples()`
- `juce::AudioPlayHead::PositionInfo::getEditOriginTime()`
- `juce::AudioPlayHead::PositionInfo::getFrameRate()`
- `juce::AudioPlayHead::PositionInfo::getIsPlaying()`

Those choices line up directly with the JUCE example/plugin client code in the reference checkout.

### Internal LTC encoder over libltc

`libltc` is technically strong, but it is the wrong default choice for this plugin.

Reference points used:

- `references/libltc/src/ltc.h`
- `references/libltc/tests/example_encode.c`
- `references/SuperTimecodeConverter/LtcOutput.h`
- `references/SuperTimecodeConverter/TimecodeCore.h`
- `references/openltc/README.md`

Why we are not embedding `libltc` here:

- It is `LGPL-3.0+`, which is avoidable for an encode-only plugin.
- The plugin only needs a narrow subset of LTC functionality: pack frame bits, handle drop-frame correctly, and emit biphase-mark audio.
- The reference code in `SuperTimecodeConverter` shows that a small encoder is enough for a real-time generator.

The internal encoder therefore uses the exact concepts validated by the references:

- 80-bit LTC frame packing
- SMPTE BCD time fields
- `dfbit` for `29.97 DF`
- sync word `0011 1111 1111 1101`
- biphase-mark transitions
- explicit `23.976`, `24`, `25`, `29.97`, `29.97 DF`, and `30` handling

## Host Time Integration

For AAX/Pro Tools via JUCE, the key host timing path is already present in JUCE's AAX wrapper:

- `references/JUCE/modules/juce_audio_plugin_client/juce_audio_plugin_client_AAX.cpp`

That file shows JUCE filling `PositionInfo` from the AAX transport with:

- transport playing state
- timeline sample position
- frame rate
- edit origin time

That is why the plugin implementation uses:

1. `getPlayHead()->getPosition()`
2. `getTimeInSamples()` for the current playhead location
3. `getEditOriginTime()` so LTC can follow the session timecode origin instead of always starting at `00:00:00:00`
4. `getFrameRate()` when `Use Host Rate` is enabled

## Bus/Layout Decision

The scaffold uses a mono input/output effect-style layout rather than a synth-only output bus.

Why:

- it is simple to insert on a normal mono track
- it still generates its own signal
- it keeps the initial plugin shape minimal

The implementation only accepts:

- mono output
- mono input or disabled input

## AAX Platform Constraint

JUCE's CMake support only emits `AAX` plugin wrapper targets on:

- `macOS`
- `Windows x86_64`

This is visible in:

- `references/JUCE/extras/Build/CMake/JUCEModuleSupport.cmake`

That means Linux can build the shared plugin code target, but not a real `.aaxplugin` bundle.

## Checked-Out References

The following reference copies were checked out under `references/`:

- `references/JUCE`
- `references/iPlug2`
- `references/libltc`
- `references/openltc`
- `references/SuperTimecodeConverter`
