#include "Language144Tests.h"
#include "EffectsControls.h"
#include "Effects/Reverb/ReverbControls.h"
#include "Effects/StereoSpread/SpreadControls.h"
#include "Effects/AmpEq/AdvancedEqControls.h"
#include "Shared/Localization/LanguageUI.h"
#include "BetaWelcome.h"
#include "AudioSettingsPanel.h"
#include "PresetBrowser.h"
#include "DiagnosticView.h"
#include "Modules/Record/UI/RecordView.h"
#include "AudioRecovery126Devices.h"
#include "Modules/Jam/UI/JamView.h"
#include "Modules/Learn/UI/LearnView.h"

namespace language144Tests
{
bool run(const juce::File& resources,const juce::File& evidence,juce::String& report)
{
    using namespace juce;using namespace tonegrid;bool ok=true;int checks=0;
    const auto check=[&](bool value,const String& label){++checks;ok&=value;report+=String(value?"PASS ":"FAIL ")+label+"\n";};
    const auto root=File::getSpecialLocation(File::tempDirectory).getChildFile("ToneGrid-language144-"+Uuid().toString());root.createDirectory();
    struct Cleanup{File folder;~Cleanup(){folder.deleteRecursively();}} cleanup{root};
    const auto shipped=resources.getChildFile("Translations"),user=root.getChildFile("Content/Translations"),preference=root.getChildFile("Preferences/language.json");
    auto& language=l10n::Language::instance();language.configure(shipped,user,preference,"fr-FR");
    check(language.available().size()==10&&language.locale()=="fr","Ten packs discovered; supported system language selected");
    check(user.findChildFiles(File::findFiles,false,"*.json").size()==10,"Editable packs seeded outside the application bundle");
    check(l10n::tr("common.close")==String::fromUTF8("Fermer"),"French text selected by stable semantic key");
    check(l10n::translate("Noise Gate")=="Noise Gate"&&l10n::translate("PLAY")=="PLAY"&&l10n::translate("Reverb")=="Reverb","Branded tabs and music terms stay unchanged");
    check(l10n::translate("[AMP] Example.nam")=="[AMP] Example.nam","Unknown NAM/content names pass through unchanged");
    check(!language.select("../fr",false)&&language.locale()=="fr","Unsafe or unavailable locale cannot be selected");
    check(language.select("ja")&&preference.loadFileAsString().contains("ja"),"Language preference saved to the isolated profile");
    language.configure(shipped,user,preference,"de-DE");check(language.locale()=="ja","Explicit preference wins over operating system language");
    l10n::Pack parsed;check(l10n::Language::decode(R"({"schemaVersion":1,"locale":"fr","nativeName":"Français","futureMetadata":{"hello":true},"strings":{"common.close":"Fermer","future.key":"future","bad":4,"empty":""}})",parsed)&&parsed.strings.count("future.key")&&!parsed.strings.count("bad")&&!parsed.strings.count("empty"),"Additional keys/metadata are tolerated; non-text or empty entries are ignored");
    check(!l10n::Language::decode("{broken",parsed)&&!l10n::Language::decode(R"({"schemaVersion":2,"locale":"fr","nativeName":"French","strings":{}})",parsed),"Malformed JSON and incompatible schema fail safely");
    const auto french=user.getChildFile("fr.json");const auto original=french.loadFileAsString();
    french.replaceWithText(R"({"schemaVersion":1,"locale":"fr","nativeName":"Français","strings":{"common.close":"Mon texte","welcome.intro":false,"record.guitar":"WRONG TOKEN","jam.backing_tracks":"Pistes d'accompagnement","record.backing":"Accompagnement","future.key":"future"}})");
    language.reload();language.select("fr",false);
    check(l10n::tr("common.close")=="Mon texte"&&l10n::tr("common.save")!="common.save","User edits override shipped strings; missing keys keep shipped fallback");
    check(l10n::translate("Backingtrack")=="Backingtrack","Backingtrack remains literal even with older profile overrides and popup titles");
    check(l10n::format("record.guitar",{{"number","2"}})==String::fromUTF8("Guitare 2"),"Invalid placeholders are rejected, keeping the valid shipped sentence");
    french.replaceWithText("{not finished");language.reload();check(l10n::tr("common.close")=="Mon texte","Reload retains the last valid pack after a malformed edit");
    language.configure(shipped,user,preference,"en");language.select("fr",false);check(l10n::tr("common.close")=="Fermer","Fresh launch falls back to shipped pack when an edited file is corrupt");
    french.replaceWithText(original);language.reload();
    l10n::TextButton bound("Close");juce::Label content;content.setText("Close",dontSendNotification);
    juce::TextEditor editableName;editableName.setText("Close",false);int actions=0;bound.onClick=[&]{++actions;};
    language.select("es",false);check(bound.getButtonText()=="Cerrar"&&content.getText()=="Close"&&editableName.getText()=="Close"&&actions==0,"Only explicitly bound UI changes; user content and actions are untouched");
    ComboBox filter;l10n::item(filter,"All",1);filter.setSelectedId(1,dontSendNotification);filter.onChange=[&]{++actions;};
    language.select("de",false);check(filter.getText()=="Alle"&&filter.getSelectedId()==1&&actions==0,"Selected filter text updates without changing its identity or firing callbacks");
    {l10n::TextButton temporary("Save");}language.select("fr",false);check(bound.getButtonText()=="Fermer","Closed widgets are safe during later language changes");
    auto deviceState=std::make_shared<audioRecovery126Tests::State>();deviceState->busy=false;
    AudioDeviceManager devices;while(!devices.getAvailableDeviceTypes().isEmpty())devices.removeAudioDeviceType(devices.getAvailableDeviceTypes().getLast());
    devices.addAudioDeviceType(std::make_unique<audioRecovery126Tests::Type>(deviceState));
    check(devices.initialise(1,2,nullptr,false).isEmpty(),"Open only a synthetic audio device for UI verification");
    const auto setup=devices.getAudioDeviceSetup();const int starts=deviceState->starts,attempts=deviceState->attempts;
    welcome::View welcome;AudioSettingsPanel audio(devices,false);audio.setAudioStatus("Audio is ready. Choose the input where your guitar is plugged in, and the output connected to your speakers or headphones.",false);
    diagnostics::View diagnostic("Diagnostic stays in English");
    record::RecordView recordView(root.getChildFile("WebView2"),true);recordView.setSize(1660,940);record::RecordSession session;recordView.setState(session,root);recordView.setBusy(false,false,false);
    PresetBrowser presets;presets.setUsers({"Close","Nuno - The Police"});
    tonebank::Bank bank;check(tonebank::parse(resources.getChildFile("ToneBank/tonebank.json").loadFileAsString(),bank).wasOk(),"Read the shipped ToneBank for localized browser verification");presets.setBank(bank);presets.showToneBank(true);
    jam::View jamView;jamView.configurePlayer(root.getChildFile("JamWebView2"),true);jamView.setSize(1200,680);
    learn::View learnView;learnView.configure(resources.getChildFile("Learn/lessons.json"),root.getChildFile("favourites.json"),root.getChildFile("LearnWebView2"),true);learnView.setSize(1200,680);
    // Keep heavyweight UI fixtures on the heap; the legacy fixture already
    // has a large native stack frame. Audio-state preservation is checked by
    // the dedicated host probe, rather than duplicated in this snapshot loop.
    auto reverb=std::make_unique<ReverbControls>();auto spread=std::make_unique<SpreadControls>();
    auto modulation=std::make_unique<EffectsControls>(EffectsPanel::Mod);auto delay=std::make_unique<EffectsControls>(EffectsPanel::Delay);
    auto pitch=std::make_unique<EffectsControls>(EffectsPanel::Pitch);auto eq=std::make_unique<ampui::AdvancedEqControls>();
    evidence.createDirectory();const auto locales=language.available();int step=0;
    class Poll final:public Timer{public:std::function<void()> action;void timerCallback()override{action();}} poll;
    const auto snapshot=[&](Component& component,const String& name)
    {MemoryOutputStream output;PNGImageFormat png;const auto image=component.createComponentSnapshot(component.getLocalBounds());check(png.writeImageToStream(image,output)&&evidence.getChildFile(name+".png").replaceWithData(output.getData(),output.getDataSize()),"UI snapshot "+name);};
    const auto effectSnapshot=[&](popup::View& body,const String& name){popup::Frame frame(body,false,name,0);frame.setSize(body.getWidth()+84,body.popupContentHeight(body.getWidth())+148);snapshot(frame,name+"-"+language.locale());};
    poll.action=[&]
    {
        if(step<int(locales.size())*2)
        {
            const auto& locale=locales[size_t(step/2)];
            if(step%2==0)language.select(locale.locale,false);
            else
            {
                check(recordView.renderFirst.getButtonText()==l10n::tr("record.render")&&recordView.exportMix.getComponentID()=="record-export-all","Recording actions keep stable identity in "+locale.locale);
                check(deviceState->starts==starts&&deviceState->attempts==attempts&&devices.getAudioDeviceSetup().inputDeviceName==setup.inputDeviceName,"Language change does not reopen audio in "+locale.locale);
                popup::Frame welcomeFrame(welcome,false,"Welcome to ToneGrid",0);welcomeFrame.setSize(724,welcome.popupContentHeight(640)+148);snapshot(welcomeFrame,"welcome-"+locale.locale);
                popup::Frame audioFrame(audio,false,"Audio Settings",0);audioFrame.setSize(844,audio.popupContentHeight(760)+148);snapshot(audioFrame,"settings-"+locale.locale);
                snapshot(recordView,"record-"+locale.locale);
                popup::Frame presetsFrame(presets,false,"Presets",0);snapshot(presetsFrame,"presets-"+locale.locale);
                snapshot(jamView,"jam-"+locale.locale);snapshot(learnView,"learn-"+locale.locale);
                effectSnapshot(*reverb,"reverb");effectSnapshot(*spread,"spread");effectSnapshot(*modulation,"mod");
                effectSnapshot(*delay,"delay");effectSnapshot(*pitch,"pitch");effectSnapshot(*eq,"eq");
                check(welcome.close.getRight()<=welcome.getWidth()&&welcome.never.getX()>=0,"Welcome buttons stay in bounds in "+locale.locale);
            }
            ++step;return;
        }
        language.select("en",false);poll.stopTimer();MessageManager::getInstance()->stopDispatchLoop();
    };
    poll.startTimer(60);MessageManager::getInstance()->runDispatchLoop();
    // External text/diagnostics are deliberately not registered for translation.
    check(!diagnostic.popupLocalised(),"Send Diagnostic explicitly opts out of shared popup translation");
    check(deviceState->starts==starts&&deviceState->attempts==attempts,"All ten changes preserve the running synthetic device");
    const auto missing=root.getChildFile("missing");language.configure(missing,missing,{},"zz-ZZ");
    check(language.available().size()==1&&language.locale()=="en"&&l10n::tr("common.close")=="Close","Embedded English works when all external packs are unavailable");
    report+=String(checks)+" localization 1.1.44 checks; "+(ok?"ALL PASSED":"FAILURES")+"\n";
    evidence.getChildFile("results.txt").replaceWithText(report);return ok;
}
}
