#pragma once
#include "NamAmp.h"
#include "Content/Presets/RigStore.h"
#include "RigSupport.h"

inline bool runNamAmpIntegrationTests(const juce::File& original,const juce::File& models,juce::String& report)
{
    using namespace juce;bool ok=true;int checks=0;auto check=[&](bool good,const String& text){++checks;ok&=good;report+=String(good?"PASS ":"FAIL ")+text+"\n";};
    const auto temp=File::getSpecialLocation(File::tempDirectory),root=temp.getChildFile("ToneGrid-real-nam-test-"+Uuid().toString());
    ScopeGuard cleanup{[&]{if(root.isAChildOf(temp))root.deleteRecursively();}};
    namamp::Library library(namamp::libraryForApplicationData(root));namamp::Catalog catalog;
    check(original.existsAsFile()&&original.hasFileExtension("nam"),"Provided real AMP+CAB capture exists locally");if(!ok)return false;
    const auto originalHash=SHA256(original).toHexString();
    check(library.refresh(catalog).wasOk()&&original.copyFileTo(library.folder().getChildFile(original.getFileName())),"Copy real capture only into temporary test library, no redistribution");
    check(library.refresh(catalog).wasOk()&&catalog.entries.size()==1,"Real local NAM metadata discovery finds one eligible model");if(catalog.entries.empty())return false;
    const auto identity=catalog.entries[0].identity;
    report+="PHYSICALLY TESTED "+original.getFullPathName()+" | "+catalog.entries[0].architecture+" | 48000 Hz | SHA256 "+identity.sha256+"\n";
    const board::RigIdentityLookup ids=[](int p,int preset){return "Synthetic pedal "+String(p)+"/"+String(preset);};
    auto state=board::migrations::initialState();state.sourceType=amp::SourceType::Nam;state.namAmp={identity,5,-8};state.name="Real local AMP+CAB";state.input.gainDb=2;
    const auto store=board::RigStore::forApplicationData(root);PluginDescription none,required;board::State saved;
    check(store.save(0,state,none,ids).result.wasOk()&&store.loadForSelection(0,ids,saved,required).wasOk()&&saved.namAmp.model==identity&&saved.namAmp.gainDb==5&&saved.namAmp.volumeDb==-8,"Real NAM amp rig v8 saves and loads complete typed source/controls");
    auto loaded=amp::loadNamAmp(library,saved.namAmp);
    check(loaded.source&&loaded.result.wasOk(),"Real NAM AMP+CAB inference model loads and prewarms outside callback");if(!loaded.source){report+=loaded.result.getErrorMessage()+"\n";return false;}
    auto* source=dynamic_cast<amp::NamAmp*>(loaded.source.get());
    MemoryBlock controlState;source->captureState(controlState);source->controls(-4,3);source->restoreState(controlState);
    check(source->state().gainDb==5&&source->state().volumeDb==-8,"Real NAM controls restore without GUI or model rebuild");
    auto recreated=amp::loadNamAmp(library,saved.namAmp);
    check(recreated.source&&dynamic_cast<amp::NamAmp*>(recreated.source.get())->state().model==identity&&dynamic_cast<amp::NamAmp*>(recreated.source.get())->state().gainDb==5,"Real NAM saved source survives full recreation");
    recreated.source.reset();
    rig::StereoGraph graph;graph.setPlayConfigDetails(2,2,48000,128);using IO=AudioProcessorGraph::AudioGraphIOProcessor;
    auto in=graph.addNode(std::make_unique<IO>(IO::audioInputNode)),out=graph.addNode(std::make_unique<IO>(IO::audioOutputNode));
    auto pedals=std::make_unique<board::Processor>();const auto loadedPedals=pedals->load(models);
    check(loadedPedals.wasOk(),"Real NAM amp uses unchanged thirteen-pedal engine");if(loadedPedals.failed())return false;
    auto pedalNode=graph.addNode(std::move(pedals)),ampNode=source->addToGraph(graph);
    check(ampNode&&rig::connectStereoChain(graph,in->nodeID,pedalNode->nodeID,ampNode->nodeID,out->nodeID),"Real NAM remains after PedalChain and before shared stereo output");
    graph.prepareToPlay(48000,128);AudioBuffer<float> audio(2,128);MidiBuffer midi;double energy=0;bool finite=true,equal=true;
    for(int block=0;block<240;++block)
    {
        for(int channel=0;channel<2;++channel)for(int i=0;i<128;++i)audio.setSample(channel,i,.02f*(float)std::sin(2*MathConstants<double>::pi*110*(block*128+i)/48000));
        graph.processBlock(audio,midi);for(int i=0;i<128;++i){const auto value=audio.getSample(0,i);finite&=std::isfinite(value)&&std::isfinite(audio.getSample(1,i));equal&=value==audio.getSample(1,i);energy+=(double)value*value;}
    }
    check(finite&&energy>1.e-9&&equal,"Real NAM produces finite non-silent audio, mono inference duplicated to stereo");
    source->controls(0,0);graph.releaseResources();graph.prepareToPlay(48000,513);audio.setSize(2,513);audio.clear();graph.processBlock(audio,midi);
    check(!source->takeFault(),"Real NAM reprepare/larger callback remains prepared through bounded 128-frame slicing");graph.releaseResources();
    const auto render=[&](float gain,float volume,std::vector<float>& data)
    {
        auto candidate=amp::loadNamAmp(library,{identity,gain,volume});if(!candidate.source)return false;
        rig::StereoGraph isolated;auto node=candidate.source->addToGraph(isolated);auto* processor=dynamic_cast<amp::NamAmpProcessor*>(node->getProcessor());processor->prepareToPlay(48000,128);
        AudioBuffer<float> buffer(2,128);MidiBuffer messages;
        for(int block=0;block<120;++block){for(int i=0;i<128;++i)buffer.setSample(0,i,.02f*(float)std::sin(2*MathConstants<double>::pi*110*(block*128+i)/48000));processor->processBlock(buffer,messages);for(int i=0;i<128;++i)data.push_back(buffer.getSample(0,i));}
        candidate.source->unload();isolated.clear();return true;
    };
    std::vector<float> neutral,quiet,driven;
    const bool rendered=render(0,0,neutral)&&render(0,-12,quiet)&&render(6,0,driven);
    check(rendered&&neutral.size()==quiet.size()&&neutral.size()==driven.size(),"Real NAM Gain/Volume can be set at construction without GUI");
    bool levelCorrect=rendered,driveDifferent=false;
    if(rendered)for(size_t i=0;i<neutral.size();++i){levelCorrect&=std::abs(quiet[i]-neutral[i]*Decibels::decibelsToGain(-12.f))<1.e-6f;driveDifferent|=std::abs(driven[i]-neutral[i])>1.e-6f;}
    check(levelCorrect,"Real NAM Volume -12 dB scales identical inference after model");check(driveDifferent,"Real NAM Gain +6 dB changes model response before inference");
    const auto rigBefore=store.slotFile(0).loadFileAsString();library.folder().getChildFile(identity.file).deleteFile();
    auto missing=amp::loadNamAmp(library,saved.namAmp);check(missing.result.failed()&&!missing.source&&source->loaded()&&store.slotFile(0).loadFileAsString()==rigBefore,"Real missing NAM recall is rejected, previous model and rig retained");
    check(SHA256(original).toHexString()==originalHash,"Original user NAM file remained byte-for-byte unchanged");
    source->unload();graph.clear();check(root.deleteRecursively()&&!root.exists(),"Temporary real NAM library/rig storage removed");
    report+=String(checks)+" real NAM amp integration checks; "+(ok?"ALL PASSED":"FAILURES")+"\n";return ok;
}
