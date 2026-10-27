#pragma once
#include <cstddef>
namespace realtimeProbe
{
// Test-only, thread-local interception of C++ new/new[]/aligned new.
void begin() noexcept;
std::size_t end() noexcept;
}
