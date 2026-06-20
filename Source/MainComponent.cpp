#include "MainComponent.h"
#include <JuceHeader.h>
#include <opencv2/opencv.hpp>
#include <thread>
#include <atomic>
#include <mutex>
#include <iostream>
#include <cstdio>
using namespace cv;

//==============================================================================
MainComponent::MainComponent()
: resamplingSource(&transportSource, false, 2)
{
    setSize (1280, 720);
    setAudioChannels (0, 2);
    formatManager.registerBasicFormats();
        
    musicFolder = juce::File("/Users/mary/Desktop/dj_hand_project/songs");
        
    arduinoThread = std::thread([this]() { readArduinoData(); });
    pythonThread = std::thread([this]()
    {
        juce::ChildProcess python;
        
        std::cout << "Starting Python process..." << std::endl;
        
        juce::String pythonCommand = "/Users/mary/Desktop/dj_hand_project/venv/bin/python -u /Users/mary/Desktop/dj_hand_project/hand_distance_stream.py";
        
        if (!python.start(pythonCommand))
        {
            std::cout << "Failed to start Python!" << std::endl;
            return;
        }
        
        std::cout << "Python process started!" << std::endl;
        
        juce::String buffer;
        
        while (running && python.isRunning())
        {
            char tempBuffer[1024];
            int bytesRead = python.readProcessOutput(tempBuffer, sizeof(tempBuffer) - 1);
            
            if (bytesRead > 0)
            {
                tempBuffer[bytesRead] = '\0';
                buffer += juce::String(tempBuffer);
                
                // Process complete lines
                while (buffer.contains("\n"))
                {
                    auto newlinePos = buffer.indexOf("\n");
                    auto line = buffer.substring(0, newlinePos).trim();
                    buffer = buffer.substring(newlinePos + 1);
                    
                    if (line.isNotEmpty())
                    {
                        std::cout << "Received: " << line << std::endl;
                        
                        try
                        {
                            auto tokens = juce::StringArray::fromTokens(line, " ", "");
                            
                            if (tokens.size() >= 3)
                            {
                                float speedDist = tokens[0].getFloatValue();
                                float pitchDist = tokens[1].getFloatValue();
                                float volumeDist = tokens[2].getFloatValue();
                                
                                std::cout << "Parsed values - Speed: " << speedDist
                                         << " Pitch: " << pitchDist
                                         << " Volume: " << volumeDist << std::endl;
                                
                                if (speedDist > 0)
                                {
                                    float newSpeed = juce::jmap(speedDist, 0.0f, 300.0f, 0.5f, 2.0f);
                                    newSpeed = juce::jlimit(0.5f, 2.0f, newSpeed);
                                    speed.store(newSpeed);
                                    std::cout << "Speed set to: " << newSpeed << std::endl;
                                }
                                
                                if (pitchDist > 0)
                                {
                                    float newPitch = juce::jmap(pitchDist, 0.0f, 300.0f, 0.5f, 2.0f);
                                    newPitch = juce::jlimit(0.5f, 2.0f, newPitch);
                                    pitch.store(newPitch);
                                    std::cout << "Pitch set to: " << newPitch << std::endl;
                                }
                                
                                if (volumeDist > 0)
                                {
                                    float newVolume = juce::jmap(volumeDist, 0.0f, 600.0f, 0.0f, 1.0f);
                                    newVolume = juce::jlimit(0.0f, 1.0f, newVolume);
                                    volume.store(newVolume);
                                    std::cout << "Volume set to: " << newVolume << std::endl;
                                }
                                
                                juce::MessageManager::callAsync([this]() { repaint(); });
                            }
                            else
                            {
                                std::cout << "Not enough tokens: " << tokens.size() << std::endl;
                            }
                        }
                        catch (const std::exception& e)
                        {
                            std::cout << "Parse error: " << e.what() << std::endl;
                        }
                    }
                }
            }
            
            juce::Thread::sleep(10);
        }
        
        std::cout << "Python thread ended" << std::endl;
        python.kill();
    });
    startTimer(30);
}

