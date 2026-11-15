#pragma once
#include <juce_core/juce_core.h>
#include <vector>
#include <utility>

namespace chop
{
struct Preset { juce::String name; std::vector<std::pair<juce::String, float>> values; };

// mode: 0 Master, 1 Aggressive | color: 0-100 in either mode | shape: 0 Hard, 1 Soft | oversample: 0 Off, 1 2x, 2 4x, 3 8x
inline const std::vector<Preset>& factoryPresets()
{
    static const std::vector<Preset> p {
        { "Init (Transparent)",  { } },
        { "Master: Clean Loud",  { { "mode", 0 }, { "shape", 1 }, { "inputDb", 3.0f },  { "ceilingDb", -0.3f }, { "oversample", 3 } } },
        { "Master: Tape Warmth", { { "mode", 0 }, { "shape", 1 }, { "color", 20 },  { "inputDb", 2.5f }, { "ceilingDb", -0.5f }, { "oversample", 3 } } },
        { "Master: Peak Shaver", { { "mode", 0 }, { "shape", 0 }, { "inputDb", 1.5f },  { "ceilingDb", -1.0f }, { "oversample", 2 } } },
        { "Master: Brickwall",   { { "mode", 0 }, { "shape", 0 }, { "inputDb", 5.0f },  { "ceilingDb", -0.1f }, { "oversample", 3 } } },
        { "Drum Bus Smash",      { { "mode", 1 }, { "shape", 0 }, { "inputDb", 6.0f },  { "ceilingDb", -1.0f }, { "oversample", 1 } } },
        { "Snare Crack",         { { "mode", 1 }, { "shape", 0 }, { "inputDb", 9.0f },  { "ceilingDb", -3.0f }, { "mix", 70 }, { "color", 15 } } },
        { "Kick Chop",           { { "mode", 1 }, { "shape", 1 }, { "inputDb", 5.0f },  { "ceilingDb", -2.0f } } },
        { "Parallel Crunch",     { { "mode", 1 }, { "shape", 0 }, { "inputDb", 14.0f }, { "mix", 35 }, { "gainMatch", 1 } } },
        { "Guitar Grind",        { { "mode", 1 }, { "shape", 1 }, { "inputDb", 8.0f },  { "ceilingDb", -4.0f }, { "gainMatch", 1 } } },
        { "Warm Glue",           { { "mode", 0 }, { "shape", 1 }, { "color", 35 },  { "inputDb", 2.0f } } },
        { "Vocal Edge",          { { "mode", 0 }, { "shape", 1 }, { "color", 25 },  { "inputDb", 3.0f }, { "ceilingDb", -2.0f } } },
        { "808 Grit",            { { "mode", 1 }, { "shape", 1 }, { "color", 60 },  { "inputDb", 5.0f }, { "ceilingDb", -1.0f } } },
        { "Rust Bucket",         { { "mode", 1 }, { "shape", 0 }, { "color", 100 }, { "inputDb", 10.0f }, { "mix", 80 }, { "gainMatch", 1 } } },
    };
    return p;
}
} // namespace chop
