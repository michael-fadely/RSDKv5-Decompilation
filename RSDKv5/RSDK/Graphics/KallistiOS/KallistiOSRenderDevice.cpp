#include <kos.h>
#include <dc/pvr.h>

#include <RSDK/Core/Stub.hpp>
#include <RSDK/Core/Math.hpp>

#define KOS_HARDWARE_RENDERER

#if defined(KOS_HARDWARE_RENDERER)
extern "C" {
#define TR_VERTBUF_SIZE (384*1024)
    uint8_t __attribute__((aligned(32))) tr_buf[TR_VERTBUF_SIZE];
    extern uint8_t bg_r, bg_g, bg_b;
};
#define DO_240 1
#define DO_24BPP 0
#endif

struct KOSTexture
{
#if !defined(KOS_HARDWARE_RENDERER)
    pvr_poly_cxt_t context;
    pvr_poly_hdr_t header;
    pvr_ptr_t pvrPtr;
    void* sysPtr;
#endif
    uint32 width;
    uint32 height;
};

static KOSTexture screenTextures[SCREEN_COUNT] {};

// ========== FAST SH4 UTILITY ROUTINES ==========

//! Calculates 1.0f/sqrtf( \p x ), using a fast approximation.
__always_inline float shz_inverse_sqrtf(float x) {
    asm("fsrra %0" : "+f" (x));
    return x;
}

//! Takes the inverse of \p p using a very fast approximation, returning a positive result.
__always_inline float shz_inverse_posf(float x) {
    return shz_inverse_sqrtf(x * x);
}

//! Divides \p num by \p denom using a very fast approximation, returning a positive result.
__always_inline float shz_div_posf(float num, float denom) {
    return num * shz_inverse_posf(denom);
}

// ===============================================

// Global pixel scale factors based on pixel-to-screen size ratio
static float pixelScaleX = 1.0f;
static float pixelScaleY = 1.0f;

#if defined(KOS_HARDWARE_RENDERER)
namespace {
extern int doing_tint;

enum PrimitiveTypes {
    PrimitiveTypes_None,
    PrimitiveTypes_TexturedQuadDR,
    PrimitiveTypes_TexturedPolyDR,
    PrimitiveTypes_ColoredPolyDR,
    PrimitiveTypes_FaceDR,
    PrimitiveTypes_LineDR,

    PrimitiveTypes_TexturedQuadDMA,
    PrimitiveTypes_TexturedPolyDMA,
    PrimitiveTypes_ColoredPolyDMA,
    PrimitiveTypes_FaceDMA,
    PrimitiveTypes_LineDMA,

    PrimitiveTypes_Roto
};

float drawDepth = 1.0f;
pvr_ptr_t lastTexture = nullptr;
pvr_dr_state_t drState = 0;
int lastSrcBlend = -1;
int lastDstBlend = -1;
int lastLineSrcBlend = -1;
int lastLineDstBlend = -1;
int lastFaceSrcBlend = -1;
int lastFaceDstBlend = -1;
PrimitiveTypes lastPrimitiveType = PrimitiveTypes_None;
bool lastPrimitiveWasConsumed = true;

#if 0
void ResetLastState() {
    lastTexture = nullptr;
    lastSrcBlend = -1;
    lastDstBlend = -1;
    lastLineSrcBlend = -1;
    lastLineDstBlend = -1;
    lastFaceSrcBlend = -1;
    lastFaceDstBlend = -1;
    lastPrimitiveType = PrimitiveTypes_None;
    // DCWIP: does resetting lastPrimitiveWasConsumed make sense???
    lastPrimitiveWasConsumed = true;
}
#endif
}
#endif

#if !defined(KOS_HARDWARE_RENDERER)
void draw_one_textured_poly(const Vector2& screenSize, const KOSTexture& kost) {
    /* Opaque Textured vertex */
    pvr_vertex_t vert;

    vert.flags = PVR_CMD_VERTEX;
    vert.argb = 0xffffffff;
    vert.oargb = 0;

    // top left
    vert.x = 0.0f;
    vert.y = 0.0f;
    vert.z = 1.0f;
    vert.u = 0.0f;
    vert.v = 0.0f;
    pvr_prim(&vert, sizeof(vert));

    // top right
    vert.x = 640.0f; // DCFIXME: hard-coded render dimensions used
    vert.y = 0.0f;
    vert.z = 1.0f;
    vert.u = static_cast<float>(screenSize.x) / static_cast<float>(kost.width);
    vert.v = 0.0f;
    pvr_prim(&vert, sizeof(vert));

    // bottom left
    vert.x = 0.0f;
    vert.y = 480.0f; // DCFIXME: hard-coded render dimensions used
    vert.z = 1.0f;
    vert.u = 0.0f;
    vert.v = static_cast<float>(screenSize.y) / static_cast<float>(kost.height);
    pvr_prim(&vert, sizeof(vert));

    // bottom right
    vert.flags = PVR_CMD_VERTEX_EOL;
    vert.x = 640.0f; // DCFIXME: hard-coded render dimensions used
    vert.y = 480.0f; // DCFIXME: hard-coded render dimensions used
    vert.z = 1.0f;
    vert.u = static_cast<float>(screenSize.x) / static_cast<float>(kost.width);
    vert.v = static_cast<float>(screenSize.y) / static_cast<float>(kost.height);
    pvr_prim(&vert, sizeof(vert));
}
#endif

// static
bool RenderDevice::Init()
{
#if defined(KOS_HARDWARE_RENDERER)
    pvr_init_params_t pvrParams = {
        // bin sizes
        {
            PVR_BINSIZE_0, // opaque polygons
            PVR_BINSIZE_0, // opaque modifiers (disabled)
            PVR_BINSIZE_32, // translucent polygons
            PVR_BINSIZE_0, // translucent modifiers (disabled)
            PVR_BINSIZE_32  // punch-through polygons
        },

        // vertex buffer size
        // 512 KB is the default used by pvr_init_defaults(). might need adjusting.
        512 * 1024,

        // dma enabled? (yes)
        1,

        // fsaa enabled? (no)
        0,

        // autosort disabled?
        0,

        // Overflow buffer count
        2,

        // Disable vertex double-buffering (no)
        0
    };
#else
#error pvr_init_params_t needs re-configuring for the software renderer!
#endif

#if defined(KOS_HARDWARE_RENDERER)
#if DO_240
    if (vid_check_cable() != CT_VGA) {
#if DO_24BPP
        vid_set_mode(DM_320x240_NTSC, PM_RGB888P);
#else
        vid_set_mode(DM_320x240_NTSC, PM_RGB565);
#endif
    } else {
#if DO_24BPP
        vid_set_mode(DM_320x240_VGA, PM_RGB888P);
#else
        vid_set_mode(DM_320x240_VGA, PM_RGB565);
#endif
    }
#endif
#endif

    if (pvr_init(&pvrParams) < 0) {
        while (true) {
            printf("pvr_init failed!!!\n");
        }
    }
    else {
        printf("pvr_init success. pvr_mem_available: %lu\n", pvr_mem_available());
    }

#if defined(KOS_HARDWARE_RENDERER)
    pvr_set_vertbuf(PVR_LIST_TR_POLY, tr_buf, TR_VERTBUF_SIZE);
#endif

    pvr_set_bg_color(0.0f, 1.0f, 1.0f);

    videoSettings.windowed = false;
    videoSettings.windowState = WINDOWSTATE_ACTIVE;
    videoSettings.shaderSupport = false;

    displayCount = 1;
#if defined(KOS_HARDWARE_RENDERER)
#if DO_240
    displayWidth[0] = 320;
    displayHeight[0] = 240;
#else
    displayWidth[0] = 640;
    displayHeight[0] = 480;
#endif
#else
    displayWidth[0] = 640;
    displayHeight[0] = 480;
#endif
    viewSize.x = static_cast<float>(displayWidth[0]);
    viewSize.y = static_cast<float>(displayHeight[0]);

    uint32 width;
    uint32 height;

    // DCFIXME: just align pixWidth and pixHeight to the nearest power of 2
    if (videoSettings.pixHeight <= 256) {
        width = 512;
        height = 256;
    }
    else {
        width = 1024;
        height = 512;
    }

    textureSize.x = (float)width;
    textureSize.y = (float)height;

#if !defined(KOS_HARDWARE_RENDERER)
    const uint32 screenTextureSize = 2 * width * height;

    // DCFIXME: support multiple screen textures? (presumably multiplayer only)
    printf("allocating %lux%lu frame buffer texture (%lu bytes)\n",
           width, height, screenTextureSize);

    screenTextures[0].pvrPtr = pvr_mem_malloc(screenTextureSize);

    if (!screenTextures[0].pvrPtr) {
        while (true) {
            printf("pvr_mem_malloc(%lu) failed!!!\n", screenTextureSize);
        }
    }

    screenTextures[0].sysPtr = memalign(32, screenTextureSize);

    if (!screenTextures[0].sysPtr) {
        while (true) {
            printf("memalign(32, %lu) failed!!!\n", screenTextureSize);
        }
    }

    pvr_poly_cxt_txr(&screenTextures[0].context,
                     PVR_LIST_OP_POLY,
                     PVR_TXRFMT_RGB565 | PVR_TXRFMT_NONTWIDDLED,
                     (int)width,
                     (int)height,
                     screenTextures[0].pvrPtr,
                     PVR_FILTER_NEAREST);

    pvr_poly_compile(&screenTextures[0].header, &screenTextures[0].context);
#endif

    printf("pvr setup complete. pvr_mem_available: %lu\n", pvr_mem_available());

    screenTextures[0].width = width;
    screenTextures[0].height = height;

    for (int32 s = 0; s < SCREEN_COUNT; ++s) {
        screens[s].size.y = videoSettings.pixHeight;

        const float viewAspect  = viewSize.x / viewSize.y;
        int32 screenWidth = (int32)((viewAspect * videoSettings.pixHeight) + 3) & 0xFFFFFFFC;
        if (screenWidth < videoSettings.pixWidth)
            screenWidth = videoSettings.pixWidth;

#if !RETRO_USE_ORIGINAL_CODE
        if (customSettings.maxPixWidth && screenWidth > customSettings.maxPixWidth)
            screenWidth = customSettings.maxPixWidth;
#else
        if (screenWidth > DEFAULT_PIXWIDTH)
            screenWidth = DEFAULT_PIXWIDTH;
#endif

#if !defined(KOS_HARDWARE_RENDERER)
        memset(&screens[s].frameBuffer, 0, sizeof(screens[s].frameBuffer));
#endif
        SetScreenSize(s, screenWidth, screens[s].size.y);
    }

    pixelSize.x = static_cast<float>(screens[0].size.x);
    pixelSize.y = static_cast<float>(screens[0].size.y);

    engine.inFocus = 1;

    videoSettings.windowState = WINDOWSTATE_ACTIVE;
    videoSettings.dimMax      = 1.0;
    videoSettings.dimPercent  = 1.0;

    const int32 size = videoSettings.pixWidth >= SCREEN_YSIZE ? videoSettings.pixWidth : SCREEN_YSIZE;
    scanlines  = (ScanlineInfo *)malloc(size * sizeof(ScanlineInfo));
    memset(scanlines, 0, size * sizeof(ScanlineInfo));

    InitInputDevices();
    return true;
}

