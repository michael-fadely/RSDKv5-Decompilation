#if RETRO_PLATFORM == RETRO_KALLISTIOS
extern "C" {

/**
 * Simple streaming audio player, derived from:
 * https://github.com/Dreamcast-Projects/libwav.git
 * 
 * Libwav is CC0/Public Domain code. So is this code.
 * 
 * The only format supported is as follows:
 * raw (headerless), interleaved 8-bit stereo PCM @ 22kHz
 *
 * The rationale behind the format choice is as follows:
 *  1) Sonic Mania requires streams to have functional loop points for seamless music.
 *  2) Music must stream from CD-R without causing any noticeable impact on game performance.
 *  3) 44khz stereo ADPCM is known to stream from CD-R without performance impact;
 *     Dreamcast ports of Doom 64 and Wipeout are notable examples.
 *  4) ADPCM streams *do not* support seamless looping to arbitrary stream positions
 *     due to decoding/predictor state issues.
 *  5) PCM streams *do* support seamless looping to arbitrary stream positions.
 *  6) The math for data transfer rate matches between 44khz 4-bit and 22khz 8-bit data
 *     given the same sample rate.
 *
 * See Dreamcast-specific documentation in the top-level `Sonic-Mania-Decompilation` repo for more details.
 */

#include <kos.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>

#include <kos/thread.h>
#include <dc/sound/stream.h>

#include <sys/cdefs.h>
__BEGIN_DECLS

#include <kos/fs.h>

// the following two defines have values pulled in from Audio.hpp
#define STREAM_SAMPLE_RATE (AUDIO_FREQUENCY)
#define STREAM_CHANNELS (AUDIO_CHANNELS)
#define STREAM_BUF_SIZE (8192)

long stream_data_length;

int stream_init(void);
void stream_shutdown(void);
void stream_destroy(void);

int stream_create(const char *filename, int loop);

void stream_play(void);
void stream_pause(void);
void stream_stop(void);
void stream_volume(int vol);
int stream_is_playing(void);

__END_DECLS
/* Keep track of things from the Driver side */
#define SNDDRV_STATUS_NULL 0x00
#define SNDDRV_STATUS_READY 0x01
#define SNDDRV_STATUS_DONE 0x02

/* Keep track of things from the Decoder side */
#define SNDDEC_STATUS_NULL 0x00
#define SNDDEC_STATUS_READY 0x01
#define SNDDEC_STATUS_STREAMING 0x02
#define SNDDEC_STATUS_PAUSING 0x03
#define SNDDEC_STATUS_STOPPING 0x04
#define SNDDEC_STATUS_RESUMING 0x05

typedef void *(*snddrv_cb)(snd_stream_hnd_t, int, int *);

typedef struct
{
    /* The buffer on the AICA side */
    snd_stream_hnd_t shnd;

    /* We either read the stream data from a file or
       we read from a buffer */
    FileIO *stream_file;

    /* Contains the buffer that we are going to send
       to the AICA in the callback.  Should be 32-byte
       aligned */
    uint8_t __attribute__((aligned(32))) drv_buf[STREAM_BUF_SIZE];

    /* Status of the stream that can be started, stopped
       paused, ready. etc */
    volatile int status;

    snddrv_cb callback;

    uint32_t loop;
    uint32_t vol; /* 0-255 */

    /* The length of the audio data */
    uint32_t data_length;
} snddrv_hnd;

static snddrv_hnd stream;
static volatile int sndstream_status = SNDDRV_STATUS_NULL;
static kthread_t *audio_thread;
static kthread_attr_t audio_attr;
static mutex_t stream_mutex;

static void *sndstream_thread(void *param);
static void *stream_file_callback(snd_stream_hnd_t hnd, int req, int *done);

int stream_init(void)
{
    if (snd_stream_init() < 0)
        return 0;

    mutex_init(&stream_mutex, MUTEX_TYPE_NORMAL);

    stream.shnd = SND_STREAM_INVALID;
    stream.vol = 0;
    stream.status = SNDDEC_STATUS_NULL;
    stream.callback = NULL;
    audio_attr.create_detached = 0;
    audio_attr.stack_size = 32768;
    audio_attr.stack_ptr = NULL;
    audio_attr.prio = PRIO_DEFAULT;
    audio_attr.label = "MusicPlayer";

    audio_thread = thd_create_ex(&audio_attr, sndstream_thread, NULL);
    if (audio_thread != NULL)
        sndstream_status = SNDDRV_STATUS_READY;

    return sndstream_status;
}

void stream_shutdown(void)
{
    sndstream_status = SNDDRV_STATUS_DONE;

    thd_join(audio_thread, NULL);

    stream_destroy();
}

void stream_destroy(void)
{
    if (stream.shnd == SND_STREAM_INVALID)
        return;

    mutex_lock(&stream_mutex);

    snd_stream_destroy(stream.shnd);
    stream.shnd = SND_STREAM_INVALID;
    stream.status = SNDDEC_STATUS_NULL;
    stream.vol = 0;
    stream.callback = NULL;

    if (stream.stream_file != NULL) {
        fClose(stream.stream_file);
        stream.stream_file = NULL;
    }

    mutex_unlock(&stream_mutex);
}

int stream_create(const char *filename, int loop)
{
    FileIO *file;
    int index;

    if (filename == NULL)
        return SND_STREAM_INVALID;

    file = fOpen(filename, "r");

    if (file == NULL)
        return SND_STREAM_INVALID;

    index = snd_stream_alloc(stream_file_callback, STREAM_BUF_SIZE);
    if (index == SND_STREAM_INVALID) {
        fClose(file);
        snd_stream_destroy(index);
        return SND_STREAM_INVALID;
    }

    if (fSeek(file, 0, SEEK_END) != 0) {
        fClose(file);
        snd_stream_destroy(index);
        return SND_STREAM_INVALID;
    }

    long stream_data_length = fTell(file);

    if (stream_data_length == -1L) {
        fClose(file);
        snd_stream_destroy(index);
        return SND_STREAM_INVALID;
    }

    stream.shnd = index;
    stream.stream_file = file;
    stream.loop = loop;
    stream.callback = stream_file_callback;
    stream.vol = 192 * engine.streamVolume;
    stream.data_length = stream_data_length;
    if (fSeek(stream.stream_file, 0, SEEK_SET) != 0) {
        fClose(file);
        snd_stream_destroy(index);
        return SND_STREAM_INVALID;
    }
    snd_stream_volume(stream.shnd, stream.vol);
    stream.status = SNDDEC_STATUS_READY;

    return index;
}

void stream_play(void)
{
    if (stream.status == SNDDEC_STATUS_STREAMING)
        return;

    stream.status = SNDDEC_STATUS_RESUMING;
}

void stream_pause(void)
{
    if (stream.status == SNDDEC_STATUS_READY || stream.status == SNDDEC_STATUS_PAUSING)
        return;

    stream.status = SNDDEC_STATUS_PAUSING;
}

void stream_stop(void)
{
    if (stream.status == SNDDEC_STATUS_READY || stream.status == SNDDEC_STATUS_STOPPING)
        return;

    stream.status = SNDDEC_STATUS_STOPPING;
}

void stream_volume(int vol)
{
    if (stream.shnd == SND_STREAM_INVALID)
        return;

    if (vol > 255)
        vol = 255;

    if (vol < 0)
        vol = 0;

    stream.vol = vol * engine.streamVolume;
    snd_stream_volume(stream.shnd, stream.vol);
}

int stream_is_playing(void)
{
    return (stream.status == SNDDEC_STATUS_STREAMING) || (stream.status == SNDDEC_STATUS_RESUMING);
}

static void *sndstream_thread(void *param)
{
    (void)param;

    while (sndstream_status != SNDDRV_STATUS_DONE) {
        mutex_lock(&stream_mutex);

        switch (stream.status) {
        case SNDDEC_STATUS_RESUMING:
            if (stream.shnd != SND_STREAM_INVALID) {
                snd_stream_volume(stream.shnd, stream.vol);
                snd_stream_start_pcm8(stream.shnd, STREAM_SAMPLE_RATE, STREAM_CHANNELS - 1);
                snd_stream_volume(stream.shnd, stream.vol);
                stream.status = SNDDEC_STATUS_STREAMING;
            } else {
                stream.status = SNDDEC_STATUS_READY;
            }
            break;
        case SNDDEC_STATUS_PAUSING:
            if (stream.shnd != SND_STREAM_INVALID) {
                snd_stream_stop(stream.shnd);
            }
            stream.status = SNDDEC_STATUS_READY;
            break;
        case SNDDEC_STATUS_STOPPING:
            if (stream.shnd != SND_STREAM_INVALID) {
                snd_stream_stop(stream.shnd);
                if (stream.stream_file != NULL) {
                    // ok if this fails, we are stopping anyway
                    fSeek(stream.stream_file, 0, SEEK_SET);
                }
            }
            stream.status = SNDDEC_STATUS_READY;
            break;
        case SNDDEC_STATUS_STREAMING:
            if (stream.shnd != SND_STREAM_INVALID) {
                snd_stream_poll(stream.shnd);
            }
            break;
        case SNDDEC_STATUS_READY:
        default:
            break;
        }

        mutex_unlock(&stream_mutex);
        thd_sleep(20);
    }

    return NULL;
}

static void *stream_file_callback(snd_stream_hnd_t hnd, int req, int *done)
{
    (void)hnd;
    size_t read = fRead(stream.drv_buf, 1, req, stream.stream_file);

    if (read != req) {
        if (fError(stream.stream_file)) {
            if (stream.shnd != SND_STREAM_INVALID) {
                snd_stream_stop(stream.shnd);
            }
            stream.status = SNDDEC_STATUS_READY;
            return NULL;
        } else if (stream.loop) {
            int seek1 = fSeek(stream.stream_file, (off_t)stream.loop, SEEK_SET);
            if (seek1 != 0) {
                if (stream.shnd != SND_STREAM_INVALID) {
                    snd_stream_stop(stream.shnd);
                }
                stream.status = SNDDEC_STATUS_READY;
                return NULL;
            }

            size_t read2 = fRead(stream.drv_buf + read, 1, req - read, stream.stream_file);

            if (read2 != (req - read)) {
                if (stream.shnd != SND_STREAM_INVALID) {
                    snd_stream_stop(stream.shnd);
                }
                stream.status = SNDDEC_STATUS_READY;
                return NULL;
            }
        } else {
            if (stream.shnd != SND_STREAM_INVALID) {
                snd_stream_stop(stream.shnd);
            }
            stream.status = SNDDEC_STATUS_READY;
            return NULL;
        }
    }

    *done = req;

    return stream.drv_buf;
}

} // extern "C"
#endif