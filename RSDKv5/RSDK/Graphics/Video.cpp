#include "RSDK/Core/RetroEngine.hpp"

#include <RSDK/Core/Stub.hpp>

using namespace RSDK;

#if RETRO_PLATFORM != RETRO_KALLISTIOS
FileInfo VideoManager::file;

ogg_sync_state VideoManager::oy;
ogg_page VideoManager::og;
ogg_packet VideoManager::op;
ogg_stream_state VideoManager::vo;
ogg_stream_state VideoManager::to;
th_info VideoManager::ti;
th_comment VideoManager::tc;
th_dec_ctx *VideoManager::td    = NULL;
th_setup_info *VideoManager::ts = NULL;

th_pixel_fmt VideoManager::pixelFormat;
ogg_int64_t VideoManager::granulePos = 0;
bool32 VideoManager::initializing    = false;
#endif  // RETRO_PLATFORM != RETRO_KALLISTIOS

#if RETRO_PLATFORM == RETRO_KALLISTIOS
// these are flags set in `PlayStream`
// they let us know which pre-muxed variation of the intro video to play
extern bool introHp;
extern bool introTee;

// provider wrappers around RSDK filesystem access for plmpeg
// we already have it customized for thread safety, might as well use it
#define PLM_FILE_TYPE                FileIO*
#define PLM_FILE_INVALID_HANDLE      NULL
#define PLM_FILE_OPEN(fn)            fOpen((fn), "rb")
#define PLM_FILE_CLOSE(fh)           fClose((fh))
#define PLM_FILE_SEEK(fh, off, st)   fSeek((fh), (off), (st))
#define PLM_FILE_READ(fh, buf, size) fRead((buf), 1, (size), (fh))
#define PLM_FILE_TELL(fh)            fTell((fh))

// there are 12 allocations that occur internally to PLMPEG on each video playback
// 11 mallocs and 1 realloc
// I verified this for every video we can possibly play as part of Sonic Mania
// provide storage for 12 pointers so they don't go out of scope and cause the RSDK pool allocator any grief
static void *mpegAllocs[12] = {};
static int mpegAllocIndex = 0;

// provide wrappers around the RSDK pool allocator for plmpeg,
// otherwise we do not have enough RAM to play a video :-)
#define PLM_MALLOC(sz) \
            ({ \
            AllocatePinnedStorage((void **)&mpegAllocs[mpegAllocIndex], sz, DATASET_STG, false); \
            mpegAllocs[mpegAllocIndex++]; \
            })

#define PLM_FREE(p) \
            ({ \
            RemoveStorageEntry((void**)&p); \
            })

#define PLM_REALLOC(p, sz) \
            ({ \
            AllocatePinnedStorage((void **)&mpegAllocs[mpegAllocIndex], sz, DATASET_STG, false); \
            memcpy(mpegAllocs[mpegAllocIndex], p, sz); \
            RemoveStorageEntry((void**)&p); \
            mpegAllocs[mpegAllocIndex++]; \
            })

#include "KallistiOS/mpeg.h"
#include "KallistiOS/mpeg.c"

static char videoFilePath[256];
static mpeg_player_t *mpegPlayer;

// Dreamcast-specific options for plmpeg playback
static mpeg_player_options_t mania_opts = {
    .player_list_type   = PVR_LIST_PT_POLY,
    .player_filter_mode = PVR_FILTER_NONE,
    .player_volume      = 255,
    .player_loop        = false,
    .extra_letterbox    = false
};
#endif

