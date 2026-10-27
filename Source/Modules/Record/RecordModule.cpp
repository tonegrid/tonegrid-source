#include "Modules/Record/RecordModule.h"
namespace tonegrid::record {
RecordModule::RecordModule(core::ToneService tone,juce::File folder,juce::File encoder,juce::File webProfile,bool testing):content(std::move(webProfile),testing),controller(content,std::move(tone),std::move(folder),std::move(encoder)){}
RecordModule::~RecordModule(){controller.shutdown();}
}
