#include "RSDK/Core/RetroEngine.hpp"

using namespace RSDK;

#if RETRO_PLATFORM == RETRO_KALLISTIOS
#include <RSDK/Core/Stub.hpp>
#endif

#if RETRO_REV0U
#include "Legacy/AudioLegacy.cpp"
#endif

#if RETRO_PLATFORM != RETRO_KALLISTIOS
#define STB_VORBIS_NO_PUSHDATA_API
#define STB_VORBIS_NO_STDIO
#define STB_VORBIS_NO_INTEGER_CONVERSION

#include "stb_vorbis/stb_vorbis.c"

stb_vorbis *vorbisInfo = NULL;
stb_vorbis_alloc vorbisAlloc;
#endif

SFXInfo RSDK::sfxList[SFX_COUNT];
ChannelInfo RSDK::channels[CHANNEL_COUNT];

char streamFilePath[0x40];
uint8 *streamBuffer    = NULL;
int32 streamBufferSize = 0;
uint32 streamStartPos  = 0;
int32 streamLoopPoint  = 0;

#if RETRO_PLATFORM != RETRO_KALLISTIOS
#define LINEAR_INTERPOLATION_LOOKUP_DIVISOR 0x40 // Determines the 'resolution' of the lookup table.
#define LINEAR_INTERPOLATION_LOOKUP_LENGTH  (TO_FIXED(1) / LINEAR_INTERPOLATION_LOOKUP_DIVISOR)

float linearInterpolationLookup[LINEAR_INTERPOLATION_LOOKUP_LENGTH];
#endif


#if RETRO_AUDIODEVICE_XAUDIO
#include "XAudio/XAudioDevice.cpp"
#elif RETRO_AUDIODEVICE_SDL2
#include "SDL2/SDL2AudioDevice.cpp"
#elif RETRO_AUDIODEVICE_PORT
#include "PortAudio/PortAudioDevice.cpp"
#elif RETRO_AUDIODEVICE_MINI
#include "MiniAudio/MiniAudioDevice.cpp"
#elif RETRO_AUDIODEVICE_OBOE
#include "Oboe/OboeAudioDevice.cpp"
#elif RETRO_AUDIODEVICE_KALLISTIOS
#include "KallistiOS/KallistiOSAudioDevice.cpp"
#endif

uint8 AudioDeviceBase::initializedAudioChannels = false;
uint8 AudioDeviceBase::audioState               = 0;
uint8 AudioDeviceBase::audioFocus               = 0;

#if RETRO_PLATFORM == RETRO_KALLISTIOS
extern "C" {
#include "dc/sound/aica_comm.h"
extern int snd_sh4_to_aica(void *packet, uint32_t size);
struct snd_effect;
typedef struct snd_effect
{
	uint32_t locl, locr;
	uint32_t len;
	uint32_t rate;
	uint32_t used;
	uint32_t fmt;
	uint16_t stereo;

	LIST_ENTRY(snd_effect)
	list;
} snd_effect_t;

void snd_sfx_update_ex(sfx_play_data_t *data)
{
	int size;
	snd_effect_t *t = (snd_effect_t *)data->idx;
	AICA_CMDSTR_CHANNEL(tmp, cmd, chan);

	size = t->len;

	if (size >= 65535)
		size = 65534;

	cmd->cmd = AICA_CMD_CHAN;
	cmd->timestamp = 0;
	cmd->size = AICA_CMDSTR_CHANNEL_SIZE;
	cmd->cmd_id = data->chn;

	chan->cmd = AICA_CH_CMD_UPDATE | AICA_CH_UPDATE_SET_FREQ | AICA_CH_UPDATE_SET_PAN | AICA_CH_UPDATE_SET_VOL;
	chan->base = t->locl;
	chan->type = t->fmt;
	chan->length = size;

	chan->loop = data->loop;
	chan->loopstart = data->loopstart;
	chan->loopend = data->loopend ? data->loopend : size;
	chan->freq = data->freq > 0 ? data->freq : t->rate;
	chan->vol = data->vol;
	chan->pan = data->pan;

	snd_sh4_to_aica(tmp, cmd->size);
}
}
#endif

void AudioDeviceBase::Release()
{
    // This is missing, meaning that the garbage collector will never reclaim stb_vorbis's buffer.
#if !RETRO_USE_ORIGINAL_CODE && RETRO_PLATFORM != RETRO_KALLISTIOS
    stb_vorbis_close(vorbisInfo);
    vorbisInfo = NULL;
#endif
}

