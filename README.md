# Chop Shop

A hard & soft clipper plugin (VST3 / AU) by **Castle Danger Music Company**.

- **Modes:** MASTER (transparent) and AGGRESSIVE (drums & tracks)
- **Color:** a dial that adds harmonic saturation to either mode (0 = none, 100 = full)
- **Clip shape:** HARD or SOFT in both modes
- **Extras:** oversampling (off / 2x / 4x / 8x, latency reported to the host), MIX for parallel clipping, GAIN MATCH, SCRAPS (listen to just what's chopped off), live transfer-curve display, IN / CHOP / OUT meters, factory + user presets

See `controls.txt` for every control and `readme.txt` for installing.

## Building

Pushing to `main` runs GitHub Actions, which builds macOS (VST3 + AU, universal) and Windows (VST3), then:

- uploads the builds as workflow artifacts, and
- pushes the release zips to the `downloads-macos` / `downloads-windows` branches.

A Linux job runs the headless checks (`test/snapshot.cpp`): DSP tests (ceiling, transparency, COLOR in both modes, SCRAPS, latency, presets) and renders the editor to PNGs, uploaded as the `gui-snapshots` artifact.

Local build:

```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --target ChopShop_VST3
```

Built with JUCE 8.0.15 (same as DREAD, Possessed and BuzzKill).
