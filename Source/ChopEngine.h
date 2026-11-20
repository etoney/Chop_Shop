#pragma once
// CUTTHROAT clipper engine.
//   Modes:  MASTER     - transparent clipper (pure hard clip / gentle soft knee)
//           AGGRESSIVE - crunchy, dense clipping for drums and tracks
//   Shape:  HARD or SOFT, available in both modes.
//   Color:  a dial that adds harmonic saturation in either mode
//           (0 = no colour at all, 100 = full colour).
//   Extras: oversampling (off/2x/4x/8x), MIX (parallel clipping), GAIN MATCH,
//           SCRAPS (listen to only what the clipper chops off), and a final
//           safety clamp so the output never goes past the CEILING.
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <cmath>

namespace chop
{
enum Mode  { master = 0, aggressive = 1 };
enum Shape { hard = 0, soft = 1 };

// Gentle soft knee: linear up to `knee`, then bends smoothly into 1.0 (slope stays continuous).
inline float softKnee (float u, float knee)
{
    const float a = std::abs (u);
    if (a <= knee) return u;
    const float r = 1.0f - knee;
    return std::copysign (knee + r * std::tanh ((a - knee) / r), u);
}

// Harmonic saturator used by the COLOR dial (runs before the clipper in both modes). Unity gain for quiet signals, rounds off
// peaks, slight asymmetry for even (2nd) harmonics. c = 0 is exactly a straight wire.
inline float colorSat (float u, float c)
{
    if (c <= 0.0f) return u;
    const float k = 1.0f + 2.2f * c;           // drive into the saturator
    const float b = 0.28f * c;                 // bias -> even harmonics
    const float tb = std::tanh (b);
    const float norm = 1.0f / (k * (1.0f - tb * tb));
    const float s = (std::tanh (k * u + b) - tb) * norm;
    const float w = 0.75f * c;                 // how much of the saturator we hear
    return u + w * (s - u);
}

// The whole transfer curve, relative to the ceiling (u = 1.0 is the ceiling).
// Used by the audio path and by the curve display in the editor.
inline float shapeSample (float u, int mode, int shape, float colorAmt)
{
    const float c = colorSat (u, colorAmt);   // COLOR first, in either mode
    if (mode == aggressive)
    {
        // pre-drive grit: odd + a little even harmonics, then clip harder/earlier
        const float v = juce::jlimit (-2.0f, 2.0f, c);
        const float g = v + 0.22f * v * std::abs (v) + 0.07f * v * v;
        return shape == hard ? juce::jlimit (-1.0f, 1.0f, g) : softKnee (g, 0.45f);
    }
    // master: transparent
    return shape == hard ? juce::jlimit (-1.0f, 1.0f, c) : softKnee (c, 0.85f);
}

struct Params
{
    int mode = master, shape = hard, oversample = 2; // 0 off, 1 2x, 2 4x, 3 8x
    float inputDb = 0.0f, ceilingDb = 0.0f, color = 0.0f /*0..1*/, mix = 1.0f /*0..1*/, outputDb = 0.0f;
    bool gainMatch = false, scraps = false;
};

class Engine
{
public:
    static constexpr int maxCh = 2;
    static constexpr int kChunk = 1024; // internal processing chunk

    void prepare (double sampleRate, int maxBlock)
    {
        sr = sampleRate;
        blockMax = juce::jlimit (1, kChunk, maxBlock);
        for (int f = 0; f < 4; ++f)
        {
            if (f == 0) { os[0].reset(); continue; }
            os[(size_t) f] = std::make_unique<juce::dsp::Oversampling<float>> (
                (size_t) maxCh, (size_t) f, juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, true);
            os[(size_t) f]->initProcessing ((size_t) blockMax);
        }
        maxLatency = (int) std::lround (os[3]->getLatencyInSamples()) + 4;
        dry.setSize (maxCh, blockMax);
        for (auto& d : dryDelay) { d.assign ((size_t) maxLatency + (size_t) blockMax + 8, 0.0f); }
        writePos = 0;
        for (auto* s : { &inGain, &ceil, &colorAmt, &mixAmt, &outGain })
            s->reset (sampleRate, 0.03);
        dcX.fill (0); dcY.fill (0);
        dcCoef = (float) std::exp (-juce::MathConstants<double>::twoPi * 7.0 / sampleRate);
        activeOs = -1;
    }

