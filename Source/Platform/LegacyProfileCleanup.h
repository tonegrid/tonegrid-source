#pragma once
#include "AppPaths.h"
#include <atomic>
#include <thread>

namespace tonegrid::paths
{
// Owner-approved best-effort removal of the retired product profile. Never
// transfer data, touch the canonical profile, follow links or report a UI error.
inline void cleanupLegacyProfile(const juce::File& dataRoot,const std::atomic_bool* cancellation=nullptr) noexcept
{
    try
    {
        const auto cancelled=[&]{return cancellation&&cancellation->load(std::memory_order_relaxed);};
        const auto legacy=dataRoot.getChildFile("ToneGridIR"),current=userFolder(dataRoot);
        // Resolve only the exact child of the explicitly supplied application-data
        // root. A link or short-name alias must never lead into the current profile.
        if(legacy.getParentDirectory()!=dataRoot||legacy==current||legacy.isSymbolicLink()||!legacy.isDirectory())return;
        const auto identity=legacy.getFileIdentifier();if(identity==0)return;
        if(current.exists())
        {
            const auto activeIdentity=current.getFileIdentifier();
            if(activeIdentity==0||activeIdentity==identity)return;
        }
        const auto remove=[&](auto&& self,const juce::File& file,int depth)->void
        {
            if(cancelled()||depth>128||file.isSymbolicLink()||(file!=legacy&&!file.isAChildOf(legacy)))return;
            if(file.isDirectory())
            {
                if(legacy.isSymbolicLink()||legacy.getFileIdentifier()!=identity)return;
                for(const auto& entry:juce::RangedDirectoryIterator(file,false,"*",juce::File::findFilesAndDirectories,juce::File::FollowSymlinks::no))
                {if(cancelled())return;self(self,entry.getFile(),depth+1);}
            }
            if(!cancelled())file.deleteFile(); // A locked file simply remains.
        };
        remove(remove,legacy,0);
    }
    catch(...){} // Cleanup is optional and can never prevent startup.
}

class LegacyProfileCleanup final
{
public:
    ~LegacyProfileCleanup(){cancel();if(worker.joinable())worker.join();}
    void start(juce::File dataRoot) noexcept
    {
        try
        {
            if(worker.joinable())return;
            worker=std::thread([this,root=std::move(dataRoot)]{cleanupLegacyProfile(root,&stopping);});
        }
        catch(...){} // Thread creation failure is optional cleanup failure too.
    }
    void cancel() noexcept{stopping.store(true,std::memory_order_relaxed);}
private:
    // Cooperative cancellation checks each entry; shutdown never waits for the
    // rest of a large directory tree to be removed.
    std::atomic_bool stopping{false};std::thread worker;
};
}
