#pragma once
#include <cmath>
#include <initializer_list>
namespace tonegrid::ampEq
{
struct Parameters
{
    float bassDb=0,midDb=0,trebleDb=0;
    bool enabled=true,lowCut=false,highCut=false;
    float lowCutHz=80,highCutHz=8000;
    float lowMidHz=250,lowMidDb=0,lowMidQ=1;
    float presenceHz=3000,presenceDb=0,presenceQ=1;
    float depthDb=0,depthHz=100;
};
inline bool hasDepth(const Parameters& p) noexcept {return p.depthDb!=0||p.depthHz!=100;}
inline bool advanced(const Parameters& p) noexcept
{return hasDepth(p)||!p.enabled||p.lowCut||p.highCut||p.lowCutHz!=80||p.highCutHz!=8000||p.lowMidHz!=250||p.lowMidDb!=0||p.lowMidQ!=1||p.presenceHz!=3000||p.presenceDb!=0||p.presenceQ!=1;}
inline bool valid(const Parameters& p) noexcept
{
    for(float value:{p.bassDb,p.midDb,p.trebleDb,p.lowMidDb,p.presenceDb})if(!std::isfinite(value)||value < -12.f||value > 12.f)return false;
    const auto in=[](float x,float lo,float hi){return std::isfinite(x)&&x>=lo&&x<=hi;};
    return in(p.depthDb,-6,6)&&in(p.depthHz,40,250)&&in(p.lowCutHz,20,500)&&in(p.highCutHz,2000,20000)&&in(p.lowMidHz,80,2000)&&in(p.presenceHz,400,12000)&&in(p.lowMidQ,.3f,4)&&in(p.presenceQ,.3f,4);
}
inline bool flat(const Parameters& p) noexcept{return !p.enabled||(p.bassDb==0&&p.midDb==0&&p.trebleDb==0&&!p.lowCut&&!p.highCut&&p.lowMidDb==0&&p.presenceDb==0&&p.depthDb==0);}
}
