#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>

class GoaFXProcessor : public juce::AudioProcessor
{
public:
    GoaFXProcessor();
    ~GoaFXProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "GoaFX"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    struct Params
    {
        int mode = 0, wave = 0, unison = 1, filterType = 0, delayDiv = 2;
        float length = 4.f, range = 24.f, detune = 10.f, noise = 0.5f, osc = 0.5f;
        float cutoffMax = 12000.f, res = 0.3f, drive = 0.2f;
        float delayMix = 0.2f, delayFb = 0.5f, reverbMix = 0.3f, reverbSize = 0.7f, volume = 0.f;
    };

    struct Voice
    {
        bool active = false, released = false;
        int note = 60;
        float vel = 1.f;
        double age = 0.0, relAge = 0.0;
        double phase[7] = { 0, 0, 0, 0, 0, 0, 0 };
        float ic1 = 0.f, ic2 = 0.f;
        float shPitch = 0.f, shCut = 0.5f;
        double stepCounter = 0.0, lfoPhase = 0.0;
    };

    void readParams();
    void noteOn (int note, float vel);
    void noteOff (int note);
    float renderVoice (Voice& v);
    void renderRange (float* out, int start, int end);

    Params cur;
    Voice voices[8];
    double sr = 44100.0;
    double bpm = 145.0;
    double stepLenSamples = 4000.0;
    juce::Random rng;
    juce::Reverb reverb;
    std::vector<float> delayL, delayR;
    int delayPos = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GoaFXProcessor)
};
