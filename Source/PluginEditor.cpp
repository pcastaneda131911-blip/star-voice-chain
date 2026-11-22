#include "PluginEditor.h"

namespace
{
class PluginWindow : public juce::DocumentWindow
{
public:
    explicit PluginWindow(juce::Component* editor, const juce::String& title)
        : DocumentWindow(title,
                         juce::Colours::darkgrey,
                         DocumentWindow::closeButton)
    {
        setUsingNativeTitleBar(true);
        setResizable(true, true);
        setContentOwned(editor, true);
        setTopLeftPosition(100, 100);
        setSize(editor != nullptr ? juce::jmax(300, editor->getWidth()) : 300,
                editor != nullptr ? juce::jmax(200, editor->getHeight()) : 200);
        setVisible(true);
    }
    void closeButtonPressed() override { delete this; }
};

class ChainRow : public juce::Component
{
public:
    ChainRow(StarAudioProcessor& processor, const StarRoleInfo& role, int index)
        : owner(processor), chainIndex(index)
    {
        roleLabel.setText(role.role, juce::dontSendNotification);
        roleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
        roleLabel.setFont(juce::Font(16.0f, juce::Font::bold));
        addAndMakeVisible(roleLabel);

        nameLabel.setText(role.name, juce::dontSendNotification);
        nameLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
        addAndMakeVisible(nameLabel);

        openButton.setButtonText("Abrir");
        openButton.onClick = [this]() {
            if (auto* editor = owner.getPluginEditor(chainIndex))
            {
                auto* window = new PluginWindow(editor, nameLabel.getText());
                juce::ignoreUnused(window);
            }
        };
        addAndMakeVisible(openButton);
    }

    void resized() override
    {
        roleLabel.setBounds(12, 8, 150, 22);
        nameLabel.setBounds(170, 8, 190, 22);
        openButton.setBounds(getWidth() - 82, 6, 70, 28);
    }

    void paint(juce::Graphics& g) override
    {
        g.setColour(juce::Colour::fromRGB(38, 42, 50));
        g.fillRoundedRectangle(getLocalBounds().toFloat(), 8.0f);
        g.setColour(juce::Colour::fromRGB(75, 82, 94));
        g.drawRoundedRectangle(getLocalBounds().toFloat(), 8.0f, 1.0f);
    }

private:
    StarAudioProcessor& owner;
    int chainIndex;
    juce::Label roleLabel, nameLabel;
    juce::TextButton openButton;
};
}

StarEditor::StarEditor(StarAudioProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    setSize(520, 620);
    addAndMakeVisible(scanButton);
    scanButton.onClick = [this]() { processor.scanPluginsAsync(); rebuildRows(); };

    addAndMakeVisible(listenButton);
    listenButton.onClick = [this]() {
        processor.beginListening();
        listenStartMs = juce::Time::getMillisecondCounter();
        listenButton.setEnabled(false);
        statusLabel.setText("Escuchando 8 segundos...", juce::dontSendNotification);
    };

    statusLabel.setJustificationType(juce::Justification::centred);
    statusLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible(statusLabel);

    chatInput.setMultiLine(false);
    chatInput.setText("mas brillante");
    addAndMakeVisible(chatInput);
    addAndMakeVisible(chatButton);
    chatButton.onClick = [this]() {
        processor.applyChatCommand(chatInput.getText());
        rebuildRows();
    };

    viewport.setViewedComponent(&rowsContainer, false);
    addAndMakeVisible(viewport);
    rebuildRows();
    startTimerHz(10);
}

StarEditor::~StarEditor() = default;

void StarEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour::fromRGB(24, 27, 33));
}

void StarEditor::resized()
{
    scanButton.setBounds(20, 16, 190, 36);
    listenButton.setBounds(220, 16, 280, 36);
    statusLabel.setBounds(20, 58, 480, 24);
    viewport.setBounds(20, 90, 480, 430);
    chatInput.setBounds(20, 532, 390, 32);
    chatButton.setBounds(420, 532, 80, 32);

    rowsContainer.setBounds(0, 0, 460, juce::jmax(430, rows.size() * 48));
    int y = 0;
    for (auto* row : rows)
    {
        row->setBounds(0, y, 460, 42);
        y += 48;
    }
}

void StarEditor::timerCallback()
{
    statusLabel.setText(processor.getScanStatus(), juce::dontSendNotification);
    if (!listenButton.isEnabled() && processor.isListening()
        && juce::Time::getMillisecondCounter() - listenStartMs >= 8000)
    {
        listenButton.setEnabled(true);
        auto analysis = processor.finishListening();
        juce::String text = "Analisis: ";
        text += juce::String(analysis.brightness, 2)
             + " brillo, " + juce::String(analysis.sibilance, 2) + " sibilancia";
        statusLabel.setText(text, juce::dontSendNotification);
        rebuildRows();
    }
}

void StarEditor::rebuildRows()
{
    rows.clear();
    int externalIndex = 0;
    for (const auto& role : processor.getChain())
    {
        rows.add(new ChainRow(processor, role, externalIndex));
        if (role.external)
            ++externalIndex;
    }
    rowsContainer.removeAllChildren();
    int y = 0;
    for (auto* row : rows)
    {
        rowsContainer.addAndMakeVisible(row);
        row->setBounds(0, y, 460, 42);
        y += 48;
    }
    rowsContainer.setBounds(0, 0, 460, juce::jmax(430, y));
    resized();
}
