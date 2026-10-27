#pragma once
#include "Shared/UI/PopupStyle.h"
#include "Shared/UI/PopupInteraction.h"
#include <ToneGridBranding.h>

namespace tonegrid::popup
{
inline juce::Rectangle<int> fittedBounds(juce::Rectangle<int> workArea,juce::Point<int> desired,juce::Point<int> centre)
{
    const auto available=workArea.reduced(juce::jmax(8,workArea.getWidth()/20),juce::jmax(8,workArea.getHeight()/20));
    return juce::Rectangle<int>(0,0,juce::jmin(desired.x,available.getWidth()),juce::jmin(desired.y,available.getHeight()))
        .withCentre(centre).constrainedWithin(available);
}

class ResizeBorder final:public juce::ResizableBorderComponent
{
public:
    ResizeBorder(juce::Component& window,juce::ComponentBoundsConstrainer* limits):ResizableBorderComponent(&window,limits)
    {setBorderThickness(juce::BorderSize<int>(12));setComponentID("popup-resize-border");setAlwaysOnTop(true);}
    void paint(juce::Graphics&) override {}
};

class Branding final : public juce::Component
{
public:
    Branding()
    {
        mark = part("logo-mark"); setComponentID("popup-brand-mark");
        setInterceptsMouseClicks(false, false);
    }
    void paint(juce::Graphics& g) override
    {
        draw(g, mark.get(), {0, 0, 34, 34});
    }
private:
    static std::unique_ptr<juce::Drawable> part(const juce::String& id)
    {
        auto svg = juce::XmlDocument::parse(juce::String::fromUTF8(ToneGridBranding::ToneGrid_logo_transparent_svg,
                                                                ToneGridBranding::ToneGrid_logo_transparent_svgSize));
        if (!svg) return {};
        auto* group = svg->getChildByName("g"); if (!group) return {};
        for (int i = group->getNumChildElements() - 1; i >= 0; --i)
        {
            auto* child = group->getChildElement(i);
            if (child->getStringAttribute("id") != id) group->removeChildElement(child, true);
        }
        auto result = juce::Drawable::createFromSVG(*svg);
        if (result) result->replaceColour(juce::Colours::black, juce::Colours::white);
        return result;
    }
    static void draw(juce::Graphics& g, juce::Drawable* drawable, juce::Rectangle<float> bounds)
    {
        if (drawable) drawable->draw(g, .9f, juce::RectanglePlacement(juce::RectanglePlacement::centred)
                                    .getTransformToFit(drawable->getOutlineAsPath().getBounds(), bounds));
    }
    std::unique_ptr<juce::Drawable> mark;
};

class CloseButton final : public juce::Button
{
public:
    explicit CloseButton(bool localised=true) : Button("Close") { setComponentID("popup-close"); setTitle("Close window"); if(localised)l10n::tooltip(*this,"Close (Esc)");else setTooltip("Close (Esc)"); }
private:
    void paintButton(juce::Graphics& g, bool hover, bool down) override
    {
        const auto centre = getLocalBounds().getCentre().toFloat();
        g.setColour(ink().withAlpha(down ? .5f : hover ? 1.f : .75f));
        g.drawLine(centre.x - 5, centre.y - 5, centre.x + 5, centre.y + 5, 1.3f);
        g.drawLine(centre.x - 5, centre.y + 5, centre.x + 5, centre.y - 5, 1.3f);
        if (hasKeyboardFocus(true)) g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(1), 5, 1);
    }
};

