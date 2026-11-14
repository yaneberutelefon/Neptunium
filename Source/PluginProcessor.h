#pragma once
#include <JuceHeader.h>

class NeptuniumProcessor : public juce::AudioProcessor,
                           private juce::AudioProcessorValueTreeState::Listener
{
public:
    struct Info
    {
        std::vector<float> peaks;
        float offsetFrac = 0.0f;
        double offsetSeconds = 0.0;
        juce::String name;
        juce::File outFile;
        bool hasFile = false;
    };

    NeptuniumProcessor();
    ~NeptuniumProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Neptunium"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    bool loadFile (const juce::File& file);
    void setPreview (bool on);
    bool isPreviewing() const { return playing.load(); }
    bool isBusy() const { return busy.load(); }
    int getVersion() const { return version.load(); }
    Info getInfo() const { const juce::ScopedLock sl (lock); return info; }

    juce::AudioProcessorValueTreeState apvts;

private:
    struct Worker : public juce::Thread
    {
        explicit Worker (NeptuniumProcessor& o) : juce::Thread ("Neptunium render"), owner (o) {}
        void run() override;
        NeptuniumProcessor& owner;
    };

    void parameterChanged (const juce::String&, float) override { worker.notify(); }
    void doRender();

    mutable juce::CriticalSection lock;
    juce::AudioBuffer<float> source;
    double sourceRate = 44100.0;
    juce::String sourceName;
    Info info;

    juce::CriticalSection previewLock;
    juce::AudioBuffer<float> previewBuf;
    std::atomic<bool> playing { false };
    std::atomic<bool> busy { false };
    std::atomic<int> playPos { 0 };
    std::atomic<int> version { 0 };
    std::atomic<double> hostRate { 0.0 };

    Worker worker;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NeptuniumProcessor)
};
