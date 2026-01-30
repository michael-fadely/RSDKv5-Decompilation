#ifndef __MPEG_C
#define __MPEG_C
#include <kos.h>
#define PL_MPEG_IMPLEMENTATION
#include "pl_mpeg.h"
#include "mpeg.h"

struct mpeg_player_t {
    /* MPEG decoder */
    plm_t *decoder;

    /* SH4 side sound buffer */
    uint32_t *snd_buf;

    /* Texture that holds decoded data */
    pvr_ptr_t texture;

    /* Width of the video in pixels */
    int width;

    /* Height of the video in pixels */
    int height;

    /* Start position for sound module playback */
    int snd_mod_start;

    /* Size of the sound module data */
    int snd_mod_size;

    /* Audio sample rate */
    int sample_rate;

    /* Sound stream handle */
    snd_stream_hnd_t snd_hnd;

    /* Current video playback time */
    float video_time;

    /* Current audio playback time */
    float audio_time;

    /* Polygon header for rendering */
    pvr_poly_hdr_t hdr;

    /* Vertices for rendering the video frame */
    pvr_vertex_t vert[4];
};

/* Output texture width and height initial values
   You can choose from 32, 64, 128, 256, 512, 1024 */
static int mpeg_texture_width;
static int mpeg_texture_height;

/* Size of the sound buffer for both the SH4 side and the AICA side */
#define SOUND_BUFFER (64 * 1024)

static void upload_frame(plm_frame_t *frame);
static void draw_frame(mpeg_player_t *player);
static int setup_graphics(mpeg_player_t *player);
static int setup_audio(mpeg_player_t *player);
static void fast_memcpy(void *dest, const void *src, size_t length);

uint32_t next_power_of_two(uint32_t n) {
    if(n == 0)
        return 1;

    n--;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    n++;

    return n;
}

mpeg_player_t *mpeg_player_create(const char *filename) {
    mpeg_player_t *player = NULL;

    if(!filename) {
        fprintf(stderr, "filename is NULL\n");
        return NULL;
    }

    player = (mpeg_player_t*)calloc(1, sizeof(mpeg_player_t));
    if(!player) {
        fprintf(stderr, "Out of memory for player\n");
        return NULL;
    }

    player->decoder = plm_create_with_filename(filename);
    if(!player->decoder) {
        fprintf(stderr, "Out of memory for player->decoder\n");
        mpeg_player_destroy(player);
        return NULL;
    }
#if 0
    player->snd_buf = memalign(32, SOUND_BUFFER);
    if(!player->snd_buf) {
        fprintf(stderr, "Out of memory for player->snd_buf\n");
        mpeg_player_destroy(player);
        return NULL;
    }
#endif
    if(setup_graphics(player) < 0) {
        fprintf(stderr, "Setting up graphics failed\n");
        mpeg_player_destroy(player);
        return NULL;
    }
#if 0
    if(setup_audio(player) < 0) {
        fprintf(stderr, "Setting up audio failed\n");
        mpeg_player_destroy(player);
        return NULL;
    }
#endif
    return player;
}

mpeg_player_t *mpeg_player_create_memory(uint8_t *memory, const size_t length) {
    mpeg_player_t *player = NULL;

    if(!memory) {
        fprintf(stderr, "memory is NULL\n");
        return NULL;
    }

    player = (mpeg_player_t*)malloc(sizeof(mpeg_player_t));
    if(!player) {
        fprintf(stderr, "Out of memory for player\n");
        return NULL;
    }

    player->decoder = plm_create_with_memory(memory, length, 1);
    if(!player->decoder) {
        fprintf(stderr, "Out of memory for player->decoder\n");
        mpeg_player_destroy(player);
        return NULL;
    }
#if 0
    player->snd_buf = memalign(32, SOUND_BUFFER);
    if(!player->snd_buf) {
        fprintf(stderr, "Out of memory for player->snd_buf\n");
        mpeg_player_destroy(player);
        return NULL;
    }
#endif
    if(setup_graphics(player) < 0) {
        fprintf(stderr, "Setting up graphics failed\n");
        mpeg_player_destroy(player);
        return NULL;
    }
#if 0
    if(setup_audio(player) < 0) {
        fprintf(stderr, "Setting up audio failed\n");
        mpeg_player_destroy(player);
        return NULL;
    }
#endif
    return player;
}


