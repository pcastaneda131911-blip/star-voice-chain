#include "PluginProcessor.h"
#include "PluginEditor.h"

StarAudioProcessor::StarAudioProcessor()
    : juce::AudioProcessor(BusesProperties()
                               .withInput("Input", juce::AudioChannelSet::stereo(), true)
                               .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
    formatManager.addFormat(std::make_unique<juce::VST3PluginFormat>());
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
    inputGain.prepare({ newRate, (juce::uint32) samplesPerBlock, 2 });
    outputGain.prepare({ newRate, (juce::uint32) samplesPerBlock, 2 });
    highPass.prepare({ newRate, (juce::uint32) samplesPerBlock, 2 });
    highPass.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass(newRate, 35.0f);
    reverb.setSampleRate((float) newRate);
    delay.prepare({ newRate, (juce::uint32) samplesPerBlock, 2 });
    delay.setDelay((float) (0.35 * newRate));
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
    juce::dsp::ProcessContextReplacing<float> context(buffer);
    highPass.process(context);

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

        if (delayMix.getNextValue() > 0.001f)
        {
            for (int ch = 0; ch < juce::jmin(2, buffer.getNumChannels()); ++ch)
            {
                float delayed = delay.popSample(ch);
                float input = buffer.getSample(ch, i);
                delay.pushSample(ch, input);
                buffer.setSample(ch, i, input + delayed * delayMix.getNextValue());
            }
        }
    }
    outputGain.process(context);
}

void StarAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                      juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    if (listening.load())
    {
        for (int ch = 0; ch < juce::jmin(2, buffer.getNumChannels()); ++ch)
        {
            auto* samples = buffer.getReadPointer(ch);
            for (int i = 0; i < buffer.getNumSamples(); ++i)
            {
                listeningRmsSquared += (double) samples[i] * (double) samples[i];
                if (listeningSampleCount > 0
                    && ((listeningPrevious >= 0.0 && samples[i] < 0.0)
                        || (listeningPrevious < 0.0 && samples[i] >= 0.0)))
                    listeningCrossings += 1.0;
                listeningPrevious = samples[i];
                ++listeningSampleCount;
            }
        }
    }
    processInternalBlock(buffer);

    for (auto& plugin : externalPlugins)
        plugin->processBlock(buffer, midi);
}

void StarAudioProcessor::beginListening()
{
    listeningStart = juce::Time::getMillisecondCounterHiRes();
    listeningRmsSquared = 0.0;
    listeningCrossings = 0.0;
    listeningPrevious = 0.0;
    listeningSampleCount = 0;
    listening = true;
}

StarAnalysis StarAudioProcessor::finishListening()
{
    listening = false;
    double rms = listeningSampleCount > 0
        ? std::sqrt(listeningRmsSquared / (double) listeningSampleCount)
        : 0.001;
    float rmsDb = (float) juce::Decibels::gainToDecibels(rms);
    float crossings = listeningSampleCount > 0
        ? (float) (listeningCrossings / (double) listeningSampleCount)
        : 0.5f;
    float brightness = juce::jlimit(0.0f, 1.0f, crossings * 8.0f);
    lastAnalysis = { rmsDb, brightness, brightness, crossings };
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
                auto plugin = format != nullptr
                    ? format->createPluginInstance(type, sampleRate, 512, error)
                    : nullptr;
                if (plugin != nullptr)
                {
                    plugin->prepareToPlay(sampleRate, 512);
                    externalPlugins.emplace_back(std::move(plugin));
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
