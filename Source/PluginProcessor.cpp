#include "PluginProcessor.h"
#include "PluginEditor.h"

#ifndef JucePlugin_VersionString
 #define JucePlugin_VersionString "dev"
#endif

ChopShopProcessor::ChopShopProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "ChopShop", createLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout ChopShopProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;
    auto dbAttr = AudioParameterFloatAttributes().withLabel ("dB")
        .withStringFromValueFunction ([] (float v, int) { return (v > 0 ? "+" : "") + String (v, 1) + " dB"; });
    auto pctAttr = AudioParameterFloatAttributes().withLabel ("%")
        .withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v)) + "%"; });

    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "mode", 1 }, "Mode", StringArray { "Master", "Aggressive", "Color" }, 0));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "shape", 1 }, "Clip Shape", StringArray { "Hard", "Soft" }, 0));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "inputDb", 1 }, "Input", NormalisableRange<float> (-12.0f, 24.0f, 0.1f), 0.0f, dbAttr));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "ceilingDb", 1 }, "Ceiling", NormalisableRange<float> (-24.0f, 0.0f, 0.1f, 2.0f), 0.0f, dbAttr));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "color", 1 }, "Color", NormalisableRange<float> (0.0f, 100.0f, 0.1f), 0.0f, pctAttr));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "mix", 1 }, "Mix", NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f, pctAttr));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "outputDb", 1 }, "Output", NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f, dbAttr));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "oversample", 1 }, "Oversampling", StringArray { "Off", "2x", "4x", "8x" }, 2));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "gainMatch", 1 }, "Gain Match", false));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "scraps", 1 }, "Scraps (Delta)", false));
    return { p.begin(), p.end() };
}

chop::Params ChopShopProcessor::currentParams() const
{
    auto v = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };
    chop::Params p;
    p.mode = (int) v ("mode");
    p.shape = (int) v ("shape");
    p.inputDb = v ("inputDb");
    p.ceilingDb = v ("ceilingDb");
    p.color = v ("color") * 0.01f;
    p.mix = v ("mix") * 0.01f;
    p.outputDb = v ("outputDb");
    p.oversample = (int) v ("oversample");
    p.gainMatch = v ("gainMatch") > 0.5f;
    p.scraps = v ("scraps") > 0.5f;
    return p;
}

bool ChopShopProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo()) return false;
    const auto in = layouts.getMainInputChannelSet();
    return in == out || (in == juce::AudioChannelSet::mono() && out == juce::AudioChannelSet::stereo());
}

void ChopShopProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, samplesPerBlock);
    const auto p = currentParams();
    engine.setParams (p, true);
    reportedLatency = engine.latencyFor (p.oversample);
    setLatencySamples (reportedLatency);
}

void ChopShopProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int inCh = getTotalNumInputChannels();
    const int outCh = juce::jmin (2, buffer.getNumChannels());
    if (inCh == 1 && outCh > 1) buffer.copyFrom (1, 0, buffer, 0, 0, n); // mono in -> stereo out

    engine.setParams (currentParams());
    engine.process (buffer.getArrayOfWritePointers(), outCh, n);
    if (engine.getLatency() != reportedLatency)
    {
        reportedLatency = engine.getLatency();
        setLatencySamples (reportedLatency);
    }
    for (int ch = outCh; ch < buffer.getNumChannels(); ++ch) buffer.clear (ch, 0, n);
}

//==============================================================================
int ChopShopProcessor::getNumPrograms() { return (int) chop::factoryPresets().size(); }

const juce::String ChopShopProcessor::getProgramName (int index)
{
    return juce::isPositiveAndBelow (index, getNumPrograms()) ? chop::factoryPresets()[(size_t) index].name : "";
}

void ChopShopProcessor::resetToDefaults()
{
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            rp->setValueNotifyingHost (rp->getDefaultValue());
}

void ChopShopProcessor::setCurrentProgram (int index)
{
    if (! juce::isPositiveAndBelow (index, getNumPrograms())) return;
    currentProgram = index;
    currentPresetName = chop::factoryPresets()[(size_t) index].name;
    resetToDefaults();
    for (auto& [id, v] : chop::factoryPresets()[(size_t) index].values)
        if (auto* rp = apvts.getParameter (id))
            rp->setValueNotifyingHost (rp->convertTo0to1 (v));
    ++presetVersion;
}

juce::File ChopShopProcessor::getUserPresetFolder()
{
    auto base = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory); // ~/Library on macOS, %APPDATA% on Windows
   #if JUCE_MAC
    base = base.getChildFile ("Application Support");
   #endif
    return base.getChildFile ("Castle Danger").getChildFile ("Chop Shop").getChildFile ("Presets");
}

juce::Array<juce::File> ChopShopProcessor::getUserPresets()
{
    auto files = getUserPresetFolder().findChildFiles (juce::File::findFiles, false, "*.chopshop");
    std::sort (files.begin(), files.end(), [] (const juce::File& a, const juce::File& b)
               { return a.getFileNameWithoutExtension().compareNatural (b.getFileNameWithoutExtension()) < 0; });
    return files;
}

juce::File ChopShopProcessor::fileForPresetName (const juce::String& name)
{
    return getUserPresetFolder().getChildFile (juce::File::createLegalFileName (name.trim()) + ".chopshop");
}

bool ChopShopProcessor::saveUserPreset (const juce::String& rawName)
{
    const auto name = rawName.trim();
    if (name.isEmpty()) return false;
    const auto file = fileForPresetName (name);
    if (! file.getParentDirectory().createDirectory()) return false;

    juce::XmlElement xml ("ChopShopPreset");
    xml.setAttribute ("name", name);
    xml.setAttribute ("pluginVersion", JucePlugin_VersionString);
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
        {
            if (rp->getParameterID() == "scraps") continue; // never save a preset that only plays the scraps
            auto* e = xml.createNewChildElement ("PARAM");
            e->setAttribute ("id", rp->getParameterID());
            e->setAttribute ("value", (double) rp->convertFrom0to1 (rp->getValue()));
        }
    if (! xml.writeTo (file)) return false;
    currentPresetName = name;
    ++presetVersion;
    return true;
}

bool ChopShopProcessor::loadUserPreset (const juce::File& file)
{
    auto xml = juce::XmlDocument::parse (file);
    if (xml == nullptr || ! xml->hasTagName ("ChopShopPreset")) return false;
    resetToDefaults(); // so parameters added in later versions behave sensibly
    for (auto* e : xml->getChildWithTagNameIterator ("PARAM"))
        if (auto* rp = apvts.getParameter (e->getStringAttribute ("id")))
            rp->setValueNotifyingHost (rp->convertTo0to1 ((float) e->getDoubleAttribute ("value")));
    currentPresetName = xml->getStringAttribute ("name", file.getFileNameWithoutExtension());
    ++presetVersion;
    return true;
}

void ChopShopProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("program", currentProgram, nullptr);
    state.setProperty ("presetName", currentPresetName, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, destData);
}

void ChopShopProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto vt = juce::ValueTree::fromXml (*xml);
            currentProgram = vt.getProperty ("program", 0);
            currentPresetName = vt.getProperty ("presetName", getProgramName (currentProgram)).toString();
            apvts.replaceState (vt);
            ++presetVersion;
        }
}

juce::AudioProcessorEditor* ChopShopProcessor::createEditor() { return new ChopShopEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ChopShopProcessor(); }
