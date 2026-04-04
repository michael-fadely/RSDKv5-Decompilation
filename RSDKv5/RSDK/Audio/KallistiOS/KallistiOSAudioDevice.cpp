#include <dc/sound/sound.h>

#include "KallistiOSSfxUpdate.cpp"
#include "KallistiOSStream.cpp"

static bool deviceInited = false;

// static
bool32 AudioDevice::Init()
{
    if (!deviceInited) {
        bool ok = snd_init() >= 0;
        InitAudioChannels();
        deviceInited = ok && !!stream_init();
    }

    return deviceInited;
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
    ;
}

// static
void AudioDevice::HandleStreamLoad(ChannelInfo *channel, bool32 async)
{
    ;
}
