#include <kos.h>
#include <dc/pvr.h>

#include <RSDK/Core/Stub.hpp>
#include <RSDK/Core/Math.hpp>

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
enum PrimitiveTypes {
    PrimitiveTypes_None,
    PrimitiveTypes_TexturedQuad,
    PrimitiveTypes_TexturedPoly,
    PrimitiveTypes_ColoredPoly,
};

float drawDepth = 1.0f;
pvr_ptr_t lastTexture = nullptr;
pvr_dr_state_t drState = 0;
int lastSrcBlend = -1;
int lastDstBlend = -1;
PrimitiveTypes lastPrimitiveType = PrimitiveTypes_None;
bool lastPrimitiveWasConsumed = true;

void ResetLastState() {
    lastTexture = nullptr;
    lastSrcBlend = -1;
    lastDstBlend = -1;
    lastPrimitiveType = PrimitiveTypes_None;
    // DCWIP: does resetting lastPrimitiveWasConsumed make sense???
    lastPrimitiveWasConsumed = true;
}

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
            PVR_BINSIZE_0  // punch-through polygons
        },

        // vertex buffer size
        // 512 KB is the default used by pvr_init_defaults(). might need adjusting.
        128 * 1024,

        // dma enabled? (no)
        0,

        // fsaa enabled? (no)
        0,

        // autosort disabled?
        1,

        // Overflow buffer count
        // gained 75 KiB by disabling
        0,

        // Disable vertex double-buffering
        0
    };
#else
#error pvr_init_params_t needs re-configuring for the software renderer!
#endif

    if (pvr_init(&pvrParams) < 0) {
        while (true) {
            printf("pvr_init failed!!!\n");
        }
    }
    else {
        printf("pvr_init success. pvr_mem_available: %lu\n", pvr_mem_available());
    }

    pvr_set_bg_color(0.0f, 1.0f, 1.0f);

    videoSettings.windowed = false;
    videoSettings.windowState = WINDOWSTATE_ACTIVE;
    videoSettings.shaderSupport = false;

    displayCount = 1;
    displayWidth[0] = 640;
    displayHeight[0] = 480;
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
    vid_border_color(0, 255, 0);
    pvr_wait_ready();
    // Render scene submission time as a red bar
    vid_border_color(255, 0, 0);
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

    // Update our cached values for pixel global pixel scaling.
    pixelScaleX = viewSize.x / pixelSize.x;
    pixelScaleY = viewSize.y / pixelSize.y;

    pvr_scene_begin();
    // DCWIP: rendering everything as transparent
    if (pvr_list_begin(PVR_LIST_TR_POLY) == -1) {
        printf("[pvr] [NG] pvr_list_begin(PVR_LIST_TR_POLY) returned -1 (%s:%zu -> %s)\n", __FILE__, static_cast<size_t>(__LINE__), __PRETTY_FUNCTION__);
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
    vid_border_color(0, 0, 255);
#endif
}