void AudioDeviceBase::ProcessAudioMixing(void *stream, int32 length)
{
//    return;
#if RETRO_PLATFORM == RETRO_KALLISTIOS
    for (int32 c = 0; c < CHANNEL_COUNT; ++c) {
        ChannelInfo *channel = &channels[c];

        switch (channel->state) {
            default:
                break;

            case CHANNEL_SFX: {
                if (channel->soundID != -1) {
                    printf("xxx Channel %d is SFX %d\n", c, channel->soundID);
                    int aicaChannel = channel->aicaChannel;
                    if ((aicaChannel != -1)){//} && (snd_get_pos(aicaChannel) == 0)) {
                        sfxhnd_t handle = sfxList[channel->soundID].handle;
                        printf("xxx\ton AICA channel %d (pos %d)\n", aicaChannel, snd_get_pos(aicaChannel));
                        double playTime = (double)(timer_ns_gettime64() - channel->startNs) * 1e-9;
                        double freq = (AUDIO_FREQUENCY / sfxList[channel->soundID].ratediv);
                        double samplesTime = (double)channel->sampleLength / freq;
                        printf("xxx\t\tloop %d freq %f playTime %f samplesTime %f\n", channel->loop, freq, playTime, samplesTime);
                        if ((!channel->loop) && (samplesTime <= playTime)) {
                                printf("xxx\t\t\tReleased AICA Channel %d\n", aicaChannel);
                                channel->state   = CHANNEL_IDLE;
                                channel->soundID = -1;
                                channel->aicaChannel = -1;
                                channel->zeroPosCount = -1;
                                snd_sfx_stop(aicaChannel);
                                snd_sfx_chn_free(aicaChannel);
                        }
                    }
                }
            }
        }
    }
#endif
#if RETRO_PLATFORM != RETRO_KALLISTIOS
    SAMPLE_FORMAT *streamF    = (SAMPLE_FORMAT *)stream;
    SAMPLE_FORMAT *streamEndF = ((SAMPLE_FORMAT *)stream) + length;

    memset(stream, 0, length * sizeof(SAMPLE_FORMAT));

    for (int32 c = 0; c < CHANNEL_COUNT; ++c) {
        ChannelInfo *channel = &channels[c];

        switch (channel->state) {
            default:
            case CHANNEL_IDLE: break;

            case CHANNEL_SFX: {
                SAMPLE_FORMAT *sfxBuffer = &channel->samplePtr[channel->bufferPos];

                float volL = channel->volume, volR = channel->volume;
                if (channel->pan < 0.0f)
                    volR = (1.0f + channel->pan) * channel->volume;
                else
                    volL = (1.0f - channel->pan) * channel->volume;

                float panL = volL * engine.soundFXVolume;
                float panR = volR * engine.soundFXVolume;

                uint32 speedPercent       = 0;
                SAMPLE_FORMAT *curStreamF = streamF;
                while (curStreamF < streamEndF && streamF < streamEndF) {
                    // Perform linear interpolation.
                    SAMPLE_FORMAT sample;
#if !RETRO_USE_ORIGINAL_CODE
                    if (!sfxBuffer) // PROTECTION FOR v5U (and other mysterious crashes 👻)
                        sample = 0;
                    else
#endif
                        sample = (sfxBuffer[1] - sfxBuffer[0]) * linearInterpolationLookup[speedPercent / LINEAR_INTERPOLATION_LOOKUP_DIVISOR]
                                 + sfxBuffer[0];

                    speedPercent += channel->speed;
                    sfxBuffer += FROM_FIXED(speedPercent);
                    channel->bufferPos += FROM_FIXED(speedPercent);
                    speedPercent %= TO_FIXED(1);

                    curStreamF[0] += sample * panL;
                    curStreamF[1] += sample * panR;
                    curStreamF += 2;

                    if (channel->bufferPos >= channel->sampleLength) {
                        if (channel->loop == (uint32)-1) {
                            channel->state   = CHANNEL_IDLE;
                            channel->soundID = -1;
                            break;
                        }
                        else {
                            channel->bufferPos -= (uint32)channel->sampleLength;
                            channel->bufferPos += channel->loop;

                            sfxBuffer = &channel->samplePtr[channel->bufferPos];
                        }
                    }
                }

                break;
            }

            case CHANNEL_STREAM: {
                SAMPLE_FORMAT *streamBuffer = &channel->samplePtr[channel->bufferPos];

                float volL = channel->volume, volR = channel->volume;
                if (channel->pan < 0.0f)
                    volR = (1.0f + channel->pan) * channel->volume;
                else
                    volL = (1.0f - channel->pan) * channel->volume;

                float panL = volL * engine.streamVolume;
                float panR = volR * engine.streamVolume;

                uint32 speedPercent       = 0;
                SAMPLE_FORMAT *curStreamF = streamF;
                while (curStreamF < streamEndF && streamF < streamEndF) {
                    speedPercent += channel->speed;
                    int32 next = FROM_FIXED(speedPercent);
                    speedPercent %= TO_FIXED(1);

                    curStreamF[0] += streamBuffer[0] * panL;
                    curStreamF[1] += streamBuffer[1] * panR;
                    curStreamF += 2;

                    streamBuffer += next * 2;
                    channel->bufferPos += next * 2;

                    if (channel->bufferPos >= channel->sampleLength) {
                        channel->bufferPos -= (uint32)channel->sampleLength;

                        streamBuffer = &channel->samplePtr[channel->bufferPos];

                        UpdateStreamBuffer(channel);
                    }
                }
                break;
            }

            case CHANNEL_LOADING_STREAM: break;
        }
    }
#endif
}

