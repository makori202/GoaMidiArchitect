#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>
#include <mutex>

struct StepCell {
    std::vector<int> notes;
    int vel = 100;
    int len = 1;
};

struct PatternData {
    double bpm = 142.0;
    int bars = 2;
    int spb = 16;
    int gatePct = 45;
    int swingPct = 0;
    int humanizePct = 10;
    bool freeRun = false;
    bool isPad = false;
    std::vector<StepCell> steps;
};

class GoaMidiArchitectProcessor : public juce::AudioProcessor {
public:
    GoaMidiArchitectProcessor();
    ~GoaMidiArchitectProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return true; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    void updatePatternFromJson(const juce::String& jsonText);

private:
    std::mutex patternMutex;
    PatternData pattern;
    double currentSampleRate = 44100.0;
    double freeRunBeatPos = 0.0;
    int lastTriggeredStep = -1;
    std::vector<std::pair<int, int>> activeNotes;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GoaMidiArchitectProcessor)
};
