#pragma once
#include "Effects/AmpEq/AmpEqParameters.h"
#include "NamEq123LegacyAdvanced.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
namespace tonegrid::legacyEq123
{
using ampEq::Parameters; using ampEq::valid;
// Frozen pre-Depth oracle. Post-model tone correction. Fixed storage, no latency or level compensation.
// RBJ biquads; centres follow the TONE3000 global tone controls (150/425/1800 Hz).
class Processor final
{
public:
    void prepare(double rate) noexcept
    {
        rate=std::isfinite(rate)&&rate>0?rate:48000.;
        for(int band=0;band<3;++band)
        {const double omega=juce::MathConstants<double>::twoPi*std::min(frequencies[band],rate*.45)/rate;sines[band]=std::sin(omega);cosines[band]=std::cos(omega);}
        for(auto& gain:gains){gain.reset(rate,.025);gain.setCurrentAndTargetValue(0);}
        active=false;clear();advancedEq.prepare(rate);
    }
    void setTarget(const Parameters& p,bool nam) noexcept
    {
        if(!valid(p))return;
        if(!nam){active=false;for(auto& gain:gains)gain.setCurrentAndTargetValue(0);clear();advancedEq.forceIdle();return;}
        const auto effective=p.enabled?p:Parameters{};
        active=true;gains[0].setTargetValue(effective.bassDb);gains[1].setTargetValue(effective.midDb);gains[2].setTargetValue(effective.trebleDb);advancedEq.target(effective);
    }
    void process(juce::AudioBuffer<float>& buffer) noexcept
    {
        if(!active)return;
        if(settledFlat()&&advancedEq.idle()){clear();advancedEq.reset();return;} // Bit-exact unity for all historical presets.
        juce::ScopedNoDenormals scope;
        const int channels=std::min(2,buffer.getNumChannels());
        for(int sample=0;sample<buffer.getNumSamples();++sample)
        {
            // Smooth the gains at sample rate, and coefficients at a bounded rate.
            // Updating at the first sample also handles arbitrarily short callbacks.
            for(int band=0;band<3;++band)
            {const auto value=gains[band].getNextValue();if(sample%16==0||!gains[band].isSmoothing())update(band,value);}
            for(int channel=0;channel<channels;++channel)
            {
                double value=buffer.getSample(channel,sample);
                for(int band=0;band<3;++band)
                {const auto& c=coefficients[band];auto& z=memory[channel][band];const double out=c.b0*value+z[0];z[0]=c.b1*value-c.a1*out+z[1];z[1]=c.b2*value-c.a2*out;value=out;}
                buffer.setSample(channel,sample,float(value));
            }
        }
        if(!advancedEq.idle())advancedEq.process(buffer);
    }
private:
    struct Coefficients{double b0=1,b1=0,b2=0,a1=0,a2=0;};
    inline static constexpr double frequencies[]{150,425,1800};
    std::array<juce::SmoothedValue<float,juce::ValueSmoothingTypes::Linear>,3> gains;
    std::array<double,3> sines{},cosines{};
    std::array<Coefficients,3> coefficients;
    std::array<float,3> coefficientGains{NAN,NAN,NAN};
    std::array<std::array<std::array<double,2>,3>,2> memory{};
    bool active=false;
    Advanced advancedEq;
    bool settledFlat() const noexcept
    {for(const auto& gain:gains)if(gain.isSmoothing()||gain.getCurrentValue()!=0)return false;return true;}
    void clear() noexcept{memory={};coefficients={};coefficientGains={NAN,NAN,NAN};}
    void update(int band,float db) noexcept
    {
        if(coefficientGains[band]==db)return;coefficientGains[band]=db;
        if(db==0){coefficients[band]={};return;}
        const double a=std::pow(10.,db/40.),c=cosines[band],s=sines[band];
        double b0,b1,b2,a0,a1,a2;
        if(band==1)
        {const double alpha=s/(2*(db<0?1.5:.7));b0=1+alpha*a;b1=-2*c;b2=1-alpha*a;a0=1+alpha/a;a1=-2*c;a2=1-alpha/a;}
        else
        {
            const double beta=2*std::sqrt(a)*s/(2*std::sqrt(.5));
            if(band==0){b0=a*((a+1)-(a-1)*c+beta);b1=2*a*((a-1)-(a+1)*c);b2=a*((a+1)-(a-1)*c-beta);a0=(a+1)+(a-1)*c+beta;a1=-2*((a-1)+(a+1)*c);a2=(a+1)+(a-1)*c-beta;}
            else{b0=a*((a+1)+(a-1)*c+beta);b1=-2*a*((a-1)+(a+1)*c);b2=a*((a+1)+(a-1)*c-beta);a0=(a+1)-(a-1)*c+beta;a1=2*((a-1)-(a+1)*c);a2=(a+1)-(a-1)*c-beta;}
        }
        coefficients[band]={b0/a0,b1/a0,b2/a0,a1/a0,a2/a0};
    }
};
}
