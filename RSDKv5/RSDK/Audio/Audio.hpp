#ifndef AUDIO_H
#define AUDIO_H

namespace RSDK
{

#define SFX_COUNT     (0x100)
#define CHANNEL_COUNT (0x10)

#define MIX_BUFFER_SIZE (0x800)
#define SAMPLE_FORMAT   float

#if RETRO_PLATFORM == RETRO_KALLISTIOS
#define AUDIO_FREQUENCY (22050)
#else
#define AUDIO_FREQUENCY (44100)
#endif
#define AUDIO_CHANNELS  (2)

#if RETRO_PLATFORM == RETRO_KALLISTIOS
#include <kos.h>
#endif

struct SFXInfo {
    RETRO_HASH_MD5(hash);
#if RETRO_PLATFORM == RETRO_KALLISTIOS
    sfxhnd_t handle;
    float freq;
#else
    float *buffer;
#endif
    size_t length;
    int32 playCount;
    uint8 maxConcurrentPlays;
    uint8 scope;
};

struct ChannelInfo {
#if RETRO_PLATFORM != RETRO_KALLISTIOS
    float *samplePtr;
#endif
    float pan;
    float volume;
#if RETRO_PLATFORM == RETRO_KALLISTIOS
    float speed;
#else
    int32 speed;
#endif
    size_t sampleLength;
#if RETRO_PLATFORM != RETRO_KALLISTIOS
    int32 bufferPos;
#endif
    int32 playIndex;
    uint32 loop;
    int16 soundID;
    uint8 priority;
    uint8 state;
#if RETRO_PLATFORM == RETRO_KALLISTIOS
    uint64_t startTimeNs;
    int aicaChannel;
#endif
};

enum ChannelStates { CHANNEL_IDLE, CHANNEL_SFX, CHANNEL_STREAM, CHANNEL_LOADING_STREAM, CHANNEL_PAUSED = 0x40 };

extern SFXInfo sfxList[SFX_COUNT];
extern ChannelInfo channels[CHANNEL_COUNT];

class AudioDeviceBase
{
public:
    static bool32 Init();
    static void Release();

    static void ProcessAudioMixing(void *stream, int32 length);

    static void FrameInit();

    static void HandleStreamLoad(ChannelInfo *channel, bool32 async);

    static uint8 initializedAudioChannels;
    static uint8 audioState;
    static uint8 audioFocus;

protected:
    static void InitAudioChannels();
};

void UpdateStreamBuffer(ChannelInfo *channel);
void LoadStream(ChannelInfo *channel);
int32 PlayStream(const char *filename, uint32 slot, uint32 startPos, uint32 loopPoint, bool32 loadASync);

void LoadSfxToSlot(char *filename, uint8 slot, uint8 plays, uint8 scope);
void LoadSfx(char *filePath, uint8 plays, uint8 scope);

} // namespace RSDK

#if RETRO_AUDIODEVICE_XAUDIO
#include "XAudio/XAudioDevice.hpp"
#elif RETRO_AUDIODEVICE_PORT
#include "PortAudio/PortAudioDevice.hpp"
#elif RETRO_AUDIODEVICE_MINI
#include "MiniAudio/MiniAudioDevice.hpp"
#elif RETRO_AUDIODEVICE_SDL2
#include "SDL2/SDL2AudioDevice.hpp"
#elif RETRO_AUDIODEVICE_OBOE
#include "Oboe/OboeAudioDevice.hpp"
#elif RETRO_AUDIODEVICE_KALLISTIOS
#include "KallistiOS/KallistiOSAudioDevice.hpp"
#endif

