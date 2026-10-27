#pragma once
#include "Content/Cabinets/CabinetBrowser.h"
#include "Shared/UI/Library/LibraryBrowserShell.h"
#include "Shared/UI/Library/LibraryNavigation.h"
#include <ToneGridApplicationAssets.h>

namespace tonegrid::cabinet
{
class Recommendation final:public popup::View
{
public:
    explicit Recommendation(const juce::String& cabinet)
    {
        for(auto* c:std::initializer_list<juce::Component*>{&message,&recommended,&fallback,&cancel})addAndMakeVisible(c);
        tonegrid::l10n::text(message,"Load the recommended cabinet IR?\n\n"+cabinet,juce::dontSendNotification);message.setFont(ui::font(16));message.setJustificationType(juce::Justification::topLeft);
        recommended.onClick=[this]{finish(1);};fallback.onClick=[this]{finish(2);};cancel.onClick=[this]{finish(0);};setSize(590,180);
    }
    void resized()override{auto r=getLocalBounds().reduced(12);message.setBounds(r.removeFromTop(110));auto buttons=r.removeFromBottom(36);recommended.setBounds(buttons.removeFromLeft(200));buttons.removeFromLeft(10);fallback.setBounds(buttons.removeFromLeft(170));buttons.removeFromLeft(10);cancel.setBounds(buttons);}
private:
    void finish(int value){if(auto* window=findParentComponentOfClass<juce::DialogWindow>())window->exitModalState(value);}
    juce::Label message;tonegrid::l10n::TextButton recommended{"Load recommended"},fallback{"Use default cabinet"},cancel{"Cancel"};
};

class Browser final:public libraryui::LibraryBrowserShell,private juce::ListBoxModel
{
    using Cell = libraryui::NavigationRow;
    class SidebarModel final:public juce::ListBoxModel
    {
    public:explicit SidebarModel(Browser& b):owner(b){}
        int getNumRows()override{return owner.categories.size();}
        void paintListBoxItem(int row,juce::Graphics& g,int w,int h,bool)override
        {
            if(!juce::isPositiveAndBelow(row,owner.categories.size()))return;
            const auto& path=owner.categories[row];int count=0;
            for(const auto& e:owner.entries)if(row==0||(row==1&&owner.favorites.contains(e.state.sha256))||(row>=2&&e.state.file.startsWith(path+"/")))++count;
            const bool selected=(row==0&&owner.mode==all)||(row==1&&owner.mode==favoriteMode)||(row>=2&&owner.mode==folderMode&&(owner.currentFolder==path||owner.currentFolder.startsWith(path+"/")));
            libraryui::sidebarRow(g,w,h,row==0?l10n::translate("All cabinets"):row==1?l10n::translate("Favorites"):path,count,selected,row==1);
        }
        void listBoxItemClicked(int row,const juce::MouseEvent&)override{if(!juce::isPositiveAndBelow(row,owner.categories.size()))return;if(row==0)owner.showAll();else if(row==1)owner.showFavorites();else owner.navigate(owner.categories[row]);}
    private:Browser& owner;
    };
    class Star final:public juce::Button
    {
    public:Star():Button("Favorite cabinet IR"){setComponentID("cabinet-ir-favorite");}
        void paintButton(juce::Graphics& g,bool hover,bool)override{juce::Path p;p.addStar(getLocalBounds().getCentre().toFloat(),5,4.5f,10);g.setColour(getToggleState()?juce::Colour(0xff62b2ce):hover?ui::ink():ui::secondaryInk());if(getToggleState())g.fillPath(p);else g.strokePath(p,juce::PathStrokeType(1.2f));}
    };
    class Row final:public juce::Button
    {
    public:Row(Browser& b,const Cell& c):Button("Cabinet"),owner(b){set(c);onClick=[this]{if(ignoreNextClick){ignoreNextClick=false;return;}dispatch(false);};addAndMakeVisible(star);star.onClick=[this]{const auto hash=entry.state.sha256;auto safe=juce::Component::SafePointer<Browser>(&owner);juce::MessageManager::callAsync([safe,hash]{if(safe)safe->toggle(hash);});};}
        void mouseDoubleClick(const juce::MouseEvent&)override{ignoreNextClick=true;dispatch(true);}
        void mouseUp(const juce::MouseEvent& event)override{juce::Button::mouseUp(event);ignoreNextClick=false;}
        bool matches(const Cell& c)const{return path==c.path&&isPack==c.pack;}
        void set(const Cell& c){path=c.path;entry=c.entry>=0?owner.entries[size_t(c.entry)]:Entry{};isPack=c.pack;count=c.count;
            if(isPack){entry.name=libraryui::leafOf(path);auto image=owner.folderImages.find(path);entry.image=image==owner.folderImages.end()?juce::Image{}:image->second;}
            setName(entry.name);setTitle(getName());setTooltip(isPack?path:getName()+"\n"+entry.creator);star.setVisible(!isPack);star.setToggleState(owner.favorites.contains(entry.state.sha256),juce::dontSendNotification);repaint();}
        void resized()override{star.setBounds(getWidth()-38,23,28,28);}
        void paintButton(juce::Graphics& g,bool hover,bool)override
        {
            using namespace juce;auto r=getLocalBounds().toFloat().reduced(1,2);const bool selected=!isPack&&owner.playing==entry.state.file;
            if(selected||hover){g.setColour(selected?Colour(0xff123756):Colour(0xff14232c));g.fillRoundedRectangle(r,6);}
            g.setColour(selected?Colour(0xff208fe5):ui::outline().withAlpha(.3f));if(selected)g.drawRoundedRectangle(r,6,1);else g.drawHorizontalLine(getHeight()-1,8.f,float(getWidth()-8));
            if(entry.image.isValid())g.drawImageWithin(entry.image,10,11,66,54,RectanglePlacement::centred);
            g.setColour(ui::ink());g.setFont(ui::font(16));g.drawText(entry.name,88,12,getWidth()-134,27,Justification::centredLeft,true);
            g.setColour(ui::metadataInk());g.setFont(ui::font(12));g.drawText(isPack?String(count)+tonegrid::l10n::translate(" IRs"):entry.category+(entry.creator.isEmpty()?String{}:tonegrid::l10n::translate(" | ")+entry.creator),88,40,getWidth()-134,22,Justification::centredLeft,true);
            if(isPack){g.drawLine(float(getWidth()-25),31,float(getWidth()-20),36,1.2f);g.drawLine(float(getWidth()-20),36,float(getWidth()-25),41,1.2f);}
        }
    private:
        void dispatch(bool close){const auto file=path;const bool folder=isPack;auto safe=juce::Component::SafePointer<Browser>(&owner);juce::MessageManager::callAsync([safe,file,folder,close]{if(safe)safe->activateFile(file,folder,close);});}
        Browser& owner;Entry entry;juce::String path;bool isPack=false,ignoreNextClick=false;int count=1;Star star;
    };
    using Crumb = libraryui::BreadcrumbButton;
    using Online = libraryui::OnlineCaptureButton;

public:
    Browser():list("Cabinet list",this),sidebarModel(*this),sidebar("Cabinet categories",&sidebarModel)
    {
        for(auto* c:std::initializer_list<juce::Component*>{&list,&sidebar,&search,&online,&breadcrumb,&count,&refresh,&openFolder,&availability,&info,&preview,&source,&ok})addAndMakeVisible(c);
        availability.setFont(ui::font(12));availability.setColour(juce::Label::textColourId,ui::secondaryInk());
        tonegrid::l10n::placeholder(search,"Search cabinet, speaker, microphone or creator...",ui::secondaryInk());search.setFont(ui::font(15));search.onTextChange=[this]{rebuild();};search.setComponentID("cabinet-ir-search");search.onEscapeKey=[this]{if(auto* dialog=findParentComponentOfClass<juce::DialogWindow>())dialog->closeButtonPressed();};
        for(auto* box:{&list,&sidebar}){box->setColour(juce::ListBox::backgroundColourId,juce::Colours::transparentBlack);box->setOutlineThickness(0);box->getViewport()->setScrollBarThickness(7);}
        list.setRowHeight(76);list.setComponentID("cabinet-ir-list");sidebar.setRowHeight(46);sidebar.setComponentID("cabinet-ir-categories");
        refresh.setComponentID("cabinet-ir-refresh");refresh.onClick=[this]{if(onRefresh)onRefresh();};openFolder.setComponentID("cabinet-ir-open-folder");openFolder.onClick=[this]{if(rootFolder.isDirectory())rootFolder.startAsProcess();};
        online.setComponentID("cabinet-ir-tone3000");tonegrid::l10n::tooltip(online,"Download cabinet impulse responses from TONE3000");online.onClick=[this]{if(onGetCaptures)onGetCaptures();};

        info.setJustificationType(juce::Justification::topLeft);info.setFont(ui::font(14));count.setFont(ui::font(12));count.setColour(juce::Label::textColourId,ui::metadataInk());
        tonegrid::l10n::button(source,"View on TONE3000");source.setFont(ui::font(13),false,juce::Justification::centredLeft);source.setColour(juce::HyperlinkButton::textColourId,juce::Colour(0xff64b7ed));setSize(1240,640);
    }
    ~Browser()override{list.setModel(nullptr);sidebar.setModel(nullptr);}
    void setSelectionEnabled(bool enabled,const juce::String& reason={}){if(enabled&&!selectionEnabled&&pendingSelection.isNotEmpty()&&playing!=pendingSelection){pendingSelection.clear();closeWhenLoaded.clear();}selectionEnabled=enabled;availability.setText(reason,juce::dontSendNotification);availability.setTooltip(reason);}
    bool canSelect()const{return selectionEnabled;}
    void setRefreshing(bool value){refresh.setEnabled(!value);tonegrid::l10n::button(refresh,value?"Refreshing...":"Refresh Library");}
    void setStatus(const juce::String& text){availability.setText(text,juce::dontSendNotification);availability.setTooltip(text);}
    void update(const Library& library,const State& active,juce::File file)
    {
        rootFolder=library.root;openFolder.setTooltip(rootFolder.getFullPathName());entries=library.entries;folders=library.folders;folderImages=library.folderImages;favoriteFile=std::move(file);favorites.clear();if(auto xml=juce::XmlDocument::parse(favoriteFile);xml&&xml->hasTagName("CabinetFavorites"))for(auto* node:xml->getChildIterator())favorites.insert(node->getStringAttribute("sha256"));if(currentFolder.isNotEmpty()&&!folders.contains(currentFolder)){currentFolder.clear();mode=all;}
        rebuildSidebar();rebuild();const auto* resolved=library.find(active);selectActive(resolved?resolved->state.file:active.file);
    }
    void showPack(const juce::String& pack){for(const auto& e:entries)if(e.pack==pack){navigate(libraryui::parentOf(e.state.file));return;}}
    void navigate(const juce::String& path){if(path.isNotEmpty()&&!folders.contains(path))return;mode=folderMode;currentFolder=path;search.setText({},false);rebuild();sidebar.repaint();}
    void showAll(){mode=all;currentFolder.clear();search.setText({},false);rebuild();sidebar.repaint();}
    void showFavorites(){mode=favoriteMode;currentFolder.clear();search.setText({},false);rebuild();sidebar.repaint();}
    juce::String folder()const{return currentFolder;}
    int visibleRows()const{return int(cells.size());}
    juce::String rowPath(int row)const{return juce::isPositiveAndBelow(row,int(cells.size()))?cells[size_t(row)].path:juce::String{};}
    int sidebarFolders()const{return juce::jmax(0,categories.size()-2);}
    void setSearch(const juce::String& query){search.setText(query,false);rebuild();}
    int packCardCount()const{return int(cells.size());}
    void selectActive(const juce::String& file){playing=file;if(pendingSelection==file)pendingSelection.clear();closeIfLoaded();for(const auto& entry:entries)if(entry.state.file==file){showDetails(entry);list.repaint();return;}preview.setImage({});tonegrid::l10n::text(info,"Choose a cabinet IR or open a folder.",juce::dontSendNotification);source.setVisible(false);list.repaint();}
    bool hasBreadcrumb()const{return !crumbs.empty();}
    int popupMinimumWidth() const override{return 760;}
    void resized()override
    {
        auto r=getLocalBounds().reduced(12);auto top=r.removeFromTop(40);online.setBounds(top.removeFromRight(getWidth()<900?136:280));top.removeFromRight(12);search.setBounds(top);r.removeFromTop(14);
        auto footer=r.removeFromBottom(44);ok.setBounds(footer.removeFromRight(132));footer.removeFromRight(12);refresh.setBounds(footer.removeFromLeft(getWidth()<1000?150:180));footer.removeFromLeft(10);openFolder.setBounds(footer.removeFromLeft(getWidth()<1000?125:155));footer.removeFromLeft(10);availability.setBounds(footer);r.removeFromBottom(14);
        left=r.removeFromLeft(getWidth()<1100?180:210);sidebar.setBounds(left.withTrimmedTop(46).reduced(12,8));r.removeFromLeft(12);right=r.removeFromRight(getWidth()<1100?230:280);r.removeFromRight(12);center=r;
        auto content=center.reduced(12);breadcrumb.setBounds(content.removeFromTop(34));count.setBounds(content.removeFromTop(25));content.removeFromTop(5);list.setBounds(content);layoutCrumbs();
        auto detail=right.reduced(14);preview.setBounds(detail.removeFromTop(198));detail.removeFromTop(12);source.setBounds(detail.removeFromBottom(28));info.setBounds(detail);repaint();
    }
    void paint(juce::Graphics& g)override { paintBrowserPanels(g,left,center,right); }
    std::function<void(const State&)> onSelect;std::function<void()> onRefresh,onGetCaptures;
private:
    void showDetails(const Entry& e){preview.setImage(e.image);tonegrid::l10n::text(info,e.name+"\n\n"+e.category+"\n"+e.creator+"\n\n"+e.pack,juce::dontSendNotification);source.setURL(juce::URL(e.source));source.setVisible(e.source.isNotEmpty());}
    void toggle(const juce::String& hash)
    {
        auto next=favorites;if(!next.erase(hash))next.insert(hash);
        juce::XmlElement xml("CabinetFavorites");for(const auto& h:next)xml.createNewChildElement("IR")->setAttribute("sha256",h);
        const auto saved=storage::writeXmlAtomically(favoriteFile,xml);
        if(saved.failed()){availability.setText(saved.getErrorMessage(),juce::dontSendNotification);return;}
        favorites=std::move(next);rebuild();sidebar.repaint();
    }
    int countFolder(const juce::String& path)const{int n=0;for(const auto& e:entries)if(e.state.file.startsWith(path+"/"))++n;return n;}
    void activateFile(const juce::String& file,bool folder,bool close=false){if(folder){navigate(file);auto image=folderImages.find(file);preview.setImage(image==folderImages.end()?juce::Image{}:image->second);tonegrid::l10n::text(info,libraryui::leafOf(file)+"\n\n"+juce::String(countFolder(file))+" IRs",juce::dontSendNotification);source.setVisible(false);list.getViewport()->setViewPosition(0,0);return;}for(const auto& e:entries)if(e.state.file==file){showDetails(e);if(!selectionEnabled&&pendingSelection!=file)return;if(close)closeWhenLoaded=file;else if(closeWhenLoaded!=file)closeWhenLoaded.clear();if(playing==file&&pendingSelection.isEmpty()){closeIfLoaded();return;}if(selectionEnabled&&onSelect){pendingSelection=file;const auto state=e.state;const auto callback=onSelect;callback(state);}return;}}
    void closeIfLoaded(){if(closeWhenLoaded.isEmpty()||playing!=closeWhenLoaded)return;const auto file=closeWhenLoaded;auto safe=juce::Component::SafePointer<Browser>(this);juce::MessageManager::callAsync([safe,file]{if(!safe||safe->closeWhenLoaded!=file||safe->playing!=file)return;safe->closeWhenLoaded.clear();if(auto* window=safe->findParentComponentOfClass<juce::DialogWindow>())window->closeButtonPressed();});}
    void rebuildSidebar()
    {
        juce::StringArray roots;for(const auto& path:folders)if(!path.containsChar('/')&&countFolder(path)>0)roots.add(path);
        roots.sort(true);categories={"",""};categories.addArray(roots);sidebar.updateContent();sidebar.repaint();
    }
    void layoutCrumbs(){int x=0;for(size_t i=0;i<crumbs.size();++i){const int remaining=juce::jmax(30,breadcrumb.getWidth()-x);const int width=juce::jmin(remaining,juce::jmax(65,juce::roundToInt(juce::GlyphArrangement::getStringWidth(ui::font(14),crumbs[i]->getButtonText()))+22));crumbs[i]->setBounds(x,1,width,30);x+=width;}}
    void updateBreadcrumb()
    {
        crumbs.clear();auto add=[&](const juce::String& title,const juce::String& path){auto button=std::make_unique<Crumb>(title);if(path=="@all")l10n::button(*button,title);tonegrid::l10n::tooltip(*button,path=="@all"?title:path);button->onClick=[this,path]{if(path=="@all")showAll();else navigate(path);};breadcrumb.addAndMakeVisible(*button);crumbs.push_back(std::move(button));};
        if(search.getText().trim().isNotEmpty())add("Search results","@all");else if(mode==all)add("All cabinets","@all");else if(mode==favoriteMode)add("Favorites","@all");else
        {add("Library","@all");juce::String path;for(const auto& part:juce::StringArray::fromTokens(currentFolder,"/",{})){path+=(path.isEmpty()?"":"/")+part;add(part,path);}}layoutCrumbs();
    }
    void rebuild()
    {
        std::vector<libraryui::NavigationItem> items;
        for(size_t i=0;i<entries.size();++i){const auto& e=entries[i];items.push_back({e.state.file,e.name,e.state.file+" "+e.name+" "+e.pack+" "+e.creator+" "+e.category+" "+e.mic,favorites.contains(e.state.sha256),int(i)});}
        const auto viewMode=mode==all?libraryui::BrowseMode::all:mode==favoriteMode?libraryui::BrowseMode::favorites:libraryui::BrowseMode::folder;
        cells=libraryui::visibleRows(folders,items,viewMode,currentFolder,search.getText());
        l10n::formattedText(count,"library.results",{{"count",juce::String(cells.size())}});updateBreadcrumb();list.updateContent();list.repaint();
    }
    int getNumRows()override{return int(cells.size());}
    void paintListBoxItem(int,juce::Graphics&,int,int,bool)override{}
    juce::Component* refreshComponentForRow(int row,bool,juce::Component* old)override{if(!juce::isPositiveAndBelow(row,getNumRows())){delete old;return nullptr;}const auto cell=cells[size_t(row)];if(auto* existing=dynamic_cast<Row*>(old);existing&&existing->matches(cell)){existing->set(cell);return existing;}delete old;return new Row(*this,cell);}
    void returnKeyPressed(int row)override{if(juce::isPositiveAndBelow(row,getNumRows())){const auto c=cells[size_t(row)];activateFile(c.path,c.pack);}}
    enum Mode{all,favoriteMode,folderMode};Mode mode=all;
    juce::StringArray categories{"",""};std::set<juce::String> folders;std::map<juce::String,juce::Image> folderImages;
    bool selectionEnabled=true;std::vector<Entry> entries;std::vector<Cell> cells;juce::String playing,currentFolder,pendingSelection,closeWhenLoaded;std::set<juce::String> favorites;juce::File favoriteFile,rootFolder;juce::Rectangle<int> left,center,right;
    libraryui::CloseLibraryButton ok{"cabinet-ir-ok"};
    juce::ListBox list;SidebarModel sidebarModel;juce::ListBox sidebar;juce::TextEditor search;Online online;juce::Component breadcrumb;std::vector<std::unique_ptr<juce::TextButton>> crumbs;tonegrid::l10n::TextButton refresh{"Refresh Library"},openFolder{"Open Folder"};juce::Label info,count,availability;juce::ImageComponent preview;juce::HyperlinkButton source;
};
}
