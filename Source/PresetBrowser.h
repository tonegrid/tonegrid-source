#pragma once
#include "Content/Presets/ToneBank.h"
#include "Shared/UI/UiStyle.h"
#include "Shared/UI/PopupStyle.h"

namespace tonegrid
{
class PresetBrowser final : public popup::View, private juce::ListBoxModel
{
public:
    std::function<void(int)> onUser;
    std::function<void(int)> onRename,onDelete,onShare;
    std::function<void(int,int)> onMove;
    std::function<void(size_t)> onTone;
    std::function<void()> onReload;
    PresetBrowser()
    {
        for(auto* c:std::initializer_list<juce::Component*>{&userTab,&toneTab,&explanation,&genre,&search,&genreLabel,&searchLabel,&reload,&list,&status})addAndMakeVisible(c);
        userTab.setComponentID("user-presets-tab");toneTab.setComponentID("tonebank-tab");genre.setComponentID("tonebank-genre");search.setComponentID("tonebank-search");reload.setComponentID("tonebank-reload");list.setComponentID("preset-list");
        userTab.onClick=[this]{showToneBank(false);};toneTab.onClick=[this]{showToneBank(true);};
        tonegrid::l10n::text(genreLabel,"Genre",juce::dontSendNotification);tonegrid::l10n::text(searchLabel,"Search",juce::dontSendNotification);
        tonegrid::l10n::item(genre,"All",1);genre.setSelectedId(1,juce::dontSendNotification);genre.onChange=[this]{updateRows();};
        search.onTextChange=[this]{updateRows();};
        // TextEditor consumes Escape before DialogWindow can handle it.
        search.onEscapeKey=[this]{if(auto* dialog=findParentComponentOfClass<juce::DialogWindow>())dialog->closeButtonPressed();};
        tonegrid::l10n::placeholder(search,"Artist, song or NAM amp",juce::Colours::grey);
        tonegrid::l10n::tooltip(reload,"Reload your personal ToneBank JSON if present; otherwise download the latest official ToneBank.");reload.onClick=[this]{if(onReload)onReload();};
        list.setModel(this);list.setRowHeight(42);list.setColour(juce::ListBox::backgroundColourId,popup::field());
        explanation.setColour(juce::Label::textColourId,popup::mutedInk());status.setMultiLine(true);status.setReadOnly(true);status.setScrollbarsShown(true);status.setColour(juce::TextEditor::textColourId,popup::mutedInk());status.setColour(juce::TextEditor::backgroundColourId,juce::Colours::transparentBlack);status.setColour(juce::TextEditor::outlineColourId,juce::Colours::transparentBlack);
        for(auto* tab:{&userTab,&toneTab}){tab->setColour(juce::TextButton::buttonColourId,popup::field());tab->setColour(juce::TextButton::buttonOnColourId,popup::selected());}
        setSize(850,650);showToneBank(false);
    }
    ~PresetBrowser() override {list.setModel(nullptr);}
    void languageChanged()override{updateRows();}
    void setBank(const tonebank::Bank& value,const juce::String& message={})
    {
        const auto selectedGenre=genre.getText();
        bank=value;bankMessage=message;genre.clear(juce::dontSendNotification);tonegrid::l10n::item(genre,"All",1);
        const auto groups=bank.genres;for(int i=0;i<groups.size();++i)genre.addItem(groups[i],i+2);genre.setSelectedId(groups.contains(selectedGenre)?groups.indexOf(selectedGenre)+2:1,juce::dontSendNotification);updateRows();
    }
    void setReloading(bool value){reload.setEnabled(!value);tonegrid::l10n::button(reload,value?"Updating...":"Reload ToneBank");}
    void setUsers(juce::StringArray value,std::vector<bool> saved={}){users=std::move(value);stored=std::move(saved);if(stored.size()!=(size_t)users.size())stored.assign((size_t)users.size(),true);if(!tones)updateRows();}
    void showToneBank(bool show)
    {
        tones=show;userTab.setToggleState(!show,juce::dontSendNotification);toneTab.setToggleState(show,juce::dontSendNotification);
        // Give the active tab the same explicit fill for either JUCE button state.
        for(auto* tab:{&userTab,&toneTab}){const auto fill=tab->getToggleState()?popup::selected():popup::field();tab->setColour(juce::TextButton::buttonColourId,fill);tab->setColour(juce::TextButton::buttonOnColourId,fill);}
        tonegrid::l10n::text(explanation,show?"Curated ToneGrid song tones. Select a tone to load it; use Save to create your own editable local copy.":"Your local presets. Load, edit and save your own tones.",juce::dontSendNotification);
        for(auto* c:std::initializer_list<juce::Component*>{&genre,&search,&genreLabel,&searchLabel,&reload})c->setVisible(show);
        updateRows();resized();repaint();
    }
    bool toneTabActive()const{return tones;}
    int popupMinimumWidth()const override{return 760;}
    int visibleCount()const{return tones?(int)rows.size():users.size();}
    void setSearch(const juce::String& value){search.setText(value);}
    void setGenre(const juce::String& value){for(int i=0;i<genre.getNumItems();++i)if(genre.getItemText(i).equalsIgnoreCase(value)){genre.setSelectedId(genre.getItemId(i),juce::sendNotificationSync);break;}}
    void activateRow(int row){if(row<0||row>=visibleCount())return;if(tones){if(onTone)onTone(rows[(size_t)row]);}else if(onUser)onUser(row);}
    void focusUser(int row){if(!tones){list.selectRow(row);list.scrollToEnsureRowIsOnscreen(row);}}
    void warning(const juce::String& text){status.setText(text,juce::dontSendNotification);resized();}
    void paint(juce::Graphics& g)override
    {
        g.setColour(popup::mutedInk());g.setFont(juce::FontOptions(12));
        if(tones){auto r=heading;g.drawText(tonegrid::l10n::translate("#"),r.removeFromLeft(40),juce::Justification::centredLeft);g.drawText(tonegrid::l10n::translate("ARTIST / GROUP"),r.removeFromLeft(180),juce::Justification::centredLeft);auto version=r.removeFromRight(62),creator=r.removeFromRight(100),part=r.removeFromRight(85);g.drawText(tonegrid::l10n::translate("SONG"),r,juce::Justification::centredLeft);g.drawText(tonegrid::l10n::translate("PART"),part,juce::Justification::centredLeft);g.drawText(tonegrid::l10n::translate("CREATOR"),creator,juce::Justification::centredLeft);g.drawText(tonegrid::l10n::translate("VERSION"),version,juce::Justification::centredLeft);}
        else g.drawText(tonegrid::l10n::translate("YOUR PRESET SLOTS"),heading,juce::Justification::centredLeft);
    }
    void resized()override
    {
        auto area=getLocalBounds().reduced(8);auto tabs=area.removeFromTop(38);userTab.setBounds(tabs.removeFromLeft(180));tabs.removeFromLeft(8);toneTab.setBounds(tabs.removeFromLeft(180));
        area.removeFromTop(8);explanation.setBounds(area.removeFromTop(44));
        if(tones){auto filters=area.removeFromTop(38);const auto font=ui::font(14);genreLabel.setBounds(filters.removeFromLeft(juce::jmax(48,font.getStringWidth(genreLabel.getText())+10)));genre.setBounds(filters.removeFromLeft(180));filters.removeFromLeft(18);searchLabel.setBounds(filters.removeFromLeft(juce::jmax(54,font.getStringWidth(searchLabel.getText())+10)));reload.setBounds(filters.removeFromRight(juce::jlimit(146,230,font.getStringWidth(reload.getButtonText())+24)));filters.removeFromRight(14);search.setBounds(filters);area.removeFromTop(12);}
        heading=area.removeFromTop(24).reduced(12,0);status.setBounds(area.removeFromBottom(status.getText().containsChar('\n')?64:28));area.removeFromBottom(8);list.setBounds(area);
    }
private:
    class ActionButton final:public juce::Button
    {
    public:
        ActionButton(int action):Button(action==1?"Delete preset":action==2?"Move preset up":action==3?"Move preset down":action==4?"Suggest preset for ToneBank":"Rename preset"),kind(action)
        {setComponentID(action==1?"preset-delete":action==2?"preset-up":action==3?"preset-down":action==4?"preset-share":"preset-rename");l10n::tooltip(*this,getName());}
        void paintButton(juce::Graphics& g,bool hover,bool down) override
        {
            using namespace juce;auto r=getLocalBounds().toFloat().reduced(1);
            if(hover||hasKeyboardFocus(true)){g.setColour(Colour(down?0xff41494e:0xff2c353b));g.fillRoundedRectangle(r,4);}
            g.setColour(isEnabled()?Colour(0xffd6dadc):Colour(0xff485056));Path p;
            if(kind==2||kind==3)
            {const float y=kind==2?8.f:16.f,other=kind==2?15.f:9.f;p.startNewSubPath(6,other);p.lineTo(12,y);p.lineTo(18,other);}
            else if(kind==4)
            {p.addEllipse(4,9,5,5);p.addEllipse(15,3,5,5);p.addEllipse(15,16,5,5);p.startNewSubPath(9,11);p.lineTo(15,6);p.startNewSubPath(9,13);p.lineTo(15,18);}
            else if(kind==1)
            {p.startNewSubPath(6,7);p.lineTo(18,7);p.startNewSubPath(8,7);p.lineTo(9,19);p.lineTo(15,19);p.lineTo(16,7);p.startNewSubPath(10,7);p.lineTo(10,4);p.lineTo(14,4);p.lineTo(14,7);p.startNewSubPath(11,10);p.lineTo(11,16);p.startNewSubPath(13,10);p.lineTo(13,16);}
            else
            {p.startNewSubPath(5,15);p.lineTo(15,5);p.lineTo(19,9);p.lineTo(9,19);p.lineTo(4,20);p.closeSubPath();p.startNewSubPath(13,7);p.lineTo(17,11);p.startNewSubPath(5,15);p.lineTo(9,19);}
            const auto glyph=r.withSizeKeepingCentre(24,24);p.applyTransform(AffineTransform::translation(glyph.getX(),glyph.getY()));g.strokePath(p,PathStrokeType(1.5f,PathStrokeType::curved,PathStrokeType::rounded));
        }
    private:int kind;
    };
    class UserRow final:public juce::Component,public juce::SettableTooltipClient
    {
    public:
        explicit UserRow(PresetBrowser& browser):owner(browser)
        {addAndMakeVisible(rename);addAndMakeVisible(erase);addAndMakeVisible(share);addAndMakeVisible(up);addAndMakeVisible(down);up.onClick=[this]{if(owner.onMove)owner.onMove(row,-1);};down.onClick=[this]{if(owner.onMove)owner.onMove(row,1);};rename.onClick=[this]{if(owner.onRename)owner.onRename(row);};erase.onClick=[this]{if(owner.onDelete)owner.onDelete(row);};share.onClick=[this]{if(owner.onShare)owner.onShare(row);};}
        void configure(int index,bool selected){row=index;chosen=selected;setTooltip(owner.users[row]);rename.setEnabled(owner.stored[(size_t)row]);erase.setEnabled(owner.stored[(size_t)row]);share.setEnabled(owner.stored[(size_t)row]);up.setEnabled(row>0&&owner.stored[(size_t)row]);down.setEnabled(row+1<owner.users.size()&&owner.stored[(size_t)row]);repaint();}
        void paint(juce::Graphics& g)override{owner.paintListBoxItem(row,g,getWidth(),getHeight(),chosen);}
        void resized()override{auto actions=getLocalBounds().removeFromRight(192).reduced(4,3);down.setBounds(actions.removeFromRight(32));actions.removeFromRight(4);up.setBounds(actions.removeFromRight(32));actions.removeFromRight(4);share.setBounds(actions.removeFromRight(32));actions.removeFromRight(4);erase.setBounds(actions.removeFromRight(32));actions.removeFromRight(4);rename.setBounds(actions.removeFromRight(32));}
        void mouseUp(const juce::MouseEvent& event)override{if(event.mouseWasClicked()){owner.list.selectRow(row);owner.activateRow(row);}}
    private:PresetBrowser& owner;int row=0;bool chosen=false;ActionButton rename{0},erase{1},up{2},down{3},share{4};
    };
    juce::Component* refreshComponentForRow(int row,bool selected,juce::Component* existing)override
    {
        if(tones||row<0||row>=users.size()){delete existing;return nullptr;}
        auto* component=dynamic_cast<UserRow*>(existing);if(!component){delete existing;component=new UserRow(*this);}component->configure(row,selected);return component;
    }
    int getNumRows()override{return visibleCount();}
    void paintListBoxItem(int row,juce::Graphics& g,int width,int height,bool selected)override
    {
        if(row<0||row>=visibleCount())return;
        g.fillAll(row%2?juce::Colour(0xff171e20):juce::Colour(0xff101719));
        if(selected){auto selectedBounds=juce::Rectangle<float>(1,1,float(width-2),float(height-2));g.setColour(popup::selected());g.fillRoundedRectangle(selectedBounds,6);g.setColour(popup::outline());g.drawRoundedRectangle(selectedBounds,6,1);}
        g.setColour(popup::ink());g.setFont(juce::FontOptions("Segoe UI",14.f,juce::Font::plain));auto r=juce::Rectangle<int>(12,0,width-24,height);
        if(tones){const auto& p=bank.presets[rows[(size_t)row]];g.drawText(juce::String(p.rank),r.removeFromLeft(40),juce::Justification::centredLeft);g.drawText(p.artist,r.removeFromLeft(180),juce::Justification::centredLeft,true);auto version=r.removeFromRight(62),creator=r.removeFromRight(100),part=r.removeFromRight(85);g.drawText(p.song,r,juce::Justification::centredLeft,true);g.setColour(popup::mutedInk());g.drawText(p.part,part,juce::Justification::centredLeft,true);g.drawText(p.creator,creator,juce::Justification::centredLeft,true);g.drawText(p.version,version,juce::Justification::centredLeft,true);}
        else g.drawText(users[row],r.withTrimmedRight(192),juce::Justification::centredLeft);
    }
    void listBoxItemClicked(int row,const juce::MouseEvent&)override{activateRow(row);}
    void returnKeyPressed(int row)override{activateRow(row);}
    void updateRows()
    {
        rows=tonebank::filter(bank,genre.getSelectedId()==1?juce::String("All"):genre.getText(),search.getText());list.deselectAllRows();list.updateContent();list.repaint();
        if(tones)tonegrid::l10n::text(status,bank.presets.empty()?(bankMessage.isNotEmpty()?bankMessage:"Loading ToneBank…"):rows.empty()?"No tones match this Genre and Search.":l10n::format("preset.count",{{"count",juce::String((int)rows.size())}})+(bankMessage.isNotEmpty()?"\n"+bankMessage:""),juce::dontSendNotification);
        else tonegrid::l10n::text(status,"Use Save in the main window to create or update a User Preset.",juce::dontSendNotification);
        resized();
    }
    tonebank::Bank bank;juce::String bankMessage;juce::StringArray users;std::vector<bool> stored;std::vector<size_t> rows;bool tones=false;
    tonegrid::l10n::TextButton userTab{"USER PRESETS"},toneTab{"TONEBANK"},reload{"Reload ToneBank"};
    juce::Label explanation,genreLabel,searchLabel;juce::ComboBox genre;juce::TextEditor search,status;juce::ListBox list;juce::Rectangle<int> heading;
};
}