    int latencyFor (int osIndex) const
    {
        return osIndex <= 0 || os[(size_t) osIndex] == nullptr ? 0 : (int) std::lround (os[(size_t) osIndex]->getLatencyInSamples());
    }
    int getLatency() const { return latencyFor (activeOs < 0 ? 0 : activeOs); }

    void setParams (const Params& p, bool snap = false)
    {
        prm = p;
        const float gm = p.gainMatch ? juce::Decibels::decibelsToGain (-p.inputDb) : 1.0f;
        inGain.setTargetValue (juce::Decibels::decibelsToGain (p.inputDb));
        ceil.setTargetValue (juce::Decibels::decibelsToGain (p.ceilingDb));
        colorAmt.setTargetValue (p.color);
        mixAmt.setTargetValue (p.mix);
        outGain.setTargetValue (juce::Decibels::decibelsToGain (p.outputDb) * gm);
        if (snap) for (auto* s : { &inGain, &ceil, &colorAmt, &mixAmt, &outGain }) s->setCurrentAndTargetValue (s->getTargetValue());
    }

    // Processes numCh (1 or 2) channels in place. Returns true if the latency changed.
    bool process (float* const* ch, int numCh, int n)
    {
        numCh = juce::jlimit (1, maxCh, numCh);
        bool latencyChanged = false;
        if (prm.oversample != activeOs)
        {
            latencyChanged = activeOs >= 0 && latencyFor (prm.oversample) != latencyFor (activeOs);
            activeOs = juce::jlimit (0, 3, prm.oversample);
            if (os[(size_t) activeOs]) os[(size_t) activeOs]->reset();
            for (auto& d : dryDelay) std::fill (d.begin(), d.end(), 0.0f);
            dcX.fill (0); dcY.fill (0);
        }
        if (prm.mode != lastMode) { dcX.fill (0); dcY.fill (0); lastMode = prm.mode; }

        for (int start = 0; start < n; start += blockMax)
            processChunk (ch, numCh, start, std::min (blockMax, n - start));
        return latencyChanged;
    }