int mpeg_player_get_loop(mpeg_player_t *player) {
    return plm_get_loop(player->decoder);
}

void mpeg_player_set_loop(mpeg_player_t *player, int loop) {
    plm_set_loop(player->decoder, loop);
}

void mpeg_player_destroy(mpeg_player_t *player) {
    if(!player)
        return;

    if(player->decoder)
        plm_destroy(player->decoder);

    if(player->texture)
        pvr_mem_free(player->texture);

#if 0
    if(player->snd_buf)
        free(player->snd_buf);

    if(player->snd_hnd != SND_STREAM_INVALID)
        snd_stream_destroy(player->snd_hnd);
#endif

    free(player);
    player = NULL;
}

int mpeg_play(mpeg_player_t *player, uint32_t cancel_buttons) {
    int decoded;
    int cancel = 0;
    plm_frame_t *frame;

    if(!player || !player->decoder)
        return -1;
#if 0
    /* Init sound stream. */
    snd_stream_start(player->snd_hnd, player->sample_rate, 0);
#endif
    while(!cancel) {
        /* Check cancel buttons. */
        MAPLE_FOREACH_BEGIN(MAPLE_FUNC_CONTROLLER, cont_state_t, st)
        if(cancel_buttons && ((st->buttons & cancel_buttons) == cancel_buttons)) {
            cancel = 1; /* Push cancel buttons */
            break;
        }
        if(st->buttons == 0x60e) {
            cancel = 2; /* ABXY + START (Software reset) */
            break;
        }
        MAPLE_FOREACH_END()

        /* Decode */
        if(player->audio_time >= player->video_time) {
            //uint64_t start = timer_ns_gettime64();
            frame = plm_decode_video(player->decoder);
            //uint64_t end = timer_ns_gettime64();
            //printf("%llu,", end-start);
            if(!frame) {
                /* Are we looping? */
                if(plm_get_loop(player->decoder)) {
                    /* Restart all audio timings and buffers */
                    player->snd_mod_size = 0;
                    player->snd_mod_start = 0;
                    player->audio_time = 0.0f;
#if 0
                    snd_stream_stop(player->snd_hnd);
                    snd_stream_start(player->snd_hnd, player->sample_rate, 0);
#endif
                    /* Need to decode video again */
                    frame = plm_decode_video(player->decoder);
                } else /* Exit */
                    break;
            }
            decoded = 1;
            player->video_time = frame->time;
        }
#if 0
        snd_stream_poll(player->snd_hnd);
#endif
        /* Render */
//        pvr_wait_ready();
//        pvr_scene_begin();

        if(decoded) {
            upload_frame(frame);
            decoded = 0;
        }

//        pvr_list_begin(PVR_LIST_OP_POLY);

        draw_frame(player);

        pvr_list_finish();
        pvr_scene_finish();
    }
#if 0
    /* Reset some stuff */
    player->snd_mod_size = 0;
    player->snd_mod_start = 0;
#endif
    player->audio_time = 0.0f;
    player->video_time = 0.0f;
#if 0
    snd_stream_stop(player->snd_hnd);
#endif
    return cancel;
}

void mpeg_process(mpeg_player_t *player, int *done) {
    int decoded = 0;
    int cancel = 0;
    plm_frame_t *frame;

    if(!player || !player->decoder) {
        printf("early return player %08x decoder %08x\n", player, player ? player->decoder : NULL);
        *done = 1;
        return;
    }

        /* Decode */
//        if(player->audio_time >= player->video_time) {
            //uint64_t start = timer_ns_gettime64();
            frame = plm_decode_video(player->decoder);
            //uint64_t end = timer_ns_gettime64();
            //printf("%llu,", end-start);
            if (!frame) {
                cancel = 1;
            } else {
                decoded = 1;
                decoded = 1;
                player->video_time = frame->time;
            }
#if 0
            if(!frame) {
                /* Are we looping? */
                if(plm_get_loop(player->decoder)) {
                    /* Restart all audio timings and buffers */
                    player->snd_mod_size = 0;
                    player->snd_mod_start = 0;
                    player->audio_time = 0.0f;
                    /* Need to decode video again */
                    frame = plm_decode_video(player->decoder);
                } else {
                    cancel = 1;
                }
            }
#endif
  //      }

        if(decoded) {
            upload_frame(frame);
            decoded = 0;
        }

        draw_frame(player);

    *done = cancel;
}




