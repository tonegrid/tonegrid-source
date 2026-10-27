#pragma once
#include "Contracts/Module.h"
#include <memory>
namespace tonegrid::recording { class SessionRecorder; }
namespace tonegrid::looping { class Engine; }
namespace tonegrid::play { class View; }
namespace tonegrid::modules {
class PlayModule final : public core::IToneGridModule {
public:
    PlayModule(recording::SessionRecorder&, looping::Engine&);
    ~PlayModule() override;
    juce::Component& view() override;
    play::View& contentView();
    void deactivated() override;
private:
    std::unique_ptr<play::View> content;
};
}
