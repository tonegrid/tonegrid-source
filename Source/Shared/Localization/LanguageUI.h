#pragma once
#include "Shared/Localization/Language.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <algorithm>
namespace tonegrid::l10n
{
// Explicit UI opt-in, never a blanket traversal of labels or metadata. A binding
// owns the English source, not the currently displayed translation. SafePointer
// entries are pruned, so closed dialogs cannot be called by a later change.
class UiBindings final
{
    struct Item{juce::Component::SafePointer<juce::Component> target;juce::String field,source;std::function<void(const juce::String&)> apply;};
public:
    static UiBindings& instance(){static UiBindings value;return value;}
    void bind(juce::Component& component,juce::String field,juce::String source,std::function<void(const juce::String&)> apply)
    {
        items.erase(std::remove_if(items.begin(),items.end(),[](const Item& i){return i.target==nullptr;}),items.end());
        for(auto& item:items)if(item.target==&component&&item.field==field){item.source=std::move(source);item.apply=std::move(apply);item.apply(tonegrid::l10n::translate(item.source));return;}
        items.push_back({&component,std::move(field),std::move(source),std::move(apply)});auto& item=items.back();item.apply(tonegrid::l10n::translate(item.source));
    }
private:
    UiBindings():subscription([this]{auto copy=items;for(const auto& item:copy)if(item.target)item.apply(tonegrid::l10n::translate(item.source));}){}
    std::vector<Item> items;Subscription subscription;
};
inline void text(juce::Label& label,const juce::String& source,juce::NotificationType notification=juce::dontSendNotification)
{
    const auto safe=juce::Component::SafePointer<juce::Label>(&label);
    UiBindings::instance().bind(label,"text",source,[safe](const juce::String& value){if(safe)safe->setText(value,juce::dontSendNotification);});
    juce::ignoreUnused(notification); // Translation never triggers editing/audio callbacks.
}
inline void button(juce::Button& target,const juce::String& source)
{
    const auto safe=juce::Component::SafePointer<juce::Button>(&target);
    UiBindings::instance().bind(target,"button",source,[safe](const juce::String& value){if(safe)safe->setButtonText(value);});
}
inline void text(juce::TextEditor& editor,const juce::String& source,juce::NotificationType notification=juce::dontSendNotification)
{
    // Read-only guidance/status only. Editable names and user content stay plain.
    const auto safe=juce::Component::SafePointer<juce::TextEditor>(&editor);
    UiBindings::instance().bind(editor,"text",source,[safe](const juce::String& value){if(safe&&safe->isReadOnly())safe->setText(value,false);});
    juce::ignoreUnused(notification);
}
template<class C> void tooltip(C& target,const juce::String& source)
{
    const auto safe=juce::Component::SafePointer<C>(&target);
    UiBindings::instance().bind(target,"tooltip",source,[safe](const juce::String& value){if(safe)safe->setTooltip(value);});
}
template<class C> void formattedTooltip(C& target,const juce::String& key,Arguments arguments)
{
    const auto safe=juce::Component::SafePointer<C>(&target);
    UiBindings::instance().bind(target,"tooltip",Language::instance().source(key),[safe,arguments=std::move(arguments)](const juce::String& pattern){if(safe)safe->setTooltip(substitute(pattern,arguments));});
}
inline void formattedText(juce::Label& target,const juce::String& key,Arguments arguments)
{
    const auto safe=juce::Component::SafePointer<juce::Label>(&target);
    UiBindings::instance().bind(target,"text",Language::instance().source(key),[safe,arguments=std::move(arguments)](const juce::String& pattern){if(safe)safe->setText(substitute(pattern,arguments),juce::dontSendNotification);});
}
inline void placeholder(juce::TextEditor& target,const juce::String& source,juce::Colour colour)
{
    const auto safe=juce::Component::SafePointer<juce::TextEditor>(&target);
    UiBindings::instance().bind(target,"placeholder",source,[safe,colour](const juce::String& value){if(safe)safe->setTextToShowWhenEmpty(value,colour);});
}
inline void item(juce::ComboBox& target,const juce::String& source,int id)
{
    target.addItem(tonegrid::l10n::translate(source),id);const auto safe=juce::Component::SafePointer<juce::ComboBox>(&target);
    UiBindings::instance().bind(target,"item-"+juce::String(id),source,[safe,id](const juce::String& value){if(safe&&safe->indexOfItemId(id)>=0){const bool selected=safe->getSelectedId()==id;safe->changeItemText(id,value);if(selected)safe->setSelectedId(id,juce::dontSendNotification);}});
}
inline void emptyText(juce::ComboBox& target,const juce::String& source)
{
    const auto safe=juce::Component::SafePointer<juce::ComboBox>(&target);
    UiBindings::instance().bind(target,"empty",source,[safe](const juce::String& value){if(safe)safe->setTextWhenNothingSelected(value);});
}
class TextButton:public juce::TextButton
{
public:
    TextButton()=default;
    explicit TextButton(const juce::String& source):juce::TextButton(source){button(*this,source);}
    void setButtonText(const juce::String& source){button(*this,source);}
};
// The native spelling of each language is visible whatever the current locale.
class LanguageChoice final:public juce::Component
{
public:
    explicit LanguageChoice(bool numbered=false):prefix(numbered?"1. ":""),subscription([this]{refresh();})
    {
        addAndMakeVisible(label);addAndMakeVisible(choices);label.setComponentID("language-label");choices.setComponentID("language-choice");
        choices.onChange=[this]{const int i=choices.getSelectedId()-1;if(i>=0&&i<int(locales.size()))Language::instance().select(locales[size_t(i)]);};refresh();
    }
    void refresh()
    {
        label.setText(prefix+tr("language.choose"),juce::dontSendNotification);
        const auto list=Language::instance().available();choices.clear(juce::dontSendNotification);locales.clear();
        // English first; the other packs retain a deterministic locale order.
        const auto add=[&](const Pack& pack){locales.push_back(pack.locale);choices.addItem(pack.nativeName,int(locales.size()));if(pack.locale==Language::instance().locale())choices.setSelectedId(int(locales.size()),juce::dontSendNotification);};
        for(const auto& pack:list)if(pack.locale=="en")add(pack);for(const auto& pack:list)if(pack.locale!="en")add(pack);
        choices.setTooltip(tr("language.help"));resized();repaint();
    }
    void resized()override{auto area=getLocalBounds();label.setBounds(area.removeFromTop(28));choices.setBounds(area.removeFromTop(34));}
private:juce::Label label;juce::ComboBox choices;std::vector<juce::String> locales;juce::String prefix;Subscription subscription;
};
}
