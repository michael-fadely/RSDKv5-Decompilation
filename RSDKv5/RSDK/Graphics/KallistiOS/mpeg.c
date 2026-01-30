#ifndef __MPEG_C
#define __MPEG_C
#include <kos.h>
#define PL_MPEG_IMPLEMENTATION
#include "pl_mpeg.h"
#include "mpeg.h"

struct mpeg_player_t {
    /* MPEG decoder */
    plm_t *decoder;

    /* Texture that holds decoded data */
    pvr_ptr_t texture;

    /* Width of the video in pixels */
    int width;

    /* Height of the video in pixels */
    int height;

    /* Current video playback time */
    float video_time;

    /* Polygon header for rendering */
    pvr_poly_hdr_t hdr;

    /* Vertices for rendering the video frame */
    pvr_vertex_t vert[4];
};

/* Output texture width and height initial values
   You can choose from 32, 64, 128, 256, 512, 1024 */
static int mpeg_texture_width;
static int mpeg_texture_height;

static void upload_frame(plm_frame_t *frame);
static void draw_frame(mpeg_player_t *player);
static int setup_graphics(mpeg_player_t *player);

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

    if(setup_graphics(player) < 0) {
        fprintf(stderr, "Setting up graphics failed\n");
        mpeg_player_destroy(player);
        return NULL;
    }

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

    if(setup_graphics(player) < 0) {
        fprintf(stderr, "Setting up graphics failed\n");
        mpeg_player_destroy(player);
        return NULL;
    }

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

    free(player);
    player = NULL;
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
    frame = plm_decode_video(player->decoder);
    if (!frame) {
        printf("last frame decode was NULL\n");
        cancel = 1;
    } else {
        decoded = 1;
        player->video_time = frame->time;
    }

    if (decoded) {
        upload_frame(frame);
        decoded = 0;
    }

    draw_frame(player);

    *done = cancel;
}

static void upload_frame(plm_frame_t *frame) {
    uint32_t *src;
    int x, y, w, h, i;

    if (!frame) {
        return;
    }

    src = frame->display;

    /* Calculate number of megablocks based on frame size */
    w = frame->width >> 4;
    h = frame->height >> 4;

    int const min_blocks_x = 32 * (frame->width / 320) - w;

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
            sq_flush(d);
        }
    }

    sq_unlock();
}

static void draw_frame(mpeg_player_t *player) {
    pvr_prim(&player->hdr, 32);
    pvr_prim(&player->vert, 128);
}

static int setup_graphics(mpeg_player_t *player) {
    pvr_poly_cxt_t cxt;
    float u, v;
    int color = 0xFFFFFFFF;

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
                     PVR_FILTER_NEAREST);
    pvr_poly_compile(&player->hdr, &cxt);

    player->video_time = 0.0f;

#if DO_240
    float pixelScaleX = 1.0f;
    float pixelScaleY = 1.0f;
#else
    float pixelScaleX = 2.0f;
    float pixelScaleY = 2.0f;
#endif
    float vidWidth = 320.0f * pixelScaleX;
    float vidHeight = 240.0f * pixelScaleY;

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

    player->vert[1].x = vidWidth;
    player->vert[1].y = 0.0f;
    player->vert[1].z = 1.0f;
    player->vert[1].u = u;
    player->vert[1].v = 0.0f;
    player->vert[1].argb = color;
    player->vert[1].oargb = 0;
    player->vert[1].flags = PVR_CMD_VERTEX;

    player->vert[2].x = 0.0f;
    player->vert[2].y = vidHeight;
    player->vert[2].z = 1.0f;
    player->vert[2].u = 0.0f;
    player->vert[2].v = v;
    player->vert[2].argb = color;
    player->vert[2].oargb = 0;
    player->vert[2].flags = PVR_CMD_VERTEX;

    player->vert[3].x = vidWidth;
    player->vert[3].y = vidHeight;
    player->vert[3].z = 1.0f;
    player->vert[3].u = u;
    player->vert[3].v = v;
    player->vert[3].argb = color;
    player->vert[3].oargb = 0;
    player->vert[3].flags = PVR_CMD_VERTEX_EOL;

    return 0;
}

#endif