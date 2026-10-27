#pragma once
#include "Content/Amps/NamAmpLibrary.h"
#include "Shared/UI/PopupStyle.h"
#include "Shared/UI/LibraryWidgets.h"
#include <ToneGridApplicationAssets.h>
#include "Shared/UI/Library/LibraryBrowserShell.h"
#include "Shared/UI/Library/LibraryNavigation.h"

namespace tonegrid
{
inline bool openNamLibraryFolder(const juce::File& root){return root.isDirectory()&&root.startAsProcess();}
class NamLibraryBrowser final:public libraryui::LibraryBrowserShell,public juce::AsyncUpdater,private juce::ListBoxModel
{
public:
    using CaptureButton = libraryui::OnlineCaptureButton;
    // Value-based navigation entries, not TreeViewItems. Rows retain their own
    // path so a refresh/model commit cannot leave a dangling catalogue pointer.
    struct Item
    {
        NamLibraryBrowser& browser;juce::String label,path;bool folder=false;
        bool isSelected() const{return !folder&&browser.selectedPath==path;}
        bool isOpen() const{return folder&&(browser.currentFolder==path||browser.currentFolder.startsWith(path+"/"));}
        void setOpen(bool open){if(open&&folder)browser.navigate(path);}
        void setSelected(bool selected,bool=true,juce::NotificationType notify=juce::sendNotification)
        {if(selected){if(notify==juce::dontSendNotification)browser.reveal(path);else activate();}}
        void activate(){browser.activate(path,false);}
    };
    NamLibraryBrowser():list("Captures",nullptr),sidebarModel(*this),sidebar("Library",&sidebarModel),header(*this),details(*this)
    {
        list.setModel(this);
        for(auto* component:std::initializer_list<juce::Component*>{&sidebar,&list,&header,&detailsViewport,&refresh,&openFolder,&importStatus,&fullChoice,&headChoice,&breadcrumb,&itemCount,&empty,&ok})addAndMakeVisible(component);
        detailsViewport.setViewedComponent(&details,false);detailsViewport.setScrollBarThickness(7);detailsViewport.setScrollBarsShown(true,false);
        for(auto* box:{&sidebar,&list}){box->setColour(juce::ListBox::backgroundColourId,juce::Colours::transparentBlack);box->setOutlineThickness(0);box->setMultipleSelectionEnabled(false);box->getViewport()->setScrollBarThickness(7);}
        sidebar.setRowHeight(46);list.setRowHeight(76);list.setComponentID("nam-library-list");sidebar.setComponentID("nam-library-sidebar");
        fullChoice.setComponentID("nam-filter-full");headChoice.setComponentID("nam-filter-head");
        tonegrid::l10n::tooltip(fullChoice,"Show complete captures with their cabinet already included.");tonegrid::l10n::tooltip(headChoice,"Show amp heads that use a separate cabinet IR. Preamp-only and power-amp-only variants are labelled separately.");
        fullChoice.onClick=headChoice.onClick=[this]{rebuildRows();};
        itemCount.setFont(ui::font(12));itemCount.setColour(juce::Label::textColourId,ui::secondaryInk());
        empty.setJustificationType(juce::Justification::centred);empty.setInterceptsMouseClicks(false,false);
        refresh.setComponentID("nam-library-refresh");openFolder.setComponentID("nam-library-open-folder");refresh.onClick=[this]{if(onRefresh)onRefresh();};openFolder.onClick=[this]{if(openDirectory)openDirectory(directory);};
        captures.setComponentID("nam-library-tone3000");tonegrid::l10n::tooltip(captures,"Download A2 NAM Amp+Cab or Amp Head packs from Tone3000");captures.onClick=[this]{if(onGetCaptures)onGetCaptures();};
        importStatus.setFont(ui::font(12));importStatus.setColour(juce::Label::textColourId,ui::secondaryInk());
        openDirectory=openNamLibraryFolder;header.addAndMakeVisible(fullChoice);header.addAndMakeVisible(headChoice);setSize(1240,640);
    }
    ~NamLibraryBrowser() override{cancelPendingUpdate();list.setModel(nullptr);sidebar.setModel(nullptr);detailsViewport.setViewedComponent(nullptr,false);}
    void setFavoritesFile(const juce::File& file){favorites.load(file);rebuildSidebar();rebuildRows();}
    void update(const namamp::Catalog& catalog,const juce::File& rootDirectory,const juce::String& active)
    {
        cancelPendingUpdate();pendingSelection.clear();closeAfterSelection=false;directory=rootDirectory;activePath=active;favorites.followFactoryMoves(directory);openFolder.setTooltip(directory.getFullPathName());
        auto next=std::make_shared<libraryui::Index>();if(catalog.browser)*next=*catalog.browser;
        // Validated import entries can be merged before the next worker snapshot.
        for(const auto& entry:catalog.entries)if(!next->captures.contains(entry.identity.file))
        {libraryui::Capture c;c.path=entry.identity.file;c.name=libraryui::leafOf(c.path).upToLastOccurrenceOf(".",false,false);c.bytes=entry.identity.bytes;c.architecture=entry.architecture;c.version=entry.version;next->captures.emplace(c.path,c);next->includeParents(libraryui::parentOf(c.path));}
        index=std::move(next);items.clear();for(const auto& folder:index->folders)items.emplace(folder,std::make_unique<Item>(Item{*this,libraryui::leafOf(folder),folder,true}));
        for(const auto& [path,c]:index->captures)items.emplace(path,std::make_unique<Item>(Item{*this,libraryui::leafOf(path),path,false}));
        if(!index->captures.contains(selectedPath))selectedPath=index->captures.contains(active)?active:juce::String{};
        if(currentFolder.isNotEmpty()&&!index->folders.contains(currentFolder)){currentFolder.clear();mode=all;}
        rebuildSidebar();rebuildRows();updateDetails();closeIfLoaded();
    }
    Item* find(const juce::String& path) const{auto i=items.find(path);return i==items.end()?nullptr:i->second.get();}
    juce::String firstModelIn(const juce::String& folder) const
    {
        // Preserve the existing directory-first, case-insensitive import order.
        std::function<juce::String(const juce::String&)> first=[&](const juce::String& parent)->juce::String
        {
            std::vector<juce::String> dirs,files;for(const auto& [path,item]:items)if(libraryui::parentOf(path)==parent)(item->folder?dirs:files).push_back(path);
            auto order=[](const auto& a,const auto& b){const int c=a.compareIgnoreCase(b);return c?c<0:a<b;};std::sort(dirs.begin(),dirs.end(),order);std::sort(files.begin(),files.end(),order);
            for(const auto& dir:dirs){auto result=first(dir);if(result.isNotEmpty())return result;}return files.empty()?juce::String{}:files.front();
        };return first(folder);
    }
    bool reveal(const juce::String& relative)
    {
        if(!index->captures.contains(relative)){showAll();selectedPath.clear();updateDetails();return false;}
        selectedPath=relative;header.search.setText({},false);currentFolder=libraryui::parentOf(relative);mode=currentFolder.isEmpty()?all:folderMode;
        rebuildSidebar();rebuildRows();updateDetails();return true;
    }
    void selectActive(const juce::String& relative)
    {activePath=relative;selectedPath=index->captures.contains(relative)?relative:juce::String{};selectRow();list.repaint();updateDetails();closeIfLoaded();}
    void navigate(const juce::String& folder)
    {if(!folder.isEmpty()&&!index->folders.contains(folder))return;mode=folderMode;currentFolder=folder;header.search.setText({},false);rebuildSidebar();rebuildRows();}
    void showAll(){mode=all;currentFolder.clear();header.search.setText({},false);rebuildSidebar();rebuildRows();}
    void showFavorites(){mode=favoriteMode;currentFolder.clear();header.search.setText({},false);rebuildSidebar();rebuildRows();}
    bool isFavorite(const juce::String& path) const{return favorites.contains(directory.getChildFile(path));}
    void toggleFavorite(const juce::String& path)
    {if(!index->captures.contains(path))return;auto result=favorites.toggle(directory.getChildFile(path));if(result.failed())setImportStatus(result.getErrorMessage());rebuildSidebar();rebuildRows();updateDetails();}
    void activate(const juce::String& path,bool close)
    {
        auto* item=find(path);if(!item)return;if(item->folder){navigate(path);return;}
        if(close)closeWhenLoaded=path;else if(closeWhenLoaded!=path)closeWhenLoaded.clear();
        selectedPath=path;pendingSelection=path;closeAfterSelection=close;list.repaint();updateDetails();triggerAsyncUpdate();
    }
    void setSearch(const juce::String& query){header.search.setText(query,false);rebuildRows();}
    void setTypes(bool full,bool head){fullChoice.setToggleState(full,juce::dontSendNotification);headChoice.setToggleState(head,juce::dontSendNotification);rebuildRows();}
    int gridColumns()const{return columns;}
    juce::String selected() const{return selectedPath;}
    juce::String onlineGear()const{return fullChoice.getToggleState()&&headChoice.getToggleState()?"amp_amp-cab":headChoice.getToggleState()?"amp":"amp-cab";}
    juce::String folder() const{return currentFolder;}
    int totalCaptures() const{return int(index->captures.size());}
    int visibleRows() const{return int(rows.size());}
    juce::String rowPath(int row) const{return juce::isPositiveAndBelow(row,int(rows.size()))?rows[(size_t)row]:juce::String{};}
    const libraryui::Capture* capture(const juce::String& path) const{auto found=index->captures.find(path);return found==index->captures.end()?nullptr:&found->second;}
    juce::String typeFor(const juce::String& path)const
    {
        if(const auto* c=capture(path))return c->gear;
        juce::String type;for(const auto& [p,c]:index->captures)if(p.startsWith(path+"/"))
        {const auto next=cabinet::kind(c.gear);const auto value=next==cabinet::CaptureKind::fullRig?juce::String("amp-cab"):next==cabinet::CaptureKind::head?juce::String("amp"):c.gear;if(type.isEmpty())type=value;else if(type!=value)return "mixed";}return type;
    }
    juce::String cardTitle(const juce::String& path)const
    {if(const auto* c=capture(path))return c->name;if(path.startsWith("06 - Head Packs/"))for(const auto& [p,c]:index->captures)if(libraryui::parentOf(p)==path&&c.model.isNotEmpty())return c.model;return libraryui::leafOf(path);}
    juce::Image folderArtwork(const juce::String& path) const
    {auto found=index->folderArtwork.find(path);return found==index->folderArtwork.end()?juce::Image{}:found->second;}
    int rowFor(const juce::String& path) const{auto i=std::find(rows.begin(),rows.end(),path);return i==rows.end()?-1:int(i-rows.begin());}
    juce::Component* rowComponent(const juce::String& path);
    void setRefreshing(bool value){refresh.setEnabled(!value);tonegrid::l10n::button(refresh,value?"Refreshing...":"Refresh Library");tonegrid::l10n::text(empty,value?"Loading local NAM library...":"No matching captures or folders",juce::dontSendNotification);}
    void setImportStatus(const juce::String& text){importStatus.setText(text,juce::dontSendNotification);importStatus.setTooltip(text);resized();}
    juce::Component* popupHeaderAction() override{return &header;}
    int popupHeaderWidth(int available) const override{return juce::jmax(300,available-350);}
    bool popupStackHeader(int width) const override{return width<1040;}
    int popupHeaderHeight(int) const override{return 42;}
    int popupMinimumWidth() const override{return 760;}
    float popupCornerRadius() const override{return 18;}
    float popupBorderWidth() const override{return 1;}
    void paint(juce::Graphics& g) override { paintBrowserPanels(g,leftPanel,centerPanel,rightPanel); }
    void resized() override
    {
        auto r=getLocalBounds();if(header.getParentComponent()==this){header.setBounds(r.removeFromTop(44));r.removeFromTop(12);}
        auto footer=r.removeFromBottom(44);ok.setBounds(footer.removeFromRight(132));footer.removeFromRight(12);refresh.setBounds(footer.removeFromLeft(200));footer.removeFromLeft(12);openFolder.setBounds(footer.removeFromLeft(180));importStatus.setBounds(footer.withTrimmedLeft(16));r.removeFromBottom(16);
        const bool compact=getWidth()<1100;
        leftPanel=r.removeFromLeft(compact?156:210);r.removeFromLeft(12);rightPanel=r.removeFromRight(compact?230:300);r.removeFromRight(12);centerPanel=r;
        sidebar.setBounds(leftPanel.withTrimmedTop(46).reduced(12,8));auto content=centerPanel.reduced(16);breadcrumb.setBounds(content.removeFromTop(36));content.removeFromTop(8);
        itemCount.setBounds(content.removeFromTop(26));content.removeFromTop(7);list.setBounds(content);empty.setBounds(content);
        columns=1;list.setRowHeight(76);
        detailsViewport.setBounds(rightPanel.reduced(16));updateDetails();repaint();
    }
    std::function<void(const juce::String&)> onSelect;std::function<void(const juce::String&,const juce::String&)> onCaptureKind;std::function<void()> onRefresh,onGetCaptures,onClose;
    std::function<bool(const juce::File&)> openDirectory;
    std::function<void(const juce::URL&)> openLink=[](const juce::URL& url){url.launchInDefaultBrowser();};
    libraryui::CloseLibraryButton ok{"nam-library-ok"};tonegrid::l10n::TextButton refresh{"Refresh Library"},openFolder{"Open Folder"};CaptureButton captures;
    juce::ListBox list;
private:
    static void folderIcon(juce::Graphics& g,juce::Rectangle<float> r)
    {juce::Path p;p.startNewSubPath(1,19);p.lineTo(1,3);p.lineTo(9,3);p.lineTo(12,6);p.lineTo(23,6);p.lineTo(23,19);p.closeSubPath();p.startNewSubPath(1,8);p.lineTo(23,8);p.applyTransform(juce::AffineTransform::scale(r.getWidth()/24,r.getHeight()/22).translated(r.getX(),r.getY()));g.strokePath(p,juce::PathStrokeType(1.5f));}
    class Star final:public juce::Button
    {
    public:Star():Button("Favorite"){setComponentID("nam-library-favorite");setTooltip("Toggle favorite");}bool marked=false;
        void paintButton(juce::Graphics& g,bool hover,bool) override
        {juce::Path p;p.addStar(getLocalBounds().getCentre().toFloat(),5,5,11);g.setColour(marked?juce::Colour(0xff2098f4):hover?ui::ink():ui::secondaryInk());if(marked)g.fillPath(p);else g.strokePath(p,juce::PathStrokeType(1.4f));}
    };
    class Header final:public juce::Component
    {
    public:Header(NamLibraryBrowser& b):owner(b){addAndMakeVisible(search);addAndMakeVisible(owner.captures);tonegrid::l10n::placeholder(search,"Search captures, amps, or folders...",ui::secondaryInk());search.setFont(ui::font(16));search.setIndents(38,10);search.setColour(juce::TextEditor::backgroundColourId,juce::Colours::transparentBlack);search.setColour(juce::TextEditor::outlineColourId,juce::Colours::transparentBlack);search.setColour(juce::TextEditor::focusedOutlineColourId,juce::Colours::transparentBlack);search.setComponentID("nam-library-search");search.onTextChange=[this]{owner.rebuildRows();};search.onEscapeKey=[this]{if(auto* dialog=findParentComponentOfClass<juce::DialogWindow>())dialog->closeButtonPressed();};}
        void paint(juce::Graphics& g) override
        {auto r=search.getBounds().toFloat().reduced(.5f);g.setColour(ui::panel());g.fillRoundedRectangle(r,7);g.setColour(search.hasKeyboardFocus(true)?juce::Colour(0xff208fe5):ui::outline());g.drawRoundedRectangle(r,7,1);g.setColour(ui::secondaryInk());const float x=r.getX()+19,y=r.getCentreY()-1;g.drawEllipse(x-7,y-7,14,14,1.4f);g.drawLine(x+5,y+5,x+10,y+10,1.4f);}
        void resized() override
        {
            auto r=getLocalBounds();
            owner.captures.setBounds(r.removeFromRight(getWidth()<850?136:280));r.removeFromRight(8);
            owner.headChoice.setBounds(r.removeFromRight(getWidth()<850?126:150));r.removeFromRight(8);
            owner.fullChoice.setBounds(r.removeFromRight(getWidth()<850?126:150));r.removeFromRight(8);search.setBounds(r);
            repaint();
        }
        juce::TextEditor search;
    private:NamLibraryBrowser& owner;
    };
    class Link final:public juce::HyperlinkButton
    {
    public:
        explicit Link(NamLibraryBrowser& b):owner(b)
        {
            setFont(ui::font(14),false,juce::Justification::centredLeft);
            setColour(juce::HyperlinkButton::textColourId,juce::Colour(0xff64b7ed));
            onClick=[this]{if(target.isNotEmpty()&&owner.openLink)owner.openLink(juce::URL(target));};
        }
        void show(const juce::String& title,const juce::String& address)
        {target=address;setButtonText(title);setTooltip(address);setVisible(address.isNotEmpty());}
    private:NamLibraryBrowser& owner;juce::String target;
    };
    class Row final:public juce::Component,public juce::SettableTooltipClient
    {
    public:Row(NamLibraryBrowser& b,juce::String p):owner(b),path(std::move(p))
        {
            setComponentID("nam-row:"+path);thumbnail.setComponentID("nam-library-thumbnail");thumbnail.setInterceptsMouseClicks(false,false);addAndMakeVisible(thumbnail);addAndMakeVisible(star);setWantsKeyboardFocus(true);
            star.onClick=[safe=juce::Component::SafePointer<NamLibraryBrowser>(&owner),p=path]{if(safe)safe->toggleFavorite(p);};
            update();
        }
        const juce::String& identity()const{return path;}
        void update(){
            const auto* c=owner.capture(path);thumbnail.setImage(c?c->artwork:owner.folderArtwork(path));star.setVisible(c!=nullptr);star.marked=c&&owner.isFavorite(path);star.setToggleState(star.marked,juce::dontSendNotification);
            setTooltip(c?c->name+"\n"+cabinet::label(cabinet::kind(c->gear)):libraryui::leafOf(path)+"\nOpen all captures in this pack");
        }
        void resized()override{thumbnail.setBounds(10,11,66,54);star.setBounds(getWidth()-38,22,28,28);}
        void paint(juce::Graphics& g)override
        {
            using namespace juce;auto r=getLocalBounds().toFloat().reduced(1,2);const bool selected=owner.selectedPath==path;
            if(selected){g.setColour(Colour(0xff123756));g.fillRoundedRectangle(r,6);g.setColour(Colour(0xff208fe5));g.drawRoundedRectangle(r,6,1);}
            else if(isMouseOver(true)){g.setColour(Colour(0xff14232c));g.fillRoundedRectangle(r,6);}
            else {g.setColour(ui::outline().withAlpha(.3f));g.drawHorizontalLine(getHeight()-1,8.f,float(getWidth()-8));}
            const auto* c=owner.capture(path);const int typeWidth=getWidth()<450?112:136;const int typeX=getWidth()-typeWidth-43;
            g.setColour(ui::ink());g.setFont(ui::font(16));g.drawText(owner.cardTitle(path),88,13,juce::jmax(40,typeX-98),25,Justification::centredLeft,true);
            g.setColour(ui::metadataInk());g.setFont(ui::font(12));g.drawText(c?libraryui::captureContext(*c):String(owner.index->count(path))+tonegrid::l10n::translate(" captures"),88,39,juce::jmax(40,typeX-98),22,Justification::centredLeft,true);
            const auto type=owner.typeFor(path);const auto label=type=="amp-cab"?String("Head + Cab"):type=="amp"?String("Head only"):type=="mixed"?String("Mixed"):cabinet::label(cabinet::kind(type));
            libraryui::gearIcon(g,{float(typeX),24,23,27},type=="amp-cab",ui::secondaryInk());g.setColour(ui::secondaryInk());g.setFont(ui::font(12));g.drawText(label,typeX+31,0,typeWidth-28,getHeight(),Justification::centredLeft,true);
            if(!c){g.setColour(ui::secondaryInk());g.drawLine(float(getWidth()-24),32,float(getWidth()-19),37,1.2f);g.drawLine(float(getWidth()-19),37,float(getWidth()-24),42,1.2f);}
        }
        void mouseEnter(const juce::MouseEvent&)override{repaint();}void mouseExit(const juce::MouseEvent&)override{repaint();}
        void mouseDown(const juce::MouseEvent&)override{grabKeyboardFocus();owner.activate(path,false);}
        void mouseDoubleClick(const juce::MouseEvent&)override{owner.activate(path,true);}
        bool keyPressed(const juce::KeyPress& key)override
        {int delta=key==juce::KeyPress::rightKey?1:key==juce::KeyPress::leftKey?-1:key==juce::KeyPress::downKey?owner.columns:key==juce::KeyPress::upKey?-owner.columns:0;if(delta){const auto next=owner.rowFor(path)+delta;if(juce::isPositiveAndBelow(next,int(owner.rows.size())))owner.activate(owner.rowPath(next),false);return true;}if(key==juce::KeyPress::returnKey){owner.activate(path,true);return true;}return false;}
    private:NamLibraryBrowser& owner;juce::String path;ampui::Thumbnail thumbnail;Star star;
    };
    class GridRow final:public juce::Component
    {
    public:GridRow(NamLibraryBrowser& b,int row):owner(b){for(int c=0;c<owner.columns;++c){const auto p=owner.rowPath(row*owner.columns+c);if(p.isEmpty())break;auto tile=std::make_unique<Row>(owner,p);addAndMakeVisible(*tile);tiles.push_back(std::move(tile));}}
        void resized()override{const auto width=getWidth()/owner.columns;for(size_t i=0;i<tiles.size();++i)tiles[i]->setBounds(int(i)*width,0,width,getHeight());}
        juce::Component* find(const juce::String& path){for(auto& tile:tiles)if(tile->identity()==path)return tile.get();return nullptr;}
        bool update(int row){if(int(tiles.size())!=juce::jmin(owner.columns,int(owner.rows.size())-row*owner.columns))return false;for(size_t i=0;i<tiles.size();++i)if(tiles[i]->identity()!=owner.rowPath(row*owner.columns+int(i)))return false;for(auto& tile:tiles)tile->update();repaint();return true;}
    private:NamLibraryBrowser& owner;std::vector<std::unique_ptr<Row>> tiles;
    };
    class SidebarModel final:public juce::ListBoxModel
    {
    public:explicit SidebarModel(NamLibraryBrowser& b):owner(b){}int getNumRows() override{return int(owner.categories.size());}
        void paintListBoxItem(int row,juce::Graphics& g,int w,int h,bool) override
        {
            using namespace juce;const auto& path=owner.categories[(size_t)row];bool selected=(row==0&&owner.mode==all)||(row==1&&owner.mode==favoriteMode)||(row>=2&&owner.mode==folderMode&&(owner.currentFolder==path||owner.currentFolder.startsWith(path+"/")));
            const int count=row==0?owner.totalCaptures():row==1?owner.favoriteCount():owner.index->count(path);
            libraryui::sidebarRow(g,w,h,row==0?l10n::translate("All Captures"):row==1?l10n::translate("Favorites"):path,count,selected,row==1);
        }
        void listBoxItemClicked(int row,const juce::MouseEvent&) override{if(row==0)owner.showAll();else if(row==1)owner.showFavorites();else if(juce::isPositiveAndBelow(row,int(owner.categories.size())))owner.navigate(owner.categories[(size_t)row]);}
    private:NamLibraryBrowser& owner;
    };
    class Details final:public juce::Component
    {
    public:explicit Details(NamLibraryBrowser& b):owner(b),author(b),more(b)
        {
            addAndMakeVisible(preview);addAndMakeVisible(star);addChildComponent(author);addChildComponent(more);
            setComponentID("nam-library-details");author.setComponentID("nam-library-author");more.setComponentID("nam-library-more-captures");
            star.onClick=[this]{owner.toggleFavorite(owner.selectedPath);};
        }
        void update()
        {
            blocks.clear();auto* c=owner.capture(owner.selectedPath);preview.setVisible(c!=nullptr);star.setVisible(c!=nullptr);
            author.show(c?"by "+c->author:juce::String{},c?libraryui::authorSearch(*c):juce::String{});
            more.show("More captures of this amp",c?libraryui::tonePage(c->sourceUrl):juce::String{});
            if(!c){setSize(juce::jmax(1,getWidth()),400);repaint();return;}
            preview.setImage(c->artwork);star.marked=owner.isFavorite(c->path);preview.setBounds(4,8,getWidth()-8,198);star.setBounds(getWidth()-36,10,32,32);
            int y=218;
            auto add=[&](const juce::String& value,float size,juce::Colour colour,int x,int width)
            {
                juce::AttributedString text;text.append(value,ui::font(size),colour);text.setWordWrap(juce::AttributedString::byWord);
                Block block;block.layout.createLayout(text,float(juce::jmax(40,width)));
                const int height=juce::jmax(22,int(std::ceil(block.layout.getHeight())));
                block.bounds={float(x),float(y),float(width),float(height)};blocks.push_back(std::move(block));return height;
            };
            y+=add(c->name,20,ui::ink(),4,getWidth()-8)+10;
            author.setBounds(4,y,getWidth()-8,24);if(author.isVisible())y+=34;
            divider=y;y+=16;
            auto field=[&](const juce::String& label,const juce::String& value)
            {
                if(value.isEmpty())return;
                const int labelHeight=add(label,14,ui::metadataInk(),4,70);
                const int valueHeight=add(value,14,ui::ink(),80,getWidth()-84);
                y+=juce::jmax(labelHeight,valueHeight)+12;
            };
            field("Type",cabinet::label(cabinet::kind(c->gear)));field("Maker",c->make);field("Model",c->model);field("Category",libraryui::categoryOf(*c));
            more.setBounds(4,y,getWidth()-8,28);if(more.isVisible())y+=44;
            const auto description=libraryui::plainDescription(c->description);
            if(description.isNotEmpty())
            {
                y+=add("Description",14,ui::metadataInk(),4,getWidth()-8)+8;
                y+=add(description,14,ui::ink(),4,getWidth()-8)+16;
            }
            setSize(juce::jmax(1,getWidth()),y+8);repaint();
        }
        void paint(juce::Graphics& g) override
        {
            using namespace juce;if(!owner.capture(owner.selectedPath)){g.setColour(ui::secondaryInk());g.setFont(ui::font(16));g.drawFittedText(tonegrid::l10n::translate("Select a capture to see its details"),getLocalBounds().reduced(12),Justification::centred,3);return;}
            g.setColour(ui::outline().withAlpha(.5f));g.drawHorizontalLine(divider,4.f,float(getWidth()-4));
            for(const auto& block:blocks)block.layout.draw(g,block.bounds);
        }
    private:
        struct Block{juce::TextLayout layout;juce::Rectangle<float> bounds;};
        NamLibraryBrowser& owner;ampui::Thumbnail preview;Star star;Link author,more;int divider=0;std::vector<Block> blocks;
    };
    int getNumRows() override{return (int(rows.size())+columns-1)/columns;}
    void paintListBoxItem(int,juce::Graphics&,int,int,bool) override{}
    juce::Component* refreshComponentForRow(int row,bool,juce::Component* old)override
    {if(juce::isPositiveAndBelow(row,getNumRows())){if(auto* group=dynamic_cast<GridRow*>(old);group&&group->update(row))return group;delete old;return new GridRow(*this,row);}delete old;return nullptr;}
    void returnKeyPressed(int row)override{activate(rowPath(row*columns),true);}
    void listBoxItemClicked(int row,const juce::MouseEvent& event)override{activate(rowPath(row*columns+juce::jlimit(0,columns-1,event.x/juce::jmax(1,list.getWidth()/columns))),false);}
    void handleAsyncUpdate() override
    {
        const auto path=pendingSelection;const bool close=closeAfterSelection;pendingSelection.clear();closeAfterSelection=false;
        if(!capture(path))return;auto safe=juce::Component::SafePointer<NamLibraryBrowser>(this);const auto callback=onSelect;if(callback)callback(path);
        if(safe&&close){safe->closeWhenLoaded=path;safe->closeIfLoaded();}
    }
    void closeIfLoaded()
    {
        if(closeWhenLoaded.isEmpty()||activePath!=closeWhenLoaded)return;
        const auto path=closeWhenLoaded;auto safe=juce::Component::SafePointer<NamLibraryBrowser>(this);
        juce::MessageManager::callAsync([safe,path]{if(!safe||safe->closeWhenLoaded!=path||safe->activePath!=path)return;safe->closeWhenLoaded.clear();const auto callback=safe->onClose;if(callback)callback();else if(auto* window=safe->findParentComponentOfClass<juce::DialogWindow>())window->closeButtonPressed();});
    }
    int favoriteCount() const{int n=0;for(const auto& [p,c]:index->captures)if(isFavorite(p))++n;return n;}
    void rebuildSidebar()
    {categories={"",""};for(const auto& p:index->folders)if(!p.containsChar('/')&&index->count(p)>0)categories.push_back(p);std::sort(categories.begin()+2,categories.end(),[](const auto& a,const auto& b){const auto c=a.compareIgnoreCase(b);return c?c<0:a<b;});sidebar.updateContent();sidebar.repaint();}
    void rebuildRows()
    {
        std::vector<libraryui::NavigationItem> items;
        for(const auto& [path,c]:index->captures)items.push_back({path,cardTitle(path),c.searchable(),isFavorite(path),int(items.size())});
        const auto viewMode=mode==all?libraryui::BrowseMode::all:mode==favoriteMode?libraryui::BrowseMode::favorites:libraryui::BrowseMode::folder;
        rows.clear();for(const auto& row:libraryui::visibleRows(index->folders,items,viewMode,currentFolder,header.search.getText()))rows.push_back(row.path);
        const auto accepts=[&](const libraryui::Capture& c){const auto type=cabinet::kind(c.gear);return (fullChoice.getToggleState()&&headChoice.getToggleState())||(fullChoice.getToggleState()&&type==cabinet::CaptureKind::fullRig)||(headChoice.getToggleState()&&cabinet::needsIR(type));};
        std::erase_if(rows,[&](const auto& path){if(const auto* c=capture(path))return !accepts(*c);for(const auto& [p,c]:index->captures)if(p.startsWith(path+"/")&&accepts(c))return false;return true;});
        const bool descending=false;
        std::sort(rows.begin(),rows.end(),[&](const auto& a,const auto& b){const auto* ca=capture(a);const auto* cb=capture(b);if(bool(ca)!=bool(cb))return !ca;const auto na=cardTitle(a),nb=cardTitle(b);const int cmp=na.compareIgnoreCase(nb);return cmp?(descending?cmp>0:cmp<0):a<b;});
        list.updateContent();selectRow();empty.setVisible(rows.empty());l10n::formattedText(itemCount,"library.results",{{"count",juce::String(rows.size())}});updateBreadcrumb();list.repaint();
    }
    void selectRow(){const int index=rowFor(selectedPath),row=index<0?-1:index/columns;list.selectRow(row,false,false);if(row>=0)list.scrollToEnsureRowIsOnscreen(row);}
    using Crumb = libraryui::BreadcrumbButton;
    void updateBreadcrumb()
    {
        crumbs.clear();auto add=[&](const juce::String& text,const juce::String& path){auto button=std::make_unique<Crumb>(text);if(path=="@all")l10n::button(*button,text);tonegrid::l10n::tooltip(*button,path=="@all"?text:path);button->onClick=[this,path]{if(path=="@all")showAll();else navigate(path);};breadcrumb.addAndMakeVisible(*button);crumbs.push_back(std::move(button));};
        if(header.search.getText().trim().isNotEmpty())add("Search results","@all");else if(mode==all)add("All Captures","@all");else if(mode==favoriteMode)add("Favorites","@all");else
        {add("Library","@all");juce::String path;for(const auto& part:juce::StringArray::fromTokens(currentFolder,"/",{})){path+=(path.isEmpty()?"":"/")+part;add(part,path);}}
        int x=0;const int maxWidth=juce::jmax(40,breadcrumb.getWidth()/juce::jmax(1,int(crumbs.size())));for(auto& crumb:crumbs){const int width=juce::jmin(maxWidth,juce::jmax(56,juce::roundToInt(juce::GlyphArrangement::getStringWidth(ui::font(14),crumb->getButtonText()))+20));crumb->setBounds(x,2,width-4,30);x+=width;}
    }
    void updateDetails(){details.setSize(juce::jmax(1,detailsViewport.getWidth()-detailsViewport.getScrollBarThickness()),details.getHeight());details.update();detailsViewport.setViewPosition(0,0);}
    enum Mode{all,favoriteMode,folderMode};Mode mode=all;
    std::shared_ptr<libraryui::Index> index=std::make_shared<libraryui::Index>();
    std::map<juce::String,std::unique_ptr<Item>> items;std::vector<juce::String> categories,rows;
    juce::File directory;libraryui::Favorites favorites;juce::String activePath,selectedPath,currentFolder,pendingSelection,closeWhenLoaded;bool closeAfterSelection=false;
    libraryui::TypeChoice fullChoice{"Head + Cab",true},headChoice{"Head Only",false};
    SidebarModel sidebarModel;juce::ListBox sidebar;Header header;Details details;juce::Viewport detailsViewport;
    juce::Label importStatus,itemCount,empty;int columns=1;juce::Component breadcrumb;std::vector<std::unique_ptr<juce::TextButton>> crumbs;
    juce::Rectangle<int> leftPanel,centerPanel,rightPanel;
};
inline juce::Component* NamLibraryBrowser::rowComponent(const juce::String& path)
{const auto index=rowFor(path);if(index<0)return nullptr;if(auto* group=dynamic_cast<GridRow*>(list.getComponentForRowNumber(index/columns)))return group->find(path);return nullptr;}

}
