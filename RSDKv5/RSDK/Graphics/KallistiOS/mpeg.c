#ifndef __MPEG_C
#define __MPEG_C

#include <kos.h>
#include "mpeg.h"
#define PL_MPEG_IMPLEMENTATION
#include "pl_mpeg.h"

struct mpeg_player_t {
    /* MPEG decoder */
    plm_t *decoder;

    /* Pointer to a decoded video frame */
    plm_frame_t *frame;

    /* PVR list type the video frame will be rendered to */
    pvr_list_type_t list_type;

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

    /* Polygon header for rendering */
    pvr_poly_hdr_t hdr;

    /* Polygon header for letterboxing */
    pvr_poly_hdr_t lbhdr;

    /* Vertices for rendering the video frame */
    pvr_vertex_t vert[4];

    /* Vertices for letterboxing */
    pvr_vertex_t lbvert[8];

    /* Start time for a/v sync */
    uint64_t start_time;
};

static int extra_letterbox = 0;

/* Output texture width and height initial values
   You can choose from 32, 64, 128, 256, 512, 1024 */
static int mpeg_texture_width;
static int mpeg_texture_height;

/* Size of the sound buffer for both the SH4 side and the AICA side */
#define SOUND_BUFFER (32 * 1024)

static int setup_graphics(mpeg_player_t *player, pvr_filter_mode_t filter_mode);
static int setup_audio(mpeg_player_t *player, uint8_t volume);
static void fast_memcpy(void *dest, const void *src, size_t length);

