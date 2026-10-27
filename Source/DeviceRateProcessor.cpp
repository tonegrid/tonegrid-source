#include "DeviceRateProcessor.h"
#ifndef DEFAULT_BLOCK_SIZE
#define DEFAULT_BLOCK_SIZE 1024
#endif
namespace iplug { inline constexpr double PI=3.14159265358979323846; }
#include "../vendor/AudioDSPTools-main/dsp/ResamplingContainer/ResamplingContainer.h"


namespace tonegrid::audio
{
using Boundary=dsp::ResamplingContainer<float,2,12>;
struct DeviceRateProcessor::Conversion {std::unique_ptr<Boundary> boundary;Boundary::BlockProcessFunc render;};
DeviceRateProcessor::DeviceRateProcessor(juce::AudioProcessor& value)
        :AudioProcessor(BusesProperties().withInput("Input",juce::AudioChannelSet::stereo(),true).withOutput("Output",juce::AudioChannelSet::stereo(),true)),conversion(std::make_unique<Conversion>()),engine(value)
    {
        conversion->render=[this](float** input,float** output,int frames)
        {
            for(int c=0;c<2;++c)if(input[c]!=output[c])juce::FloatVectorOperations::copy(output[c],input[c],frames);
            juce::AudioBuffer<float> block(output,2,frames);
            engine.processBlock(block,emptyMidi);
        };
    }

DeviceRateProcessor::~DeviceRateProcessor()=default;
bool DeviceRateProcessor::converting()const noexcept{return conversion->boundary!=nullptr;}
void DeviceRateProcessor::prepareToPlay(double rate,int block)
    {
        valid=false;fault.store(false);conversion->boundary.reset();delay=0;
        if(!supported(rate))return;
        slice=juce::jlimit(128,512,block);
        const bool different=std::abs(rate-engineRate)>.1;
        // Padding covers the resampler's filter history and short host blocks.
        const int internal=different?int(std::ceil(slice*engineRate/rate))+64:juce::jmax(1,block);
        engine.setPlayConfigDetails(2,2,engineRate,internal);
        engine.prepareToPlay(engineRate,internal);
        if(different)
        {
            auto probe=std::make_unique<Boundary>(engineRate);probe->Reset(rate,slice);
            const int total=((probe->GetLatency()+2*slice)/slice+1)*slice;
            juce::AudioBuffer<float> impulse(2,total);impulse.clear();
            impulse.setSample(0,0,1);impulse.setSample(1,0,1);
            Boundary::BlockProcessFunc identity=[](float** in,float** out,int n)
            {for(int c=0;c<2;++c)juce::FloatVectorOperations::copy(out[c],in[c],n);};
            for(int p=0;p<total;p+=slice){float* channels[]{impulse.getWritePointer(0,p),impulse.getWritePointer(1,p)};probe->ProcessBlock(channels,channels,slice,identity);}
            float peak=0;for(int i=0;i<total;++i)if(std::abs(impulse.getSample(0,i))>peak){peak=std::abs(impulse.getSample(0,i));delay=i;}
            conversion->boundary=std::make_unique<Boundary>(engineRate);conversion->boundary->Reset(rate,slice);
        }
        setLatencySamples(delay+juce::roundToInt(engine.getLatencySamples()*rate/engineRate));
        valid=true;
    }
void DeviceRateProcessor::releaseResources(){valid=false;engine.releaseResources();engine.setPlayHead(nullptr);}
void DeviceRateProcessor::processBlock(juce::AudioBuffer<float>& audio,juce::MidiBuffer& midi)
    {
        juce::ScopedNoDenormals denormals;
        if(!valid||audio.getNumChannels()!=2){audio.clear();midi.clear();return;}
        engine.setPlayHead(getPlayHead());
        struct ClearPlayHead {juce::AudioProcessor& processor;~ClearPlayHead(){processor.setPlayHead(nullptr);}} clearPlayHead{engine};
        if(!conversion->boundary){try{engine.processBlock(audio,midi);}catch(...){audio.clear();fault.store(true);}return;} // Exact 48k path, with the same exception guard as resampling.
        emptyMidi.clear();midi.clear();
        try
        {
            for(int p=0;p<audio.getNumSamples();p+=slice)
            {
                float* channels[]{audio.getWritePointer(0,p),audio.getWritePointer(1,p)};
                conversion->boundary->ProcessBlock(channels,channels,juce::jmin(slice,audio.getNumSamples()-p),conversion->render);
            }
        }
        catch(...){audio.clear();fault.store(true);}
    }

}
