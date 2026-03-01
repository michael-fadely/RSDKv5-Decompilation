#include <dc/sound/sound.h>

#include "KallistiOSSfxUpdate.cpp"
#include "KallistiOSStream.cpp"

static bool deviceInited;

// static
bool32 AudioDevice::Init()
{
    bool32 ok = snd_init() >= 0;
    InitAudioChannels();
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
    if (!deviceInited) {
        Init();
        deviceInited = true;
    }
}

// static
void AudioDevice::HandleStreamLoad(ChannelInfo *channel, bool32 async)
{
    ;
}