static uint32_t next_power_of_two(uint32_t n) {
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

static inline void sound_stream_reset(mpeg_player_t *player) {
    if (!player)
        return;

    if (player->start_time != 0) {
        if (player->snd_hnd != SND_STREAM_INVALID)
            snd_stream_stop(player->snd_hnd);
    }

    player->snd_mod_size = 0;
    player->snd_mod_start = 0;
}

static int mpeg_check_cancel(const mpeg_cancel_options_t *opt) {
    if(!opt) return 0;

    /*   Controller Cancel   */
    MAPLE_FOREACH_BEGIN(MAPLE_FUNC_CONTROLLER, cont_state_t, st)
        if(opt->pad_button_any && (st->buttons & opt->pad_button_any))
            return 1;

        if(opt->pad_button_combo &&
            (st->buttons & opt->pad_button_combo) == opt->pad_button_combo)
            return 1;

        /* Always cancel on reset combo */
        if(st->buttons == CONT_RESET_BUTTONS)
            return 2;
    MAPLE_FOREACH_END()

    /*   Keyboard Cancel   */
    MAPLE_FOREACH_BEGIN(MAPLE_FUNC_KEYBOARD, kbd_state_t, kbd_st)
        for(size_t i = 0; i < opt->kbd_keys_any_count; ++i) {
            if(kbd_st->key_states[opt->kbd_keys_any[i]].is_down)
                return 1;
        }

        int all_pressed = 1;
        for(size_t i = 0; i < opt->kbd_keys_combo_count; ++i) {
            if(!kbd_st->key_states[opt->kbd_keys_combo[i]].is_down) {
                all_pressed = 0;
                break;
            }
        }

        if(opt->kbd_keys_combo_count && all_pressed)
            return 1;
    MAPLE_FOREACH_END()

    return 0;
}

/** Default MPEG player options used when NULL is passed to *_ex() functions. */
static const mpeg_player_options_t default_options = {
    .player_list_type   = PVR_LIST_OP_POLY,
    .player_filter_mode = PVR_FILTER_BILINEAR,
    .player_volume      = 255,
    .player_loop        = false,
    .extra_letterbox    = false,
};

mpeg_player_t *mpeg_player_create_ex(const char *filename, const mpeg_player_options_t *options) {
    mpeg_player_t *player = NULL;
    const mpeg_player_options_t *opts = options ? options : &default_options;

    if (opts->extra_letterbox == true) {
        extra_letterbox = 1;
    } else {
        extra_letterbox = 0;
    }

    if(!filename) {
        fprintf(stderr, "filename is NULL\n");
        return NULL;
    }

    player = (mpeg_player_t *)MPEG_MALLOC(sizeof(mpeg_player_t));
    if(!player) {
        fprintf(stderr, "Out of memory for player\n");
        return NULL;
    }

    MPEG_MEMZERO(player, sizeof(mpeg_player_t));

    player->decoder = plm_create_with_filename(filename);
    if(!player->decoder) {
        fprintf(stderr, "Out of memory for player->decoder\n");
        mpeg_player_destroy(player);
        return NULL;
    }
    plm_set_loop(player->decoder, opts->player_loop);

    player->snd_buf = (uint32_t *)MPEG_MEMALIGN(32, SOUND_BUFFER);
    if(!player->snd_buf) {
        fprintf(stderr, "Out of memory for player->snd_buf\n");
        mpeg_player_destroy(player);
        return NULL;
    }

    player->list_type = opts->player_list_type;
    if(setup_graphics(player, opts->player_filter_mode) < 0) {
        fprintf(stderr, "Setting up graphics failed\n");
        mpeg_player_destroy(player);
        return NULL;
    }

    if(setup_audio(player, opts->player_volume) < 0) {
        fprintf(stderr, "Setting up audio failed\n");
        mpeg_player_destroy(player);
        return NULL;
    }

    return player;
}

mpeg_player_t *mpeg_player_create_memory_ex(unsigned char *memory, const size_t length, const mpeg_player_options_t *options) {
    mpeg_player_t *player = NULL;
    const mpeg_player_options_t *opts = options ? options : &default_options;

    if(!memory) {
        fprintf(stderr, "memory is NULL\n");
        return NULL;
    }

    player = (mpeg_player_t *)MPEG_MALLOC(sizeof(mpeg_player_t));
    if(!player) {
        fprintf(stderr, "Out of memory for player\n");
        return NULL;
    }

    MPEG_MEMZERO(player, sizeof(mpeg_player_t));

    player->decoder = plm_create_with_memory(memory, length, 1);
    if(!player->decoder) {
        fprintf(stderr, "Out of memory for player->decoder\n");
        mpeg_player_destroy(player);
        return NULL;
    }
    plm_set_loop(player->decoder, opts->player_loop);

    player->snd_buf = (uint32_t *)MPEG_MEMALIGN(32, SOUND_BUFFER);
    if(!player->snd_buf) {
        fprintf(stderr, "Out of memory for player->snd_buf\n");
        mpeg_player_destroy(player);
        return NULL;
    }

    player->list_type = opts->player_list_type;
    if(setup_graphics(player, opts->player_filter_mode) < 0) {
        fprintf(stderr, "Setting up graphics failed\n");
        mpeg_player_destroy(player);
        return NULL;
    }

    if(setup_audio(player, opts->player_volume) < 0) {
        fprintf(stderr, "Setting up audio failed\n");
        mpeg_player_destroy(player);
        return NULL;
    }

    return player;
}

mpeg_player_t *mpeg_player_create(const char *filename) {
    return mpeg_player_create_ex(filename, NULL);
}

mpeg_player_t *mpeg_player_create_memory(uint8_t *memory, const size_t length) {
    return mpeg_player_create_memory_ex(memory, length, NULL);
}

int mpeg_player_get_loop(mpeg_player_t *player) {
    return plm_get_loop(player->decoder);
}

void mpeg_player_set_loop(mpeg_player_t *player, int loop) {
    plm_set_loop(player->decoder, loop);
}

void mpeg_player_set_volume(mpeg_player_t *player, uint8_t volume) {
    snd_stream_volume(player->snd_hnd, volume);
}

void mpeg_player_destroy(mpeg_player_t *player) {
    if(!player)
        return;

    if(player->decoder) {
        plm_destroy(player->decoder);
        player->decoder = NULL;
    }

    if(player->texture) {
        MPEG_PVR_FREE(player->texture);
        player->texture = NULL;
    }

    if(player->snd_buf) {
        MPEG_FREE(player->snd_buf);
        player->snd_buf = NULL;
    }

    if(player->snd_hnd != SND_STREAM_INVALID)
        snd_stream_destroy(player->snd_hnd);

    MPEG_FREE(player);
    player = NULL;
}

mpeg_play_result_t mpeg_play_ex(mpeg_player_t *player, const mpeg_cancel_options_t *cancel_options) {
    mpeg_play_result_t result = MPEG_PLAY_NORMAL;

    if(!player || !player->decoder)
        return MPEG_PLAY_ERROR;

    /* Init sound stream. */
    sound_stream_reset(player);
    snd_stream_start(player->snd_hnd, player->sample_rate, 0);

    player->frame = plm_decode_video(player->decoder);
    if(!player->frame) {
        result = MPEG_PLAY_ERROR;
        /* Reset some stuff */
        sound_stream_reset(player);

        return result;
    }

    uint64_t start = timer_ns_gettime64();

    while(true) {
        /* Get elapsed playback time */
        double playback_time = (timer_ns_gettime64() - start) * 1e-9f;

        /* Check cancel matching */
        int cancel = mpeg_check_cancel(cancel_options);
        if(cancel == 1 || cancel == 2) {
            result = (cancel == 1) ? MPEG_PLAY_CANCEL_INPUT : MPEG_PLAY_CANCEL_RESET;
            goto finish;
        }

        /* Poll audio regardless */
        snd_stream_poll(player->snd_hnd);

        if(playback_time >= player->frame->time) {
            /* Render the current frame */
            pvr_scene_begin();
            pvr_list_begin(player->list_type);

            mpeg_upload_frame(player);
            mpeg_draw_frame(player);

            pvr_list_finish();
            pvr_scene_finish();

            /* Decode the NEXT frame to have it ready */
            player->frame = plm_decode_video(player->decoder);
            if(!player->frame) {
                /* Are we looping? */
                if(!plm_get_loop(player->decoder)) {
                    result = MPEG_PLAY_NORMAL;
                    goto finish;
                }

                /* We are looping. Reset and restart */
                sound_stream_reset(player);
                snd_stream_start(player->snd_hnd, player->sample_rate, 0);

                player->frame = plm_decode_video(player->decoder);
                if(!player->frame) {
                    result = MPEG_PLAY_ERROR;
                    goto finish;
                }

                start = timer_ns_gettime64();
            }
        }
    }

finish:
    /* Reset some stuff */
    sound_stream_reset(player);

    return result;
}

mpeg_play_result_t mpeg_play(mpeg_player_t *player, uint32_t cancel_buttons) {
    mpeg_cancel_options_t opts = {
        .pad_button_any = cancel_buttons
    };

    return mpeg_play_ex(player, &opts);
}

mpeg_decode_result_t mpeg_decode_step(mpeg_player_t *player) {
    if(!player || !player->decoder)
        return MPEG_DECODE_ERROR;

    if(player->start_time == 0) {
        /* Init sound stream. */
        sound_stream_reset(player);
        snd_stream_start(player->snd_hnd, player->sample_rate, 0);

        /* Prime the first frame */
        player->frame = plm_decode_video(player->decoder);
        if(!player->frame)
            return MPEG_DECODE_EOF;

        player->start_time = timer_ns_gettime64();

        /* Need to poll audio at the start */
        snd_stream_poll(player->snd_hnd);
        return MPEG_DECODE_FRAME;
    }

    /* Get elapsed playback time */
    double playback_time = (timer_ns_gettime64() - player->start_time) * 1e-9f;

    /* Always poll audio regardless */
    snd_stream_poll(player->snd_hnd);

    /* Check if it's time to decode the next frame */
    if(playback_time >= player->frame->time) {
        player->frame = plm_decode_video(player->decoder);
        if(player->frame) {
            return MPEG_DECODE_FRAME;
        }
        /* Are we looping? */
        if(!plm_get_loop(player->decoder)) {
            sound_stream_reset(player);
            return MPEG_DECODE_EOF;
        }

        /* We are Looping. Reset and restart */
        sound_stream_reset(player);
        snd_stream_start(player->snd_hnd, player->sample_rate, 0);

        player->frame = plm_decode_video(player->decoder);
        if(!player->frame) {
            sound_stream_reset(player);
            return MPEG_DECODE_EOF;
        }

        player->start_time = timer_ns_gettime64();

        return MPEG_DECODE_FRAME;
    }

    return MPEG_DECODE_IDLE;
}

void mpeg_upload_frame(mpeg_player_t *player) {
    uint32_t *src;
    int x, y, w, h, i;

    if(!player || !player->frame)
        return;

    src = player->frame->display;

    /* Calculate number of megablocks based on frame size */
    w = player->frame->width >> 4;
    h = player->frame->height >> 4;

    int const min_blocks_x = 32 * (player->frame->width / 320) - w;
    //int const min_blocks_y = 16 * (player->frame->height / 240) - h;

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

void mpeg_draw_frame(mpeg_player_t *player) {
    if(!player || !player->frame)
        return;

    pvr_prim(&player->hdr, sizeof(pvr_poly_hdr_t));

    pvr_prim(player->vert, sizeof(pvr_vertex_t) * 4);

    if (extra_letterbox) {
        pvr_prim(&player->lbhdr, sizeof(pvr_poly_hdr_t));

        pvr_prim(player->lbvert, sizeof(pvr_vertex_t) * 8);
    }
}

static int setup_graphics(mpeg_player_t *player, pvr_filter_mode_t filter_mode) {
    const uint32_t color = 0xffffffff;
    const float leftX = 0.0f;
    const float topY = 40.0f;
    const float lbTopBottomY = 58.0f;
    const float rightX = 320.0f;
    const float bottomY = 200.0f;
    const float lbBottomTopY = 182.0f;

    pvr_poly_cxt_t cxt;
    float u, v;

    player->width = plm_get_width(player->decoder);
    player->height = plm_get_height(player->decoder);

    mpeg_texture_width = next_power_of_two(player->width);
    mpeg_texture_height = next_power_of_two(player->height);

    player->texture = MPEG_PVR_MALLOC(mpeg_texture_width * mpeg_texture_height * 2);
    if(!player->texture) {
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

    pvr_poly_cxt_col(&cxt, player->list_type);
    pvr_poly_compile(&player->lbhdr, &cxt);

    pvr_poly_cxt_txr(&cxt, player->list_type,
                     PVR_TXRFMT_YUV422 | PVR_TXRFMT_NONTWIDDLED,
                     mpeg_texture_width, mpeg_texture_height,
                     player->texture,
                     filter_mode);
    pvr_poly_compile(&player->hdr, &cxt);

    u = (float)player->width / mpeg_texture_width;
    v = (float)player->height / mpeg_texture_height;

    player->lbvert[0].flags = PVR_CMD_VERTEX;
    player->lbvert[0].x = leftX;
    player->lbvert[0].y = topY;
    player->lbvert[0].z = 2.0f;
    player->lbvert[0].argb = 0xff000000;

    player->lbvert[1].flags = PVR_CMD_VERTEX;
    player->lbvert[1].x = rightX;
    player->lbvert[1].y = topY;
    player->lbvert[1].z = 2.0f;
    player->lbvert[1].argb = 0xff000000;

    player->lbvert[2].flags = PVR_CMD_VERTEX;
    player->lbvert[2].x = leftX;
    player->lbvert[2].y = lbTopBottomY;
    player->lbvert[2].z = 2.0f;
    player->lbvert[2].argb = 0xff000000;

    player->lbvert[3].flags = PVR_CMD_VERTEX_EOL;
    player->lbvert[3].x = rightX;
    player->lbvert[3].y = lbTopBottomY;
    player->lbvert[3].z = 2.0f;
    player->lbvert[3].argb = 0xff000000;

    player->lbvert[4].flags = PVR_CMD_VERTEX;
    player->lbvert[4].x = leftX;
    player->lbvert[4].y = lbBottomTopY;
    player->lbvert[4].z = 2.0f;
    player->lbvert[4].argb = 0xff000000;

    player->lbvert[5].flags = PVR_CMD_VERTEX;
    player->lbvert[5].x = rightX;
    player->lbvert[5].y = lbBottomTopY;
    player->lbvert[5].z = 2.0f;
    player->lbvert[5].argb = 0xff000000;

    player->lbvert[6].flags = PVR_CMD_VERTEX;
    player->lbvert[6].x = leftX;
    player->lbvert[6].y = bottomY;
    player->lbvert[6].z = 2.0f;
    player->lbvert[6].argb = 0xff000000;

    player->lbvert[7].flags = PVR_CMD_VERTEX_EOL;
    player->lbvert[7].x = rightX;
    player->lbvert[7].y = bottomY;
    player->lbvert[7].z = 2.0f;
    player->lbvert[7].argb = 0xff000000;

    player->vert[0].x = leftX;
    player->vert[0].y = topY;
    player->vert[0].z = 1.0f;
    player->vert[0].u = 0.0f;
    player->vert[0].v = 0.0f;
    player->vert[0].argb = color;
    player->vert[0].oargb = 0;
    player->vert[0].flags = PVR_CMD_VERTEX;

    player->vert[1].x = rightX;
    player->vert[1].y = topY;
    player->vert[1].z = 1.0f;
    player->vert[1].u = u;
    player->vert[1].v = 0.0f;
    player->vert[1].argb = color;
    player->vert[1].oargb = 0;
    player->vert[1].flags = PVR_CMD_VERTEX;

    player->vert[2].x = leftX;
    player->vert[2].y = bottomY;
    player->vert[2].z = 1.0f;
    player->vert[2].u = 0.0f;
    player->vert[2].v = v;
    player->vert[2].argb = color;
    player->vert[2].oargb = 0;
    player->vert[2].flags = PVR_CMD_VERTEX;

    player->vert[3].x = rightX;
    player->vert[3].y = bottomY;
    player->vert[3].z = 1.0f;
    player->vert[3].u = u;
    player->vert[3].v = v;
    player->vert[3].argb = color;
    player->vert[3].oargb = 0;
    player->vert[3].flags = PVR_CMD_VERTEX_EOL;

    return 0;
}

static void *sound_callback(snd_stream_hnd_t hnd, int size, int *size_out) {
    plm_samples_t *sample;
    mpeg_player_t *player = (mpeg_player_t *)snd_stream_get_userdata(hnd);
    uint32_t *dest = player->snd_buf;
    int out = player->snd_mod_size;

    if(out > 0)
        fast_memcpy(dest, player->snd_buf + player->snd_mod_start / 4, out);

    while(size > out) {
        sample = plm_decode_audio(player->decoder);
        if(!sample)
            break;
        fast_memcpy(dest + out / 4, sample->pcm, 1152 * 2);
        out += 1152 * 2;
    }

    player->snd_mod_start = size;
    player->snd_mod_size = out - size;
    *size_out = size;

    return player->snd_buf;
}

static int setup_audio(mpeg_player_t *player, uint8_t volume) {
    player->snd_mod_size = 0;
    player->snd_mod_start = 0;
    player->sample_rate = plm_get_samplerate(player->decoder);

    player->snd_hnd = snd_stream_alloc(sound_callback, SOUND_BUFFER);
    if(player->snd_hnd == SND_STREAM_INVALID)
        return -1;

    snd_stream_volume(player->snd_hnd, volume);
    snd_stream_set_userdata(player->snd_hnd, player);

    return 0;
}

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
