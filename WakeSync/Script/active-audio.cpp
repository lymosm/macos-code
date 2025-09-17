#include <iostream>
#include <AudioToolbox/AudioToolbox.h>
#include <CoreAudio/CoreAudio.h>
#include <unistd.h> // For usleep

// 用于管理播放状态的结构体
struct PlayerState {
    AudioQueueRef               queue = nullptr;
    AudioFileID                 audioFile = nullptr;
    bool                        isDone = false;
};

// 回调函数，当一个缓冲区播放完成时被系统调用
void audioQueueOutputCallback(void *inUserData, AudioQueueRef inAQ, AudioQueueBufferRef inBuffer) {
    AudioQueueFreeBuffer(inAQ, inBuffer);
    PlayerState *state = (PlayerState *)inUserData;
    std::cout << "Audio playback completed!" << std::endl;
    state->isDone = true;
}

// 播放一个音频文件（.aiff 或 .wav）
void playAudioFile(const char* filePath) {
    PlayerState state;

    // 1. 打开音频文件
    CFURLRef fileURL = CFURLCreateFromFileSystemRepresentation(kCFAllocatorDefault, (const UInt8*)filePath, strlen(filePath), false);
    if (!fileURL) {
        std::cerr << "Failed to create CFURLRef!" << std::endl;
        return;
    }

    OSStatus status = AudioFileOpenURL(fileURL, kAudioFileReadPermission, 0, &state.audioFile);
    CFRelease(fileURL);
    
    if (status != noErr) {
        std::cerr << "Failed to open audio file! Error code: " << status << std::endl;
        return;
    }

    // 2. 获取音频文件的格式
    AudioStreamBasicDescription fileFormat;
    UInt32 size = sizeof(fileFormat);
    status = AudioFileGetProperty(state.audioFile, kAudioFilePropertyDataFormat, &size, &fileFormat);
    
    if (status != noErr) {
        std::cerr << "Failed to get audio file format!" << std::endl;
        AudioFileClose(state.audioFile);
        return;
    }

    std::cout << "Audio Format: Sample Rate = " << fileFormat.mSampleRate
              << ", Channels = " << fileFormat.mChannelsPerFrame
              << ", Bits per Channel = " << fileFormat.mBitsPerChannel << std::endl;

    // 3. 创建音频输出队列
    status = AudioQueueNewOutput(&fileFormat, audioQueueOutputCallback, &state, CFRunLoopGetCurrent(), kCFRunLoopCommonModes, 0, &state.queue);
    
    if (status != noErr) {
        std::cerr << "Error creating audio queue!" << std::endl;
        AudioFileClose(state.audioFile);
        return;
    }
    std::cout << "Audio queue created successfully!" << std::endl;

    // 4. 设置缓冲区大小并分配缓冲区
    const int bufferSize = 32768;
    AudioQueueBufferRef buffer;
    status = AudioQueueAllocateBuffer(state.queue, bufferSize, &buffer);
    
    if (status != noErr) {
        std::cerr << "Error allocating buffer!" << std::endl;
        AudioQueueDispose(state.queue, true);
        AudioFileClose(state.audioFile);
        return;
    }

    // 5. 从文件中读取音频数据到缓冲区
    UInt32 numBytesToRead = bufferSize;
    UInt32 numPackets = numBytesToRead / fileFormat.mBytesPerPacket;
    SInt64 startingPacket = 0;
    
    // **唯一的改动在这里：使用新的函数 AudioFileReadPacketData**
    status = AudioFileReadPacketData(state.audioFile, false, &numBytesToRead, NULL, startingPacket, &numPackets, buffer->mAudioData);

    if (status != noErr) {
        std::cerr << "Error reading audio data from file!" << std::endl;
        AudioQueueDispose(state.queue, true);
        AudioFileClose(state.audioFile);
        return;
    }

    if (numPackets > 0) {
        buffer->mAudioDataByteSize = numBytesToRead;
        // 6. 将填充好的缓冲区放入队列
        status = AudioQueueEnqueueBuffer(state.queue, buffer, 0, nullptr);
        
        if (status != noErr) {
            std::cerr << "Error enqueuing buffer!" << std::endl;
            AudioQueueDispose(state.queue, true);
            AudioFileClose(state.audioFile);
            return;
        }

        // 7. 开始播放
        status = AudioQueueStart(state.queue, nullptr);
        
        if (status != noErr) {
            std::cerr << "Error starting audio queue!" << std::endl;
            AudioQueueDispose(state.queue, true);
            AudioFileClose(state.audioFile);
            return;
        }

        std::cout << "Audio playback started! Waiting for completion..." << std::endl;

        // 8. 等待播放完成
        while (!state.isDone) {
            CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.1, false);
        }
        
        usleep(100000);
    } else {
        AudioQueueFreeBuffer(state.queue, buffer);
    }
    
    // 9. 清理资源
    std::cout << "Cleaning up resources." << std::endl;
    AudioQueueDispose(state.queue, true);
    AudioFileClose(state.audioFile);
}

// main 函数保持不变
int main() {
    playAudioFile("/System/Library/Sounds/Frog.aiff");
    std::cout << "Program finished." << std::endl;
    return 0;
}
