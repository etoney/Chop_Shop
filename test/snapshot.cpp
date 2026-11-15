// Headless checks (run locally and in CI):
//  1) renders the editor to gui.png (plus one per mode and the save dialog)
//  2) DSP: ceiling is never exceeded, MASTER is transparent below the ceiling,
//     COLOR 0 == transparent, COLOR adds harmonics, SCRAPS + output == driven input,
//     latency follows the oversampling setting
//  3) user-preset round trip
#include <juce_gui_basics/juce_gui_basics.h>
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"

static bool writePng (const juce::Image& img, const juce::File& f)
{
    f.deleteFile();
    if (auto os = f.createOutputStream()) return juce::PNGImageFormat().writeImageToStream (img, *os);
    return false;
}

static void setP (ChopShopProcessor& p, const char* id, float v)
{
    auto* rp = p.apvts.getParameter (id);
    rp->setValueNotifyingHost (rp->convertTo0to1 (v));
}

// renders a sine through the real processBlock, returns left channel (after the first 0.25 s)
static std::vector<float> renderSine (ChopShopProcessor& p, float amp, float freq, double sr = 48000.0, int blocks = 60)
{
    p.prepareToPlay (sr, 512);
    juce::AudioBuffer<float> buf (2, 512);
    juce::MidiBuffer midi;
    std::vector<float> out;
    double ph = 0;
    for (int b = 0; b < blocks; ++b)
    {
        for (int i = 0; i < 512; ++i)
        {
            const float s = amp * (float) std::sin (ph);
            ph += juce::MathConstants<double>::twoPi * freq / sr;
            buf.setSample (0, i, s); buf.setSample (1, i, s);
        }
        p.processBlock (buf, midi);
        if (b >= 24) out.insert (out.end(), buf.getReadPointer (0), buf.getReadPointer (0) + 512);
    }
    return out;
}

static float peak (const std::vector<float>& v) { float m = 0; for (auto x : v) m = std::max (m, std::abs (x)); return m; }

