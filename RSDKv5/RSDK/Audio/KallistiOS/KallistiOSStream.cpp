extern "C" {
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

typedef struct {
    uint32_t format;
    uint32_t channels;
    uint32_t sample_rate;
    uint32_t sample_size;
    uint32_t data_offset;
    uint32_t data_length;
} StreamFileInfo;

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
    file_t stream_file;

    /* Contains the buffer that we are going to send
       to the AICA in the callback.  Should be 32-byte
       aligned */
    uint8_t __attribute__((aligned(32))) drv_buf[16384];

    /* Status of the stream that can be started, stopped
       paused, ready. etc */
    volatile int status;

    snddrv_cb callback;

    uint32_t loop;
    uint32_t vol; /* 0-255 */

    uint32_t format;      /* stream format */
    uint32_t channels;      /* 1-Mono/2-Stereo */
    uint32_t sample_rate; /* 44100Hz */
    uint32_t sample_size; /* 4/8/16-Bit */

    /* Offset into the file or buffer where the audio
       data starts */
    uint32_t data_offset;

    /* The length of the audio data */
    uint32_t data_length;

    /* Used only in reading stream data from a buffer
       and not a file */
    uint32_t buf_offset;

} snddrv_hnd;

static snddrv_hnd stream;
static volatile int sndstream_status = SNDDRV_STATUS_NULL;
static kthread_t *audio_thread;
static kthread_attr_t audio_attr;
static mutex_t stream_mutex;

static void *sndstream_thread(void *param);
static void *stream_file_callback(snd_stream_hnd_t hnd, int req, int *done);

extern mutex_t io_lock;

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
    audio_attr.stack_size = 65536;
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

    if (stream.stream_file != FILEHND_INVALID) {
        mutex_lock(&io_lock);
        fs_close(stream.stream_file);
        mutex_unlock(&io_lock);
    }

    mutex_unlock(&stream_mutex);
}

#define WAVE_FORMAT_PCM                   0x0001 /* PCM */

static int stream_get_info_pcm(file_t file, StreamFileInfo *result) {
    result->format = WAVE_FORMAT_PCM;
    result->channels = 2;
    result->sample_rate = 22050;
    result->sample_size = 2;
    mutex_lock(&io_lock);
    result->data_length = fs_total(file);
    mutex_unlock(&io_lock);
    result->data_offset = 0;

    return 1;
}

int stream_create(const char *filename, int loop)
{
    file_t file;
    StreamFileInfo info;
    int index;

    if (filename == NULL)
        return SND_STREAM_INVALID;

    mutex_lock(&io_lock);
    file = fs_open(filename, O_RDONLY);
    mutex_unlock(&io_lock);

    if (file == FILEHND_INVALID)
        return SND_STREAM_INVALID;

    index = snd_stream_alloc(stream_file_callback, 8192);
    if (index == SND_STREAM_INVALID) {
        mutex_lock(&io_lock);
        fs_close(file);
        mutex_unlock(&io_lock);
        snd_stream_destroy(index);
        return SND_STREAM_INVALID;
    }

    stream_get_info_pcm(file, &info);

    stream.shnd = index;
    stream.stream_file = file;
    stream.loop = loop;
    stream.callback = stream_file_callback;
    stream.vol = 192 * engine.streamVolume;
    stream.format = info.format;
    stream.channels = info.channels;
    stream.sample_rate = info.sample_rate;
    stream.sample_size = info.sample_size;
    stream.data_length = info.data_length;
    stream.data_offset = info.data_offset;
    mutex_lock(&io_lock);
    fs_seek(stream.stream_file, stream.data_offset, SEEK_SET);
    mutex_unlock(&io_lock);
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
    return stream.status == SNDDEC_STATUS_STREAMING;
}

static void *sndstream_thread(void *param)
{
    (void)param;

    while (sndstream_status != SNDDRV_STATUS_DONE) {
        mutex_lock(&stream_mutex);

        switch (stream.status) {
        case SNDDEC_STATUS_RESUMING:
            snd_stream_volume(stream.shnd, stream.vol);
            snd_stream_start_pcm8(stream.shnd, stream.sample_rate, stream.channels - 1);
            snd_stream_volume(stream.shnd, stream.vol);
            stream.status = SNDDEC_STATUS_STREAMING;
            break;
        case SNDDEC_STATUS_PAUSING:
            snd_stream_stop(stream.shnd);
            stream.status = SNDDEC_STATUS_READY;
            break;
        case SNDDEC_STATUS_STOPPING:
            snd_stream_stop(stream.shnd);
            if (stream.stream_file != FILEHND_INVALID)
                fs_seek(stream.stream_file, stream.data_offset, SEEK_SET);
            else
                stream.buf_offset = stream.data_offset;

            stream.status = SNDDEC_STATUS_READY;
            break;
        case SNDDEC_STATUS_STREAMING:
            snd_stream_poll(stream.shnd);
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
    mutex_lock_scoped(&io_lock);
    (void)hnd;
    ssize_t read = fs_read(stream.stream_file, stream.drv_buf, req);

    if (read == -1) {
        snd_stream_stop(stream.shnd);
        stream.status = SNDDEC_STATUS_READY;
        return NULL;
    }

    if (read != req) {

        if (stream.loop) {
            fs_seek(stream.stream_file, stream.data_offset + stream.loop, SEEK_SET);
            ssize_t read2 = fs_read(stream.stream_file, stream.drv_buf + read, req - read);

            if (read2 == -1) {
                snd_stream_stop(stream.shnd);
                stream.status = SNDDEC_STATUS_READY;
                return NULL;
            }
        } else {
            snd_stream_stop(stream.shnd);
            stream.status = SNDDEC_STATUS_READY;
            return NULL;
        }
    }

    *done = req;

    return stream.drv_buf;
}

} // extern "C"