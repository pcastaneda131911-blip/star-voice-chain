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
    StarAudioProcessor& processor;
    juce::int64 listenStartMs = 0;

    juce::TextButton scanButton { "Buscar mis plugins" };
    juce::TextButton listenButton { "Escuchar mi voz (8 s)" };
    juce::Label statusLabel;
    juce::TextEditor chatInput;
    juce::TextButton chatButton { "Aplicar" };
    juce::OwnedArray<juce::Component> rows;
    juce::Component rowsContainer;
    juce::Viewport viewport;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StarEditor)
};