// THD+N-ish: energy outside the fundamental via a simple DFT at harmonics 2..10
static float harmonicsDb (const std::vector<float>& v, float freq, double sr)
{
    auto mag = [&] (double f)
    {
        double re = 0, im = 0;
        for (size_t i = 0; i < v.size(); ++i)
        {
            const double w = 0.5 - 0.5 * std::cos (juce::MathConstants<double>::twoPi * (double) i / (double) (v.size() - 1));
            re += v[i] * w * std::cos (juce::MathConstants<double>::twoPi * f * (double) i / sr);
            im -= v[i] * w * std::sin (juce::MathConstants<double>::twoPi * f * (double) i / sr);
        }
        return std::sqrt (re * re + im * im);
    };
    const double fund = mag (freq);
    double h = 0;
    for (int k = 2; k <= 10; ++k) { const double m = mag (freq * k); h += m * m; }
    return (float) juce::Decibels::gainToDecibels (std::sqrt (h) / fund, -200.0);
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const juce::File dir (argc > 1 ? juce::File (juce::String (argv[1])) : juce::File::getCurrentWorkingDirectory());
    dir.createDirectory();
    int failures = 0;
    auto check = [&] (bool ok, const juce::String& what) { std::printf ("[%s] %s\n", ok ? "PASS" : "FAIL", what.toRawUTF8()); if (! ok) ++failures; };

    // ---------------- DSP
    {
        ChopShopProcessor p;
        p.setPlayConfigDetails (2, 2, 48000.0, 512);
        check (p.getProgramName (p.getCurrentProgram()) == "Init (Transparent)", "opens on Init (Transparent)");
        check (p.getProgramName (5) == "Drum Bus Smash" && p.getProgramName (12) == "808 Grit", "preset order");

        for (int mode = 0; mode < 2; ++mode)
            for (int shape = 0; shape < 2; ++shape)
                for (int osI : { 0, 2, 3 })
                {
                    p.setCurrentProgram (0);
                    setP (p, "mode", (float) mode); setP (p, "shape", (float) shape); setP (p, "oversample", (float) osI);
                    setP (p, "color", 100.0f); setP (p, "inputDb", 18.0f); setP (p, "ceilingDb", -3.0f);
                    const float pk = peak (renderSine (p, 0.9f, 997.0f));
                    check (pk <= juce::Decibels::decibelsToGain (-3.0f) + 1e-4f,
                           juce::String::formatted ("ceiling holds: mode %d shape %d os %d -> peak %.2f dBFS", mode, shape, osI, juce::Decibels::gainToDecibels (pk)));
                }

        // transparency below the ceiling (MASTER, hard) at every oversampling setting
        for (int osI = 0; osI < 4; ++osI)
        {
            p.setCurrentProgram (0);
            setP (p, "oversample", (float) osI);
            const auto y = renderSine (p, 0.25f, 1000.0f);
            const float h = harmonicsDb (y, 1000.0f, 48000.0);
            check (h < -90.0f && std::abs (juce::Decibels::gainToDecibels (peak (y) / 0.25f)) < 0.05f,
                   juce::String::formatted ("MASTER transparent below ceiling (os %d): harmonics %.1f dB, gain %.3f dB", osI, h, juce::Decibels::gainToDecibels (peak (y) / 0.25f)));
            check (p.getLatencySamples() == p.getEngine().latencyFor (osI), juce::String::formatted ("latency reported for os %d: %d samples", osI, p.getLatencySamples()));
        }

        // COLOR 0 = transparent, more COLOR = more harmonics (MASTER, below the ceiling, so it's the saturator alone)
        float last = -300.0f;
        for (float c : { 0.0f, 25.0f, 50.0f, 75.0f, 100.0f })
        {
            p.setCurrentProgram (0);
            setP (p, "mode", 0); setP (p, "color", c); setP (p, "shape", 1);
            const auto y = renderSine (p, 0.5f, 500.0f);
            const float h = harmonicsDb (y, 500.0f, 48000.0);
            std::printf ("   color %3.0f%% @ -6 dBFS: harmonics %.1f dB, peak %.2f dBFS\n", c, h, juce::Decibels::gainToDecibels (peak (y)));
            if (c == 0.0f) check (h < -90.0f, "COLOR 0 is transparent");
            else check (h > last + 1.0f, juce::String::formatted ("COLOR %.0f adds harmonics", c));
            last = h;
        }

        // COLOR works in AGGRESSIVE too
        {
            auto hA = [&] (float c)
            {
                p.setCurrentProgram (0);
                setP (p, "mode", 1); setP (p, "color", c); setP (p, "shape", 1);
                return harmonicsDb (renderSine (p, 0.3f, 500.0f), 500.0f, 48000.0);
            };
            const float h0 = hA (0.0f), h1 = hA (100.0f);
            check (h1 > h0 + 3.0f, juce::String::formatted ("COLOR adds harmonics in AGGRESSIVE: %.1f -> %.1f dB", h0, h1));
        }

        // AGGRESSIVE is grittier than MASTER at the same drive
        auto hAt = [&] (int mode, int shape)
        {
            p.setCurrentProgram (0);
            setP (p, "mode", (float) mode); setP (p, "shape", (float) shape); setP (p, "inputDb", 6.0f);
            return harmonicsDb (renderSine (p, 0.5f, 500.0f), 500.0f, 48000.0);
        };
        for (int s = 0; s < 2; ++s)
        {
            const float hm = hAt (0, s), ha = hAt (1, s);
            check (ha > hm + 3.0f, juce::String::formatted ("AGGRESSIVE (%s) grittier than MASTER: %.1f vs %.1f dB", s ? "soft" : "hard", ha, hm));
        }

        // SCRAPS: clipped output + scraps == driven input (delayed)
        {
            p.setCurrentProgram (0);
            setP (p, "inputDb", 9.0f); setP (p, "oversample", 0);
            const auto clipped = renderSine (p, 0.8f, 440.0f);
            setP (p, "scraps", 1.0f);
            const auto scraps = renderSine (p, 0.8f, 440.0f);
            setP (p, "scraps", 0.0f);
            setP (p, "ceilingDb", 0.0f); setP (p, "inputDb", 9.0f);
            float err = 0, ref = 0;
            const float g = juce::Decibels::decibelsToGain (9.0f);
            // reference: same render with the sine computed directly (no latency at os 0)
            double ph = juce::MathConstants<double>::twoPi * 440.0 / 48000.0 * 512.0 * 24.0;
            for (size_t i = 0; i < clipped.size(); ++i)
            {
                const float x = 0.8f * g * (float) std::sin (ph);
                ph += juce::MathConstants<double>::twoPi * 440.0 / 48000.0;
                err = std::max (err, std::abs (clipped[i] + scraps[i] - x));
                ref = std::max (ref, std::abs (x));
            }
            check (err < 1e-3f * ref, juce::String::formatted ("output + SCRAPS reconstructs the input (err %.2g)", err));
        }

        // GAIN MATCH keeps quiet signals at unity while INPUT is up
        {
            p.setCurrentProgram (0);
            setP (p, "inputDb", 12.0f); setP (p, "gainMatch", 1.0f);
            const auto y = renderSine (p, 0.05f, 1000.0f);
            check (std::abs (juce::Decibels::gainToDecibels (peak (y) / 0.05f)) < 0.1f, "GAIN MATCH compensates the input gain");
        }

        // factory presets all load and hold their ceiling
        for (int i = 0; i < p.getNumPrograms(); ++i)
        {
            p.setCurrentProgram (i);
            const float ceil = juce::Decibels::decibelsToGain (p.apvts.getRawParameterValue ("ceilingDb")->load());
            const float mix = p.apvts.getRawParameterValue ("mix")->load();
            const float pk = peak (renderSine (p, 0.7f, 120.0f, 48000.0, 40));
            if (mix >= 99.9f && p.apvts.getRawParameterValue ("gainMatch")->load() < 0.5f)
                check (pk <= ceil + 1e-4f, "preset '" + p.getProgramName (i) + "' holds its ceiling");
            else check (std::isfinite (pk) && pk > 0.0f, "preset '" + p.getProgramName (i) + "' renders");
        }

        // user presets: save -> change -> load -> delete
        {
            p.setCurrentProgram (0);
            setP (p, "mode", 1); setP (p, "inputDb", 7.5f);
            check (p.saveUserPreset ("__test chop"), "save user preset");
            const auto f = ChopShopProcessor::fileForPresetName ("__test chop");
            check (f.existsAsFile() && f.getFullPathName().contains ("Castle Danger"), "preset file in the Castle Danger folder: " + f.getFullPathName());
            p.setCurrentProgram (0);
            check (p.loadUserPreset (f) && p.apvts.getRawParameterValue ("mode")->load() == 1.0f
                   && std::abs (p.apvts.getRawParameterValue ("inputDb")->load() - 7.5f) < 0.01f, "load user preset");
            f.deleteFile();
        }
    }

    // ---------------- GUI
    {
        ChopShopProcessor p;
        p.setPlayConfigDetails (2, 2, 48000.0, 512);
        p.setCurrentProgram (5); // Drum Bus Smash
        renderSine (p, 0.9f, 80.0f, 48000.0, 30);
        std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
        // let the meters / saw / curve catch the signal
        for (int i = 0; i < 4; ++i) { renderSine (p, 0.9f, 80.0f, 48000.0, 26); juce::MessageManager::getInstance()->runDispatchLoopUntil (40); }
        check (writePng (ed->createComponentSnapshot (ed->getLocalBounds(), true, 2.0f), dir.getChildFile ("gui.png")), "main snapshot");

        p.setCurrentProgram (12); // 808 Grit (AGGRESSIVE + COLOR)
        for (int i = 0; i < 3; ++i) { renderSine (p, 0.6f, 60.0f, 48000.0, 26); juce::MessageManager::getInstance()->runDispatchLoopUntil (40); }
        check (writePng (ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f), dir.getChildFile ("gui_color.png")), "color snapshot");

        p.setCurrentProgram (0);
        setP (p, "shape", 1); setP (p, "scraps", 1.0f);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (80);
        check (writePng (ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f), dir.getChildFile ("gui_master_soft.png")), "master soft snapshot");

        if (auto* ce = dynamic_cast<ChopShopEditor*> (ed.get())) ce->getSaveButton().triggerClick();
        juce::MessageManager::getInstance()->runDispatchLoopUntil (120);
        check (writePng (ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f), dir.getChildFile ("gui_save.png")), "save dialog snapshot");
    }

    std::printf ("\n%d failure(s)\n", failures);
    return failures == 0 ? 0 : 1;
}
