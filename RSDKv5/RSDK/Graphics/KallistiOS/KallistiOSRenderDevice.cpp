#include <kos.h>
#include <dc/pvr.h>

#include <RSDK/Core/Stub.hpp>

struct KOSTexture
{
    pvr_poly_cxt_t context;
    pvr_poly_hdr_t header;
    pvr_ptr_t pvrPtr;
    uint16* sysPtr;
    uint32 width;
    uint32 height;
};

static KOSTexture screenTextures[SCREEN_COUNT] {};

void draw_one_textured_poly(const KOSTexture& kost) {
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
    vert.u = 1.0f;
    vert.v = 0.0f;
    pvr_prim(&vert, sizeof(vert));

    // bottom left
    vert.x = 0.0f;
    vert.y = 480.0f;
    vert.z = 1.0f;
    vert.u = 0.0f;
    vert.v = 1.0f;
    pvr_prim(&vert, sizeof(vert));

    vert.flags = PVR_CMD_VERTEX_EOL;
    vert.x = 640.0f;
    vert.y = 480.0f;
    vert.z = 1.0f;
    vert.u = 1.0f;
    vert.v = 1.0f;
    pvr_prim(&vert, sizeof(vert));
}

// static
bool RenderDevice::Init()
{
    pvr_init_params_t pvrParams = {
        { PVR_BINSIZE_16, PVR_BINSIZE_0, PVR_BINSIZE_0, PVR_BINSIZE_0, PVR_BINSIZE_0 },
        64 * 1024
    };

    if (pvr_init(&pvrParams) < 0) {
        while (true) {
            printf("pvr_init failed!!!\n");
        }
    }

    pvr_set_bg_color(0.0f, 1.0f, 1.0f);

    videoSettings.windowed = false;
    videoSettings.windowState = WINDOWSTATE_ACTIVE;
    videoSettings.shaderSupport = false;
    
    uint32 width;
    uint32 height;

    if (videoSettings.pixHeight <= 256){
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

    screenTextures[0].sysPtr = (uint16*)memalign(32, 2 * width * height);

    if (!screenTextures[0].sysPtr) {
        while (true) {
            printf("memalign() failed!!!\n");
        }
    }

    pvr_poly_cxt_txr(&screenTextures[0].context,
                     PVR_LIST_OP_POLY,
                     PVR_TXRFMT_RGB565 | PVR_TXRFMT_NONTWIDDLED,
                     (int)width,
                     (int)height,
                     screenTextures[0].pvrPtr,
                     PVR_FILTER_NONE);

    pvr_poly_compile(&screenTextures[0].header, &screenTextures[0].context);

    screenTextures[0].width = width;
    screenTextures[0].height = height;

    SetScreenSize(0, (uint16)width, (uint16)height);

    engine.inFocus = 1;
    InitInputDevices();
    return true;
}

// static
void RenderDevice::CopyFrameBuffer()
{
    for (int32 s = 0; s < /* DCFIXME: videoSettings.screenCount*/ 1; ++s) {
        const KOSTexture& screenTexture = screenTextures[s];

        uint16* pixels = (uint16*)screenTexture.sysPtr;
        uint16* frameBuffer = screens[s].frameBuffer;
        uint32 pitch = screenTexture.width;

        for (int32 y = 0; y < SCREEN_YSIZE; ++y) {
            memcpy(pixels, frameBuffer, screens[s].size.x * sizeof(uint16));
            frameBuffer += screens[s].pitch;
            pixels += pitch;
        }

        pvr_txr_load(screenTexture.sysPtr,
                     screenTexture.pvrPtr,
                     screenTexture.width * screenTexture.height * 2);
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
                draw_one_textured_poly(screenTextures[0]);
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