void AudioDeviceBase::InitAudioChannels()
{
    for (int32 i = 0; i < CHANNEL_COUNT; ++i) {
        channels[i].soundID = -1;
        channels[i].state   = CHANNEL_IDLE;
#if RETRO_PLATFORM == RETRO_KALLISTIOS
        channels[i].aicaChannel = -1;
        channels[i].startNs = 0xFFFFFFFFFFFFFFFFL;
#endif
    }

#if RETRO_PLATFORM != RETRO_KALLISTIOS
    // Compute a lookup table of floating-point linear interpolation delta scales,
    // to speed-up the process of converting from fixed-point to floating-point.
    for (int32 i = 0; i < LINEAR_INTERPOLATION_LOOKUP_LENGTH; ++i) linearInterpolationLookup[i] = i / (float)LINEAR_INTERPOLATION_LOOKUP_LENGTH;
#endif

    GEN_HASH_MD5("Stream Channel 0", sfxList[SFX_COUNT - 1].hash);
    sfxList[SFX_COUNT - 1].scope              = SCOPE_GLOBAL;
    sfxList[SFX_COUNT - 1].maxConcurrentPlays = 1;
    sfxList[SFX_COUNT - 1].length             = MIX_BUFFER_SIZE;
#if RETRO_PLATFORM != RETRO_KALLISTIOS
    AllocateStorage((void **)&sfxList[SFX_COUNT - 1].buffer, MIX_BUFFER_SIZE * sizeof(SAMPLE_FORMAT), DATASET_MUS, false);
#endif

    initializedAudioChannels = true;
}

void RSDK::UpdateStreamBuffer(ChannelInfo *channel)
{
#if RETRO_PLATFORM != RETRO_KALLISTIOS
    int32 bufferRemaining = MIX_BUFFER_SIZE;
    float *buffer         = channel->samplePtr;

    for (int32 s = 0; s < MIX_BUFFER_SIZE;) {
        int32 samples = stb_vorbis_get_samples_float_interleaved(vorbisInfo, 2, buffer, bufferRemaining) * 2;
        if (!samples) {
            if (channel->loop == 1 && stb_vorbis_seek_frame(vorbisInfo, streamLoopPoint)) {
                // we're looping & the seek was successful, get more samples
            }
            else {
                channel->state   = CHANNEL_IDLE;
                channel->soundID = -1;
                memset(buffer, 0, sizeof(float) * bufferRemaining);

                break;
            }
        }

        s += samples;
        buffer += samples;
        bufferRemaining = MIX_BUFFER_SIZE - s;
    }

    for (int32 i = 0; i < MIX_BUFFER_SIZE; ++i) channel->samplePtr[i] *= 0.5f;
#endif
}

void RSDK::LoadStream(ChannelInfo *channel)
{
#if RETRO_PLATFORM != RETRO_KALLISTIOS
    if (channel->state != CHANNEL_LOADING_STREAM)
        return;

    stb_vorbis_close(vorbisInfo);

    FileInfo info;
    InitFileInfo(&info);

    if (LoadFile(&info, streamFilePath, FMODE_RB)) {
        streamBufferSize = info.fileSize;
        streamBuffer     = NULL;
        AllocateStorage((void **)&streamBuffer, info.fileSize, DATASET_MUS, false);
        ReadBytes(&info, streamBuffer, streamBufferSize);
        CloseFile(&info);

        if (streamBufferSize > 0) {
            vorbisAlloc.alloc_buffer_length_in_bytes = 512 * 1024; // 512KiB
            AllocateStorage((void **)&vorbisAlloc.alloc_buffer, 512 * 1024, DATASET_MUS, false);

            vorbisInfo = stb_vorbis_open_memory(streamBuffer, streamBufferSize, NULL, &vorbisAlloc);
            if (vorbisInfo) {
                if (streamStartPos)
                    stb_vorbis_seek(vorbisInfo, streamStartPos);
                UpdateStreamBuffer(channel);

                channel->state = CHANNEL_STREAM;
            }
        }
    }

    if (channel->state == CHANNEL_LOADING_STREAM)
        channel->state = CHANNEL_IDLE;
#endif
}