namespace RSDK
{

inline uint16 GetSfx(const char *sfxName)
{
    RETRO_HASH_MD5(hash);
    GEN_HASH_MD5(sfxName, hash);

    for (int32 s = 0; s < SFX_COUNT; ++s) {
        if (HASH_MATCH_MD5(sfxList[s].hash, hash)) {
            return s;
        }
    }

#if RETRO_PLATFORM == RETRO_KALLISTIOS
    LoadSfx((char*)sfxName, 1, 1);

    for (int32 s = 0; s < SFX_COUNT; ++s) {
        if (HASH_MATCH_MD5(sfxList[s].hash, hash)) {
            return s;
        }
    }
#endif

    return -1;
}
int32 PlaySfx(uint16 sfx, uint32 loopPoint, uint32 priority);
inline void StopSfx(uint16 sfx)
{
#if !RETRO_USE_ORIGINAL_CODE
    LockAudioDevice();
#endif

    for (int32 i = 0; i < CHANNEL_COUNT - 1; ++i) {
        if (channels[i].soundID == sfx) {
#if RETRO_PLATFORM == RETRO_KALLISTIOS
            // get the hardware playback channel number before MEM_ZERO call
            int aicaChannel = channels[i].aicaChannel;
#endif
            MEM_ZERO(channels[i]);
#if RETRO_PLATFORM == RETRO_KALLISTIOS
            // if this is not -1, a hardware playback channel was assigned to this RSDK ChannelInfo
            // release it
            if (aicaChannel != -1) {
                snd_sfx_stop(aicaChannel);
                snd_sfx_chn_free(aicaChannel);
            }
            channels[i].aicaChannel = -1;
            channels[i].startTimeNs = 0xFFFFFFFFFFFFFFFFL;
#endif
            channels[i].soundID = -1;
            channels[i].state   = CHANNEL_IDLE;
        }
    }

#if !RETRO_USE_ORIGINAL_CODE
    UnlockAudioDevice();
#endif
}

#if RETRO_REV0U
inline void StopAllSfx()
{
#if !RETRO_USE_ORIGINAL_CODE
    LockAudioDevice();
#endif

    for (int32 i = 0; i < CHANNEL_COUNT - 1; ++i) {
#if RETRO_PLATFORM == RETRO_KALLISTIOS
        // get the hardware playback channel number before MEM_ZERO call
        int aicaChannel = channels[i].aicaChannel;
#endif
        if (channels[i].state == CHANNEL_SFX) {
            MEM_ZERO(channels[i]);
#if RETRO_PLATFORM == RETRO_KALLISTIOS
            // if this is not -1, a hardware playback channel was assigned to this RSDK ChannelInfo
            // release it
            if (aicaChannel != -1) {
                snd_sfx_stop(aicaChannel);
                snd_sfx_chn_free(aicaChannel);
            }
            channels[i].aicaChannel = -1;
            channels[i].startTimeNs = 0xFFFFFFFFFFFFFFFFL;
#endif
            channels[i].soundID = -1;
            channels[i].state   = CHANNEL_IDLE;
        }
    }

#if !RETRO_USE_ORIGINAL_CODE
    UnlockAudioDevice();
#endif
}
#endif

void SetChannelAttributes(uint8 channel, float volume, float panning, float speed);

inline void StopChannel(uint32 channel)
{
    if (channel < CHANNEL_COUNT) {
        if (channels[channel].state != CHANNEL_LOADING_STREAM)
            channels[channel].state = CHANNEL_IDLE;
    }
}

inline void PauseChannel(uint32 channel)
{
    if (channel < CHANNEL_COUNT) {
        if (channels[channel].state != CHANNEL_LOADING_STREAM)
            channels[channel].state |= CHANNEL_PAUSED;
    }
}

inline void ResumeChannel(uint32 channel)
{
    if (channel < CHANNEL_COUNT) {
        if (channels[channel].state != CHANNEL_LOADING_STREAM)
            channels[channel].state &= ~CHANNEL_PAUSED;
    }
}

inline void PauseSound()
{
    for (int32 c = 0; c < CHANNEL_COUNT; ++c) PauseChannel(c);
}

inline void ResumeSound()
{
    for (int32 c = 0; c < CHANNEL_COUNT; ++c) ResumeChannel(c);
}

inline bool32 SfxPlaying(uint16 sfx)
{
    for (int32 c = 0; c < CHANNEL_COUNT; ++c) {
        if (channels[c].state == CHANNEL_SFX && channels[c].soundID == sfx)
            return true;
    }
    return false;
}

#if RETRO_PLATFORM == RETRO_KALLISTIOS
extern "C" {
    int stream_is_playing(void);
}
#endif

inline bool32 ChannelActive(uint32 channel)
{
#if RETRO_PLATFORM == RETRO_KALLISTIOS
    // afaict only used to check if song still playing in credits scroll
    return stream_is_playing();
#else
    if (channel >= CHANNEL_COUNT)
        return false;
    else
        return (channels[channel].state & 0x3F) != CHANNEL_IDLE;
#endif
}

uint32 GetChannelPos(uint32 channel);
double GetVideoStreamPos();

void ClearStageSfx();
#if RETRO_USE_MOD_LOADER
void ClearGlobalSfx();
#endif

#if RETRO_REV0U
#include "Legacy/AudioLegacy.hpp"
#endif

} // namespace RSDK

#endif
