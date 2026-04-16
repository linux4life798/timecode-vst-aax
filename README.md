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

Clean build dir:

```bash
rm -rf build-ninja
```

## Notes

- This repo currently targets `AAX` in CMake.
- On Linux, JUCE builds the shared plugin code, not a real `.aaxplugin` bundle.
