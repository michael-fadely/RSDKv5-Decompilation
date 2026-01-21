#include <dc/sound/sound.h>

#include "KallistiOSStream.cpp"

extern "C" {
    static bool32 device_inited = 0;
    extern int stream_init(void);
};

// static
bool32 AudioDevice::Init()
{
    bool32 ok = snd_init() >= 0;
    return ok && !!stream_init();
}

// static
void AudioDevice::Release()
{
    ;
}

// static
void AudioDevice::ProcessAudioMixing(void *stream, int32 length)
{
    ;
}

// static
void AudioDevice::FrameInit()
{
    if (!device_inited) {
        Init();
        device_inited = 1;
    }
}

// static
void AudioDevice::HandleStreamLoad(ChannelInfo *channel, bool32 async)
{
    ;
}