// static
void RenderDevice::CopyFrameBuffer()
{
#if !defined(KOS_HARDWARE_RENDERER)
    for (int32 s = 0; s < /*videoSettings.screenCount*/ 1; ++s) {
        const KOSTexture& screenTexture = screenTextures[s];

        uint16* pixels = (uint16*)screenTexture.sysPtr;
        uint16* frameBuffer = screens[s].frameBuffer;
        uint32 pitch = screenTexture.width;

        for (int32 y = 0; y < SCREEN_YSIZE; ++y) {
            memcpy(pixels, frameBuffer, screens[s].size.x * sizeof(uint16));
            frameBuffer += screens[s].pitch;
            pixels += pitch;
        }

        uint32 textureSize = screenTexture.width * screenTexture.height * 2;
        pvr_txr_load(screenTexture.sysPtr, screenTexture.pvrPtr, textureSize);

        // DMA version:
        //dcache_flush_range(screenTexture.sysPtr, textureSize);
        //pvr_txr_load_dma(screenTexture.sysPtr, screenTexture.pvrPtr, textureSize, 1, nullptr, 0);
    }
#endif
}
// static
void RenderDevice::FlipScreen()
{
#if !defined(KOS_HARDWARE_RENDERER)
    pvr_scene_begin();

    // DCFIXME: support multiple screen textures? (presumably multiplayer only)
    switch (videoSettings.screenCount) {
        case 1: {
            pvr_list_begin(PVR_LIST_OP_POLY);
            {
                pvr_prim(&screenTextures[0].header, sizeof(screenTextures[0].header));
                draw_one_textured_poly(screens[0].size, screenTextures[0]);
            }
            pvr_list_finish();
            break;
        }

        default:
            DC_STUB();
            break;
    }

    pvr_scene_finish();
#endif
}
// static
void RenderDevice::Release(bool32 isRefresh)
{
    DC_STUB();
}
// static
void RenderDevice::RefreshWindow()
{
    DC_STUB();
    printf("window size: %dx%d\n", videoSettings.windowWidth, videoSettings.windowHeight);
}
// static
void RenderDevice::GetWindowSize(int32 *width, int32 *height)
{
    printf("window size: %dx%d\n", videoSettings.windowWidth, videoSettings.windowHeight);
    if (width) {
        *width = videoSettings.windowWidth;
    }

    if (height) {
        *height = videoSettings.windowHeight;
    }
}

// static
void RenderDevice::SetupImageTexture(int32 width, int32 height, uint8 *imagePixels)
{
    DC_STUB();
}
// static
void RenderDevice::SetupVideoTexture_YUV420(int32 width, int32 height, uint8 *imagePixels)
{
    DC_STUB();
}
// static
void RenderDevice::SetupVideoTexture_YUV422(int32 width, int32 height, uint8 *imagePixels)
{
    DC_STUB();
}
// static
void RenderDevice::SetupVideoTexture_YUV424(int32 width, int32 height, uint8 *imagePixels)
{
    DC_STUB();
}

// static
bool RenderDevice::ProcessEvents()
{
    // commented because this becomes very noisy...
    //DC_STUB();
    return true;
}

// static
void RenderDevice::InitFPSCap()
{
    DC_STUB();
}
// static
bool RenderDevice::CheckFPSCap() {
    // Render idle time as a green bar
    //vid_border_color(0, 255, 0);
    // this is handled internally by KOS now
    //pvr_wait_ready();
    // Render scene submission time as a red bar
    //vid_border_color(255, 0, 0);
    return true;
}
// static
void RenderDevice::UpdateFPSCap()
{
    // commented because this becomes very noisy...
    //DC_STUB();
}

// Public since it's needed for the ModAPI
// static
void RenderDevice::LoadShader(const char *fileName, bool32 linear)
{
    DC_STUB();
}

// static
void RenderDevice::SetWindowTitle()
{
    DC_STUB();
}
// static
bool RenderDevice::GetCursorPos(void*)
{
    DC_STUB();
    return false;
}
// static
void RenderDevice::ShowCursor(bool)
{
    DC_STUB();
}

// KallistiOS only!!!

// static
void RenderDevice::BeginScene() {
#if defined(KOS_HARDWARE_RENDERER)
    SetDepth(0);
    lastPrimitiveType = PrimitiveTypes_None;
//    pvr_wait_ready();

    // Update our cached values for pixel global pixel scaling.
    pixelScaleX = viewSize.x / pixelSize.x;
    pixelScaleY = viewSize.y / pixelSize.y;

    pvr_scene_begin();
    pvr_set_bg_color(0.0f,0.0f,0.0f);//(float)bg_r / 255.0f, (float)bg_g / 255.0f, (float)bg_b / 255.0f);
    // direct render anything that doesn't blend with punch-through
    if (pvr_list_begin(PVR_LIST_PT_POLY) == -1) {
        printf("[pvr] [NG] pvr_list_begin(PVR_LIST_PT_POLY) returned -1 (%s:%zu -> %s)\n", __FILE__, static_cast<size_t>(__LINE__), __PRETTY_FUNCTION__);
    }

    pvr_dr_init(&drState);
#endif
}

// static
void RenderDevice::EndScene() {
#if defined(KOS_HARDWARE_RENDERER)
    pvr_dr_finish();

    // DCWIP: rendering everything as transparent
    if (pvr_list_finish() == -1) {
        printf("[pvr] [NG] pvr_list_finish() returned -1 (%s:%zu -> %s)\n", __FILE__, static_cast<size_t>(__LINE__), __PRETTY_FUNCTION__);
    }

    if (pvr_scene_finish() == -1) {
        printf("[pvr] [NG] pvr_scene_finish() returned -1 (%s:%zu -> %s)\n", __FILE__, static_cast<size_t>(__LINE__), __PRETTY_FUNCTION__);
    }

    // Render CPU time as a blue bar.
    //vid_border_color(0, 0, 255);
#endif
}

// static
float RenderDevice::GetDepth() {
#if defined(KOS_HARDWARE_RENDERER)
    drawDepth += 0.001f;
    return drawDepth;
#else
    return 1.0f;
#endif
}

// static
void RenderDevice::SetDepth(uint32 depth) {
    // DCWIP: maybe drop the DRAWGROUP_COUNT limit and use EntityBase.zdepth (ctrl+shift+f that mfer)
#if defined(KOS_HARDWARE_RENDERER)
    depth = MIN(depth + 1, DRAWGROUP_COUNT);
    drawDepth = static_cast<float>(depth);
#endif
}

inline uint32 CalculatePvrPaletteBankOffset(uint32 pvrPaletteBankIndex) {
    return 256 * pvrPaletteBankIndex;
}

// static
uint32 RenderDevice::GetGamePaletteBankIndex(int32 y) {
    const uint8* lineBuffer = &gfxLineBuffer[CLAMP(y, 0, screens[0].size.y - 1)];
    return static_cast<uint32>(*lineBuffer);
}

// static
uint32 RenderDevice::GameToPvrPaletteBankIndex(uint32 gamePaletteBankIndex) {
    uint32 pvrPaletteBankIndex = gamePaletteBankIndex;

    if (gamePaletteBankIndex >= 4) {
        const uint32 corrected = gamePaletteBankIndex % 4;

        printf("[pvr] WARNING: palette bank index exceeds 4: %lu; applying mod, using this instead: %lu\n",
               gamePaletteBankIndex,
               corrected);

        pvrPaletteBankIndex = corrected;
    }

    return pvrPaletteBankIndex;
}

