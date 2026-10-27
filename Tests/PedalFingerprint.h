#pragma once
#include <juce_cryptography/juce_cryptography.h>
// Shared deterministic renderer for the saved pre-change processor and the current
// processor. Retired Blues captures are intentionally excluded, not relabelled.
template<class Processor,class Defaults>
juce::String unchangedPedalFingerprint(Processor& processor,Defaults defaults)
{
    using namespace juce;MemoryOutputStream stream;AudioBuffer<float> buffer(2,256);MidiBuffer midi;int frame=0;
    for(int p=0;p<13;++p)processor.set(p,defaults(p,0));processor.prepareToPlay(48000,256);
    auto render=[&](int blocks){constexpr std::array<int,5> sizes{96,128,1,256,37};for(int b=0;b<blocks;++b){const auto n=sizes[(size_t)b%sizes.size()];buffer.setSize(2,n,false,false,true);for(int i=0;i<n;++i,++frame){const float x=.035f*(float)std::sin(frame*.031)+.013f*(float)std::sin(frame*.073);buffer.setSample(0,i,x);buffer.setSample(1,i,-x);}processor.processBlock(buffer,midi);for(int c=0;c<2;++c)stream.write(buffer.getReadPointer(c),(size_t)n*sizeof(float));}};
    render(20);for(int p=0;p<13;++p){if(p==10)continue;for(int preset=0;preset<8;++preset){auto s=defaults(p,preset);s.enabled=true;if(board::isNam(p)){s.input=2;s.tone=preset%2?3.f:-2.f;s.output=-1;}processor.set(p,s);render(30);}auto off=processor.get(p);off.enabled=false;processor.set(p,off);render(20);}
    for(int p=0;p<13;++p){auto s=defaults(p,3);s.enabled=p!=10;processor.set(p,s);}render(50);return SHA256(stream.getData(),stream.getDataSize()).toHexString();
}
