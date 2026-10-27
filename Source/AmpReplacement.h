#pragma once
#include "AmpSource.h"
#include <vector>

namespace amp
{
// Caller has detached the player and closed the editor. JUCE's void restore API
// cannot detect silent plugin rejection; caught failures attempt prior-blob rollback.
inline juce::Result restoreWithRollback(AmpSource& source,const juce::MemoryBlock& state,bool& recoveryFailed)
{
    jassert(juce::MessageManager::existsAndIsCurrentThread());recoveryFailed=false;juce::MemoryBlock previous;
    try {source.captureState(previous);}catch(...){return juce::Result::fail("Plugin state capture failed; current rig kept.");}
    try {source.restoreState(state);return juce::Result::ok();}
    catch(...)
    {
        if(previous.isEmpty())recoveryFailed=true;
        else try{source.restoreState(previous);}catch(...){recoveryFailed=true;}
        return juce::Result::fail(recoveryFailed?"Plugin state restore and rollback failed; audio must stay paused.":"Plugin state restore failed; previous blob restored.");
    }
}
// Declare before graph so managers outlive retained render sequences at teardown.
// Retain removed nodes until JUCE releases all other references; collect on the
// message thread to ensure plugin destruction cannot occur in the audio callback.
class Retirement
{
public:
    void retain(std::unique_ptr<AmpSource> source,juce::AudioProcessorGraph::Node::Ptr node)
    {if(source)source->unload();items.push_back({std::move(source),std::move(node)});}
    void collect()
    {
        jassert(juce::MessageManager::existsAndIsCurrentThread());
        items.erase(std::remove_if(items.begin(),items.end(),[](const auto& item){return !item.node||item.node->getReferenceCount()==1;}),items.end());
    }
private:
    struct Item
    {
        std::unique_ptr<AmpSource> source;
        juce::AudioProcessorGraph::Node::Ptr node; // Destroy node before manager.
        Item(std::unique_ptr<AmpSource> s,juce::AudioProcessorGraph::Node::Ptr n):source(std::move(s)),node(std::move(n)){}
        Item(Item&&)=default;
        // vector erase assignment must destroy the old node BEFORE its manager.
        Item& operator=(Item&& other) noexcept {node=nullptr;source.reset();source=std::move(other.source);node=std::move(other.node);return *this;}
    };
    std::vector<Item> items;
};

// Caller has detached AudioProcessorPlayer. New edges are validated first;
// previous amp/edges/editor survive a failed insertion. No pedal IDs involved.
inline juce::Result replace(juce::AudioProcessorGraph& graph,std::unique_ptr<AmpSource>& current,
                            juce::AudioProcessorGraph::Node::Ptr& currentNode,
                            juce::AudioProcessorGraph::NodeID pedals,juce::AudioProcessorGraph::NodeID output,
                            std::unique_ptr<AmpSource> candidate,Retirement& retired)
{
    using namespace juce;jassert(MessageManager::existsAndIsCurrentThread());
    if(!candidate||!candidate->loaded()||!current||!currentNode||!graph.getNodeForId(pedals)||!graph.getNodeForId(output))
        return Result::fail("Amp replacement preconditions failed; previous amp kept.");
    auto node=candidate->addToGraph(graph);bool connected=node!=nullptr;
    if(node)for(int channel=0;channel<2&&connected;++channel)
        connected=graph.addConnection({{pedals,channel},{node->nodeID,channel}})&&graph.addConnection({{node->nodeID,channel},{output,channel}});
    if(!connected)
    {
        if(node)graph.removeNode(node->nodeID);
        retired.retain(std::move(candidate),std::move(node));
        return Result::fail("Amp graph replacement failed; previous amp kept.");
    }
    current->closeEditor();graph.removeNode(currentNode->nodeID);
    retired.retain(std::move(current),std::move(currentNode));
    current=std::move(candidate);currentNode=std::move(node);return Result::ok();
}
}