static void upload_frame(plm_frame_t *frame) {
    uint32_t *src;
    int x, y, w, h, i;

    if(!frame)
        return;

    src = frame->display;

    /* Calculate number of megablocks based on frame size */
    w = frame->width >> 4;
    h = frame->height >> 4;

    int const min_blocks_x = 32 * (frame->width / 320) - w;
    //int const min_blocks_y = 16 * (frame->height / 240) - h;

    uint32_t *d = SQ_MASK_DEST((void *)PVR_TA_YUV_CONV);
    sq_lock((void *)PVR_TA_YUV_CONV);

    /* For each row of megablocks */
    for(y = 0; y < h; y++) {
        /* Copy one megablock from src to the YUV convertor */
        for(x = 0; x < w; x++, src += 96) {
            sq_fast_cpy(d, src, 384/32);
        }

        /* Send a dummy megablock if required for padding */
        for(i = 0; i < min_blocks_x * 384/32; i++) {
            //d[0] = d[1] = d[2] = d[3] = d[4] = d[5] = d[6] = d[7] = 0;
            sq_flush(d);
            //d += 8;
        }
    }

    sq_unlock();

    /* Send a row of dummy megablocks if required for padding */
    // for(i = 0; i < min_blocks_y; i++) {
    //      sq_set((void *)PVR_TA_YUV_CONV, 0, 384 * 32);
    // }
}

static void draw_frame(mpeg_player_t *player) {
    pvr_prim(&player->hdr, sizeof(pvr_poly_hdr_t));
    pvr_prim(&player->vert, 128);
//    sq_lock((void *)PVR_TA_INPUT);
//    sq_fast_cpy(SQ_MASK_DEST(PVR_TA_INPUT), &player->vert, (sizeof(pvr_vertex_t) * 4)/32);
//    sq_unlock();
}

static int setup_graphics(mpeg_player_t *player) {
    pvr_poly_cxt_t cxt;
    float u, v;
    int color = PVR_PACK_COLOR(1.0f, 1.0f, 1.0f, 1.0f);

    player->width = plm_get_width(player->decoder);
    player->height = plm_get_height(player->decoder);

    mpeg_texture_width = next_power_of_two(player->width);
    mpeg_texture_height = next_power_of_two(player->height);

    if(!(player->texture = pvr_mem_malloc(mpeg_texture_width * mpeg_texture_height * 2))) {
        fprintf(stderr, "Failed to allocate PVR memory!\n");
        return -1;
    }

    /* Set SQ to YUV converter. */
    PVR_SET(PVR_YUV_ADDR, (((uint32_t)player->texture) & 0xffffff));
    /* Divide texture width and texture height by 16 and subtract 1.
       The actual values to set are 1, 3, 7, 15, 31, 63. */
    PVR_SET(PVR_YUV_CFG, (((mpeg_texture_height / 16) - 1) << 8) |
                          ((mpeg_texture_width / 16) - 1));
    PVR_GET(PVR_YUV_CFG);

    /* Clear texture to black */
    sq_set(player->texture, 0, mpeg_texture_width * mpeg_texture_height * 2);

    pvr_poly_cxt_txr(&cxt, PVR_LIST_PT_POLY,
                     PVR_TXRFMT_YUV422 | PVR_TXRFMT_NONTWIDDLED,
                     mpeg_texture_width, mpeg_texture_height,
                     player->texture,
                     PVR_FILTER_BILINEAR);
    pvr_poly_compile(&player->hdr, &cxt);

    player->video_time = 0.0f;

    u = (float)player->width / mpeg_texture_width;
    v = (float)player->height / mpeg_texture_height;

    player->vert[0].x = 0.0f;
    player->vert[0].y = 0.0f;
    player->vert[0].z = 1.0f;
    player->vert[0].u = 0.0f;
    player->vert[0].v = 0.0f;
    player->vert[0].argb = color;
    player->vert[0].oargb = 0;
    player->vert[0].flags = PVR_CMD_VERTEX;

    player->vert[1].x = 320.0f;
    player->vert[1].y = 0.0f;
    player->vert[1].z = 1.0f;
    player->vert[1].u = u;
    player->vert[1].v = 0.0f;
    player->vert[1].argb = color;
    player->vert[1].oargb = 0;
    player->vert[1].flags = PVR_CMD_VERTEX;

    player->vert[2].x = 0.0f;
    player->vert[2].y = 240.0f;
    player->vert[2].z = 1.0f;
    player->vert[2].u = 0.0f;
    player->vert[2].v = v;
    player->vert[2].argb = color;
    player->vert[2].oargb = 0;
    player->vert[2].flags = PVR_CMD_VERTEX;

    player->vert[3].x = 320.0f;
    player->vert[3].y = 240.0f;
    player->vert[3].z = 1.0f;
    player->vert[3].u = u;
    player->vert[3].v = v;
    player->vert[3].argb = color;
    player->vert[3].oargb = 0;
    player->vert[3].flags = PVR_CMD_VERTEX_EOL;

    return 0;
}
#if 0
static void *sound_callback(snd_stream_hnd_t hnd, int size, int *size_out) {
    plm_samples_t *sample;
    mpeg_player_t *player = snd_stream_get_userdata(hnd);
    uint32_t *dest = player->snd_buf;
    int out = player->snd_mod_size;

    if(out > 0)
        fast_memcpy(dest, player->snd_buf + player->snd_mod_start / 4, out);

    while(size > out) {
        sample = plm_decode_audio(player->decoder);
        if(!sample)
            break;
        
        player->audio_time = sample->time;
        fast_memcpy(dest + out / 4, sample->pcm, 1152 * 2);
        out += 1152 * 2;
    }

    player->snd_mod_start = size;
    player->snd_mod_size = out - size;
    *size_out = size;

    return player->snd_buf;
}
#endif

