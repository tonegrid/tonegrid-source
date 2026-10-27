#pragma once
#include "Modules/Record/RecordController.h"
#include "Contracts/Module.h"
namespace tonegrid::record {
class RecordModule final:public core::IToneGridModule {
public:
 RecordModule(core::ToneService,juce::File sessionFolder,juce::File encoder,juce::File webProfile,bool testing=false);
 ~RecordModule()override;
 juce::Component& view()override{return content;}
 void activated()override{controller.activate();}
 void deactivated()override{controller.deactivate();}
 void shutdown(){controller.shutdown();}
 core::IAudioInsert& audioInsert(){return controller.audio();}
 RecordView& recordView(){return content;}
 RecordController& recordController(){return controller;}
private:RecordView content;RecordController controller;
};
}
