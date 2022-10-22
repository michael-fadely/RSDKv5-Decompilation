#include <kos.h>
#include <dc/pvr.h>

#include <RSDK/Core/Stub.hpp>

struct KOSTexture
{
    pvr_poly_cxt_t context;
    pvr_poly_hdr_t header;
    pvr_ptr_t pvrPtr;
    void* sysPtr;
    uint32 width;
    uint32 height;
};

static KOSTexture screenTextures[SCREEN_COUNT] {};

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

    vert.flags = PVR_CMD_VERTEX_EOL;
    vert.x = 640.0f;
    vert.y = 480.0f;
    vert.z = 1.0f;
    vert.u = static_cast<float>(screenSize.x) / static_cast<float>(kost.width);
    vert.v = static_cast<float>(screenSize.y) / static_cast<float>(kost.height);
    pvr_prim(&vert, sizeof(vert));
}

// static
bool RenderDevice::Init()
{
    pvr_init_params_t pvrParams = {
        { PVR_BINSIZE_16, PVR_BINSIZE_0, PVR_BINSIZE_0, PVR_BINSIZE_0, PVR_BINSIZE_0 },
        1024
    };

    // DCFIXME: these init parameters don't actually work on hardware. initialization succeeds, but rendering fails.
//    if (pvr_init(&pvrParams) < 0) {
    if (pvr_init_defaults() < 0) {
        while (true) {
            printf("pvr_init failed!!!\n");
        }
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

    // DCFIXME: support multiple screen textures? (presumably multiplayer only)

    screenTextures[0].pvrPtr = pvr_mem_malloc(2 * width * height);

    if (!screenTextures[0].pvrPtr) {
        while (true) {
            printf("pvr_mem_malloc() failed!!!\n");
        }
    }

    screenTextures[0].sysPtr = memalign(32, 2 * width * height);

    if (!screenTextures[0].sysPtr) {
        while (true) {
            printf("memalign() failed!!!\n");
        }
    }

    printf("pvr_mem_available: %lu\n", pvr_mem_available());

    pvr_poly_cxt_txr(&screenTextures[0].context,
                     PVR_LIST_OP_POLY,
                     PVR_TXRFMT_RGB565 | PVR_TXRFMT_NONTWIDDLED,
                     (int)width,
                     (int)height,
                     screenTextures[0].pvrPtr,
                     PVR_FILTER_NEAREST);

    pvr_poly_compile(&screenTextures[0].header, &screenTextures[0].context);

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
}
// static
void RenderDevice::FlipScreen()
{
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
}
// static
void RenderDevice::GetWindowSize(int32 *width, int32 *height)
{
    DC_STUB();
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