#pragma once
#include <memory>
namespace tonegrid::platform
{
// Preserve the native window and its live WebView2 controller across fullscreen.
class VideoFullscreen final
{
public:
    VideoFullscreen();~VideoFullscreen();
    bool enter(void* nativeWindow);bool leave();
private:
    struct State;std::unique_ptr<State> state;
};
}
