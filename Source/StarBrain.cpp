#include "StarBrain.h"

static juce::Array<juce::PluginDescription> findForRole(const juce::KnownPluginList& list,
                                                         const juce::StringArray& keywords)
{
    juce::Array<juce::PluginDescription> found;
    for (const auto& type : list.getTypes())
    {
        auto haystack = type.name.toLowerCase() + " " + type.manufacturerName.toLowerCase();
        for (auto keyword : keywords)
            if (haystack.contains(keyword.toLowerCase()))
            {
                found.add(type);
                break;
            }
    }
    return found;
}

juce::Array<StarRoleInfo> StarBrain::chooseRoles(const StarAnalysis& analysis,
                                                  const juce::KnownPluginList& plugins) const
{
    juce::Array<StarRoleInfo> chain;

    auto add = [&](const juce::String& role, const juce::StringArray& keywords,
                   const juce::String& internalName)
    {
        auto found = findForRole(plugins, keywords);
        StarRoleInfo info;
        info.role = role;
        if (!found.isEmpty())
        {
            info.name = found.getFirst().name;
            info.manufacturer = found.getFirst().manufacturerName;
            info.external = true;
        }
        else
        {
            info.name = internalName;
        }
        chain.add(info);
    };

    add("EQ", {"pro-q", "pultec", "api-560", "eq", "equalizer"}, "STAR EQ");
    add("Compresor", {"cla-2a", "1176", "rvox", "compressor", "comp"}, "STAR Compresor");
    add("Limitador", {"pro-l", "ozone", "limiter", "limitador"}, "STAR Limitador");

    if (analysis.sibilance > 0.35f)
        add("De-esser", {"pro-ds", "deesser", "de-esser", "de ess"}, "STAR De-esser");

    if (analysis.rmsDb < -24.0f)
        add("Reduccion de ruido", {"ns1", "x-noise", "noise reduction", "denoise"},
            "STAR Reduccion de ruido");

    if (analysis.brightness < 0.42f)
        add("Aire", {"fresh air", "air", "maag"}, "STAR Aire");

    add("Reverb", {"valhallavintageverb", "valhalla", "reverb", "room"}, "STAR Reverb");

    if (wantsPitch)
        add("Afinacion", {"auto-tune", "melodyne", "graillon", "pitch", "tune"},
            "STAR Afinacion interna");

    return chain;
}

void StarBrain::applyCommand(const juce::String& command, juce::Array<StarRoleInfo>& chain)
{
    auto text = command.toLowerCase();
    if (text.contains("mas brillante") || text.contains("más brillante"))
    {
        bool found = false;
        for (auto& item : chain)
            if (item.role == "Aire")
            {
                found = true;
                break;
            }
        if (!found)
            chain.add({ "Aire", "STAR Aire", {}, false });
    }
    if (text.contains("menos sibilancia") || text.contains("de-esser"))
    {
        bool found = false;
        for (auto& item : chain)
            if (item.role == "De-esser")
            {
                found = true;
                break;
            }
        if (!found)
            chain.add({ "De-esser", "STAR De-esser", {}, false });
    }
    if (text.contains("pon afinacion") || text.contains("pon afinación") || text.contains("pitch"))
        wantsPitch = true;
    if (text.contains("quita la reverb") || text.contains("sin reverb"))
    {
        for (int i = chain.size() - 1; i >= 0; --i)
            if (chain[i].role == "Reverb")
                chain.remove(i);
    }
    if (text.contains("pon delay"))
        chain.add({ "Delay", "STAR Delay", {}, false });
    if (text.contains("quita el delay"))
    {
        for (int i = chain.size() - 1; i >= 0; --i)
            if (chain[i].role == "Delay")
                chain.remove(i);
    }
}
