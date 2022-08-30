#ifndef VIDEO_H
#define VIDEO_H

// DCFIXME
#if defined(_arch_dreamcast)
#include <ogg/ogg.h>
#endif  // defined(_arch_dreamcast)

namespace RSDK
{

struct VideoManager {
    #if !defined(_arch_dreamcast) // DCFIXME
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
    #endif  // !defined(_arch_dreamcast)
};

bool32 LoadVideo(const char *filename, double startDelay, bool32 (*skipCallback)());
void ProcessVideo();

} // namespace RSDK

#endif // VIDEO_H
