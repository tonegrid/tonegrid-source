#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <TransparentOdAssets.h>
#include <PedalDeploymentAssets.h>
#include <PedalSkin137Assets.h>
#include "Modules/Play/UI/CaptureSelectorStyle.h"
#include "Modules/Play/UI/PedalSkinFiles.h"
#include <array>
#include <vector>

namespace tonegrid::assets
{
// UI assets and logical layout only. Parameter/state ownership stays in PedalCard.
struct Skin
{
    juce::Point<float> referenceSize;
    juce::Rectangle<float> viewport;
    std::array<juce::Rectangle<float>,3> knobs;
    juce::Rectangle<float> led, preset, midi, footswitch, titleHover;
    int frames=0, frameWidth=0, frameHeight=0;
    juce::Image face, strip, ledOff, ledOn, switchUp, switchDown;
    juce::Image sliderCap;
    std::array<juce::Rectangle<float>,7> faders;
    juce::Rectangle<float> artworkClip;
    float artworkRadius=0;
    bool darkInk=false;
    bool bakedKnobs=false;
    bool registeredBox=false;
    float jackAxis=0;
    std::vector<juce::Point<float>> frameAxes;
    bool valid=false;

    static juce::Rectangle<float> bounds(const juce::var& value)
    {return {(float)value["x"],(float)value["y"],(float)value["width"],(float)value["height"]};}
    static juce::Rectangle<float> circle(const juce::var& value)
    {const auto d=(float)value["diameter"];return {(float)value["center"]["x"]-d/2,(float)value["center"]["y"]-d/2,d,d};}
    static Skin fromMetadata(const juce::var& metadata)
    {
        Skin s;s.referenceSize={(float)metadata["referenceSize"]["width"],(float)metadata["referenceSize"]["height"]};
        s.viewport=metadata.hasProperty("viewport")?bounds(metadata["viewport"]):juce::Rectangle<float>(0,0,s.referenceSize.x,s.referenceSize.y);
        const auto* controls=metadata["controls"].getArray();
        if(!controls||controls->size()!=3||s.referenceSize.x<=0||s.referenceSize.y<=0)return s;
        for(int i=0;i<3;++i)s.knobs[(size_t)i]=circle(controls->getReference(i));
        s.led=circle(metadata["led"]);s.footswitch=circle(metadata["footswitch"]);
        s.preset=bounds(metadata["preset"]["bounds"]);s.midi=bounds(metadata["midi"]["bounds"]);
        s.darkInk=(bool)metadata["darkInk"];
        s.registeredBox=(bool)metadata["registeredBox"];s.jackAxis=(float)metadata["jackAxis"];
        if(metadata.hasProperty("artworkClip")){s.artworkClip=bounds(metadata["artworkClip"]);s.artworkRadius=(float)metadata["artworkClip"]["radius"];}
        if(const auto* f=metadata["faders"].getArray())for(int i=0;i<juce::jmin(7,f->size());++i)s.faders[(size_t)i]=bounds(f->getReference(i));
        s.frames=(int)metadata["knobStrip"]["frames"];s.frameWidth=(int)metadata["knobStrip"]["frameWidth"];s.frameHeight=(int)metadata["knobStrip"]["frameHeight"];
        float bottom=0;for(const auto& k:s.knobs)bottom=juce::jmax(bottom,k.getBottom());
        s.titleHover={0,bottom,s.referenceSize.x,juce::jmax(0.f,s.preset.getY()-bottom)};
        if(metadata.hasProperty("titleHover"))s.titleHover=bounds(metadata["titleHover"]);
        s.valid=s.frames>1&&s.frameWidth>0&&s.frameHeight>0&&!s.viewport.isEmpty();
        const juce::Rectangle<float> faceBounds(0,0,s.referenceSize.x,s.referenceSize.y);
        s.valid&=faceBounds.contains(s.viewport);
        for(const auto& k:s.knobs)s.valid&=!k.isEmpty()&&faceBounds.contains(k);
        for(const auto& r:{s.led,s.footswitch,s.preset,s.midi})s.valid&=!r.isEmpty()&&faceBounds.contains(r);
        return s;
    }
    juce::Rectangle<float> faceBounds(juce::Rectangle<int> local)const
    {
        if(registeredBox){const auto sx=local.getWidth()/viewport.getWidth(),sy=local.getHeight()/viewport.getHeight();return {local.getX()-viewport.getX()*sx,local.getY()-viewport.getY()*sy,referenceSize.x*sx,referenceSize.y*sy};}
        const auto visible=juce::RectanglePlacement(juce::RectanglePlacement::centred).appliedTo(viewport,local.toFloat());
        const auto scale=visible.getWidth()/viewport.getWidth();
        return {visible.getX()-viewport.getX()*scale,visible.getY()-viewport.getY()*scale,referenceSize.x*scale,referenceSize.y*scale};
    }
    juce::Rectangle<float> logicalToLocal(juce::Rectangle<float> r,juce::Rectangle<int> local)const
    {const auto f=faceBounds(local);const auto sx=f.getWidth()/referenceSize.x,sy=registeredBox?f.getHeight()/referenceSize.y:sx;return {f.getX()+r.getX()*sx,f.getY()+r.getY()*sy,r.getWidth()*sx,r.getHeight()*sy};}
    int frameFor(float normalized)const noexcept
    {return juce::roundToInt(juce::jlimit(0.f,1.f,normalized)*(frames-1));}
    // Register the opaque rim once on the UI thread. Brightness/pointer motion
    // must not determine the physical axis; do not edit or redraw source pixels.
    void registerFrameAxes()
    {
        frameAxes.assign((size_t)frames,{frameWidth*.5f,frameHeight*.5f});
        juce::Image::BitmapData pixels(strip,juce::Image::BitmapData::readOnly);
        for(int frame=0;frame<frames;++frame)
        {
            const auto opaque=[&](int x,int y){return pixels.getPixelColour(x,frame*frameHeight+y).getAlpha()>180;};
            double n=0,sx=0,sy=0,sxx=0,syy=0,sxy=0,sz=0,sxz=0,syz=0;
            for(int y=1;y<frameHeight-1;++y)for(int x=1;x<frameWidth-1;++x)
            {
                if(!opaque(x,y)||(opaque(x-1,y)&&opaque(x+1,y)&&opaque(x,y-1)&&opaque(x,y+1)))continue;
                const double px=x+.5-frameWidth*.5,py=y+.5-frameHeight*.5,z=px*px+py*py;
                ++n;sx+=px;sy+=py;sxx+=px*px;syy+=py*py;sxy+=px*py;sz+=z;sxz+=px*z;syz+=py*z;
            }
            if(n<8)continue;
            const auto xx=sxx-sx*sx/n,yy=syy-sy*sy/n,xy=sxy-sx*sy/n;
            const auto xz=sxz-sx*sz/n,yz=syz-sy*sz/n,det=xx*yy-xy*xy;
            if(std::abs(det)<1.e-9)continue;
            const auto dx=.5*(yy*xz-xy*yz)/det,dy=.5*(xx*yz-xy*xz)/det;
            if(std::abs(dx)>4||std::abs(dy)>4)continue;
            frameAxes[(size_t)frame]={frameWidth*.5f+(float)dx,frameHeight*.5f+(float)dy};
        }
    }
    juce::Rectangle<float> frameDestination(int frame,juce::Rectangle<float> bounds)const
    {
        const auto scale=juce::jmin(bounds.getWidth()/frameWidth,bounds.getHeight()/frameHeight);
        const auto axis=frameAxes[(size_t)frame];
        return {bounds.getCentreX()-axis.x*scale,bounds.getCentreY()-axis.y*scale,frameWidth*scale,frameHeight*scale};
    }
    void paint(juce::Graphics& g,juce::Rectangle<int> local,bool active,float ledIntensity=1.f)const
    {
        g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
        {juce::Graphics::ScopedSaveState saved(g);
        if(!artworkClip.isEmpty()){juce::Path clip;const auto r=logicalToLocal(artworkClip,local);clip.addRoundedRectangle(r,artworkRadius*r.getWidth()/artworkClip.getWidth());g.reduceClipRegion(clip);}
        g.drawImage(face,faceBounds(local));}
        paintLed(g,logicalToLocal(led,local),active,ledIntensity);
    }
    void paintLed(juce::Graphics& g,juce::Rectangle<float> destination,bool active,float intensity=1.f)const
    {
        // The supplied lens is eccentric within its bezel. Keep the original
        // metal pixels fixed and register the lens separately, as UI artwork
        // only; the source PNGs and pedal state are unchanged.
        const auto scale=destination.getWidth()/40.f;
        const juce::Point<float> bezelAxis(25.05f,21.2f);
        const auto centre=destination.getPosition()+bezelAxis*scale;
        const auto radius=11.5f*scale;
        const juce::Rectangle<float> aperture(centre.x-radius,centre.y-radius,2*radius,2*radius);
        juce::Path hole;hole.addEllipse(aperture);
        // The PNG also contains an off-centre black backing/shadow. Limit the
        // supplied artwork to the metal housing and place its soft black
        // support concentrically beneath it. Lens registration stays unchanged.
        const auto supportRadius=18.4f*scale;
        const auto housingRadius=17.f*scale;
        const juce::Rectangle<float> support(centre.x-supportRadius,centre.y-supportRadius,2*supportRadius,2*supportRadius);
        juce::ColourGradient shadow(juce::Colours::black.withAlpha(.7f),centre.x,centre.y,
                                   juce::Colours::transparentBlack,centre.x+supportRadius,centre.y,true);
        shadow.addColour(.87,juce::Colours::black.withAlpha(.6f));
        {
            juce::Graphics::ScopedSaveState saved(g);juce::Path backing;
            backing.setUsingNonZeroWinding(false);backing.addEllipse(support);
            backing.addEllipse(centre.x-housingRadius,centre.y-housingRadius,2*housingRadius,2*housingRadius);
            g.reduceClipRegion(backing);g.setGradientFill(shadow);g.fillEllipse(support);
        }
        {
            juce::Graphics::ScopedSaveState saved(g);juce::Path bezel;
            bezel.setUsingNonZeroWinding(false);
            bezel.addEllipse(centre.x-housingRadius,centre.y-housingRadius,2*housingRadius,2*housingRadius);
            bezel.addEllipse(aperture);
            g.reduceClipRegion(bezel);g.drawImage(ledOff,destination);
        }
        {
            juce::Graphics::ScopedSaveState saved(g);g.reduceClipRegion(hole);
            const auto offDelta=(bezelAxis-juce::Point<float>(26.3f,22.05f))*scale;
            if(bakedKnobs||!active)g.drawImage(ledOff,destination.translated(offDelta.x,offDelta.y));
            if(active)
            {
                const auto onDelta=(bezelAxis-juce::Point<float>(25.92f,21.96f))*scale;
                g.setOpacity(juce::jlimit(0.f,1.f,intensity));
                g.drawImage(ledOn,destination.translated(onDelta.x,onDelta.y));
            }
        }
    }
};

inline const Skin& transparentOd()
{
    static const Skin skin=[]
    {
        auto s=Skin::fromMetadata(juce::JSON::parse(juce::String::fromUTF8(TransparentOdAssets::transparent_od_layout_json,TransparentOdAssets::transparent_od_layout_jsonSize)));
        auto image=[](const char* data,int size){return juce::ImageCache::getFromMemory(data,size);};
        s.face=image(TransparentOdAssets::face_png,TransparentOdAssets::face_pngSize);
        s.strip=image(TransparentOdAssets::knob_strip_png,TransparentOdAssets::knob_strip_pngSize);
        s.ledOff=image(TransparentOdAssets::led_off_png,TransparentOdAssets::led_off_pngSize);
        s.ledOn=image(TransparentOdAssets::led_on_png,TransparentOdAssets::led_on_pngSize);
        s.switchUp=image(TransparentOdAssets::footswitch_up_png,TransparentOdAssets::footswitch_up_pngSize);
        s.switchDown=image(TransparentOdAssets::footswitch_down_png,TransparentOdAssets::footswitch_down_pngSize);
        s.valid&=s.face.isValid()&&s.strip.getWidth()==s.frameWidth&&s.strip.getHeight()==s.frames*s.frameHeight&&s.ledOff.isValid()&&s.ledOn.isValid()&&s.switchUp.isValid()&&s.switchDown.isValid();
        jassert(s.valid);if(s.valid)s.registerFrameAxes();return s;
    }();return skin;
}

// Visual assets only: the fixed visual order never becomes a persistence or
// DSP identity. Transparent OD itself still returns the original golden master.
inline const Skin& pedal(int visual)
{
    static const std::array<Skin,13> skins=[]
    {
        const auto& master=transparentOd();std::array<Skin,13> result;
        const std::array<const char*,13> names{"classic_fuzz","treble_boost","auto_wah","studio_comp","","horse_od","808_od","blues_od","filter_dist","orange_dist","vintage_fuzz","graphic_eq","clean_boost"};
        for(int v=0;v<13;++v)
        {
            if(v==4)continue;
            const auto prefix=juce::String("p")+names[(size_t)v];int bytes=0;
            const auto* json=PedalDeploymentAssets::getNamedResource((prefix+"_layout_json").toRawUTF8(),bytes);
            auto s=Skin::fromMetadata(juce::JSON::parse(juce::String::fromUTF8(json,bytes)));
            const auto* png=PedalDeploymentAssets::getNamedResource((prefix+"_face_png").toRawUTF8(),bytes);s.face=juce::ImageCache::getFromMemory(png,bytes);
            s.strip=master.strip;s.frameAxes=master.frameAxes;s.ledOn=master.ledOn;s.ledOff=master.ledOff;s.switchUp=master.switchUp;s.switchDown=master.switchDown;
            s.sliderCap=juce::ImageCache::getFromMemory(PedalDeploymentAssets::slider_cap_png,PedalDeploymentAssets::slider_cap_pngSize).getClippedImage({378,369,499,516});
            s.valid&=s.face.isValid()&&s.strip.isValid();jassert(s.valid);result[(size_t)v]=std::move(s);
        }return result;
    }();
    return visual==4?transparentOd():skins[(size_t)visual];
}
inline juce::Image background()
{return juce::ImageCache::getFromMemory(PedalDeploymentAssets::background_png,PedalDeploymentAssets::background_pngSize);}

// The supplied EQ is RGB on white. Remove only edge-connected white pixels in
// this UI loading copy; the original supplied PNG remains unchanged on disk.
inline juce::Image transparentEq137(const juce::Image& source)
{
    auto image=source.convertedToFormat(juce::Image::ARGB).createCopy();
    juce::Image::BitmapData pixels(image,juce::Image::BitmapData::readWrite);
    const int w=image.getWidth(),h=image.getHeight();std::vector<unsigned char> visited(size_t(w*h),0);std::vector<juce::Point<int>> queue;
    auto offer=[&](int x,int y){if(x<0||y<0||x>=w||y>=h)return;const auto i=size_t(y*w+x);if(visited[i])return;visited[i]=1;const auto c=pixels.getPixelColour(x,y);if(juce::jmin(c.getRed(),c.getGreen(),c.getBlue())>=239)queue.push_back({x,y});};
    for(int x=0;x<w;++x){offer(x,0);offer(x,h-1);}for(int y=0;y<h;++y){offer(0,y);offer(w-1,y);}
    for(size_t i=0;i<queue.size();++i){const auto p=queue[i];pixels.setPixelColour(p.x,p.y,juce::Colours::transparentBlack);offer(p.x-1,p.y);offer(p.x+1,p.y);offer(p.x,p.y-1);offer(p.x,p.y+1);}
    return image;
}
inline const Skin& pedal137(int visual)
{
    static const std::array<Skin,13> skins=[]
    {
        std::array<Skin,13> result;const auto& master=transparentOd();
        const auto metadata=juce::JSON::parse(juce::String::fromUTF8(PedalSkin137Assets::layout_json,PedalSkin137Assets::layout_jsonSize));
        auto image=[](const char* name){int bytes=0;const auto* data=PedalSkin137Assets::getNamedResource(name,bytes);return juce::ImageCache::getFromMemory(data,bytes);};
        auto high=image("switch_high_png"),low=image("switch_low_png");
        high=loadSkinPng137(skinFolders137(),"switch_high.png",{high.getWidth(),high.getHeight()},high);
        low=loadSkinPng137(skinFolders137(),"switch_low.png",{low.getWidth(),low.getHeight()},low);
        for(int v=0;v<13;++v)
        {
            auto s=Skin::fromMetadata(metadata["pedals"][v]);
            const auto name="face"+juce::String(v+1).paddedLeft('0',2)+"_png";
            s.face=loadSkinPng137(skinFolders137(),"face"+juce::String(v+1).paddedLeft('0',2)+".png",{int(s.referenceSize.x),int(s.referenceSize.y)},{});
            if(!s.face.isValid())s.face=image(name.toRawUTF8());
            s.ledOff=master.ledOff;s.ledOn=master.ledOn;s.switchUp=high;s.switchDown=low;
            s.sliderCap=juce::ImageCache::getFromMemory(PedalDeploymentAssets::slider_cap_png,PedalDeploymentAssets::slider_cap_pngSize).getClippedImage({378,369,499,516});s.bakedKnobs=true;
            // Padded viewports register enclosures with differing source canvases.
            s.valid=s.face.isValid()&&high.isValid()&&low.isValid()&&!s.viewport.isEmpty();
            result[size_t(v)]=std::move(s);
        }
        return result;
    }();return skins[size_t(visual)];
}

class Look final : public juce::LookAndFeel_V4
{
public:
    explicit Look(const Skin& s):skin(s)
    {
        using namespace juce;
        setColour(ComboBox::backgroundColourId,Colours::transparentBlack);setColour(ComboBox::textColourId,controlInk());
        setColour(ComboBox::outlineColourId,controlInk());setColour(TextButton::textColourOffId,controlInk());setColour(TextButton::textColourOnId,controlInk());
        setColour(PopupMenu::backgroundColourId,Colour(0xff202427));setColour(PopupMenu::textColourId,ink());
        setColour(PopupMenu::highlightedBackgroundColourId,Colour(0xff3c4246));setColour(PopupMenu::highlightedTextColourId,ink());
    }
    static juce::Colour ink(){return ui::ink();}
    juce::Colour controlInk()const{return skin.darkInk?juce::Colour(0xff172027):ink();}
    void drawRotarySlider(juce::Graphics& g,int x,int y,int w,int h,float pos,float,float,juce::Slider& slider)override
    {
        // Slider supplies an inset rotary rectangle. Metadata defines the full bitmap bounds.
        juce::ignoreUnused(x,y,w,h);const auto r=slider.getLocalBounds().toFloat();
        if(skin.bakedKnobs)
        {
            // The cleaned skin supplies the stationary knob, lighting and shadow.
            // JUCE draws only one movable white indicator, never another bitmap disk.
            const auto angle=(pos-.5f)*juce::MathConstants<float>::pi*1.5f;
            const auto radius=juce::jmin(r.getWidth(),r.getHeight())*.5f;
            const auto centre=r.getCentre();
            const auto point=[&](float distance){return centre+juce::Point<float>(std::sin(angle)*distance,-std::cos(angle)*distance);};
            const auto outer=point(radius*.88f),inner=point(radius*.40f);
            juce::Path marker;marker.startNewSubPath(outer);marker.lineTo(inner);
            g.setColour(juce::Colour(0xfff5f2e8));
            g.strokePath(marker,juce::PathStrokeType(juce::jmax(1.1f,r.getWidth()*.042f),juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
            return;
        }
        const auto frame=skin.frameFor(pos);
        // The strip rotates a perspective photograph: its white cap orbits
        // inside the black housing. Keep the supplied neutral housing/cap edge
        // fixed; rotate only the photographed inner face about the cap's axis.
        // Source coordinates belong to this asset, not the outer alpha rim.
        constexpr float capX=62.5f,capY=57.f,innerRadius=42.5f;
        const int neutral=skin.frames/2;
        const auto image=skin.strip.getClippedImage({0,neutral*skin.frameHeight,skin.frameWidth,skin.frameHeight});
        const auto destination=skin.frameDestination(neutral,r);
        const auto scale=destination.getWidth()/skin.frameWidth;
        g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
        g.drawImage(image,destination);
        if(frame==neutral)return;
        juce::Graphics::ScopedSaveState saved(g);juce::Path face;
        const auto cx=destination.getX()+capX*scale,cy=destination.getY()+capY*scale,radius=innerRadius*scale;
        face.addEllipse(cx-radius,cy-radius,2*radius,2*radius);g.reduceClipRegion(face);
        const auto angle=(frame-neutral)*juce::MathConstants<float>::pi*1.5f/(skin.frames-1);
        g.drawImageTransformed(image,juce::AffineTransform::rotation(angle,capX,capY).scaled(scale).translated(destination.getX(),destination.getY()));
    }
    void drawComboBox(juce::Graphics& g,int w,int h,bool,int,int,int,int,juce::ComboBox& box)override
    {
        const float scale=h/(skin.bakedKnobs?32.f:skin.preset.getHeight());const auto r=juce::Rectangle<float>(.5f,.5f,w-1.f,h-1.f);
        const float alpha=box.isEnabled()?1.f:ui::disabledOpacity;
        g.setColour((skin.darkInk?juce::Colours::white:ui::panel()).withAlpha(skin.darkInk?.20f:.48f));g.fillRoundedRectangle(r,ui::controlRadius*scale);
        g.setColour(controlInk().withAlpha(alpha*(box.hasKeyboardFocus(true)?.78f:.42f)));g.drawRoundedRectangle(r,ui::controlRadius*scale,ui::stroke);
        g.setColour(controlInk().withAlpha(alpha*.88f));
        const float right=w-10*scale,cy=h*.5f;juce::Path arrow;
        arrow.startNewSubPath(right-3.5f*scale,cy-2*scale);arrow.lineTo(right,cy+2*scale);arrow.lineTo(right+3.5f*scale,cy-2*scale);
        g.strokePath(arrow,juce::PathStrokeType(1));
    }
    juce::Font getComboBoxFont(juce::ComboBox&)override{return tonegrid::captureui::font();}
    void positionComboBoxText(juce::ComboBox& box,juce::Label& label)override{tonegrid::captureui::position(box,label);}
    void drawLabel(juce::Graphics& g,juce::Label& label)override
    {if(label.getComponentID()=="pedal-preset-text")tonegrid::captureui::paint(g,label);else LookAndFeel_V4::drawLabel(g,label);}
    juce::Font getPopupMenuFont()override{return ui::font(ui::primarySize);}
    juce::PopupMenu::Options getOptionsForComboBoxPopupMenu(juce::ComboBox& box,juce::Label& label)override
    {return tonegrid::captureui::options(box,label,getPopupMenuFont());}
    juce::Font getTextButtonFont(juce::TextButton& b,int height)override
    {return ui::font(juce::jmax(10.f,height/skin.midi.getHeight()*13.f));}
    void drawButtonBackground(juce::Graphics& g,juce::Button& b,const juce::Colour&,bool,bool down)override
    {
        if(b.getComponentID()=="power")
        {g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);g.drawImage(down?skin.switchDown:skin.switchUp,b.getLocalBounds().toFloat());return;}
        const auto r=b.getLocalBounds().toFloat().reduced(.5f);const auto radius=ui::controlRadius*b.getHeight()/skin.midi.getHeight();
        g.setColour((skin.darkInk?juce::Colours::white:ui::panel()).withAlpha(.16f));g.fillRoundedRectangle(r,radius);
        g.setColour(controlInk().withAlpha(b.isEnabled()?.38f:.22f));g.drawRoundedRectangle(r,radius,ui::stroke);
    }
    void drawButtonText(juce::Graphics& g,juce::TextButton& b,bool,bool)override
    {
        if(b.getComponentID()=="power")return;
        g.setColour(controlInk().withAlpha(b.isEnabled()?.85f:ui::disabledOpacity));g.setFont(getTextButtonFont(b,b.getHeight()));
        g.drawFittedText(b.getButtonText(),b.getLocalBounds().reduced(2,0),juce::Justification::centred,1);
    }
    void drawLinearSlider(juce::Graphics& g,int,int,int,int,float position,float,float,const juce::Slider::SliderStyle,juce::Slider& slider)override
    {
        // Track/frequency text belongs to the static face. The supplied cap
        // moves linearly without rotation, with a small physical press travel.
        const float size=slider.getWidth()*.86f;
        const auto cap=juce::Rectangle<float>(0,0,size,size).withCentre({slider.getWidth()*.5f,position+(slider.isMouseButtonDown()?1.f:0.f)});
        g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);g.drawImage(skin.sliderCap,cap);
    }
private:
    const Skin& skin;
};
}
