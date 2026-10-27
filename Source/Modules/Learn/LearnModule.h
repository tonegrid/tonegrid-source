#pragma once
#include "Contracts/Module.h"
#include <memory>
namespace tonegrid::learn { class View; }
namespace tonegrid::modules {
class LearnModule final : public core::IToneGridModule {
public:
    LearnModule();
    ~LearnModule() override;
    juce::Component& view() override;
    learn::View& contentView();
    void deactivated() override;
private:
    std::unique_ptr<learn::View> content;
};
}
