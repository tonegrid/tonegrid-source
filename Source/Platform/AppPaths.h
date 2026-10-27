#pragma once
#include <juce_core/juce_core.h>

namespace tonegrid::paths
{
enum class Platform { windows, macOS, linuxOS };
constexpr Platform currentPlatform()
{
#if JUCE_MAC
    return Platform::macOS;
#elif JUCE_LINUX
    return Platform::linuxOS;
#else
    return Platform::windows;
#endif
}
inline juce::File applicationDataRoot()
{
    auto root=juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory);
#if JUCE_MAC
    // JUCE returns ~/Library on macOS. Mutable application data belongs below it.
    return root.getChildFile("Application Support");
#else
    return root;
#endif
}
inline juce::File userFolder(const juce::File& root) { return root.getChildFile("ToneGrid"); }
inline juce::File temporaryRoot() { return juce::File::getSpecialLocation(juce::File::tempDirectory); }

// Path policy only: no IO and no audio/device ownership. Explicit roots permit
// relocation and platform-layout tests without accessing the real user profile.
class Layout final
{
public:
    Layout(juce::File executable,juce::File data,Platform platform=currentPlatform())
        : exe(std::move(executable)),dataRoot(std::move(data)),system(platform) {}
    const juce::File& executable() const { return exe; }
    juce::File resources() const
    {
        const auto folder=exe.getParentDirectory();
        return system==Platform::macOS?folder.getParentDirectory().getChildFile("Resources"):folder;
    }
    juce::File models() const { return resources().getChildFile("Models"); }
    juce::File starterPresets() const { return resources().getChildFile("StarterPresets"); }
    juce::File shippedToneBank() const { return resources().getChildFile("ToneBank/tonebank.json"); }
    juce::File factoryCabinetIR() const { return resources().getChildFile("FactoryCabinetIR"); }
    juce::File factoryAmps() const { return resources().getChildFile("FactoryAmps"); }
    juce::File encoder() const
    {
        return system==Platform::macOS?exe.getSiblingFile("lame"):resources().getChildFile(system==Platform::linuxOS?"Tools/lame":"Tools/lame.exe");
    }
    juce::File user() const { return userFolder(dataRoot); }
    juce::File editableContent() const { return user().getChildFile("Content"); }
    juce::File shippedTranslations() const { return resources().getChildFile("Translations"); }
    juce::File translations() const { return editableContent().getChildFile("Translations"); }
    juce::File languagePreference() const { return user().getChildFile("Preferences/language.json"); }
    juce::File calibrationProfiles() const { return editableContent().getChildFile("Calibration/interface-profiles.json"); }
    juce::File jamCatalogue() const { return editableContent().getChildFile("Jam/backing-tracks.json"); }
    juce::File learnCatalogue() const { return editableContent().getChildFile("Learn/lessons.json"); }
    juce::File toneBankOverride() const { return editableContent().getChildFile("ToneBank/tonebank.json"); }
    juce::File rigs() const { return user().getChildFile("Rigs-v1"); }
    juce::File toneBankCache() const { return user().getChildFile("ToneBank/tonebank.json"); }
    juce::File namAmps() const { return user().getChildFile("NAM/Amps"); }
    juce::File vst3() const { return user().getChildFile("Vst3"); }
    juce::File onlineChecks() const { return user().getChildFile("Online/checks.xml"); }
    juce::File betaWelcome() const { return user().getChildFile("Preferences/beta-welcome.xml"); }
    juce::File levelCalibration() const { return user().getChildFile("Audio/level-calibration.xml"); }
    juce::File namQuality() const { return user().getChildFile("Audio/nam-quality.xml"); }
    juce::File ampSelection() const { return user().getChildFile("Amp/selected.xml"); }
    juce::File browserProfile() const { return user().getChildFile("Tone3000/WebView2"); }
private:
    juce::File exe,dataRoot;
    Platform system;
};
inline Layout installed(juce::File data=applicationDataRoot())
{ return {juce::File::getSpecialLocation(juce::File::currentExecutableFile),std::move(data)}; }
}