// static
void RenderDevice::PopulatePvrPalette(uint32 gamePaletteBankIndex, uint32 pvrPaletteBankIndex) {
    const uint32 pvrPaletteBankOffset = CalculatePvrPaletteBankOffset(pvrPaletteBankIndex);

    auto rgb565toargb1555 = [](uint16 color16) -> uint16 {
        return (color16 & 0x1F) | ((color16 >> 1) & 0x7FE0);
    };

    const uint16 *activePalette = fullPalette[gamePaletteBankIndex];

    pvr_set_pal_format(PVR_PAL_ARGB1555);

    // first color (0) is always completely translucent
    pvr_set_pal_entry(pvrPaletteBankOffset, rgb565toargb1555(activePalette[0]));

    // now set every other color with the opaque bit set
    for (int i = 1; i < PALETTE_BANK_SIZE; ++i) {
        const uint16 color16 = rgb565toargb1555(activePalette[i]) | 0x8000;
        pvr_set_pal_entry(pvrPaletteBankOffset + i, (uint32) color16);
    }
}

// static
bool RenderDevice::InkToBlendModes(int inkEffect, int* srcBlend, int* dstBlend)
{
    switch (inkEffect) {
        case INK_NONE:
            if (srcBlend) {
                *srcBlend = PVR_BLEND_SRCALPHA;
            }

            if (dstBlend) {
                *dstBlend = PVR_BLEND_INVSRCALPHA;
            }

            return true;

        case INK_BLEND:
            if (srcBlend) {
                *srcBlend = PVR_BLEND_SRCALPHA;
            }

            if (dstBlend) {
                *dstBlend = PVR_BLEND_INVSRCALPHA;
            }

            return true;

        case INK_ALPHA:
            if (srcBlend) {
                *srcBlend = PVR_BLEND_SRCALPHA;
            }

            if (dstBlend) {
                *dstBlend = PVR_BLEND_INVSRCALPHA;
            }

            return true;

        case INK_ADD:
            if (srcBlend) {
                *srcBlend = PVR_BLEND_SRCALPHA;
            }

            if (dstBlend) {
                *dstBlend = PVR_BLEND_ONE;
            }

            return true;

        case INK_SUB:
            // Handling this weirdly. It it supported, but yeah.
            //printf("[pvr] WARNING: unsupported ink effect INK_SUB; skipping sprite\n");
            return false;

        case INK_TINT:
            if (srcBlend) {
                *srcBlend = PVR_BLEND_SRCALPHA;
            }

            if (dstBlend) {
                *dstBlend = PVR_BLEND_INVSRCALPHA;
            }
            return true;

        case INK_MASKED:
//            printf("[pvr] WARNING: unsupported ink effect INK_MASKED; skipping sprite\n");
            return false;

        case INK_UNMASKED:
//            printf("[pvr] WARNING: unsupported ink effect INK_UNMASKED; skipping sprite\n");
            return false;

        default:
            return false;
    }
}

// static
bool RenderDevice::PreparePrimitive(int primitiveType,
                                    uint32 gamePaletteBankIndex,
                                    uint32 pvrPaletteBankIndex,
                                    int srcBlend,
                                    int dstBlend,
                                    pvr_ptr_t texture) {
    if (PaletteFlags::GetDirty(gamePaletteBankIndex)
        || pvrPaletteBankIndex != gamePaletteBankIndex) {
        PopulatePvrPalette(gamePaletteBankIndex, pvrPaletteBankIndex);
        PaletteFlags::ClearDirty(gamePaletteBankIndex);
    }

    if (lastPrimitiveType != primitiveType
        || !PaletteFlags::BankIsSame(gamePaletteBankIndex)
        || srcBlend != lastSrcBlend
        || dstBlend != lastDstBlend
        || texture != lastTexture) {
        lastPrimitiveType = static_cast<PrimitiveTypes>(primitiveType);
        PaletteFlags::SetBank(gamePaletteBankIndex);
        lastSrcBlend = srcBlend;
        lastDstBlend = dstBlend;
        lastTexture = texture;

        return true;
    }

    return false;
}

extern "C" {
    extern int doing_sub_mode;
    int reset_texpoly = 0;
    int reset_colpoly = 0;
    int reset_linepoly = 0;
    int reset_facepoly = 0;
    uint32_t lastpalbank;
};

// static
void RenderDevice::PrepareTexturedQuadDR(int32 y, const GFXSurface* surface) {
    const uint32 gamePaletteBankIndex = GetGamePaletteBankIndex(y);
    const uint32 pvrPaletteBankIndex = GameToPvrPaletteBankIndex(gamePaletteBankIndex);
    lastpalbank = pvrPaletteBankIndex;
    
    if (PreparePrimitive(PrimitiveTypes_TexturedQuadDR,
                         gamePaletteBankIndex,
                         pvrPaletteBankIndex,
                         PVR_BLEND_SRCALPHA,
                         PVR_BLEND_INVSRCALPHA,
                         surface->texture)) {
        if (!lastPrimitiveWasConsumed) {
            printf("[pvr] [NG] LAST PRIMITIVE NOT CONSUMED BEFORE CALL TO %s\n", __FUNCTION__);
        }
        lastPrimitiveWasConsumed = false;

        pvr_sprite_cxt_t context;
        pvr_sprite_cxt_txr(
                &context,
                PVR_LIST_PT_POLY,
                PVR_TXRFMT_PAL8BPP | PVR_TXRFMT_8BPP_PAL(pvrPaletteBankIndex),
                surface->width,
                surface->height,
                surface->texture,
                PVR_FILTER_NEAREST
        );

        context.depth.comparison = PVR_DEPTHCMP_ALWAYS;
        context.depth.write = 0;

        auto *header = reinterpret_cast<pvr_sprite_hdr_t *>(pvr_dr_target(drState));
        pvr_sprite_compile(header, &context);
        pvr_dr_commit(header);
    }
}

// static
void RenderDevice::PrepareTexturedQuadDMA(int32 y, const GFXSurface* surface) {  
    const uint32 gamePaletteBankIndex = GetGamePaletteBankIndex(y);
    const uint32 pvrPaletteBankIndex = GameToPvrPaletteBankIndex(gamePaletteBankIndex);
    lastpalbank = pvrPaletteBankIndex;
    
    if (PreparePrimitive(PrimitiveTypes_TexturedQuadDMA,
                         gamePaletteBankIndex,
                         pvrPaletteBankIndex,
                         PVR_BLEND_SRCALPHA,
                         PVR_BLEND_INVSRCALPHA,
                         surface->texture)) {
        if (!lastPrimitiveWasConsumed) {
            printf("[pvr] [NG] LAST PRIMITIVE NOT CONSUMED BEFORE CALL TO %s\n", __FUNCTION__);
        }
        lastPrimitiveWasConsumed = false;
        pvr_sprite_cxt_t context;
        pvr_sprite_cxt_txr(
                &context,
                PVR_LIST_TR_POLY,
                PVR_TXRFMT_PAL8BPP | PVR_TXRFMT_8BPP_PAL(pvrPaletteBankIndex),
                surface->width,
                surface->height,
                surface->texture,
                PVR_FILTER_NEAREST
        );

        context.depth.comparison = PVR_DEPTHCMP_ALWAYS;
        context.depth.write = 0;
        pvr_sprite_hdr_t *hdr_ptr = (pvr_sprite_hdr_t *)pvr_vertbuf_tail(PVR_LIST_TR_POLY);
        pvr_sprite_compile(hdr_ptr, &context);
        pvr_vertbuf_written(PVR_LIST_TR_POLY, sizeof(pvr_sprite_hdr_t));
    }
}

// static
void RenderDevice::DrawTexturedQuadDR(
        int32 x, int32 y,
        int32 width, int32 height,
        int32 sprX0, int32 sprX1,
        int32 sprY0, int32 sprY1,
        const GFXSurface* surface
) {
    if (lastPrimitiveType != PrimitiveTypes_TexturedQuadDR) {
        printf("[pvr] [NG] ATTEMPTED TO DRAW TexturedQuadDR BEFORE PREPPING!\n");
        return;
    }

    lastPrimitiveWasConsumed = true;

    // Compute constants up-front.
    const float x0 = static_cast<float>(x) * pixelScaleX;
    const float x1 = x0 + (static_cast<float>(width) * pixelScaleX);
    const float y0 = static_cast<float>(y) * pixelScaleY;
    const float y1 = y0 + (static_cast<float>(height) * pixelScaleY);
    const float u0 = shz_div_posf(sprX0, surface->width);
    const float v0 = shz_div_posf(sprY0, surface->height);
    const float u1 = shz_div_posf(sprX1, surface->width);
    const float v1 = shz_div_posf(sprY1, surface->height);
    const float z  = GetDepth();

    // First 32 bytes of a `pvr_sprite_txr_t` structure mapped to a SQ.
    auto *spr1 = reinterpret_cast<pvr_sprite_txr_t *>(pvr_dr_target(drState));
    spr1->ax = x0;
    spr1->ay = y0;
    spr1->az = z;
    spr1->bx = x1;
    spr1->by = y0;
    spr1->bz = z;
    spr1->cx = x1;
    spr1->flags = PVR_CMD_VERTEX_EOL;
    pvr_dr_commit(spr1);

    /* NOTE: Disgusting manual pointer offsetting is due to the fact that KOS's
            PVR DR API only returns `pvr_vertex_t*,` even though it works fine with
            all geometry types. */
    // Second 32 bytes of a 'pvr_sprite_txr_t' structure mapped to the second SQ.
    auto *spr2 = reinterpret_cast<pvr_sprite_txr_t *>(reinterpret_cast<uintptr_t>(pvr_dr_target(drState)) - 32);
    spr2->cy = y1;
    spr2->cz = z;
    spr2->dx = x0;
    spr2->dy = y1;
    spr2->auv = PVR_PACK_16BIT_UV(u0, v0);
    spr2->buv = PVR_PACK_16BIT_UV(u1, v0);
    spr2->cuv = PVR_PACK_16BIT_UV(u1, v1);
    pvr_dr_commit(reinterpret_cast<uint8_t *>(spr2) + 32);
}

