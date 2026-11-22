#include "PluginProcessor.h"
#include "PluginEditor.h"

GoaMidiArchitectProcessor::GoaMidiArchitectProcessor()
    : AudioProcessor(BusesProperties()
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)) {
    pattern.steps.resize(32);
    for (int i = 0; i < 32; ++i) {
        if (i % 4 != 0) {
            pattern.steps[(size_t)i].notes = { (i >= 28) ? 53 : 41 };
            pattern.steps[(size_t)i].vel = 100;
            pattern.steps[(size_t)i].len = 1;
        }
    }
}

void GoaMidiArchitectProcessor::prepareToPlay(double sampleRate, int) {
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    lastTriggeredStep = -1;
    freeRunBeatPos = 0.0;
    activeNotes.clear();
}

void GoaMidiArchitectProcessor::releaseResources() {
    activeNotes.clear();
}

bool GoaMidiArchitectProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo()
        && !layouts.getMainOutputChannelSet().isDisabled())
        return false;
    return true;
}

void GoaMidiArchitectProcessor::updatePatternFromJson(const juce::String& jsonText) {
    auto parsed = juce::JSON::parse(jsonText);
    if (!parsed.isObject()) return;
    auto type = parsed.getProperty("type", "").toString();
    if (type != "sync") return;
    auto p = parsed.getProperty("payload", {});
    if (!p.isObject()) return;

    PatternData next;
    next.bpm = juce::jlimit(60.0, 200.0, (double)p.getProperty("bpm", 142.0));
    next.bars = juce::jlimit(1, 8, (int)p.getProperty("bars", 2));
    next.spb = juce::jlimit(4, 32, (int)p.getProperty("spb", 16));
    next.gatePct = juce::jlimit(10, 100, (int)p.getProperty("gate", 45));
    next.swingPct = juce::jlimit(0, 75, (int)p.getProperty("swing", 0));
    next.humanizePct = juce::jlimit(0, 100, (int)p.getProperty("humanize", 10));
    next.freeRun = (bool)p.getProperty("playing", false);
    auto roleStr = p.getProperty("role", "").toString();
    next.isPad = (roleStr == "PAD" || roleStr == "CHORDS" || roleStr == "ATMOS");

    if (auto* arr = p.getProperty("steps", {}).getArray()) {
        next.steps.resize((size_t)arr->size());
        for (int i = 0; i < arr->size(); ++i) {
            auto item = (*arr)[i];
            if (item.isObject()) {
                next.steps[(size_t)i].vel = juce::jlimit(1, 127, (int)item.getProperty("vel", 100));
                next.steps[(size_t)i].len = juce::jlimit(1, 64, (int)item.getProperty("len", 1));
                if (auto* nArr = item.getProperty("notes", {}).getArray()) {
                    for (auto& nv : *nArr)
                        next.steps[(size_t)i].notes.push_back(juce::jlimit(0, 127, (int)nv));
                }
            }
        }
    }

    std::lock_guard<std::mutex> lock(patternMutex);
    pattern = std::move(next);
}

void GoaMidiArchitectProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) {
    const int numSamples = buffer.getNumSamples();

    PatternData pat;
    {
        std::lock_guard<std::mutex> lock(patternMutex);
        pat = pattern;
    }

    if (pat.steps.empty()) return;

    bool dawPlaying = false;
    double bpm = pat.bpm;
    double ppqStart = freeRunBeatPos;

    if (auto* ph = getPlayHead()) {
        if (auto pos = ph->getPosition()) {
            dawPlaying = pos->getIsPlaying();
            if (pos->getBpm().hasValue() && *pos->getBpm() > 20.0)
                bpm = *pos->getBpm();
            if (dawPlaying && pos->getPpqPosition().hasValue())
                ppqStart = *pos->getPpqPosition();
        }
    }

    const bool active = dawPlaying || pat.freeRun;
    if (!active) {
        for (auto& an : activeNotes)
            midiMessages.addEvent(juce::MidiMessage::noteOff(1, an.first), 0);
        activeNotes.clear();
        lastTriggeredStep = -1;
        return;
    }

    const double beatsPerSample = (bpm / 60.0) / currentSampleRate;
    const double stepsPerBeat = (double)pat.spb / 4.0;
    const int totalSteps = (int)pat.steps.size();
    const double stepDurationSec = (60.0 / bpm) / stepsPerBeat;
    const double gateMult = pat.gatePct / 100.0;
    const double swingFrac = (pat.swingPct / 100.0) * 0.35;

    for (int s = 0; s < numSamples; ++s) {
        for (int i = (int)activeNotes.size() - 1; i >= 0; --i) {
            if (--activeNotes[(size_t)i].second <= 0) {
                midiMessages.addEvent(juce::MidiMessage::noteOff(1, activeNotes[(size_t)i].first), s);
                activeNotes.erase(activeNotes.begin() + i);
            }
        }

        const double curPpq = dawPlaying ? (ppqStart + s * beatsPerSample) : freeRunBeatPos;
        if (!dawPlaying) freeRunBeatPos += beatsPerSample;

        const double rawStepFloat = std::abs(curPpq) * stepsPerBeat;
        const int baseStep = ((int)std::floor(rawStepFloat)) % totalSteps;
        const double stepFrac = rawStepFloat - std::floor(rawStepFloat);
        const double targetOffset = (baseStep % 2 == 1) ? swingFrac : 0.0;

        if (baseStep != lastTriggeredStep && stepFrac >= targetOffset && baseStep >= 0 && baseStep < totalSteps) {
            lastTriggeredStep = baseStep;
            const auto& cell = pat.steps[(size_t)baseStep];
            const int durSamples = juce::jmax(64, (int)(stepDurationSec * (double)cell.len * gateMult * currentSampleRate));
            for (int note : cell.notes) {
                int vel = cell.vel;
                if (pat.humanizePct > 0) {
                    int delta = (int)(((baseStep * 37 + note * 13) % 21) - 10) * pat.humanizePct / 100;
                    vel = juce::jlimit(25, 127, vel + delta);
                }
                midiMessages.addEvent(juce::MidiMessage::noteOn(1, note, (juce::uint8)vel), s);
                activeNotes.push_back({ note, durSamples });
            }
        }
    }
}

juce::AudioProcessorEditor* GoaMidiArchitectProcessor::createEditor() {
    return new GoaMidiArchitectEditor(*this);
}

void GoaMidiArchitectProcessor::getStateInformation(juce::MemoryBlock&) {}
void GoaMidiArchitectProcessor::setStateInformation(const void*, int) {}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new GoaMidiArchitectProcessor();
}