int32 RSDK::PlayStream(const char *filename, uint32 slot, uint32 startPos, uint32 loopPoint, bool32 loadASync)
{
#if RETRO_PLATFORM == RETRO_KALLISTIOS
    sprintf_s(streamFilePath, sizeof(streamFilePath), "%s/Data/Music/%s", KOS_USER_DIR, filename);
    stream_destroy();
    stream_create(streamFilePath, loopPoint);
    stream_play();

    return 0;
#else
    if (!engine.streamsEnabled)
        return -1;

    if (slot >= CHANNEL_COUNT) {
        for (int32 c = 0; c < CHANNEL_COUNT && slot >= CHANNEL_COUNT; ++c) {
            if (channels[c].soundID == -1 && channels[c].state != CHANNEL_LOADING_STREAM) {
                slot = c;
            }
        }

        // as a last resort, run through all channels
        // pick the channel closest to being finished
        if (slot >= CHANNEL_COUNT) {
            uint32 len = 0xFFFFFFFF;
            for (int32 c = 0; c < CHANNEL_COUNT; ++c) {
                if (channels[c].sampleLength < len && channels[c].state != CHANNEL_LOADING_STREAM) {
                    slot = c;
                    len  = (uint32)channels[c].sampleLength;
                }
            }
        }
    }

    if (slot >= CHANNEL_COUNT)
        return -1;

    ChannelInfo *channel = &channels[slot];

    LockAudioDevice();

    channel->soundID      = 0xFF;
    channel->loop         = loopPoint != 0;
    channel->priority     = 0xFF;
    channel->state        = CHANNEL_LOADING_STREAM;
    channel->pan          = 0.0f;
    channel->volume       = 1.0f;
    channel->sampleLength = sfxList[SFX_COUNT - 1].length;
    channel->samplePtr    = sfxList[SFX_COUNT - 1].buffer;
    channel->bufferPos    = 0;
    channel->speed        = TO_FIXED(1);

    sprintf_s(streamFilePath, sizeof(streamFilePath), "Data/Music/%s", filename);
    streamStartPos  = startPos;
    streamLoopPoint = loopPoint;

    AudioDevice::HandleStreamLoad(channel, loadASync);

    UnlockAudioDevice();

    return slot;
#endif
}

#define WAV_SIG_HEADER (0x46464952) // RIFF
#define WAV_SIG_DATA   (0x61746164) // data

#if 0
//RETRO_PLATFORM == RETRO_KALLISTIOS
struct snd_effect;
LIST_HEAD(selist, snd_effect);

typedef struct snd_effect {
    uint32_t  locl, locr;
    uint32_t  len;
    uint32_t  rate;
    uint32_t  used;
    uint32_t  fmt;
    uint16_t  stereo;

    LIST_ENTRY(snd_effect)  list;
} snd_effect_t;
#endif

