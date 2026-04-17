# timecode-vst

## Dirs

- `src/`: plugin source
- `docs/`: research notes and implementation decisions
- `references/`: local upstream reference checkouts for analysis only; ignored by git
- `build/`: generated build tree
- `build-ninja/`: generated Ninja build tree

## Build

Configure with local JUCE checkout:

```bash
cmake -B build-ninja -G Ninja -DJUCE_SOURCE_DIR="/workspace/timecode-vst/references/JUCE"
```

Build only the standalone `libltc` decoder utility on Debian-based systems:

```bash
apt-get install -y libltc-dev
cmake -B build-ltc-util -G Ninja \
  -DTIMECODE_LTC_BUILD_PLUGIN=OFF \
  -DTIMECODE_LTC_BUILD_LIBLTC_UTIL=ON
```

Build:

```bash
cmake --build build-ninja
```

Build just the decoder utility:

```bash
cmake --build build-ltc-util --target ltc_decode_packets
```

Build just the Linux VST3 target:

```bash
cmake --build build-ninja --target TimecodeLTC_VST3
```

Clean build dir:

```bash
rm -rf build-ninja
```

## Decoder Utility

`ltc_decode_packets` is a standalone `libltc`-based CLI for dumping every decoded LTC packet as CSV for comparison work.

Decode a `.wav` file:

```bash
./build-ltc-util/ltc_decode_packets capture.wav --channel 0 --fps 25
```

Decode raw unsigned 8-bit mono data such as the upstream `libltc` test encoder output:

```bash
./build-ltc-util/ltc_decode_packets capture.raw --raw-u8 --sample-rate 48000 --fps 25
```

CSV columns:

- `packet`
- `start_sample`
- `end_sample`
- `reverse`
- `drop_frame`
- `timecode`
- `raw_ltc_hex`
- `volume_dbfs`
- `sample_min`
- `sample_max`

## Notes

- This repo builds `VST3` everywhere and `AAX` only on supported macOS/Windows builds.
- On Linux, the main test artifact is `build-ninja/TimecodeLTC_artefacts/VST3/LTC Generator.vst3`.