// static
float RenderDevice::GetDepth() {
#if defined(KOS_HARDWARE_RENDERER)
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
        default:
            return false;

        case INK_NONE:
            if (srcBlend) {
                *srcBlend = PVR_BLEND_SRCALPHA;
            }

            if (dstBlend) {
                *dstBlend = PVR_BLEND_INVSRCALPHA;
            }

            return true;

        case INK_BLEND:
            //printf("[pvr] WARNING: unsupported ink effect INK_BLEND; skipping sprite\n");
            return false;

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
            printf("[pvr] WARNING: unsupported ink effect INK_SUB; skipping sprite\n");
            return false;

        case INK_TINT:
            printf("[pvr] WARNING: unsupported ink effect INK_TINT; skipping sprite\n");
            return false;

        case INK_MASKED:
            //printf("[pvr] WARNING: unsupported ink effect INK_MASKED; skipping sprite\n");
            return false;

        case INK_UNMASKED:
            printf("[pvr] WARNING: unsupported ink effect INK_UNMASKED; skipping sprite\n");
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

// static
void RenderDevice::PrepareTexturedQuad(int32 y, const GFXSurface* surface) {
    const uint32 gamePaletteBankIndex = GetGamePaletteBankIndex(y);
    const uint32 pvrPaletteBankIndex = GameToPvrPaletteBankIndex(gamePaletteBankIndex);

    if (PreparePrimitive(PrimitiveTypes_TexturedQuad,
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

        // Compile polygon header directly into SQ to avoid having to copy it.
        auto *header = reinterpret_cast<pvr_sprite_hdr_t *>(pvr_dr_target(drState));
        pvr_sprite_compile(header, &context);
        pvr_dr_commit(header);
    }
}

// static
void RenderDevice::DrawTexturedQuad(
        int32 x, int32 y,
        int32 width, int32 height,
        int32 sprX0, int32 sprX1,
        int32 sprY0, int32 sprY1,
        const GFXSurface* surface
) {
    if (lastPrimitiveType != PrimitiveTypes_TexturedQuad) {
        printf("[pvr] [NG] ATTEMPTED TO DRAW TexturedQuad BEFORE PREPPING!\n");
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
void RenderDevice::PrepareTexturedPoly(int32 y, int srcBlend, int dstBlend, const GFXSurface *surface) {
    const uint32 gamePaletteBankIndex = GetGamePaletteBankIndex(y);
    const uint32 pvrPaletteBankIndex = GameToPvrPaletteBankIndex(gamePaletteBankIndex);

    if (PreparePrimitive(PrimitiveTypes_TexturedPoly,
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
        pvr_poly_cxt_txr(
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

        context.blend.src = srcBlend;
        context.blend.dst = dstBlend;

        // Compile polygon header directly into SQ to avoid having to copy it.
        auto *header = reinterpret_cast<pvr_poly_hdr_t *>(pvr_dr_target(drState));
        pvr_poly_compile(header, &context);
        pvr_dr_commit(header);
    }
}

// static
void RenderDevice::DrawTexturedPoly(
        int32 x, int32 y,
        int32 ox, int32 oy,
        int32 width, int32 height,
        int32 sprX0, int32 sprX1,
        int32 sprY0, int32 sprY1,
        int32 rotation,
        int32 alpha,
        const GFXSurface *surface
) {
    if (lastPrimitiveType != PrimitiveTypes_TexturedPoly) {
        printf("[pvr] [NG] ATTEMPTED TO DRAW TexturedPoly BEFORE PREPPING!\n");
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
            .argb = 0x00ffffffu | (alpha << 24)
        },
        {
            .flags = PVR_CMD_VERTEX,
            .x = x1,
            .y = y0,
            .z = z,
            .u = u1,
            .v = v0,
            .argb = 0x00ffffffu | (alpha << 24)
        },
        {
            .flags = PVR_CMD_VERTEX,
            .x = x0,
            .y = y1,
            .z = z,
            .u = u0,
            .v = v1,
            .argb = 0x00ffffffu | (alpha << 24)
        },
        {
            .flags = PVR_CMD_VERTEX_EOL,
            .x = x1,
            .y = y1,
            .z = z,
            .u = u1,
            .v = v1,
            .argb = 0x00ffffffu | (alpha << 24)
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

    /* This is the routine called by `pvr_prim()` under-the-hood when
       the current list does not use DMA transfers and PVR DR mode is
       enabled. We can call this directly to shave off a few cycles. */
    sq_fast_cpy(SQ_MASK_DEST(PVR_TA_INPUT), verts, 4);
}

// static
void RenderDevice::PrepareColoredPoly(int32 y, int srcBlend, int dstBlend) {
    const uint32 gamePaletteBankIndex = GetGamePaletteBankIndex(y);
    const uint32 pvrPaletteBankIndex = GameToPvrPaletteBankIndex(gamePaletteBankIndex);

    if (PreparePrimitive(PrimitiveTypes_ColoredPoly,
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
        pvr_poly_cxt_col(&context, PVR_LIST_TR_POLY);

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

// static
void RenderDevice::DrawColoredPoly(
        int32 x, int32 y,
        int32 width, int32 height,
        uint32 color
) {
    if (lastPrimitiveType != PrimitiveTypes_ColoredPoly) {
        printf("[pvr] [NG] ATTEMPTED TO DRAW ColoredPoly BEFORE PREPPING!\n");
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
    //vert->flags = PVR_CMD_VERTEX;
    //vert->argb = color;
    vert->x = x0;
    vert->y = y1;
    //vert->z = z;
    pvr_dr_commit(vert);

    // bottom right
    vert = reinterpret_cast<pvr_vertex_t *>(pvr_dr_target(drState));
    vert->flags = PVR_CMD_VERTEX_EOL;
    //vert->argb = color;
    vert->x = x1;
    vert->y = y1;
    //vert->z = z;
    pvr_dr_commit(vert);
}
