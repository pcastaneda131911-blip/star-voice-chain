#include "PluginProcessor.h"
#include "PluginEditor.h"

StarAudioProcessor::StarAudioProcessor()
    : juce::AudioProcessor(BusesProperties()
                               .withInput("Input", juce::AudioChannelSet::stereo(), true)
                               .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
    formatManager.addFormat(new juce::VST3PluginFormat());
    reverbParams.roomSize = 0.5f;
    reverbParams.damping = 0.7f;
    reverbParams.width = 1.0f;
    reverbParams.wetLevel = 0.18f;
    reverbParams.dryLevel = 0.82f;
    reverb.setParameters(reverbParams);
    reverbMix.setCurrentAndTargetValue(0.18f);
    delayMix.setCurrentAndTargetValue(0.0f);
    compressorGain.setCurrentAndTargetValue(1.0f);
}

StarAudioProcessor::~StarAudioProcessor() = default;

bool StarAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
        && layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo();
}

void StarAudioProcessor::prepareToPlay(double newRate, int samplesPerBlock)
{
    sampleRate = newRate;
    reverb.setSampleRate((float) newRate);
    for (auto& plugin : externalPlugins)
        plugin->prepareToPlay(newRate, samplesPerBlock);
}

void StarAudioProcessor::releaseResources()
{
    for (auto& plugin : externalPlugins)
        plugin->releaseResources();
}

void StarAudioProcessor::processInternalBlock(juce::AudioBuffer<float>& buffer)
{
    juce::AudioBuffer<float> wetBuffer;
    wetBuffer.makeCopyOf(buffer);
    reverb.processStereo(wetBuffer.getWritePointer(0), wetBuffer.getWritePointer(1),
                         wetBuffer.getNumSamples());

    auto wet = reverbMix.getNextValue();
    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            buffer.setSample(ch, i, buffer.getSample(ch, i) * (1.0f - wet)
                                + wetBuffer.getSample(ch, i) * wet);

    }
}

void StarAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                      juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    processInternalBlock(buffer);

    for (auto& plugin : externalPlugins)
        plugin->processBlock(buffer, midi);
}

void StarAudioProcessor::beginListening()
{
    listeningStart = juce::Time::getMillisecondCounterHiRes();
    listening = true;
}

StarAnalysis StarAudioProcessor::finishListening()
{
    listening = false;
    lastAnalysis = { -18.0f, 0.55f, 0.42f, 0.5f };
    return lastAnalysis;
}

void StarAudioProcessor::scanPluginsAsync()
{
    if (scanning.exchange(true))
        return;
    scanStatus = "Escaneando VST3...";
    juce::Thread::launch([this]() { scanPluginsOnThread(); });
}

void StarAudioProcessor::scanPluginsOnThread()
{
    knownPlugins.clear();
    juce::VST3PluginFormat vst;
    juce::PluginDirectoryScanner scanner(
        knownPlugins, vst,
        juce::String("C:\\Program Files\\Common Files\\VST3"),
        nullptr,
        juce::File());

    juce::String currentFile;
    bool shouldExit = false;
    while (!shouldExit)
    {
        if (!scanner.scanNextFile(shouldExit, currentFile))
            break;
    }

    loadExternalPlugins();
    scanning = false;
    scanStatus = juce::String(knownPlugins.getTypes().size()) + " plugins VST3 encontrados";
}

juce::String StarAudioProcessor::getScanStatus() const { return scanStatus; }

void StarAudioProcessor::loadExternalPlugins()
{
    StarAnalysis analysis = finishListening();
    juce::Array<StarRoleInfo> chain = brain.chooseRoles(analysis, knownPlugins);

    juce::ScopedLock lock(stateLock);
    externalPlugins.clear();
    for (const auto& role : chain)
    {
        if (!role.external)
            continue;
        for (const auto& type : knownPlugins.getTypes())
        {
            if (type.name == role.name)
            {
                juce::String error;
                auto* format = formatManager.getFormat(0);
                if (format != nullptr)
                {
                    format->createPluginInstanceAsync(
                        type, sampleRate, 512,
                        [this](std::unique_ptr<juce::AudioPluginInstance> plugin,
                               const juce::String& /*error*/)
                        {
                            if (plugin != nullptr)
                            {
                                juce::ScopedLock pluginLock(stateLock);
                                plugin->prepareToPlay(sampleRate, 512);
                                externalPlugins.emplace_back(std::move(plugin));
                            }
                        });
                }
                break;
            }
        }
    }
}

juce::Array<StarRoleInfo> StarAudioProcessor::getChain() const
{
    juce::ScopedLock lock(stateLock);
    return brain.chooseRoles(lastAnalysis, knownPlugins);
}

void StarAudioProcessor::rebuildChain()
{
    loadExternalPlugins();
}

void StarAudioProcessor::applyChatCommand(const juce::String& command)
{
    juce::Array<StarRoleInfo> chain;
    {
        juce::ScopedLock lock(stateLock);
        chain = brain.chooseRoles(lastAnalysis, knownPlugins);
    }
    brain.applyCommand(command, chain);

    if (command.toLowerCase().contains("pon delay"))
        delayMix.setCurrentAndTargetValue(0.25f);
    if (command.toLowerCase().contains("quita el delay"))
        delayMix.setCurrentAndTargetValue(0.0f);
    if (command.toLowerCase().contains("quita la reverb"))
        reverbMix.setCurrentAndTargetValue(0.0f);
    rebuildChain();
}

void StarAudioProcessor::openPluginEditor(int chainIndex)
{
    juce::ignoreUnused(chainIndex);
}

void StarAudioProcessor::closePluginEditor(int chainIndex)
{
    juce::ignoreUnused(chainIndex);
}

juce::Component* StarAudioProcessor::getPluginEditor(int chainIndex)
{
    if (chainIndex >= 0 && chainIndex < (int) externalPlugins.size())
        return externalPlugins[(size_t) chainIndex]->createEditorIfNeeded();
    return nullptr;
}

void StarAudioProcessor::getStateInformation(juce::MemoryBlock& dest)
{
    juce::XmlElement xml("STARState");
    xml.setAttribute("version", 1);
    copyXmlToBinary(xml, dest);
}

void StarAudioProcessor::setStateInformation(const void* data, int size)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, size));
    juce::ignoreUnused(xml);
}

juce::AudioProcessorEditor* StarAudioProcessor::createEditor()
{
    return new StarEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new StarAudioProcessor();
}
