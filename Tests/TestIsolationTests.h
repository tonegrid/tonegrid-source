#pragma once
#include "Platform/AppPaths.h"
#include "Content/Presets/RigStore.h"

namespace testIsolationTests
{
inline bool run(juce::String& report)
{
    using namespace juce;
    bool ok=true;
    const auto check=[&](bool value,const String& label){ok=value&&ok;report+=(value?"PASS ":"FAIL ")+label+"\n";};
    const auto root=tonegrid::paths::temporaryRoot().getChildFile("ToneGrid-isolation-"+Uuid().toString());
    const ScopeGuard cleanup{[&]{if(root.isAChildOf(tonegrid::paths::temporaryRoot()))root.deleteRecursively();}};
    const auto data=root.getChildFile("data");
    const auto productionRoot=root.getChildFile("production-data");
    const auto production=productionRoot.getChildFile("ToneGrid");
    const auto legacy=productionRoot.getChildFile("TonexStateProbe/Rigs-v3");
    const auto preset=production.getChildFile("Rigs-v1/slot-001.tonexrig");
    const auto settings=production.getChildFile("Rigs-v1/audio.xml");
    check(preset.getParentDirectory().createDirectory().wasOk()&&legacy.createDirectory().wasOk(),"Create fake production data under isolated temporary root");
    check(preset.replaceWithText("production preset sentinel")&&settings.replaceWithText("production audio sentinel")
        &&legacy.getChildFile("slot-001.tonexrig").replaceWithText("legacy production sentinel"),"Seed production and legacy sentinels");
    const tonegrid::paths::Layout paths(root.getChildFile(tonegrid::paths::currentPlatform()==tonegrid::paths::Platform::macOS?"test/ToneGrid.app/Contents/MacOS/ToneGrid":"test/ToneGrid.exe"),data);
    check(paths.user()==data.getChildFile("ToneGrid"),"Independent mutable data root");
    const std::array<File,11> folders{paths.rigs(),paths.toneBankCache(),paths.namAmps(),paths.vst3(),paths.onlineChecks(),
        paths.betaWelcome(),paths.levelCalibration(),paths.namQuality(),paths.ampSelection(),paths.browserProfile(),paths.user().getChildFile("IRs")};
    for(const auto& folder:folders)check(folder.isAChildOf(paths.user())&&!folder.isAChildOf(production),"Independent path: "+folder.getRelativePathFrom(data));
    auto store=board::RigStore::forApplicationData(data);
    store.migrateLegacyBank();
    check(store.directory()==paths.rigs()&&!store.contains(0),"No automatic import from ToneGrid or TonexStateProbe");
    check(store.slotFile(0).replaceWithText("experimental preset")&&paths.levelCalibration().getParentDirectory().createDirectory().wasOk()
        &&paths.levelCalibration().replaceWithText("experimental calibration"),"Write test preset and calibration in test data only");
    check(preset.loadFileAsString()=="production preset sentinel"&&settings.loadFileAsString()=="production audio sentinel"
        &&legacy.getChildFile("slot-001.tonexrig").loadFileAsString()=="legacy production sentinel","Production presets, audio settings and legacy data remain byte-identical");
    check(paths.models().isAChildOf(root.getChildFile("test"))&&paths.starterPresets().isAChildOf(root.getChildFile("test")),"Bundled models and starter presets resolve beside the test executable");
    return ok;
}
}
