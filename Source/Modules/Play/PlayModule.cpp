#include "Modules/Play/PlayModule.h"
#include "Modules/Play/UI/PlayView.h"
namespace tonegrid::modules {
PlayModule::PlayModule(recording::SessionRecorder& recorder, looping::Engine& looper) : content(std::make_unique<play::View>(recorder, looper)) {}
PlayModule::~PlayModule() = default;
juce::Component& PlayModule::view() { return *content; }
play::View& PlayModule::contentView() { return *content; }
void PlayModule::deactivated() {  }
}
