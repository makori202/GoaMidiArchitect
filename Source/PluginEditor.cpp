#include "PluginEditor.h"
#include "BinaryData.h"

namespace {
class BridgeWebView : public juce::WebBrowserComponent {
public:
    BridgeWebView(const Options& opts, std::function<void(const juce::String&)> cb)
        : juce::WebBrowserComponent(opts), onBridge(std::move(cb)) {}

    bool pageAboutToLoad(const juce::String& newURL) override {
        if (newURL.startsWith("https://juce.bridge/")) {
            if (onBridge) onBridge(newURL);
            return false;
        }
        return true;
    }

private:
    std::function<void(const juce::String&)> onBridge;
};
}

GoaMidiArchitectEditor::GoaMidiArchitectEditor(GoaMidiArchitectProcessor& p)
    : AudioProcessorEditor(&p), processor(p) {
    auto opts = juce::WebBrowserComponent::Options{}
        .withBackend(juce::WebBrowserComponent::Options::Backend::webview2)
        .withWinWebView2Options(
            juce::WebBrowserComponent::Options::WinWebView2{}
                .withUserDataFolder(juce::File::getSpecialLocation(juce::File::tempDirectory))
        );

    webView = std::make_unique<BridgeWebView>(opts, [this](const juce::String& url) {
        handleBridgeUrl(url);
    });
    addAndMakeVisible(*webView);

    auto html = juce::String::fromUTF8(BinaryData::ui_html, BinaryData::ui_htmlSize);
    auto tempHtml = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("goa_midi_architect_ui.html");
    tempHtml.replaceWithText(html);
    webView->goToURL(juce::URL(tempHtml).toString(false));

    setResizable(true, true);
    setSize(1220, 720);
}

void GoaMidiArchitectEditor::resized() {
    if (webView)
        webView->setBounds(getLocalBounds());
}

void GoaMidiArchitectEditor::handleBridgeUrl(const juce::String& url) {
    juce::URL u(url);
    auto path = url.fromFirstOccurrenceOf("https://juce.bridge/", false, false).upToFirstOccurrenceOf("?", false, false);

    if (path == "sync") {
        auto jsonStr = juce::URL::removeEscapeChars(url.fromFirstOccurrenceOf("data=", false, false));
        processor.updatePatternFromJson("{\"type\":\"sync\",\"payload\":" + jsonStr + "}");
    } else if (path == "drag") {
        auto filename = juce::URL::removeEscapeChars(
            url.fromFirstOccurrenceOf("name=", false, false).upToFirstOccurrenceOf("&", false, false));
        if (filename.isEmpty()) filename = "goa_pattern.mid";
        auto arrStr = juce::URL::removeEscapeChars(url.fromFirstOccurrenceOf("data=", false, false));
        auto parsed = juce::JSON::parse(arrStr);
        if (auto* arr = parsed.getArray()) {
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