// static
void RenderDevice::DrawTexturedQuadDMA(
        int32 x, int32 y,
        int32 width, int32 height,
        int32 sprX0, int32 sprX1,
        int32 sprY0, int32 sprY1,
        const GFXSurface* surface
) {
    if (lastPrimitiveType != PrimitiveTypes_TexturedQuadDMA) {
        printf("[pvr] [NG] ATTEMPTED TO DRAW TexturedQuadDMA BEFORE PREPPING!\n");
        return;
    }

    lastPrimitiveWasConsumed = true;

    // Compute constants up-front.
    const float x0 = static_cast<float>(x) * pixelScaleX;
    const float x1 = x0 + (static_cast<float>(width) * pixelScaleX);
    const float y0 = static_cast<float>(y) * pixelScaleY;
    const float y1 = y0 + (static_cast<float>(height) * pixelScaleY);
    const float u0 = shz_div_posf(sprX0, surface->width);
    const float v0 = shz_div_posf(sprY0, surface->height);
    const float u1 = shz_div_posf(sprX1, surface->width);
    const float v1 = shz_div_posf(sprY1, surface->height);
    const float z  = GetDepth();// + 2.0f;

    pvr_sprite_txr_t *spr = (pvr_sprite_txr_t *)pvr_vertbuf_tail(PVR_LIST_TR_POLY);
    spr->ax = x0;
    spr->ay = y0;
    spr->az = z;
    spr->bx = x1;
    spr->by = y0;
    spr->bz = z;
    spr->cx = x1;
    spr->cy = y1;
    spr->cz = z;
    spr->dx = x0;
    spr->dy = y1;
    spr->auv = PVR_PACK_16BIT_UV(u0, v0);
    spr->buv = PVR_PACK_16BIT_UV(u1, v0);
    spr->cuv = PVR_PACK_16BIT_UV(u1, v1);
    spr->flags = PVR_CMD_VERTEX_EOL;
    pvr_vertbuf_written(PVR_LIST_TR_POLY, sizeof(pvr_sprite_txr_t));
}

// JN64 TODO
// static
void RenderDevice::DrawTexturedQuadEx(
        const Vector2& upperLeft, const Vector2& upperRight,
        const Vector2& lowerLeft, const Vector2& lowerRight,
        int32 sprX0, int32 sprX1,
        int32 sprY0, int32 sprY1,
        const GFXSurface* surface
) {
    if (lastPrimitiveType != PrimitiveTypes_TexturedQuadDR) {
        printf("[pvr] [NG] ATTEMPTED TO DRAW TexturedQuadDR BEFORE PREPPING!\n");
        return;
    }

    lastPrimitiveWasConsumed = true;

    struct vec2f { float x; float y; };

    const vec2f upperLeftF {
        static_cast<float>(upperLeft.x) * pixelScaleX,
        static_cast<float>(upperLeft.y) * pixelScaleY
    };

    const vec2f upperRightF {
        static_cast<float>(upperRight.x) * pixelScaleX,
        static_cast<float>(upperRight.y) * pixelScaleY
    };

    const vec2f lowerLeftF {
        static_cast<float>(lowerLeft.x) * pixelScaleX,
        static_cast<float>(lowerLeft.y) * pixelScaleY
    };

    const vec2f lowerRightF {
        static_cast<float>(lowerRight.x) * pixelScaleX,
        static_cast<float>(lowerRight.y) * pixelScaleY
    };

    // Compute constants up-front.
    const float u0 = shz_div_posf(sprX0, surface->width);
    const float v0 = shz_div_posf(sprY0, surface->height);
    const float u1 = shz_div_posf(sprX1, surface->width);
    const float v1 = shz_div_posf(sprY1, surface->height);
    const float z  = GetDepth();

    // First 32 bytes of a `pvr_sprite_txr_t` structure mapped to a SQ.
    auto *spr1 = reinterpret_cast<pvr_sprite_txr_t *>(pvr_dr_target(drState));
    spr1->ax = upperLeftF.x;
    spr1->ay = upperLeftF.y;
    spr1->az = z;
    spr1->bx = upperRightF.x;
    spr1->by = upperRightF.y;
    spr1->bz = z;
    spr1->cx = lowerRightF.x;
    spr1->flags = PVR_CMD_VERTEX_EOL;
    pvr_dr_commit(spr1);

    /* NOTE: Disgusting manual pointer offsetting is due to the fact that KOS's
             PVR DR API only returns `pvr_vertex_t*,` even though it works fine with
             all geometry types. */
    // Second 32 bytes of a 'pvr_sprite_txr_t' structure mapped to the second SQ.
    auto *spr2 = reinterpret_cast<pvr_sprite_txr_t *>(reinterpret_cast<uintptr_t>(pvr_dr_target(drState)) - 32);
    spr2->cy = lowerRightF.y;
    spr2->cz = z;
    spr2->dx = lowerLeftF.x;
    spr2->dy = lowerLeftF.y;
    spr2->auv = PVR_PACK_16BIT_UV(u0, v0);
    spr2->buv = PVR_PACK_16BIT_UV(u1, v0);
    spr2->cuv = PVR_PACK_16BIT_UV(u1, v1);
    pvr_dr_commit(reinterpret_cast<uint8_t *>(spr2) + 32);
}

// static
void RenderDevice::PrepareTexturedPolyDR(int32 y, int srcBlend, int dstBlend, const GFXSurface *surface) {
    const uint32 gamePaletteBankIndex = GetGamePaletteBankIndex(y);
    const uint32 pvrPaletteBankIndex = GameToPvrPaletteBankIndex(gamePaletteBankIndex);
    lastpalbank = pvrPaletteBankIndex;

    if (PreparePrimitive(PrimitiveTypes_TexturedPolyDR,
                         gamePaletteBankIndex,
                         pvrPaletteBankIndex,
                         srcBlend,
                         dstBlend,
                         surface->texture)) {
        if (!lastPrimitiveWasConsumed) {
            printf("[pvr] [NG] LAST PRIMITIVE NOT CONSUMED BEFORE CALL TO %s\n", __FUNCTION__);
        }
        lastPrimitiveWasConsumed = false;

        pvr_poly_cxt_t context;
        if (surface->is_vq) {
            pvr_poly_cxt_txr(
                    &context,
                    PVR_LIST_PT_POLY,
                    PVR_TXRFMT_VQ_ENABLE | PVR_TXRFMT_ARGB1555 | PVR_TXRFMT_TWIDDLED,
                    surface->width,
                    surface->height,
                    surface->texture,
                    PVR_FILTER_NEAREST
            );
        } else {
            pvr_poly_cxt_txr(
                    &context,
                    PVR_LIST_PT_POLY,
                    PVR_TXRFMT_PAL8BPP | PVR_TXRFMT_8BPP_PAL(pvrPaletteBankIndex),
                    surface->width,
                    surface->height,
                    surface->texture,
                    PVR_FILTER_NEAREST
            );
        }
        context.gen.alpha = 1;
        context.depth.comparison = PVR_DEPTHCMP_ALWAYS;
        context.depth.write = 0;
        context.blend.src = srcBlend;
        context.blend.dst = dstBlend;

        // Compile polygon header directly into SQ to avoid having to copy it.
        auto *header = reinterpret_cast<pvr_poly_hdr_t *>(pvr_dr_target(drState));
        pvr_poly_compile(header, &context);
        pvr_dr_commit(header);    
    }
}

#define next_dma_hdr (pvr_poly_hdr_t *)pvr_vertbuf_tail(PVR_LIST_TR_POLY)
#define next_dma_vert (pvr_vertex_t *)pvr_vertbuf_tail(PVR_LIST_TR_POLY)

// static
void RenderDevice::PrepareTexturedPolyDMA(int32 y, int srcBlend, int dstBlend, const GFXSurface *surface) {
    const uint32 gamePaletteBankIndex = GetGamePaletteBankIndex(y);
    const uint32 pvrPaletteBankIndex = GameToPvrPaletteBankIndex(gamePaletteBankIndex);
    lastpalbank = pvrPaletteBankIndex;
    
    if (PreparePrimitive(PrimitiveTypes_TexturedPolyDMA,
                         gamePaletteBankIndex,
                         pvrPaletteBankIndex,
                         srcBlend,
                         dstBlend,
                         surface->texture) || reset_texpoly) {
        if (!lastPrimitiveWasConsumed) {
            printf("[pvr] [NG] LAST PRIMITIVE NOT CONSUMED BEFORE CALL TO %s\n", __FUNCTION__);
        }

        reset_texpoly = 0;

        lastPrimitiveWasConsumed = false;
        if (doing_sub_mode) return;

        pvr_poly_cxt_t context;
 
        if (surface->is_vq) {
            pvr_poly_cxt_txr(
                    &context,
                    PVR_LIST_TR_POLY,
                    PVR_TXRFMT_VQ_ENABLE | PVR_TXRFMT_ARGB1555 | PVR_TXRFMT_TWIDDLED,
                    surface->width,
                    surface->height,
                    surface->texture,
                    PVR_FILTER_NEAREST
            );
        } else {
            pvr_poly_cxt_txr(
                    &context,
                    PVR_LIST_TR_POLY,
                    PVR_TXRFMT_PAL8BPP | PVR_TXRFMT_8BPP_PAL(pvrPaletteBankIndex),
                    surface->width,
                    surface->height,
                    surface->texture,
                    PVR_FILTER_NEAREST
            );
        }

        context.gen.alpha = 1;
        context.depth.comparison = PVR_DEPTHCMP_ALWAYS;
        context.depth.write = 0;
        context.blend.src = srcBlend;
        context.blend.dst = dstBlend;

        pvr_poly_hdr_t *hdr_ptr = (pvr_poly_hdr_t *)pvr_vertbuf_tail(PVR_LIST_TR_POLY);
        pvr_poly_compile(hdr_ptr, &context);   
        pvr_vertbuf_written(PVR_LIST_TR_POLY, sizeof(pvr_poly_hdr_t));     
    }
}