bool32 RSDK::LoadVideo(const char *filename, double startDelay, bool32 (*skipCallback)())
{
#if RETRO_PLATFORM == RETRO_KALLISTIOS
    // do not play BadEnd.ogv, MREnd.ogv as a standalone video;
    // ending videos are combined and muxed with music tracks during asset generation
    bool vidSkip = false;

    // reset plmpeg alloc pointer storage index
    mpegAllocIndex = 0;

    // do not play BadEnd.ogv, MREnd.ogv as a standalone video
    // ending videos are combined and muxed with music tracks during asset generation
    // Mania is a special case, there are two possible variations; that has a specific check within
    // GoodEnd is muxed with music but is not combined with any other videos; it is not checked for here
    if ((strncmp("BadEnd", filename, 6) == 0) || (strncmp("MREnd", filename, 5) == 0) || (strncmp("Mania", filename, 5) == 0)) {
        // specifically for Mania, check which song was set to play over the video
        // and choose one of the two pre-muxed versions for playback
        if (strncmp("Mania", filename, 5) == 0) {
            if (introHp)
                filename = "ManiaHP.mpg"; // the "first"
            else 
                filename = "ManiaTee.mpg"; // the "alternate"
        } else {
            // either BadEnd or MRend were set to play
            // return without starting it
            // the next video that gets played will be a combined and muxed verison of:
            // BadKnux, BadMighty, BadRay, BadSonic, BadSonic2 or BadTails
            introHp = false;
            introTee = false;
            vidSkip = true;
        }
    }

    if ((strstr(filename, "Bad") || strstr(filename, "Good"))) {
        mania_opts.extra_letterbox = true;
    } else {
        mania_opts.extra_letterbox = false;
    }

    if (ENGINE_VERSION == 5 && sceneInfo.state == ENGINESTATE_VIDEOPLAYBACK)
        return false;

    // if we are actually going to play a video
    if(!vidSkip) {
        // generate KOS_USER_DIR-oriented path to video file
        sprintf_s(videoFilePath, sizeof(videoFilePath), "%sData/Video/%s", KOS_USER_DIR, filename);
        // create an mpeg_player_t with the path and custom player options
        mpegPlayer = mpeg_player_create_ex(videoFilePath, &mania_opts);
        // give up if it fails
        if (!mpegPlayer) {
            return false;
        }
    }

    // set up the engine state for video playback like the original code does
    engine.skipCallback = skipCallback;
    engine.storedShaderID     = videoSettings.shaderID;
    videoSettings.screenCount = 0;
    changedVideoSettings = false;
    if (ENGINE_VERSION == 5)
        engine.storedState = sceneInfo.state;
    if (ENGINE_VERSION == 5)
        sceneInfo.state = ENGINESTATE_VIDEOPLAYBACK;

    // enable dither when playing videos
#define PM_DITHER_BIT 8
    uint32_t cfg = PVR_GET(PVR_FB_CFG_2);
    cfg |= PM_DITHER_BIT;
    PVR_SET(PVR_FB_CFG_2, cfg);

    // success
    return true;
#else  // RETRO_PLATFORM == RETRO_KALLISTIOS
    if (ENGINE_VERSION == 5 && sceneInfo.state == ENGINESTATE_VIDEOPLAYBACK)
        return false;
#if RETRO_REV0U
    if (ENGINE_VERSION == 3 && RSDK::Legacy::gameMode == RSDK::Legacy::v3::ENGINE_VIDEOWAIT)
        return false;
#endif

    char fullFilePath[0x80];
    sprintf_s(fullFilePath, sizeof(fullFilePath), "Data/Video/%s", filename);

    InitFileInfo(&VideoManager::file);
    if (LoadFile(&VideoManager::file, fullFilePath, FMODE_RB)) {
        // Init
        ogg_sync_init(&VideoManager::oy);

        th_comment_init(&VideoManager::tc);
        th_info_init(&VideoManager::ti);

        int32 theora_p = 0;
        char *buffer   = NULL;

        // Parse header stuff
        bool32 finishedHeader = false;
        while (!finishedHeader) {
            buffer    = ogg_sync_buffer(&VideoManager::oy, 0x1000);
            int32 ret = (int32)ReadBytes(&VideoManager::file, buffer, 0x1000);
            ogg_sync_wrote(&VideoManager::oy, 0x1000);

            if (ret == 0)
                break;

            while (ogg_sync_pageout(&VideoManager::oy, &VideoManager::og) > 0) {
                ogg_stream_state test;

                /* is this a mandated initial header? If not, stop parsing */
                if (!ogg_page_bos(&VideoManager::og)) {
                    /* don't leak the page; get it into the appropriate stream */
                    ogg_stream_pagein(&VideoManager::to, &VideoManager::og);
                    finishedHeader = true;
                    break;
                }

                ogg_stream_init(&test, ogg_page_serialno(&VideoManager::og));
                ogg_stream_pagein(&test, &VideoManager::og);
                ogg_stream_packetout(&test, &VideoManager::op);

                // identify codec
                if (!theora_p && th_decode_headerin(&VideoManager::ti, &VideoManager::tc, &VideoManager::ts, &VideoManager::op) >= 0) {
                    // theora
                    memcpy(&VideoManager::to, &test, sizeof(test));
                    theora_p = 1;
                }
                else {
                    // we dont care (possibly vorbis)
                    ogg_stream_clear(&test);
                }
            }
        }

        if (theora_p) {
            VideoManager::ts = NULL;
            theora_p         = 1;
            while (theora_p && theora_p < 3) {
                int32 ret;

                while (theora_p && (theora_p < 3) && (ret = ogg_stream_packetout(&VideoManager::to, &VideoManager::op))) {
                    if (ret < 0) {
#if !RETRO_USE_ORIGINAL_CODE
                        PrintLog(PRINT_NORMAL, "ERROR: failed to parse theora stream headers. corrupted stream?");
#endif

                        theora_p = 0;
                        break;
                    }

                    if (!th_decode_headerin(&VideoManager::ti, &VideoManager::tc, &VideoManager::ts, &VideoManager::op)) {
#if !RETRO_USE_ORIGINAL_CODE
                        PrintLog(PRINT_NORMAL, "ERROR: failed to parse theora stream headers. corrupted stream?");
#endif

                        theora_p = 0;
                        break;
                    }

                    theora_p++;
                }

                if (!theora_p)
                    break;

                /* The header pages/packets will arrive before anything else we
                   care about, or the stream is not obeying spec */

                if (ogg_sync_pageout(&VideoManager::oy, &VideoManager::og) > 0) {
                    ogg_stream_pagein(&VideoManager::to, &VideoManager::og);
                }
                else {
                    buffer    = ogg_sync_buffer(&VideoManager::oy, 0x1000);
                    int32 ret = (int32)ReadBytes(&VideoManager::file, buffer, 0x1000);
                    ogg_sync_wrote(&VideoManager::oy, 0x1000);
                    if (ret == 0) {
#if !RETRO_USE_ORIGINAL_CODE
                        PrintLog(PRINT_NORMAL, "ERROR: Reached end of file while searching for codec headers.");
#endif

                        theora_p = 0;
                    }
                }
            }

            if (!theora_p) {
                th_info_clear(&VideoManager::ti);
                th_comment_clear(&VideoManager::tc);
                th_setup_free(VideoManager::ts);
            }
            else {
                VideoManager::td          = th_decode_alloc(&VideoManager::ti, VideoManager::ts);
                VideoManager::pixelFormat = VideoManager::ti.pixel_fmt;

                int32 ppLevelMax = 0;
                th_decode_ctl(VideoManager::td, TH_DECCTL_GET_PPLEVEL_MAX, &ppLevelMax, sizeof(int32));
                int32 ppLevel = 0;
                th_decode_ctl(VideoManager::td, TH_DECCTL_SET_PPLEVEL, &ppLevel, sizeof(int32));

                th_setup_free(VideoManager::ts);

                engine.storedShaderID     = videoSettings.shaderID;
                videoSettings.screenCount = 0;

                if (ENGINE_VERSION == 5)
                    engine.storedState = sceneInfo.state;
#if RETRO_REV0U
                else if (ENGINE_VERSION == 3)
                    engine.storedState = RSDK::Legacy::gameMode;
#endif
                engine.displayTime         = 0.0;
                VideoManager::initializing = true;
                VideoManager::granulePos   = 0;

                engine.displayTime     = 0.0;
                engine.videoStartDelay = 0.0;
                if (AudioDevice::audioState == 1)
                    engine.videoStartDelay = startDelay;

                switch (VideoManager::pixelFormat) {
                    default: break;
                    case TH_PF_420: videoSettings.shaderID = SHADER_YUV_420; break;
                    case TH_PF_422: videoSettings.shaderID = SHADER_YUV_422; break;
                    case TH_PF_444: videoSettings.shaderID = SHADER_YUV_444; break;
                }

                engine.skipCallback = NULL;
                ProcessVideo();
                engine.skipCallback = skipCallback;

                changedVideoSettings = false;
                if (ENGINE_VERSION == 5)
                    sceneInfo.state = ENGINESTATE_VIDEOPLAYBACK;
#if RETRO_REV0U
                else if (ENGINE_VERSION == 3)
                    RSDK::Legacy::gameMode = RSDK::Legacy::v3::ENGINE_VIDEOWAIT;
#endif

                return true;
            }
        }

        CloseFile(&VideoManager::file);
    }

    return false;
#endif  // RETRO_PLATFORM != RETRO_KALLISTIOS
}

