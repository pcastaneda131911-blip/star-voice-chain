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
        roleLabel.setFont(juce::Font(15.0f, juce::Font::bold));
        addAndMakeVisible(roleLabel);

        nameLabel.setText(role.name, juce::dontSendNotification);
        nameLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(190, 196, 205));
        nameLabel.setFont(juce::Font(12.0f));
        addAndMakeVisible(nameLabel);

        openButton.setButtonText("Abrir");
        openButton.setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGB(119, 112, 201));
        openButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
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
        roleLabel.setBounds(14, 7, 120, 20);
        nameLabel.setBounds(136, 7, juce::jmax(120, getWidth() - 236), 20);
        openButton.setBounds(getWidth() - 92, 5, 82, 30);
    }

    void paint(juce::Graphics& g) override
    {
        g.setColour(juce::Colour::fromRGB(39, 42, 50));
        g.fillRoundedRectangle(getLocalBounds().toFloat(), 7.0f);
        g.setColour(juce::Colour::fromRGB(74, 78, 88));
        g.drawRoundedRectangle(getLocalBounds().toFloat(), 7.0f, 1.0f);
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
    setSize(860, 620);

    titleLabel.setText("STAR", juce::dontSendNotification);
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    titleLabel.setFont(juce::Font(30.0f, juce::Font::bold));
    titleLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(titleLabel);

    modelLabel.setText("VOICE CHAIN", juce::dontSendNotification);
    modelLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(180, 185, 195));
    modelLabel.setFont(juce::Font(11.0f, juce::Font::bold));
    modelLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(modelLabel);

    presetTitle.setText("TAMASHI PRESET", juce::dontSendNotification);
    presetTitle.setColour(juce::Label::textColourId, juce::Colours::white);
    presetTitle.setFont(juce::Font(17.0f, juce::Font::bold));
    presetTitle.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(presetTitle);
    addAndMakeVisible(presetLeft);
    addAndMakeVisible(presetRight);

    scanButton.setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGB(43, 47, 56));
    scanButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    addAndMakeVisible(scanButton);
    scanButton.onClick = [this]() { processor.scanPluginsAsync(); rebuildRows(); };

    listenButton.setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGB(188, 89, 92));
    listenButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    addAndMakeVisible(listenButton);
    listenButton.onClick = [this]() {
        processor.beginListening();
        listenStartMs = juce::Time::getMillisecondCounter();
        listenButton.setEnabled(false);
        showListeningOverlay(true);
    };

    statusLabel.setJustificationType(juce::Justification::centred);
    statusLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(150, 155, 165));
    statusLabel.setFont(juce::Font(12.0f));
    addAndMakeVisible(statusLabel);

    chatInput.setMultiLine(false);
    chatInput.setText("Escucha mi voz");
    chatInput.setColour(juce::TextEditor::backgroundColourId, juce::Colour::fromRGB(34, 37, 44));
    chatInput.setColour(juce::TextEditor::textColourId, juce::Colours::white);
    chatInput.setColour(juce::TextEditor::outlineColourId, juce::Colour::fromRGB(86, 89, 98));
    addAndMakeVisible(chatInput);
    chatButton.setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGB(126, 112, 194));
    chatButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    addAndMakeVisible(chatButton);
    chatButton.onClick = [this]() {
        processor.applyChatCommand(chatInput.getText());
        rebuildRows();
    };

    viewport.setViewedComponent(&rowsContainer, false);
    addAndMakeVisible(viewport);

    listeningOverlay.setVisible(false);
    listeningOverlay.setAlwaysOnTop(true);
    listeningOverlay.setInterceptsMouseClicks(true, true);
    listeningOverlay.addAndMakeVisible(listeningTitle);
    listeningOverlay.addAndMakeVisible(listeningText);
    listeningOverlay.addAndMakeVisible(listeningProgress);
    addAndMakeVisible(listeningOverlay);

    listeningTitle.setText("Te estoy escuchando...", juce::dontSendNotification);
    listeningTitle.setColour(juce::Label::textColourId, juce::Colours::white);
    listeningTitle.setFont(juce::Font(22.0f, juce::Font::bold));
    listeningTitle.setJustificationType(juce::Justification::centred);
    listeningText.setText("Canta o habla normal. Necesito unos 8 segundos.",
                         juce::dontSendNotification);
    listeningText.setColour(juce::Label::textColourId, juce::Colour::fromRGB(200, 202, 208));
    listeningText.setJustificationType(juce::Justification::centred);
    listeningProgress.setText("1 / 8", juce::dontSendNotification);
    listeningProgress.setColour(juce::Label::backgroundColourId, juce::Colour::fromRGB(210, 128, 178));
    listeningProgress.setColour(juce::Label::textColourId, juce::Colours::white);
    listeningProgress.setJustificationType(juce::Justification::centred);

    rebuildRows();
    startTimerHz(10);
}

