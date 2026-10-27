#pragma once
#include <juce_gui_extra/juce_gui_extra.h>

#if JUCE_MAC && JUCE_WEB_BROWSER
namespace tonegrid::platform
{
// WKWebView owns trusted DOM fullscreen. Do not reparent it a second time.
bool browserHasNativeFullscreen(juce::WebBrowserComponent&);

// Load the inline player with an HTTPS document identity, preserving the JUCE
// WKWebView's delegates and native integration scripts.
bool loadVideoDocument(juce::WebBrowserComponent&, const juce::String& html,
                       const juce::String& baseUrl);
}
#endif
