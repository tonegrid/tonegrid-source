#pragma once
#include "Modules/Play/UI/PedalCard.h"
#include "RealtimeAllocationProbe.h"
#include <set>

namespace pedalLibrary119Tests
{
inline bool run(const juce::File& directory,juce::String& report)
{
    using namespace juce;bool ok=true;int checks=0;
    auto check=[&](bool pass,const String& text){++checks;ok&=pass;report+=(pass?"PASS ":"FAIL ")+String("PedalLibrary119 ")+text+"\n";};
    constexpr std::array<int,13> counts{8,8,18,6,18,16,8,20,24,8,21,20,8};
    check(board::presetCounts==counts,"nine selected bank counts and four unchanged algorithmic banks");
    board::Processor processor;auto result=processor.load(directory);check(result.wasOk(),"all production NAM files load/prewarm: "+result.getErrorMessage());if(result.failed())return false;
    auto catalog=XmlDocument::parse(directory.getChildFile("catalog.xml"));check(catalog&&catalog->getNumChildElements()==9,"one catalog with exactly nine NAM banks including MaestrodiMusica Blues Driver");
    int total=0,complete=0,inputOnly=0,missing=0;std::set<String> hashes;
    Image preview(Image::RGB,1050,1470,true);int previewIndex=0;
    for(int visual=0;visual<board::count;++visual)
    {
        const int pedal=board::order[size_t(visual)];if(!board::isNam(pedal))continue;
        auto* bank=catalog->getChildByAttribute("id",String(pedal));
        check(bank&&bank->getIntAttribute("recommended",-1)==board::recommendedPreset(pedal)&&processor.get(pedal).preset==board::recommendedPreset(pedal),"catalog and fresh selection agree on Recommended for "+String(pedal));
        PedalCard card(visual,processor);card.setSize(224,288);auto* picker=dynamic_cast<ComboBox*>(card.findChildWithID("pedal-preset"));
        bool captures=true;int headers=0;if(picker){for(int n=0;n<counts[size_t(pedal)];++n)captures&=picker->indexOfItemId(n+1)>=0;for(int i=0;i<picker->getNumItems();++i)headers+=picker->getItemId(i)>=PedalCard::creatorFirstItem;}
        check(picker&&captures&&picker->getNumItems()==counts[size_t(pedal)]+1+headers&&picker->getItemId(picker->getNumItems()-1)==PedalCard::midiLearnItem&&picker->getText().contains("Recommended"),"actual dropdown exposes the full unchanged bank, recommended selection and final MIDI action for "+String(pedal));
        if(!picker)continue;
        {Graphics g(preview);g.drawImageAt(card.createComponentSnapshot(card.getLocalBounds()),(previewIndex%3)*350+63,(previewIndex/3)*490);g.setColour(Colours::white);g.setFont(14.f);g.drawFittedText(processor.label(pedal,board::recommendedPreset(pedal)),{(previewIndex%3)*350+12,(previewIndex/3)*490+300,326,150},Justification::centred,6);++previewIndex;}
        for(int n=0;n<counts[size_t(pedal)];++n)
        {
            ++total;auto* item=bank->getChildElement(n);check(item!=nullptr,"catalog row exists "+String(pedal)+"/"+String(n));if(!item)continue;const auto file=directory.getChildFile(processor.modelFile(pedal,n));const auto info=processor.calibrationInfo(pedal,n);
            complete+=info.complete();inputOnly+=info.input.has_value()&&!info.output.has_value();missing+=!info.input.has_value()&&!info.output.has_value();
            check(file.isAChildOf(directory)&&file.existsAsFile()&&SHA256(file).toHexString()==processor.identity(pedal,n)&&hashes.insert(processor.identity(pedal,n)).second,"capture path, original hash and uniqueness "+String(pedal)+"/"+String(n));
            check(item->getStringAttribute("label")==processor.label(pedal,n)&&processor.details(pedal,n).contains("https://www.tone3000.com/tones/")&&item->getStringAttribute("sourceFile").endsWith(".nam"),"display name and source traceability "+String(pedal)+"/"+String(n));
            picker->setSelectedId(n+1,sendNotificationSync);check(processor.get(pedal).preset==n&&picker->getText()==processor.label(pedal,n),"dropdown selects distinct capture "+String(pedal)+"/"+String(n));
        }
    }
    check(total==151&&complete==151&&inputOnly==0&&missing==0,"actual metadata: all 151 captures have both input/output references");
    if(auto stream=File::getSpecialLocation(File::currentExecutableFile).getSiblingFile("PedalLibrary119.png").createOutputStream()){stream->setPosition(0);stream->truncate();PNGImageFormat().writeImageToStream(preview,*stream);}
    const auto old=processor.get(3);auto invalid=old;invalid.preset=board::presetCount(3)+1;invalid.enabled=true;processor.set(3,invalid);
    check(!processor.get(3).enabled&&processor.get(3).preset==old.preset&&!processor.available(3,invalid.preset)&&processor.identity(3,invalid.preset).isEmpty(),"out-of-range Orange selection bypasses without substitution or unsafe access");
    invalid.preset=board::presetCount(3);processor.set(3,invalid);
    check(!processor.get(3).enabled&&!processor.available(3,invalid.preset)&&processor.identity(3,invalid.preset).isEmpty(),"uninstalled reserved personal slot stays bypassed");
    check(processor.setNamLite(true)==151,"all 151 A2 models support native Lite switching");
    for(int p=0;p<board::count;++p)processor.set(p,board::defaults(p,board::recommendedPreset(p)));
    AudioBuffer<float> audio(2,128);MidiBuffer midi;
    auto renderAll=[&](const String& quality){for(int p=0;p<board::count;++p)if(board::isNam(p))for(int n=0;n<board::presetCount(p);++n)
    {
        auto setting=board::defaults(p,n);setting.enabled=true;processor.set(p,setting);processor.prepareToPlay(48000,128);bool finite=true;double energy=0;
        for(int block=0;block<16;++block){for(int i=0;i<128;++i)audio.setSample(0,i,.03f*std::sin(float(block*128+i)*.031f));processor.processBlock(audio,midi);for(int i=0;i<128;++i){const float sample=audio.getSample(0,i);finite&=std::isfinite(sample)&&sample==audio.getSample(1,i);energy+=sample*sample;}}
        check(finite&&energy>1.e-12&&!processor.fault.load(),quality+" processes finite non-silent audio "+String(p)+"/"+String(n));setting.enabled=false;processor.set(p,setting);
    }};
    renderAll("Native Lite");
    check(processor.setNamLite(false)==151,"all models return to Full without additional sonic presets");
    renderAll("Native Full");
    for(int p=0;p<board::count;++p){auto s=board::defaults(p,board::recommendedPreset(p));s.enabled=true;processor.set(p,s);}
    processor.prepareToPlay(48000,128);audio.clear();realtimeProbe::begin();processor.processBlock(audio,midi);const auto allocations=realtimeProbe::end();check(allocations==0,"variable model storage never allocates in the audio callback");
    report+=String(checks)+" pedal library 1.1.19 checks; "+(ok?"ALL PASSED":"FAILURES")+"\n";return ok;
}
}
