#include "PluginEditor.h"
#include "BinaryData.h"

GoaMidiArchitectEditor::GoaMidiArchitectEditor(GoaMidiArchitectProcessor& p)
    : AudioProcessorEditor(&p), processor(p) {
    auto opts = juce::WebBrowserComponent::Options{}
        .withBackend(juce::WebBrowserComponent::Options::Backend::webview2)
        .withWinWebView2Options(
            juce::WebBrowserComponent::Options::WinWebView2{}
                .withUserDataFolder(juce::File::getSpecialLocation(juce::File::tempDirectory))
        )
        .withEventListener("message", [this](const juce::var& v) {
            handleWebMessage(v.toString());
        });

    webView = std::make_unique<juce::WebBrowserComponent>(opts);
    addAndMakeVisible(*webView);

    auto html = juce::String::fromUTF8(BinaryData::ui_html, BinaryData::ui_htmlSize);
    auto tempHtml = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("goa_midi_architect_ui.html");
    tempHtml.replaceWithText(html);
    webView->goToURL(juce::URL(tempHtml).toString(false));

    setResizable(true, true);
    setSize(1240, 760);
}

void GoaMidiArchitectEditor::resized() {
    if (webView)
        webView->setBounds(getLocalBounds());
}

void GoaMidiArchitectEditor::handleWebMessage(const juce::String& msg) {
    auto parsed = juce::JSON::parse(msg);
    if (!parsed.isObject()) return;
    auto type = parsed.getProperty("type", "").toString();

    if (type == "sync") {
        processor.updatePatternFromJson(msg);
    } else if (type == "dragMidi") {
        auto filename = parsed.getProperty("filename", "goa_pattern.mid").toString();
        if (auto* arr = parsed.getProperty("bytes", {}).getArray()) {
            juce::MemoryBlock mb;
            for (auto& b : *arr) {
                juce::uint8 byteVal = (juce::uint8)(int)b;
                mb.append(&byteVal, 1);
            }
            auto outFile = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile(filename);
            outFile.replaceWithData(mb.getData(), mb.getSize());
            performExternalDragDropOfFiles({ outFile.getFullPathName() }, false);
        }
    }
}