void RSDK::ProcessVideo()
{
#if RETRO_PLATFORM == RETRO_KALLISTIOS
    // always attempt to run a decode step if the player exists
    // it should exist if this gets called, but we have error handling *just in case*
    mpeg_decode_result_t res = mpegPlayer ? mpeg_decode_step(mpegPlayer) : MPEG_DECODE_ERROR;

    // the various conditions where playback should end
    if ((!mpegPlayer) || (engine.skipCallback && engine.skipCallback()) || ((res == MPEG_DECODE_EOF) || (res == MPEG_DECODE_ERROR))) {
        // safety first
        if (mpegPlayer) {
            // clean up the player, frees all resources
            mpeg_player_destroy(mpegPlayer);
            mpegPlayer = NULL;
        }

        // restore engine settings like the original code would do
        videoSettings.shaderID    = engine.storedShaderID;
        videoSettings.screenCount = 1;
        if (ENGINE_VERSION == 5)
            sceneInfo.state = engine.storedState;
#if RETRO_REV0U
        else if (ENGINE_VERSION == 3)
            RSDK::Legacy::gameMode = engine.storedState;
#endif

        // disable dither when done playing videos
        uint32_t cfg = PVR_GET(PVR_FB_CFG_2);
        cfg &= ~PM_DITHER_BIT;
        PVR_SET(PVR_FB_CFG_2, cfg);
        return;
    } else {
        // playback should not end

        // if the last step produced a frame, upload it
        if (res == MPEG_DECODE_FRAME) {
            mpeg_upload_frame(mpegPlayer);
        }
        // and always render the last produced frame
        mpeg_draw_frame(mpegPlayer);
    }
#else
    bool32 finished = false;
    double curTime  = 0;
    if (!VideoManager::initializing) {
        double streamPos = GetVideoStreamPos();

        if (streamPos <= -1.0)
            engine.displayTime += (1.0 / 60.0); // deltaTime frame-step
        else
            engine.displayTime = streamPos;

        curTime = th_granule_time(VideoManager::td, VideoManager::granulePos);

#if RETRO_USE_MOD_LOADER
        RunModCallbacks(MODCB_ONVIDEOSKIPCB, (void *)engine.skipCallback);
#endif
        if (engine.skipCallback && engine.skipCallback()) {
            finished = true;
        }
    }

    if (!finished && (VideoManager::initializing || engine.displayTime >= engine.videoStartDelay + curTime)) {
        while (ogg_stream_packetout(&VideoManager::to, &VideoManager::op) <= 0) {
            char *buffer = ogg_sync_buffer(&VideoManager::oy, 0x1000);
            // if we're playing and reached the end of file
            if (!ReadBytes(&VideoManager::file, buffer, 0x1000) && !VideoManager::initializing) {
                finished = true;
                break;
            }

            ogg_sync_wrote(&VideoManager::oy, 0x1000);

            while (ogg_sync_pageout(&VideoManager::oy, &VideoManager::og) > 0) ogg_stream_pagein(&VideoManager::to, &VideoManager::og);
        }

        if (!finished && !th_decode_packetin(VideoManager::td, &VideoManager::op, &VideoManager::granulePos)) {
            th_ycbcr_buffer yuv;
            th_decode_ycbcr_out(VideoManager::td, yuv);

            int32 dataPos = (VideoManager::ti.pic_x & 0xFFFFFFFE) + (VideoManager::ti.pic_y & 0xFFFFFFFE) * yuv[0].stride;
            switch (VideoManager::pixelFormat) {
                default: break;

                case TH_PF_444:
                    RenderDevice::SetupVideoTexture_YUV444(yuv[0].width, yuv[0].height, &yuv[0].data[dataPos], &yuv[1].data[dataPos],
                                                           &yuv[2].data[dataPos], yuv[0].stride, yuv[1].stride, yuv[2].stride);
                    break;

                case TH_PF_422:
                    RenderDevice::SetupVideoTexture_YUV422(yuv[0].width, yuv[0].height, &yuv[0].data[dataPos],
                                                           &yuv[1].data[yuv[1].stride * VideoManager::ti.pic_y + (VideoManager::ti.pic_x >> 1)],
                                                           &yuv[2].data[yuv[1].stride * VideoManager::ti.pic_y + (VideoManager::ti.pic_x >> 1)],
                                                           yuv[0].stride, yuv[1].stride, yuv[2].stride);
                    break;

                case TH_PF_420:
                    RenderDevice::SetupVideoTexture_YUV420(
                        yuv[0].width, yuv[0].height, &yuv[0].data[dataPos],
                        &yuv[1].data[yuv[1].stride * (VideoManager::ti.pic_y >> 1) + (VideoManager::ti.pic_x >> 1)],
                        &yuv[2].data[yuv[1].stride * (VideoManager::ti.pic_y >> 1) + (VideoManager::ti.pic_x >> 1)], yuv[0].stride, yuv[1].stride,
                        yuv[2].stride);
                    break;
            }
        }

        VideoManager::initializing = false;
    }

    if (finished) {
        CloseFile(&VideoManager::file);

        // Flush everything out
        while (ogg_sync_pageout(&VideoManager::oy, &VideoManager::og) > 0) ogg_stream_pagein(&VideoManager::to, &VideoManager::og);

        ogg_stream_clear(&VideoManager::to);
        th_decode_free(VideoManager::td);
        th_comment_clear(&VideoManager::tc);
        th_info_clear(&VideoManager::ti);
        ogg_sync_clear(&VideoManager::oy);

        videoSettings.shaderID    = engine.storedShaderID;
        videoSettings.screenCount = 1;
        if (ENGINE_VERSION == 5)
            sceneInfo.state = engine.storedState;
#if RETRO_REV0U
        else if (ENGINE_VERSION == 3)
            RSDK::Legacy::gameMode = engine.storedState;
#endif
    }
#endif  // RETRO_PLATFORM != RETRO_KALLISTIOS
}
