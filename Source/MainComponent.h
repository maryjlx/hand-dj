#pragma once

#include <JuceHeader.h>
#include <thread>
#include <atomic>
#include "SerialPort.h"

//==============================================================================
class PitchShifter
{
public:
    void prepare(double sampleRate, int samplesPerBlock)
    {
        this->sampleRate = sampleRate;
        buffer.setSize(2, samplesPerBlock * 4); // Larger buffer for better quality
        buffer.clear();
        writePos = 0;
    }
    
    void setPitchRatio(float ratio) { pitchRatio = juce::jlimit(0.5f, 2.0f, ratio); }
    
    void process(juce::AudioBuffer<float>& inputBuffer)
    {
        if (std::abs(pitchRatio - 1.0f) < 0.001f)
            return;
        
        int numSamples = inputBuffer.getNumSamples();
        int numChannels = inputBuffer.getNumChannels();
        
        for (int channel = 0; channel < numChannels; ++channel)
        {
            auto* input = inputBuffer.getReadPointer(channel);
            auto* output = inputBuffer.getWritePointer(channel);
            
            for (int i = 0; i < numSamples; ++i)
            {
                float readPos = i * pitchRatio;
                int pos1 = (int)readPos;
                float frac = readPos - pos1;
                
                if (pos1 + 1 < numSamples)
                {
                    output[i] = input[pos1] * (1.0f - frac) + input[pos1 + 1] * frac;
                }
            }
        }
    }
    
private:
    float pitchRatio = 1.0f;
    double sampleRate = 44100.0;
    juce::AudioBuffer<float> buffer;
    int writePos = 0;
};


class MainComponent  : public juce::AudioAppComponent,
                    private juce::Timer
{
public:
    MainComponent();
    ~MainComponent() override;
    void timerCallback() override;

    void prepareToPlay (int samplesPerBlockExpected, double sampleRate) override;
    void getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill) override;
    void releaseResources() override;
    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    std::atomic<float> speed { 1.0f };    //speed from hand distance
    std::atomic<float> volume { 0.5f };   //volume control
    std::atomic<float> distance { 0.0f }; //distance from Python
    std::atomic<float> pitch{1.0f}; 
    std::atomic<bool> running { true };
    SerialPort serialPort;

    std::thread pythonThread;
    
    juce::AudioFormatManager formatManager;
    std::unique_ptr<juce::AudioFormatReaderSource> readerSource;
    juce::AudioTransportSource transportSource;
    juce::ResamplingAudioSource resamplingSource{&transportSource, false, 2};
        
    void loadAudioFile();
    PitchShifter pitchShifter;
    
    std::thread arduinoThread;
    void readArduinoData();
    void loadSongByName(const juce::String& songName);
    juce::File musicFolder;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
