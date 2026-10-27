#pragma once
#include "Contracts/Module.h"
#include <memory>
namespace tonegrid::jam { class View; }
namespace tonegrid::modules {
class JamModule final : public core::IToneGridModule {
public:
    JamModule();
    ~JamModule() override;
    juce::Component& view() override;
    jam::View& contentView();
    void deactivated() override;
private:
    std::unique_ptr<jam::View> content;
};
}
