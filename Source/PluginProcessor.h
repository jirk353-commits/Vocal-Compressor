#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <unordered_map>
#include "DSP.h"

class VocalCompProcessor : public juce::AudioProcessor
{
public:
    VocalCompProcessor();
    ~VocalCompProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Vocal Compressor"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int) override;
    const juce::String getProgramName (int) override;
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    float value (const char* id) const { return apvts.getRawParameterValue (id)->load(); }

    // meters (dBFS / dB), written by the audio thread, read by the editor
    struct Meters
    {
        std::atomic<float> cInL { -100 }, cInR { -100 }, cOutL { -100 }, cOutR { -100 }, cChange { 0 }, cLimit { 0 };
        std::atomic<float> vEnergy { -100 }, vOutL { -100 }, vOutR { -100 }, vGr { 0 }, vLimit { 0 };
    } meters;

private:
    void processChunk (juce::AudioBuffer<float>&);
    float p (const char* id) const { return cache.at (id)->load(); }

    vc::CompSection comp;
    vc::VoxSection vox;
    double fs = 48000;
    int maxBlock = 512, currentProgram = 0;
    std::unordered_map<std::string, std::atomic<float>*> cache;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VocalCompProcessor)
};
