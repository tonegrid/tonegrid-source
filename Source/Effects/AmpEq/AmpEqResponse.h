#pragma once
#include "Effects/AmpEq/AmpEqParameters.h"
#include <juce_dsp/juce_dsp.h>
#include <complex>
namespace tonegrid::ampEq
{
// GUI-only response of the established DSP, including its gain-dependent Mid Q.
inline double responseDb(const Parameters& p,double frequency,double rate=48000.)
{
    if(!p.enabled)return 0;
    using C=juce::dsp::IIR::ArrayCoefficients<double>;
    const auto gain=[](double db){return juce::Decibels::decibelsToGain(db);};const auto cap=[&](double value){return std::min(value,rate*.45);};
    const auto z=std::polar(1.,-juce::MathConstants<double>::twoPi*frequency/rate);double magnitude=1;
    const auto apply=[&](const std::array<double,6>& c){magnitude*=std::abs((c[0]+c[1]*z+c[2]*z*z)/(c[3]+c[4]*z+c[5]*z*z));};
    if(p.bassDb!=0)apply(C::makeLowShelf(rate,cap(150),std::sqrt(.5),gain(p.bassDb)));
    if(p.midDb!=0)apply(C::makePeakFilter(rate,cap(425),p.midDb<0?1.5:.7,gain(p.midDb)));
    if(p.trebleDb!=0)apply(C::makeHighShelf(rate,cap(1800),std::sqrt(.5),gain(p.trebleDb)));
    if(p.lowCut)apply(C::makeHighPass(rate,cap(p.lowCutHz)));
    if(p.highCut)apply(C::makeLowPass(rate,cap(p.highCutHz)));
    if(p.lowMidDb!=0)apply(C::makePeakFilter(rate,cap(p.lowMidHz),p.lowMidQ,gain(p.lowMidDb)));
    if(p.presenceDb!=0)apply(C::makePeakFilter(rate,cap(p.presenceHz),p.presenceQ,gain(p.presenceDb)));
    if(p.depthDb!=0)apply(C::makeLowShelf(rate,cap(p.depthHz),std::sqrt(.5),gain(p.depthDb)));
    return 20*std::log10(std::max(magnitude,1.e-12));
}
}
