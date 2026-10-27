#include "Effects/Pitch/PitchEngine.h"
#include <juce_dsp/juce_dsp.h>
#include <StftPitchShift/Vocoder.h>
#include <StftPitchShift/Pitcher.h>
#include <array>
#include <vector>
#include <algorithm>
namespace tonegrid
{
struct PitchEngine::Impl
{
    // Owned radix-2 FFT: immutable permutation/twiddles prepared on the message
    // thread. Unlike JUCE's Windows fallback, execution has no process SpinLock.
    struct StreamingFft
    {
        int size;std::vector<int> reverse;std::vector<std::complex<float>> twiddle;
        explicit StreamingFft(int n):size(n),reverse(n),twiddle(n/2)
        {
            const auto bits=int(std::log2(n));
            for(int i=0;i<n;++i){int value=i,r=0;for(int b=0;b<bits;++b){r=(r<<1)|(value&1);value>>=1;}reverse[i]=r;}
            for(int i=0;i<n/2;++i){const double phase=-juce::MathConstants<double>::twoPi*i/n;twiddle[i]={float(std::cos(phase)),float(std::sin(phase))};}
        }
        void perform(std::vector<std::complex<float>>& data,bool inverse) const noexcept
        {
            for(int i=0;i<size;++i)if(i<reverse[i])std::swap(data[i],data[reverse[i]]);
            for(int length=2;length<=size;length*=2)
                for(int start=0;start<size;start+=length)
                    for(int j=0;j<length/2;++j){auto w=twiddle[j*(size/length)];if(inverse)w=std::conj(w);const auto a=data[start+j],b=data[start+j+length/2]*w;data[start+j]=a+b;data[start+j+length/2]=a-b;}
            if(inverse)for(auto& x:data)x/=float(size);
        }
    };
    struct Voice
    {
        stftpitchshift::Vocoder<float> vocoder;
        stftpitchshift::Pitcher<float> pitcher;
        std::vector<std::complex<float>> spectrum;
        std::vector<std::complex<float>> fftData;
        std::vector<float> ola;
        std::vector<double> factor{1.};
        Voice(int n,int h,int,double sr):vocoder(n,h,sr),pitcher(n,sr),spectrum(n/2+1),fftData(n),ola(2*n)
        {pitcher.factors(factor);}
        void reset() noexcept {vocoder.reset();std::fill(ola.begin(),ola.end(),0.f);}
        void setFactor(double f) {factor[0]=f;pitcher.factors(factor);} // All capacities fixed in constructor.
    };
    int n,h,latency,inputPos=0,outPos=0,hopCount=0,warm=0,activeSemitones=0;
    double rate; pitch::Mode active=pitch::Mode::Off;
    StreamingFft fft;
    std::vector<float> input,window,synthesisWindow,dryLine;
    int dryPos=0;
    std::array<std::unique_ptr<Voice>,3> voices;
    juce::SmoothedValue<float> engage,dryGain,downGain,upGain;
    Impl(double sr,int size,int hop,int synthesis):n(size),h(hop),latency(synthesis),rate(sr),fft(size),input(size),window(size),synthesisWindow(size),dryLine(synthesis)
    {
        const auto hann=[](int i,int length){return .5f-.5f*std::cos(juce::MathConstants<float>::twoPi*i/length);};
        // Exact asymmetric window construction from upstream STFT.h. A long
        // analysis window preserves low-note resolution; the short synthesis
        // window determines causal streaming latency.
        if(synthesis==n){for(int j=0;j<n;++j)window[j]=synthesisWindow[j]=hann(j,n);}
        else
        {
            const int m=synthesis/2;
            for(int j=0;j<n-m;++j)window[j]=hann(j,2*n-2*m);
            for(int j=0;j<m;++j)window[j+n-m]=hann(j+m,2*m);
            for(int j=0;j<m;++j){const auto v=hann(j,2*m);synthesisWindow[j+n-2*m]=v*v/window[j+n-2*m];synthesisWindow[j+n-m]=hann(j+m,2*m);}
        }
        float sum=0;for(int j=0;j<n;++j)sum+=window[j]*synthesisWindow[j];for(auto& v:synthesisWindow)v*=h/sum;
        for(auto& v:voices)v=std::make_unique<Voice>(n,h,latency,rate);
        voices[1]->setFactor(.5);voices[2]->setFactor(2.);
        engage.reset(rate,.005);engage.setCurrentAndTargetValue(1);
        for(auto* s:{&dryGain,&downGain,&upGain}){s->reset(rate,.025);s->setCurrentAndTargetValue(0);}
    }
    void clear() noexcept
    {for(auto& v:voices)v->reset();std::fill(input.begin(),input.end(),0.f);std::fill(dryLine.begin(),dryLine.end(),0.f);inputPos=outPos=hopCount=dryPos=0;}
    void frame(Voice& v)
    {
        for(int j=0;j<n;++j)v.fftData[j]={input[(inputPos+j)%n]*window[j],0};
        fft.perform(v.fftData,false);
        std::copy_n(v.fftData.begin(),n/2+1,v.spectrum.begin());
        v.vocoder.encode(v.spectrum);v.pitcher.shiftpitch(v.spectrum);v.vocoder.decode(v.spectrum);
        v.spectrum[0]=v.spectrum[n/2]=0;
        std::copy(v.spectrum.begin(),v.spectrum.end(),v.fftData.begin());
        for(int j=1;j<n/2;++j)v.fftData[n-j]=std::conj(v.spectrum[j]);
        fft.perform(v.fftData,true);
        for(int j=n-latency;j<n;++j)v.ola[(outPos+1+j-(n-latency))%(2*n)]+=v.fftData[j].real()*synthesisWindow[j];
    }
    float tick(float x,const pitch::Parameters& p)
    {
        const bool changed=p.mode!=active||(p.mode==pitch::Mode::Transpose&&p.semitones!=activeSemitones);
        if(changed)engage.setTargetValue(0);
        else if(warm==0)engage.setTargetValue(1);
        if(changed&&!engage.isSmoothing()&&engage.getCurrentValue()==0)
        {
            active=p.mode;activeSemitones=p.semitones;clear();
            voices[0]->setFactor(std::pow(2.,activeSemitones/12.));
            warm=active==pitch::Mode::Off?0:n;
        }
        float y=x;
        if(active!=pitch::Mode::Off)
        {
            const float aligned=dryLine[dryPos];dryLine[dryPos]=x;dryPos=(dryPos+1)%latency;
            input[inputPos]=x;inputPos=(inputPos+1)%n;
            if(++hopCount==h){hopCount=0;if(active==pitch::Mode::Transpose)frame(*voices[0]);else if(active==pitch::Mode::Octaver){frame(*voices[1]);frame(*voices[2]);}}
            if(active==pitch::Mode::Transpose)y=voices[0]->ola[outPos];
            else y=aligned*dryGain.getNextValue()+voices[1]->ola[outPos]*downGain.getNextValue()+voices[2]->ola[outPos]*upGain.getNextValue();
            for(auto& v:voices)v->ola[outPos]=0;
            outPos=(outPos+1)%(2*n);if(warm>0)--warm;
        }
        return y*engage.getNextValue();
    }
};
PitchEngine::PitchEngine()=default;PitchEngine::~PitchEngine()=default;
void PitchEngine::prepare(double sr,int n,int h,int synthesis)
{impl=std::make_unique<Impl>(sr,n,h,synthesis==0?n:synthesis);reset();}
void PitchEngine::reset() noexcept
{if(!impl)return;impl->clear();impl->active=pitch::Mode::Off;impl->activeSemitones=0;impl->warm=0;impl->engage.setCurrentAndTargetValue(1);}
bool PitchEngine::configure(const pitch::Parameters& p) noexcept
{if(!pitch::valid(p))return false;transpose.store(p.semitones);dry.store(p.dry);down.store(p.down);up.store(p.up);mode.store(int(p.mode));published.publish(p);return true;}
pitch::Parameters PitchEngine::settings() const noexcept
{return {pitch::Mode(mode.load()),transpose.load(),dry.load(),down.load(),up.load()};}
void PitchEngine::process(float* data,int count) noexcept
{
    if(!impl)return;const auto p=published.read();
    if(p.mode==pitch::Mode::Off&&impl->active==pitch::Mode::Off&&!impl->engage.isSmoothing())return;
    impl->dryGain.setTargetValue(p.dry*.01f);impl->downGain.setTargetValue(p.down*.01f);impl->upGain.setTargetValue(p.up*.01f);
    for(int i=0;i<count;++i)data[i]=impl->tick(data[i],p);
}
int PitchEngine::latencySamples() const noexcept {return impl?impl->latency:0;}
}
