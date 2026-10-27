#pragma once
#include "NamAmpTests.h"
#include "CompactAmpLayout.h"
#include "Shared/UI/Library/NamLibraryBrowser.h"

namespace compactUxTests
{
inline bool run(juce::String& report)
{
    using namespace juce;bool ok=true;int checks=0;auto check=[&](bool pass,const String& name){++checks;ok&=pass;report+=String(pass?"PASS ":"FAIL ")+name+"\n";};
    for(const int width:{1632,1332,2212})
    {
        const int oldHeight=roundToInt(width*280.f/2016);const auto newHeight=tonegrid::ampui::compactHeight(width);juce::Rectangle<int> panel(14,600,width,newHeight);
        check(newHeight>=120&&newHeight<oldHeight&&tonegrid::ampui::panelBox(panel,292,6,370,116).getHeight()>=80,"Head + IR rack reserves usable artwork height at width "+String(width));
        const auto gear=tonegrid::ampui::settingsButtonBounds(panel,0);bool square=true;for(int i=0;i<4;++i){const auto r=tonegrid::ampui::settingsButtonBounds(panel,i);const auto toggle=tonegrid::ampui::effectToggleBounds(panel,i);square&=r.getWidth()==r.getHeight()&&r.getWidth()==gear.getWidth()&&panel.contains(r)&&r.getWidth()>=24&&panel.contains(toggle)&&!toggle.intersects(r);}
        check(square,"All four PostFX toggle/gear pairs have equal usable geometry without overlap at width "+String(width));
        const auto selector=tonegrid::ampui::vstSelectorBounds(panel),actions=tonegrid::ampui::vstActionsBounds(panel);
        check(selector.getRight()==actions.getRight()&&selector.getWidth()<roundToInt(width*720.f/2016)&&!selector.intersects(actions),"VST selector ends at action area and is shorter than 1.1.4 at width "+String(width));
        const auto folder=tonegrid::ampui::folderButtonBounds(panel),normalize=tonegrid::ampui::normalizationBounds(panel);
        const auto text=tonegrid::ampui::captureBounds(panel);
        check(normalize.getHeight()==tonegrid::ui::buttonHeight&&normalize.getHeight()==folder.getHeight()&&normalize.getCentreY()==folder.getCentreY()&&normalize.getRight()<folder.getX()&&text.getRight()<normalize.getX()&&panel.contains(normalize),"Normalization shares Folder horizontal centre on the right with no NAM text overlap at width "+String(width));
        const auto glyph=tonegrid::ampui::folderGlyphBounds(folder);
        check(std::abs(glyph.getWidth()/glyph.getHeight()-24.f/20.f)<1e-5&&glyph.getCentre()==folder.toFloat().getCentre()&&folder.toFloat().contains(glyph),"Folder glyph retains centred 24:20 aspect independent of button proportions at width "+String(width));
    }
    const auto temp=File::getSpecialLocation(File::tempDirectory),root=temp.getChildFile("ToneGrid-tree-"+Uuid().toString());ScopeGuard cleanup{[&]{if(root.isAChildOf(temp))root.deleteRecursively();}};
    namamp::Library library(root);namamp::Catalog catalog;tonegrid::NamLibraryBrowser browser;
    auto refresh=[&](const String& active=String{}){const auto result=library.refresh(catalog);browser.update(catalog,root,active);return result.wasOk();};
    auto put=[&](const String& path){auto file=root.getChildFile(path);file.getParentDirectory().createDirectory();return file.replaceWithText(namTests::linear());};
    check(refresh()&&browser.totalCaptures()==0&&browser.visibleRows()==0,"Empty library has no loadable placeholder");
    put("Z.nam");put("a.nam");check(refresh()&&browser.totalCaptures()==2&&browser.rowPath(0)=="a.nam","Root captures sort by case-insensitive name");
    put("z/Clean.nam");put("a/Nested/Clean.nam");put("a/z.nam");put("a/A.nam");check(refresh()&&browser.totalCaptures()==6,"All Captures includes every physical recursive NAM once");
    browser.navigate("a");check(browser.visibleRows()==3&&browser.rowPath(0)=="a/Nested"&&browser.rowPath(1)=="a/A.nam","Mixed folder contents sort directories first");
    int selectedCount=0;String selected;browser.onSelect=[&](const String& path){++selectedCount;selected=path;};
    browser.activate("a/Nested",false);browser.handleUpdateNowIfNeeded();check(browser.folder()=="a/Nested"&&selectedCount==0,"Folder navigation never loads a model");
    auto* first=browser.find("z/Clean.nam");auto* second=browser.find("a/Nested/Clean.nam");check(first!=second&&first->label==second->label&&first->path!=second->path,"Duplicate filenames retain distinct physical paths");
    first->activate();check(selectedCount==0&&browser.isUpdatePending(),"Capture activation is deferred beyond mouse traversal");browser.handleUpdateNowIfNeeded();check(selectedCount==1&&selected=="z/Clean.nam","Selection delivers full relative path");
    second->activate();browser.handleUpdateNowIfNeeded();check(selectedCount==2&&selected=="a/Nested/Clean.nam","Duplicate capture resolves to correct directory");
    browser.onRefresh=[&]{refresh("a/Nested/Clean.nam");};browser.reveal("a/Nested/Clean.nam");root.getChildFile("Empty New Folder").createDirectory();browser.refresh.onClick();
    check(browser.find("Empty New Folder")&&browser.folder()=="a/Nested"&&browser.selected()=="a/Nested/Clean.nam","Refresh indexes empty folders and preserves navigation/selection");
    put("Empty New Folder/Added.nam");browser.refresh.onClick();check(browser.find("Empty New Folder/Added.nam"),"Refresh discovers new NAM");
    root.getChildFile("z/Clean.nam").deleteFile();browser.refresh.onClick();check(!browser.find("z/Clean.nam")&&browser.find("z")&&browser.find("a/Nested/Clean.nam"),"Refresh removes only deleted capture");
    File opened;browser.openDirectory=[&](const File& target){opened=target;return true;};browser.openFolder.onClick();check(opened==root,"Open Folder uses injected library root");
    check(browser.openFolder.getY()==browser.refresh.getY()&&browser.createComponentSnapshot(browser.getLocalBounds()).isValid(),"Three-panel browser and aligned footer render");
    root.getChildFile("a/Nested/Clean.nam").deleteFile();browser.refresh.onClick();check(!browser.find("a/Nested/Clean.nam")&&browser.selected().isEmpty(),"Deleted active capture is not substituted");
    report+=String(checks)+" compact layout/tree checks; "+(ok?"ALL PASSED":"FAILURES")+"\n";return ok;
}
}
