#include "Modules/Learn/LearnModule.h"
#include "Modules/Learn/UI/LearnView.h"
namespace tonegrid::modules {
LearnModule::LearnModule() : content(std::make_unique<learn::View>()) {}
LearnModule::~LearnModule() = default;
juce::Component& LearnModule::view() { return *content; }
learn::View& LearnModule::contentView() { return *content; }
void LearnModule::deactivated() { content->stopPlayer(); }
}
