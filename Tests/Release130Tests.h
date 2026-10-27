#pragma once
#include "Modules/Jam/UI/JamView.h"
#include "Effects/AmpEq/AdvancedEqControls.h"
#include "Effects/AmpEq/AmpEqProcessor.h"
#include <map>
namespace release130Tests
{
inline bool run(const juce::File& output,juce::String& report)
{
    using namespace juce;using namespace tonegrid;bool ok=true;int checks=0;auto check=[&](bool pass,const String& name){++checks;ok&=pass;report+=String(pass?"PASS ":"FAIL ")+name+"\n";};output.createDirectory();
    check(jam::scales().size()==42,"42 named scales");StringArray ids;
    for(const auto& scale:jam::scales())
    {
        const auto degrees=StringArray::fromTokens(scale.formula," ","");check(!ids.contains(scale.id)&&degrees.size()==int(scale.notes.size()),"unique scale and complete formula: "+scale.name);ids.add(scale.id);
        bool valid=true;int previous=-1;for(size_t n=0;n<scale.notes.size();++n)
        {
            const auto d=degrees[int(n)];const int degree=d.retainCharacters("1234567").getIntValue();const std::array<int,7> major{0,2,4,5,7,9,11};const int accidental=d.retainCharacters("#").length()-d.retainCharacters("b").length();valid&=degree>=1&&degree<=7&&scale.notes[n]==jam::pitch(major[size_t(jlimit(1,7,degree)-1)]+accidental)&&scale.notes[n]>previous;previous=scale.notes[n];
        }check(valid,"interval formula agrees with pitch classes: "+scale.name);
        for(int root=0;root<12;++root)
        {
            bool mapped=true;for(int open:jam::openMidi)for(int fret=0;fret<=22;++fret){const int pc=jam::pitch(open+fret-root);const auto d=jam::degreeAt(scale,pc);if(d.isNotEmpty())mapped&=std::find(scale.notes.begin(),scale.notes.end(),pc)!=scale.notes.end();}
            check(mapped,"six strings / 22 frets map "+scale.id+" key "+String(root));
        }
    }
    check(jam::noteName(6,11,"7")=="E#"&&jam::noteName(1,4,"3")=="F"&&jam::noteName(10,3,"b3")=="Db","correct enharmonic spelling F# / Db / Bb");
    const std::vector<int> expected{0,3,5,8,10,12,15,17,20,22};std::vector<int> actual;for(int f=0;f<=22;++f)if(jam::degreeAt(jam::scales()[0],jam::pitch(64+f-9)).isNotEmpty())actual.push_back(f);check(actual==expected,"A minor pentatonic high-E positions agree with independent reference");
    std::vector<jam::Track> tracks;const auto json=String::fromUTF8(ToneGridApplicationAssets::backingtracks_json,ToneGridApplicationAssets::backingtracks_jsonSize);check(jam::decodeCatalogue(json,tracks).wasOk()&&tracks.size()==900,"900 bundled valid YouTube links");
    std::map<String,int> styles;StringArray urls;for(const auto& t:tracks){styles[t.style]++;check(!urls.contains(t.url),"unique backing link "+t.id);urls.add(t.url);}check(styles.size()==9,"nine styles");for(const auto& [name,count]:styles)check(count==100,"100 tracks for "+name);
    const auto first=tracks[0];check(jam::matches(first,first.style,jam::rootFor(first.key),first.bpm,first.bpm)&&!jam::matches(first,"Wrong style",-1,20,400)&&!jam::matches(first,{},(jam::rootFor(first.key)+1)%12,20,400),"style/key/tempo filters include exact bounds");
    check(!jam::safeTrackUrl("file:///C:/Windows/notepad.exe")&&!jam::safeTrackUrl("https://www.youtube.com.evil.test/watch?v=abcdefghijk")&&!jam::safeTrackUrl("https://www.youtube.com/watch?v=bad"),"catalogue cannot launch arbitrary schemes or hosts");
    const size_t before=tracks.size();check(jam::decodeCatalogue("{broken",tracks).failed()&&tracks.size()==before,"invalid edits keep existing catalogue");
    jam::View jam;jam.configurePlayer(output.getChildFile("fake-browser"),true);jam.setSize(1200,560);jam.setCatalogueFile(output.getChildFile("missing.json"));check(jam.trackCount()==900,"offline embedded catalogue fallback");
    auto* scale=dynamic_cast<ComboBox*>(jam.content().findChildWithID("jam-scale"));auto* key=dynamic_cast<ComboBox*>(jam.content().findChildWithID("jam-key"));auto* style=dynamic_cast<ComboBox*>(jam.content().findChildWithID("jam-style"));check(scale&&key&&style,"real JAM selectors exist");
    check(!jam.content().findChildWithID("jam-notes")&&!jam.content().findChildWithID("jam-intervals")&&!jam.content().findChildWithID("jam-reload")&&!jam.content().findChildWithID("jam-edit-catalogue"),"simplified JAM removes redundant controls");
    if(scale&&key)for(size_t s=0;s<jam::scales().size();++s){scale->setSelectedId(int(s)+1,sendNotificationSync);for(int k=1;k<=12;++k)key->setSelectedId(k,sendNotificationSync);}if(scale)scale->setSelectedId(1,sendNotificationSync);if(key)key->setSelectedId(10,sendNotificationSync);
    auto snapshot=[&](Component& component,const String& name){MemoryOutputStream png;check(PNGImageFormat().writeImageToStream(component.createComponentSnapshot(component.getLocalBounds(),true,1.5f),png)&&output.getChildFile(name+".png").replaceWithData(png.getData(),png.getDataSize()),"render "+name);};snapshot(jam,"jam130-wide");jam.setSize(780,470);snapshot(jam,"jam130-small");
    auto* list=dynamic_cast<ListBox*>(jam.content().findChildWithID("jam-tracks"));
    String launched;jam.launch=[&](const String& address){launched=address;return true;};
    if(list){jam.playTrackForTest(0);check(jam.playerRequested()&&jam.playerBounds().getHeight()==200&&jam.playerBounds().getWidth()==356&&launched.isEmpty()&&key&&key->getSelectedId()==jam::rootFor(first.key)+1,"inline Play expands 356x200 player and updates key without launching browser");check(list->getModel()->getNumRows()==904,"one accordion inserts exactly four spacer rows");snapshot(jam,"jam131-accordion");jam.playTrackForTest(1);check(jam.activeTrack()==tracks[1].id&&list->getModel()->getNumRows()==904,"switching tracks replaces the one open player");jam.stopPlayer();check(!jam.playerRequested()&&jam.activeTrack().isEmpty()&&list->getModel()->getNumRows()==900,"collapse stops playback and removes accordion rows");}

    if(style&&list){style->setText("Pop",sendNotificationSync);check(list->getModel()->getNumRows()==100,"style filter shows 100 Pop tracks");}
    const auto edited=output.getChildFile("editable-tracks.json");edited.replaceWithText(json);jam.setCatalogueFile(edited);check(jam.trackCount()==900,"editable JSON loaded");
    if(style)style->setSelectedId(1,sendNotificationSync);jam.playTrackForTest(0);jam.reloadIfChanged();check(jam.playerRequested(),"unchanged catalogue check preserves active playback");
    auto changed=JSON::parse(json);changed.getDynamicObject()->getProperty("tracks").getArray()->removeLast();edited.replaceWithText(JSON::toString(changed));jam.reloadIfChanged();check(jam.trackCount()==899&&!jam.playerRequested(),"saved JSON changes automatically reload the catalogue and stop the old selection");
    edited.replaceWithText(json);jam.reloadIfChanged();check(jam.trackCount()==900,"restored JSON automatically reloads all 900 tracks");edited.replaceWithText("{broken");jam.reloadIfChanged();check(jam.trackCount()==900&&edited.loadFileAsString()=="{broken","invalid edited JSON preserves working content and never overwrites user's file");
    check(media::embedAddress(first.url).startsWith("https://www.youtube.com/embed/")&&media::embedAddress("https://evil.test/").isEmpty(),"embed URL accepts only validated YouTube links");
    // Independent signal measurement validates the plotted response against the real audio filters.
    for(int setting=0;setting<5;++setting)
    {
        ampEq::Parameters p;if(setting==0){p.bassDb=5;p.midDb=-4;p.trebleDb=3;}if(setting==1){p.lowCut=true;p.highCut=true;p.lowCutHz=90;p.highCutHz=7000;}if(setting==2){p.lowMidDb=-5;p.lowMidHz=500;p.lowMidQ=2;p.presenceDb=4;p.presenceHz=2600;p.presenceQ=.7f;p.depthDb=3;}if(setting==3){p.bassDb=-3;p.midDb=4;p.trebleDb=-2;p.depthDb=-4;p.depthHz=180;}if(setting==4){p.enabled=false;p.bassDb=12;}
        for(double hz:{60.,150.,425.,1000.,3000.,10000.})
        {
            ampEq::Processor dsp;dsp.prepare(48000);dsp.setTarget(p,true);AudioBuffer<float> b(1,128);double sumIn=0,sumOut=0;int sample=0;
            for(int block=0;block<220;++block){for(int i=0;i<128;++i)b.setSample(0,i,float(.01*std::sin(MathConstants<double>::twoPi*hz*(sample+i)/48000.)));dsp.process(b);if(block>=120)for(int i=0;i<128;++i){const double x=.01*std::sin(MathConstants<double>::twoPi*hz*(sample+i)/48000.);sumIn+=x*x;sumOut+=double(b.getSample(0,i))*b.getSample(0,i);}sample+=128;}
            const double measured=10*std::log10(sumOut/sumIn),predicted=ampEq::responseDb(p,hz);check(std::abs(measured-predicted)<.045,"graph matches rendered audio at "+String(hz)+" Hz, case "+String(setting));
        }
    }
    ampui::AdvancedEqControls eq;for(int width:{940,700,560})
    {
        eq.setSize(width,eq.popupContentHeight(width));for(int band=0;band<8;++band){eq.selectBand(band);for(int i:{0,1,2,3,4,9,11}){auto* c=eq.findChildWithID("amp-eq-"+String(i));check(c&&c->isVisible()&&eq.getLocalBounds().contains(c->getBounds()),"essential EQ control visible at "+String(width));}}
        snapshot(eq,"eq130-"+String(width));
    }
    report+=String(checks)+" release130 checks; "+(ok?"ALL PASSED":"FAILURES")+"\n";return ok;
}
}