void RSDK::LoadSfxToSlot(char *filename, uint8 slot, uint8 plays, uint8 scope)
{
    char fullFilePath[0x80];

    RETRO_HASH_MD5(hash);
    GEN_HASH_MD5(filename, hash);

#if RETRO_PLATFORM == RETRO_KALLISTIOS
    sprintf_s(fullFilePath, sizeof(fullFilePath), "%s/Data/SoundFX/%s", KOS_USER_DIR, filename);

    sfxhnd_t hnd = snd_sfx_load(fullFilePath);

    if (hnd != 0) {
        snd_effect_t *t = (snd_effect_t *)hnd;
        HASH_COPY_MD5(sfxList[slot].hash, hash);
        sfxList[slot].scope              = scope;
        sfxList[slot].maxConcurrentPlays = plays;
        sfxList[slot].length = t->len;
        sfxList[slot].handle = hnd;
        sfxList[slot].ratediv = 44100 / t->rate;
    }
#else
    FileInfo info;
    InitFileInfo(&info);

    sprintf_s(fullFilePath, sizeof(fullFilePath), "Data/SoundFX/%s", filename);

    if (LoadFile(&info, fullFilePath, FMODE_RB)) {
        HASH_COPY_MD5(sfxList[slot].hash, hash);
        sfxList[slot].scope              = scope;
        sfxList[slot].maxConcurrentPlays = plays;

        uint8 type = fullFilePath[strlen(fullFilePath) - 1];
        if (type == 'v' || type == 'V') { // A very loose way of checking that we're trying to load a '.wav' file.
            uint32 signature = ReadInt32(&info, false);

            if (signature == WAV_SIG_HEADER) {
                ReadInt32(&info, false); // chunk size
                ReadInt32(&info, false); // WAVE
                ReadInt32(&info, false); // FMT
#if !RETRO_USE_ORIGINAL_CODE
                int32 chunkSize = ReadInt32(&info, false); // chunk size
#else
                ReadInt32(&info, false); // chunk size
#endif
                ReadInt16(&info);        // audio format
                ReadInt16(&info);        // channels
                ReadInt32(&info, false); // sample rate
                ReadInt32(&info, false); // bytes per sec
                ReadInt16(&info);        // block align
                ReadInt16(&info);        // format

                Seek_Set(&info, 34);
                uint16 sampleBits = ReadInt16(&info);

#if !RETRO_USE_ORIGINAL_CODE
                // Original code added to help fix some issues
                Seek_Set(&info, 20 + chunkSize);
#endif

                // Find the data header
                int32 loop = 0;
                while (true) {
                    signature = ReadInt32(&info, false);
                    if (signature == WAV_SIG_DATA)
                        break;

                    loop += 4;
                    if (loop >= 0x40) {
                        if (loop != 0x100) {
                            CloseFile(&info);
                            // There's a bug here: `sfxList[id].scope` is not reset to `SCOPE_NONE`,
                            // meaning that the game will consider the SFX valid and allow it to be played.
                            // This can cause a crash because the SFX is incomplete.
#if !RETRO_USE_ORIGINAL_CODE
                            PrintLog(PRINT_ERROR, "Unable to read sfx: %s", filename);
#endif
                            return;
                        }
                        else {
                            break;
                        }
                    }
                }

                uint32 length = ReadInt32(&info, false);
                if (sampleBits == 16)
                    length /= 2;

                AllocateStorage((void **)&sfxList[slot].buffer, sizeof(float) * length, DATASET_SFX, false);
                sfxList[slot].length = length;

                // Convert the sample data to F32 format
                float *buffer = (float *)sfxList[slot].buffer;
                if (sampleBits == 8) {
                    // 8-bit sample. Convert from U8 to S8, and then from S8 to F32.
                    for (int32 s = 0; s < length; ++s) {
                        int32 sample = ReadInt8(&info);
                        *buffer++    = (sample - 0x80) / (float)0x80;
                    }
                }
                else {
                    // 16-bit sample. Convert from S16 to F32.
                    for (int32 s = 0; s < length; ++s) {
                        // For some reason, the game performs sign-extension manually here.
                        // Note that this is different from the 8-bit format's unsigned-to-signed conversion.
                        int32 sample = (uint16)ReadInt16(&info);

                        if (sample > 0x7FFF)
                            sample = (sample & 0x7FFF) - 0x8000;

                        *buffer++ = (sample / (float)0x8000) * 0.75f;
                    }
                }
            }
#if !RETRO_USE_ORIGINAL_CODE
            else {
                PrintLog(PRINT_ERROR, "Invalid header in sfx: %s", filename);
            }
#endif
        }
#if !RETRO_USE_ORIGINAL_CODE
        else {
            // what the
            PrintLog(PRINT_ERROR, "Could not find header in sfx: %s", filename);
        }
#endif
    }
#if !RETRO_USE_ORIGINAL_CODE
    else {
        PrintLog(PRINT_ERROR, "Unable to open sfx: %s", filename);
    }
#endif

    CloseFile(&info);
#endif
}

void RSDK::LoadSfx(char *filename, uint8 plays, uint8 scope)
{
    // Find an empty sound slot.
    uint16 id = -1;
    for (uint32 i = 0; i < SFX_COUNT; ++i) {
        if (sfxList[i].scope == SCOPE_NONE) {
            id = i;
            break;
        }
    }

    if (id != (uint16)-1)
        LoadSfxToSlot(filename, id, plays, scope);
}

