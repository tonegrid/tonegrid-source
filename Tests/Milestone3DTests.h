#pragma once
#include "AmpUi.h"
#include "Modules/Play/UI/PedalCard.h"
#include "Content/Presets/RigStore.h"
#include "PedalLibrary119Tests.h"

namespace milestone3dTests
{
inline bool run(const juce::File& directory,juce::String& report)
{
    using namespace juce;bool ok=true;int checks=0;
    auto check=[&](bool good,const String& text){++checks;ok&=good;report+=String(good?"PASS ":"FAIL ")+text+"\n";};
    for(const auto limits:std::array<std::array<double,2>,2>{{{-12,12},{-24,12}}})
    {
        Slider slider;tonegrid::ampui::configure(slider,limits[0],limits[1]);
        check(slider.getMinimum()==limits[0]&&slider.getMaximum()==limits[1],"NAM UI correction preserves original dB range");
        slider.setValue(0,dontSendNotification);check(slider.getValue()==0&&slider.valueToProportionOfLength(0)==.5,"NAM UI zero dB is exactly centered without changing parameter value");
        check(slider.valueToProportionOfLength(limits[0])==0&&slider.valueToProportionOfLength(limits[1])==1,"NAM UI minimum/maximum reach full counter-clockwise/clockwise");
        bool roundTrip=true;double previous=-1;
        for(int i=0;i<=360;++i){const auto v=limits[0]+(limits[1]-limits[0])*i/360.;const auto p=slider.valueToProportionOfLength(v);roundTrip&=p>=previous&&std::abs(slider.proportionOfLengthToValue(p)-v)<1e-12;previous=p;}
        check(roundTrip,"NAM bipolar visual mapping is monotonic and preserves every dB value on round trip");
        slider.setValue(-8,dontSendNotification);check(slider.getValue()==-8,"NAM UI retains saved negative dB exactly");
    }
    ok=pedalLibrary119Tests::run(directory,report)&&ok;
    report+=String(checks)+" milestone 3D checks; "+(ok?"ALL PASSED":"FAILURES")+"\n";return ok;
}
}
