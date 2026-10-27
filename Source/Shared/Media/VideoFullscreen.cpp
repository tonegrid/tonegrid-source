#include "Shared/Media/VideoFullscreen.h"
#include <juce_core/juce_core.h>
#if JUCE_WINDOWS
#include <windows.h>
#elif JUCE_MAC
#import <Cocoa/Cocoa.h>
#endif
namespace tonegrid::platform
{
struct VideoFullscreen::State
{
#if JUCE_WINDOWS
    HWND window=nullptr;LONG_PTR style=0,extendedStyle=0;WINDOWPLACEMENT placement{sizeof(WINDOWPLACEMENT)};
#elif JUCE_MAC
    NSView* view=nil;
    ~State(){[view release];}
#endif
};
VideoFullscreen::VideoFullscreen()=default;
VideoFullscreen::~VideoFullscreen(){leave();}
bool VideoFullscreen::enter(void* nativeWindow)
{
#if JUCE_WINDOWS
    if(state)return true;auto next=std::make_unique<State>();next->window=static_cast<HWND>(nativeWindow);
    MONITORINFO monitor{sizeof(MONITORINFO)};
    if(!IsWindow(next->window)||!GetWindowPlacement(next->window,&next->placement)||!GetMonitorInfoW(MonitorFromWindow(next->window,MONITOR_DEFAULTTONEAREST),&monitor))return false;
    next->style=GetWindowLongPtrW(next->window,GWL_STYLE);next->extendedStyle=GetWindowLongPtrW(next->window,GWL_EXSTYLE);
    SetWindowLongPtrW(next->window,GWL_STYLE,(next->style&~(WS_CAPTION|WS_THICKFRAME|WS_MINIMIZEBOX|WS_MAXIMIZEBOX|WS_SYSMENU))|WS_POPUP);
    SetWindowLongPtrW(next->window,GWL_EXSTYLE,next->extendedStyle&~(WS_EX_DLGMODALFRAME|WS_EX_WINDOWEDGE|WS_EX_CLIENTEDGE|WS_EX_STATICEDGE));
    ShowWindow(next->window,SW_MAXIMIZE);
    const auto r=monitor.rcMonitor;SetWindowPos(next->window,HWND_TOP,r.left,r.top,r.right-r.left,r.bottom-r.top,SWP_FRAMECHANGED|SWP_SHOWWINDOW);
    state=std::move(next);return true;
#elif JUCE_MAC
    if(state)return true;
    auto* view=static_cast<NSView*>(nativeWindow);
    if(view==nil||![view isKindOfClass:[NSView class]]||view.window==nil||view.window.screen==nil)return false;
    auto next=std::make_unique<State>();next->view=[view retain];
    // AppKit moves the existing view hierarchy into its fullscreen container,
    // retaining the same WKWebView and playback session. Presentation options
    // avoid capturing other displays and are restored automatically on exit.
    NSDictionary* options=@{NSFullScreenModeAllScreens:@NO,
        NSFullScreenModeApplicationPresentationOptions:@(NSApplicationPresentationAutoHideDock|NSApplicationPresentationAutoHideMenuBar)};
    if(![view enterFullScreenMode:view.window.screen withOptions:options])return false;
    state=std::move(next);return true;
#else
    juce::ignoreUnused(nativeWindow);return false;
#endif
}
bool VideoFullscreen::leave()
{
    if(!state)return false;
#if JUCE_WINDOWS
    if(IsWindow(state->window))
    {
        SetWindowLongPtrW(state->window,GWL_STYLE,state->style);SetWindowLongPtrW(state->window,GWL_EXSTYLE,state->extendedStyle);
        SetWindowPlacement(state->window,&state->placement);
        SetWindowPos(state->window,nullptr,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE|SWP_FRAMECHANGED);
    }
#elif JUCE_MAC
    if([state->view isInFullScreenMode])[state->view exitFullScreenModeWithOptions:nil];
#endif
    state.reset();return true;
}
}
