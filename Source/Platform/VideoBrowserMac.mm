#include "Platform/VideoBrowserMac.h"
#import <WebKit/WebKit.h>

namespace tonegrid::platform
{
bool browserHasNativeFullscreen(juce::WebBrowserComponent& browser)
{
    for(auto* child:browser.getChildren())
    {
        auto* native=dynamic_cast<juce::NSViewComponent*>(child);
        if(native==nullptr)continue;
        auto* view=static_cast<NSView*>(native->getView());
        if(![view isKindOfClass:[WKWebView class]])continue;
        if(@available(macOS 13.0,*))
            return [(WKWebView*)view fullscreenState]!=WKFullscreenStateNotInFullscreen;
    }
    return false;
}

bool loadVideoDocument(juce::WebBrowserComponent& browser, const juce::String& html,
                       const juce::String& baseUrl)
{
    // JUCE 8.0.6 embeds its WKWebView in an NSViewComponent child. Use that
    // child's public view accessor; do not search another component's window.
    for (auto* child : browser.getChildren())
    {
        auto* native = dynamic_cast<juce::NSViewComponent*>(child);
        if (native == nullptr) continue;
        auto* view = static_cast<NSView*>(native->getView());
        if (![view isKindOfClass:[WKWebView class]]) continue;
        auto* document = [NSString stringWithUTF8String:html.toRawUTF8()];
        auto* addressText = [NSString stringWithUTF8String:baseUrl.toRawUTF8()];
        if (document == nil || addressText == nil) return false;
        auto* address = [NSURL URLWithString:addressText];
        if (address == nil) return false;
        return [(WKWebView*)view loadHTMLString:document baseURL:address] != nil;
    }
    return false;
}
}