// static
void RenderDevice::DrawTexturedPolyDR(
        int32 x, int32 y,
        int32 ox, int32 oy,
        int32 width, int32 height,
        int32 sprX0, int32 sprX1,
        int32 sprY0, int32 sprY1,
        int32 rotation,
        int32 alpha,
        const GFXSurface *surface
) {
    if (lastPrimitiveType != PrimitiveTypes_TexturedPolyDR) {
        printf("[pvr] [NG] ATTEMPTED TO DRAW TexturedPolyDR BEFORE PREPPING!\n");
        return;
    }

    lastPrimitiveWasConsumed = true;

    // Compute constants up-front
    const float x0 = static_cast<float>(x) * pixelScaleX;
    const float y0 = static_cast<float>(y) * pixelScaleY;
    const float x1 = x0 + (static_cast<float>(width)  * pixelScaleX);
    const float y1 = y0 + (static_cast<float>(height) * pixelScaleY);
    const float z  = GetDepth();
    const float u0 = shz_div_posf(sprX0, surface->width);
    const float v0 = shz_div_posf(sprY0, surface->height);
    const float u1 = shz_div_posf(sprX1, surface->width);
    const float v1 = shz_div_posf(sprY1, surface->height);
    const uint32 argb = 0x00ffffffu | (alpha << 24);

    /* Since we have to potentially modify the vertex stream later on to apply
       rotation to it, we're better off constructing it in RAM rather than 
       using the PVR DR API to write to the SQs directly. SQs are read-only! */
    pvr_vertex_t verts[4] = {
        {
            .flags = PVR_CMD_VERTEX,
            .x = x0,
            .y = y0,
            .z = z,
            .u = u0,
            .v = v0,
            .argb = argb
        },
        {
            .flags = PVR_CMD_VERTEX,
            .x = x1,
            .y = y0,
            .z = z,
            .u = u1,
            .v = v0,
            .argb = argb
        },
        {
            .flags = PVR_CMD_VERTEX,
            .x = x0,
            .y = y1,
            .z = z,
            .u = u0,
            .v = v1,
            .argb = argb
        },
        {
            .flags = PVR_CMD_VERTEX_EOL,
            .x = x1,
            .y = y1,
            .z = z,
            .u = u1,
            .v = v1,
            .argb = argb
        }
    };

    rotation = (rotation & 0x1FF) << 7;

    if (rotation != 0) {
        const float cx = static_cast<float>(ox) * pixelScaleX;
        const float cy = static_cast<float>(oy) * pixelScaleY;

        const float s = fast_isin(rotation);
        const float c = fast_icos(rotation);

        auto rotate = [cx, cy, s, c](pvr_vertex_t& p) {
            p.x -= cx;
            p.y -= cy;

            const auto px = p.x * c - p.y * s;
            const auto py = p.x * s + p.y * c;

            p.x = px + cx;
            p.y = py + cy;
        };

        rotate(verts[0]);
        rotate(verts[1]);
        rotate(verts[2]);
        rotate(verts[3]);
    }

    sq_fast_cpy(SQ_MASK_DEST(PVR_TA_INPUT), verts, 4);
}

// static
void RenderDevice::DrawTexturedPolyDMA(
        int32 x, int32 y,
        int32 ox, int32 oy,
        int32 width, int32 height,
        int32 sprX0, int32 sprX1,
        int32 sprY0, int32 sprY1,
        int32 rotation,
        int32 alpha,
        const GFXSurface *surface
) {
    if (lastPrimitiveType != PrimitiveTypes_TexturedPolyDMA) {
        printf("[pvr] [NG] ATTEMPTED TO DRAW TexturedPolyDMA BEFORE PREPPING!\n");
        return;
    }

    lastPrimitiveWasConsumed = true;

    // Compute constants up-front
    const float x0 = static_cast<float>(x) * pixelScaleX;
    const float y0 = static_cast<float>(y) * pixelScaleY;
    const float x1 = x0 + (static_cast<float>(width)  * pixelScaleX);
    const float y1 = y0 + (static_cast<float>(height) * pixelScaleY);
    const float z  = GetDepth();
    const float u0 = shz_div_posf(sprX0, surface->width);
    const float v0 = shz_div_posf(sprY0, surface->height);
    const float u1 = shz_div_posf(sprX1, surface->width);
    const float v1 = shz_div_posf(sprY1, surface->height);
    const uint32 argb = 0x00ffffffu | (alpha << 24);

    /* Since we have to potentially modify the vertex stream later on to apply
       rotation to it, we're better off constructing it in RAM rather than 
       using the PVR DR API to write to the SQs directly. SQs are read-only! */
    pvr_vertex_t srcverts[4] = {
        {
            .flags = PVR_CMD_VERTEX,
            .x = x0,
            .y = y0,
            .z = z,
            .u = u0,
            .v = v0,
            .argb = argb
        },
        {
            .flags = PVR_CMD_VERTEX,
            .x = x1,
            .y = y0,
            .z = z,
            .u = u1,
            .v = v0,
            .argb = argb
        },
        {
            .flags = PVR_CMD_VERTEX,
            .x = x0,
            .y = y1,
            .z = z,
            .u = u0,
            .v = v1,
            .argb = argb
        },
        {
            .flags = PVR_CMD_VERTEX_EOL,
            .x = x1,
            .y = y1,
            .z = z,
            .u = u1,
            .v = v1,
            .argb = argb
        }
    };

    rotation = (rotation & 0x1FF) << 7;

    if (rotation != 0) {
        const float cx = static_cast<float>(ox) * pixelScaleX;
        const float cy = static_cast<float>(oy) * pixelScaleY;

        const float s = fast_isin(rotation);
        const float c = fast_icos(rotation);

        auto rotate = [cx, cy, s, c](pvr_vertex_t& p) {
            p.x -= cx;
            p.y -= cy;

            const auto px = p.x * c - p.y * s;
            const auto py = p.x * s + p.y * c;

            p.x = px + cx;
            p.y = py + cy;
        };

        rotate(srcverts[0]);
        rotate(srcverts[1]);
        rotate(srcverts[2]);
        rotate(srcverts[3]);
    }

    if(!doing_sub_mode) {
        pvr_vertex_t *newverts = next_dma_vert;
        memcpy(newverts, srcverts, 4 * sizeof(pvr_vertex_t));
        pvr_vertbuf_written(PVR_LIST_TR_POLY, 4 * sizeof(pvr_vertex_t));
    } else {
        reset_texpoly = 1;

        pvr_poly_cxt_t context;
        pvr_poly_cxt_t contextcol;
        if (surface->is_vq) {
            pvr_poly_cxt_txr(
                    &context,
                    PVR_LIST_TR_POLY,
                    PVR_TXRFMT_VQ_ENABLE | PVR_TXRFMT_ARGB1555 | PVR_TXRFMT_TWIDDLED,
                    surface->width,
                    surface->height,
                    surface->texture,
                    PVR_FILTER_NEAREST
            );
        } else {
            pvr_poly_cxt_txr(
                    &context,
                    PVR_LIST_TR_POLY,
                    PVR_TXRFMT_PAL8BPP | PVR_TXRFMT_8BPP_PAL(lastpalbank),
                    surface->width,
                    surface->height,
                    surface->texture,
                    PVR_FILTER_NEAREST
            );
        }
        pvr_poly_cxt_col(&contextcol, PVR_LIST_TR_POLY);
        // step one
        // render a fully transparent white polygon to primary buffer
        // with alpha disabled
        // INVDESTCOLOR/ZERO
        {
            contextcol.depth.comparison = PVR_DEPTHCMP_ALWAYS;
            contextcol.depth.write = 0;
            contextcol.gen.alpha = 0;
            contextcol.blend.src = PVR_BLEND_INVDESTCOLOR;
            contextcol.blend.dst = PVR_BLEND_ZERO;
            contextcol.blend.src_enable = 0;
            contextcol.blend.dst_enable = 0;

            pvr_poly_hdr_t *hdr_ptr = (pvr_poly_hdr_t *)pvr_vertbuf_tail(PVR_LIST_TR_POLY);
            pvr_poly_compile(hdr_ptr, &contextcol);
            pvr_vertbuf_written(PVR_LIST_TR_POLY, sizeof(pvr_poly_hdr_t));

            pvr_vertex_t *newverts = next_dma_vert;
            memcpy(newverts, srcverts, 4 * sizeof(pvr_vertex_t));
            for (int i = 0; i < 4; i++) {
                newverts[i].argb = 0x00ffffff;
            }
            pvr_vertbuf_written(PVR_LIST_TR_POLY, 4 * sizeof(pvr_vertex_t));
        }

        // step two
        // render a full white poly to second buffer
        {
            pvr_poly_cxt_col(&contextcol, PVR_LIST_TR_POLY);
            contextcol.depth.comparison = PVR_DEPTHCMP_ALWAYS;
            contextcol.depth.write = 0;
            contextcol.gen.alpha = 0;
            contextcol.blend.src_enable = 0;
            contextcol.blend.dst_enable = 1;

            pvr_poly_hdr_t *hdr_ptr = next_dma_hdr;
            pvr_poly_compile(hdr_ptr, &contextcol);
            pvr_vertbuf_written(PVR_LIST_TR_POLY, sizeof(pvr_poly_hdr_t));

            pvr_vertex_t *newverts = next_dma_vert;
            memcpy(newverts, srcverts, 4 * sizeof(pvr_vertex_t));
            for (int i = 0; i < 4; i++) {
                newverts[i].argb = 0xffffffff;
            }
            pvr_vertbuf_written(PVR_LIST_TR_POLY, 4 * sizeof(pvr_vertex_t));
        }

        // step three
        // render textured poly to second buffer
        // ZERO/INVDESTCOLOR
        {
            context.depth.comparison = PVR_DEPTHCMP_ALWAYS;
            context.depth.write = 0;
            context.gen.alpha = 1;
            context.txr.env = PVR_TXRENV_MODULATEALPHA;
            context.blend.src_enable = 0;
            context.blend.dst_enable = 1;
            context.blend.src = PVR_BLEND_ZERO;
            context.blend.dst = PVR_BLEND_INVDESTCOLOR;

            pvr_poly_hdr_t *hdr_ptr = next_dma_hdr;
            pvr_poly_compile(hdr_ptr, &context);
            pvr_vertbuf_written(PVR_LIST_TR_POLY, sizeof(pvr_poly_hdr_t));

            pvr_vertex_t *newverts = next_dma_vert;
            memcpy(newverts, srcverts, 4 * sizeof(pvr_vertex_t));
            for (int i = 0; i < 4; i++) {
                newverts[i].argb = argb;
            }
            pvr_vertbuf_written(PVR_LIST_TR_POLY, 4 * sizeof(pvr_vertex_t));
        }

        // step four
        // flush the second buffer/draw it to primary buffer
        {
            contextcol.depth.comparison = PVR_DEPTHCMP_ALWAYS;
            contextcol.depth.write = 0;
            contextcol.blend.src_enable = 1;
            contextcol.blend.dst_enable = 0;
            contextcol.gen.alpha = 0;
            contextcol.blend.src = PVR_BLEND_INVSRCALPHA;
            contextcol.blend.dst = PVR_BLEND_ONE;

            pvr_poly_hdr_t *hdr_ptr = next_dma_hdr;
            pvr_poly_compile(hdr_ptr, &contextcol);
            pvr_vertbuf_written(PVR_LIST_TR_POLY, sizeof(pvr_poly_hdr_t));

            pvr_vertex_t *newverts = next_dma_vert;
            memcpy(newverts, srcverts, 4 * sizeof(pvr_vertex_t));
            for (int i = 0; i < 4; i++) {
                newverts[i].argb = 0xffffffff;
            }
            pvr_vertbuf_written(PVR_LIST_TR_POLY, 4 * sizeof(pvr_vertex_t));
        }

        // step five
        // render a fully opaque white polygon to primary buffer
        // with alpha disabled
        // INVDESTCOLOR/ZERO
        {
            contextcol.depth.comparison = PVR_DEPTHCMP_ALWAYS;
            contextcol.depth.write = 0;
            contextcol.gen.alpha = 0;
            contextcol.blend.src_enable = 0;
            contextcol.blend.dst_enable = 0;
            contextcol.blend.src = PVR_BLEND_INVDESTCOLOR;
            contextcol.blend.dst = PVR_BLEND_ZERO;
            contextcol.txr.enable = contextcol.txr2.enable = 0;

            pvr_poly_hdr_t *hdr_ptr = next_dma_hdr;
            pvr_poly_compile(hdr_ptr, &contextcol);
            pvr_vertbuf_written(PVR_LIST_TR_POLY, sizeof(pvr_poly_hdr_t));

            pvr_vertex_t *newverts = next_dma_vert;
            memcpy(newverts, srcverts, 4 * sizeof(pvr_vertex_t));
            for (int i = 0; i < 4; i++) {
                newverts[i].argb = 0xffffffff;
            }
            pvr_vertbuf_written(PVR_LIST_TR_POLY, 4 * sizeof(pvr_vertex_t));
        }
    }
}

