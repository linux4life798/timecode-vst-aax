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

Build:

```bash
cmake --build build-ninja
```

Build just the Linux VST3 target:

```bash
cmake --build build-ninja --target TimecodeLTC_VST3
```

Clean build dir:

```bash
rm -rf build-ninja
```

## Notes

- This repo builds `VST3` everywhere and `AAX` only on supported macOS/Windows builds.
- On Linux, the main test artifact is `build-ninja/TimecodeLTC_artefacts/VST3/LTC Generator.vst3`.
