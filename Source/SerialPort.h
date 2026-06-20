#pragma once
#include <JuceHeader.h>
#include <termios.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <iostream>

class SerialPort
{
public:
    SerialPort() : fileDescriptor(-1) {}
    
    ~SerialPort() { close(); }
    
    bool open(const juce::String& portPath, int baudRate = 9600)
    {
        close();
        
        std::cout << "Attempting to open: " << portPath << std::endl;
        
        fileDescriptor = ::open(portPath.toRawUTF8(), O_RDWR | O_NOCTTY | O_NONBLOCK);
        
        if (fileDescriptor < 0)
        {
            std::cout << " Open failed with errno: " << errno << " (" << strerror(errno) << ")" << std::endl;
            return false;
        }
        
        int flags = fcntl(fileDescriptor, F_GETFL, 0);
        fcntl(fileDescriptor, F_SETFL, flags & ~O_NONBLOCK);
        
        std::cout << " Port opened, configuring..." << std::endl;
        
        usleep(100000);//stabalizing
        
        struct termios options;
        if (tcgetattr(fileDescriptor, &options) != 0)
        {
            std::cout << " tcgetattr failed: " << strerror(errno) << std::endl;
            ::close(fileDescriptor);
            fileDescriptor = -1;
            return false;
        }
        
        speed_t baud = B9600;
        switch (baudRate)
        {
            case 4800:   baud = B4800;   break;
            case 9600:   baud = B9600;   break;
            case 19200:  baud = B19200;  break;
            case 38400:  baud = B38400;  break;
            case 57600:  baud = B57600;  break;
            case 115200: baud = B115200; break;
        }
        
        cfsetispeed(&options, baud);
        cfsetospeed(&options, baud);
        
        //8N1 mode
        options.c_cflag &= ~PARENB;
        options.c_cflag &= ~CSTOPB;
        options.c_cflag &= ~CSIZE;
        options.c_cflag |= CS8;
        options.c_cflag |= (CLOCAL | CREAD);
        
        options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
        options.c_oflag &= ~OPOST;
        options.c_iflag &= ~(IXON | IXOFF | IXANY);
        
        options.c_cc[VMIN] = 0;
        options.c_cc[VTIME] = 1;
        
        if (tcsetattr(fileDescriptor, TCSANOW, &options) != 0)
        {
            std::cout << " tcsetattr failed: " << strerror(errno) << std::endl;
            ::close(fileDescriptor);
            fileDescriptor = -1;
            return false;
        }
        
        tcflush(fileDescriptor, TCIOFLUSH);//flushes past data
        
        std::cout << " Waiting for Arduino to reset..." << std::endl;
        usleep(2000000);
        
        std::cout << " Port configured successfully!" << std::endl;
        return true;
    }
    
    void close()
    {
        if (fileDescriptor >= 0)
        {
            ::close(fileDescriptor);
            fileDescriptor = -1;
        }
    }
    
    int read(void* buffer, int bytesToRead)
    {
        if (fileDescriptor < 0) return -1;
        return ::read(fileDescriptor, buffer, bytesToRead);
    }
    
    int write(const void* data, int bytesToWrite)
    {
        if (fileDescriptor < 0) return -1;
        return ::write(fileDescriptor, data, bytesToWrite);
    }
    
    int bytesAvailable()
    {
        if (fileDescriptor < 0) return 0;
        int bytes = 0;
        ioctl(fileDescriptor, FIONREAD, &bytes);
        return bytes;
    }
    
    static juce::StringArray getAvailablePorts()
    {
        juce::StringArray ports;
        juce::File devFolder("/dev");
        
        auto files = devFolder.findChildFiles(juce::File::findFiles, false, "cu.*");
        for (auto& file : files)
            ports.add(file.getFullPathName());
            
        return ports;
    }
    
private:
    int fileDescriptor;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SerialPort)
};
