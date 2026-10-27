#pragma once
#include "Modules/Play/UI/PedalAssetSkin.h"
namespace tonegrid::assets
{
inline constexpr int pedalShadowClearance=12;
inline std::array<juce::Rectangle<int>,13> boardBoundsFor(juce::Rectangle<int> area,bool modern)
{
    constexpr int gap=6,rowGap=12;
    if(modern)
    {
        const int cellHeight=juce::jmax(1,(area.getHeight()-rowGap)/2);std::array<float,13> ratios;
        float maximumHeight=float(cellHeight);
        for(int v=0;v<13;++v){const auto viewport=pedal137(v).viewport;ratios[size_t(v)]=viewport.getWidth()/viewport.getHeight();}
        for(int row=0;row<2;++row){const int first=row?7:0,count=row?6:7;float sum=0;for(int v=first;v<first+count;++v)sum+=ratios[size_t(v)];maximumHeight=juce::jmin(maximumHeight,area.getWidth()/sum);
            // The lower row leaves room for icons and their readable hover labels.
            if(row==1)maximumHeight=juce::jmin(maximumHeight,(area.getWidth()-2*juce::jmin(138,area.getWidth()/5))/sum);}
        const int height=juce::jmax(1,int(std::floor(maximumHeight)));std::array<juce::Rectangle<int>,13> result;
        for(int row=0;row<2;++row)
        {
            const int first=row?7:0,count=row?6:7;int total=0;
            for(int v=first;v<first+count;++v)total+=int(std::floor(height*ratios[size_t(v)]));
            int x=area.getX()+(area.getWidth()-total)/2;const int y=area.getY()+row*(cellHeight+rowGap)+(cellHeight-height)/2;
            for(int v=first;v<first+count;++v){const int width=int(std::floor(height*ratios[size_t(v)]));result[size_t(v)]={x,y,width,height};x+=width;}
        }
        return result;
    }
    const int clearance=modern?0:pedalShadowClearance;
    const auto standard=modern?pedal137(4).viewport:transparentOd().viewport;const auto eq=modern?pedal137(11).viewport:pedal(11).viewport;
    const auto ratio=standard.getWidth()/standard.getHeight(),eqRatio=eq.getWidth()/eq.getHeight();
    const int cellHeight=juce::jmax(1,(area.getHeight()-rowGap)/2);
    const int height=juce::jmax(1,juce::jmin(cellHeight-clearance,(int)std::floor((area.getWidth()-6*gap)/7.f/ratio)));
    const int width=(int)std::floor(height*ratio),eqWidth=(int)std::floor(height*eqRatio);
    std::array<juce::Rectangle<int>,13> result;
    for(int row=0;row<2;++row)
    {
        const int count=row==0?7:6,total=row==0?7*width+6*gap:5*width+eqWidth+5*gap;
        int x=area.getX()+(area.getWidth()-total)/2;
        const int y=area.getY()+row*(cellHeight+rowGap)+(cellHeight-clearance-height)/2;
        for(int v=row==0?0:7,end=v+count;v<end;++v){const int w=v==11?eqWidth:width;result[(size_t)v]={x,y,w,height};x+=w+gap;}
    }return result;
}
inline std::array<juce::Rectangle<int>,13> boardBounds(juce::Rectangle<int> area){return boardBoundsFor(area,true);}
inline std::array<juce::Rectangle<int>,13> legacyBoardBounds(juce::Rectangle<int> area){return boardBoundsFor(area,false);}
}
