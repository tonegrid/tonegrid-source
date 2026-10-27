    Component::SafePointer<DialogWindow> contentSettingsDialog;
    Component::SafePointer<tonegrid::content::SettingsView> contentSettingsView;
    std::future<Result> contentValidation;

    void reloadConfiguredToneBank()
    {
        if(bankLoading.valid())return;
        const auto overrideFile=appPaths.toneBankOverride();
        bankLoading=std::async(std::launch::async,[cache=toneBankCache,shipped=appPaths.shippedToneBank(),overrideFile]{
            auto result=tonegrid::tonebank::readCache(cache,shipped);
            if(overrideFile.exists()){
                String text;auto loaded=tonegrid::content::readText(overrideFile,8*1024*1024,text);
                tonegrid::tonebank::Bank personal;
                if(loaded.wasOk())loaded=tonegrid::tonebank::parse(text,personal);
                if(loaded.wasOk()){result.bank=std::move(personal);result.warning="Personal ToneBank override loaded.";}
                else result.warning=loaded.getErrorMessage()+" Official ToneBank kept.";
            }
            return result;
        });
    }
    void showContentSettings()
    {
        if(contentSettingsDialog){contentSettingsDialog->toFront(true);return;}
        auto* view=new tonegrid::content::SettingsView;contentSettingsView=view;
        const auto safe=Component::SafePointer<RigHost>(this);
        view->onOpen=[safe]{if(safe)safe->openEditableContent();};
        view->onReload=[safe]{if(safe)safe->validateEditableContent();};
        Component& owner=audioDialog?static_cast<Component&>(*audioDialog.getComponent()):static_cast<Component&>(*this);
        contentSettingsDialog=tonegrid::popup::launch("Editable content",*view,owner,true);
    }
    void openEditableContent()
    {
        const auto resources=appPaths.resources();
        const std::array<std::pair<File,File>,4> files{{
            {appPaths.calibrationProfiles(),resources.getChildFile("Calibration/interface-profiles.json")},
            {appPaths.jamCatalogue(),resources.getChildFile("Jam/backing-tracks.json")},
            {appPaths.learnCatalogue(),resources.getChildFile("Learn/lessons.json")},
            {appPaths.toneBankOverride(),appPaths.shippedToneBank()}
        }};
        for(const auto& [destination,source]:files){const auto result=tonegrid::content::createTemplate(destination,source);if(result.failed()){if(contentSettingsView)contentSettingsView->message(result.getErrorMessage());return;}}
        appPaths.editableContent().startAsProcess();
        if(contentSettingsView)contentSettingsView->message("Edit the JSON files, save, then click Reload content.");
    }
    void validateEditableContent()
    {
        if(contentValidation.valid())return;
        if(contentSettingsView)contentSettingsView->message("Checking your content...",true);
        const auto paths=appPaths;
        contentValidation=std::async(std::launch::async,[paths]{
            String text;
            const auto check=[&](const File& file,juce::int64 limit,auto decode)->Result{
                if(!file.exists())return Result::ok();auto loaded=tonegrid::content::readText(file,limit,text);return loaded.failed()?loaded:decode(text);
            };
            auto result=check(paths.calibrationProfiles(),2*1024*1024,[](const String& raw){std::vector<tonegrid::calibration::InterfaceProfile> values;return tonegrid::calibration::decodeInterfaceProfiles(raw,values);});if(result.failed())return result;
            result=check(paths.jamCatalogue(),2*1024*1024,[](const String& raw){std::vector<tonegrid::jam::Track> values;return tonegrid::jam::decodeCatalogue(raw,values);});if(result.failed())return result;
            result=check(paths.learnCatalogue(),8*1024*1024,[](const String& raw){std::vector<tonegrid::learn::Lesson> values;tonegrid::learn::Partner partner;return tonegrid::learn::decode(raw,values,partner);});if(result.failed())return result;
            return check(paths.toneBankOverride(),8*1024*1024,[](const String& raw){tonegrid::tonebank::Bank values;return tonegrid::tonebank::parse(raw,values);});
        });
    }
    void pollEditableContent()
    {
        if(!contentValidation.valid()||contentValidation.wait_for(std::chrono::seconds(0))!=std::future_status::ready)return;
        Result result=Result::ok();try{result=contentValidation.get();}catch(const std::exception& error){result=Result::fail(error.what());}
        if(result.wasOk()){jamView.reload();learnView.reload();reloadConfiguredToneBank();reloadSlots(selected());}
        const auto message=result.wasOk()?String("Content reloaded. Reopen Calibration to see changed profiles."):result.getErrorMessage()+" Previous catalogues kept.";
        if(contentSettingsView)contentSettingsView->message(message);
        report(message);
    }
