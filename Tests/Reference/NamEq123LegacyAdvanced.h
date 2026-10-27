#pragma once
#include "Effects/AmpEq/AmpEqParameters.h"
#include <juce_dsp/juce_dsp.h>
namespace tonegrid::legacyEq123
{
using ampEq::Parameters;
// Frozen pre-Depth oracle. Stock JUCE second-order IIR filters. Coefficient/storage allocation occurs once.
class Advanced final
{
public:
    void prepare(double sampleRate)
    {
        rate=sampleRate;
        for(auto& channel:filters)for(auto& filter:channel)
        {
            *filter.coefficients=juce::dsp::IIR::ArrayCoefficients<double>::makeLowPass(rate,rate*.4);
            filter.prepare({rate,128,1});filter.reset();
        }
        const Parameters p;const auto values=parameters(p);
        for(size_t i=0;i<values.size();++i){smooth[i].reset(rate,.025);smooth[i].setCurrentAndTargetValue(values[i]);}
        for(auto& mix:cutMix){mix.reset(rate,.025);mix.setCurrentAndTargetValue(0);}
    }
    void target(const Parameters& p)
    {const auto values=parameters(p);for(size_t i=0;i<values.size();++i)smooth[i].setTargetValue(values[i]);cutMix[0].setTargetValue(p.lowCut?1.f:0.f);cutMix[1].setTargetValue(p.highCut?1.f:0.f);}
    void reset() noexcept{for(auto& channel:filters)for(auto& filter:channel)filter.reset();}
    void forceIdle() noexcept{reset();for(size_t i:{size_t(3),size_t(6)})smooth[i].setCurrentAndTargetValue(0);for(auto& value:cutMix)value.setCurrentAndTargetValue(0);}
    bool idle() const noexcept
    {for(size_t i:{size_t(3),size_t(6)})if(smooth[i].isSmoothing()||smooth[i].getCurrentValue()!=0)return false;for(const auto& value:cutMix)if(value.isSmoothing()||value.getCurrentValue()!=0)return false;return true;}
    void process(juce::AudioBuffer<float>& buffer) noexcept
    {
        for(int i=0;i<buffer.getNumSamples();++i)
        {
            std::array<float,8> values;for(size_t n=0;n<values.size();++n)values[n]=smooth[n].getNextValue();
            if(i%16==0)
            {
                using C=juce::dsp::IIR::ArrayCoefficients<double>;
                const auto cap=[&](float x){return juce::jmin(double(x),rate*.45);};
                const std::array<std::array<double,6>,4> c{C::makeHighPass(rate,cap(values[0])),C::makeLowPass(rate,cap(values[1])),
                    C::makePeakFilter(rate,cap(values[2]),values[4],juce::Decibels::decibelsToGain(double(values[3]))),C::makePeakFilter(rate,cap(values[5]),values[7],juce::Decibels::decibelsToGain(double(values[6])))};
                for(auto& channel:filters)for(size_t n=0;n<c.size();++n)*channel[n].coefficients=c[n];
            }
            const std::array<float,2> wet{cutMix[0].getNextValue(),cutMix[1].getNextValue()};
            for(int channel=0;channel<juce::jmin(2,buffer.getNumChannels());++channel)
            {
                double x=buffer.getSample(channel,i);
                for(size_t band=0;band<4;++band){const double y=filters[size_t(channel)][band].processSample(x);x=band<2?x+wet[band]*(y-x):y;}
                buffer.setSample(channel,i,float(x));
            }
        }
    }
private:
    static std::array<float,8> parameters(const Parameters& p){return {p.lowCutHz,p.highCutHz,p.lowMidHz,p.lowMidDb,p.lowMidQ,p.presenceHz,p.presenceDb,p.presenceQ};}
    double rate=48000;
    std::array<std::array<juce::dsp::IIR::Filter<double>,4>,2> filters;
    std::array<juce::SmoothedValue<float>,8> smooth;std::array<juce::SmoothedValue<float>,2> cutMix;
};
}
