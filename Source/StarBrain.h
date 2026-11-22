#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

struct StarAnalysis
{
    float rmsDb = -60.0f;
    float brightness = 0.5f;
    float sibilance = 0.2f;
    float zeroCrossings = 0.5f;
};

struct StarRoleInfo
{
    juce::String role;
    juce::String name;
    juce::String manufacturer;
    bool external = false;
};

class StarBrain
{
public:
    juce::Array<StarRoleInfo> chooseRoles(const StarAnalysis&, const juce::KnownPluginList&) const;
    void applyCommand(const juce::String&, juce::Array<StarRoleInfo>&);
    void addPitch(bool) { wantsPitch = true; }

private:
    bool wantsPitch = false;
};