// static
void RenderDevice::PrepareColoredPolyDR(int32 y, int srcBlend, int dstBlend) {
    const uint32 gamePaletteBankIndex = GetGamePaletteBankIndex(y);
    const uint32 pvrPaletteBankIndex = GameToPvrPaletteBankIndex(gamePaletteBankIndex);

    if (PreparePrimitive(PrimitiveTypes_ColoredPolyDR,
                         gamePaletteBankIndex,
                         pvrPaletteBankIndex,
                         srcBlend,
                         dstBlend,
                         nullptr)) {
        if (!lastPrimitiveWasConsumed) {
            printf("[pvr] [NG] LAST PRIMITIVE NOT CONSUMED BEFORE CALL TO %s\n", __FUNCTION__);
        }

        lastPrimitiveWasConsumed = false;

        pvr_poly_cxt_t context;
        pvr_poly_cxt_col(&context, PVR_LIST_PT_POLY);

        context.depth.comparison = PVR_DEPTHCMP_ALWAYS;
        context.depth.write = 0;
        context.gen.alpha = 1;
        context.blend.src = srcBlend;
        context.blend.dst = dstBlend;

        // Compile polygon header directly into SQ to avoid having to copy it.
        auto *header = reinterpret_cast<pvr_poly_hdr_t *>(pvr_dr_target(drState));
        pvr_poly_compile(header, &context);
        pvr_dr_commit(header);
    }
}

// static
void RenderDevice::PrepareColoredPolyDMA(int32 y, int srcBlend, int dstBlend) {
    const uint32 gamePaletteBankIndex = GetGamePaletteBankIndex(y);
    const uint32 pvrPaletteBankIndex = GameToPvrPaletteBankIndex(gamePaletteBankIndex);
    
    if (PreparePrimitive(PrimitiveTypes_ColoredPolyDMA,
                         gamePaletteBankIndex,
                         pvrPaletteBankIndex,
                         srcBlend,
                         dstBlend,
                         nullptr) || reset_colpoly) {
        if (!lastPrimitiveWasConsumed) {
            printf("[pvr] [NG] LAST PRIMITIVE NOT CONSUMED BEFORE CALL TO %s\n", __FUNCTION__);
        }

        reset_colpoly = 0;
        lastPrimitiveWasConsumed = false;

        pvr_poly_cxt_t context;
        pvr_poly_cxt_col(&context, PVR_LIST_TR_POLY);

        context.depth.comparison = PVR_DEPTHCMP_ALWAYS;
        context.depth.write = 0;
        context.gen.alpha = 1;
        context.blend.src = srcBlend;
        context.blend.dst = dstBlend;
        
        pvr_poly_hdr_t *hdr_ptr = next_dma_hdr;
        pvr_poly_compile(hdr_ptr, &context);
        pvr_vertbuf_written(PVR_LIST_TR_POLY, sizeof(pvr_poly_hdr_t));
    }
}

// static
void RenderDevice::DrawColoredPolyDR(
        int32 x, int32 y,
        int32 width, int32 height,
        uint32 color
) {
    if (lastPrimitiveType != PrimitiveTypes_ColoredPolyDR) {
        printf("[pvr] [NG] ATTEMPTED TO DRAW ColoredPolyDR BEFORE PREPPING!\n");
        return;
    }

    lastPrimitiveWasConsumed = true;

    // Compute constants up-front.
    const float x0 = static_cast<float>(x) * pixelScaleX;
    const float y0 = static_cast<float>(y) * pixelScaleY;
    const float x1 = x0 + static_cast<float>(width) * pixelScaleX;
    const float y1 = y0 + static_cast<float>(height) * pixelScaleY;
    const float z  = GetDepth();

    // top left
    auto* vert = reinterpret_cast<pvr_vertex_t *>(pvr_dr_target(drState));
    vert->flags = PVR_CMD_VERTEX;
    vert->argb = color;
    vert->z = z;
    vert->x = x0;
    vert->y = y0;
    pvr_dr_commit(vert);

    // top right
    vert = reinterpret_cast<pvr_vertex_t *>(pvr_dr_target(drState));
    vert->flags = PVR_CMD_VERTEX;
    vert->argb = color;
    vert->z = z;
    vert->x = x1;
    vert->y = y0;
    pvr_dr_commit(vert);

    /* NOTE: After you have submitted two vertices to the PVR DR API,
             you can safely skip setting anything held constant for
             all verts, since both of the 2 SQs already have the value. */
    // bottom left
    vert = reinterpret_cast<pvr_vertex_t *>(pvr_dr_target(drState));
    vert->flags = PVR_CMD_VERTEX;
    vert->argb = color;
    vert->z = z;
    vert->x = x0;
    vert->y = y1;
    pvr_dr_commit(vert);

    // bottom right
    vert = reinterpret_cast<pvr_vertex_t *>(pvr_dr_target(drState));
    vert->flags = PVR_CMD_VERTEX_EOL;
    vert->argb = color;
    vert->z = z;
    vert->x = x1;
    vert->y = y1;
    pvr_dr_commit(vert);
}

