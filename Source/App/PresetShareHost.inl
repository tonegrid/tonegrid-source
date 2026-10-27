    void sharePreset(int index)
    {
        if(!pedals||!rigStore.contains(index))return;
        std::unique_ptr<XmlElement> snapshot;const auto read=rigStore.readSnapshot(index,snapshot);if(read.failed()){presetBrowser.warning(read.getErrorMessage());return;}
        const auto* source=snapshot->getChildByName("AmpSource");if(!source||source->getStringAttribute("type")!="NAM"){presetBrowser.warning("Community suggestions currently support NAM amp presets only. Your VST3 preset is unchanged.");return;}
        std::array<String,board::count> hashes;for(auto* item:snapshot->getChildIterator())if(item->hasTagName("Pedal")){const int p=item->getIntAttribute("id",-1);if(p>=0&&p<board::count)hashes[size_t(p)]=item->getStringAttribute("model");}
        const board::RigIdentityLookup savedIdentities=[hashes](int p,int){return hashes[size_t(p)];};
        board::State saved;const auto loaded=board::decode(*snapshot,{},savedIdentities,saved);
        if(loaded.failed()){presetBrowser.warning(loaded.getErrorMessage());return;}
        if(saved.sourceType!=amp::SourceType::Nam){presetBrowser.warning("Community suggestions currently support NAM amp presets only. Your VST3 preset is unchanged.");return;}
        String references="Amp: "+saved.namAmp.model.file.replaceCharacter('\\','/').fromLastOccurrenceOf("/",false,false)+"\nSHA256: "+saved.namAmp.model.sha256;
        if(saved.cabinet&&!saved.cabinet->empty())references+="\nCabinet IR: "+saved.cabinet->file+"\nIR SHA256: "+saved.cabinet->sha256;
        const auto ampManifest=tonegrid::t3k::localManifest(namLibrary.folder().getChildFile(saved.namAmp.model.file));
        if(ampManifest.isObject())references+="\nTone3000: "+ampManifest["url"].toString()+" / tone_id="+ampManifest["tone_id"].toString()+" / creator="+ampManifest["creator"].toString();
        for(int p=0;p<board::count;++p)if(board::isNam(p))
        {
            const auto& setting=saved.pedals[size_t(p)];const bool custom=saved.customPedalHashes[size_t(p)].isNotEmpty();
            auto entry=custom?userPedals.entry(p,saved.customPedalHashes[size_t(p)]):tonegrid::userpedals::Entry{hashes[size_t(p)],"Saved capture unavailable in the current factory library",{}};
            if(!custom)for(int n=0;n<board::presetCount(p);++n)if(pedals->identity(p,n)==entry.hash){entry.label=pedals->label(p,n);break;}
            references+="\n\n"+board::pedalName(p)+(setting.enabled?" [ON]: ":" [OFF]: ")+entry.label+"\nSHA256: "+entry.hash;
            if(entry.sourceUrl.isNotEmpty())references+="\nSource: "+entry.sourceUrl;
            if(custom&&entry.sourceUrl.isEmpty())references+="\nLocal personal capture; the reviewer will need to obtain it separately.";
        }
        const auto emailBody=tonegrid::sharing::body(saved,savedIdentities,references);
        auto* prompt=new tonegrid::popup::Prompt("Submit this saved preset for review for a future ToneBank release?\n\nThis opens an email to christian@tonegrid.net. You must send it yourself. Capture files are not attached.","YES");prompt->setCancelText("NO");
        auto& owner=presetDialog?static_cast<Component&>(*presetDialog.getComponent()):static_cast<Component&>(*this);
        shareConfirmDialog=tonegrid::popup::launch("Share preset",*prompt,owner,true,0,ModalCallbackFunction::create(
            [safe=Component::SafePointer<RigHost>(this),emailBody](int result)
            {
                if(!safe||result!=1)return;delete safe->shareDialog.getComponent();auto* view=new tonegrid::sharing::View(emailBody);
                safe->shareDialog=tonegrid::popup::launch("Preset suggestion",*view,*safe,true);view->openDraft();
            }));
    }