StarEditor::~StarEditor() = default;

void StarEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour::fromRGB(24, 26, 32));

    auto header = juce::Rectangle<int>(0, 0, getWidth(), 74);
    g.setColour(juce::Colour::fromRGB(31, 34, 41));
    g.fillRect(header);
    g.setColour(juce::Colour::fromRGB(65, 69, 78));
    g.drawRect(header, 1);

    g.setColour(juce::Colour::fromRGB(16, 18, 23));
    g.fillRoundedRectangle(juce::Rectangle<float>(18.0f, 92.0f, 824.0f, 426.0f), 10.0f);
    g.setColour(juce::Colour::fromRGB(66, 69, 78));
    g.drawRoundedRectangle(juce::Rectangle<float>(18.0f, 92.0f, 824.0f, 426.0f), 10.0f, 1.0f);
}

void StarEditor::resized()
{
    titleLabel.setBounds(22, 8, 170, 34);
    modelLabel.setBounds(22, 40, 170, 18);
    presetLeft.setBounds(300, 22, 44, 34);
    presetTitle.setBounds(350, 20, 180, 38);
    presetRight.setBounds(536, 22, 44, 34);
    scanButton.setBounds(620, 18, 105, 38);
    listenButton.setBounds(735, 18, 105, 38);
    statusLabel.setBounds(20, 74, 820, 18);
    viewport.setBounds(38, 112, 784, 386);
    chatInput.setBounds(38, 530, 650, 38);
    chatButton.setBounds(700, 530, 122, 38);

    rowsContainer.setBounds(0, 0, 764, juce::jmax(386, rows.size() * 54));
    int y = 0;
    for (auto* row : rows)
    {
        row->setBounds(0, y, 764, 48);
        y += 54;
    }

    listeningOverlay.setBounds(190, 190, 480, 220);
    listeningTitle.setBounds(30, 30, 420, 40);
    listeningText.setBounds(30, 80, 420, 30);
    listeningProgress.setBounds(45, 125, 390, 18);
}

void StarEditor::timerCallback()
{
    statusLabel.setText(processor.getScanStatus(), juce::dontSendNotification);
    if (processor.isListening())
    {
        auto elapsed = juce::jlimit(0, 8000,
            (int) (juce::Time::getMillisecondCounter() - listenStartMs));
        listeningProgressValue = elapsed / 8000.0;
        listeningText.setText("Grabando: " + juce::String(elapsed / 1000.0, 1) + " / 8 segundos",
                              juce::dontSendNotification);
        listeningProgress.setText(juce::String(elapsed / 1000.0, 1) + " / 8",
                                  juce::dontSendNotification);
    }
    if (!listenButton.isEnabled() && processor.isListening()
        && juce::Time::getMillisecondCounter() - listenStartMs >= 8000)
    {
        listenButton.setEnabled(true);
        auto analysis = processor.finishListening();
        showListeningOverlay(false);
        juce::String text = "Analisis: ";
        text += juce::String(analysis.brightness, 2)
             + " brillo, " + juce::String(analysis.sibilance, 2) + " sibilancia";
        statusLabel.setText(text, juce::dontSendNotification);
        rebuildRows();
    }
}

void StarEditor::showListeningOverlay(bool shouldShow)
{
    listeningOverlay.setVisible(shouldShow);
    listeningOverlay.toFront(shouldShow);
    listeningProgressValue = 0.0;
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
        row->setBounds(0, y, 764, 48);
        y += 54;
    }
    rowsContainer.setBounds(0, 0, 764, juce::jmax(386, y));
    resized();
}