int32 RSDK::PlaySfx(uint16 sfx, uint32 loopPoint, uint32 priority)
{
    if (sfx >= SFX_COUNT || !sfxList[sfx].scope)
        return -1;

    if (sfx == -1)
        return -1;

#if RETRO_PLATFORM == RETRO_KALLISTIOS
    if (sfxList[sfx].handle == (sfxhnd_t)0)
        return -1;

    int reservedChannel = snd_sfx_chn_alloc();
    if (reservedChannel == -1) {
        printf("PlaySfx: reservedChannel == -1\n");
        return -1;
    }

    sfx_play_data_t data = {0};
    // sound effect to play, by handle
    data.idx = sfxList[sfx].handle;
    data.chn = reservedChannel;
    // 192 is chosen as maximum value to avoid clipping/distortion in sfx playback
    data.vol = 192 * engine.soundFXVolume;
    // 127 is neutral/center pan
    data.pan = 127;
    // looping was causing problems
    data.loop = loopPoint != 0;
    data.loopstart = loopPoint;
    data.freq = AUDIO_FREQUENCY / sfxList[sfx].ratediv;

    // trying to have the sound engine actually work like the sw mixer
    uint8 count = 0;
    for (int32 c = 0; c < CHANNEL_COUNT; ++c) {
        if (channels[c].soundID == sfx)
            ++count;
    }

    printf("*** playing sfx id: %d, count: %d\n", sfx, count);
    
    int8 slot = -1;
    // if we've hit the max, replace the oldest one
    if (count >= sfxList[sfx].maxConcurrentPlays) {
        printf("***\tmaxConcurrent reached\n");
        int32 highestStackID = 0;
        for (int32 c = 0; c < CHANNEL_COUNT; ++c) {
            if (channels[c].soundID == sfx) {
                int32 stackID = sfxList[sfx].playCount - channels[c].playIndex;
                if (stackID > highestStackID) {//} && channels[c].soundID == sfx) {
                    printf("***\t\tdoing replacement %d -> %d\n", highestStackID, stackID);
                    slot           = c;
                    highestStackID = stackID;
                }
            }
        }
    }

    int need_to_free = 0;
    if (slot >= 0) {
//        snd_sfx_stop(channels[slot].aicaChannel);
        snd_sfx_chn_free(reservedChannel);
        printf("***\t\t\treplacement was going to go in aica ch # %d but now is playing on existing aica ch # %d\n", reservedChannel, channels[slot].aicaChannel);
        data.chn = channels[slot].aicaChannel;
//        return -1;
    }

    if (slot == -1) {
        // if we don't have a slot yet, try to pick any channel that's not currently playing
        for (int32 c = 0; c < CHANNEL_COUNT; ++c) {
            if (channels[c].soundID == -1) { //} && (channels[c].state == CHANNEL_SFX || channels[c].state == CHANNEL_IDLE)) { //!= CHANNEL_LOADING_STREAM) {
                printf("***\t\tempty channel %d\n", c);
                slot = c;
                break;
            }
        }
    }

    // as a last resort, run through all channels
    // pick the channel closest to being finished AND with lower priority
    if (slot < 0) {
        printf("***\t\tas a last resort, run through all channels\n");
        uint32 len = 0xFFFFFFFF;
        for (int32 c = 0; c < CHANNEL_COUNT; ++c) {
                if (channels[c].sampleLength < len &&  priority > channels[c].priority /* && (channels[c].state == CHANNEL_SFX || channels[c].state == CHANNEL_IDLE) */) { //!= CHANNEL_LOADING_STREAM) {
                printf("\t\t evicting channel %d for sfx %d\n", c, sfx);
                slot = c;
                len  = (uint32)channels[c].sampleLength;
                need_to_free = 1;
            }
        }
    }

    if (slot >= 0 && need_to_free) {
        if (channels[slot].aicaChannel != -1) {
            snd_sfx_stop(channels[slot].aicaChannel);
            snd_sfx_chn_free(channels[slot].aicaChannel);
            printf("***\t\t\tReleased AICA Channel %d\n", channels[slot].aicaChannel);
            channels[slot].aicaChannel = -1;
            channels[slot].zeroPosCount = -1;
        }
    }

    if (slot == -1) {
        snd_sfx_chn_free(reservedChannel);
        printf("PlaySfx: slot == -1\n");
        return -1;
    }

    channels[slot].state        = CHANNEL_SFX;
    channels[slot].bufferPos    = 0;
//    channels[slot].samplePtr    = sfxList[sfx].buffer;
    channels[slot].sampleLength = sfxList[sfx].length;
    channels[slot].volume       = 1.0f;
    channels[slot].pan          = 0.0f;
    channels[slot].speed        = TO_FIXED(1);
    channels[slot].soundID      = sfx;
    if (loopPoint >= 2)
        channels[slot].loop = loopPoint;
    else
        channels[slot].loop = 0; //loopPoint - 1;
    channels[slot].priority  = priority;
    channels[slot].playIndex = sfxList[sfx].playCount++;
    channels[slot].aicaChannel = data.chn;
    channels[slot].zeroPosCount = -1;

    snd_sfx_play_ex(&data);
    channels[slot].startNs = timer_ns_gettime64();

    return slot;
#else
    uint8 count = 0;
    for (int32 c = 0; c < CHANNEL_COUNT; ++c) {
        if (channels[c].soundID == sfx)
            ++count;
    }

    int8 slot = -1;
    // if we've hit the max, replace the oldest one
    if (count >= sfxList[sfx].maxConcurrentPlays) {
        int32 highestStackID = 0;
        for (int32 c = 0; c < CHANNEL_COUNT; ++c) {
            int32 stackID = sfxList[sfx].playCount - channels[c].playIndex;
            if (stackID > highestStackID && channels[c].soundID == sfx) {
                slot           = c;
                highestStackID = stackID;
            }
        }
    }

    // if we don't have a slot yet, try to pick any channel that's not currently playing
    for (int32 c = 0; c < CHANNEL_COUNT && slot < 0; ++c) {
        if (channels[c].soundID == -1 && channels[c].state != CHANNEL_LOADING_STREAM) {
            slot = c;
        }
    }

    // as a last resort, run through all channels
    // pick the channel closest to being finished AND with lower priority
    if (slot < 0) {
        uint32 len = 0xFFFFFFFF;
        for (int32 c = 0; c < CHANNEL_COUNT; ++c) {
            if (channels[c].sampleLength < len && priority > channels[c].priority && channels[c].state != CHANNEL_LOADING_STREAM) {
                slot = c;
                len  = (uint32)channels[c].sampleLength;
            }
        }
    }

    if (slot == -1)
        return -1;

    LockAudioDevice();

    channels[slot].state        = CHANNEL_SFX;
    channels[slot].bufferPos    = 0;
    channels[slot].samplePtr    = sfxList[sfx].buffer;
    channels[slot].sampleLength = sfxList[sfx].length;
    channels[slot].volume       = 1.0f;
    channels[slot].pan          = 0.0f;
    channels[slot].speed        = TO_FIXED(1);
    channels[slot].soundID      = sfx;
    if (loopPoint >= 2)
        channels[slot].loop = loopPoint;
    else
        channels[slot].loop = loopPoint - 1;
    channels[slot].priority  = priority;
    channels[slot].playIndex = sfxList[sfx].playCount++;

    UnlockAudioDevice();

    return slot;
#endif
}

