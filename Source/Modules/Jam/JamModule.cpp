#include "Modules/Jam/JamModule.h"
#include "Modules/Jam/UI/JamView.h"
namespace tonegrid::modules {
JamModule::JamModule() : content(std::make_unique<jam::View>()) {}
JamModule::~JamModule() = default;
juce::Component& JamModule::view() { return *content; }
jam::View& JamModule::contentView() { return *content; }
void JamModule::deactivated() { content->stopPlayer(); }
}
