#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>

namespace tonegrid::looping
{
enum class Mode { empty, recording, playing, stopped };
// One audio-thread-owned loop, independent of the disk recorder. No overdubbing.
// prepare() runs while the device callback is stopped; process() never allocates.
class Engine final
{
public:
    static constexpr int maximumSeconds=60;
    Engine(){prepare(48000);}
    void prepare(double sampleRate)
    {
        rate=std::isfinite(sampleRate)&&sampleRate>=8000&&sampleRate<=192000?sampleRate:48000;
        buffer.setSize(2,int(rate*maximumSeconds));
        length=position=0;clearing=false;mode.store(Mode::empty);command.store(Command::none);
        frames.store(0);publishedRate.store(rate);gain.reset(rate,.005);gain.setCurrentAndTargetValue(0);
    }
    void deviceStopped() noexcept{command.store(Command::clear);}
    bool advance() noexcept{return request(Command::advance);}
    bool clear() noexcept{return request(Command::clear);}
    bool pending() const noexcept{return command.load()!=Command::none;}
    Mode state() const noexcept{return mode.load();}
    double seconds() const noexcept{return frames.load()/publishedRate.load();}
    void process(float* const* output,int channels,int samples) noexcept
    {
        auto current=mode.load();
        const auto next=command.exchange(Command::none);
        if(next==Command::clear){current=Mode::stopped;clearing=true;gain.setTargetValue(0);}
        else if(next==Command::advance&&!clearing)
        {
            if(current==Mode::empty){length=position=0;current=Mode::recording;gain.setCurrentAndTargetValue(0);}
            else if(current==Mode::recording){position=0;current=length>=64?Mode::playing:Mode::empty;if(current==Mode::empty)length=0;gain.setTargetValue(current==Mode::playing?1.f:0.f);}
            else if(current==Mode::playing){current=Mode::stopped;gain.setTargetValue(0);}
            else if(length>0){position=0;current=Mode::playing;gain.setTargetValue(1);}
        }
        const int fade=juce::jmax(1,int(rate*.002));
        for(int i=0;i<samples;++i)
        {
            if(current==Mode::recording)
            {
                for(int c=0;c<2;++c){const auto* source=channels>0?output[juce::jmin(c,channels-1)]:nullptr;const float x=source?source[i]:0;buffer.setSample(c,length,std::isfinite(x)?x:0);}
                if(++length==buffer.getNumSamples()){position=0;current=Mode::playing;gain.setTargetValue(1);}
            }
            else if(length>0&&(current==Mode::playing||gain.isSmoothing()||gain.getCurrentValue()>0))
            {
                const int edge=juce::jmin(fade,juce::jmax(1,length/4));
                const float seam=juce::jmin(1.f,juce::jmin(float(position)/edge,float(length-1-position)/edge));
                const float level=gain.getNextValue()*seam;
                for(int c=0;c<juce::jmin(2,channels);++c)if(output[c])output[c][i]+=buffer.getSample(c,position)*level;
                if(++position==length)position=0;
            }
            if(clearing&&!gain.isSmoothing()&&gain.getCurrentValue()==0){length=position=0;clearing=false;current=Mode::empty;}
        }
        frames.store(length);mode.store(current);
    }
private:
    enum class Command { none, advance, clear };
    bool request(Command value) noexcept{auto expected=Command::none;return command.compare_exchange_strong(expected,value);}
    juce::AudioBuffer<float> buffer;juce::SmoothedValue<float> gain;
    std::atomic<Command> command{Command::none};std::atomic<Mode> mode{Mode::empty};
    std::atomic<int> frames{0};std::atomic<double> publishedRate{48000};
    double rate=48000;int length=0,position=0;bool clearing=false;
};
}