// static
void RenderDevice::DrawColoredPolyDMA(
        int32 x, int32 y,
        int32 width, int32 height,
        uint32 color
) {
    if (lastPrimitiveType != PrimitiveTypes_ColoredPolyDMA) {
        printf("[pvr] [NG] ATTEMPTED TO DRAW ColoredPolyDMA BEFORE PREPPING!\n");
        return;
    }

    lastPrimitiveWasConsumed = true;

    // Compute constants up-front.
    const float x0 = static_cast<float>(x) * pixelScaleX;
    const float y0 = static_cast<float>(y) * pixelScaleY;
    const float x1 = x0 + static_cast<float>(width) * pixelScaleX;
    const float y1 = y0 + static_cast<float>(height) * pixelScaleY;
    const float z  = GetDepth();

    if (doing_tint == 2) {
        color = 0x7f000000;
        doing_tint = 0;
    }

    if (!doing_tint) {
        // top left
        pvr_vertex_t *vert = next_dma_vert;
        vert->flags = PVR_CMD_VERTEX;
        vert->x = x0;
        vert->y = y0;
        vert->z = z;
        vert->argb = color;
        vert++;

        // top right
        vert->flags = PVR_CMD_VERTEX;
        vert->x = x1;
        vert->y = y0;
        vert->z = z;
        vert->argb = color;
        vert++;

        // bottom left
        vert->flags = PVR_CMD_VERTEX;
        vert->x = x0;
        vert->y = y1;
        vert->z = z;
        vert->argb = color;
        vert++;

        // bottom right
        vert->flags = PVR_CMD_VERTEX_EOL;
        vert->x = x1;
        vert->y = y1;
        vert->z = z;
        vert->argb = color;

        pvr_vertbuf_written(PVR_LIST_TR_POLY, 4 * sizeof(pvr_vertex_t));
    } else {
        pvr_poly_cxt_t context;
        pvr_poly_cxt_col(&context, PVR_LIST_TR_POLY);

        context.gen.alpha = 1;
        context.depth.comparison = PVR_DEPTHCMP_ALWAYS;
        context.depth.write = 0;
        context.blend.src = PVR_BLEND_INVDESTCOLOR;
        context.blend.dst = PVR_BLEND_ZERO;
        
        pvr_poly_hdr_t *hdr_ptr = next_dma_hdr;
        pvr_poly_compile(hdr_ptr, &context);
        pvr_vertbuf_written(PVR_LIST_TR_POLY, sizeof(pvr_poly_hdr_t));

        reset_colpoly = 1;
        color = 0xffffffff;
        doing_tint = 0;

        pvr_vertex_t *vert = next_dma_vert;
        // top left
        vert->flags = PVR_CMD_VERTEX;
        vert->x = x0;
        vert->y = y0;
        vert->z = z;
        vert->argb = color;
        vert++;

        // top right
        vert->flags = PVR_CMD_VERTEX;
        vert->x = x1;
        vert->y = y0;
        vert->z = z;
        vert->argb = color;
        vert++;

        // bottom left
        vert->flags = PVR_CMD_VERTEX;
        vert->x = x0;
        vert->y = y1;
        vert->z = z;
        vert->argb = color;
        vert++;

        // bottom right
        vert->flags = PVR_CMD_VERTEX_EOL;
        vert->x = x1;
        vert->y = y1;
        vert->z = z;
        vert->argb = color;

        pvr_vertbuf_written(PVR_LIST_TR_POLY, 4 * sizeof(pvr_vertex_t));
    }
}


// static
void RenderDevice::PrepareLinePolyDR(int srcBlend, int dstBlend) {
    if ((lastPrimitiveType != PrimitiveTypes_LineDR) || 
        ((lastLineSrcBlend != srcBlend) || (lastLineDstBlend != dstBlend))) {
        if (!lastPrimitiveWasConsumed) {
            printf("[pvr] [NG] LAST PRIMITIVE NOT CONSUMED BEFORE CALL TO %s\n", __FUNCTION__);
        }

        lastPrimitiveWasConsumed = false;

        pvr_poly_cxt_t context;
        pvr_poly_cxt_col(&context, PVR_LIST_PT_POLY);

        context.depth.comparison = PVR_DEPTHCMP_ALWAYS;
        context.depth.write = 0;
        context.gen.alpha = 1;
        context.blend.src = srcBlend;
        context.blend.dst = dstBlend;
        auto *header = reinterpret_cast<pvr_poly_hdr_t *>(pvr_dr_target(drState));
        pvr_poly_compile(header, &context);
        pvr_dr_commit(header);
        lastLineSrcBlend = srcBlend;
        lastLineDstBlend = dstBlend;
        lastPrimitiveType = PrimitiveTypes_LineDR;
    }
}

// static
void RenderDevice::PrepareLinePolyDMA(int srcBlend, int dstBlend) {
    if ((lastPrimitiveType != PrimitiveTypes_LineDMA) || 
        ((lastLineSrcBlend != srcBlend) || (lastLineDstBlend != dstBlend)) || 
        reset_linepoly) {
        if (!lastPrimitiveWasConsumed) {
            printf("[pvr] [NG] LAST PRIMITIVE NOT CONSUMED BEFORE CALL TO %s\n", __FUNCTION__);
        }

        reset_linepoly = 0;
        lastPrimitiveWasConsumed = false;
        pvr_poly_cxt_t context;
        pvr_poly_cxt_col(&context, PVR_LIST_TR_POLY);
        context.gen.alpha = 1;
        context.depth.comparison = PVR_DEPTHCMP_ALWAYS;
        context.depth.write = 0;

        context.blend.src = srcBlend;
        context.blend.dst = dstBlend;
        
        pvr_poly_hdr_t *hdr_ptr = next_dma_hdr;
        pvr_poly_compile(hdr_ptr, &context);
        pvr_vertbuf_written(PVR_LIST_TR_POLY, sizeof(pvr_poly_hdr_t));
        lastLineSrcBlend = srcBlend;
        lastLineDstBlend = dstBlend;
        lastPrimitiveType = PrimitiveTypes_LineDMA;
    }
}


#define SET_LINEPOLY_VERT_DR(_x, _y, _z, _end)    { \
    vert = reinterpret_cast<pvr_vertex_t *>(pvr_dr_target(drState)); \
    vert->flags = (_end) ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX; \
    vert->x = (_x); \
    vert->y = (_y); \
    vert->z = (_z); \
    vert->argb = color; \
    pvr_dr_commit(vert); \
}

#define SET_LINEPOLY_VERT_DMA(_x, _y, _z, _end)    { \
    vert->flags = (_end) ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX; \
    vert->x = (_x); \
    vert->y = (_y); \
    vert->z = (_z); \
    vert++->argb = color; \
}


extern "C" {
#if DO_240
    int line_w = 1;
#else
    int line_w = 2;
#endif
};

// static
void RenderDevice::DrawLinePolyDR(int lx1, int ly1, int lx2, int ly2, int color)
{
    pvr_vertex_t *vert;
    float hlw_invmag;
    float dx,dy;
    float nx,ny;

    if (lastPrimitiveType != PrimitiveTypes_LineDR) {
        printf("[pvr] [NG] ATTEMPTED TO DRAW LineDR BEFORE PREPPING!\n");
        return;
    }

    lastPrimitiveWasConsumed = true;

    const float x1 = static_cast<float>(lx1) * pixelScaleX;
    const float y1 = static_cast<float>(ly1) * pixelScaleY;
    const float x2 = static_cast<float>(lx2) * pixelScaleX;
    const float y2 = static_cast<float>(ly2) * pixelScaleY;
    const float z = GetDepth();

    dx = x2 - x1;
    dy = y1 - y2;
    hlw_invmag = (1.0f / sqrtf((dx * dx) + (dy * dy)));
    hlw_invmag *= ((float)line_w * 0.5f);
    nx = dy * hlw_invmag;
    ny = dx * hlw_invmag;

    SET_LINEPOLY_VERT_DR(x1 + nx, y1 + ny, z, 0);
    SET_LINEPOLY_VERT_DR(x1 - nx, y1 - ny, z, 0);
    SET_LINEPOLY_VERT_DR(x2 + nx, y2 + ny, z, 0);
    SET_LINEPOLY_VERT_DR(x2 - nx, y2 - ny, z, 1);
}


// static
void RenderDevice::DrawLinePolyDMA(int lx1, int ly1, int lx2, int ly2, int color)
{
    pvr_vertex_t *vert = next_dma_vert;
    float hlw_invmag;
    float dx,dy;
    float nx,ny;

    if (lastPrimitiveType != PrimitiveTypes_LineDMA) {
        printf("[pvr] [NG] ATTEMPTED TO DRAW LineDMA BEFORE PREPPING!\n");
        return;
    }

    lastPrimitiveWasConsumed = true;

    const float x1 = static_cast<float>(lx1) * pixelScaleX;
    const float y1 = static_cast<float>(ly1) * pixelScaleY;
    const float x2 = static_cast<float>(lx2) * pixelScaleX;
    const float y2 = static_cast<float>(ly2) * pixelScaleY;
    const float z = GetDepth();

    dx = x2 - x1;
    dy = y1 - y2;
    hlw_invmag = (1.0f / sqrtf((dx * dx) + (dy * dy)));
    hlw_invmag *= ((float)line_w * 0.5f);
    nx = dy * hlw_invmag;
    ny = dx * hlw_invmag;

    if (!doing_tint) {
        SET_LINEPOLY_VERT_DMA(x1 + nx, y1 + ny, z, 0);
        SET_LINEPOLY_VERT_DMA(x1 - nx, y1 - ny, z, 0);
        SET_LINEPOLY_VERT_DMA(x2 + nx, y2 + ny, z, 0);
        SET_LINEPOLY_VERT_DMA(x2 - nx, y2 - ny, z, 1);

        pvr_vertbuf_written(PVR_LIST_TR_POLY, 4 * sizeof(pvr_vertex_t));
    } else {
        pvr_poly_cxt_t context;
        pvr_poly_cxt_col(&context, PVR_LIST_TR_POLY);

        context.gen.alpha = 1;
        context.depth.comparison = PVR_DEPTHCMP_ALWAYS;
        context.depth.write = 0;
        context.blend.src = PVR_BLEND_INVDESTCOLOR;
        context.blend.dst = PVR_BLEND_ZERO;
        
        pvr_poly_hdr_t *hdr_ptr = next_dma_hdr;
        pvr_poly_compile(hdr_ptr, &context);
        pvr_vertbuf_written(PVR_LIST_TR_POLY, sizeof(pvr_poly_hdr_t));

        color = 0xffffffff;

        pvr_vertex_t *vert = next_dma_vert;
        SET_LINEPOLY_VERT_DMA(x1 + nx, y1 + ny, z, 0);
        SET_LINEPOLY_VERT_DMA(x1 - nx, y1 - ny, z, 0);
        SET_LINEPOLY_VERT_DMA(x2 + nx, y2 + ny, z, 0);
        SET_LINEPOLY_VERT_DMA(x2 - nx, y2 - ny, z, 1);

        pvr_vertbuf_written(PVR_LIST_TR_POLY, 4 * sizeof(pvr_vertex_t));

        doing_tint = 0;
        reset_linepoly = 1;
    }
}