// Non-owned persistent views detach on close. Owned short-lived views are deleted
// by the viewport. Rendering and window lifetime are independent of view actions.
class Frame final : public View
{
public:
    Frame(juce::Component& content, bool owned, juce::String name, int minimumHeight)
        : body(content), title(std::move(name)), preferredWidth(content.getWidth()),
          preferredHeight(content.getHeight()), minimumBodyHeight(minimumHeight > 0 ? minimumHeight : preferredHeight),closeButton(localised(content)),
          tooltip(this, 600)
    {
        setComponentID("popup-frame"); addAndMakeVisible(branding); addAndMakeVisible(closeButton); addAndMakeVisible(viewport);
        addAndMakeVisible(titleLabel); titleLabel.setComponentID("popup-title");
        const auto safeTitle=juce::Component::SafePointer<juce::Label>(&titleLabel);
        if(localised(content))l10n::UiBindings::instance().bind(titleLabel,"text",title,[safeTitle](const juce::String& value){if(safeTitle)safeTitle->setText(value.toUpperCase(),juce::dontSendNotification);});
        else titleLabel.setText(title.toUpperCase(),juce::dontSendNotification);
        titleLabel.setFont(ui::font(22));
        titleLabel.setColour(juce::Label::textColourId, ink()); titleLabel.setBorderSize({});
        titleLabel.setInterceptsMouseClicks(false, false);
        if (auto* view = dynamic_cast<View*>(&body)){headerAction = view->popupHeaderAction();branding.setVisible(view->popupBranding());cornerRadius=view->popupCornerRadius();borderWidth=view->popupBorderWidth();}
        if (headerAction) addAndMakeVisible(headerAction);
        if(auto* view=dynamic_cast<View*>(&body))footerActions=view->popupFooterActions();
        if(!footerActions.empty())
        {
            bodyFocus=body.isKeyboardFocusContainer()?juce::Component::FocusContainerType::keyboardFocusContainer:body.isFocusContainer()?juce::Component::FocusContainerType::focusContainer:juce::Component::FocusContainerType::none;
            body.setFocusContainerType(juce::Component::FocusContainerType::none);setFocusContainerType(juce::Component::FocusContainerType::keyboardFocusContainer);
        }
        footer.setComponentID("popup-footer");addChildComponent(footer);footer.setVisible(!footerActions.empty());
        for(auto* action:footerActions)footer.addChildComponent(action);
        viewport.setViewedComponent(&body, owned); viewport.setScrollBarsShown(true, true); viewport.setScrollBarThickness(10);
        closeButton.onClick = [this] { if (auto* dialog = findParentComponentOfClass<juce::DialogWindow>()) dialog->closeButtonPressed(); };
        constrainer.setMinimumOnscreenAmounts(0x10000, 48, 48, 48);
        setSize(preferredWidth + 84, preferredHeight + 148+footerHeight());
    }
    ~Frame() override
    {
        // The action belongs to the persistent body, including when its callback
        // synchronously closes this window to open Tone3000. Never delete it here.
        if (headerAction) { body.addAndMakeVisible(headerAction); body.resized(); }
        for(auto* action:footerActions)body.addChildComponent(action);if(!footerActions.empty()){body.setFocusContainerType(bodyFocus);body.resized();}
    }
    // Opt-in for wizard steps of different heights. Other popups retain their size.
    void setBodyHeight(int height)
    {preferredHeight=minimumBodyHeight=height;viewport.setViewPosition(0,0);resized();}
    void enableResizing(juce::Component& window,juce::ComponentBoundsConstrainer* limits)
    {resizeBorder=std::make_unique<ResizeBorder>(window,limits);addAndMakeVisible(*resizeBorder);resized();}
    int minimumWindowHeight(int width) const{return headerHeight(width-36)+164+footerHeight();}
    void paint(juce::Graphics& g) override
    {
        g.drawImageAt(shadow, 0, 0);
        const auto bounds = getLocalBounds().reduced(18);
        // Draw the opaque panel into the window's renderer, as the tuner does.
        // A native image cache can disappear in a transparent Windows peer.
        const auto r = bounds.toFloat();
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xff1b2022), r.getX() + r.getWidth() * .4f, r.getY() + 110,
                                             juce::Colour(0xff080c0e), r.getRight(), r.getBottom(), true));
        g.fillRoundedRectangle(r, cornerRadius);
        g.setColour(juce::Colour(0xff384347)); g.drawRoundedRectangle(r, cornerRadius, borderWidth);
        g.setColour(outline().withAlpha(.32f));
        g.drawHorizontalLine(bounds.getY() + headerHeight(bounds.getWidth()), float(bounds.getX() + 24), float(bounds.getRight() - 24));
        g.setColour(mutedInk().withAlpha(.6f));
        for(int i=0;i<3;++i){const float edge=float(bounds.getRight()-6),bottom=float(bounds.getBottom()-6),length=float(3+4*i);g.drawLine(edge-length,bottom,edge,bottom-length,1.f);}
    }
    void resized() override
    {
        const auto bounds = getLocalBounds().reduced(18);
        branding.setBounds(bounds.getX() + 24, bounds.getY() + 21, 34, 34);
        closeButton.setBounds(bounds.getRight() - 36, bounds.getY() + 12, 24, 24);
        const int titleLeft = branding.isVisible()?branding.getRight()+14:bounds.getX()+24;
        int titleRight = closeButton.getX() - 16;
        if (headerAction)
        {
            const auto* view=dynamic_cast<View*>(&body);const bool stacked=headerHeight(bounds.getWidth())>76;
            const int width=stacked?bounds.getWidth()-48:view?view->popupHeaderWidth(bounds.getWidth()-78):300;
            headerAction->setBounds(stacked?bounds.getX()+24:bounds.getRight()-54-width,bounds.getY()+(stacked?66:17),width,view?view->popupHeaderHeight(width):42);
            if(!stacked)titleRight = headerAction->getX() - 16;
        }
        titleLabel.setBounds(titleLeft, bounds.getY() + 21, juce::jmax(0, titleRight - titleLeft), 34);
        viewport.setBounds(bounds.reduced(24).withTrimmedTop(headerHeight(bounds.getWidth())-12).withTrimmedBottom(footerHeight()));
        // Lists shrink vertically on smaller monitors. Fixed control panels scroll
        // only when needed, keeping all controls reachable without shrinking text.
        const auto* view=dynamic_cast<const View*>(&body);
        const int minimumWidth=view&&view->popupMinimumWidth()>0?view->popupMinimumWidth():preferredWidth;
        const int width = juce::jmax(minimumWidth, viewport.getWidth()-viewport.getScrollBarThickness());
        const int naturalHeight=view?view->popupContentHeight(width):0;
        const int height = naturalHeight>0?naturalHeight:juce::jmax(minimumBodyHeight, viewport.getMaximumVisibleHeight());
        body.setSize(width, height);
        body.resized(); // Reparenting the header action also changes body layout.
        if(!footerActions.empty())
        {
            footer.setBounds(bounds.getX()+24,bounds.getBottom()-24-footerHeight()+12,bounds.getWidth()-48,footerHeight()-12);
            const int leftWidth=juce::jmin(112,footer.getWidth()/3),rightWidth=juce::jmin(206,footer.getWidth()-leftWidth-16);
            footerActions.front()->setBounds(0,8,leftWidth,34);
            if(footerActions.size()>1)footerActions.back()->setBounds(footer.getWidth()-rightWidth,8,rightWidth,34);
        }
        if(resizeBorder)resizeBorder->setBounds(bounds);
        // Only the expensive blur is cached, with CPU pixels that can be uploaded
        // by either Windows renderer. The body never depends on this image.
        shadow = juce::Image(juce::Image::ARGB, getWidth(), getHeight(), true, juce::SoftwareImageType{});
        juce::Graphics g(shadow); juce::Path shape; shape.addRoundedRectangle(bounds.toFloat(), cornerRadius);
        juce::DropShadow(juce::Colours::black.withAlpha(.42f), 14, {0, 4}).drawForPath(g, shape);
    }
    void mouseDown(const juce::MouseEvent& event) override
    {
        dragging = event.position.y < 90;
        if (dragging) dragger.startDraggingComponent(getTopLevelComponent(), event);
    }
    void mouseDrag(const juce::MouseEvent& event) override
    { if (dragging) dragger.dragComponent(getTopLevelComponent(), event, &constrainer); }
