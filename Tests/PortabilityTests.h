#pragma once
#include "Platform/AppPaths.h"
#include "Content/Presets/RigStore.h"
#include "Content/Amps/NamAmpLibrary.h"
#include "Content/Presets/ToneBank.h"
#include "Platform/PlatformFiles.h"
#include "NamAmpTests.h"
#include "Vst3Catalog.h"

namespace portabilityTests
{
inline bool run(juce::String& report)
{
    using namespace juce;
    bool ok=true;int checks=0;
    const auto check=[&](bool value,const String& label){++checks;ok=value&&ok;report+=(value?"PASS ":"FAIL ")+label+"\n";};
    const auto root=tonegrid::paths::temporaryRoot().getChildFile("ToneGrid-path-policy-"+Uuid().toString());
    const auto data=root.getChildFile("user data");
    const auto exe=root.getChildFile("installation/ToneGrid.exe");
    const tonegrid::paths::Layout win(exe,data,tonegrid::paths::Platform::windows);
    check(win.resources()==exe.getParentDirectory(),"Windows portable resource directory preserved");
    check(win.encoder()==exe.getSiblingFile("Tools/lame.exe"),"Windows MP3 encoder path preserved");
    check(win.models()==exe.getSiblingFile("Models")&&win.factoryAmps()==exe.getSiblingFile("FactoryAmps")&&win.shippedToneBank()==exe.getSiblingFile("ToneBank/tonebank.json"),"Windows factory resources unchanged");
    check(win.rigs()==data.getChildFile("ToneGrid/Rigs-v1")&&win.namAmps()==data.getChildFile("ToneGrid/NAM/Amps"),"Windows user presets and NAM locations unchanged");
    check(win.toneBankCache()==data.getChildFile("ToneGrid/ToneBank/tonebank.json")&&win.vst3()==data.getChildFile("ToneGrid/Vst3"),"Windows cache locations unchanged");
    check(win.onlineChecks()==data.getChildFile("ToneGrid/Online/checks.xml")&&win.namQuality()==data.getChildFile("ToneGrid/Audio/nam-quality.xml")&&win.ampSelection()==data.getChildFile("ToneGrid/Amp/selected.xml")&&win.browserProfile()==data.getChildFile("ToneGrid/Tone3000/WebView2"),"Windows preferences and browser profile unchanged");
    const auto macExe=root.getChildFile("Applications/ToneGrid.app/Contents/MacOS/ToneGrid");
    const tonegrid::paths::Layout mac(macExe,data,tonegrid::paths::Platform::macOS);
    check(mac.resources()==root.getChildFile("Applications/ToneGrid.app/Contents/Resources"),"Mac resources separate from executable directory");
    check(mac.encoder()==macExe.getSiblingFile("lame"),"Mac helper has native executable name");
    check(mac.user()==win.user()&&!mac.user().isAChildOf(mac.resources()),"Mutable user data stays outside application resources");
    const auto movedExe=root.getChildFile("relocated/ToneGrid.app/Contents/MacOS/ToneGrid");
    const tonegrid::paths::Layout moved(movedExe,data,tonegrid::paths::Platform::macOS);
    check(moved.models()==root.getChildFile("relocated/ToneGrid.app/Contents/Resources/Models")&&moved.rigs()==mac.rigs(),"Moving application preserves user data and relocates shipped models");
    check(board::RigStore::forApplicationData(data).directory()==win.rigs()&&namamp::libraryForApplicationData(data)==win.namAmps()&&tonegrid::tonebank::cacheForApplicationData(data)==win.toneBankCache(),"Existing storage entry points agree with central path policy");
    const tonegrid::paths::Layout linuxLayout(root.getChildFile("opt/ToneGrid"),data,tonegrid::paths::Platform::linuxOS);
    check(win.factoryCabinetIR()==exe.getSiblingFile("FactoryCabinetIR")&&mac.factoryCabinetIR()==mac.resources().getChildFile("FactoryCabinetIR")&&linuxLayout.factoryCabinetIR()==linuxLayout.resources().getChildFile("FactoryCabinetIR"),"FactoryCabinetIR uses native Windows, macOS and Linux resource layouts");
    check(!root.exists(),"Constructing path policy creates no folders or user files");
    const ScopeGuard cleanup{[&]{if(root.isAChildOf(tonegrid::paths::temporaryRoot()))root.deleteRecursively();}};
    const auto exportFolder=root.getChildFile("exports with spaces");
    const auto destination=exportFolder.getChildFile("take "+String::charToString(0x00e9)+".mp3");
    const auto staged=exportFolder.getChildFile("completed.tmp");
    check(exportFolder.createDirectory().wasOk()&&destination.replaceWithText("previous take")&&staged.replaceWithText("completed take"),"Prepare isolated Unicode export destination and previous recording");
    check(tonegrid::platform::commitExport(staged,destination)&&destination.loadFileAsString()=="completed take"&&!staged.exists(),"Completed export replaces previous file using platform publication");
    check(!tonegrid::platform::commitExport(staged,destination)&&destination.loadFileAsString()=="completed take","Failed publication preserves existing export");

    const auto firstLibrary=root.getChildFile("first computer/NAM/Amps");
    const auto secondLibrary=root.getChildFile("second computer/NAM/Amps");
    const String relative="collection/capture "+String::charToString(0x00e9)+".nam";
    const auto firstCapture=firstLibrary.getChildFile(relative),secondCapture=secondLibrary.getChildFile(relative);
    check(firstCapture.getParentDirectory().createDirectory().wasOk()&&secondCapture.getParentDirectory().createDirectory().wasOk()
        &&firstCapture.replaceWithText(namTests::linear())&&firstCapture.copyFileTo(secondCapture),"Copy identical NAM into independent computer libraries");
    namamp::Library first(firstLibrary),second(secondLibrary);namamp::Entry entry,resolvedEntry;nlohmann::json payload;
    check(first.read(firstCapture,entry).wasOk()&&second.resolve(entry.identity,resolvedEntry,payload).wasOk()
        &&resolvedEntry.identity==entry.identity,"Relative Unicode NAM identity resolves independently of installation and user root");
    auto state=board::migrations::initialState();state.name="Portable NAM preset";state.sourceType=amp::SourceType::Nam;
    state.namAmp={entry.identity,-2.f,3.1f};state.input={1.f,-62.f,true};
    const board::RigIdentityLookup identities=[](int pedal,int preset){return "synthetic-"+String(pedal)+"-"+String(preset);};
    auto sourceStore=board::RigStore::forApplicationData(root.getChildFile("first computer/data"));
    auto targetStore=board::RigStore::forApplicationData(root.getChildFile("second computer/data"));
    check(sourceStore.save(0,state,{},identities).result.wasOk()&&targetStore.directory().createDirectory().wasOk()
        &&sourceStore.slotFile(0).copyFileTo(targetStore.slotFile(0)),"Transfer existing-format NAM preset without schema migration");
    board::State restored;PluginDescription unused;
    check(targetStore.loadForSelection(0,identities,restored,unused).wasOk()&&restored.sourceType==amp::SourceType::Nam
        &&restored.namAmp.model==entry.identity&&restored.namAmp.gainDb==-2.f&&restored.namAmp.volumeDb==3.1f
        &&restored.input.gainDb==1.f&&restored.input.thresholdDb==-62.f&&restored.input.gate,"Transferred preset preserves amp, gain, volume and noise gate");
    check(second.resolve(restored.namAmp.model,resolvedEntry,payload).wasOk(),"Transferred preset selects the matching capture in destination library");
    const auto priorEntry=resolvedEntry;const auto priorPayload=payload;
    check(secondCapture.appendText(" "),"Change destination capture bytes without changing JSON validity");
    check(second.resolve(restored.namAmp.model,resolvedEntry,payload).failed()&&resolvedEntry.identity==priorEntry.identity&&payload==priorPayload,"Changed capture rejected without publishing partial model state");
    check(!namamp::validRelativePath("../capture.nam")&&!namamp::validRelativePath("C:\\capture.nam")
        &&!namamp::validRelativePath("/Library/capture.nam"),"Portable presets reject traversal and machine-specific absolute NAM paths");

    const auto shared=root.getChildFile("shared/Audio/Plug-Ins/VST3/TONEX.vst3");
    const auto perUser=root.getChildFile("user/Audio/Plug-Ins/VST3/TONEX.vst3");
    check(perUser.createDirectory().wasOk()&&vst3::preferredTonexLocation(shared,perUser)==perUser,"TONEX lookup accepts user-installed plugin bundles");
    check(shared.createDirectory().wasOk()&&vst3::preferredTonexLocation(shared,perUser)==shared,"Shared TONEX bundle takes precedence when both installations exist");
    check(!vst3::locationExists("TONEX.vst3")&&!vst3::locationExists("../TONEX.vst3"),"Relative VST3 hints cannot select plugins from process working directory");
    PluginDescription saved;saved.name="TONEX";saved.manufacturerName="Synthetic vendor";saved.pluginFormatName="VST3";
    saved.uniqueId=123;saved.deprecatedUid=124;saved.version="1.0";saved.fileOrIdentifier=root.getChildFile("old computer/TONEX.vst3").getFullPathName();
    auto local=saved;local.fileOrIdentifier=perUser.getFullPathName();vst3::Catalog catalog;catalog.add(local);PluginDescription selected;
    check(catalog.resolve(saved,selected).wasOk()&&selected.fileOrIdentifier==local.fileOrIdentifier,"Saved plugin resolves to unique local bundle after rescan");
    auto unsafe=saved;unsafe.fileOrIdentifier="../TONEX.vst3";vst3::Catalog empty;selected.name="sentinel";
    check(empty.resolve(unsafe,selected).failed()&&selected.name=="sentinel","Unresolved foreign or relative plugin hint preserves selected metadata");
    auto changedIdentity=local;++changedIdentity.deprecatedUid;vst3::Catalog different;different.add(changedIdentity);
    check(different.resolve(saved,selected).failed(),"Historical VST3 identity checks remain strict pending real Mac comparison");
    check(root.deleteRecursively()&&!root.exists(),"Export policy tests leave no temporary files");
    report+=String(checks)+" portability checks; "+(ok?"ALL PASSED":"FAILURES")+"\n";return ok;
}
}
