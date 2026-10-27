#pragma once
#include <juce_core/juce_core.h>
#include <vector>

namespace tonegrid::storage
{
// A small recovery journal for paired preset/name writes and adjacent moves.
// Originals are retained until every replacement has committed. On restart an
// interrupted transaction rolls back; a completed transaction only needs cleanup.
struct Update {juce::File target;juce::MemoryBlock bytes;bool exists=true;};
inline bool presetName(const juce::String& name)
{
    if(name=="starter-presets-sha256.txt")return true; // Committed with first-launch presets.
    if(!name.startsWith("slot-"))return false;
    const auto suffix=name.endsWith(".tonexrig")?juce::String(".tonexrig"):name.endsWith(".name")?juce::String(".name"):juce::String{};
    if(suffix.isEmpty())return false;
    const auto number=name.substring(5,name.length()-suffix.length());
    return number.isNotEmpty()&&number.length()<=5&&number.containsOnly("0123456789")&&number.getIntValue()>=1&&number.getIntValue()<=10000;
}
inline bool replace(const juce::File& target,const juce::File& source)
{
    juce::TemporaryFile temporary(target);
    return source.copyFileTo(temporary.getFile())&&temporary.overwriteTargetFileWithTemporary();
}
inline juce::Result recover(const juce::File& bank)
{
    using namespace juce;
    for(const auto& folder:bank.findChildFiles(File::findDirectories,false,".preset-transaction-*",File::FollowSymlinks::no))
    {
        if(folder.isSymbolicLink())return Result::fail("Invalid preset recovery directory.");
        if(folder.getChildFile("committed").existsAsFile()){folder.deleteRecursively();continue;}
        const auto journal=folder.getChildFile("journal.xml");
        // No journal means no originals were touched; only staging was interrupted.
        if(!journal.existsAsFile()){folder.deleteRecursively();continue;}
        if(journal.getSize()>16384)return Result::fail("Preset recovery journal is oversized.");
        auto xml=XmlDocument::parse(journal);
        if(!xml||!xml->hasTagName("PresetTransaction")||xml->getIntAttribute("version")!=1||xml->getNumChildElements()>32)return Result::fail("Invalid preset recovery journal.");
        // Validate the entire recovery plan before restoring any destination.
        StringArray destinations;
        for(auto* item:xml->getChildIterator())
        {
            const auto name=item->getStringAttribute("file"),backup=item->getStringAttribute("backup");
            const auto existed=item->getStringAttribute("existed");
            if(!item->hasTagName("File")||!presetName(name)||destinations.contains(name)||!backup.containsOnly("0123456789")||backup.isEmpty()||(existed!="0"&&existed!="1"))return Result::fail("Invalid preset recovery path.");
            destinations.add(name);
            const auto target=bank.getChildFile(name),original=folder.getChildFile(backup);
            if(target.isSymbolicLink()||original.isSymbolicLink())return Result::fail("Invalid preset recovery file.");
            if(item->getBoolAttribute("existed")&&(!original.existsAsFile()||original.getSize()>128*1024*1024))return Result::fail("Preset recovery backup is missing.");
        }
        bool ok=true;for(auto* item:xml->getChildIterator())
        {
            const auto target=bank.getChildFile(item->getStringAttribute("file")),original=folder.getChildFile(item->getStringAttribute("backup"));
            if(item->getBoolAttribute("existed"))ok=replace(target,original)&&ok;
            else if(target.exists())ok=target.deleteFile()&&ok;
        }
        if(!ok)return Result::fail("Could not restore interrupted preset operation. Recovery retained in "+folder.getFullPathName());
        folder.deleteRecursively();
    }
    return Result::ok();
}
inline juce::Result transact(const juce::File& bank,const std::vector<Update>& updates)
{
    using namespace juce;
    if(updates.empty()||updates.size()>32)return Result::fail("Invalid preset operation.");
    if(bank.createDirectory().failed())return Result::fail("Cannot create preset directory.");
    auto recovery=recover(bank);if(recovery.failed())return recovery;
    for(const auto& update:updates)if(update.target.getParentDirectory()!=bank||!presetName(update.target.getFileName())||update.target.isSymbolicLink()||(update.target.exists()&&!update.target.existsAsFile())||update.bytes.getSize()>128*1024*1024)return Result::fail("Invalid preset destination.");
    const auto folder=bank.getChildFile(".preset-transaction-"+Uuid().toString());if(folder.createDirectory().failed())return Result::fail("Cannot stage preset operation.");
    XmlElement journal("PresetTransaction");journal.setAttribute("version",1);
    for(size_t i=0;i<updates.size();++i)
    {
        const auto& update=updates[i];const auto backup=String(int(i)),next="new-"+backup;
        auto* item=journal.createNewChildElement("File");item->setAttribute("file",update.target.getFileName());item->setAttribute("backup",backup);item->setAttribute("existed",update.target.existsAsFile());
        if((update.target.existsAsFile()&&!update.target.copyFileTo(folder.getChildFile(backup)))||(update.exists&&!folder.getChildFile(next).replaceWithData(update.bytes.getData(),update.bytes.getSize())))
        {folder.deleteRecursively();return Result::fail("Cannot stage preset files. Originals retained.");}
    }
    // Write the complete recovery plan before changing the first destination.
    if(!folder.getChildFile("journal.xml").replaceWithText(journal.toString())){folder.deleteRecursively();return Result::fail("Cannot write preset recovery journal.");}
    bool ok=true;for(size_t i=0;i<updates.size()&&ok;++i){const auto& update=updates[i];ok=update.exists?replace(update.target,folder.getChildFile("new-"+String(int(i)))):!update.target.exists()||update.target.deleteFile();}
    if(ok)ok=folder.getChildFile("committed").replaceWithText("1");
    if(!ok){recovery=recover(bank);return recovery.failed()?recovery:Result::fail("Preset operation failed. Original presets restored.");}
    folder.deleteRecursively();return Result::ok();
}
inline Update text(const juce::File& target,const juce::String& value)
{return {target,juce::MemoryBlock(value.toRawUTF8(),value.getNumBytesAsUTF8()),true};}
}
