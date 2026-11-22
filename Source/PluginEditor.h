#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

class StarEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit StarEditor(StarAudioProcessor&);
    ~StarEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void rebuildRows();
    void showListeningOverlay(bool);
    StarAudioProcessor& processor;
    juce::int64 listenStartMs = 0;

    juce::TextButton scanButton { "Buscar plugins" };
    juce::TextButton listenButton { "Escuchar mi voz" };
    juce::TextButton presetLeft { "<" };
    juce::TextButton presetRight { ">" };
    juce::Label presetTitle;
    juce::Label titleLabel;
    juce::Label modelLabel;
    juce::Label statusLabel;
    juce::TextEditor chatInput;
    juce::TextButton chatButton { "Aplicar IA" };
    juce::Component listeningOverlay;
    juce::Label listeningTitle;
    juce::Label listeningText;
    juce::Label listeningProgress;
    double listeningProgressValue = 0.0;
    juce::OwnedArray<juce::Component> rows;
    juce::Component rowsContainer;
    juce::Viewport viewport;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StarEditor)
};
