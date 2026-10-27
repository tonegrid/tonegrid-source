#pragma once
#include <NAM/dsp.h>
#include <NAM/slimmable.h>
#include <juce_core/juce_core.h>

namespace tonegrid::namquality
{
// Model creation/control thread only, before publication or with audio detached.
// A2 container tiers use 0 = Lite, 1 = Full. A1/non-slimmable models are unchanged.
inline bool apply(nam::DSP& model, bool lite)
{
    auto* scalable=dynamic_cast<nam::SlimmableModel*>(&model);
    if(!scalable)return false;
    scalable->SetSlimmableSize(lite?0.0:1.0);
    return true;
}
inline bool read(const juce::File& file)
{
    auto xml=juce::XmlDocument::parse(file);
    return xml&&xml->hasTagName("NamQuality")&&xml->getIntAttribute("version")==1&&xml->getStringAttribute("lite")=="1";
}
inline bool write(const juce::File& file,bool lite)
{
    if(file.getParentDirectory().createDirectory().failed())return false;
    juce::XmlElement xml("NamQuality");xml.setAttribute("version",1);xml.setAttribute("lite",lite?1:0);
    juce::TemporaryFile temporary(file);
    return temporary.getFile().replaceWithText(xml.toString())&&temporary.overwriteTargetFileWithTemporary();
}
}
