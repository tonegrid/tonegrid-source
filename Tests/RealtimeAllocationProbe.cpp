#include "RealtimeAllocationProbe.h"
#include <cstdlib>
#include <new>
#ifdef _WIN32
#include <malloc.h>
#endif
namespace {thread_local bool tracking=false;thread_local std::size_t allocations=0;void count() noexcept {if(tracking)++allocations;}}
namespace realtimeProbe {void begin() noexcept {allocations=0;tracking=true;}std::size_t end() noexcept {tracking=false;return allocations;}}
void* operator new(std::size_t bytes){count();if(auto* p=std::malloc(bytes?bytes:1))return p;throw std::bad_alloc();}
void* operator new[](std::size_t bytes){return ::operator new(bytes);}
void operator delete(void* p) noexcept {std::free(p);}
void operator delete[](void* p) noexcept {std::free(p);}
void operator delete(void* p,std::size_t) noexcept {std::free(p);}
void operator delete[](void* p,std::size_t) noexcept {std::free(p);}
void* operator new(std::size_t bytes,std::align_val_t alignment)
{
    count();const auto align=static_cast<std::size_t>(alignment);
#ifdef _WIN32
    void* p=_aligned_malloc(bytes?bytes:1,align);
#elif defined(__APPLE__)
    // Apple's libc uses POSIX aligned allocation. Match malloc/free ownership
    // without relying on C11 aligned_alloc being available in the macOS SDK.
    void* p=nullptr;
    if(posix_memalign(&p,align,bytes?bytes:1)!=0)p=nullptr;
#else
    void* p=std::aligned_alloc(align,((bytes?bytes:1)+align-1)/align*align);
#endif
    if(p)return p;throw std::bad_alloc();
}
void* operator new[](std::size_t n,std::align_val_t a){return ::operator new(n,a);}
void operator delete(void* p,std::align_val_t) noexcept {
#ifdef _WIN32
    _aligned_free(p);
#else
    std::free(p);
#endif
}
void operator delete[](void* p,std::align_val_t a) noexcept {::operator delete(p,a);}
void operator delete(void* p,std::size_t,std::align_val_t a) noexcept {::operator delete(p,a);}
void operator delete[](void* p,std::size_t,std::align_val_t a) noexcept {::operator delete(p,a);}