    // metering (read from the UI thread)
    std::atomic<float> inPeak { 0 }, outPeak { 0 }, chopDb { 0 }, drivePeak { 0 };

private:
    void processChunk (float* const* ch, int numCh, int off, int n)
    {
        const int lat = getLatency();
        const size_t dl = dryDelay[0].size();

        // 1) keep a latency-aligned copy of the dry input for MIX
        float inPk = 0.0f;
        for (int c = 0; c < numCh; ++c)
        {
            auto* x = ch[c] + off;
            auto* dOut = dry.getWritePointer (c);
            auto& d = dryDelay[(size_t) c];
            size_t w = writePos;
            for (int i = 0; i < n; ++i)
            {
                inPk = std::max (inPk, std::abs (x[i]));
                d[w] = x[i];
                dOut[i] = d[(w + dl - (size_t) lat) % dl];
                w = (w + 1) % dl;
            }
        }
        writePos = (writePos + (size_t) n) % dl;

        // 2) input gain (base rate, smoothed)
        float gIn[kChunk], gCeil[kChunk], gCol[kChunk];
        for (int i = 0; i < n; ++i) { gIn[i] = inGain.getNextValue(); gCeil[i] = ceil.getNextValue(); gCol[i] = colorAmt.getNextValue(); }
        for (int c = 0; c < numCh; ++c)
            for (int i = 0; i < n; ++i) ch[c][off + i] *= gIn[i];

        // 3) clip, oversampled
        float preMax = 0.0f;
        const int mode = prm.mode, shape = prm.shape;
        const bool delta = prm.scraps;
        auto shapeBuf = [&] (float* s, int len, int factor)
        {
            for (int i = 0; i < len; ++i)
            {
                const int bi = std::min (n - 1, i / factor);
                const float cl = gCeil[bi];
                const float u = s[i] / cl;
                preMax = std::max (preMax, std::abs (u));
                const float y = shapeSample (u, mode, shape, gCol[bi]) * cl;
                s[i] = delta ? s[i] - y : y;
            }
        };
        if (activeOs <= 0)
        {
            for (int c = 0; c < numCh; ++c) shapeBuf (ch[c] + off, n, 1);
        }
        else
        {
            float* ptrs[maxCh] = { ch[0] + off, numCh > 1 ? ch[1] + off : ch[0] + off };
            juce::dsp::AudioBlock<float> block (ptrs, (size_t) numCh, (size_t) n);
            auto up = os[(size_t) activeOs]->processSamplesUp (block);
            const int factor = 1 << activeOs;
            for (size_t c = 0; c < up.getNumChannels(); ++c)
                shapeBuf (up.getChannelPointer (c), (int) up.getNumSamples(), factor);
            os[(size_t) activeOs]->processSamplesDown (block);
        }

        // 4) DC blocker (AGGRESSIVE and COLOR create a little DC), safety ceiling, mix, output
        const bool dcOn = mode != master || prm.color > 0.0f;
        float outPk = 0.0f;
        std::array<float, kChunk> gMix, gOut;
        for (int i = 0; i < n; ++i) { gMix[(size_t) i] = mixAmt.getNextValue(); gOut[(size_t) i] = outGain.getNextValue(); }
        for (int c = 0; c < numCh; ++c)
        {
            auto* y = ch[c] + off;
            const auto* d = dry.getReadPointer (c);
            for (int i = 0; i < n; ++i)
            {
                float v = y[i];
                if (dcOn)
                {
                    const float o = v - dcX[(size_t) c] + dcCoef * dcY[(size_t) c];
                    dcX[(size_t) c] = v; dcY[(size_t) c] = o; v = o;
                }
                if (! delta) v = juce::jlimit (-gCeil[i], gCeil[i], v);   // guaranteed ceiling
                const float m = gMix[(size_t) i];
                // SCRAPS: only what was chopped off. Otherwise blend the dry signal back in
                // (with GAIN MATCH the dry is lifted by the input gain too, so the
                // output stage brings both back to the original level).
                const float out = delta ? v * gOut[(size_t) i]
                                        : (d[i] * (1.0f - m) * (prm.gainMatch ? gIn[i] : 1.0f) + v * m) * gOut[(size_t) i];
                y[i] = out;
                outPk = std::max (outPk, std::abs (out));
            }
        }

        inPeak.store (std::max (inPeak.load(), inPk));
        outPeak.store (std::max (outPeak.load(), outPk));
        drivePeak.store (std::max (drivePeak.load(), preMax));
        chopDb.store (std::max (chopDb.load(), preMax > 1.0f ? juce::Decibels::gainToDecibels (preMax) : 0.0f));
    }

    double sr = 44100.0;
    int blockMax = 512, maxLatency = 0, activeOs = -1, lastMode = -1;
    Params prm;
    std::array<std::unique_ptr<juce::dsp::Oversampling<float>>, 4> os;
    juce::AudioBuffer<float> dry;
    std::array<std::vector<float>, maxCh> dryDelay;
    size_t writePos = 0;
    juce::SmoothedValue<float> inGain { 1.0f }, ceil { 1.0f }, colorAmt { 0.0f }, mixAmt { 1.0f }, outGain { 1.0f };
    std::array<float, maxCh> dcX {}, dcY {};
    float dcCoef = 0.999f;
};
} // namespace chop
