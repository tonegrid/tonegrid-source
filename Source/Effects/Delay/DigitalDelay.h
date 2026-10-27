#pragma once
#include "EffectsParameters.h"
#include <juce_dsp/juce_dsp.h>
namespace tonegrid
{
class DigitalDelay
{
public:
    void prepare(double sr,int block)
    {
        sampleRate=sr;line.setMaximumDelayInSamples(int(sr*2)+4);line.prepare({sr,juce::uint32(block),2});
        time.reset(sr,.08);feedback.reset(sr,.025);mix.reset(sr,.025);engage.reset(sr,.025);
        time.setCurrentAndTargetValue(float(target.timeMs*sr/1000.));feedback.setCurrentAndTargetValue(target.feedback*.01f);mix.setCurrentAndTargetValue(target.mix*.01f);engage.setCurrentAndTargetValue(0);reset();
    }
    void reset() noexcept {line.reset();engage.setCurrentAndTargetValue(0);idle=true;}
    void setTarget(const delay::Parameters& p) noexcept
    {if(!delay::valid(p))return;target=p;time.setTargetValue(float(p.timeMs*sampleRate/1000.));feedback.setTargetValue(p.feedback*.01f);mix.setTargetValue(p.mix*.01f);engage.setTargetValue(p.enabled?1.f:0.f);}
    void process(juce::AudioBuffer<float>& b) noexcept
    {
        juce::ScopedNoDenormals scoped;
        engage.setTargetValue(target.enabled?1.f:0.f);
        if(!target.enabled&&!engage.isSmoothing()&&engage.getCurrentValue()==0)
        {if(!idle){line.reset();idle=true;}return;}
        idle=false;
        for(int i=0;i<b.getNumSamples();++i)
        {
            const float delaySamples=time.getNextValue(),fb=feedback.getNextValue(),amount=mix.getNextValue()*engage.getNextValue();
            for(int c=0;c<juce::jmin(2,b.getNumChannels());++c)
            {
                const float x=b.getSample(c,i),echo=line.popSample(c,delaySamples);
                line.pushSample(c,(target.enabled?x:0.f)+fb*echo);
                if(amount!=0)b.setSample(c,i,x*(1-amount)+echo*amount);
            }
        }
    }
private:
    juce::dsp::DelayLine<float,juce::dsp::DelayLineInterpolationTypes::Linear> line;
    juce::SmoothedValue<float> time,feedback,mix,engage;
    delay::Parameters target;double sampleRate=48000;bool idle=true;
};
// Message-thread only. Four-interval mean, outlier rejection, restart after pause.
class TapTime
{
public:
    double tap(double nowMs) noexcept
    {
        if(last<0){last=nowMs;return 0;}const double interval=nowMs-last;last=nowMs;
        if(interval<100||interval>2000){count=0;return 0;}
        if(count>0&&std::abs(interval-mean())>mean()*.35){count=0;}
        intervals[count%4]=interval;++count;return mean();
    }
private:
    double mean() const noexcept {double sum=0;const int size=std::min(count,4);for(int i=0;i<size;++i)sum+=intervals[i];return size?sum/size:0;}
    double last=-1,intervals[4]{};int count=0;
};
}
