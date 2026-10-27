#pragma once
#include "Modules/Jam/Model/JamCatalogue.h"
#include "Shared/Media/VideoPlayer.h"
#include "Shared/UI/PopupStyle.h"
#include "Shared/UI/UiMotion138.h"
#include <ToneGridApplicationAssets.h>
#include "Content/EditableContent.h"
namespace tonegrid::jam
{
using media::Player;
using media::playerHeight;
using media::playerWidth;
using media::accordionHeight;
inline juce::Colour blue(){return ui::tabBlue();}
class PlayButton final:public juce::TextButton
{
public:
    PlayButton():TextButton("Play"){}
    void paintButton(juce::Graphics& g,bool hover,bool down)override
    {
        using namespace juce;auto r=getLocalBounds().toFloat().reduced(.5f);const auto fill=ui::field().brighter(hover?.13f:0.f).darker(down?.12f:0.f);
        g.setGradientFill(ColourGradient(fill.brighter(.08f),r.getTopLeft(),fill.darker(.12f),r.getBottomLeft(),false));g.fillRoundedRectangle(r,14);
        g.setColour(hasKeyboardFocus(true)?blue():ui::outline());g.drawRoundedRectangle(r,14,1);
        g.setColour(getToggleState()?blue():ui::secondaryInk());const float x=r.getX()+18,y=r.getCentreY();
        if(getToggleState())g.fillRoundedRectangle(x-4,y-4,8,8,1);
        else{Path p;p.startNewSubPath(x-4,y-5);p.lineTo(x+4,y);p.lineTo(x-4,y+5);p.closeSubPath();g.strokePath(p,PathStrokeType(1.1f,PathStrokeType::curved,PathStrokeType::rounded));}
        g.setColour(ui::ink());g.setFont(ui::font(13));g.drawText(getButtonText(),getLocalBounds().withTrimmedLeft(29).withTrimmedRight(10),Justification::centred);
    }
};
class Tab final:public juce::Button
{
public:
    explicit Tab(const juce::String& text):Button(text){setClickingTogglesState(true);setWantsKeyboardFocus(true);}
    void animate(double seconds)
    {
        const bool active=getToggleState();if(active&&!wasActive)activatedAt=seconds;wasActive=active;
        const auto value=active?motion::tabAccent(seconds-activatedAt):0.f;
        if(value!=accent){accent=value;repaint();}
    }
    void paintButton(juce::Graphics& g,bool hover,bool)override
    {
        g.setColour(getToggleState()?ui::ink().interpolatedWith(juce::Colour(0xffa5aaae),accent):hover?ui::ink():ui::secondaryInk());g.setFont(ui::font(13));g.drawText(getName(),getLocalBounds().reduced(2),juce::Justification::centred);
        if(getToggleState()){g.setColour(blue());g.fillRect(5,getHeight()-3,getWidth()-10,2);}
    }
private:
    float accent=0.f;double activatedAt=0;bool wasActive=false;
};
class Fretboard final:public juce::Component
{
public:
    int root=9,scale=0;bool intervals=false;
    void paint(juce::Graphics& g)override
    {
        using namespace juce;const auto& definition=scales()[size_t(scale)];const auto area=getLocalBounds().toFloat().withTrimmedLeft(40).withTrimmedRight(14).withTrimmedTop(13).withTrimmedBottom(28);
        const float cell=area.getWidth()/23.f,step=area.getHeight()/5.f;
        g.setGradientFill(ColourGradient(Colour(0xff242a2e),area.getX(),area.getY(),Colour(0xff121a20),area.getRight(),area.getBottom(),false));g.fillRoundedRectangle(area.expanded(0,8),5);
        g.setColour(Colour(0xff47515a));for(int fret=1;fret<=23;++fret)g.drawVerticalLine(roundToInt(area.getX()+(fret-.5f)*cell),area.getY()-8,area.getBottom()+8);
        g.setColour(ui::secondaryInk());g.fillRect(area.getX()+cell*.5f-1,area.getY()-8,2.f,area.getHeight()+16);
        const StringArray strings{"e","B","G","D","A","E"};
        for(int string=0;string<6;++string)
        {
            const float y=area.getY()+step*string;g.setColour(Colour(0xffa0aab0).withAlpha(.65f));g.drawLine(area.getX(),y,area.getRight(),y,.6f+string*.17f);
            g.setColour(ui::secondaryInk());g.setFont(ui::font(12));g.drawText(strings[string],0,roundToInt(y)-9,23,18,Justification::centred);
        }
        for(int fret:{3,5,7,9,12,15,17,19,21})
        {g.setColour(Colour(0xff64717a));const float x=area.getX()+cell*fret;for(float y:fret==12?std::vector<float>{area.getY()+step*1.5f,area.getY()+step*3.5f}:std::vector<float>{area.getCentreY()})g.fillEllipse(x-2,y-2,4,4);}
        for(int fret=0;fret<=22;++fret)
        {
            const float x=area.getX()+cell*fret;g.setColour(ui::metadataInk());g.setFont(ui::font(11));g.drawText(String(fret),roundToInt(x)-15,getHeight()-21,30,19,Justification::centred);
            for(int string=0;string<6;++string)
            {
                const int interval=pitch(openMidi[size_t(string)]+fret-root);const auto degree=degreeAt(definition,interval);if(degree.isEmpty())continue;
                const float y=area.getY()+step*string;const float diameter=jmin(24.f,cell-5,step-3);juce::Rectangle<float> circle(diameter,diameter);circle=circle.withCentre({x,y});
                g.setColour(interval==0?blue():Colour(0xffb3c4cf));g.fillEllipse(circle);g.setColour(interval==0?Colours::white:Colour(0xff101820));g.setFont(ui::font(degree.length()>2?10.f:12.f));
                g.drawText(intervals?degree:noteName(root,interval,degree),circle.toNearestInt(),Justification::centred);
            }
        }
    }
};
class View final:public juce::Component
{
    class Body final:public juce::Component,public juce::ListBoxModel,private juce::Timer
    {
        class Row final:public juce::Component,public juce::SettableTooltipClient
        {
        public:
            explicit Row(Body& value):owner(value){addAndMakeVisible(open);open.setComponentID("jam-play-track");tonegrid::l10n::tooltip(open,"Play this backing track inside ToneGrid. Click Stop to collapse it.");open.onClick=[this]{owner.requestTrack(track);};}
            Track track;bool selected=false,spacer=false;
            void resized()override{open.setBounds(getWidth()-100,(getHeight()-28)/2,88,28);}
            void refreshAction(){const bool playing=owner.activeId==track.id&&owner.player.isRequested();open.setVisible(!spacer);open.setToggleState(playing,juce::dontSendNotification);tonegrid::l10n::button(open,playing?"Stop":"Play");}
            void mouseDown(const juce::MouseEvent&)override{if(spacer)return;owner.selectTrack(track);for(size_t i=0;i<owner.filtered.size();++i)if(owner.owner.tracks[size_t(owner.filtered[i])].id==track.id){owner.list.selectRow(int(i)+(owner.expandedPosition()>=0&&int(i)>owner.expandedPosition()?4:0));break;}}
            void mouseDoubleClick(const juce::MouseEvent&)override{if(!spacer)owner.requestTrack(track);}
            void paint(juce::Graphics& g)override
            {
                using namespace juce;auto r=getLocalBounds();if(spacer){g.setColour(juce::Colour(0xff0d2130));g.fillRect(r);return;}g.setColour(selected?Colour(0xff12354e):ui::panel());g.fillRoundedRectangle(r.toFloat().reduced(1),5);g.setColour(ui::separator().withAlpha(.3f));g.drawHorizontalLine(getHeight()-1,6.f,float(getWidth()-6));
                int right=open.getX()-14;const auto tag=[&](const String& text)
                {
                    if(text.isEmpty())return;const int width=jlimit(42,104,ui::font(12).getStringWidth(text)+16);const juce::Rectangle<float> bounds(float(right-width),float((getHeight()-23)/2),float(width),23.f);
                    g.setColour(Colour(0xff202c33));g.fillRoundedRectangle(bounds,5);g.setColour(Colour(0xff36454e));g.drawRoundedRectangle(bounds.reduced(.5f),5,1);g.setColour(ui::secondaryInk());g.setFont(ui::font(12));g.drawText(text,bounds.toNearestInt().reduced(7,0),Justification::centred);right-=width+10;
                };
                tag(String(track.bpm,track.bpm==std::floor(track.bpm)?0:1)+" BPM");tag((track.key+" "+track.mode).trim());tag(track.style);
                auto left=r.reduced(12,5).withRight(right);
                g.setColour(ui::ink());g.setFont(ui::font(14));g.drawText(track.title,left.removeFromTop(25),Justification::centredLeft,true);g.setColour(ui::metadataInk());g.setFont(ui::font(12));g.drawText(track.creator,left,Justification::centredLeft,true);
            }
        private:Body& owner;PlayButton open;
        };
    public:
        explicit Body(View& value):owner(value)
        {
            using namespace juce;for(auto* box:{&scale,&key,&style,&trackKey})addAndMakeVisible(box);
            String group;for(size_t i=0;i<scales().size();++i){const auto& s=scales()[i];if(group!=s.group){scale.addSectionHeading(s.group);group=s.group;}scale.addItem(s.name,int(i)+1);}
            scale.setSelectedId(1,dontSendNotification);scale.setComponentID("jam-scale");scale.setTitle("Scale");
            key.addItemList(keyNames(),1);key.setSelectedId(10,dontSendNotification);key.setComponentID("jam-key");key.setTitle("Key");
            tonegrid::l10n::item(trackKey,"All keys",1);trackKey.addItemList(keyNames(),2);trackKey.setSelectedId(1,dontSendNotification);trackKey.setTitle("Backing track key");trackKey.setComponentID("jam-track-key");
            scale.onChange=key.onChange=[this]{updateScale();};style.onChange=trackKey.onChange=[this]{filter();};style.setComponentID("jam-style");style.setTitle("Backing track style");
            for(auto* editor:{&minimum,&maximum}){addAndMakeVisible(editor);editor->setInputRestrictions(5,"0123456789.");editor->setJustification(Justification::centred);editor->onTextChange=[this]{filter();};}
            minimum.setComponentID("jam-min-bpm");maximum.setComponentID("jam-max-bpm");minimum.setTitle("Minimum BPM");maximum.setTitle("Maximum BPM");minimum.setText("20",false);maximum.setText("400",false);
            addAndMakeVisible(clear);clear.setComponentID("jam-clear");clear.onClick=[this]{style.setSelectedId(1,dontSendNotification);trackKey.setSelectedId(1,dontSendNotification);minimum.setText("20",false);maximum.setText("400",false);filter();};
            addAndMakeVisible(list);list.setModel(this);list.setRowHeight(58);list.setComponentID("jam-tracks");
            addChildComponent(empty);tonegrid::l10n::text(empty,"No tracks match. Try All styles, All keys or a wider BPM range.",dontSendNotification);empty.setJustificationType(Justification::centred);empty.setFont(ui::font(14));empty.setInterceptsMouseClicks(false,false);
            addAndMakeVisible(fretViewport);fretViewport.setViewedComponent(&fretboard,false);fretViewport.setScrollBarsShown(false,true);fretViewport.setScrollBarThickness(10);
            addChildComponent(player);addChildComponent(playerInfo);playerInfo.setFont(ui::font(14));playerInfo.setJustificationType(Justification::topLeft);playerInfo.setInterceptsMouseClicks(false,false);
            addChildComponent(external);external.setComponentID("jam-open-youtube");external.onClick=[this]{for(const auto& t:owner.tracks)if(t.id==activeId){owner.launch(t.url);break;}};
            addChildComponent(closeInline);closeInline.setComponentID("jam-close-player");closeInline.onClick=[this]{closePlayer();};
            player.openExternal=[this](const String& address){owner.launch(address);};
            updateScale();startTimerHz(15);
        }
        ~Body()override{stopTimer();player.stop();list.setModel(nullptr);fretViewport.setViewedComponent(nullptr,false);}
        int expandedPosition()const{for(size_t i=0;i<filtered.size();++i)if(owner.tracks[size_t(filtered[i])].id==activeId)return int(i);return -1;}
        int trackPosition(int row)const{const int expanded=expandedPosition();if(expanded>=0&&row>expanded&&row<=expanded+4)return -1;return row-(expanded>=0&&row>expanded+4?4:0);}
        int getNumRows()override{return int(filtered.size())+(expandedPosition()>=0?4:0);}
        void listWasScrolled()override{layoutPlayer();}
        void paintListBoxItem(int,juce::Graphics&,int,int,bool)override{}
        juce::Component* refreshComponentForRow(int row,bool selected,juce::Component* current)override
        {
            if(row<0||row>=getNumRows())return current;auto* view=dynamic_cast<Row*>(current);if(!view)view=new Row(*this);const int position=trackPosition(row);view->spacer=position<0;if(position>=0&&position<int(filtered.size()))view->track=owner.tracks[size_t(filtered[size_t(position)])];tonegrid::l10n::tooltip(*view,view->spacer?juce::String{}:view->track.title+"\n"+view->track.creator);view->selected=selected;view->refreshAction();view->repaint();return view;
        }
        void listBoxItemClicked(int row,const juce::MouseEvent&)override{if(const int position=trackPosition(row);position>=0&&position<int(filtered.size()))selectTrack(owner.tracks[size_t(filtered[size_t(position)])]);}
        void listBoxItemDoubleClicked(int row,const juce::MouseEvent&)override{if(const int position=trackPosition(row);position>=0&&position<int(filtered.size()))openTrack(owner.tracks[size_t(filtered[size_t(position)])]);}
        void returnKeyPressed(int row)override{if(const int position=trackPosition(row);position>=0&&position<int(filtered.size()))openTrack(owner.tracks[size_t(filtered[size_t(position)])]);}
        void selectTrack(const Track& track)
        {
            key.setSelectedId(rootFor(track.key)+1,juce::dontSendNotification);const int suggested=scaleIndex(track.suggestedScale);if(suggested>=0)scale.setSelectedId(suggested+1,juce::dontSendNotification);updateScale();
        }
        void openTrack(const Track& track)
        {
            const Track chosen=track;if(!safeTrackUrl(chosen.url))return;
            if(activeId==chosen.id&&player.isRequested()){closePlayer();return;}
            player.stop();activeId=chosen.id;selectTrack(chosen);list.updateContent();owner.resized();
            const int position=expandedPosition();if(position>=0)list.getViewport()->setViewPosition(0,(position+(list.getHeight()>=accordionHeight+58?0:1))*58);
            layoutPlayer();owner.viewport.setViewPosition(0,juce::jmax(0,player.getBottom()+16-owner.viewport.getHeight()));
            playerInfo.setText(chosen.title+"\n\n"+chosen.creator+"\n"+chosen.key+" "+chosen.mode+" | "+juce::String(chosen.bpm,1)+" BPM",juce::dontSendNotification);
            player.start(chosen);layoutPlayer();list.updateContent();repaint();
        }
        void requestTrack(const Track& track)
        {juce::MessageManager::callAsync([safe=juce::Component::SafePointer<Body>(this),chosen=track]{if(safe)safe->openTrack(chosen);});}
        void closePlayer(){player.stop();activeId.clear();player.setVisible(false);playerInfo.setVisible(false);external.setVisible(false);closeInline.setVisible(false);list.updateContent();repaint();}
        void timerCallback()override{if(activeId.isNotEmpty())layoutPlayer();if(++fileCheckTicks>=15){fileCheckTicks=0;if(owner.isShowing())owner.reloadIfChanged();}}
        void layoutPlayer()
        {
            if(layingOut||player.isFullScreen())return;const juce::ScopedValueSetter<bool> guard(layingOut,true);const int position=expandedPosition();if(position<0){player.stop();player.setVisible(false);playerInfo.setVisible(false);external.setVisible(false);closeInline.setVisible(false);return;}
            const auto row=list.getRowPosition(position+1,true).translated(list.getX(),list.getY());
            player.setBounds(row.getX()+12,row.getY()+16,playerWidth,playerHeight);playerInfo.setBounds(player.getRight()+20,player.getY()+6,juce::jmax(120,list.getRight()-player.getRight()-36),112);closeInline.setBounds(playerInfo.getX(),player.getY()+124,160,28);external.setBounds(playerInfo.getX(),player.getBottom()-40,160,32);
            const auto outer=owner.viewport.getViewArea();const bool fullyVisible=(owner.testingPlayer||owner.isShowing())&&list.getBounds().contains(player.getBounds())&&outer.contains(player.getBounds());const bool was=player.isRequested();
            // The board scales to the MacBook work area. Screen points must not
            // be compared with the unscaled design height; clipping is checked above.
#if JUCE_MAC
            const bool nativeSize=player.getHeight()>=playerHeight;
#else
            const bool nativeSize=owner.testingPlayer||player.getScreenBounds().getHeight()>=playerHeight;
#endif
            player.setVisible(fullyVisible);playerInfo.setVisible(fullyVisible);external.setVisible(fullyVisible);closeInline.setVisible(fullyVisible);player.showPlayer(fullyVisible&&nativeSize);
            if(was!=player.isRequested())list.updateContent();
        }
        void catalogueChanged()
        {
            closePlayer();
            const auto selected=style.getText();style.clear(juce::dontSendNotification);tonegrid::l10n::item(style,"All styles",1);juce::StringArray names;for(const auto& track:owner.tracks)names.addIfNotAlreadyThere(track.style);
            style.addItemList(names,2);style.setSelectedId(names.contains(selected)?names.indexOf(selected)+2:1,juce::dontSendNotification);filter();
        }
        void filter()
        {
            closePlayer();filtered.clear();const double low=minimum.getText().isEmpty()?20:minimum.getText().getDoubleValue(),high=maximum.getText().isEmpty()?400:maximum.getText().getDoubleValue();
            for(size_t i=0;i<owner.tracks.size();++i)if(matches(owner.tracks[i],style.getSelectedId()==1?juce::String{}:style.getText(),trackKey.getSelectedId()-2,low,high))filtered.push_back(int(i));list.updateContent();empty.setVisible(filtered.empty());empty.toFront(false);repaint();
        }
        void updateScale()
        {fretboard.root=juce::jmax(0,key.getSelectedId()-1);fretboard.scale=juce::jmax(0,scale.getSelectedId()-1);fretboard.repaint();repaint();}
        void resized()override
        {
            const int w=getWidth()-32;scale.setBounds(16,16,250,32);key.setBounds(278,16,155,32);
            fretViewport.setBounds(16,58,w,170);fretboard.setSize(juce::jmax(1040,fretViewport.getMaximumVisibleWidth()),155);
            const int top=310;style.setBounds(16,top,170,32);trackKey.setBounds(196,top,154,32);minimum.setBounds(396,top,64,32);maximum.setBounds(474,top,64,32);clear.setBounds(550,top,104,32);
            list.setBounds(16,354,w,juce::jmax(116,getHeight()-370-(owner.message.isEmpty()?0:30)));empty.setBounds(list.getBounds().reduced(12));layoutPlayer();
        }
        void paint(juce::Graphics& g)override
        {
            using namespace juce;g.setColour(ui::panel());g.fillRoundedRectangle(getLocalBounds().toFloat().reduced(1),9);g.setColour(ui::outline());g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(1),9,1);
            const auto& definition=scales()[size_t(fretboard.scale)];StringArray names;for(int note:definition.notes)names.add(noteName(fretboard.root,note,degreeAt(definition,note)));
            g.setColour(blue());g.fillEllipse(18,233,10,10);g.setColour(ui::secondaryInk());g.setFont(ui::font(12));g.drawText(tonegrid::l10n::translate("Root ")+tonic(fretboard.root)+tonegrid::l10n::translate("     |     ")+names.joinIntoString(tonegrid::l10n::translate(" - "))+tonegrid::l10n::translate("     |     ")+definition.formula,36,228,getWidth()-52,20,Justification::centredLeft,true);
            g.setColour(ui::ink());g.setFont(ui::font(27));g.drawText(juce::String("Backingtrack"),16,264,300,36,Justification::centredLeft);g.setFont(ui::font(12));g.setColour(ui::metadataInk());
            g.drawText(tonegrid::l10n::translate("BPM"),358,310,34,32,Justification::centredLeft);g.drawText(tonegrid::l10n::translate("-"),461,310,12,32,Justification::centred);
            if(filtered.empty()){g.setColour(ui::secondaryInk());g.drawText(tonegrid::l10n::translate("No tracks match. Try All styles, All keys or a wider BPM range."),list.getBounds().reduced(12),Justification::centred);}
            if(owner.message.isNotEmpty()){g.setColour(Colour(0xffffca80));g.setFont(ui::font(12));g.drawText(owner.message,16,getHeight()-34,getWidth()-32,28,Justification::centredLeft,true);}
        }
        View& owner;juce::ComboBox scale,key,style,trackKey;tonegrid::l10n::TextButton clear{"Clear filters"};
        juce::TextEditor minimum,maximum;juce::Viewport fretViewport;Fretboard fretboard;juce::ListBox list;juce::Label empty;std::vector<int> filtered;Player player;juce::Label playerInfo;tonegrid::l10n::TextButton external{"Open on YouTube"},closeInline{"Close player"};juce::String activeId;int fileCheckTicks=0;bool layingOut=false;
    };
public:
    View():body(*this)
    {setLookAndFeel(&look);addAndMakeVisible(viewport);viewport.setViewedComponent(&body,false);viewport.setScrollBarsShown(true,true);viewport.setScrollBarThickness(12);setComponentID("jam-view");}
    ~View()override{body.closePlayer();viewport.setViewedComponent(nullptr,false);setLookAndFeel(nullptr);}
    void setCatalogueFile(const juce::File& file){catalogueLayer.shipped=file;reload();}
    void setCatalogueOverride(const juce::File& file){catalogueLayer.user=file;reload();}
    void reload()
    {
        catalogueFile=catalogueLayer.effective();catalogueRevision=catalogueLayer.revision();
        fileTime=catalogueFile.getLastModificationTime();fileSize=catalogueFile.getSize();message.clear();std::vector<Track> next;juce::String source;
        if(catalogueFile.existsAsFile())
        {if(catalogueFile.getSize()>2*1024*1024){message="Track JSON exceeds 2 MB. Existing catalogue kept.";resized();body.repaint();return;}source=catalogueFile.loadFileAsString();}
        else source=juce::String::fromUTF8(ToneGridApplicationAssets::backingtracks_json,ToneGridApplicationAssets::backingtracks_jsonSize);
        const auto result=decodeCatalogue(source,next);
        if(result.failed())
        {
            message=result.getErrorMessage()+" Existing catalogue kept.";
            if(tracks.empty())decodeCatalogue(juce::String::fromUTF8(ToneGridApplicationAssets::backingtracks_json,ToneGridApplicationAssets::backingtracks_jsonSize),tracks);
        }
        else
        {
            // Retire rows while their indices still refer to the old catalogue.
            // A shorter edited list must never refresh rows through old indices.
            body.closePlayer();body.filtered.clear();tracks=std::move(next);
        }
        body.catalogueChanged();resized();
    }
    void reloadIfChanged(){if(catalogueLayer.revision()!=catalogueRevision)reload();}
    void resized()override{viewport.setBounds(getLocalBounds());body.setSize(juce::jmax(760,getWidth()-12),juce::jmax(680,getHeight()-12));}
    size_t trackCount()const{return tracks.size();}
    juce::String catalogueError()const{return message;}
    void configurePlayer(const juce::File& profile,bool testing=false){testingPlayer=testing;body.player.configure(profile,testing);}
    void stopPlayer(){body.closePlayer();}
    bool playerRequested()const{return body.player.isRequested();}
    juce::Rectangle<int> playerBounds()const{return body.player.getBounds();}
    juce::String activeTrack()const{return body.activeId;}
    void playTrackForTest(int index){if(index>=0&&index<int(tracks.size()))body.openTrack(tracks[size_t(index)]);}
    juce::Component& content(){return body;}
    std::function<bool(const juce::String&)> launch=[](const juce::String& address){return juce::URL(address).launchInDefaultBrowser();};
private:
    popup::Look look;juce::Viewport viewport;std::vector<Track> tracks;content::FileLayer catalogueLayer;juce::String catalogueRevision;juce::File catalogueFile;juce::Time fileTime;juce::int64 fileSize=0;juce::String message;bool testingPlayer=false;Body body;
};
}
