#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include "StarBrain.h"

class StarAudioProcessor : public juce::AudioProcessor
{
public:
    StarAudioProcessor();
    ~StarAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    bool hasEditor() const override { return true; }
    juce::AudioProcessorEditor* createEditor() override;
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    const juce::String getName() const override { return "STAR Voice Chain"; }
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock& dest) override;
    void setStateInformation(const void* data, int size) override;

    void scanPluginsAsync();
    bool isScanning() const { return scanning; }
    juce::String getScanStatus() const;
    juce::Array<StarRoleInfo> getChain() const;
    void rebuildChain();
    void applyChatCommand(const juce::String&);
    void openPluginEditor(int chainIndex);
    void closePluginEditor(int chainIndex);
    juce::Component* getPluginEditor(int chainIndex);

    StarAnalysis finishListening();
    void beginListening();
    bool isListening() const { return listening; }

    mutable juce::CriticalSection stateLock;
    StarBrain brain;
    juce::AudioPluginFormatManager formatManager;
    juce::KnownPluginList knownPlugins;
    std::vector<std::unique_ptr<juce::AudioProcessor>> externalPlugins;

private:
    void loadExternalPlugins();
    void scanPluginsOnThread();
    void processInternalBlock(juce::AudioBuffer<float>&);

    std::atomic<bool> scanning { false };
    std::atomic<bool> listening { false };
    juce::String scanStatus { "No escaneado" };
    juce::Time listeningStart;
    double sampleRate = 44100.0;
    double listeningRmsSquared = 0.0;
    double listeningCrossings = 0.0;
    double listeningPrevious = 0.0;
    juce::int64 listeningSampleCount = 0;
    StarAnalysis lastAnalysis;

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> compressorGain;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> reverbMix;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> delayMix;
    juce::Reverb reverb;
    juce::Reverb::Parameters reverbParams;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StarAudioProcessor)
};

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();
