#pragma once
#include "Core/ToneService.h"
#include "Effects/Pedalboard/PedalBoard.h"
#include "NamAmp.h"
#include "CabinetProcessor.h"
#include "EffectsNodes.h"
namespace tonegrid::core {
struct GraphStages {
 juce::AudioProcessorGraph::Node::Ptr amp;
 juce::AudioProcessorGraph::NodeID pedal,output,normalization,cabinet;
 cabinet::Node* cabinetProcessor=nullptr;output::AmpEqNode* eq=nullptr;
 output::NormalizationNode* normalize=nullptr;output::SpreadNode* spread=nullptr;
 output::ModNode* mod=nullptr;output::DelayNode* delay=nullptr;output::ReverbNode* reverb=nullptr;
 bool connected=false;
};
GraphStages buildGuitarGraph(juce::AudioProcessorGraph&,std::unique_ptr<board::Processor>,amp::AmpSource&);
void configureGuitarGraph(const GraphStages&,board::Processor&,amp::AmpSource&,const output::Settings&,calibration::Settings,bool cabinetEnabled);
struct RenderResources {
 juce::File models,namLibrary,cabinetLibrary,userPedals;bool lite=false;
 calibration::Settings calibration;
 juce::File factoryAmps;
};
std::unique_ptr<IRenderProcessor> createRenderProcessor(const juce::String& canonicalXml,double rate,const RenderResources&,juce::String& error);
}
