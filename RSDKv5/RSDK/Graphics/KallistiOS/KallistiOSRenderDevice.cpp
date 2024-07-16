#include <kos.h>
#include <dc/pvr.h>

#include <RSDK/Core/Stub.hpp>

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
        1
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

        memset(&screens[s].frameBuffer, 0, sizeof(screens[s].frameBuffer));
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
    // Render idle time as a red bar.
    vid_border_color(255, 0, 0);
    pvr_wait_ready();
    // Render scene time as a green bar.
    vid_border_color(0, 255, 0);
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

    const float scaleX = viewSize.x / pixelSize.x;
    const float scaleY = viewSize.y / pixelSize.y;

    const auto surfaceWidth = static_cast<float>(surface->width);
    const auto surfaceHeight = static_cast<float>(surface->height);

    const float x0 = static_cast<float>(x) * scaleX;
    const float x1 = x0 + (static_cast<float>(width) * scaleX);
    const float y0 = static_cast<float>(y) * scaleY;
    const float y1 = y0 + static_cast<float>(height) * scaleY;

    const float u0 = static_cast<float>(sprX0) / surfaceWidth;
    const float u1 = static_cast<float>(sprX1) / surfaceWidth;

    const float v0 = static_cast<float>(sprY0) / surfaceHeight;
    const float v1 = static_cast<float>(sprY1) / surfaceHeight;

    auto *vert = reinterpret_cast<pvr_sprite_txr_t *>(pvr_dr_target(drState));

    vert->flags = PVR_CMD_VERTEX_EOL;
    vert->ax = x0;
    vert->ay = y0;
    vert->az = GetDepth();

    vert->bx = x1;
    vert->by = y0;
    vert->bz = vert->az;

    vert->cx = x1;
    pvr_dr_commit(vert);
    vert = reinterpret_cast<pvr_sprite_txr_t *>(pvr_dr_target(drState));

     *((float*)&vert->flags) = y1;
    vert->ax = GetDepth();

    vert->ay = x0;
    vert->az = y1;

    *((uint32_t*)&vert->by) = PVR_PACK_16BIT_UV(u0, v0);
    *((uint32_t*)&vert->bz) = PVR_PACK_16BIT_UV(u1, v0);
    *((uint32_t*)&vert->cx) = PVR_PACK_16BIT_UV(u1, v1);

    pvr_dr_commit(reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(vert)));
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

    const float scaleX = viewSize.x / pixelSize.x;
    const float scaleY = viewSize.y / pixelSize.y;

    const auto surfaceWidth = static_cast<float>(surface->width);
    const auto surfaceHeight = static_cast<float>(surface->height);

    const float depth = GetDepth();

    vec3f p0 {
        .x = static_cast<float>(x) * scaleX,
        .y = static_cast<float>(y) * scaleY,
        .z = depth
    };

    vec3f p3 {
        .x = p0.x + (static_cast<float>(width) * scaleX),
        .y = p0.y + (static_cast<float>(height) * scaleY),
        .z = depth
    };

    vec3f p1 {
        .x = p3.x,
        .y = p0.y,
        .z = depth
    };

    vec3f p2 {
        .x = p0.x,
        .y = p3.y,
        .z = depth
    };

    if (rotation != 0) {
        const float cx = static_cast<float>(ox) * scaleX;
        const float cy = static_cast<float>(oy) * scaleY;

        const float radians = static_cast<float>(rotation) * static_cast<float>(M_TWOPI) / 512.0f;
        const float s = sinf(radians);
        const float c = cosf(radians);

        auto rotate = [cx, cy, s, c](vec3f& p) 
        /* Allowing this to get inlined causes an ICE in SH-GCC14.
           Remove __noinline once it's fixed, becaue it's bullshit. */
#if __GNUC__ >= 14
        __noinline
#endif 
        {
            p.x -= cx;
            p.y -= cy;

            const auto px = p.x * c - p.y * s;
            const auto py = p.x * s + p.y * c;

            p.x = px + cx;
            p.y = py + cy;
        };

        rotate(p0);
        rotate(p1);
        rotate(p2);
        rotate(p3);
    }

    const float u0 = static_cast<float>(sprX0) / surfaceWidth;
    const float u1 = static_cast<float>(sprX1) / surfaceWidth;

    const float v0 = static_cast<float>(sprY0) / surfaceHeight;
    const float v1 = static_cast<float>(sprY1) / surfaceHeight;

    {
        auto *vert = reinterpret_cast<pvr_vertex_t *>(pvr_dr_target(drState));

        vert->flags = PVR_CMD_VERTEX;
        vert->argb = 0x00ffffff | (alpha << 24);
        vert->oargb = 0;
        vert->z = depth;

        // top left
        vert->x = p0.x;
        vert->y = p0.y;
        vert->u = u0;
        vert->v = v0;

        pvr_dr_commit(vert);
        vert = reinterpret_cast<pvr_vertex_t *>(pvr_dr_target(drState));

        // top right
        vert->flags = PVR_CMD_VERTEX;
        vert->argb = 0x00ffffff | (alpha << 24);
        vert->oargb = 0;
        vert->z = depth;
        vert->x = p1.x;
        vert->y = p1.y;
        vert->u = u1;
        vert->v = v0;

        pvr_dr_commit(vert);
        vert = reinterpret_cast<pvr_vertex_t *>(pvr_dr_target(drState));

        // bottom left
        vert->flags = PVR_CMD_VERTEX;
        vert->argb = 0x00ffffff | (alpha << 24);
        vert->oargb = 0;
        vert->z = depth;
        vert->x = p2.x;
        vert->y = p2.y;
        vert->u = u0;
        vert->v = v1;

        pvr_dr_commit(vert);
        vert = reinterpret_cast<pvr_vertex_t *>(pvr_dr_target(drState));

        // bottom right
        vert->flags = PVR_CMD_VERTEX_EOL;
        vert->argb = 0x00ffffff | (alpha << 24);
        vert->oargb = 0;
        vert->z = depth;
        vert->x = p3.x;
        vert->y = p3.y;
        vert->u = u1;
        vert->v = v1;
        pvr_dr_commit(vert);
    }
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

    const float scaleX = viewSize.x / pixelSize.x;
    const float scaleY = viewSize.y / pixelSize.y;

    const float renderX = static_cast<float>(x) * scaleX;
    const float renderY = static_cast<float>(y) * scaleY;
    const float lmaoWidth = static_cast<float>(width) * scaleX;
    const float lmaoHeight = static_cast<float>(height) * scaleY;

    {
        auto* vert = reinterpret_cast<pvr_vertex_t *>(pvr_dr_target(drState));

        vert->flags = PVR_CMD_VERTEX;
        vert->argb = color;
        vert->oargb = 0;
        vert->z = GetDepth();

        // top left
        vert->x = renderX;
        vert->y = renderY;
        pvr_dr_commit(vert);

        vert = reinterpret_cast<pvr_vertex_t *>(pvr_dr_target(drState));
        // top right
        vert->flags = PVR_CMD_VERTEX;
        vert->argb = color;
        vert->oargb = 0;
        vert->z = GetDepth();
        vert->x = renderX + lmaoWidth;
        vert->y = renderY;

        pvr_dr_commit(vert);

        vert = reinterpret_cast<pvr_vertex_t *>(pvr_dr_target(drState));
        // bottom left
        vert->flags = PVR_CMD_VERTEX;
        vert->argb = color;
        vert->oargb = 0;
        vert->z = GetDepth();
        vert->x = renderX;
        vert->y = renderY + lmaoHeight;
        pvr_dr_commit(vert);

        vert = reinterpret_cast<pvr_vertex_t *>(pvr_dr_target(drState));
        // bottom right
        vert->flags = PVR_CMD_VERTEX_EOL;
        vert->argb = color;
        vert->oargb = 0;
        vert->z = GetDepth();
        vert->x = renderX + lmaoWidth;
        vert->y = renderY + lmaoHeight;

        pvr_dr_commit(vert);
    }
}
