#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <atomic>
#include <cmath>
#include "ParameterMailbox.h"
namespace board
{
struct InputSettings { float gainDb=0, thresholdDb=-62; bool gate=false; };
inline bool validInput(const InputSettings& s)
{ return std::isfinite(s.gainDb)&&s.gainDb>=-12&&s.gainDb<=12&&std::isfinite(s.thresholdDb)&&s.thresholdDb>=-80&&s.thresholdDb<=-20; }
class InputStage
{
public:
    void set(InputSettings s){gain.store(s.gainDb);threshold.store(s.thresholdDb);enabled.store(s.gate);settings.publish(s);}
    InputSettings get()const{return {gain.load(),threshold.load(),enabled.load()};}
    void prepare(double rate)
    {
        sr=rate;gainSmooth.reset(rate,.02);gainSmooth.setCurrentAndTargetValue(juce::Decibels::decibelsToGain(gain.load()));
        envelope=0;hold=0;open=false;gateGain=enabled.load()?0.f:1.f;
        detectorRelease=(float)std::exp(-1.0/(rate*.02));attack=(float)std::exp(-1.0/(rate*.002));release=(float)std::exp(-1.0/(rate*.08));peak.store(0);
    }
    void process(float* data,int length)
    {
        const auto s=settings.read();gainSmooth.setTargetValue(juce::Decibels::decibelsToGain(s.gainDb));
        const float opening=juce::Decibels::decibelsToGain(s.thresholdDb),closing=opening*.501187f;float maximum=0;
        for(int i=0;i<length;++i)
        {
            float x=data[i];if(!std::isfinite(x))x=0;
            x*=gainSmooth.getNextValue();const float a=std::abs(x);maximum=juce::jmax(maximum,a);
            envelope=juce::jmax(a,envelope*detectorRelease);
            if(envelope>=opening){open=true;hold=(int)(sr*.03);}
            else if(open&&envelope<closing){if(hold>0)--hold;else open=false;}
            const float target=(!s.gate||open)?1.f:0.f;
            const float coefficient=target>gateGain?attack:release;gateGain=target+coefficient*(gateGain-target);
            if(std::abs(gateGain-target)<.000001f)gateGain=target;
            data[i]=x*gateGain;
        }
        float previous=peak.load();while(previous<maximum&&!peak.compare_exchange_weak(previous,maximum)){}
    }
    float takePeak(){return peak.exchange(0);}
private:
    tonegrid::ParameterMailbox<InputSettings> settings;
    std::atomic<float> gain{0},threshold{-62},peak{0};std::atomic<bool> enabled{false};
    juce::SmoothedValue<float> gainSmooth;double sr=48000;float envelope=0,gateGain=1,detectorRelease=0,attack=0,release=0;int hold=0;bool open=false;
};
}
