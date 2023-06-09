#ifndef VIDEO_H
#define VIDEO_H

// DCFIXME
#if RETRO_PLATFORM != RETRO_KALLISTIOS
#include <ogg/ogg.h>
#endif  // RETRO_PLATFORM != RETRO_KALLISTIOS

namespace RSDK
{

struct VideoManager {
    #if RETRO_PLATFORM != RETRO_KALLISTIOS // DCFIXME
    static FileInfo file;

    static ogg_sync_state oy;
    static ogg_page og;
    static ogg_stream_state vo;
    static ogg_stream_state to;
    static ogg_packet op;
    static th_info ti;
    static th_comment tc;
    static th_dec_ctx *td;
    static th_setup_info *ts;

    static th_pixel_fmt pixelFormat;
    static ogg_int64_t granulePos;
    static bool32 initializing;
    #endif  // RETRO_PLATFORM != RETRO_KALLISTIOS
};

bool32 LoadVideo(const char *filename, double startDelay, bool32 (*skipCallback)());
void ProcessVideo();

} // namespace RSDK

#endif // VIDEO_H