void MainComponent::readArduinoData()
{
    auto ports = SerialPort::getAvailablePorts();
    
    std::cout << "Scanning for serial ports..." << std::endl;
    std::cout << "Found " << ports.size() << "port(s):" << std::endl;
    
    for (int i = 0; i < ports.size(); i++)
    {
        std::cout << "   [" << i << "] " << ports[i] << std::endl;
    }
    
    juce::String arduinoPort;
    for (auto& port : ports)
    {
        if (port.contains("usbmodem") || port.contains("usbserial"))
        {
            arduinoPort = port;
            std::cout << "\n✓ Arduino port identified: " << arduinoPort << std::endl;
            
            if (port.contains("/dev/cu."))//tty version???
            {
                arduinoPort = port.replace("/dev/cu.", "/dev/tty.");
                std::cout << "  Converting to tty version: " << arduinoPort << std::endl;
            }
            break;
        }
    }
    
    if (arduinoPort.isEmpty())
    {
        std::cout << "\n ERROR: No Arduino port found!" << std::endl;
        std::cout << "   Make sure Arduino is plugged in" << std::endl;
        std::cout << "   Try unplugging and replugging the USB cable\n" << std::endl;
        return;
    }
    
    std::cout << "\n Attempting connection..." << std::endl;
    std::cout << "   Port: " << arduinoPort << std::endl;
    std::cout << "   Baud: 9600" << std::endl;
    
    int retries = 3;
    bool connected = false;
    
    while (retries > 0 && running)
    {
        std::cout << "\n   Attempt " << (4 - retries) << "/3..." << std::endl;
        
        if (serialPort.open(arduinoPort, 9600))
        {
            std::cout << "\n SUCCESS! Connected to Arduino!" << std::endl;
            connected = true;
            break;
        }
        
        std::cout << " Failed to open port" << std::endl;//troubleshooting
        retries--;
        
        if (retries > 0)
        {
            std::cout << "  Waiting 2 seconds before retry..." << std::endl;
            juce::Thread::sleep(2000);
        }
    }
    
    if (!connected)
    {
        std::cout << "\n FINAL ERROR: Could not connect after 3 attempts" << std::endl;
        std::cout << "\n Troubleshooting:" << std::endl;
        return;
    }
    
    std::cout << "\n========================================" << std::endl;
    std::cout << " LISTENING FOR ARDUINO DATA" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Waiting for button press on Arduino...\n" << std::endl;
    
    juce::String buffer;
    int totalBytesRead = 0;
    int messageCount = 0;
    
    while (running)
    {
        char tempBuffer[256];
        int bytesRead = serialPort.read(tempBuffer, sizeof(tempBuffer) - 1);
        
        if (bytesRead > 0)
        {
            totalBytesRead += bytesRead;
            tempBuffer[bytesRead] = '\0';
            
            std::cout << "RAW DATA RECEIVED (" << bytesRead << " bytes):" << std::endl;
            std::cout << "   Hex: ";
            for (int i = 0; i < bytesRead; i++)
            {
                printf("%02X ", (unsigned char)tempBuffer[i]);
            }
            std::cout << std::endl;
            std::cout << "   Text: \"" << tempBuffer << "\"" << std::endl;
            
            buffer += juce::String(tempBuffer);
            std::cout << "   Buffer now contains: \"" << buffer << "\" (length: " << buffer.length() << ")" << std::endl;
            
            // Process complete lines
            while (buffer.contains("\n"))
            {
                auto newlinePos = buffer.indexOf("\n");
                auto line = buffer.substring(0, newlinePos).trim();
                buffer = buffer.substring(newlinePos + 1);
                
                messageCount++;
                
                std::cout << "\n  MESSAGE #" << messageCount << " DETECTED:" << std::endl;
                std::cout << "   Raw line: \"" << line << "\"" << std::endl;
                std::cout << "   Length: " << line.length() << " characters" << std::endl;
                
                if (line.isEmpty())
                {
                    std::cout << "  Line is empty, skipping" << std::endl;
                    continue;
                }
                
                std::cout << "   First 5 chars: \"" << line.substring(0, 5) << "\"" << std::endl;
                
                if (line.startsWith("Song:"))
                {
                    std::cout << "  VALID SONG COMMAND!" << std::endl;
                    
                    auto songName = line.substring(5).trim();
                    std::cout << "   Song name extracted: \"" << songName << "\"" << std::endl;
                    std::cout << "   Will look for file: \"" << songName << ".mp3\"" << std::endl;
                    
                    juce::File songFile = musicFolder.getChildFile(songName + ".mp3");
                    std::cout << "   Full path: " << songFile.getFullPathName() << std::endl;
                    
                    if (songFile.existsAsFile())
                    {
                        std::cout << " File exists! Loading..." << std::endl;
                    }
                    else
                    {
                        std::cout << "File NOT FOUND!" << std::endl;
                        std::cout << "   Checking what files exist in folder:" << std::endl;
                        
                        auto files = musicFolder.findChildFiles(juce::File::findFiles, false, "*.mp3");
                        for (auto& f : files)
                        {
                            std::cout << "      - " << f.getFileName() << std::endl;
                        }
                    }
                    
                    juce::MessageManager::callAsync([this, songName]() {
                        loadSongByName(songName + ".mp3");
                    });
                }
                else
                {
                    std::cout << "  NOT a song command (doesn't start with 'SONG:')" << std::endl;
                    std::cout << "   Received: \"" << line << "\"" << std::endl;
                }
                
                std::cout << "   Remaining buffer: \"" << buffer << "\"\n" << std::endl;
            }
        }
        
        juce::Thread::sleep(50);
    }
    
    serialPort.close();
    std::cout << "\n========================================" << std::endl;
    std::cout << "Arduino thread ended" << std::endl;
    std::cout << "Total bytes read: " << totalBytesRead << std::endl;
    std::cout << "Total messages: " << messageCount << std::endl;
    std::cout << "========================================\n" << std::endl;
}

