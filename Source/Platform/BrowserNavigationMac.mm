#include "Platform/BrowserPlatform.h"
#import <WebKit/WebKit.h>

namespace tonegrid::platform
{
bool loadEncodedBrowserUrl(juce::WebBrowserComponent& browser, const juce::String& address)
{
    // JUCE 8.0.6's Mac goToURL escapes '%' again. Keep the already encoded
    // query intact, including nested redirect URIs and opaque OAuth values.
    // Use this browser's existing WKWebView, delegates and website data store.
    for (auto* child : browser.getChildren())
    {
        auto* native = dynamic_cast<juce::NSViewComponent*>(child);
        if (native == nullptr) continue;
        auto* view = static_cast<NSView*>(native->getView());
        if (![view isKindOfClass:[WKWebView class]]) continue;
        auto* text = [NSString stringWithUTF8String:address.toRawUTF8()];
        if (text == nil) return false;
        auto* url = [NSURL URLWithString:text];
        if (url == nil) return false;
        browser.stop();
        return [(WKWebView*)view loadRequest:[NSURLRequest requestWithURL:url]] != nil;
    }
    return false;
}
}