private:
    static bool localised(juce::Component& c){const auto* view=dynamic_cast<const View*>(&c);return !view||view->popupLocalised();}
    int footerHeight()const{return footerActions.empty()?0:64;}
    int headerHeight(int width) const{const auto* view=dynamic_cast<const View*>(&body);return view&&view->popupStackHeader(width)?86+view->popupHeaderHeight(width-48):76;}
    juce::Component& body;
    juce::String title;
    int preferredWidth, preferredHeight, minimumBodyHeight;
    juce::Viewport viewport;
    Branding branding;
    juce::Label titleLabel;
    juce::Component* headerAction = nullptr;
    juce::Component footer;std::vector<juce::Component*> footerActions;
    juce::Component::FocusContainerType bodyFocus=juce::Component::FocusContainerType::none;
    CloseButton closeButton;
    juce::TooltipWindow tooltip;
    juce::Image shadow;
    juce::ComponentDragger dragger;
    juce::ComponentBoundsConstrainer constrainer;
    std::unique_ptr<ResizeBorder> resizeBorder;
    bool dragging = false;
    float cornerRadius=20.f,borderWidth=1.5f;
};

class Window final : public DismissibleDialog
{
public:
    Window(const juce::String& title, juce::Component& content, juce::Component& owner, bool owned, int minimumHeight)
        : DismissibleDialog(title,owner)
    {
        setUsingNativeTitleBar(false); setTitleBarHeight(0); setTitleBarButtonsRequired(0, false);
        setOpaque(false); setResizable(true, false); setDropShadowEnabled(false);
        auto* frame = new Frame(content, owned, title, minimumHeight); setContentOwned(frame, true);
        frame->enableResizing(*this,getConstrainer());
        const auto area = owner.getParentMonitorArea();
        const float scale = juce::Component::getApproximateScaleFactorForComponent(this);
        const auto logicalArea=area.toFloat().transformedBy(juce::AffineTransform::scale(1.f/scale)).toNearestInt();
        setResizeLimits(juce::jmin(360,logicalArea.getWidth()),juce::jmin(frame->minimumWindowHeight(360),logicalArea.getHeight()),32768,32768);
        getConstrainer()->setMinimumOnscreenAmounts(0x10000,48,48,48);
        setBounds(fittedBounds(logicalArea,{getWidth(),getHeight()},owner.getScreenBounds().getCentre().toFloat().transformedBy(juce::AffineTransform::scale(1.f/scale)).roundToInt()));
    }
    juce::BorderSize<int> getBorderThickness() const override { return {}; }
    void resizeToBody(int height)
    {
        if(auto* frame=dynamic_cast<Frame*>(getContentComponent()))frame->setBodyHeight(height);
        // Preserve a manually resized window when a wizard step/advanced panel
        // changes. The viewport makes the newly expanded content reachable.
    }
    void closeButtonPressed() override { exitModalState(0); setVisible(false); }
};