void RSDK::SetChannelAttributes(uint8 channel, float volume, float panning, float speed)
{
//    return;
#if RETRO_PLATFORM == RETRO_KALLISTIOS
    if ((channel < CHANNEL_COUNT) && (channels[channel].state == CHANNEL_SFX)) {
        volume                   = fminf(4.0f, volume);
        volume                   = fmaxf(0.0f, volume);
        channels[channel].volume = volume;

        panning               = fminf(1.0f, panning);
        panning               = fmaxf(-1.0f, panning);
        channels[channel].pan = panning;

        if (speed > 0.0f)
            channels[channel].speed = (int32)(speed * TO_FIXED(1));
        else if (speed == 1.0f)
            channels[channel].speed = TO_FIXED(1);

        sfx_play_data_t data = {0};
        // sound effect to play, by handle
        data.idx = sfxList[channels[channel].soundID].handle;
        data.chn = channels[channel].aicaChannel;
        // 192 is chosen as maximum value to avoid clipping/distortion in sfx playback
        data.vol = 192 * (channels[channel].volume /* * 0.25f */) * engine.soundFXVolume;
        // 127 is neutral/center pan
        data.pan = 127 * (channels[channel].pan + 1.0f);
        // looping was causing problems
        data.loop = channels[channel].loop != 0;
        data.loopstart = channels[channel].loop;
        data.freq = /* channels[channel]. */speed * (AUDIO_FREQUENCY / sfxList[channels[channel].soundID].ratediv);
        snd_sfx_update_ex(&data);
        printf("%%%%%% set channel %d to freq %d vol %d pan %d\n", channel, data.freq, data.vol, data.pan);
    }
#endif
#if RETRO_PLATFORM != RETRO_KALLISTIOS
    if (channel < CHANNEL_COUNT) {
        volume                   = fminf(4.0f, volume);
        volume                   = fmaxf(0.0f, volume);
        channels[channel].volume = volume;

        panning               = fminf(1.0f, panning);
        panning               = fmaxf(-1.0f, panning);
        channels[channel].pan = panning;

        if (speed > 0.0f)
            channels[channel].speed = (int32)(speed * TO_FIXED(1));
        else if (speed == 1.0f)
            channels[channel].speed = TO_FIXED(1);
    }
#endif
}

