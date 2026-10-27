#pragma once
#include "Platform/LegacyProfileCleanup.h"
#include "Content/Cabinets/CabinetBrowser.h"
#if JUCE_WINDOWS
#include <windows.h>
#endif
namespace release143Tests
{
inline bool run(const juce::File& evidence,juce::String& report)
{
    using namespace juce;bool ok=true;int checks=0;
    const auto check=[&](bool yes,const String& label){++checks;ok&=yes;report+=String(yes?"PASS ":"FAIL ")+label+"\n";};
    const auto temp=File::getSpecialLocation(File::tempDirectory).getChildFile("ToneGrid-release143-"+Uuid().toString());
    temp.createDirectory();struct Cleanup{File root;~Cleanup(){root.deleteRecursively();}} cleanup{temp};
    const auto legacy=temp.getChildFile("ToneGridIR"),current=temp.getChildFile("ToneGrid"),outside=temp.getChildFile("Personal");
    current.createDirectory();current.getChildFile("keep.txt").replaceWithText("Current settings");outside.createDirectory();outside.getChildFile("keep.txt").replaceWithText("Outside settings");
    legacy.getChildFile("Nested").createDirectory();legacy.getChildFile("Nested/old.txt").replaceWithText("Retired");
    std::atomic_bool cancelled{true};tonegrid::paths::cleanupLegacyProfile(temp,&cancelled);
    check(legacy.getChildFile("Nested/old.txt").existsAsFile(),"Cancelled cleanup leaves legacy data alone");
    {tonegrid::paths::LegacyProfileCleanup worker;worker.start(temp);for(int i=0;i<400&&legacy.exists();++i)Thread::sleep(5);}
    check(!legacy.exists()&&current.getChildFile("keep.txt").loadFileAsString()=="Current settings"&&outside.getChildFile("keep.txt").loadFileAsString()=="Outside settings","Background cleanup removes only the exact retired profile");
    tonegrid::paths::cleanupLegacyProfile(temp);
    check(!current.getChildFile("Nested/old.txt").exists()&&temp.findChildFiles(File::findDirectories,false,"ToneGrid-before-*").isEmpty(),"No data transfer and no profile backup are performed");
    legacy.createDirectory();const auto locked=legacy.getChildFile("locked.txt");locked.replaceWithText("Busy legacy file");
#if JUCE_WINDOWS
    const auto handle=CreateFileW(locked.getFullPathName().toWideCharPointer(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    check(handle!=INVALID_HANDLE_VALUE,"Open a synthetic legacy file without delete sharing");
    tonegrid::paths::cleanupLegacyProfile(temp);
    check(locked.loadFileAsString()=="Busy legacy file"&&current.getChildFile("keep.txt").existsAsFile(),"A Windows deletion failure returns silently and retains the canonical profile");
    if(handle!=INVALID_HANDLE_VALUE)CloseHandle(handle);
#endif
    tonegrid::paths::cleanupLegacyProfile(temp);check(!legacy.exists(),"A later cleanup can remove a formerly busy legacy file");
    const auto linked=outside.createSymbolicLink(legacy,true);
    if(linked){tonegrid::paths::cleanupLegacyProfile(temp);check(outside.getChildFile("keep.txt").existsAsFile(),"Legacy profile links are never followed or recursively removed");legacy.deleteFile();}
    else report+="INFO Directory-link fixture unavailable on this host; link checks are guarded in the implementation.\n";
    tonegrid::cabinet::Entry entry;entry.name="Cabinet";entry.image=Image(Image::RGB,80,80,true);{Graphics g(entry.image);g.fillAll(Colour(0xffc8a078));}
    tonegrid::cabinet::Panel panel;panel.setVisible(true);panel.setSize(300,130);panel.update(&entry,"External cabinet IR",true,false,true,true);
    const auto enabled=panel.createComponentSnapshot(panel.getLocalBounds());panel.update(&entry,"Cabinet simulation off",true,false,true,false);
    const auto disabled=panel.createComponentSnapshot(panel.getLocalBounds());const auto point=panel.artworkBounds().getCentre();
    const auto a=enabled.getPixelAt(point.x,point.y),b=disabled.getPixelAt(point.x,point.y);
    check(std::abs(int(b.getRed())*2-int(a.getRed()))<=2&&std::abs(int(b.getGreen())*2-int(a.getGreen()))<=2&&std::abs(int(b.getBlue())*2-int(a.getBlue()))<=2,"CAB OFF adds exactly 50 percent black over the cabinet face");
    check(panel.power.isEnabled()&&!panel.power.getToggleState(),"CAB OFF keeps the power control usable");
    for(int i=0;i<40;++i)panel.advanceHover(true,1./60.);const auto hover=panel.createComponentSnapshot(panel.getLocalBounds());
    const auto button=panel.getLocalArea(&panel.power,panel.power.getLocalBounds());auto* hit=panel.getComponentAt(button.getCentre());
    check(panel.hoverOpacity()>.99f&&hit==&panel.power,"The disabled-face overlay does not intercept the power button");
    if(hit!=&panel.power)report+="INFO Power hit: "+String(hit?hit->getComponentID():"null")+"; bounds "+button.toString()+"; opacity "+String(panel.hoverOpacity())+"\n";
    evidence.createDirectory();for(const auto& item:std::initializer_list<std::pair<const char*,Image>>{{"cabinet-on.png",enabled},{"cabinet-off.png",disabled},{"cabinet-off-controls.png",hover}})
    {MemoryOutputStream stream;PNGImageFormat png;check(png.writeImageToStream(item.second,stream)&&evidence.getChildFile(item.first).replaceWithData(stream.getData(),stream.getDataSize()),String("Snapshot ")+item.first);}
    panel.update(&entry,"Cabinet included",false,false,false,false);check(!panel.power.isEnabled(),"Full Rig keeps its existing inactive controls");
    const auto bundle=temp.getChildFile("BundledCabinets"),stock=temp.getChildFile("StockCabinets"),custom=temp.getChildFile("CustomCabinets");
    const String oldPath="4x10/Example/thumbnail.png",newPath="4x10/Example/thumbnail.jpg";
    const auto source=bundle.getChildFile(newPath);source.getParentDirectory().createDirectory();
    const auto resources=File(TONEGRID_BANK_FILE).getParentDirectory().getParentDirectory();
    check(resources.getChildFile("FactoryCabinetIR/4x10/Fender Super Reverb - Jensen P10R/thumbnail.jpg").copyFileTo(source),"Stage the new Super Reverb studio photograph");
    const auto stockPng=stock.getChildFile(oldPath),customPng=custom.getChildFile(oldPath);
    stockPng.getParentDirectory().createDirectory();stockPng.replaceWithText("Known old factory diagram");customPng.getParentDirectory().createDirectory();customPng.replaceWithText("User artwork");
    nlohmann::json catalog={{"irs",nlohmann::json::array()},{"recommendations",nlohmann::json::object()},{"assets",nlohmann::json::array({{{"file",newPath.toStdString()},{"sha256",SHA256(source).toHexString().toStdString()}}})},{"retiredAssets",nlohmann::json::array({{{"file",oldPath.toStdString()},{"sha256",SHA256(stockPng).toHexString().toStdString()}}})}};
    bundle.getChildFile("catalog.json").replaceWithText(String(catalog.dump()));
    tonegrid::cabinet::Library installed(stock,bundle);installed.scan(true);
    check(!stockPng.exists()&&SHA256(stock.getChildFile(newPath)).toHexString()==SHA256(source).toHexString(),"An existing stock diagram is replaced by the verified studio JPEG on upgrade");
    tonegrid::cabinet::Library personalized(custom,bundle);personalized.scan(true);
    check(customPng.loadFileAsString()=="User artwork"&&!custom.getChildFile(newPath).exists(),"Modified personal artwork is preserved");
    const auto factory=resources.getChildFile("FactoryCabinetIR");
    const auto factoryDoc=nlohmann::json::parse(factory.getChildFile("catalog.json").loadFileAsString().toStdString());
    const auto& selected=factoryDoc.at("irs").at(0);
    tonegrid::cabinet::State selectedState{String(selected.at("file").get<std::string>()),String(selected.at("sha256").get<std::string>()),false};
    tonegrid::cabinet::Library immediate(factory,factory);auto preview=immediate.previewSelected(selectedState);
    check(preview.image.isValid()&&preview.name==String(selected.at("name").get<std::string>())&&immediate.entries.empty(),"Selected cabinet metadata and artwork load without a full browser scan");
    const auto previewRoot=temp.getChildFile("Preview");const auto selectedFile=previewRoot.getChildFile(selectedState.file);selectedFile.getParentDirectory().createDirectory();factory.getChildFile(selectedState.file).copyFileTo(selectedFile);
    Image customThumb(Image::RGB,16,16,true);{Graphics g(customThumb);g.fillAll(Colours::red);}MemoryOutputStream thumb;PNGImageFormat().writeImageToStream(customThumb,thumb);
    selectedFile.getSiblingFile("thumbnail.png").replaceWithData(thumb.getData(),thumb.getDataSize());
    tonegrid::cabinet::Library customized(previewRoot,factory);auto personal=customized.previewSelected(selectedState);
    check(personal.image.isValid()&&personal.image.getPixelAt(8,8)==Colours::red,"Priority artwork preserves the same local thumbnail precedence as the full browser");
    selectedFile.getSiblingFile(".tonegrid-cabinet.json").replaceWithText("{\"title\":42}");
    auto malformed=customized.previewSelected(selectedState);
    check(malformed.state.sha256==selectedState.sha256&&malformed.image.isValid(),"Malformed optional artwork metadata does not prevent the verified IR preview");
    report+=String(checks)+" release 1.1.43 checks; "+(ok?"ALL PASSED":"FAILURES")+"\n";return ok;
}
}