void MainComponent::loadSongByName(const juce::String& songName)
{
    std::cout << "\n LOAD SONG FUNCTION CALLED" << std::endl;
    std::cout << "   Requested: \"" << songName << "\"" << std::endl;
    
    juce::File audioFile = musicFolder.getChildFile(songName);
    std::cout << "   Full path: " << audioFile.getFullPathName() << std::endl;
    std::cout << "   Exists: " << (audioFile.existsAsFile() ? "YES" : "NO") << std::endl;
    
    if (!audioFile.existsAsFile())
    {
        std::cout << " ERROR: Song file not found!" << std::endl;
        std::cout << "   Available files in folder:" << std::endl;
        
        auto files = musicFolder.findChildFiles(juce::File::findFiles, false, "*.mp3");
        if (files.isEmpty())
        {
            std::cout << "      (no MP3 files found)" << std::endl;
        }
        else
        {
            for (auto& f : files)
            {
                std::cout << "      - " << f.getFileName() << std::endl;
            }
        }
        return;
    }
    
    std::cout << "   Stopping current playback..." << std::endl;
    transportSource.stop();
    transportSource.setSource(nullptr);
    readerSource.reset();
    
    std::cout << "   Creating reader..." << std::endl;
    auto* reader = formatManager.createReaderFor(audioFile);
    
    if (reader != nullptr)
    {
        std::cout << "  Reader created successfully" << std::endl;
        std::cout << "   Sample rate: " << reader->sampleRate << std::endl;
        std::cout << "   Channels: " << reader->numChannels << std::endl;
        std::cout << "   Length: " << (reader->lengthInSamples / reader->sampleRate) << " seconds" << std::endl;
        
        auto newSource = std::make_unique<juce::AudioFormatReaderSource>(reader, true);
        transportSource.setSource(newSource.get(), 0, nullptr, reader->sampleRate);
        readerSource.reset(newSource.release());
        transportSource.start();
        
        std::cout << "  NOW PLAYING: " << songName << "\n" << std::endl;
    }
    else
    {
        std::cout << "   ERROR: Could not create reader for file!" << std::endl;
        std::cout << "   File might be corrupted or wrong format\n" << std::endl;
    }
}

void MainComponent::timerCallback()
{
    repaint();
}

/*void MainComponent::loadAudioFile()
{
    juce::File audioFile("/Users/mary/Desktop/dj_hand_project/touchthesky.mp3");
    
    if (!audioFile.existsAsFile())
    {
        std::cout << "Audio file not found! Put an MP3 in your Music folder." << std::endl;
        return;
    }
    
    auto* reader = formatManager.createReaderFor(audioFile);
    
    if (reader != nullptr)
    {
        auto newSource = std::make_unique<juce::AudioFormatReaderSource>(reader, true);
        transportSource.setSource(newSource.get(), 0, nullptr, reader->sampleRate);
        readerSource.reset(newSource.release());
        transportSource.start();
        
        std::cout << "Audio file loaded and playing!" << std::endl;
    }
} loading audio file code, bring it back if doesnt work*/

MainComponent::~MainComponent()
{
    running = false;
    if (pythonThread.joinable())
        pythonThread.join();
    
    if (arduinoThread.joinable())
            arduinoThread.join();
    
    transportSource.setSource(nullptr);
    shutdownAudio();
}

//==============================================================================
void MainComponent::paint (juce::Graphics& g)
{
        g.fillAll (juce::Colours::darkgrey);

        g.setColour (juce::Colours::white);
        g.setFont (juce::Font (28.0f, juce::Font::bold));

        auto speedVal = speed.load();
        auto pitchVal = pitch.load();
        auto volumeVal = volume.load();

        juce::String info = "DJ HAND CONTROL\n\n";
        info += "Speed (Left Hand): " + juce::String(speedVal, 2) + "x\n";
        info += "Pitch (Right Hand): " + juce::String(pitchVal, 2) + "x\n";
        info += "Volume (Both Hands): " + juce::String(volumeVal * 100, 0) + "%";

        g.drawText(info,
                   getLocalBounds(),
                   juce::Justification::centred);
}

void MainComponent::resized()
{
}

//==============================================================================
void MainComponent::prepareToPlay (int samplesPerBlock, double sampleRate)
{
    transportSource.prepareToPlay(samplesPerBlock, sampleRate);
    resamplingSource.prepareToPlay(samplesPerBlock, sampleRate);
    pitchShifter.prepare(sampleRate, samplesPerBlock);
}

void MainComponent::getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill)
{
    float speedVal = speed.load();
    float pitchVal = pitch.load();
        
    resamplingSource.setResamplingRatio(speedVal * pitchVal);
        
    resamplingSource.getNextAudioBlock(bufferToFill);
        
    float volumeVal = volume.load();
    bufferToFill.buffer->applyGain(volumeVal);
    static int debugCounter = 0;
        if (++debugCounter % 1000 == 0)
        {
            std::cout << "Speed: " << speedVal
                      << " Pitch: " << pitchVal
                      << " Volume: " << volumeVal << std::endl;
        }
}

void MainComponent::releaseResources()
{
    transportSource.releaseResources();
    resamplingSource.releaseResources();
}