// static
void RenderDevice::PrepareRotoPoly(int srcBlend, int dstBlend, pvr_ptr_t texture) {
#if 0
    lastPrimitiveWasConsumed = false;
    lastPrimitiveType = PrimitiveTypes_Roto;
#endif
}

// static
void RenderDevice::DrawRotoPoly(int x, int y, int w, int h, float u, float v) {
#if 0
    lastPrimitiveWasConsumed = true;
#endif
}

// static
void RenderDevice::PrepareFacePolyDR(int srcBlend, int dstBlend) {
    if ((lastPrimitiveType != PrimitiveTypes_FaceDR) || 
        ((lastFaceSrcBlend != srcBlend) || (lastFaceDstBlend != dstBlend))) {
        if (!lastPrimitiveWasConsumed) {
            printf("[pvr] [NG] LAST PRIMITIVE NOT CONSUMED BEFORE CALL TO %s\n", __FUNCTION__);
        }
        lastPrimitiveWasConsumed = false;
        pvr_poly_cxt_t context;
        pvr_poly_cxt_col(&context, PVR_LIST_PT_POLY);
        context.gen.alpha = 1;
        context.depth.comparison = PVR_DEPTHCMP_ALWAYS;
        context.depth.write = 0;

        context.blend.src = srcBlend;
        context.blend.dst = dstBlend;
                
        auto *header = reinterpret_cast<pvr_poly_hdr_t *>(pvr_dr_target(drState));
        pvr_poly_compile(header, &context);
        pvr_dr_commit(header);

        lastFaceSrcBlend = srcBlend;
        lastFaceDstBlend = dstBlend;
        lastPrimitiveType = PrimitiveTypes_FaceDR;
    }
}


// static
void RenderDevice::PrepareFacePolyDMA(int srcBlend, int dstBlend) {
    if ((lastPrimitiveType != PrimitiveTypes_FaceDMA) || 
        ((lastFaceSrcBlend != srcBlend) || (lastFaceDstBlend != dstBlend)) || 
        reset_facepoly) {
        if (!lastPrimitiveWasConsumed) {
            printf("[pvr] [NG] LAST PRIMITIVE NOT CONSUMED BEFORE CALL TO %s\n", __FUNCTION__);
        }
        lastPrimitiveWasConsumed = false;
        reset_facepoly = 0;
        pvr_poly_cxt_t context;
        pvr_poly_cxt_col(&context, PVR_LIST_TR_POLY);
        context.gen.alpha = 1;
        context.depth.comparison = PVR_DEPTHCMP_ALWAYS;
        context.depth.write = 0;

        context.blend.src = srcBlend;
        context.blend.dst = dstBlend;

        pvr_poly_hdr_t *hdr_ptr = next_dma_hdr;
        pvr_poly_compile(hdr_ptr, &context);
        pvr_vertbuf_written(PVR_LIST_TR_POLY, sizeof(pvr_poly_hdr_t));

        lastFaceSrcBlend = srcBlend;
        lastFaceDstBlend = dstBlend;
        lastPrimitiveType = PrimitiveTypes_FaceDMA;
    }
}

#define SET_FACEPOLY_VERT_DR(_idx, _z, _end)   { \
            vert = reinterpret_cast<pvr_vertex_t *>(pvr_dr_target(drState)); \
            vert->flags = (_end) ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX; \
            if (use_fc) \
                vert->argb = ualpha | faceColor; \
            else \
               vert->argb = ualpha | colors[(_idx)]; \
            vert->x = static_cast<float>(vertices[(_idx)].x) * scale_x; \
            vert->y = static_cast<float>(vertices[(_idx)].y) * scale_y; \
            vert->z = (_z); \
            pvr_dr_commit(vert); \
}

#define SET_FACEPOLY_VERT_DMA(_idx, _z, _end)   { \
            vert->flags = (_end) ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX; \
            if (use_fc) \
                vert->argb = ualpha | faceColor; \
            else \
               vert->argb = ualpha | colors[(_idx)]; \
            vert->x = static_cast<float>(vertices[(_idx)].x) * scale_x; \
            vert->y = static_cast<float>(vertices[(_idx)].y) * scale_y; \
            vert++->z = (_z); \
}

// static
void RenderDevice::DrawFacePolyDR(
        Vector2 *vertices, int32 vertCount, int32 faceColor, int32 alpha, uint32 *colors
) {
    if (lastPrimitiveType != PrimitiveTypes_FaceDR) {
        printf("[pvr] [NG] ATTEMPTED TO DRAW FaceDR BEFORE PREPPING!\n");
        return;
    }

    lastPrimitiveWasConsumed = true;

    pvr_vertex_t *vert;
    const float z = GetDepth();
    const float scale_x = (pixelScaleX / 65535.0f);
    const float scale_y = (pixelScaleY / 65535.0f);
    int use_fc = (colors == NULL);
    uint32 ualpha = (uint32)alpha;
    if (ualpha < 64) ualpha = 64;
    ualpha <<= 24;


    if (vertCount == 3) {
        SET_FACEPOLY_VERT_DR(0, z, 0);
        SET_FACEPOLY_VERT_DR(2, z, 0);
        SET_FACEPOLY_VERT_DR(1, z, 1);
    } else { // if (vertCount == 4) {
        SET_FACEPOLY_VERT_DR(0, z, 0);
        SET_FACEPOLY_VERT_DR(1, z, 0);
        SET_FACEPOLY_VERT_DR(3, z, 0);
        SET_FACEPOLY_VERT_DR(2, z, 1);
    }
}

// static
void RenderDevice::DrawFacePolyDMA(
        Vector2 *vertices, int32 vertCount, int32 faceColor, int32 alpha, uint32 *colors
) {
    if (lastPrimitiveType != PrimitiveTypes_FaceDMA) {
        printf("[pvr] [NG] ATTEMPTED TO DRAW FaceDMA BEFORE PREPPING!\n");
        return;
    }

    lastPrimitiveWasConsumed = true;
    
    const float z = GetDepth();
    const float scale_x = (pixelScaleX / 65535.0f);
    const float scale_y = (pixelScaleY / 65535.0f);
    int use_fc = (colors == NULL);
    uint32 ualpha = (uint32)alpha;
    if (ualpha < 64) ualpha = 64;
    ualpha <<= 24;
    if (!doing_tint) {
        pvr_vertex_t *vert = next_dma_vert;
        if (vertCount == 3) {
            SET_FACEPOLY_VERT_DMA(0, z, 0);
            SET_FACEPOLY_VERT_DMA(2, z, 0);
            SET_FACEPOLY_VERT_DMA(1, z, 1);
        } else { // if (vertCount == 4) {
            SET_FACEPOLY_VERT_DMA(0, z, 0);
            SET_FACEPOLY_VERT_DMA(1, z, 0);
            SET_FACEPOLY_VERT_DMA(3, z, 0);
            SET_FACEPOLY_VERT_DMA(2, z, 1);
        }

        pvr_vertbuf_written(PVR_LIST_TR_POLY, vertCount * sizeof(pvr_vertex_t));
    } else {
        pvr_poly_cxt_t context;
        pvr_poly_cxt_col(&context, PVR_LIST_TR_POLY);
        context.gen.alpha = 1;
        context.depth.comparison = PVR_DEPTHCMP_ALWAYS;
        context.depth.write = 0;

        context.blend.src = PVR_BLEND_INVDESTCOLOR;
        context.blend.dst = PVR_BLEND_ZERO;
        
        pvr_poly_hdr_t *hdr_ptr = next_dma_hdr;
        pvr_poly_compile(hdr_ptr, &context);
        pvr_vertbuf_written(PVR_LIST_TR_POLY, sizeof(pvr_poly_hdr_t));
        pvr_vertex_t *vert = next_dma_vert;
        use_fc = 1;
        faceColor = 0x00ffffff;
        ualpha = 0xff000000;

        if (vertCount == 3) {
            SET_FACEPOLY_VERT_DMA(0, z, 0);
            SET_FACEPOLY_VERT_DMA(2, z, 0);
            SET_FACEPOLY_VERT_DMA(1, z, 1);
        } else { // if (vertCount == 4) {
            SET_FACEPOLY_VERT_DMA(0, z, 0);
            SET_FACEPOLY_VERT_DMA(1, z, 0);
            SET_FACEPOLY_VERT_DMA(3, z, 0);
            SET_FACEPOLY_VERT_DMA(2, z, 1);
        }

        pvr_vertbuf_written(PVR_LIST_TR_POLY, vertCount * sizeof(pvr_vertex_t));
        doing_tint = 0;
        reset_facepoly = 1;
    }
}