#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "ChopEngine.h"
#include "Presets.h"

class ChopShopProcessor : public juce::AudioProcessor
{
public:
    ChopShopProcessor();
    ~ChopShopProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Chop Shop"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    juce::AudioProcessorValueTreeState apvts;
    chop::Params currentParams() const;

    // metering, read and reset by the editor
    chop::Engine& getEngine() { return engine; }

    // ---- user presets (files shared by every project and DAW)
    static juce::File getUserPresetFolder();
    static juce::Array<juce::File> getUserPresets();
    static juce::File fileForPresetName (const juce::String& name);
    bool saveUserPreset (const juce::String& name);
    bool loadUserPreset (const juce::File& file);
    juce::String getCurrentPresetName() const { return currentPresetName; }
    std::atomic<int> presetVersion { 0 };

private:
    void resetToDefaults();
    chop::Engine engine;
    int currentProgram = 0, reportedLatency = -1;
    juce::String currentPresetName { "Init (Transparent)" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChopShopProcessor)
};