inline juce::DialogWindow* launch(const juce::String& title, juce::Component& content, juce::Component& owner,
                                bool owned = false, int minimumHeight = 0, juce::ModalComponentManager::Callback* callback = nullptr)
{
    auto* window = new Window(title, content, owner, owned, minimumHeight);
    window->enterModalState(true, callback, true);
    return window;
}

class Prompt final : public View
{
public:
    Prompt(const juce::String& message, const juce::String& action, const juce::String* initialName = nullptr)
    {
        addAndMakeVisible(description); l10n::text(description,message);
        description.setFont(juce::FontOptions("Segoe UI", 15.f, juce::Font::plain)); description.setJustificationType(juce::Justification::topLeft);
        if (initialName)
        {
            addAndMakeVisible(editor); editor.setComponentID("preset-name"); editor.setText(*initialName);
            editor.setFont(juce::FontOptions("Segoe UI", 16.f, juce::Font::plain));
            editor.onReturnKey = [this] { finish(1); }; editor.onEscapeKey = [this] { finish(0); };
            confirm.addShortcut(juce::KeyPress(juce::KeyPress::returnKey));
        }
        editor.setVisible(initialName != nullptr);
        for (auto* button : {&cancel, &confirm}) addAndMakeVisible(button);
        l10n::button(confirm,action); confirm.setComponentID("prompt-confirm"); cancel.setComponentID("prompt-cancel");
        confirm.onClick = [this] { finish(1); }; cancel.onClick = [this] { finish(0); };
        cancel.addShortcut(juce::KeyPress(juce::KeyPress::escapeKey));
        juce::AttributedString copy;copy.append(l10n::translate(message),ui::font(15),ink());juce::TextLayout layout;layout.createLayout(copy,480);
        descriptionHeight=juce::jmax(60,juce::roundToInt(layout.getHeight())+8);
        setSize(480,(initialName ? 210 : 164)+descriptionHeight-60);
    }
    juce::String name() const { return editor.getText(); }
    void setCancelText(const juce::String& text){cancel.setButtonText(text);}
    void focusName() { if (editor.isVisible()) { editor.grabKeyboardFocus(); editor.selectAll(); } }
    void resized() override
    {
        description.setBounds(0, 0, getWidth(), descriptionHeight);
        editor.setBounds(0, descriptionHeight+16, getWidth(), 40);
        const int y = getHeight() - 40;
        const int confirmWidth=juce::jmin(260,getWidth()/2+20);cancel.setBounds(0,y,juce::jmin(160,getWidth()-confirmWidth-16),38);confirm.setBounds(getWidth()-confirmWidth,y,confirmWidth,38);
    }
private:
    void finish(int result)
    {
        if (auto* window = findParentComponentOfClass<juce::DialogWindow>())
        { window->exitModalState(result); window->setVisible(false); }
    }
    juce::Label description;
    int descriptionHeight=60;
    juce::TextEditor editor;
    tonegrid::l10n::TextButton cancel{"Cancel"}, confirm;
};
}
