#pragma once
#include "PluginProcessor.h"
#include <juce_gui_extra/juce_gui_extra.h>

class GoaMidiArchitectEditor : public juce::AudioProcessorEditor,
                               public juce::DragAndDropContainer {
public:
    explicit GoaMidiArchitectEditor(GoaMidiArchitectProcessor&);
    ~GoaMidiArchitectEditor() override = default;

    void resized() override;

private:
    GoaMidiArchitectProcessor& processor;
    std::unique_ptr<juce::WebBrowserComponent> webView;

    void handleWebMessage(const juce::String& msg);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GoaMidiArchitectEditor)
};