#if 0
static int setup_audio(mpeg_player_t *player) {
    player->snd_mod_size = 0;
    player->snd_mod_start = 0;
    player->audio_time = 0.0f;
    player->sample_rate = plm_get_samplerate(player->decoder);

    player->snd_hnd = snd_stream_alloc(sound_callback, SOUND_BUFFER);
    if(player->snd_hnd == SND_STREAM_INVALID)
        return -1;

    snd_stream_volume(player->snd_hnd, 0xff);
    snd_stream_set_userdata(player->snd_hnd, player);

    return 0;
}
#endif 
static __attribute__((noinline)) void fast_memcpy(void *dest, const void *src, size_t length) {
    int blocks;
    int remainder;
    char *char_dest = (char *)dest;
    const char *char_src = (const char *)src;

    _Complex float ds;
    _Complex float ds2;
    _Complex float ds3;
    _Complex float ds4;

    if(((uintptr_t)dest | (uintptr_t)src) & 7) {
        memcpy(dest, src, length);
    }
    else { /* Fast Path */
        blocks = length / 32;
        remainder = length % 32;

        if(blocks > 0) {
            __asm__ __volatile__ (
                "fschg\n\t"
                "clrs\n" 
                ".align 2\n"
                "1:\n\t"
                /* *dest++ = *src++ */
                "fmov.d @%[in]+, %[scratch]\n\t"
                "fmov.d @%[in]+, %[scratch2]\n\t"
                "fmov.d @%[in]+, %[scratch3]\n\t"
                "fmov.d @%[in]+, %[scratch4]\n\t"
                "movca.l %[r0], @%[out]\n\t"
                "add #32, %[out]\n\t"
                "dt %[blocks]\n\t"   /* while(blocks--) */
                "fmov.d %[scratch4], @-%[out]\n\t"
                "fmov.d %[scratch3], @-%[out]\n\t"
                "fmov.d %[scratch2], @-%[out]\n\t"
                "fmov.d %[scratch], @-%[out]\n\t"
                "bf.s 1b\n\t"
                "add #32, %[out]\n\t"
                "fschg\n"
                : [in] "+&r" ((uintptr_t)src), [out] "+&r" ((uintptr_t)dest), 
                [blocks] "+&r" (blocks), [scratch] "=&d" (ds), [scratch2] "=&d" (ds2), 
                [scratch3] "=&d" (ds3), [scratch4] "=&d" (ds4) /* outputs */
                : [r0] "z" (remainder) /* inputs */
                : "t", "memory" /* clobbers */
            );
        }

        while(remainder--)
            *char_dest++ = *char_src++;
    }
}
#endif