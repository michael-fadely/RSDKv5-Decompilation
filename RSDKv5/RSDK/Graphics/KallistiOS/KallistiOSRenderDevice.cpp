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
static float drawDepth = 1.0f;
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
    vert.x = 640.0f;
    vert.y = 0.0f;
    vert.z = 1.0f;
    vert.u = static_cast<float>(screenSize.x) / static_cast<float>(kost.width);
    vert.v = 0.0f;
    pvr_prim(&vert, sizeof(vert));

    // bottom left
    vert.x = 0.0f;
    vert.y = 480.0f;
    vert.z = 1.0f;
    vert.u = 0.0f;
    vert.v = static_cast<float>(screenSize.y) / static_cast<float>(kost.height);
    pvr_prim(&vert, sizeof(vert));

    // bottom right
    vert.flags = PVR_CMD_VERTEX_EOL;
    vert.x = 640.0f;
    vert.y = 480.0f;
    vert.z = 1.0f;
    vert.u = static_cast<float>(screenSize.x) / static_cast<float>(kost.width);
    vert.v = static_cast<float>(screenSize.y) / static_cast<float>(kost.height);
    pvr_prim(&vert, sizeof(vert));
}
#endif

// static
bool RenderDevice::Init()
{
    pvr_init_params_t pvrParams = {
        // bin sizes
        {
            PVR_BINSIZE_0, // opaque polygons
            PVR_BINSIZE_0, // opaque modifiers (disabled)
            PVR_BINSIZE_16, // translucent polygons
            PVR_BINSIZE_0, // translucent modifiers (disabled)
            PVR_BINSIZE_0  // punch-through polygons
        },

        // vertex buffer size
        // 512 KB is the default used by pvr_init_defaults(). might need adjusting.
        128 * 1024, // UNDONE: was 512 * 1024

        // dma enabled? (no)
        0,

        // fsaa enabled? (no)
        0,

        // autosort disabled?
        0
    };

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

    int32 bufferWidth  = videoSettings.fsWidth;
    int32 bufferHeight = videoSettings.fsHeight;
    if (videoSettings.fsWidth <= 0 || videoSettings.fsHeight <= 0) {
        bufferWidth  = displayWidth[0];
        bufferHeight = displayHeight[0];
    }

    viewSize.x = bufferWidth;
    viewSize.y = bufferHeight;

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

    viewSize.x = 640.0f;
    viewSize.y = 480.0f;

    screenTextures[0].width = width;
    screenTextures[0].height = height;

    int32 maxPixHeight = 0;
    for (int32 s = 0; s < SCREEN_COUNT; ++s) {
        if (videoSettings.pixHeight > maxPixHeight)
            maxPixHeight = videoSettings.pixHeight;

        screens[s].size.y = videoSettings.pixHeight;

        float viewAspect  = viewSize.x / viewSize.y;
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

    pixelSize.x = screens[0].size.x;
    pixelSize.y = screens[0].size.y;

    engine.inFocus = 1;

    videoSettings.windowState = WINDOWSTATE_ACTIVE;
    videoSettings.dimMax      = 1.0;
    videoSettings.dimPercent  = 1.0;

    int32 size = videoSettings.pixWidth >= SCREEN_YSIZE ? videoSettings.pixWidth : SCREEN_YSIZE;
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
bool RenderDevice::CheckFPSCap()
{
    pvr_wait_ready();
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

    pvr_scene_begin();
    pvr_list_begin(PVR_LIST_TR_POLY); // DCWIP
#endif
}

// static
void RenderDevice::EndScene() {
#if defined(KOS_HARDWARE_RENDERER)
    pvr_list_finish(); // DCWIP
    pvr_scene_finish();
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
    const uint8* lineBuffer  = &gfxLineBuffer[CLAMP(y, 0, 239)]; // DCWIP: hard-coded height
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

    uint16 *activePalette = fullPalette[gamePaletteBankIndex];

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

enum PrimitiveTypes {
    PrimitiveTypes_None,
    PrimitiveTypes_TexturedQuad,
    PrimitiveTypes_TexturedPoly,
    PrimitiveTypes_ColoredPoly,
};

static pvr_ptr_t lastTexture = nullptr;
static int lastSrcBlend = -1;
static int lastDstBlend = -1;
static PrimitiveTypes lastPrimitiveType = PrimitiveTypes_None;

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
        lastPrimitiveType = (PrimitiveTypes)primitiveType;
        PaletteFlags::SetBank(gamePaletteBankIndex);
        lastSrcBlend = srcBlend;
        lastDstBlend = dstBlend;
        lastTexture = texture;

        return true;
    }

    // DCFIXME: this should just return false, but sprites were breaking the debug menu render
    return primitiveType != PrimitiveTypes_TexturedQuad;
}

// static
void RenderDevice::PrepareTexturedQuad(int32 y, GFXSurface* surface) {
    const uint32 gamePaletteBankIndex = GetGamePaletteBankIndex(y);
    const uint32 pvrPaletteBankIndex = GameToPvrPaletteBankIndex(gamePaletteBankIndex);

    if (PreparePrimitive(PrimitiveTypes_TexturedQuad,
                         gamePaletteBankIndex,
                         pvrPaletteBankIndex,
                         PVR_BLEND_SRCALPHA,
                         PVR_BLEND_INVSRCALPHA,
                         surface->texture)) {
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

        pvr_sprite_hdr_t header;
        pvr_sprite_compile(&header, &context);

        pvr_prim(&header, sizeof(header));
    }
}

// static
void RenderDevice::DrawTexturedQuad(
        int32 x, int32 y,
        int32 width, int32 height,
        int32 sprX0, int32 sprX1,
        int32 sprY0, int32 sprY1,
        GFXSurface* surface
) {
    constexpr float renderWidth  = 640.0f; // DCWIP: hard-coded render dimensions used
    constexpr float renderHeight = 480.0f; // DCWIP: hard-coded render dimensions used
    constexpr float screenWidth  = 320.0f; // DCWIP: hard-coded render dimensions used
    constexpr float screenHeight = 240.0f; // DCWIP: hard-coded render dimensions used

    constexpr float scaleX = renderWidth / screenWidth;
    constexpr float scaleY = renderHeight / screenHeight;

    const auto surfaceWidth = static_cast<float>(surface->width);
    const auto surfaceHeight = static_cast<float>(surface->height);

    const float x0 = static_cast<float>(x) * scaleX;
    const float x1 = x0 + (static_cast<float>(width) * scaleX);
    const float y0 = static_cast<float>(y) * scaleY;
    const float y1 = y0 + static_cast<float>(height) * scaleY;

    float u0 = static_cast<float>(sprX0) / surfaceWidth;
    float u1 = static_cast<float>(sprX1) / surfaceWidth;

    float v0 = static_cast<float>(sprY0) / surfaceHeight;
    float v1 = static_cast<float>(sprY1) / surfaceHeight;

    pvr_sprite_txr_t vert {};

    vert.flags = PVR_CMD_VERTEX_EOL;
    vert.ax = x0;
    vert.ay = y0;
    vert.az = GetDepth();

    vert.bx = x1;
    vert.by = y0;
    vert.bz = vert.az;

    vert.cx = x1;
    vert.cy = y1;
    vert.cz = vert.az;

    vert.dx = x0;
    vert.dy = y1;
    vert.auv = PVR_PACK_16BIT_UV(u0, v0);
    vert.buv = PVR_PACK_16BIT_UV(u1, v0);
    vert.cuv = PVR_PACK_16BIT_UV(u1, v1);

    pvr_prim(&vert, sizeof(vert));
}

// static
void RenderDevice::PrepareTexturedPoly(int32 y, int srcBlend, int dstBlend, RSDK::GFXSurface *surface) {
    const uint32 gamePaletteBankIndex = GetGamePaletteBankIndex(y);
    const uint32 pvrPaletteBankIndex = GameToPvrPaletteBankIndex(gamePaletteBankIndex);

    if (PreparePrimitive(PrimitiveTypes_TexturedPoly,
                         gamePaletteBankIndex,
                         pvrPaletteBankIndex,
                         srcBlend,
                         dstBlend,
                         surface->texture)) {
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

        context.blend.src = srcBlend;
        context.blend.dst = dstBlend;

        pvr_poly_hdr_t header;
        pvr_poly_compile(&header, &context);

        pvr_prim(&header, sizeof(header));
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
        GFXSurface *surface
) {
    constexpr float renderWidth  = 640.0f; // DCWIP: hard-coded render dimensions used
    constexpr float renderHeight = 480.0f; // DCWIP: hard-coded render dimensions used
    constexpr float screenWidth  = 320.0f; // DCWIP: hard-coded render dimensions used
    constexpr float screenHeight = 240.0f; // DCWIP: hard-coded render dimensions used

    constexpr float scaleX = renderWidth / screenWidth;
    constexpr float scaleY = renderHeight / screenHeight;

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
        .y = p0.y + static_cast<float>(height) * scaleY,
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

    if (rotation) {
        const float cx = static_cast<float>(ox) * scaleX;
        const float cy = static_cast<float>(oy) * scaleY;

        const float radians = static_cast<float>(rotation) * (float)M_TWOPI / 512.0f;

        const float s = sinf(radians);
        const float c = cosf(radians);

        auto rotate = [cx, cy, s, c](vec3f& p) {
            p.x -= cx;
            p.y -= cy;

            auto px = p.x * c - p.y * s;
            auto py = p.x * s + p.y * c;

            p.x = px + cx;
            p.y = py + cy;
        };

        rotate(p0);
        rotate(p1);
        rotate(p2);
        rotate(p3);
    }

    float u0 = static_cast<float>(sprX0) / surfaceWidth;
    float u1 = static_cast<float>(sprX1) / surfaceWidth;

    float v0 = static_cast<float>(sprY0) / surfaceHeight;
    float v1 = static_cast<float>(sprY1) / surfaceHeight;

    {
        pvr_vertex_t vert {};

        vert.flags = PVR_CMD_VERTEX;
        vert.argb = 0x00ffffff | (alpha << 24);
        vert.oargb = 0;
        vert.z = depth;

        // top left
        vert.x = p0.x;
        vert.y = p0.y;
        vert.u = u0;
        vert.v = v0;
        pvr_prim(&vert, sizeof(vert));

        // top right
        vert.x = p1.x;
        vert.y = p1.y;
        vert.u = u1;
        vert.v = v0;
        pvr_prim(&vert, sizeof(vert));

        // bottom left
        vert.x = p2.x;
        vert.y = p2.y;
        vert.u = u0;
        vert.v = v1;
        pvr_prim(&vert, sizeof(vert));

        // bottom right
        vert.flags = PVR_CMD_VERTEX_EOL;
        vert.x = p3.x;
        vert.y = p3.y;
        vert.u = u1;
        vert.v = v1;
        pvr_prim(&vert, sizeof(vert));
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
        pvr_poly_cxt_t context;
        pvr_poly_cxt_col(&context, PVR_LIST_TR_POLY);

        context.blend.src = srcBlend;
        context.blend.dst = dstBlend;

        pvr_poly_hdr_t header;
        pvr_poly_compile(&header, &context);

        pvr_prim(&header, sizeof(header));
    }
}

// static
void RenderDevice::DrawColoredPoly(
        int32 x, int32 y,
        int32 width, int32 height,
        uint32 color
) {
    constexpr float renderWidth  = 640.0f; // DCWIP: hard-coded render dimensions used
    constexpr float renderHeight = 480.0f; // DCWIP: hard-coded render dimensions used
    constexpr float screenWidth  = 320.0f; // DCWIP: hard-coded render dimensions used
    constexpr float screenHeight = 240.0f; // DCWIP: hard-coded render dimensions used

    constexpr float scaleX = renderWidth / screenWidth;
    constexpr float scaleY = renderHeight / screenHeight;

    const float renderX = static_cast<float>(x) * scaleX;
    const float renderY = static_cast<float>(y) * scaleY;
    const float lmaoWidth = static_cast<float>(width) * scaleX;
    const float lmaoHeight = static_cast<float>(height) * scaleY;

    {
        pvr_vertex_t vert {};

        vert.flags = PVR_CMD_VERTEX;
        vert.argb = color;
        vert.oargb = 0;
        vert.z = GetDepth();

        // top left
        vert.x = renderX;
        vert.y = renderY;
        pvr_prim(&vert, sizeof(vert));

        // top right
        vert.x = renderX + lmaoWidth;
        vert.y = renderY;
        pvr_prim(&vert, sizeof(vert));

        // bottom left
        vert.x = renderX;
        vert.y = renderY + lmaoHeight;
        pvr_prim(&vert, sizeof(vert));

        // bottom right
        vert.flags = PVR_CMD_VERTEX_EOL;
        vert.x = renderX + lmaoWidth;
        vert.y = renderY + lmaoHeight;
        pvr_prim(&vert, sizeof(vert));
    }
}
