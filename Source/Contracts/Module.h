#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
namespace tonegrid::core {
struct IToneGridModule {
 virtual juce::Component& view()=0;
 virtual void activated(){} virtual void deactivated(){}
 virtual ~IToneGridModule()=default;
};
}
