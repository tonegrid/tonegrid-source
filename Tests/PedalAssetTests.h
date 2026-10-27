#pragma once
#include "Modules/Play/UI/PedalCard.h"
#include <iostream>
#include <juce_cryptography/juce_cryptography.h>
#include "Modules/Play/UI/PedalScene.h"

namespace pedalAssetTests
{
inline juce::String hash(const juce::Image& image)
{juce::MemoryOutputStream png;juce::PNGImageFormat().writeImageToStream(image,png);return juce::SHA256(png.getData(),png.getDataSize()).toHexString();}
// Independent pixel oracle: opaque silhouette centroid, not the fitted rim axis.
inline juce::Point<float> opaqueCentre(const juce::Image& image)
{
    juce::Image::BitmapData pixels(image,juce::Image::BitmapData::readOnly);
    double x=0,y=0,n=0;
    for(int row=0;row<image.getHeight();++row)for(int col=0;col<image.getWidth();++col)
        if(pixels.getPixelColour(col,row).getAlpha()>180){x+=col+.5;y+=row+.5;++n;}
    return {(float)(x/n),(float)(y/n)};
}
// Measure the actual metallic LED rim, not the transparent PNG rectangle.
inline float ledRimCentreX(const juce::Image& image,juce::Rectangle<float> bounds)
{
    juce::Image::BitmapData pixels(image,juce::Image::BitmapData::readOnly);
    const double scale=bounds.getWidth()/40.,cx=bounds.getX()+25.5*scale,cy=bounds.getY()+22.5*scale;
    double n=0,sx=0,sy=0,sxx=0,syy=0,sxy=0,sz=0,sxz=0,syz=0;
    const auto scan=bounds.expanded(1).toNearestInt().getIntersection(image.getBounds());
    for(int y=scan.getY();y<scan.getBottom();++y)for(int x=scan.getX();x<scan.getRight();++x)
    {
        const double px=(x+.5-cx)/scale,py=(y+.5-cy)/scale,z=px*px+py*py;
        const auto c=pixels.getPixelColour(x,y);
        if(z<=13*13||z>=17*17||juce::jmax(c.getRed(),c.getGreen(),c.getBlue())<=70)continue;
        ++n;sx+=px;sy+=py;sxx+=px*px;syy+=py*py;sxy+=px*py;sz+=z;sxz+=px*z;syz+=py*z;
    }
    if(n<8)return -1000;
    const auto xx=sxx-sx*sx/n,yy=syy-sy*sy/n,xy=sxy-sx*sy/n,det=xx*yy-xy*xy;
    if(std::abs(det)<1e-9)return -1000;
    return (float)(cx+.5*(yy*(sxz-sx*sz/n)-xy*(syz-sy*sz/n))/det*scale);
}
inline bool run(const juce::File& models,juce::String& report,bool trace=false)
{
    using namespace juce;bool ok=true;int checks=0;
    auto check=[&](bool value,const String& message){++checks;ok&=value;report+=String(value?"PASS ":"FAIL ")+message+"\n";if(trace)std::cout<<(value?"PASS ":"FAIL ")<<message<<std::endl;};
    const auto& skin=tonegrid::assets::transparentOd();
    check(skin.valid&&skin.face.getWidth()==1024&&skin.face.getHeight()==1536,"Pilot embeds unchanged clean static face and valid integration metadata");
    check(skin.frames==101&&skin.frameWidth==128&&skin.frameHeight==128&&skin.strip.getHeight()==12928,"Pilot retains all 101 corrected v3 filmstrip frames");
    check(skin.frameFor(-1)==0&&skin.frameFor(0)==0&&skin.frameFor(.5f)==50&&skin.frameFor(1)==100&&skin.frameFor(2)==100,"Filmstrip maps parameter normalization to exact endpoints and neutral frame");
    auto wideJson=JSON::parse(String::fromUTF8(TransparentOdAssets::transparent_od_layout_json,TransparentOdAssets::transparent_od_layout_jsonSize));wideJson["referenceSize"].getDynamicObject()->setProperty("width",340);wideJson.getDynamicObject()->removeProperty("viewport");
    const auto wide=tonegrid::assets::Skin::fromMetadata(wideJson);
    check(wide.valid&&wide.faceBounds({0,0,340,450}).getWidth()==340,"Asset layout accepts a future different logical width without changing Graphic EQ");
    board::Processor processor;const auto loaded=processor.load(models);check(loaded.wasOk(),"Pilot interaction tests load unchanged model catalog");if(loaded.failed())return false;
    const int id=board::id(board::PedalId::TransparentOd);PedalCard card(4,processor,false);card.setSize(300,450);card.setVisible(true);
    auto* preset=dynamic_cast<ComboBox*>(card.findChildWithID("pedal-preset"));auto* midi=dynamic_cast<tonegrid::LearnButton*>(card.findChildWithID("pedal-midi"));auto* power=dynamic_cast<Button*>(card.findChildWithID("power"));auto* title=dynamic_cast<Label*>(card.findChildWithID("pedal-title"));
    auto* input=dynamic_cast<Slider*>(card.findChildWithID("input-trim"));auto* tone=dynamic_cast<Slider*>(card.findChildWithID("tone"));auto* level=dynamic_cast<Slider*>(card.findChildWithID("level"));
    check(preset&&midi&&power&&title&&input&&tone&&level,"Pilot reuses all existing JUCE interactive controls");if(!preset||!midi||!power||!title||!input||!tone||!level)return false;
    check(title->getText().isEmpty()&&title->getTooltip().contains(processor.modelFile(id,processor.get(id).preset)),"Baked title is not duplicated and real NAM tooltip remains on title region");
    bool staticLabels=false;for(auto* child:card.getChildren())if(auto* label=dynamic_cast<Label*>(child))staticLabels|=label->isVisible()&&label->getText().isNotEmpty();
    check(!staticLabels,"Pilot does not duplicate baked knob labels, title or subtitle");
    check(preset->getBounds()==skin.logicalToLocal(skin.preset,card.getLocalBounds()).toNearestInt()&&midi->getBounds()==skin.logicalToLocal(skin.midi,card.getLocalBounds()).toNearestInt()&&power->getBounds()==skin.logicalToLocal(skin.footswitch,card.getLocalBounds()).toNearestInt(),"Preset/MIDI/footswitch use documented integration JSON logical coordinates");
    check(input->getMinimum()==-12&&input->getMaximum()==12&&tone->getMinimum()==-6&&tone->getMaximum()==6&&level->getMinimum()==-24&&level->getMaximum()==24,"Pilot preserves Input Trim, post-NAM Tone and Level ranges");
    const auto initial=processor.get(id);input->setValue(5.2,sendNotificationSync);tone->setValue(-3.1,sendNotificationSync);level->setValue(-4.7,sendNotificationSync);
    auto state=processor.get(id);check(std::abs(state.input-5.2f)<1e-5&&std::abs(state.tone+3.1f)<1e-5&&std::abs(state.output+4.7f)<1e-5&&state.mix==initial.mix,"Filmstrip knobs retain original bindings; no dry/wet Mix reintroduced");
    processor.set(id,initial);card.sync();
    check(input->getValue()==initial.input&&tone->getValue()==initial.tone&&level->getValue()==initial.output,"Existing state restore synchronizes filmstrip values");
    card.midi({},false,true);check(midi->getButtonText()=="MIDI"&&midi->getTooltip().contains("Right-click"),"Pilot MIDI caption is compact with existing learn/clear instructions");
    card.midi({2,16,127},false,true);check(midi->getWidth()==preset->getWidth(),"Assigned MIDI text uses compact preset width rather than shrinking below readable font size");check(midi->getButtonText()=="CC 127 / Ch 16"&&midi->getTooltip().contains("channel 16"),"Pilot preserves full MIDI assignment in tooltip and compact assignment on face");card.midi({},false,true);
    auto& look=dynamic_cast<tonegrid::assets::Look&>(card.getLookAndFeel());
    check(look.findColour(ComboBox::backgroundColourId).isTransparent()&&look.findColour(ComboBox::outlineColourId)==tonegrid::assets::Look::ink(),"Pilot dropdown uses transparent fill and off-white outline only");
    check(look.getComboBoxFont(*preset).getHeight()==12.f&&look.getTextButtonFont(*midi,midi->getHeight()).getHeight()>=13.f,"Capture font fixed at twelve logical pixels; MIDI font retained");
    check(skin.led.getCentre()==Point<float>(145.6f,52.f)&&std::abs(skin.led.getWidth()-21.28f*1.5f)<1e-4&&skin.led.getY()-skin.viewport.getY()>11.f,"LED retains approved size/height/top clearance with horizontal compensation for the source rim axis");
    Image rotary(Image::ARGB,58,58,true),expected(Image::ARGB,58,58,true);
    input->setSize(58,58);
    {Graphics a(rotary),b(expected);look.drawRotarySlider(a,5,3,48,52,.5f,0,0,*input);b.setImageResamplingQuality(Graphics::highResamplingQuality);b.drawImage(skin.strip.getClippedImage({0,50*128,128,128}),skin.frameDestination(50,{0,0,58,58}));}
    check(hash(rotary)==hash(expected),"Neutral rotary retains supplied cap/housing pixels; JUCE inset ignored and no additional pointer drawn");card.resized();
    check(skin.frameAxes.size()==101,"All filmstrip axes are registered once before interactive rendering");
    Point<float> rawMin(1000,1000),rawMax(-1000,-1000),fixedMin=rawMin,fixedMax=rawMax;
    bool fixedAxis=true,fixedHousing=true,markerSweep=true;float minRadius=1000,maxRadius=0;Image referenceRotary(Image::ARGB,128,128,true);input->setSize(128,128);{Graphics g(referenceRotary);look.drawRotarySlider(g,0,0,128,128,.5f,0,0,*input);}
    const auto neutralBounds=skin.frameDestination(50,{0,0,128,128});
    double refX=0,refY=0,refCount=0;
    {Image::BitmapData pixels(referenceRotary,Image::BitmapData::readOnly);
    for(int y=0;y<128;++y)for(int x=0;x<128;++x)
    {
        const auto c=pixels.getPixelColour(x,y);const float dx=x+.5f-neutralBounds.getX()-62.5f,dy=y+.5f-neutralBounds.getY()-57.f;
        if(dx*dx+dy*dy<42.f*42.f&&c.getAlpha()>180){const auto darkness=1.-jmax(c.getRed(),c.getGreen(),c.getBlue())/200.;if(darkness>0){refX+=dx*darkness;refY+=dy*darkness;refCount+=darkness;}}
    }}
    // Intensity-weighted marker measurements include antialias coverage instead of
    // thresholding it to whole pixels. Preserve the source neutral angular bias.
    const auto referenceAngle=std::atan2(refX,-refY);double maxAngleError=0;
    check(refCount>20,"Neutral source pointer is independently measurable within the white cap");
    Image sweep(Image::ARGB,640,128,true);input->setSize(128,128);
    for(int frame=0;frame<101;++frame)
    {
        const auto raw=skin.strip.getClippedImage({0,frame*128,128,128});Image rendered(Image::ARGB,128,128,true);
        {Graphics g(rendered);look.drawRotarySlider(g,0,0,128,128,frame/100.f,0,0,*input);}
        const auto a=opaqueCentre(raw),b=opaqueCentre(rendered);
        rawMin={jmin(rawMin.x,a.x),jmin(rawMin.y,a.y)};rawMax={jmax(rawMax.x,a.x),jmax(rawMax.y,a.y)};
        fixedMin={jmin(fixedMin.x,b.x),jmin(fixedMin.y,b.y)};fixedMax={jmax(fixedMax.x,b.x),jmax(fixedMax.y,b.y)};
        const auto r=skin.frameDestination(50,{0,0,128,128});const auto axis=skin.frameAxes[50];
        fixedAxis&=std::abs(r.getX()+axis.x-64.f)<1e-5&&std::abs(r.getY()+axis.y-64.f)<1e-5&&r.getWidth()==128&&r.getHeight()==128;
        const float cx=r.getX()+62.5f,cy=r.getY()+57.f;double px=0,py=0,n=0;
        {Image::BitmapData pixels(rendered,Image::BitmapData::readOnly),baseline(referenceRotary,Image::BitmapData::readOnly);
        for(int y=0;y<128;++y)for(int x=0;x<128;++x)
        {
            const float dx=x+.5f-cx,dy=y+.5f-cy;const auto c=pixels.getPixelColour(x,y);
            if(dx*dx+dy*dy>44.f*44.f)fixedHousing&=c==baseline.getPixelColour(x,y);
            if(dx*dx+dy*dy<42.f*42.f&&c.getAlpha()>180){const auto darkness=1.-jmax(c.getRed(),c.getGreen(),c.getBlue())/200.;if(darkness>0){px+=dx*darkness;py+=dy*darkness;n+=darkness;}}
        }}
        markerSweep&=n>20;
        if(n>0){const auto radius=(float)std::hypot(px/n,py/n);minRadius=jmin(minRadius,radius);maxRadius=jmax(maxRadius,radius);
        const auto measured=std::atan2(px/n,-py/n),expectedAngle=(frame-50)*MathConstants<double>::pi*1.5/100;
        const auto error=std::abs(measured-expectedAngle-referenceAngle);maxAngleError=jmax(maxAngleError,error);markerSweep&=error<.025;}

        if(frame%25==0){Graphics g(sweep);g.drawImageAt(rendered,(frame/25)*128,0);}
    }
    check(rawMax.x-rawMin.x>1.f&&rawMax.y-rawMin.y>1.f,"Negative control confirms visible axis drift in the unregistered source frames");
    check(fixedAxis,"Fixed reference housing remains at the same centre and scale throughout rotation");
    check(fixedMax.x-fixedMin.x<.01f&&fixedMax.y-fixedMin.y<.01f,"All 101 rendered frames retain the exact opaque silhouette centre");
    check(fixedHousing,"White face edge, black housing and shadow pixels outside the rotating interior stay bit-identical across all 101 positions");
    check(markerSweep&&maxRadius-minRadius<.5f,"Single source pointer tracks the 270-degree sweep on a constant-radius arc about the fixed white-cap centre");
    report+="POINTER_RADIUS_SPAN="+String(maxRadius-minRadius,4)+" pixels\n";
    report+="POINTER_ANGLE_MAX_ERROR="+String(maxAngleError,6)+" radians\n";
    if(trace)std::cout<<"POINTER_RADIUS_SPAN="<<maxRadius-minRadius<<" POINTER_ANGLE_MAX_ERROR="<<maxAngleError<<std::endl;
    report+="KNOB_CENTROID_SPAN raw="+String(rawMax.x-rawMin.x,4)+","+String(rawMax.y-rawMin.y,4)+" registered="+String(fixedMax.x-fixedMin.x,4)+","+String(fixedMax.y-fixedMin.y,4)+" pixels\n";card.resized();
    Image released(Image::ARGB,56,56,true),pressed(Image::ARGB,56,56,true);
    power->setSize(56,56);
    // End native image drawing before reading back pixels (JUCE Direct2D).
    {Graphics up(released),down(pressed);look.drawButtonBackground(up,*power,Colours::transparentBlack,false,false);look.drawButtonBackground(down,*power,Colours::transparentBlack,false,true);}
    check(hash(released)!=hash(pressed)&&released.getPixelAt(28,28).getAlpha()>0&&pressed.getPixelAt(28,28).getAlpha()>0,"Switch rendering swaps the supplied UP/DOWN images");card.resized();
    auto offState=initial;offState.enabled=false;processor.set(id,offState);card.sync();const auto off=card.createComponentSnapshot(card.getLocalBounds());
    power->onClick();const auto on=card.createComponentSnapshot(card.getLocalBounds());check(processor.get(id).enabled&&hash(off)!=hash(on),"Existing bypass click changes state and supplied LED image");processor.set(id,initial);card.sync();
    const auto ledBounds=skin.logicalToLocal(skin.led,card.getLocalBounds());
    check(std::abs(ledRimCentreX(off,ledBounds)-card.getWidth()*.5f)<.75f&&std::abs(ledRimCentreX(on,ledBounds)-card.getWidth()*.5f)<.75f,"Actual rendered ON and OFF metallic LED rims align with the pedal horizontal centre within subpixel raster tolerance");
    const auto output=File::getSpecialLocation(File::currentExecutableFile).getSiblingFile("PilotPreview");output.createDirectory();
    auto writePreview=[&](const String& filename,const Image& image){if(auto stream=output.getChildFile(filename).createOutputStream()){stream->setPosition(0);stream->truncate();PNGImageFormat().writeImageToStream(image,*stream);}};
    writePreview("transparent-od-off.png",off);writePreview("transparent-od-on.png",on);writePreview("knob-sweep.png",sweep);
    const File golden(TONEGRID_TEST_REFERENCE_DIR);
    // Compare decoded PNG pixels on both sides. Direct2D's premultiplied
    // surface and the PNG decoder can otherwise round partial alpha differently.
    // 1.1.23 intentionally refines the two JUCE overlay surfaces (selector and MIDI).
    // Keep the original approved PNGs as independent oracles for all other pixels:
    // baked face, physical knobs, LED, footswitch and their registration/shadows.
    // These two exact component rectangles were independently validated against
    // immutable asset metadata above; do not expand the mask to the pedal face.
    const auto changedSelector=preset->getBounds(),changedMidi=midi->getBounds();
    auto samePixels=[&](const Image& surface,const Image& b){MemoryOutputStream png;PNGImageFormat().writeImageToStream(surface,png);const auto a=ImageFileFormat::loadFrom(png.getData(),png.getDataSize());if(!a.isValid()||!b.isValid()||a.getBounds()!=b.getBounds())return false;for(int y=0;y<a.getHeight();++y)for(int x=0;x<a.getWidth();++x)if(!changedSelector.contains(x,y)&&!changedMidi.contains(x,y)&&a.getPixelAt(x,y)!=b.getPixelAt(x,y))return false;return true;};
    check(golden.getChildFile("TransparentOD118-off.png").existsAsFile()&&golden.getChildFile("TransparentOD118-on.png").existsAsFile(),"Manually approved golden PNGs are available independently of build working directory");
    check(samePixels(off,ImageFileFormat::loadFrom(golden.getChildFile("TransparentOD118-off.png")))&&samePixels(on,ImageFileFormat::loadFrom(golden.getChildFile("TransparentOD118-on.png"))),"Transparent OD ON/OFF matches original approved skin outside the two intentionally restyled JUCE selector/MIDI surfaces");
    Image ledDetail(Image::ARGB,1200,500,true),ledOff(Image::ARGB,600,500,true),ledOn(Image::ARGB,600,500,true);
    {Graphics g(ledOff);g.setImageResamplingQuality(Graphics::highResamplingQuality);skin.paintLed(g,{50,25,400,400},false);}
    {Graphics g(ledOn);g.setImageResamplingQuality(Graphics::highResamplingQuality);skin.paintLed(g,{50,25,400,400},true);}
    {Graphics g(ledDetail);g.drawImageAt(ledOff,0,0);g.drawImageAt(ledOn,600,0);}writePreview("led-lens-detail.png",ledDetail);
    // Independent ON lens measurement excludes the metal rim and evaluates the
    // bright optical core, not the position of the PNG rectangle or bezel.
    double lightX=0,lightY=0,lightCount=0,supportX=0,supportY=0,supportWeight=0;bool commonBezel=true;
    {Image::BitmapData offPixels(ledOff,Image::BitmapData::readOnly),onPixels(ledOn,Image::BitmapData::readOnly);
    for(int y=0;y<500;++y)for(int x=0;x<600;++x)
    {
        const auto a=offPixels.getPixelColour(x,y),b=onPixels.getPixelColour(x,y);
        const double dx=x+.5-300.5,dy=y+.5-237.;
        if(dx*dx+dy*dy>117.*117.)commonBezel&=a==b;
        if(dx*dx+dy*dy>172.*172.&&a.getAlpha()>0)
        {const auto weight=(double)a.getAlpha();supportX+=(x+.5)*weight;supportY+=(y+.5)*weight;supportWeight+=weight;}
        if(dx*dx+dy*dy<100.*100.&&b.getRed()>240&&b.getGreen()>220&&b.getBlue()>160)
        {lightX+=x+.5;lightY+=y+.5;++lightCount;}
    }}
    check(commonBezel,"ON/OFF retain pixel-identical fixed metallic housing outside the lens aperture");
    check(lightCount>1000&&std::abs(lightX/lightCount-300.5)<8.&&std::abs(lightY/lightCount-237.)<8.,"Optical ON core is concentric with the independently measured metallic hole within source subpixel tolerance");
    check(supportWeight>1000&&std::abs(supportX/supportWeight-300.5)<1.&&std::abs(supportY/supportWeight-237.)<1.,"Actual black support/shadow alpha centroid aligns with the accepted lens axis on both dimensions");
    for(float dpi:{1.f,1.25f,1.5f,2.f})
    {
        card.setSize(300,450);const auto rendered=card.createComponentSnapshot(card.getLocalBounds(),true,dpi);const int percent=roundToInt(dpi*100);
        check(rendered.getWidth()==roundToInt(300*dpi)&&rendered.getHeight()==roundToInt(450*dpi)&&rendered.getPixelAt(rendered.getWidth()/2,rendered.getHeight()/2).getAlpha()>0,"Pilot renders non-empty at "+String(percent)+" percent pixel scale from original assets");
        if(auto stream=output.getChildFile("transparent-od-"+String(percent)+".png").createOutputStream()){stream->setPosition(0);stream->truncate();PNGImageFormat().writeImageToStream(rendered,*stream);}
    }
    for(auto size:{Point<int>(188,240),Point<int>(224,288),Point<int>(300,450),Point<int>(600,900)})
    {
        card.setSize(size.x,size.y);const auto f=skin.faceBounds(card.getLocalBounds());
        bool aligned=true;for(int k=0;k<3;++k){auto* knob=k==0?input:k==1?tone:level;aligned&=knob->getBounds()==skin.logicalToLocal(skin.knobs[(size_t)k],card.getLocalBounds()).toNearestInt();}
        check(std::abs(f.getWidth()/f.getHeight()-300.f/450)<1e-6&&aligned&&card.getLocalBounds().contains(power->getBounds()),"Pilot scales proportionally with JSON-aligned controls at "+String(size.x)+"x"+String(size.y));
        check(look.getComboBoxFont(*preset).getHeight()>=10.f&&look.getTextButtonFont(*midi,midi->getHeight()).getHeight()>=10.f,"Preset/MIDI fonts retain readable minimum at "+String(size.x)+"x"+String(size.y));
    }
    for(int v=0;v<13;++v)if(PedalCard::engineFor(v)!=id)
    {
        PedalCard current(v,processor,false);current.setSize(v==11?360:300,450);current.setVisible(true);current.midi({1,5,1},false,true);
        const auto& variant=tonegrid::assets::pedal(v);
        check(variant.valid&&variant.strip==skin.strip&&variant.ledOn==skin.ledOn&&variant.ledOff==skin.ledOff&&variant.switchUp==skin.switchUp&&variant.switchDown==skin.switchDown&&dynamic_cast<tonegrid::assets::Look*>(&current.getLookAndFeel())!=nullptr,"Pedal "+String(PedalCard::engineFor(v))+" uses the same registered knob, concentric LED and footswitch renderer/assets as the golden master");
        writePreview("pedal-"+String(v)+".png",current.createComponentSnapshot(current.getLocalBounds()));
        if(v==11)for(int band=0;band<7;++band)
        {
            auto* f=dynamic_cast<Slider*>(current.findChildWithID("eq-band-"+String(band)));
            check(f&&f->isVisible()&&f->getMinimum()==-12&&f->getMaximum()==12,"EQ band "+String(band)+" remains visible with unchanged gain bounds");
            if(f){const auto original=processor.get(board::id(board::PedalId::GraphicEq));f->setValue(band-3,sendNotificationSync);check(processor.get(board::id(board::PedalId::GraphicEq)).eqBands[(size_t)band]==band-3,"EQ band "+String(band)+" retains its original DSP binding");processor.set(board::id(board::PedalId::GraphicEq),original);current.sync();}
        }
    }
    for(auto area:{juce::Rectangle<int>(0,0,1332,540),juce::Rectangle<int>(0,0,1632,680),juce::Rectangle<int>(0,0,2212,850)})
    {
        const auto r=tonegrid::assets::legacyBoardBounds(area);bool fits=true,spaced=true,standard=true;
        for(int v=0;v<13;++v){fits&=area.contains(r[(size_t)v]);standard&=r[(size_t)v].getHeight()==r[4].getHeight()&&(v==11||r[(size_t)v].getWidth()==r[4].getWidth());if(v!=0&&v!=7)spaced&=r[(size_t)v].getX()-r[(size_t)v-1].getRight()==6;}
        check(fits&&spaced&&standard&&r[11].getWidth()>r[4].getWidth(),"Two balanced compact rows fit "+String(area.getWidth())+" px with identical standard dimensions, wider EQ and 6px gaps");
    }
    check(tonegrid::assets::background().getWidth()==1942&&tonegrid::assets::background().getHeight()==809,"Approved generated studio PNG is embedded at its original resolution");
    for(auto size:{Point<int>(1332,540),Point<int>(1632,656),Point<int>(2212,656)})
    {
        const juce::Rectangle<int> area(10,10,size.x,size.y);const auto cards=tonegrid::assets::legacyBoardBounds(area);const auto anchors=tonegrid::assets::studioDestinationY(area);
        check(anchors[0]==area.getY()&&anchors[3]==area.getBottom()&&anchors[1]==cards[0].getBottom()+3&&anchors[2]==cards[7].getBottom()+3&&anchors[0]<anchors[1]&&anchors[1]<anchors[2]&&anchors[2]<anchors[3],"Both light bands stay anchored beneath actual row bases at "+String(size.x)+" pixels without global cropping");
        check(area.getBottom()-cards[7].getBottom()>=tonegrid::assets::pedalShadowClearance&&cards[7].getY()-cards[0].getBottom()>=tonegrid::assets::pedalShadowClearance+12,"Both rows reserve space for visible contact shadows at "+String(size.x)+" pixels");
        Image bare(Image::ARGB,size.x+20,size.y+20,true),shaded(Image::ARGB,size.x+20,size.y+20,true);
        {Graphics g(bare);tonegrid::assets::paintStudioBackground(g,area);}
        {Graphics g(shaded);tonegrid::assets::paintLegacyPedalScene(g,area);}
        bool darkened=true;for(const auto& c:cards){const auto a=bare.getPixelAt(c.getCentreX(),c.getBottom()+3),b=shaded.getPixelAt(c.getCentreX(),c.getBottom()+3);darkened&=b.getBrightness()<a.getBrightness();}
        check(darkened&&shaded.getPixelAt(0,0).isTransparent()&&shaded.getPixelAt(size.x+19,size.y+19).isTransparent(),"All thirteen shadows visibly darken their footprint while the scene stays clipped away from other UI at "+String(size.x)+" pixels");
        writePreview("studio-scene-"+String(size.x)+".png",shaded);
    }
    report+=String(checks)+" pilot asset checks; "+(ok?"ALL PASSED":"FAILURES")+"\n";return ok;
}
}
