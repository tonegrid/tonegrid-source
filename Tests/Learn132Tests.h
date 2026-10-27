#pragma once
#include "Modules/Learn/UI/LearnView.h"
namespace learn132Tests
{
inline bool run(const juce::File& output,juce::String& report)
{
    using namespace juce;using namespace tonegrid;bool ok=true;auto check=[&](bool pass,const String& name){ok&=pass;report+=String(pass?"PASS ":"FAIL ")+name+"\n";};output.createDirectory();
    const auto source=String::fromUTF8(ToneGridApplicationAssets::lessons_json,ToneGridApplicationAssets::lessons_jsonSize);std::vector<learn::Lesson> lessons;learn::Partner partner;
    check(learn::decode(source,lessons,partner).wasOk()&&lessons.size()==134,"134 validated lesson cards");size_t videoCount=0,descriptions=0;for(const auto& lesson:lessons)for(const auto& video:lesson.videos){++videoCount;if(video.description.isNotEmpty())++descriptions;}check(videoCount==163&&descriptions==160,"all 163 videos, 160 original descriptions; empty descriptions preserved");
    check(learn::partnerLink(partner.url)&&!learn::partnerLink("https://guitarclub.io.evil.test/")&&!learn::safeLink("javascript:alert(1)")&&!learn::localThumbnail("thumbnails/../../private.jpg"),"unsafe links and thumbnail traversal rejected");
    const auto first=lessons.front();check(learn::matches(first,first.artist.substring(0,5),first.style,first.artist,30,Time::fromISO8601("2026-10-14T12:00:00Z"))&&!learn::matches(first,"no matching song",{}, {},0),"search, genre, artist and date filters combine");
    const auto count=lessons.size();check(learn::decode("{broken",lessons,partner).failed()&&lessons.size()==count,"invalid JSON preserves working catalogue");
    const auto favourites=output.getChildFile("favourites.json");favourites.deleteFile();learn::Favourites saved;saved.load(favourites);check(saved.toggle(first.id).wasOk(),"save favourite atomically");learn::Favourites reopened;reopened.load(favourites);check(reopened.contains(first.id)&&reopened.toggle(first.id).wasOk()&&!reopened.contains(first.id),"favourites persist and can be removed");
    const auto file=output.getChildFile("lessons.json");file.replaceWithText(source);
    const auto originals=File(__FILE__).getParentDirectory().getParentDirectory().getChildFile("Resources/Learn");
    for(size_t i=0;i<3;++i){const auto target=output.getChildFile(lessons[i].thumbnail);target.getParentDirectory().createDirectory();originals.getChildFile(lessons[i].thumbnail).copyFileTo(target);}
    learn::View view;view.configure(file,favourites,output.getChildFile("fake-browser"),true);view.setSize(1200,760);check(view.lessonCount()==134&&view.visibleCount()==134,"complete prepopulated grid");
    auto* search=dynamic_cast<TextEditor*>(view.content().findChildWithID("learn-search"));auto* styles=dynamic_cast<ComboBox*>(view.content().findChildWithID("learn-style"));auto* artists=dynamic_cast<ComboBox*>(view.content().findChildWithID("learn-artist"));auto* favouritesOnly=dynamic_cast<ToggleButton*>(view.content().findChildWithID("learn-favourites-only"));
    check(search&&styles&&artists&&favouritesOnly,"all four catalogue controls available");
    if(artists){artists->setText("The Beatles",sendNotificationSync);check(view.visibleCount()>0&&view.visibleCount()<134,"artist filter narrows catalogue");artists->setSelectedId(1,sendNotificationSync);}
    if(styles){styles->setText("Blues",sendNotificationSync);check(view.visibleCount()>0&&view.visibleCount()<134,"style filter narrows catalogue");styles->setSelectedId(1,sendNotificationSync);}
    if(search){search->setText("zzznomatchzzz",false);search->onTextChange();check(view.visibleCount()==0,"empty search result");search->setText({},false);search->onTextChange();}
    view.toggleFavourite(first.id);if(favouritesOnly){favouritesOnly->setToggleState(true,sendNotificationSync);check(view.visibleCount()==1,"favourites-only filter");favouritesOnly->setToggleState(false,sendNotificationSync);}
    auto screenshot=[&](const String& name){MemoryOutputStream png;check(PNGImageFormat().writeImageToStream(view.createComponentSnapshot(view.getLocalBounds(),true,1.5f),png)&&output.getChildFile(name+".png").replaceWithData(png.getData(),png.getDataSize()),"render "+name);};screenshot("learn132-grid");
    for(int width:{760,1100,1660})
    {
        view.setSize(width,820);std::vector<Component*> controls;
        for(const auto& id:{"learn-search","learn-date","learn-style","learn-artist","learn-favourites-only"})controls.push_back(view.content().findChildWithID(id));
        bool fit=true;for(size_t i=0;i<controls.size();++i){fit&=controls[i]&&view.content().getLocalBounds().contains(controls[i]->getBounds());for(size_t j=0;j<i&&fit;++j)fit&=!controls[i]->getBounds().intersects(controls[j]->getBounds());}
        check(fit,"filters remain readable and do not overlap at width "+String(width));screenshot("learn132-grid-"+String(width));
    }
    auto* cta=dynamic_cast<TextButton*>(view.content().findChildWithID("learn-partner"));check(cta&&cta->getButtonText()=="START YOUR FREE TRIAL\non Guitar Club","single two-line trial CTA");view.setSize(1200,760);
    String launched;view.openExternal=[&](const String& url){launched=url;return true;};view.openLesson(first.id);check(view.isDetail()&&view.playerRequested()&&view.playerBounds().getHeight()>200&&std::abs(view.playerBounds().getWidth()/double(view.playerBounds().getHeight())-16./9.)<.01&&launched.isEmpty(),"large 16:9 player opens without external browser");check(view.descriptionText()==first.videos.front().description&&view.descriptionLinks()>0,"original complete description with clickable links");screenshot("learn132-open");
    const auto multi=std::find_if(lessons.begin(),lessons.end(),[](const auto& lesson){return lesson.videos.size()>1;});if(multi!=lessons.end()){view.openLesson(multi->id);view.selectPartForTest(1);check(view.playerRequested()&&view.descriptionText()==multi->videos[1].description,"switch lesson part updates player and original description");}
    view.stopPlayer();check(!view.playerRequested(),"stop when leaving Learn");view.closeLesson();check(!view.isDetail()&&view.visibleCount()==134,"back restores catalogue");view.setSize(760,540);screenshot("learn132-small");
    view.openLesson(first.id);view.reloadIfChanged();check(view.playerRequested(),"unchanged JSON check preserves playback");auto edited=JSON::parse(source);edited.getDynamicObject()->getProperty("lessons").getArray()->removeLast();file.replaceWithText(JSON::toString(edited));view.reloadIfChanged();check(view.lessonCount()==count-1&&!view.playerRequested()&&view.isFavourite(first.id),"saved shorter catalogue reloads safely and preserves favourites");file.replaceWithText("{broken");view.reloadIfChanged();check(view.lessonCount()==count-1&&file.loadFileAsString()=="{broken","invalid file preserves lessons and never overwrites edits");
    report+=ok?"ALL LEARN CHECKS PASSED\n":"LEARN FAILURES\n";return ok;
}
}
