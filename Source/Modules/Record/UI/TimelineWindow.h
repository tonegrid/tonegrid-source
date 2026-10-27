#pragma once
#include <juce_core/juce_core.h>
namespace tonegrid::record {
// View-only state. Panning and zooming never move audio or edit the selection.
struct TimelineWindow {
 double first=0,seconds=60;bool manual=false;
 double limit(double extent)const{return juce::jmax(seconds,extent+juce::jmax(2.,seconds*.12));}
 void pan(double start,double extent){first=juce::jlimit(0.,juce::jmax(0.,limit(extent)-seconds),start);manual=true;}
 void zoom(double factor,double anchor,double extent){
  const auto fraction=juce::jlimit(0.,1.,(anchor-first)/seconds);
  seconds=juce::jlimit(.5,juce::jmax(120.,extent+2.),seconds*factor);
  pan(anchor-fraction*seconds,extent);
 }
 void fit(double extent){first=0;seconds=juce::jlimit(.5,86402.,extent>0?extent+2.:60.);manual=true;}
 void reveal(double at){manual=false;if(at<=0)first=0;else if(at<first||at>=first+seconds)first=juce::jmax(0.,at-seconds*.05);}
 void follow(double at){if(!manual&&(at<first||at>first+seconds*.97))first=juce::jmax(0.,at-seconds*.05);}
};
}