uint32 RSDK::GetChannelPos(uint32 channel)
{
#if RETRO_PLATFORM == RETRO_KALLISTIOS
    if (channel >= CHANNEL_COUNT)
        return 0;

    if (channels[channel].state == CHANNEL_SFX) {
        int aicaChannel = channels[channel].aicaChannel;
        if (aicaChannel != -1) {
            return snd_get_pos(aicaChannel);
        }
    }

#endif
#if RETRO_PLATFORM != RETRO_KALLISTIOS
    if (channel >= CHANNEL_COUNT)
        return 0;

    if (channels[channel].state == CHANNEL_SFX)
        return channels[channel].bufferPos;

    if (channels[channel].state == CHANNEL_STREAM) {
        if (!vorbisInfo->current_loc_valid || vorbisInfo->current_loc < 0)
            return 0;

        return vorbisInfo->current_loc;
    }
#endif

    return 0;
}

double RSDK::GetVideoStreamPos()
{
#if RETRO_PLATFORM != RETRO_KALLISTIOS
    if (channels[0].state == CHANNEL_STREAM && AudioDevice::audioState && AudioDevice::initializedAudioChannels && vorbisInfo->current_loc_valid) {
        return vorbisInfo->current_loc / (double)AUDIO_FREQUENCY;
    }
#endif

    return -1.0;
}

void RSDK::ClearStageSfx()
{
#if RETRO_PLATFORM == RETRO_KALLISTIOS
    stream_destroy();

    for (int i = 0; i < 16; i++) {
        if (channels[i].aicaChannel != -1) {
            snd_sfx_stop(channels[i].aicaChannel);
            snd_sfx_chn_free(channels[i].aicaChannel);
        }
    }
#endif
    LockAudioDevice();

    for (int32 c = 0; c < CHANNEL_COUNT; ++c) {
        if (channels[c].state == CHANNEL_SFX || channels[c].state == (CHANNEL_SFX | CHANNEL_PAUSED)) {
            channels[c].soundID = -1;
            channels[c].state   = CHANNEL_IDLE;
#if RETRO_PLATFORM == RETRO_KALLISTIOS
            channels[c].aicaChannel = -1;
            channels[c].startNs = 0xFFFFFFFFFFFFFFFFL;
#endif
        }
    }

    // Unload stage SFX
    for (int32 s = 0; s < SFX_COUNT; ++s) {
        if (sfxList[s].scope >= SCOPE_STAGE) {
#if RETRO_PLATFORM == RETRO_KALLISTIOS
            snd_sfx_unload(sfxList[s].handle);
#endif
            MEM_ZERO(sfxList[s]);
            sfxList[s].scope = SCOPE_NONE;
        }
    }

    UnlockAudioDevice();
}

#if RETRO_USE_MOD_LOADER
void RSDK::ClearGlobalSfx()
{
#if RETRO_PLATFORM == RETRO_KALLISTIOS
    stream_destroy();

    for (int i = 0; i < 16; i++) {
        if (channels[i].aicaChannel != -1) {
            snd_sfx_stop(channels[i].aicaChannel);
            snd_sfx_chn_free(channels[i].aicaChannel);
        }
    }
#endif
    LockAudioDevice();

    for (int32 c = 0; c < CHANNEL_COUNT; ++c) {
        if (channels[c].state == CHANNEL_SFX || channels[c].state == (CHANNEL_SFX | CHANNEL_PAUSED)) {
            channels[c].soundID = -1;
            channels[c].state   = CHANNEL_IDLE;
#if RETRO_PLATFORM == RETRO_KALLISTIOS
            channels[c].aicaChannel = -1;
            channels[c].startNs = 0xFFFFFFFFFFFFFFFFL;
#endif
        }
    }

    // Unload global SFX
    for (int32 s = 0; s < SFX_COUNT; ++s) {
        // clear global sfx (do NOT clear the stream channel 0 slot)
        if (sfxList[s].scope == SCOPE_GLOBAL && s != SFX_COUNT - 1) {
#if RETRO_PLATFORM == RETRO_KALLISTIOS
            snd_sfx_unload(sfxList[s].handle);
#endif
            MEM_ZERO(sfxList[s]);
            sfxList[s].scope = SCOPE_NONE;
        }
    }

    UnlockAudioDevice();
}
#endif
