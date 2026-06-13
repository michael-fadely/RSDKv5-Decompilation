#include "RSDK/Core/RetroEngine.hpp"

using namespace RSDK;

#if RETRO_REV0U
#include "Legacy/DrawingLegacy.cpp"
#endif

#if RETRO_PLATFORM == RETRO_KALLISTIOS && defined(KOS_HARDWARE_RENDERER)
#include <RSDK/Graphics/KallistiOS/AniTileTracker.hpp>
extern uint8 lastFlashGigaAlpha;
#endif

// all render devices need to access the initial vertex buffer :skull:

// clang-format off
#if RETRO_REV02
const RenderVertex rsdkVertexBuffer[60] = {
    // 1 Screen (0)
    { { -1.0,  1.0,  1.0 }, 0xFFFFFFFF, {  0.0,  0.0 } },
    { { -1.0, -1.0,  1.0 }, 0xFFFFFFFF, {  0.0,  0.9375 } },
    { {  1.0, -1.0,  1.0 }, 0xFFFFFFFF, {  0.625,  0.9375 } },
    { { -1.0,  1.0,  1.0 }, 0xFFFFFFFF, {  0.0,  0.0 } },
    { {  1.0,  1.0,  1.0 }, 0xFFFFFFFF, {  0.625,  0.0 } },
    { {  1.0, -1.0,  1.0 }, 0xFFFFFFFF, {  0.625,  0.9375 } },
    
    // 2 Screens - Bordered (Top Screen) (6)
    { { -0.5,  1.0,  1.0 }, 0xFFFFFFFF, {  0.0,  0.0 } },
    { { -0.5,  0.0,  1.0 }, 0xFFFFFFFF, {  0.0,  0.9375 } },
    { {  0.5,  0.0,  1.0 }, 0xFFFFFFFF, {  0.625,  0.9375 } },
    { { -0.5,  1.0,  1.0 }, 0xFFFFFFFF, {  0.0,  0.0 } },
    { {  0.5,  1.0,  1.0 }, 0xFFFFFFFF, {  0.625,  0.0 } },
    { {  0.5,  0.0,  1.0 }, 0xFFFFFFFF, {  0.625,  0.9375 } },
    
    // 2 Screens - Bordered (Bottom Screen) (12)
    { { -0.5,  0.0,  1.0 }, 0xFFFFFFFF, {  0.0,  0.0 } },
    { { -0.5, -1.0,  1.0 }, 0xFFFFFFFF, {  0.0,  0.9375 } },
    { {  0.5, -1.0,  1.0 }, 0xFFFFFFFF, {  0.625,  0.9375 } },
    { { -0.5,  0.0,  1.0 }, 0xFFFFFFFF, {  0.0,  0.0 } },
    { {  0.5,  0.0,  1.0 }, 0xFFFFFFFF, {  0.625,  0.0 } },
    { {  0.5, -1.0,  1.0 }, 0xFFFFFFFF, {  0.625,  0.9375 } },
    
    // 2 Screens - Stretched (Top Screen)  (18)
    { { -1.0,  1.0,  1.0 }, 0xFFFFFFFF, {  0.0,  0.0 } },
    { { -1.0,  0.0,  1.0 }, 0xFFFFFFFF, {  0.0,  0.9375 } },
    { {  1.0,  0.0,  1.0 }, 0xFFFFFFFF, {  0.625,  0.9375 } },
    { { -1.0,  1.0,  1.0 }, 0xFFFFFFFF, {  0.0,  0.0 } },
    { {  1.0,  1.0,  1.0 }, 0xFFFFFFFF, {  0.625,  0.0 } },
    { {  1.0,  0.0,  1.0 }, 0xFFFFFFFF, {  0.625,  0.9375 } },
    
    // 2 Screens - Stretched (Bottom Screen) (24)
    { { -1.0,  0.0,  1.0 }, 0xFFFFFFFF, {  0.0,  0.0 } },
    { { -1.0, -1.0,  1.0 }, 0xFFFFFFFF, {  0.0,  0.9375 } },
    { {  1.0, -1.0,  1.0 }, 0xFFFFFFFF, {  0.625,  0.9375 } },
    { { -1.0,  0.0,  1.0 }, 0xFFFFFFFF, {  0.0,  0.0 } },
    { {  1.0,  0.0,  1.0 }, 0xFFFFFFFF, {  0.625,  0.0 } },
    { {  1.0, -1.0,  1.0 }, 0xFFFFFFFF, {  0.625,  0.9375 } },
    
    // 4 Screens (Top-Left) (30)
    { { -1.0,  1.0,  1.0 }, 0xFFFFFFFF, {  0.0,  0.0 } },
    { { -1.0,  0.0,  1.0 }, 0xFFFFFFFF, {  0.0,  0.9375 } },
    { {  0.0,  0.0,  1.0 }, 0xFFFFFFFF, {  0.625,  0.9375 } },
    { { -1.0,  1.0,  1.0 }, 0xFFFFFFFF, {  0.0,  0.0 } },
    { {  0.0,  1.0,  1.0 }, 0xFFFFFFFF, {  0.625,  0.0 } },
    { {  0.0,  0.0,  1.0 }, 0xFFFFFFFF, {  0.625,  0.9375 } },
    
    // 4 Screens (Top-Right) (36)
    { {  0.0,  1.0,  1.0 }, 0xFFFFFFFF, {  0.0,  0.0 } },
    { {  0.0,  0.0,  1.0 }, 0xFFFFFFFF, {  0.0,  0.9375 } },
    { {  1.0,  0.0,  1.0 }, 0xFFFFFFFF, {  0.625,  0.9375 } },
    { {  0.0,  1.0,  1.0 }, 0xFFFFFFFF, {  0.0,  0.0 } },
    { {  1.0,  1.0,  1.0 }, 0xFFFFFFFF, {  0.625,  0.0 } },
    { {  1.0,  0.0,  1.0 }, 0xFFFFFFFF, {  0.625,  0.9375 } },
    
    // 4 Screens (Bottom-Right) (42)
    { { -1.0,  0.0,  1.0 }, 0xFFFFFFFF, {  0.0,  0.0 } },
    { { -1.0, -1.0,  1.0 }, 0xFFFFFFFF, {  0.0,  0.9375 } },
    { {  0.0, -1.0,  1.0 }, 0xFFFFFFFF, {  0.625,  0.9375 } },
    { { -1.0,  0.0,  1.0 }, 0xFFFFFFFF, {  0.0,  0.0 } },
    { {  0.0,  0.0,  1.0 }, 0xFFFFFFFF, {  0.625,  0.0 } },
    { {  0.0, -1.0,  1.0 }, 0xFFFFFFFF, {  0.625,  0.9375 } },
    
    // 4 Screens (Bottom-Left) (48)
    { {  0.0,  0.0,  1.0 }, 0xFFFFFFFF, {  0.0,  0.0 } },
    { {  0.0, -1.0,  1.0 }, 0xFFFFFFFF, {  0.0,  0.9375 } },
    { {  1.0, -1.0,  1.0 }, 0xFFFFFFFF, {  0.625,  0.9375 } },
    { {  0.0,  0.0,  1.0 }, 0xFFFFFFFF, {  0.0,  0.0 } },
    { {  1.0,  0.0,  1.0 }, 0xFFFFFFFF, {  0.625,  0.0 } },
    { {  1.0, -1.0,  1.0 }, 0xFFFFFFFF, {  0.625,  0.9375 } },
    
    // Image/Video (54)
    { { -1.0,  1.0,  1.0 }, 0xFFFFFFFF, {  0.0,  0.0 } },
    { { -1.0, -1.0,  1.0 }, 0xFFFFFFFF, {  0.0,  1.0 } },
    { {  1.0, -1.0,  1.0 }, 0xFFFFFFFF, {  1.0,  1.0 } },
    { { -1.0,  1.0,  1.0 }, 0xFFFFFFFF, {  0.0,  0.0 } },
    { {  1.0,  1.0,  1.0 }, 0xFFFFFFFF, {  1.0,  0.0 } },
    { {  1.0, -1.0,  1.0 }, 0xFFFFFFFF, {  1.0,  1.0 } }
};
#else
const RenderVertex rsdkVertexBuffer[24] =
{
    // 1 Screen (0)
    { { -1.0,  1.0,  1.0 }, 0xFFFFFFFF, {  0.0,  0.0 } },
    { { -1.0, -1.0,  1.0 }, 0xFFFFFFFF, {  0.0,  0.9375 } },
    { {  1.0, -1.0,  1.0 }, 0xFFFFFFFF, {  0.625,  0.9375 } },
    { { -1.0,  1.0,  1.0 }, 0xFFFFFFFF, {  0.0,  0.0 } },
    { {  1.0,  1.0,  1.0 }, 0xFFFFFFFF, {  0.625,  0.0 } },
    { {  1.0, -1.0,  1.0 }, 0xFFFFFFFF, {  0.625,  0.9375 } },

  // 2 Screens - Stretched (Top Screen) (6)
    { { -1.0,  1.0,  1.0 }, 0xFFFFFFFF, {  0.0,  0.0 } },
    { { -1.0,  0.0,  1.0 }, 0xFFFFFFFF, {  0.0,  0.9375 } },
    { {  1.0,  0.0,  1.0 }, 0xFFFFFFFF, {  0.625,  0.9375 } },
    { { -1.0,  1.0,  1.0 }, 0xFFFFFFFF, {  0.0,  0.0 } },
    { {  1.0,  1.0,  1.0 }, 0xFFFFFFFF, {  0.625,  0.0 } },
    { {  1.0,  0.0,  1.0 }, 0xFFFFFFFF, {  0.625,  0.9375 } },
  
  // 2 Screens - Stretched (Bottom Screen) (12)
    { { -1.0,  0.0,  1.0 }, 0xFFFFFFFF, {  0.0,  0.0 } },
    { { -1.0, -1.0,  1.0 }, 0xFFFFFFFF, {  0.0,  0.9375 } },
    { {  1.0, -1.0,  1.0 }, 0xFFFFFFFF, {  0.625,  0.9375 } },
    { { -1.0,  0.0,  1.0 }, 0xFFFFFFFF, {  0.0,  0.0 } },
    { {  1.0,  0.0,  1.0 }, 0xFFFFFFFF, {  0.625,  0.0 } },
    { {  1.0, -1.0,  1.0 }, 0xFFFFFFFF, {  0.625,  0.9375 } },
  
    // Image/Video (18)
    { { -1.0,  1.0,  1.0 }, 0xFFFFFFFF, {  0.0,  0.0 } },
    { { -1.0, -1.0,  1.0 }, 0xFFFFFFFF, {  0.0,  1.0 } },
    { {  1.0, -1.0,  1.0 }, 0xFFFFFFFF, {  1.0,  1.0 } },
    { { -1.0,  1.0,  1.0 }, 0xFFFFFFFF, {  0.0,  0.0 } },
    { {  1.0,  1.0,  1.0 }, 0xFFFFFFFF, {  1.0,  0.0 } },
    { {  1.0, -1.0,  1.0 }, 0xFFFFFFFF, {  1.0,  1.0 } }
};
#endif
// clang-format on

#if RETRO_RENDERDEVICE_DIRECTX9
#include "DX9/DX9RenderDevice.cpp"
#elif RETRO_RENDERDEVICE_DIRECTX11
#include "DX11/DX11RenderDevice.cpp"
#elif RETRO_RENDERDEVICE_SDL2
#include "SDL2/SDL2RenderDevice.cpp"
#elif RETRO_RENDERDEVICE_GLFW
#include "GLFW/GLFWRenderDevice.cpp"
#elif RETRO_RENDERDEVICE_VK
#include "Vulkan/VulkanRenderDevice.cpp"
#elif RETRO_RENDERDEVICE_EGL
#include "EGL/EGLRenderDevice.cpp"
#elif RETRO_RENDERDEVICE_KALLISTIOS
#include "KallistiOS/KallistiOSRenderDevice.cpp"
#endif

RenderDevice::WindowInfo RenderDevice::displayInfo;

DrawList RSDK::drawGroups[DRAWGROUP_COUNT];

#if !defined(KOS_HARDWARE_RENDERER)
uint16 RSDK::blendLookupTable[0x20 * 0x100];
uint16 RSDK::subtractLookupTable[0x20 * 0x100];
#endif

GFXSurface RSDK::gfxSurface[SURFACE_COUNT];

#if RETRO_PLATFORM == RETRO_KALLISTIOS
// DC_SILHOUETTE
int32 RSDK::silhouetteRegionCount = 0;
SilhouetteRegion RSDK::silhouetteRegions[MAX_SILHOUETTE_REGIONS];

void RSDK::SetSilhouetteRegion(int32 x1, int32 y1, int32 x2, int32 y2, int32 drawGroup)
{
    if (silhouetteRegionCount < MAX_SILHOUETTE_REGIONS) {
        SilhouetteRegion *r = &silhouetteRegions[silhouetteRegionCount++];
        r->x1        = x1;
        r->y1        = y1;
        r->x2        = x2;
        r->y2        = y2;
        r->drawGroup = drawGroup;
    }
}

void RSDK::ClearSilhouetteRegions() { silhouetteRegionCount = 0; }

// DC_DESATURATE
void RSDK::SetPaletteDesaturation(uint8 amount) { RenderDevice::SetPaletteDesaturation(amount); }
#endif

float RSDK::dpi         = 1;
int32 RSDK::cameraCount = 0;
ScreenInfo RSDK::screens[SCREEN_COUNT];
CameraInfo RSDK::cameras[CAMERA_COUNT];
ScreenInfo *RSDK::currentScreen = NULL;

#if RETRO_PLATFORM != RETRO_KALLISTIOS
int32 RSDK::shaderCount = 0;
ShaderEntry RSDK::shaderList[SHADER_COUNT];
#endif

bool32 RSDK::changedVideoSettings = false;
VideoSettings RSDK::videoSettings;
VideoSettings RSDK::videoSettingsBackup;

#if RETRO_USE_MOD_LOADER
int32 RSDK::userShaderCount = 0;
#endif

bool32 RenderDeviceBase::isRunning         = false;
int32 RenderDeviceBase::windowRefreshDelay = 0;

#if RETRO_REV02
uint8 RenderDeviceBase::startVertex_2P[] = { 18, 24 };
uint8 RenderDeviceBase::startVertex_3P[] = { 30, 36, 12 };
#endif

float2 RenderDeviceBase::pixelSize   = { DEFAULT_PIXWIDTH, SCREEN_YSIZE };
float2 RenderDeviceBase::textureSize = { 512.0, 256.0 };
float2 RenderDeviceBase::viewSize    = { 0, 0 };

int32 RenderDeviceBase::displayWidth[16];
int32 RenderDeviceBase::displayHeight[16];
int32 RenderDeviceBase::displayCount = 0;

int32 RenderDeviceBase::lastShaderID = -1;

char RSDK::drawGroupNames[0x10][0x10] = {
    "Draw Group 0", "Draw Group 1", "Draw Group 2",  "Draw Group 3",  "Draw Group 4",  "Draw Group 5",  "Draw Group 6",  "Draw Group 7",
    "Draw Group 8", "Draw Group 9", "Draw Group 10", "Draw Group 11", "Draw Group 12", "Draw Group 13", "Draw Group 14", "Draw Group 15",
};

#include "RSDK/Dev/DevFont.hpp"

// 50% alpha, but way faster
#define setPixelBlend(pixel, frameBufferClr) frameBufferClr = ((pixel >> 1) & 0x7BEF) + ((frameBufferClr >> 1) & 0x7BEF)

// Alpha blending
#define setPixelAlpha(pixel, frameBufferClr, alpha)                                                                                                  \
    int32 R = (fbufferBlend[(frameBufferClr & 0xF800) >> 11] + pixelBlend[(pixel & 0xF800) >> 11]) << 11;                                            \
    int32 G = (fbufferBlend[(frameBufferClr & 0x7E0) >> 6] + pixelBlend[(pixel & 0x7E0) >> 6]) << 6;                                                 \
    int32 B = fbufferBlend[frameBufferClr & 0x1F] + pixelBlend[pixel & 0x1F];                                                                        \
                                                                                                                                                     \
    frameBufferClr = R | G | B;

// Additive Blending
#define setPixelAdditive(pixel, frameBufferClr)                                                                                                      \
    int32 R = MIN((blendTablePtr[(pixel & 0xF800) >> 11] << 11) + (frameBufferClr & 0xF800), 0xF800);                                                \
    int32 G = MIN((blendTablePtr[(pixel & 0x7E0) >> 6] << 6) + (frameBufferClr & 0x7E0), 0x7E0);                                                     \
    int32 B = MIN(blendTablePtr[pixel & 0x1F] + (frameBufferClr & 0x1F), 0x1F);                                                                      \
                                                                                                                                                     \
    frameBufferClr = R | G | B;

// Subtractive Blending
#define setPixelSubtractive(pixel, frameBufferClr)                                                                                                   \
    int32 R = MAX((frameBufferClr & 0xF800) - (subBlendTable[(pixel & 0xF800) >> 11] << 11), 0);                                                     \
    int32 G = MAX((frameBufferClr & 0x7E0) - (subBlendTable[(pixel & 0x7E0) >> 6] << 6), 0);                                                         \
    int32 B = MAX((frameBufferClr & 0x1F) - subBlendTable[pixel & 0x1F], 0);                                                                         \
                                                                                                                                                     \
    frameBufferClr = R | G | B;

// Only draw if framebuffer clr IS maskColor
#define setPixelMasked(pixel, frameBufferClr)                                                                                                        \
    if (frameBufferClr == maskColor)                                                                                                                 \
        frameBufferClr = pixel;

// Only draw if framebuffer clr is NOT maskColor
#define setPixelUnmasked(pixel, frameBufferClr)                                                                                                      \
    if (frameBufferClr != maskColor)                                                                                                                 \
        frameBufferClr = pixel;

void RSDK::RenderDeviceBase::ProcessDimming()
{
    // Bug Details:
    // if you've ever used debug mode before the first frame of v5 executes you may've seen how the game starts dimmed
    // this is because dimLimit hasn't been initialized yet, so it starts at 0. Likewise, the timer also starts at 0
    // this leads the game to immediately begin dimming when it shouldn't be
    // Fix:
    // initialize dimLimit before ProcessStage(), maybe in RenderDevice::SetupRendering() or something

    if (videoSettings.dimTimer < videoSettings.dimLimit) {
        if (videoSettings.dimPercent < 1.0f) {
            videoSettings.dimPercent += 0.05f;

            if (videoSettings.dimPercent > 1.0f)
                videoSettings.dimPercent = 1.0f;
        }
    }
    else {
        if (videoSettings.dimPercent > 0.25f)
            videoSettings.dimPercent *= 0.9f;
    }
}

void RSDK::GenerateBlendLookupTable()
{
#if !defined(KOS_HARDWARE_RENDERER)
    for (int32 y = 0; y < 0x100; y++) {
        for (int32 x = 0; x < 0x20; x++) {
            blendLookupTable[x + (y * 0x20)]    = y * x >> 8;
            subtractLookupTable[x + (y * 0x20)] = y * (0x1F - x) >> 8;
        }
    }
#endif

#if !RETRO_REV02
    for (int32 i = 0; i < 0x10000; ++i) {
        int32 tintValue    = (((uint32)i & 0x1F) + ((i >> 6) & 0x1F) + (((uint16)i >> 11) & 0x1F)) / 3 + 6;
        tintLookupTable[i] = 0x841 * MIN(0x1F, tintValue);
    }
#endif

    #if RETRO_PLATFORM != RETRO_KALLISTIOS || RETRO_USE_ORIGINAL_CODE
    for (int32 c = 0; c < 0x100; ++c) {
        rgb32To16_R[c] = (c & 0xFFF8) << 8;
        rgb32To16_G[c] = (c & 0xFFFC) << 3;
        rgb32To16_B[c] = c >> 3;
    }
    #endif
}

void RSDK::InitSystemSurfaces()
{
    GEN_HASH_MD5("TileBuffer", gfxSurface[0].hash);
    gfxSurface[0].scope    = SCOPE_GLOBAL;
#if RETRO_PLATFORM == RETRO_KALLISTIOS
    gfxSurface[0].width    = KOS_ATLAS_WIDTH_PIXELS;
    gfxSurface[0].height   = KOS_ATLAS_HEIGHT_PIXELS;
    gfxSurface[0].lineSize = KOS_ATLAS_LINE_SIZE_SHIFT;
    // texture is allocated when tileset is loaded
#else
    gfxSurface[0].width    = TILE_SIZE;
    gfxSurface[0].height   = TILE_COUNT * TILE_SIZE;
    gfxSurface[0].lineSize = 4; // 16px
#endif
#if RETRO_PLATFORM == RETRO_KALLISTIOS && defined(KOS_HARDWARE_RENDERER)
    gfxSurface[0].pixels   = nullptr;
#else
    gfxSurface[0].pixels   = tilesetPixels;
#endif

#if RETRO_REV02
    GEN_HASH_MD5("EngineText", gfxSurface[1].hash);
    gfxSurface[1].scope    = SCOPE_GLOBAL;
    gfxSurface[1].width    = 8;
    gfxSurface[1].height   = 128 * 8;
    gfxSurface[1].lineSize = 3; // 8px
    gfxSurface[1].pixels   = devTextStencil;
#endif
}

void RSDK::UpdateGameWindow() { RenderDevice::RefreshWindow(); }

void RSDK::GetDisplayInfo(int32 *displayID, int32 *width, int32 *height, int32 *refreshRate, char *text)
{
    if (!displayID)
        return;

    int32 prevDisplay = *displayID;
    int32 display     = 0;

    if (*displayID == -2) { // -2 == "get FS size display"
        if (videoSettings.fsWidth && videoSettings.fsHeight) {
            for (display = 0; display < RenderDevice::displayCount; ++display) {
#if RETRO_RENDERDEVICE_DIRECTX11
                int32 refresh = RenderDevice::displayInfo.displays[display].refresh_rate.Numerator
                                / RenderDevice::displayInfo.displays[display].refresh_rate.Denominator;
#else
                int32 refresh = RenderDevice::displayInfo.displays[display].refresh_rate;
#endif

                if (RenderDevice::displayInfo.displays[display].width == videoSettings.fsWidth
                    && RenderDevice::displayInfo.displays[display].height == videoSettings.fsHeight && refresh == videoSettings.refreshRate) {
                    break;
                }
            }

            display++;
        }
    }
    else {
        display = *displayID;
        if (prevDisplay < 0)
            display = RenderDevice::displayCount;
        else if (prevDisplay > RenderDevice::displayCount)
            display = 0;
    }

    *displayID = display;
    if (display) {
        int32 d = display - 1;

#if RETRO_RENDERDEVICE_DIRECTX11
        int32 refresh = RenderDevice::displayInfo.displays[d].refresh_rate.Numerator / RenderDevice::displayInfo.displays[d].refresh_rate.Denominator;
#else
        int32 refresh = RenderDevice::displayInfo.displays[d].refresh_rate;
#endif

        int32 displayWidth   = RenderDevice::displayInfo.displays[d].width;
        int32 displayHeight  = RenderDevice::displayInfo.displays[d].height;
        int32 displayRefresh = refresh;

        if (width)
            *width = displayWidth;

        if (height)
            *height = displayHeight;

        if (refreshRate)
            *refreshRate = displayRefresh;

        if (text)
            sprintf(text, "%ix%i @%iHz", displayWidth, displayHeight, displayRefresh);
    }
    else { // displayID 0 == "default fullscreen size"
        if (width)
            *width = 0;

        if (height)
            *height = 0;

        if (refreshRate)
            *refreshRate = 0;

        if (text)
            sprintf(text, "%s", "DEFAULT");
    }
}

void RSDK::GetWindowSize(int32 *width, int32 *height) { RenderDevice::GetWindowSize(width, height); }

void RSDK::SetScreenSize(uint8 screenID, uint16 width, uint16 height)
{
    if (screenID < SCREEN_COUNT) {
        ScreenInfo *screen = &screens[screenID];

        screen->size.x   = width;
        screen->size.y   = height & 0xFFF0;
        screen->pitch    = (screen->size.x + 15) & 0xFFFFFFF0;
        screen->center.x = screen->size.x >> 1;
        screen->center.y = screen->size.y >> 1;

        screen->clipBound_X1 = 0;
        screen->clipBound_X2 = screen->size.x;
        screen->clipBound_Y1 = 0;
        screen->clipBound_Y2 = screen->size.y;

        screen->waterDrawPos = screen->size.y;

#if RETRO_REV0U
        RSDK::Legacy::SCREEN_XSIZE        = width;
        RSDK::Legacy::SCREEN_CENTERX      = width / 2;
        RSDK::Legacy::SCREEN_SCROLL_LEFT  = RSDK::Legacy::SCREEN_CENTERX - 8;
        RSDK::Legacy::SCREEN_SCROLL_RIGHT = RSDK::Legacy::SCREEN_CENTERX + 8;
        RSDK::Legacy::OBJECT_BORDER_X2    = width + 0x80;
        RSDK::Legacy::OBJECT_BORDER_X4    = width + 0x20;

        RSDK::Legacy::GFX_LINESIZE          = screen->pitch;
        RSDK::Legacy::GFX_LINESIZE_MINUSONE = screen->pitch - 1;
        RSDK::Legacy::GFX_LINESIZE_DOUBLE   = 2 * screen->pitch;
        RSDK::Legacy::GFX_FRAMEBUFFERSIZE   = SCREEN_YSIZE * screen->pitch;
        RSDK::Legacy::GFX_FBUFFERMINUSONE   = SCREEN_YSIZE * screen->pitch - 1;
#endif
    }
}

int32 RSDK::GetVideoSetting(int32 id)
{
    switch (id) {
        case VIDEOSETTING_WINDOWED: return videoSettings.windowed;
        case VIDEOSETTING_BORDERED: return videoSettings.bordered;
        case VIDEOSETTING_EXCLUSIVEFS: return videoSettings.exclusiveFS;
        case VIDEOSETTING_VSYNC: return videoSettings.vsync;
        case VIDEOSETTING_TRIPLEBUFFERED: return videoSettings.tripleBuffered;
        case VIDEOSETTING_WINDOW_WIDTH: return videoSettings.windowWidth;
        case VIDEOSETTING_WINDOW_HEIGHT: return videoSettings.windowHeight;
        case VIDEOSETTING_FSWIDTH: return videoSettings.fsWidth;
        case VIDEOSETTING_FSHEIGHT: return videoSettings.fsHeight;
        case VIDEOSETTING_REFRESHRATE: return videoSettings.refreshRate;
        case VIDEOSETTING_SHADERSUPPORT: return videoSettings.shaderSupport;
        case VIDEOSETTING_SHADERID: return videoSettings.shaderID;
        case VIDEOSETTING_SCREENCOUNT: return videoSettings.screenCount;
#if RETRO_REV02
        case VIDEOSETTING_DIMTIMER: return videoSettings.dimTimer;
#endif
        case VIDEOSETTING_STREAMSENABLED: return engine.streamsEnabled;
        case VIDEOSETTING_STREAM_VOL: return (int32)(engine.streamVolume * 1024.0);
        case VIDEOSETTING_SFX_VOL: return (int32)(engine.soundFXVolume * 1024.0);
        case VIDEOSETTING_LANGUAGE:
#if RETRO_REV02
            return SKU::curSKU.language;
#else
            return gameVerInfo.language;
#endif
        case VIDEOSETTING_CHANGED: return changedVideoSettings;

        default: break;
    }

    return 0;
}

void RSDK::SetVideoSetting(int32 id, int32 value)
{
    bool32 boolVal = value;
    switch (id) {
        case VIDEOSETTING_WINDOWED:
            if (videoSettings.windowed != boolVal) {
                videoSettings.windowed = boolVal;
                changedVideoSettings   = true;
            }
            break;

        case VIDEOSETTING_BORDERED:
            if (videoSettings.bordered != boolVal) {
                videoSettings.bordered = boolVal;
                changedVideoSettings   = true;
            }
            break;

        case VIDEOSETTING_EXCLUSIVEFS:
            if (videoSettings.exclusiveFS != boolVal) {
                videoSettings.exclusiveFS = boolVal;
                changedVideoSettings      = true;
            }
            break;

        case VIDEOSETTING_VSYNC:
            if (videoSettings.vsync != boolVal) {
                videoSettings.vsync  = boolVal;
                changedVideoSettings = true;
            }
            break;

        case VIDEOSETTING_TRIPLEBUFFERED:
            if (videoSettings.tripleBuffered != boolVal) {
                videoSettings.tripleBuffered = boolVal;
                changedVideoSettings         = true;
            }
            break;

        case VIDEOSETTING_WINDOW_WIDTH:
            if (videoSettings.windowWidth != value) {
                videoSettings.windowWidth = value;
                changedVideoSettings      = true;
            }
            break;

        case VIDEOSETTING_WINDOW_HEIGHT:
            if (videoSettings.windowHeight != value) {
                videoSettings.windowHeight = value;
                changedVideoSettings       = true;
            }
            break;

        case VIDEOSETTING_FSWIDTH: videoSettings.fsWidth = value; break;
        case VIDEOSETTING_FSHEIGHT: videoSettings.fsHeight = value; break;
        case VIDEOSETTING_REFRESHRATE: videoSettings.refreshRate = value; break;
        case VIDEOSETTING_SHADERSUPPORT: videoSettings.shaderSupport = value; break;
        case VIDEOSETTING_SHADERID:
            if (videoSettings.shaderID != value) {
                videoSettings.shaderID = value;
                changedVideoSettings   = true;
            }
            break;

        case VIDEOSETTING_SCREENCOUNT: videoSettings.screenCount = value; break;
#if RETRO_REV02
        case VIDEOSETTING_DIMTIMER: videoSettings.dimLimit = value; break;
#endif
        case VIDEOSETTING_STREAMSENABLED:
            if (engine.streamsEnabled != boolVal)
                changedVideoSettings = true;

            engine.streamsEnabled = boolVal;
            break;

        case VIDEOSETTING_STREAM_VOL:
            if (engine.streamVolume != (value / 1024.0f)) {
                engine.streamVolume  = (float)value / 1024.0f;
                changedVideoSettings = true;
            }
            break;

        case VIDEOSETTING_SFX_VOL:
            if (engine.soundFXVolume != ((float)value / 1024.0f)) {
                engine.soundFXVolume = (float)value / 1024.0f;
                changedVideoSettings = true;
            }
            break;

        case VIDEOSETTING_LANGUAGE:
#if RETRO_REV02
            SKU::curSKU.language = value;
#else
            gameVerInfo.language = value;
#endif
            break;

        case VIDEOSETTING_STORE: memcpy(&videoSettingsBackup, &videoSettings, sizeof(videoSettings)); break;

        case VIDEOSETTING_RELOAD:
            changedVideoSettings = true;
            memcpy(&videoSettings, &videoSettingsBackup, sizeof(videoSettingsBackup));
            break;

        case VIDEOSETTING_CHANGED: changedVideoSettings = boolVal; break;

        case VIDEOSETTING_WRITE: SaveSettingsINI(value); break;

        default: break;
    }
}

void RSDK::SwapDrawListEntries(uint8 drawGroup, uint16 slot1, uint16 slot2, uint16 count)
{
    if (drawGroup < DRAWGROUP_COUNT) {
        DrawList *group = &drawGroups[drawGroup];
        if (count <= 0 || count > group->entityCount)
            count = group->entityCount;

        int32 index1 = -1;
        int32 index2 = -1;
        for (int32 i = 0; i < count; ++i) {
            if (group->entries[i] == slot1)
                index1 = i;
            if (group->entries[i] == slot2)
                index2 = i;
        }

        if (index1 > -1 && index2 > -1 && index1 < index2) {
            int32 temp             = group->entries[index2];
            group->entries[index2] = group->entries[index1];
            group->entries[index1] = temp;
        }
    }
}

void RSDK::FillScreen(uint32 color, int32 alphaR, int32 alphaG, int32 alphaB)
{
    alphaR = CLAMP(alphaR, 0x00, 0xFF);
    alphaG = CLAMP(alphaG, 0x00, 0xFF);
    alphaB = CLAMP(alphaB, 0x00, 0xFF);

    if (alphaR + alphaG + alphaB) {
        #if RETRO_PLATFORM == RETRO_KALLISTIOS
        validDraw = true;

        if (alphaR == 0xFF && alphaG == 0xFF && alphaB == 0xFF) {
            RenderDevice::PrepareColoredPolyPT(0, INK_NONE);
            RenderDevice::DrawColoredPolyPT(0, 0, currentScreen->size.x, currentScreen->size.y, 0xFF000000 | color);
        } else {
            RenderDevice::DrawTintedFillScreen(alphaR, alphaG, alphaB, color);
        }
        #else
        validDraw        = true;
        #if RETRO_PLATFORM != RETRO_KALLISTIOS || RETRO_USE_ORIGINAL_CODE
        uint16 clrBlendR = blendLookupTable[0x20 * alphaR + rgb32To16_B[(color >> 0x10) & 0xFF]];
        uint16 clrBlendG = blendLookupTable[0x20 * alphaG + rgb32To16_B[(color >> 0x08) & 0xFF]];
        uint16 clrBlendB = blendLookupTable[0x20 * alphaB + rgb32To16_B[(color >> 0x00) & 0xFF]];
        #else
        uint16 clrBlendR = blendLookupTable[0x20 * alphaR + (((color >> 0x10) & 0xFF) >> 3)];
        uint16 clrBlendG = blendLookupTable[0x20 * alphaG + (((color >> 0x08) & 0xFF) >> 3)];
        uint16 clrBlendB = blendLookupTable[0x20 * alphaB + ((color & 0xFF) >> 3)];
        #endif

        uint16 *fbBlendR = &blendLookupTable[0x20 * (0xFF - alphaR)];
        uint16 *fbBlendG = &blendLookupTable[0x20 * (0xFF - alphaG)];
        uint16 *fbBlendB = &blendLookupTable[0x20 * (0xFF - alphaB)];

        int32 cnt = currentScreen->size.y * currentScreen->pitch;
        for (int32 id = 0; cnt > 0; --cnt, ++id) {
            uint16 px = currentScreen->frameBuffer[id];

            int32 R = fbBlendR[(px & 0xF800) >> 11] + clrBlendR;
            int32 G = fbBlendG[(px & 0x7E0) >> 6] + clrBlendG;
            int32 B = fbBlendB[px & 0x1F] + clrBlendB;

            currentScreen->frameBuffer[id] = (B) | (G << 6) | (R << 11);
        }
        #endif
    }
}

#if RETRO_PLATFORM == RETRO_KALLISTIOS && defined(KOS_HARDWARE_RENDERER)
void RSDK::Draw3DLine(float z, int32 x1, int32 y1, int32 x2, int32 y2, uint32 color, int32 alpha, int32 inkEffect, bool32 screenRelative) {
    float oldDepth = RenderDevice::GetDepth();
    RenderDevice::SetDepth(oldDepth + z);
    DrawLine(x1, y1, x2, y2, color, alpha, inkEffect, screenRelative);
    RenderDevice::SetDepth(oldDepth);
}
#endif
void RSDK::DrawLine(int32 x1, int32 y1, int32 x2, int32 y2, uint32 color, int32 alpha, int32 inkEffect, bool32 screenRelative)
{
    switch (inkEffect) {
        default: break;

        case INK_ALPHA:
            if (alpha > 0xFF)
                inkEffect = INK_NONE;
            else if (alpha <= 0)
                return;
            break;

        case INK_ADD:
        case INK_SUB:
            if (alpha > 0xFF)
                alpha = 0xFF;
            else if (alpha <= 0)
                return;
            break;

        case INK_TINT:
            if (!tintLookupTable)
                return;
            break;
    }

    int32 drawY1 = y1;
    int32 drawX1 = x1;
    int32 drawY2 = y2;
    int32 drawX2 = x2;

    if (!screenRelative) {
        drawX1 = FROM_FIXED(x1) - currentScreen->position.x;
        drawY1 = FROM_FIXED(y1) - currentScreen->position.y;
        drawX2 = FROM_FIXED(x2) - currentScreen->position.x;
        drawY2 = FROM_FIXED(y2) - currentScreen->position.y;
    }

#if !defined(KOS_HARDWARE_RENDERER)
    int32 flags1 = 0;
    if (drawX1 >= currentScreen->clipBound_X2)
        flags1 = 2;
    else if (drawX1 < currentScreen->clipBound_X1)
        flags1 = 1;

    if (drawY1 >= currentScreen->clipBound_Y2)
        flags1 |= 8;
    else if (drawY1 < currentScreen->clipBound_Y1)
        flags1 |= 4;

    int32 flags2 = 0;
    if (drawX2 >= currentScreen->clipBound_X2)
        flags2 = 2;
    else if (drawX2 < currentScreen->clipBound_X1)
        flags2 = 1;

    if (drawY2 >= currentScreen->clipBound_Y2)
        flags2 |= 8;
    else if (drawY2 < currentScreen->clipBound_Y1)
        flags2 |= 4;

    while (flags1 || flags2) {
        if (flags1 & flags2)
            return;

        int32 curFlags = flags2;
        if (flags1)
            curFlags = flags1;

        int32 x = 0;
        int32 y = 0;
        if (curFlags & 8) {
            int32 div = (drawY2 - drawY1);
            if (!div)
                div = 1;
            x = drawX1 + ((drawX2 - drawX1) * (((currentScreen->clipBound_Y2 - drawY1) << 8) / div) >> 8);
            y = currentScreen->clipBound_Y2;
        }
        else if (curFlags & 4) {
            int32 div = (drawY2 - drawY1);
            if (!div)
                div = 1;
            x = drawX1 + ((drawX2 - drawX1) * (((currentScreen->clipBound_Y1 - drawY1) << 8) / div) >> 8);
            y = currentScreen->clipBound_Y1;
        }
        else if (curFlags & 2) {
            int32 div = (drawX2 - drawX1);
            if (!div)
                div = 1;
            x = currentScreen->clipBound_X2;
            y = drawY1 + ((drawY2 - drawY1) * (((currentScreen->clipBound_X2 - drawX1) << 8) / div) >> 8);
        }
        else if (curFlags & 1) {
            int32 div = (drawX2 - drawX1);
            if (!div)
                div = 1;
            x = currentScreen->clipBound_X1;
            y = drawY1 + ((drawY2 - drawY1) * (((currentScreen->clipBound_X1 - drawX1) << 8) / div) >> 8);
        }

        if (curFlags == flags1) {
            drawX1 = x;
            drawY1 = y;
            flags1 = 0;
            if (x > currentScreen->clipBound_X2) {
                flags1 = 2;
            }
            else if (x < currentScreen->clipBound_X1) {
                flags1 = 1;
            }

            if (y < currentScreen->clipBound_Y1) {
                flags1 |= 4;
            }
            else if (y > currentScreen->clipBound_Y2) {
                flags1 |= 8;
            }
        }
        else {
            drawX2 = x;
            drawY2 = y;
            flags2 = 0;
            if (x > currentScreen->clipBound_X2) {
                flags2 = 2;
            }
            else if (x < currentScreen->clipBound_X1) {
                flags2 = 1;
            }

            if (y < currentScreen->clipBound_Y1) {
                flags2 |= 4;
            }
            else if (y > currentScreen->clipBound_Y2) {
                flags2 |= 8;
            }
        }
    }

    if (drawX1 > currentScreen->clipBound_X2)
        drawX1 = currentScreen->clipBound_X2;
    else if (drawX1 < currentScreen->clipBound_X1)
        drawX1 = currentScreen->clipBound_X1;

    if (drawY1 > currentScreen->clipBound_Y2)
        drawY1 = currentScreen->clipBound_Y2;
    else if (drawY1 < currentScreen->clipBound_Y1)
        drawY1 = currentScreen->clipBound_Y1;

    if (drawX2 > currentScreen->clipBound_X2)
        drawX2 = currentScreen->clipBound_X2;
    else if (drawX2 < currentScreen->clipBound_X1)
        drawX2 = currentScreen->clipBound_X1;

    if (drawY2 > currentScreen->clipBound_Y2)
        drawY2 = currentScreen->clipBound_Y2;
    else if (drawY2 < currentScreen->clipBound_Y1)
        drawY2 = currentScreen->clipBound_Y1;

    int32 sizeX = abs(drawX2 - drawX1);
    int32 sizeY = abs(drawY2 - drawY1);
    int32 max   = sizeY;
    int32 hSize = sizeX >> 2;
    if (sizeX <= sizeY)
        hSize = -sizeY >> 2;

    if (drawX2 < drawX1) {
        int32 v = drawX1;
        drawX1  = drawX2;
        drawX2  = v;

        v      = drawY1;
        drawY1 = drawY2;
        drawY2 = v;
    }

#if RETRO_PLATFORM != RETRO_KALLISTIOS || RETRO_USE_ORIGINAL_CODE
    uint16 color16      = rgb32To16_B[(color >> 0) & 0xFF] | rgb32To16_G[(color >> 8) & 0xFF] | rgb32To16_R[(color >> 16) & 0xFF];
#else
    uint16 color16 = PACK_RGB888_32(color);
#endif
#endif // !defined(KOS_HARDWARE_RENDERER)

#if RETRO_PLATFORM == RETRO_KALLISTIOS && defined(KOS_HARDWARE_RENDERER)
    if (!RenderDevice::SupportedInk(inkEffect)) {
        return;
    }

    if (inkEffect == INK_NONE) {
        alpha = 0xFF;
    }

    // DC_DESATURATE: desaturate line color when paused, but not for the pause menu's own draw group
    uint8 desat = RenderDevice::GetPaletteDesaturation();
    if (desat > 0 && sceneInfo.currentDrawGroup < DRAWGROUP_COUNT - 1)
        color = DesaturateColor32(color, desat);

    uint32 color32      = (alpha << 24) | color;

    if ((inkEffect != INK_TINT) && (inkEffect != INK_ADD) && (alpha == 0xFF)) {
        RenderDevice::PrepareLinePolyPT(inkEffect);
        RenderDevice::DrawLinePolyPT(drawX1, drawY1, drawX2, drawY2, color32);
    } else {
        RenderDevice::PrepareLinePolyTR(inkEffect);
        RenderDevice::DrawLinePolyTR(drawX1, drawY1, drawX2, drawY2, color32);
    }
#else
    uint16 *frameBuffer = &currentScreen->frameBuffer[drawX1 + drawY1 * currentScreen->pitch];

    switch (inkEffect) {
        case INK_NONE:
            if (drawY1 > drawY2) {
                while (drawX1 < drawX2 || drawY1 >= drawY2) {
                    *frameBuffer = color16;

                    if (hSize > -sizeX) {
                        hSize -= max;
                        ++drawX1;
                        ++frameBuffer;
                    }

                    if (hSize < max) {
                        --drawY1;
                        hSize += sizeX;
                        frameBuffer -= currentScreen->pitch;
                    }
                }
            }
            else {
                while (true) {
                    *frameBuffer = color16;

                    if (drawX1 < drawX2 || drawY1 < drawY2) {
                        if (hSize > -sizeX) {
                            hSize -= max;
                            ++drawX1;
                            ++frameBuffer;
                        }

                        if (hSize < max) {
                            hSize += sizeX;
                            ++drawY1;
                            frameBuffer += currentScreen->pitch;
                        }
                    }
                    else {
                        break;
                    }
                }
            }
            break;

        case INK_BLEND:
            if (drawY1 > drawY2) {
                while (drawX1 < drawX2 || drawY1 >= drawY2) {
                    setPixelBlend(color16, *frameBuffer);

                    if (hSize > -sizeX) {
                        hSize -= max;
                        ++drawX1;
                        ++frameBuffer;
                    }

                    if (hSize < max) {
                        --drawY1;
                        hSize += sizeX;
                        frameBuffer -= currentScreen->pitch;
                    }
                }
            }
            else {
                while (true) {
                    setPixelBlend(color16, *frameBuffer);

                    if (drawX1 < drawX2 || drawY1 < drawY2) {
                        if (hSize > -sizeX) {
                            hSize -= max;
                            ++drawX1;
                            ++frameBuffer;
                        }

                        if (hSize < max) {
                            hSize += sizeX;
                            ++drawY1;
                            frameBuffer += currentScreen->pitch;
                        }
                    }
                    else {
                        break;
                    }
                }
            }
            break;

        case INK_ALPHA:
            if (drawY1 > drawY2) {
                uint16 *fbufferBlend = &blendLookupTable[0x20 * (0xFF - alpha)];
                uint16 *pixelBlend   = &blendLookupTable[0x20 * alpha];

                while (drawX1 < drawX2 || drawY1 >= drawY2) {
                    setPixelAlpha(color16, *frameBuffer, alpha);

                    if (hSize > -sizeX) {
                        hSize -= max;
                        ++drawX1;
                        ++frameBuffer;
                    }

                    if (hSize < max) {
                        --drawY1;
                        hSize += sizeX;
                        frameBuffer -= currentScreen->pitch;
                    }
                }
            }
            else {
                uint16 *fbufferBlend = &blendLookupTable[0x20 * (0xFF - alpha)];
                uint16 *pixelBlend   = &blendLookupTable[0x20 * alpha];

                while (true) {
                    setPixelAlpha(color16, *frameBuffer, alpha);

                    if (drawX1 < drawX2 || drawY1 < drawY2) {
                        if (hSize > -sizeX) {
                            hSize -= max;
                            ++drawX1;
                            ++frameBuffer;
                        }
                        if (hSize < max) {
                            hSize += sizeX;
                            ++drawY1;
                            frameBuffer += currentScreen->pitch;
                        }
                    }
                    else {
                        break;
                    }
                }
            }
            break;

        case INK_ADD:
            if (drawY1 > drawY2) {
                uint16 *blendTablePtr = &blendLookupTable[0x20 * alpha];

                while (drawX1 < drawX2 || drawY1 >= drawY2) {
                    setPixelAdditive(color16, *frameBuffer);

                    if (hSize > -sizeX) {
                        hSize -= max;
                        ++drawX1;
                        ++frameBuffer;
                    }

                    if (hSize < max) {
                        --drawY1;
                        hSize += sizeX;
                        frameBuffer -= currentScreen->pitch;
                    }
                }
            }
            else {
                uint16 *blendTablePtr = &blendLookupTable[0x20 * alpha];

                while (true) {
                    setPixelAdditive(color16, *frameBuffer);
                    if (drawX1 < drawX2 || drawY1 < drawY2) {
                        if (hSize > -sizeX) {
                            hSize -= max;
                            ++drawX1;
                            ++frameBuffer;
                        }

                        if (hSize < max) {
                            hSize += sizeX;
                            ++drawY1;
                            frameBuffer += currentScreen->pitch;
                        }
                    }
                    else {
                        break;
                    }
                }
            }
            break;

        case INK_SUB:
            if (drawY1 > drawY2) {
                uint16 *subBlendTable = &subtractLookupTable[0x20 * alpha];
                while (drawX1 < drawX2 || drawY1 >= drawY2) {
                    setPixelSubtractive(color16, *frameBuffer);

                    if (hSize > -sizeX) {
                        hSize -= max;
                        ++drawX1;
                        ++frameBuffer;
                    }

                    if (hSize < max) {
                        --drawY1;
                        hSize += sizeX;
                        frameBuffer -= currentScreen->pitch;
                    }
                }
            }
            else {
                uint16 *subBlendTable = &subtractLookupTable[0x20 * alpha];
                while (true) {
                    setPixelSubtractive(color16, *frameBuffer);

                    if (drawX1 < drawX2 || drawY1 < drawY2) {
                        if (hSize > -sizeX) {
                            hSize -= max;
                            ++drawX1;
                            ++frameBuffer;
                        }

                        if (hSize < max) {
                            hSize += sizeX;
                            ++drawY1;
                            frameBuffer += currentScreen->pitch;
                        }
                    }
                    else {
                        break;
                    }
                }
            }
            break;

        case INK_TINT:
            if (drawY1 > drawY2) {
                while (drawX1 < drawX2 || drawY1 >= drawY2) {
                    *frameBuffer = tintLookupTable[*frameBuffer];

                    if (hSize > -sizeX) {
                        hSize -= max;
                        ++drawX1;
                        ++frameBuffer;
                    }

                    if (hSize < max) {
                        --drawY1;
                        hSize += sizeX;
                        frameBuffer -= currentScreen->pitch;
                    }
                }
            }
            else {
                while (true) {
                    *frameBuffer = tintLookupTable[*frameBuffer];

                    if (drawX1 < drawX2 || drawY1 < drawY2) {
                        if (hSize > -sizeX) {
                            hSize -= max;
                            ++drawX1;
                            ++frameBuffer;
                        }

                        if (hSize < max) {
                            hSize += sizeX;
                            ++drawY1;
                            frameBuffer += currentScreen->pitch;
                        }
                    }
                    else {
                        break;
                    }
                }
            }
            break;

        case INK_MASKED:
            if (drawY1 > drawY2) {
                while (drawX1 < drawX2 || drawY1 >= drawY2) {
                    if (*frameBuffer == maskColor)
                        *frameBuffer = color16;

                    if (hSize > -sizeX) {
                        hSize -= max;
                        ++drawX1;
                        ++frameBuffer;
                    }

                    if (hSize < max) {
                        --drawY1;
                        hSize += sizeX;
                        frameBuffer -= currentScreen->pitch;
                    }
                }
            }
            else {
                while (true) {
                    if (*frameBuffer == maskColor)
                        *frameBuffer = color16;

                    if (drawX1 < drawX2 || drawY1 < drawY2) {
                        if (hSize > -sizeX) {
                            hSize -= max;
                            ++drawX1;
                            ++frameBuffer;
                        }

                        if (hSize < max) {
                            hSize += sizeX;
                            ++drawY1;
                            frameBuffer += currentScreen->pitch;
                        }
                    }
                    else {
                        break;
                    }
                }
            }
            break;

        case INK_UNMASKED:
            if (drawY1 > drawY2) {
                while (drawX1 < drawX2 || drawY1 >= drawY2) {
                    if (*frameBuffer != maskColor)
                        *frameBuffer = color16;

                    if (hSize > -sizeX) {
                        hSize -= max;
                        ++drawX1;
                        ++frameBuffer;
                    }

                    if (hSize < max) {
                        --drawY1;
                        hSize += sizeX;
                        frameBuffer -= currentScreen->pitch;
                    }
                }
            }
            else {
                while (true) {
                    if (*frameBuffer != maskColor)
                        *frameBuffer = color16;

                    if (drawX1 < drawX2 || drawY1 < drawY2) {
                        if (hSize > -sizeX) {
                            hSize -= max;
                            ++drawX1;
                            ++frameBuffer;
                        }

                        if (hSize < max) {
                            hSize += sizeX;
                            ++drawY1;
                            frameBuffer += currentScreen->pitch;
                        }
                    }
                    else {
                        break;
                    }
                }
            }
            break;
    }
#endif
}
void RSDK::DrawRectangle(int32 x, int32 y, int32 width, int32 height, uint32 color, int32 alpha, int32 inkEffect, bool32 screenRelative)
{
    switch (inkEffect) {
        default: break;
        case INK_ALPHA:
            if (alpha > 0xFF)
                inkEffect = INK_NONE;
            else if (alpha <= 0)
                return;
            break;

        case INK_ADD:
        case INK_SUB:
            if (alpha > 0xFF)
                alpha = 0xFF;
            else if (alpha <= 0)
                return;
            break;

        case INK_TINT:
            if (!tintLookupTable)
                return;
            break;
    }

    if (!screenRelative) {
        x      = FROM_FIXED(x) - currentScreen->position.x;
        y      = FROM_FIXED(y) - currentScreen->position.y;
        width  = FROM_FIXED(width);
        height = FROM_FIXED(height);
    }

    if (width + x > currentScreen->clipBound_X2)
        width = currentScreen->clipBound_X2 - x;

    if (x < currentScreen->clipBound_X1) {
        width += x - currentScreen->clipBound_X1;
        x = currentScreen->clipBound_X1;
    }

    if (height + y > currentScreen->clipBound_Y2)
        height = currentScreen->clipBound_Y2 - y;

    if (y < currentScreen->clipBound_Y1) {
        height += y - currentScreen->clipBound_Y1;
        y = currentScreen->clipBound_Y1;
    }

    if (width <= 0 || height <= 0)
        return;

#if RETRO_PLATFORM == RETRO_KALLISTIOS && defined(KOS_HARDWARE_RENDERER)
    if (!RenderDevice::SupportedInk(inkEffect)) {
        return;
    }

    if (inkEffect == INK_NONE) {
        alpha = 0xFF;
    }

    // DC_DESATURATE: desaturate rect color when paused, but not for the pause menu's own draw group
    uint8 desat = RenderDevice::GetPaletteDesaturation();
    if (desat > 0 && sceneInfo.currentDrawGroup < DRAWGROUP_COUNT - 1)
        color = DesaturateColor32(color, desat);

    // water pools in GHZ Act 2 use INK_SUB with solid colored rectangles
    // we don't have any special handling for this in KallistiOSRenderDevice
    // if we don't handle it here, they will be opaque robin-egg blue rectangles
    // this is THE ONLY DrawRect call with INK_SUB in Mania (I checked -- jn64)
    if (inkEffect == INK_SUB) {
        uint8_t r = (color >> 16) & 0xFF;
        uint8_t g = (color >>  8) & 0xFF;
        uint8_t b = (color      ) & 0xFF;
        // INK_SUB isn't quite a linear darken, but not a lot we can do here
        if (r > 32)
            r -= 32;
        else
            r = 0;

        if (b > 32)
            b -= 32;
        else
            b = 0;

        if (g > 32)
            g -= 32;
        else
            g = 0;

        color = (r << 16) | (g << 8) | b;
        alpha = 0x60;
        inkEffect = INK_ALPHA;
    }

    validDraw = true;

    if ((inkEffect != INK_TINT) && (inkEffect != INK_ADD) && (alpha == 0xFF)) {
        RenderDevice::PrepareColoredPolyPT(y, inkEffect);
        RenderDevice::DrawColoredPolyPT(x, y, width, height, color | (alpha << 24));
    } else {
        RenderDevice::PrepareColoredPolyTR(y, inkEffect);
        RenderDevice::DrawColoredPolyTR(x, y, width, height, color | (alpha << 24));
    }
#else
    int32 pitch         = currentScreen->pitch - width;
    validDraw           = true;
    uint16 *frameBuffer = &currentScreen->frameBuffer[x + (y * currentScreen->pitch)];
#if RETRO_PLATFORM != RETRO_KALLISTIOS || RETRO_USE_ORIGINAL_CODE
    uint16 color16      = rgb32To16_B[(color >> 0) & 0xFF] | rgb32To16_G[(color >> 8) & 0xFF] | rgb32To16_R[(color >> 16) & 0xFF];
#else
    uint16 color16 = PACK_RGB888_32(color);
#endif

    switch (inkEffect) {
        case INK_NONE: {
            int32 h = height;
            while (h--) {
                int32 w = width;
                while (w--) {
                    *frameBuffer = color16;
                    ++frameBuffer;
                }

                frameBuffer += pitch;
            }
            break;
        }

        case INK_BLEND: {
            int32 h = height;
            while (h--) {
                int32 w = width;
                while (w--) {
                    setPixelBlend(color16, *frameBuffer);
                    ++frameBuffer;
                }
                frameBuffer += pitch;
            }
            break;
        }

        case INK_ALPHA: {
            uint16 *fbufferBlend = &blendLookupTable[0x20 * (0xFF - alpha)];
            uint16 *pixelBlend   = &blendLookupTable[0x20 * alpha];

            int32 h = height;
            while (h--) {
                int32 w = width;
                while (w--) {
                    setPixelAlpha(color16, *frameBuffer, alpha);
                    ++frameBuffer;
                }
                frameBuffer += pitch;
            }
            break;
        }

        case INK_ADD: {
            uint16 *blendTablePtr = &blendLookupTable[0x20 * alpha];
            int32 h               = height;
            while (h--) {
                int32 w = width;
                while (w--) {
                    setPixelAdditive(color16, *frameBuffer);
                    ++frameBuffer;
                }
                frameBuffer += pitch;
            }
            break;
        }

        case INK_SUB: {
            uint16 *subBlendTable = &subtractLookupTable[0x20 * alpha];
            int32 h               = height;
            while (h--) {
                int32 w = width;
                while (w--) {
                    setPixelSubtractive(color16, *frameBuffer);
                    ++frameBuffer;
                }
                frameBuffer += pitch;
            }
            break;
        }

        case INK_TINT: {
            int32 h = height;
            while (h--) {
                int32 w = width;
                while (w--) {
                    *frameBuffer = tintLookupTable[*frameBuffer];
                    ++frameBuffer;
                }
                frameBuffer += pitch;
            }
            break;
        }

        case INK_MASKED: {
            int32 h = height;
            while (h--) {
                int32 w = width;
                while (w--) {
                    if (*frameBuffer == maskColor)
                        *frameBuffer = color16;
                    ++frameBuffer;
                }
                frameBuffer += pitch;
            }
            break;
        }

        case INK_UNMASKED: {
            int32 h = height;
            while (h--) {
                int32 w = width;
                while (w--) {
                    if (*frameBuffer != maskColor)
                        *frameBuffer = color16;
                    ++frameBuffer;
                }
                frameBuffer += pitch;
            }
            break;
        }
    }
#endif
}
#if RETRO_PLATFORM == RETRO_KALLISTIOS && defined(KOS_HARDWARE_RENDERER)
// Helpers for DrawCircle/DrawCircleOutline functions
static void DrawFilledCircleFromTris(int cx, int cy, int radius, uint32 color, int32 alpha, int32 inkEffect)
{
    const int segs = 64;
    const float step = 2.0f * (float)M_PI / (float)segs;

    struct Vector2 triv[3];

    int32 red = (color >> 16) & 0xFF;
    int32 green = (color >> 8) & 0xFf;
    int32 blue = color & 0xFF;

    radius <<= 16;
    cx <<= 16;
    cy <<= 16;

    int x0 = cx + radius;
    int y0 = cy;

    triv[2].x = cx;
    triv[2].y = cy;

    float angle = step;

    for (int i = 1; i <= segs; i++) {
        int x1 = cx + (int)((float)radius * cosf(angle));
        int y1 = cy + (int)((float)radius * sinf(angle));
        triv[1].x = x0;
        triv[1].y = y0;
        triv[0].x = x1;
        triv[0].y = y1;

        DrawFace(triv, 3, red, green, blue, alpha, inkEffect);

        x0 = x1;
        y0 = y1;

        angle += step;
    }
}

static void DrawCircleOutlineWithLines(int cx, int cy, int radius, int width, uint32 color, int32 alpha, int32 inkEffect)
{
    const int segs = 64;
    const float step = 2.0f * (float)M_PI / (float)segs;

    RenderDevice::SetLinePolyThickness(width);

    int x0 = cx + radius;
    int y0 = cy;

    // avoid blowing up hybrid rendering with a huge amount of buffered TR
    if (inkEffect == INK_BLEND) {
        inkEffect = INK_NONE;
        alpha = 0xFF;
    }

    float angle = step;
    for (int i = 1; i <= segs; i++) {
        int x1 = cx + (int)((float)radius * cosf(angle));
        int y1 = cy + (int)((float)radius * sinf(angle));

        DrawLine(x0, y0, x1, y1, color, alpha, inkEffect, true);

        x0 = x1;
        y0 = y1;

        angle += step;
    }

    RenderDevice::SetLinePolyThickness(2);
}

static void DrawFilledCircleClippedToTriangle(int cx, int cy, int radius, uint32 color, int32 alpha, int32 inkEffect,
                                              int32 triTopX, int32 triTopY, int32 triBotLeftX, int32 triBotY, int32 triBotRightX)
{
    int32 red   = (color >> 16) & 0xFF;
    int32 green = (color >> 8) & 0xFF;
    int32 blue  = color & 0xFF;

    int32 yTop    = cy - radius;
    int32 yBottom = cy + radius;
    if (yTop < triTopY)
        yTop = triTopY;
    if (yBottom > triBotY)
        yBottom = triBotY;

    int32 triHeight = triBotY - triTopY;
    if (triHeight <= 0)
        return;

    for (int32 scanY = yTop; scanY <= yBottom; ++scanY) {
        int32 dy    = scanY - cy;
        int32 r2    = radius * radius;
        int32 dy2   = dy * dy;
        if (dy2 >= r2)
            continue;

        float dx    = sqrtf((float)(r2 - dy2));
        int32 cLeft  = cx - (int32)dx;
        int32 cRight = cx + (int32)dx;

        int32 t_num  = scanY - triTopY;
        int32 diagX  = triTopX + (triBotLeftX - triTopX) * t_num / triHeight;

        int32 clipLeft  = diagX;
        int32 clipRight = triBotRightX;

        if (cLeft < clipLeft)
            cLeft = clipLeft;
        if (cRight > clipRight)
            cRight = clipRight;

        if (cLeft >= cRight)
            continue;

        Vector2 verts[4];
        verts[0].x = cLeft << 16;
        verts[0].y = scanY << 16;
        verts[1].x = cRight << 16;
        verts[1].y = scanY << 16;
        verts[2].x = cRight << 16;
        verts[2].y = (scanY + 1) << 16;
        verts[3].x = cLeft << 16;
        verts[3].y = (scanY + 1) << 16;

        DrawFace(verts, 4, red, green, blue, alpha, inkEffect);
    }
}

static bool ClipLineToTriangle(int *lx0, int *ly0, int *lx1, int *ly1,
                               int32 triTopX, int32 triTopY, int32 triBotLeftX, int32 triBotY, int32 triBotRightX)
{
    float x0 = (float)*lx0, y0 = (float)*ly0;
    float x1 = (float)*lx1, y1 = (float)*ly1;
    float dx = x1 - x0, dy = y1 - y0;
    float tMin = 0.0f, tMax = 1.0f;

    if (fabsf(dx) > 0.001f) {
        float t = ((float)triBotRightX - x0) / dx;
        if (dx > 0) { if (t < tMax) tMax = t; } else { if (t > tMin) tMin = t; }
    }
    else if (x0 > (float)triBotRightX) return false;

    if (fabsf(dy) > 0.001f) {
        float t = ((float)triBotY - y0) / dy;
        if (dy > 0) { if (t < tMax) tMax = t; } else { if (t > tMin) tMin = t; }
    }
    else if (y0 > (float)triBotY) return false;

    if (fabsf(dy) > 0.001f) {
        float t = ((float)triTopY - y0) / dy;
        if (dy < 0) { if (t < tMax) tMax = t; } else { if (t > tMin) tMin = t; }
    }
    else if (y0 < (float)triTopY) return false;

    {
        float A = (float)(triBotY - triTopY);
        float B = (float)(triBotLeftX - triTopX);
        float d0 = (x0 - (float)triTopX) * A - (y0 - (float)triTopY) * B;
        float d1 = (x1 - (float)triTopX) * A - (y1 - (float)triTopY) * B;
        float dd = d1 - d0;
        if (fabsf(dd) > 0.001f) {
            float t = -d0 / dd;
            if (dd < 0) { if (t < tMax) tMax = t; } else { if (t > tMin) tMin = t; }
        }
        else if (d0 < 0) return false;
    }

    if (tMin >= tMax)
        return false;

    *lx0 = (int)(x0 + tMin * dx);
    *ly0 = (int)(y0 + tMin * dy);
    *lx1 = (int)(x0 + tMax * dx);
    *ly1 = (int)(y0 + tMax * dy);
    return true;
}

static void DrawCircleOutlineClippedToTriangle(int cx, int cy, int innerRadius, int outerRadius, uint32 color, int32 alpha, int32 inkEffect,
                                               int32 triTopX, int32 triTopY, int32 triBotLeftX, int32 triBotY, int32 triBotRightX)
{
    int32 width = outerRadius - innerRadius;
    RenderDevice::SetLinePolyThickness(width);

    const int segs = 64;
    const float step = 2.0f * (float)M_PI / (float)segs;

    if (inkEffect == INK_BLEND) {
        inkEffect = INK_NONE;
        alpha = 0xFF;
    }

    int x0 = cx + outerRadius;
    int y0 = cy;

    float angle = step;
    for (int i = 1; i <= segs; i++) {
        int x1 = cx + (int)((float)outerRadius * cosf(angle));
        int y1 = cy + (int)((float)outerRadius * sinf(angle));

        int cx0 = x0, cy0 = y0, cx1 = x1, cy1 = y1;
        if (ClipLineToTriangle(&cx0, &cy0, &cx1, &cy1, triTopX, triTopY, triBotLeftX, triBotY, triBotRightX))
            DrawLine(cx0, cy0, cx1, cy1, color, alpha, inkEffect, true);

        x0 = x1;
        y0 = y1;
        angle += step;
    }

    RenderDevice::SetLinePolyThickness(2);
}

void RSDK::DrawCircleClipped(int32 x, int32 y, int32 radius, uint32 color, int32 alpha, int32 inkEffect, bool32 screenRelative,
                             int32 triTopX, int32 triTopY, int32 triBotLeftX, int32 triBotY, int32 triBotRightX)
{
    if (radius <= 0)
        return;

    switch (inkEffect) {
        default: break;
        case INK_ALPHA:
            if (alpha > 0xFF) inkEffect = INK_NONE;
            else if (alpha <= 0) return;
            break;
        case INK_ADD:
        case INK_SUB:
            if (alpha > 0xFF) alpha = 0xFF;
            else if (alpha <= 0) return;
            break;
    }

    if (!screenRelative) {
        x = FROM_FIXED(x) - currentScreen->position.x;
        y = FROM_FIXED(y) - currentScreen->position.y;
    }

    DrawFilledCircleClippedToTriangle(x, y, radius, color, alpha, inkEffect,
                                     triTopX, triTopY, triBotLeftX, triBotY, triBotRightX);
}

void RSDK::DrawCircleOutlineClipped(int32 x, int32 y, int32 innerRadius, int32 outerRadius, uint32 color, int32 alpha, int32 inkEffect,
                                    bool32 screenRelative, int32 triTopX, int32 triTopY, int32 triBotLeftX, int32 triBotY, int32 triBotRightX)
{
    if (outerRadius <= 0)
        return;

    switch (inkEffect) {
        default: break;
        case INK_ALPHA:
            if (alpha > 0xFF) inkEffect = INK_NONE;
            else if (alpha <= 0) return;
            break;
        case INK_ADD:
        case INK_SUB:
            if (alpha > 0xFF) alpha = 0xFF;
            else if (alpha <= 0) return;
            break;
    }

    if (!screenRelative) {
        x = FROM_FIXED(x) - currentScreen->position.x;
        y = FROM_FIXED(y) - currentScreen->position.y;
    }

    DrawCircleOutlineClippedToTriangle(x, y, innerRadius, outerRadius, color, alpha, inkEffect,
                                      triTopX, triTopY, triBotLeftX, triBotY, triBotRightX);
}

float RSDK::DepthGet(void) { return RenderDevice::GetDepth(); }

void RSDK::DepthSet(float depth) { RenderDevice::SetDepth(depth); }
#endif
void RSDK::DrawCircle(int32 x, int32 y, int32 radius, uint32 color, int32 alpha, int32 inkEffect, bool32 screenRelative)
{
    if (radius > 0) {
        switch (inkEffect) {
            default: break;
            case INK_ALPHA:
                if (alpha > 0xFF)
                    inkEffect = INK_NONE;
                else if (alpha <= 0)
                    return;
                break;

            case INK_ADD:
            case INK_SUB:
                if (alpha > 0xFF)
                    alpha = 0xFF;
                else if (alpha <= 0)
                    return;
                break;

            case INK_TINT:
                if (!tintLookupTable)
                    return;
                break;
        }

        if (!screenRelative) {
            x = FROM_FIXED(x) - currentScreen->position.x;
            y = FROM_FIXED(y) - currentScreen->position.y;
        }

#if RETRO_PLATFORM == RETRO_KALLISTIOS && defined(KOS_HARDWARE_RENDERER)
        DrawFilledCircleFromTris(x, y, radius, color, alpha, inkEffect);
#else
        int32 yRadiusBottom = y + radius;
        int32 bottom        = yRadiusBottom + 1;
        int32 yRadiusTop    = y - radius;
        int32 top           = yRadiusTop;

        if (top < currentScreen->clipBound_Y1)
            top = currentScreen->clipBound_Y1;
        else if (top > currentScreen->clipBound_Y2)
            top = currentScreen->clipBound_Y2;

        if (bottom < currentScreen->clipBound_Y1)
            bottom = currentScreen->clipBound_Y1;
        else if (bottom > currentScreen->clipBound_Y2)
            bottom = currentScreen->clipBound_Y2;

        if (top != bottom) {
            for (int32 i = top; i < bottom; ++i) {
                scanEdgeBuffer[i].start = 0x7FFF;
                scanEdgeBuffer[i].end   = -1;
            }

            int32 r                  = 3 - 2 * radius;
            int32 xRad               = x - radius;
            int32 curY               = y;
            int32 curX               = x;
            int32 dist               = x - y;

            for (int32 i = 0; i <= radius; ++i) {
                int32 scanX = i + curX;

                if (yRadiusBottom >= top && yRadiusBottom <= bottom && scanX > scanEdgeBuffer[yRadiusBottom].end)
                    scanEdgeBuffer[yRadiusBottom].end = scanX;

                if (yRadiusTop >= top && yRadiusTop <= bottom && scanX > scanEdgeBuffer[yRadiusTop].end)
                    scanEdgeBuffer[yRadiusTop].end = scanX;

                int32 scanY = i + y;
                if (scanY >= top && scanY <= bottom) {
                    ScanEdge *edge = &scanEdgeBuffer[scanY];
                    if (yRadiusBottom + dist > edge->end)
                        edge->end = yRadiusBottom + dist;
                }

                if (curY >= top && curY <= bottom && yRadiusBottom + dist > scanEdgeBuffer[curY].end)
                    scanEdgeBuffer[curY].end = yRadiusBottom + dist;

                if (yRadiusBottom >= top && yRadiusBottom <= bottom && x < scanEdgeBuffer[yRadiusBottom].start)
                    scanEdgeBuffer[yRadiusBottom].start = x;

                if (yRadiusTop >= top && yRadiusTop <= bottom && x < scanEdgeBuffer[yRadiusTop].start)
                    scanEdgeBuffer[yRadiusTop].start = x;

                if (scanY >= top && scanY <= bottom) {
                    ScanEdge *edge = &scanEdgeBuffer[scanY];
                    if (xRad < edge->start)
                        edge->start = xRad;
                }

                if (curY >= top && curY <= bottom && xRad < scanEdgeBuffer[curY].start)
                    scanEdgeBuffer[curY].start = xRad;

                if (r >= 0) {
                    --yRadiusBottom;
                    ++yRadiusTop;
                    r += 4 * (i - radius) + 10;
                    --radius;
                    ++xRad;
                }
                else {
                    r += 4 * i + 6;
                }
                --curY;
                --x;
            }

            // validDraw              = true;
            uint16 *frameBuffer = &currentScreen->frameBuffer[top * currentScreen->pitch];
#if RETRO_PLATFORM != RETRO_KALLISTIOS || RETRO_USE_ORIGINAL_CODE
            uint16 color16      = rgb32To16_B[(color >> 0) & 0xFF] | rgb32To16_G[(color >> 8) & 0xFF] | rgb32To16_R[(color >> 16) & 0xFF];
#else
            uint16 color16 = PACK_RGB888_32(color);
#endif

            switch (inkEffect) {
                default: break;
                case INK_NONE:
                    if (top <= bottom) {
                        ScanEdge *edge = &scanEdgeBuffer[top];
                        int32 sizeY    = bottom - top;

                        for (int32 y = 0; y < sizeY; ++y) {
                            if (edge->start < currentScreen->clipBound_X1)
                                edge->start = currentScreen->clipBound_X1;
                            if (edge->start > currentScreen->clipBound_X2)
                                edge->start = currentScreen->clipBound_X2;

                            if (edge->end < currentScreen->clipBound_X1)
                                edge->end = currentScreen->clipBound_X1;
                            if (edge->end > currentScreen->clipBound_X2)
                                edge->end = currentScreen->clipBound_X2;

                            int32 count = edge->end - edge->start;
                            for (int32 x = 0; x < count; ++x) {
                                frameBuffer[edge->start + x] = color16;
                            }
                            ++edge;
                            frameBuffer += currentScreen->pitch;
                        }
                    }
                    break;

                case INK_BLEND:
                    if (top <= bottom) {
                        ScanEdge *edge = &scanEdgeBuffer[top];
                        int32 sizeY    = bottom - top;

                        for (int32 y = 0; y < sizeY; ++y) {
                            if (edge->start < currentScreen->clipBound_X1)
                                edge->start = currentScreen->clipBound_X1;
                            if (edge->start > currentScreen->clipBound_X2)
                                edge->start = currentScreen->clipBound_X2;

                            if (edge->end < currentScreen->clipBound_X1)
                                edge->end = currentScreen->clipBound_X1;
                            if (edge->end > currentScreen->clipBound_X2)
                                edge->end = currentScreen->clipBound_X2;

                            int32 count = edge->end - edge->start;
                            for (int32 x = 0; x < count; ++x) {
                                setPixelBlend(color16, frameBuffer[edge->start + x]);
                            }

                            ++edge;
                            frameBuffer += currentScreen->pitch;
                        }
                    }
                    break;

                case INK_ALPHA:
                    if (top <= bottom) {
                        uint16 *fbufferBlend = &blendLookupTable[0x20 * (0xFF - alpha)];
                        uint16 *pixelBlend   = &blendLookupTable[0x20 * alpha];

                        ScanEdge *edge = &scanEdgeBuffer[top];
                        int32 sizeY    = bottom - top;

                        for (int32 y = 0; y < sizeY; ++y) {
                            if (edge->start < currentScreen->clipBound_X1)
                                edge->start = currentScreen->clipBound_X1;
                            if (edge->start > currentScreen->clipBound_X2)
                                edge->start = currentScreen->clipBound_X2;

                            if (edge->end < currentScreen->clipBound_X1)
                                edge->end = currentScreen->clipBound_X1;
                            if (edge->end > currentScreen->clipBound_X2)
                                edge->end = currentScreen->clipBound_X2;

                            int32 count = edge->end - edge->start;
                            for (int32 x = 0; x < count; ++x) {
                                setPixelAlpha(color16, frameBuffer[edge->start + x], alpha);
                            }
                            ++edge;
                            frameBuffer += currentScreen->pitch;
                        }
                    }
                    break;

                case INK_ADD: {
                    uint16 *blendTablePtr = &blendLookupTable[0x20 * alpha];
                    if (top <= bottom) {
                        ScanEdge *edge = &scanEdgeBuffer[top];
                        int32 sizeY    = bottom - top;

                        for (int32 y = 0; y < sizeY; ++y) {
                            if (edge->start < currentScreen->clipBound_X1)
                                edge->start = currentScreen->clipBound_X1;
                            if (edge->start > currentScreen->clipBound_X2)
                                edge->start = currentScreen->clipBound_X2;

                            if (edge->end < currentScreen->clipBound_X1)
                                edge->end = currentScreen->clipBound_X1;
                            if (edge->end > currentScreen->clipBound_X2)
                                edge->end = currentScreen->clipBound_X2;

                            int32 count = edge->end - edge->start;
                            for (int32 x = 0; x < count; ++x) {
                                setPixelAdditive(color16, frameBuffer[edge->start + x]);
                            }
                            ++edge;
                            frameBuffer += currentScreen->pitch;
                        }
                    }
                    break;
                }

                case INK_SUB: {
                    uint16 *subBlendTable = &subtractLookupTable[0x20 * alpha];
                    if (top <= bottom) {
                        ScanEdge *edge = &scanEdgeBuffer[top];
                        int32 sizeY    = bottom - top;

                        for (int32 y = 0; y < sizeY; ++y) {
                            if (edge->start < currentScreen->clipBound_X1)
                                edge->start = currentScreen->clipBound_X1;
                            if (edge->start > currentScreen->clipBound_X2)
                                edge->start = currentScreen->clipBound_X2;

                            if (edge->end < currentScreen->clipBound_X1)
                                edge->end = currentScreen->clipBound_X1;
                            if (edge->end > currentScreen->clipBound_X2)
                                edge->end = currentScreen->clipBound_X2;

                            int32 count = edge->end - edge->start;
                            for (int32 x = 0; x < count; ++x) {
                                setPixelSubtractive(color16, frameBuffer[edge->start + x]);
                            }
                            ++edge;
                            frameBuffer += currentScreen->pitch;
                        }
                    }
                    break;
                }

                case INK_TINT:
                    if (top <= bottom) {
                        ScanEdge *edge = &scanEdgeBuffer[top];
                        int32 sizeY    = bottom - top;

                        for (int32 y = 0; y < sizeY; ++y) {
                            if (edge->start < currentScreen->clipBound_X1)
                                edge->start = currentScreen->clipBound_X1;
                            if (edge->start > currentScreen->clipBound_X2)
                                edge->start = currentScreen->clipBound_X2;

                            if (edge->end < currentScreen->clipBound_X1)
                                edge->end = currentScreen->clipBound_X1;
                            if (edge->end > currentScreen->clipBound_X2)
                                edge->end = currentScreen->clipBound_X2;

                            int32 count = edge->end - edge->start;
                            for (int32 x = 0; x < count; ++x) {
                                frameBuffer[edge->start + x] = tintLookupTable[frameBuffer[edge->start + x]];
                            }
                            ++edge;
                            frameBuffer += currentScreen->pitch;
                        }
                    }
                    break;

                case INK_MASKED:
                    if (top <= bottom) {
                        ScanEdge *edge = &scanEdgeBuffer[top];
                        int32 sizeY    = bottom - top;

                        for (int32 y = 0; y < sizeY; ++y) {
                            if (edge->start < currentScreen->clipBound_X1)
                                edge->start = currentScreen->clipBound_X1;
                            if (edge->start > currentScreen->clipBound_X2)
                                edge->start = currentScreen->clipBound_X2;

                            if (edge->end < currentScreen->clipBound_X1)
                                edge->end = currentScreen->clipBound_X1;
                            if (edge->end > currentScreen->clipBound_X2)
                                edge->end = currentScreen->clipBound_X2;

                            int32 count = edge->end - edge->start;
                            for (int32 x = 0; x < count; ++x) {
                                if (frameBuffer[edge->start + x] == maskColor)
                                    frameBuffer[edge->start + x] = color16;
                            }
                            ++edge;
                            frameBuffer += currentScreen->pitch;
                        }
                    }
                    break;

                case INK_UNMASKED:
                    if (top <= bottom) {
                        ScanEdge *edge = &scanEdgeBuffer[top];
                        int32 sizeY    = bottom - top;

                        for (int32 y = 0; y < sizeY; ++y) {
                            if (edge->start < currentScreen->clipBound_X1)
                                edge->start = currentScreen->clipBound_X1;
                            if (edge->start > currentScreen->clipBound_X2)
                                edge->start = currentScreen->clipBound_X2;

                            if (edge->end < currentScreen->clipBound_X1)
                                edge->end = currentScreen->clipBound_X1;
                            if (edge->end > currentScreen->clipBound_X2)
                                edge->end = currentScreen->clipBound_X2;

                            int32 count = edge->end - edge->start;
                            for (int32 x = 0; x < count; ++x) {
                                if (frameBuffer[edge->start + x] != maskColor)
                                    frameBuffer[edge->start + x] = color16;
                            }
                            ++edge;
                            frameBuffer += currentScreen->pitch;
                        }
                    }
                    break;
            }
        }
#endif
    }
}
void RSDK::DrawCircleOutline(int32 x, int32 y, int32 innerRadius, int32 outerRadius, uint32 color, int32 alpha, int32 inkEffect,
                             bool32 screenRelative)
{
    switch (inkEffect) {
        default: break;
        case INK_ALPHA:
            if (alpha > 0xFF)
                inkEffect = INK_NONE;
            else if (alpha <= 0)
                return;
            break;

        case INK_ADD:
        case INK_SUB:
            if (alpha > 0xFF)
                alpha = 0xFF;
            else if (alpha <= 0)
                return;
            break;

        case INK_TINT:
            if (!tintLookupTable)
                return;
            break;
    }

    if (!screenRelative) {
        x = FROM_FIXED(x) - currentScreen->position.x;
        y = FROM_FIXED(y) - currentScreen->position.y;
    }

    if (outerRadius > 0 && innerRadius < outerRadius) {
        int32 top    = y - outerRadius;
        int32 left   = x - outerRadius;
        int32 right  = x + outerRadius;
        int32 bottom = y + outerRadius;

        if (left < currentScreen->clipBound_X1)
            left = currentScreen->clipBound_X1;
        if (left > currentScreen->clipBound_X2)
            left = currentScreen->clipBound_X2;

        if (right < currentScreen->clipBound_X1)
            right = currentScreen->clipBound_X1;
        if (right > currentScreen->clipBound_X2)
            right = currentScreen->clipBound_X2;

        if (top < currentScreen->clipBound_Y1)
            top = currentScreen->clipBound_Y1;
        if (top > currentScreen->clipBound_Y1)
            top = currentScreen->clipBound_Y1;

        if (bottom < currentScreen->clipBound_Y2)
            bottom = currentScreen->clipBound_Y2;
        if (bottom > currentScreen->clipBound_Y2)
            bottom = currentScreen->clipBound_Y2;

        if (left != right && top != bottom) {
            int32 ir2           = innerRadius * innerRadius;
            int32 or2           = outerRadius * outerRadius;
            validDraw           = true;

#if RETRO_PLATFORM == RETRO_KALLISTIOS && defined(KOS_HARDWARE_RENDERER)
            DrawCircleOutlineWithLines(x, y, outerRadius, (outerRadius - innerRadius), color, alpha, inkEffect);
#else
            uint16 *frameBuffer = &currentScreen->frameBuffer[left + top * currentScreen->pitch];
#if RETRO_PLATFORM != RETRO_KALLISTIOS || RETRO_USE_ORIGINAL_CODE
            uint16 color16      = rgb32To16_B[(color >> 0) & 0xFF] | rgb32To16_G[(color >> 8) & 0xFF] | rgb32To16_R[(color >> 16) & 0xFF];
#else
            uint16 color16 = PACK_RGB888_32(color);
#endif
            int32 pitch         = (left + currentScreen->pitch - right);

            switch (inkEffect) {
                default: break;
                case INK_NONE:
                    if (top < bottom) {
                        int32 distY1 = top - y;
                        int32 distY2 = bottom - top;
                        do {
                            int32 y2 = distY1 * distY1;
                            if (left < right) {
                                int32 distX1 = left - x;
                                int32 distX2 = right - left;
                                do {
                                    int32 r2 = y2 + distX1 * distX1;
                                    if (r2 >= ir2 && r2 < or2)
                                        *frameBuffer = color16;
                                    ++frameBuffer;
                                    ++distX1;
                                    --distX2;
                                } while (distX2);
                            }
                            frameBuffer += pitch;
                            --distY2;
                            ++distY1;
                        } while (distY2);
                    }
                    break;

                case INK_BLEND:
                    if (top < bottom) {
                        int32 distY1 = top - y;
                        int32 distY2 = bottom - top;
                        do {
                            int32 y2 = distY1 * distY1;
                            if (left < right) {
                                int32 distX1 = left - x;
                                int32 distX2 = right - left;
                                do {
                                    int32 r2 = y2 + distX1 * distX1;
                                    if (r2 >= ir2 && r2 < or2)
                                        setPixelBlend(color16, *frameBuffer);
                                    ++frameBuffer;
                                    ++distX1;
                                    --distX2;
                                } while (distX2);
                            }
                            frameBuffer += pitch;
                            --distY2;
                            ++distY1;
                        } while (distY2);
                    }
                    break;

                case INK_ALPHA:
                    if (top < bottom) {
                        uint16 *fbufferBlend = &blendLookupTable[0x20 * (0xFF - alpha)];
                        uint16 *pixelBlend   = &blendLookupTable[0x20 * alpha];

                        int32 distY1 = top - y;
                        int32 distY2 = bottom - top;
                        do {
                            int32 y2 = distY1 * distY1;
                            if (left < right) {
                                int32 distX1 = left - x;
                                int32 distX2 = right - left;
                                do {
                                    int32 r2 = y2 + distX1 * distX1;
                                    if (r2 >= ir2 && r2 < or2) {
                                        setPixelAlpha(color16, *frameBuffer, alpha);
                                    }
                                    ++frameBuffer;
                                    ++distX1;
                                    --distX2;
                                } while (distX2);
                            }
                            frameBuffer += pitch;
                            --distY2;
                            ++distY1;
                        } while (distY2);
                    }
                    break;

                case INK_ADD: {
                    uint16 *blendTablePtr = &blendLookupTable[0x20 * alpha];
                    if (top < bottom) {
                        int32 distY1 = top - y;
                        int32 distY2 = bottom - top;
                        do {
                            int32 y2 = distY1 * distY1;
                            if (left < right) {
                                int32 distX1 = left - x;
                                int32 distX2 = right - left;
                                do {
                                    int32 r2 = y2 + distX1 * distX1;
                                    if (r2 >= ir2 && r2 < or2) {
                                        setPixelAdditive(color16, *frameBuffer);
                                    }
                                    ++frameBuffer;
                                    ++distX1;
                                    --distX2;
                                } while (distX2);
                            }
                            frameBuffer += pitch;
                            --distY2;
                            ++distY1;
                        } while (distY2);
                    }
                    break;
                }

                case INK_SUB: {
                    uint16 *subBlendTable = &subtractLookupTable[0x20 * alpha];
                    if (top < bottom) {
                        int32 distY1 = top - y;
                        int32 distY2 = bottom - top;
                        do {
                            int32 y2 = distY1 * distY1;
                            if (left < right) {
                                int32 distX1 = left - x;
                                int32 distX2 = right - left;
                                do {
                                    int32 r2 = y2 + distX1 * distX1;
                                    if (r2 >= ir2 && r2 < or2) {
                                        setPixelSubtractive(color16, *frameBuffer);
                                    }
                                    ++frameBuffer;
                                    ++distX1;
                                    --distX2;
                                } while (distX2);
                            }
                            frameBuffer += pitch;
                            --distY2;
                            ++distY1;
                        } while (distY2);
                    }
                    break;
                }

                case INK_TINT:
                    if (top < bottom) {
                        int32 distY1 = top - y;
                        int32 distY2 = bottom - top;
                        do {
                            int32 y2 = distY1 * distY1;
                            if (left < right) {
                                int32 distX1 = left - x;
                                int32 distX2 = right - left;
                                do {
                                    int32 r2 = y2 + distX1 * distX1;
                                    if (r2 >= ir2 && r2 < or2)
                                        *frameBuffer = tintLookupTable[*frameBuffer];
                                    ++frameBuffer;
                                    ++distX1;
                                    --distX2;
                                } while (distX2);
                            }
                            frameBuffer += pitch;
                            --distY2;
                            ++distY1;
                        } while (distY2);
                    }
                    break;

                case INK_MASKED:
                    if (top < bottom) {
                        int32 distY1 = top - y;
                        int32 distY2 = bottom - top;
                        do {
                            int32 y2 = distY1 * distY1;
                            if (left < right) {
                                int32 distX1 = left - x;
                                int32 distX2 = right - left;
                                do {
                                    int32 r2 = y2 + distX1 * distX1;
                                    if (r2 >= ir2 && r2 < or2 && *frameBuffer == maskColor)
                                        *frameBuffer = color16;
                                    ++frameBuffer;
                                    ++distX1;
                                    --distX2;
                                } while (distX2);
                            }
                            frameBuffer += pitch;
                            --distY2;
                            ++distY1;
                        } while (distY2);
                    }
                    break;

                case INK_UNMASKED:
                    if (top < bottom) {
                        int32 distY1 = top - y;
                        int32 distY2 = bottom - top;
                        do {
                            int32 y2 = distY1 * distY1;
                            if (left < right) {
                                int32 distX1 = left - x;
                                int32 distX2 = right - left;
                                do {
                                    int32 r2 = y2 + distX1 * distX1;
                                    if (r2 >= ir2 && r2 < or2 && *frameBuffer != maskColor)
                                        *frameBuffer = color16;
                                    ++frameBuffer;
                                    ++distX1;
                                    --distX2;
                                } while (distX2);
                            }
                            frameBuffer += pitch;
                            --distY2;
                            ++distY1;
                        } while (distY2);
                    }
                    break;
            }
#endif
        }
    }
}

#if RETRO_PLATFORM == RETRO_KALLISTIOS && defined(KOS_HARDWARE_RENDERER)
void RSDK::Draw3DFace(Vector4f *vertices, int32 vertCount, int32 r, int32 g, int32 b, int32 alpha, int32 inkEffect)
{
    if (!RenderDevice::SupportedInk(inkEffect)) {
        return;
    }

    validDraw = true;

    switch (inkEffect) {
        default: break;
        case INK_ALPHA:
            if (alpha > 0xFF)
                inkEffect = INK_NONE;
            else if (alpha <= 0)
                return;
            break;

        case INK_ADD:
        case INK_SUB:
            if (alpha > 0xFF)
                alpha = 0xFF;
            else if (alpha <= 0)
                return;
            break;

        case INK_TINT:
            if (!tintLookupTable)
                return;
            break;
    }

    if (inkEffect == INK_NONE) {
        alpha = 0xFF;
    }

    if ((inkEffect != INK_TINT) && (inkEffect != INK_ADD) && (alpha == 0xFF)) {
        RenderDevice::PrepareFacePolyPT(inkEffect);
        RenderDevice::Draw3DFacePolyPT(vertices, vertCount, ((r << 16) | (g << 8) | b), alpha, NULL);
    } else {
        RenderDevice::PrepareFacePolyTR(inkEffect);
        RenderDevice::Draw3DFacePolyTR(vertices, vertCount, ((r << 16) | (g << 8) | b), alpha, NULL);
    }
}
#endif
void RSDK::DrawFace(Vector2 *vertices, int32 vertCount, int32 r, int32 g, int32 b, int32 alpha, int32 inkEffect)
{
#if RETRO_PLATFORM == RETRO_KALLISTIOS && defined(KOS_HARDWARE_RENDERER)
    if (!RenderDevice::SupportedInk(inkEffect)) {
        return;
    }
#endif

    validDraw = true;

    switch (inkEffect) {
        default: break;
        case INK_ALPHA:
            if (alpha > 0xFF)
                inkEffect = INK_NONE;
            else if (alpha <= 0)
                return;
            break;

        case INK_ADD:
        case INK_SUB:
            if (alpha > 0xFF)
                alpha = 0xFF;
            else if (alpha <= 0)
                return;
            break;

        case INK_TINT:
            if (!tintLookupTable)
                return;
            break;
    }

    if (inkEffect == INK_NONE) {
        alpha = 0xFF;
    }

#if RETRO_PLATFORM == RETRO_KALLISTIOS && defined(KOS_HARDWARE_RENDERER)
    // DC_DESATURATE: desaturate face colors when paused, but not for the pause menu's own draw group
    uint8 desat = RenderDevice::GetPaletteDesaturation();
    if (desat > 0 && sceneInfo.currentDrawGroup < DRAWGROUP_COUNT - 1) {
        uint32 dc = DesaturateColor32((r << 16) | (g << 8) | b, desat);
        r = (dc >> 16) & 0xFF; g = (dc >> 8) & 0xFF; b = dc & 0xFF;
    }

    if ((inkEffect != INK_TINT) && (inkEffect != INK_ADD) && (alpha == 0xFF)) {
        RenderDevice::PrepareFacePolyPT(inkEffect);
        RenderDevice::DrawFacePolyPT(vertices, vertCount, ((r << 16) | (g << 8) | b), alpha, NULL);
    } else {
        RenderDevice::PrepareFacePolyTR(inkEffect);
        RenderDevice::DrawFacePolyTR(vertices, vertCount, ((r << 16) | (g << 8) | b), alpha, NULL);
    }
#else
    int32 top    = 0x7FFFFFFF;
    int32 bottom = -0x10000;
    for (int32 v = 0; v < vertCount; ++v) {
        if (vertices[v].y < top)
            top = vertices[v].y;
        if (vertices[v].y > bottom)
            bottom = vertices[v].y;
    }

    int32 topScreen    = FROM_FIXED(top);
    int32 bottomScreen = FROM_FIXED(bottom);

    if (topScreen < currentScreen->clipBound_Y1)
        topScreen = currentScreen->clipBound_Y1;
    if (topScreen > currentScreen->clipBound_Y2)
        topScreen = currentScreen->clipBound_Y2;

    if (bottomScreen < currentScreen->clipBound_Y1)
        bottomScreen = currentScreen->clipBound_Y1;
    if (bottomScreen > currentScreen->clipBound_Y2)
        bottomScreen = currentScreen->clipBound_Y2;

    if (topScreen != bottomScreen) {
        ScanEdge *edge = &scanEdgeBuffer[topScreen];
        for (int32 s = topScreen; s <= bottomScreen; ++s) {
            edge->start = 0x7FFF;
            edge->end   = -1;
            ++edge;
        }

        for (int32 v = 0; v < vertCount - 1; ++v) {
            ProcessScanEdge(vertices[v + 0].x, vertices[v + 0].y, vertices[v + 1].x, vertices[v + 1].y);
        }
        ProcessScanEdge(vertices[0].x, vertices[0].y, vertices[vertCount - 1].x, vertices[vertCount - 1].y);

        uint16 *frameBuffer = &currentScreen->frameBuffer[topScreen * currentScreen->pitch];
#if RETRO_PLATFORM != RETRO_KALLISTIOS || RETRO_USE_ORIGINAL_CODE
        uint16 color16      = rgb32To16_B[b] | rgb32To16_G[g] | rgb32To16_R[r];
#else
        uint16 color16 = PACK_RGB888(r, g, b);
#endif

        edge = &scanEdgeBuffer[topScreen];
        switch (inkEffect) {
            default: break;

            case INK_NONE:
                for (int32 s = topScreen; s <= bottomScreen; ++s) {
                    if (edge->start < currentScreen->clipBound_X1)
                        edge->start = currentScreen->clipBound_X1;
                    if (edge->start > currentScreen->clipBound_X2)
                        edge->start = currentScreen->clipBound_X2;

                    if (edge->end < currentScreen->clipBound_X1)
                        edge->end = currentScreen->clipBound_X1;
                    if (edge->end > currentScreen->clipBound_X2)
                        edge->end = currentScreen->clipBound_X2;

                    int32 count = edge->end - edge->start;
                    for (int32 x = 0; x < count; ++x) {
                        frameBuffer[edge->start + x] = color16;
                    }
                    ++edge;
                    frameBuffer += currentScreen->pitch;
                }
                break;

            case INK_BLEND:
                for (int32 s = topScreen; s <= bottomScreen; ++s) {
                    if (edge->start < currentScreen->clipBound_X1)
                        edge->start = currentScreen->clipBound_X1;
                    if (edge->start > currentScreen->clipBound_X2)
                        edge->start = currentScreen->clipBound_X2;

                    if (edge->end < currentScreen->clipBound_X1)
                        edge->end = currentScreen->clipBound_X1;
                    if (edge->end > currentScreen->clipBound_X2)
                        edge->end = currentScreen->clipBound_X2;

                    int32 count = edge->end - edge->start;
                    for (int32 x = 0; x < count; ++x) {
                        setPixelBlend(color16, frameBuffer[edge->start + x]);
                    }
                    ++edge;
                    frameBuffer += currentScreen->pitch;
                }
                break;

            case INK_ALPHA: {
                uint16 *fbufferBlend = &blendLookupTable[0x20 * (0xFF - alpha)];
                uint16 *pixelBlend   = &blendLookupTable[0x20 * alpha];

                for (int32 s = topScreen; s <= bottomScreen; ++s) {
                    if (edge->start < currentScreen->clipBound_X1)
                        edge->start = currentScreen->clipBound_X1;
                    if (edge->start > currentScreen->clipBound_X2)
                        edge->start = currentScreen->clipBound_X2;

                    if (edge->end < currentScreen->clipBound_X1)
                        edge->end = currentScreen->clipBound_X1;
                    if (edge->end > currentScreen->clipBound_X2)
                        edge->end = currentScreen->clipBound_X2;

                    int32 count = edge->end - edge->start;
                    for (int32 x = 0; x < count; ++x) {
                        setPixelAlpha(color16, frameBuffer[edge->start + x], alpha);
                    }
                    ++edge;
                    frameBuffer += currentScreen->pitch;
                }
                break;
            }

            case INK_ADD: {
                uint16 *blendTablePtr = &blendLookupTable[0x20 * alpha];

                for (int32 s = topScreen; s <= bottomScreen; ++s) {
                    if (edge->start < currentScreen->clipBound_X1)
                        edge->start = currentScreen->clipBound_X1;
                    if (edge->start > currentScreen->clipBound_X2)
                        edge->start = currentScreen->clipBound_X2;

                    if (edge->end < currentScreen->clipBound_X1)
                        edge->end = currentScreen->clipBound_X1;
                    if (edge->end > currentScreen->clipBound_X2)
                        edge->end = currentScreen->clipBound_X2;

                    int32 count = edge->end - edge->start;
                    for (int32 x = 0; x < count; ++x) {
                        setPixelAdditive(color16, frameBuffer[edge->start + x]);
                    }

                    ++edge;
                    frameBuffer += currentScreen->pitch;
                }
                break;
            }

            case INK_SUB: {
                uint16 *subBlendTable = &subtractLookupTable[0x20 * alpha];
                for (int32 s = topScreen; s <= bottomScreen; ++s) {
                    if (edge->start < currentScreen->clipBound_X1)
                        edge->start = currentScreen->clipBound_X1;
                    if (edge->start > currentScreen->clipBound_X2)
                        edge->start = currentScreen->clipBound_X2;

                    if (edge->end < currentScreen->clipBound_X1)
                        edge->end = currentScreen->clipBound_X1;
                    if (edge->end > currentScreen->clipBound_X2)
                        edge->end = currentScreen->clipBound_X2;

                    int32 count = edge->end - edge->start;
                    for (int32 x = 0; x < count; ++x) {
                        setPixelSubtractive(color16, frameBuffer[edge->start + x]);
                    }

                    ++edge;
                    frameBuffer += currentScreen->pitch;
                }
                break;
            }

            case INK_TINT:
                for (int32 s = topScreen; s <= bottomScreen; ++s) {
                    if (edge->start < currentScreen->clipBound_X1)
                        edge->start = currentScreen->clipBound_X1;
                    if (edge->start > currentScreen->clipBound_X2)
                        edge->start = currentScreen->clipBound_X2;

                    if (edge->end < currentScreen->clipBound_X1)
                        edge->end = currentScreen->clipBound_X1;
                    if (edge->end > currentScreen->clipBound_X2)
                        edge->end = currentScreen->clipBound_X2;

                    int32 count = edge->end - edge->start;
                    for (int32 x = 0; x < count; ++x) {
                        frameBuffer[edge->start + x] = tintLookupTable[frameBuffer[edge->start + x]];
                    }

                    ++edge;
                    frameBuffer += currentScreen->pitch;
                }
                break;

            case INK_MASKED:
                for (int32 s = topScreen; s <= bottomScreen; ++s) {
                    if (edge->start < currentScreen->clipBound_X1)
                        edge->start = currentScreen->clipBound_X1;
                    if (edge->start > currentScreen->clipBound_X2)
                        edge->start = currentScreen->clipBound_X2;

                    if (edge->end < currentScreen->clipBound_X1)
                        edge->end = currentScreen->clipBound_X1;
                    if (edge->end > currentScreen->clipBound_X2)
                        edge->end = currentScreen->clipBound_X2;

                    int32 count = edge->end - edge->start;
                    for (int32 x = 0; x < count; ++x) {
                        if (frameBuffer[edge->start + x] == maskColor)
                            frameBuffer[edge->start + x] = color16;
                    }

                    ++edge;
                    frameBuffer += currentScreen->pitch;
                }
                break;

            case INK_UNMASKED:
                for (int32 s = topScreen; s <= bottomScreen; ++s) {
                    if (edge->start < currentScreen->clipBound_X1)
                        edge->start = currentScreen->clipBound_X1;
                    if (edge->start > currentScreen->clipBound_X2)
                        edge->start = currentScreen->clipBound_X2;

                    if (edge->end < currentScreen->clipBound_X1)
                        edge->end = currentScreen->clipBound_X1;
                    if (edge->end > currentScreen->clipBound_X2)
                        edge->end = currentScreen->clipBound_X2;

                    int32 count = edge->end - edge->start;
                    for (int32 x = 0; x < count; ++x) {
                        if (frameBuffer[edge->start + x] != maskColor)
                            frameBuffer[edge->start + x] = color16;
                    }

                    ++edge;
                    frameBuffer += currentScreen->pitch;
                }
                break;
        }
    }
#endif
}
#if RETRO_PLATFORM == RETRO_KALLISTIOS && defined(KOS_HARDWARE_RENDERER)
void RSDK::Draw3DBlendedFace(Vector4f *vertices, uint32 *colors, int32 vertCount, int32 alpha, int32 inkEffect)
{
    if (!RenderDevice::SupportedInk(inkEffect)) {
        return;
    }

    validDraw = true;

    switch (inkEffect) {
        default: break;
        case INK_ALPHA:
            if (alpha > 0xFF)
                inkEffect = INK_NONE;
            else if (alpha <= 0)
                return;
            break;

        case INK_ADD:
        case INK_SUB:
            if (alpha > 0xFF)
                alpha = 0xFF;
            else if (alpha <= 0)
                return;
            break;

        case INK_TINT:
            if (!tintLookupTable)
                return;
            break;
    }

    if (inkEffect == INK_NONE) {
        alpha = 0xFF;
    }

    if ((inkEffect != INK_ADD) && (alpha == 0xFF)) {
        RenderDevice::PrepareFacePolyPT(inkEffect);
        RenderDevice::Draw3DFacePolyPT(vertices, vertCount, 0, alpha, colors);
    } else {
        RenderDevice::PrepareFacePolyTR(inkEffect);
        RenderDevice::Draw3DFacePolyTR(vertices, vertCount, 0, alpha, colors);
    }
}
#endif
void RSDK::DrawBlendedFace(Vector2 *vertices, uint32 *colors, int32 vertCount, int32 alpha, int32 inkEffect)
{
#if RETRO_PLATFORM == RETRO_KALLISTIOS && defined(KOS_HARDWARE_RENDERER)
    if (!RenderDevice::SupportedInk(inkEffect)) {
        return;
    }
#endif

    validDraw = true;

    switch (inkEffect) {
        default: break;
        case INK_ALPHA:
            if (alpha > 0xFF)
                inkEffect = INK_NONE;
            else if (alpha <= 0)
                return;
            break;

        case INK_ADD:
        case INK_SUB:
            if (alpha > 0xFF)
                alpha = 0xFF;
            else if (alpha <= 0)
                return;
            break;

        case INK_TINT:
            if (!tintLookupTable)
                return;
            break;
    }

    if (inkEffect == INK_NONE) {
        alpha = 0xFF;
    }

#if RETRO_PLATFORM == RETRO_KALLISTIOS && defined(KOS_HARDWARE_RENDERER)
    // DC_DESATURATE: desaturate per-vertex colors when paused, but not for the pause menu's own draw group
    uint8 desat = RenderDevice::GetPaletteDesaturation();
    uint32 localColors[4];
    if (desat > 0 && sceneInfo.currentDrawGroup < DRAWGROUP_COUNT - 1) {
        for (int32 v = 0; v < vertCount; ++v)
            localColors[v] = DesaturateColor32(colors[v], desat);
        colors = localColors;
    }

    if ((inkEffect != INK_ADD) && (alpha == 0xFF)) {
        RenderDevice::PrepareFacePolyPT(inkEffect);
        RenderDevice::DrawFacePolyPT(vertices, vertCount, 0, alpha, colors);
    } else {
        RenderDevice::PrepareFacePolyTR(inkEffect);
        RenderDevice::DrawFacePolyTR(vertices, vertCount, 0, alpha, colors);
    }
#else
    int32 top    = 0x7FFFFFFF;
    int32 bottom = -0x10000;
    for (int32 v = 0; v < vertCount; ++v) {
        if (vertices[v].y < top)
            top = vertices[v].y;
        if (vertices[v].y > bottom)
            bottom = vertices[v].y;
    }

    int32 topScreen    = FROM_FIXED(top);
    int32 bottomScreen = FROM_FIXED(bottom);

    if (topScreen < currentScreen->clipBound_Y1)
        topScreen = currentScreen->clipBound_Y1;
    if (topScreen > currentScreen->clipBound_Y2)
        topScreen = currentScreen->clipBound_Y2;

    if (bottomScreen < currentScreen->clipBound_Y1)
        bottomScreen = currentScreen->clipBound_Y1;
    if (bottomScreen > currentScreen->clipBound_Y2)
        bottomScreen = currentScreen->clipBound_Y2;

    if (topScreen != bottomScreen) {
        ScanEdge *edge = &scanEdgeBuffer[topScreen];
        for (int32 s = topScreen; s <= bottomScreen; ++s) {
            edge->start = 0x7FFF;
            edge->end   = -1;
            ++edge;
        }

        for (int32 v = 0; v < vertCount - 1; ++v) {
            ProcessScanEdgeClr(colors[v + 0], colors[v + 1], vertices[v + 0].x, vertices[v + 0].y, vertices[v + 1].x, vertices[v + 1].y);
        }
        ProcessScanEdgeClr(colors[vertCount - 1], colors[0], vertices[vertCount - 1].x, vertices[vertCount - 1].y, vertices[0].x, vertices[0].y);

        uint16 *frameBuffer = &currentScreen->frameBuffer[topScreen * currentScreen->pitch];

        edge = &scanEdgeBuffer[topScreen];
        switch (inkEffect) {
            default: break;
            case INK_NONE:
                for (int32 s = topScreen; s <= bottomScreen; ++s) {
                    int32 count  = edge->end - edge->start;
                    int32 deltaR = 0;
                    int32 deltaG = 0;
                    int32 deltaB = 0;
                    if (count > 0) {
                        deltaR = (edge->endR - edge->startR) / count;
                        deltaG = (edge->endG - edge->startG) / count;
                        deltaB = (edge->endB - edge->startB) / count;
                    }
                    int32 startR = edge->startR;
                    int32 startG = edge->startG;
                    int32 startB = edge->startB;

                    if (edge->start > currentScreen->clipBound_X2) {
                        edge->start = currentScreen->clipBound_X2;
                    }
                    else if (edge->start < currentScreen->clipBound_X1) {
                        int32 dist = (currentScreen->clipBound_X1 - edge->start);
                        startR += deltaR * dist;
                        startG += deltaG * dist;
                        startB += deltaB * dist;
                        count -= dist;
                        edge->start = currentScreen->clipBound_X1;
                    }

                    if (edge->end < currentScreen->clipBound_X1) {
                        edge->end = currentScreen->clipBound_X1;
                        count     = currentScreen->clipBound_X1 - edge->start;
                    }

                    if (edge->end > currentScreen->clipBound_X2) {
                        edge->end = currentScreen->clipBound_X2;
                        count     = currentScreen->clipBound_X2 - edge->start;
                    }

                    for (int32 x = 0; x < count; ++x) {
                        frameBuffer[edge->start + x] = (startB >> 19) + ((startG >> 13) & 0x7E0) + ((startR >> 8) & 0xF800);

                        startR += deltaR;
                        startG += deltaG;
                        startB += deltaB;
                    }
                    ++edge;
                    frameBuffer += currentScreen->pitch;
                }
                break;

            case INK_BLEND:
                for (int32 s = topScreen; s <= bottomScreen; ++s) {
                    int32 start  = edge->start;
                    int32 count  = edge->end - edge->start;
                    int32 deltaR = 0;
                    int32 deltaG = 0;
                    int32 deltaB = 0;
                    if (count > 0) {
                        deltaR = (edge->endR - edge->startR) / count;
                        deltaG = (edge->endG - edge->startG) / count;
                        deltaB = (edge->endB - edge->startB) / count;
                    }
                    int32 startR = edge->startR;
                    int32 startG = edge->startG;
                    int32 startB = edge->startB;

                    if (start > currentScreen->clipBound_X2) {
                        edge->start = currentScreen->clipBound_X2;
                    }

                    if (start < currentScreen->clipBound_X1) {
                        startR += deltaR * (currentScreen->clipBound_X1 - edge->start);
                        startG += deltaG * (currentScreen->clipBound_X1 - edge->start);
                        startB += deltaB * (currentScreen->clipBound_X1 - edge->start);
                        count -= (currentScreen->clipBound_X1 - edge->start);
                        edge->start = currentScreen->clipBound_X1;
                    }

                    if (edge->end < currentScreen->clipBound_X1) {
                        edge->end = currentScreen->clipBound_X1;
                    }

                    if (edge->end > currentScreen->clipBound_X2) {
                        edge->end = currentScreen->clipBound_X2;
                        count     = currentScreen->clipBound_X2 - edge->start;
                    }

                    for (int32 x = 0; x < count; ++x) {
                        uint16 color = (startB >> 19) + ((startG >> 13) & 0x7E0) + ((startR >> 8) & 0xF800);
                        setPixelBlend(color, frameBuffer[edge->start + x]);

                        startR += deltaR;
                        startG += deltaG;
                        startB += deltaB;
                    }

                    ++edge;
                    frameBuffer += currentScreen->pitch;
                }
                break;

            case INK_ALPHA: {
                uint16 *fbufferBlend = &blendLookupTable[0x20 * (0xFF - alpha)];
                uint16 *pixelBlend   = &blendLookupTable[0x20 * alpha];

                for (int32 s = topScreen; s <= bottomScreen; ++s) {
                    int32 start  = edge->start;
                    int32 count  = edge->end - edge->start;
                    int32 deltaR = 0;
                    int32 deltaG = 0;
                    int32 deltaB = 0;
                    if (count > 0) {
                        deltaR = (edge->endR - edge->startR) / count;
                        deltaG = (edge->endG - edge->startG) / count;
                        deltaB = (edge->endB - edge->startB) / count;
                    }
                    int32 startR = edge->startR;
                    int32 startG = edge->startG;
                    int32 startB = edge->startB;

                    if (start > currentScreen->clipBound_X2) {
                        edge->start = currentScreen->clipBound_X2;
                    }

                    if (start < currentScreen->clipBound_X1) {
                        startR += deltaR * (currentScreen->clipBound_X1 - edge->start);
                        startG += deltaG * (currentScreen->clipBound_X1 - edge->start);
                        startB += deltaB * (currentScreen->clipBound_X1 - edge->start);
                        count -= (currentScreen->clipBound_X1 - edge->start);
                        edge->start = currentScreen->clipBound_X1;
                    }

                    if (edge->end < currentScreen->clipBound_X1) {
                        edge->end = currentScreen->clipBound_X1;
                    }

                    if (edge->end > currentScreen->clipBound_X2) {
                        edge->end = currentScreen->clipBound_X2;
                        count     = currentScreen->clipBound_X2 - edge->start;
                    }

                    for (int32 x = 0; x < count; ++x) {
                        uint16 color = (startB >> 19) + ((startG >> 13) & 0x7E0) + ((startR >> 8) & 0xF800);
                        setPixelAlpha(color, frameBuffer[edge->start + x], alpha);

                        startR += deltaR;
                        startG += deltaG;
                        startB += deltaB;
                    }
                    ++edge;
                    frameBuffer += currentScreen->pitch;
                }
                break;
            }

            case INK_ADD: {
                uint16 *blendTablePtr = &blendLookupTable[0x20 * alpha];

                for (int32 s = topScreen; s <= bottomScreen; ++s) {
                    int32 start  = edge->start;
                    int32 count  = edge->end - edge->start;
                    int32 deltaR = 0;
                    int32 deltaG = 0;
                    int32 deltaB = 0;
                    if (count > 0) {
                        deltaR = (edge->endR - edge->startR) / count;
                        deltaG = (edge->endG - edge->startG) / count;
                        deltaB = (edge->endB - edge->startB) / count;
                    }
                    int32 startR = edge->startR;
                    int32 startG = edge->startG;
                    int32 startB = edge->startB;

                    if (start > currentScreen->clipBound_X2) {
                        edge->start = currentScreen->clipBound_X2;
                    }

                    if (start < currentScreen->clipBound_X1) {
                        startR += deltaR * (currentScreen->clipBound_X1 - edge->start);
                        startG += deltaG * (currentScreen->clipBound_X1 - edge->start);
                        startB += deltaB * (currentScreen->clipBound_X1 - edge->start);
                        count -= (currentScreen->clipBound_X1 - edge->start);
                        edge->start = currentScreen->clipBound_X1;
                    }

                    if (edge->end < currentScreen->clipBound_X1) {
                        edge->end = currentScreen->clipBound_X1;
                    }

                    if (edge->end > currentScreen->clipBound_X2) {
                        edge->end = currentScreen->clipBound_X2;
                        count     = currentScreen->clipBound_X2 - edge->start;
                    }

                    for (int32 x = 0; x < count; ++x) {
                        uint16 color = (startB >> 19) + ((startG >> 13) & 0x7E0) + ((startR >> 8) & 0xF800);
                        setPixelAdditive(color, frameBuffer[edge->start + x]);

                        startR += deltaR;
                        startG += deltaG;
                        startB += deltaB;
                    }

                    ++edge;
                    frameBuffer += currentScreen->pitch;
                }
                break;
            }

            case INK_SUB: {
                uint16 *subBlendTable = &subtractLookupTable[0x20 * alpha];

                for (int32 s = topScreen; s <= bottomScreen; ++s) {
                    int32 start  = edge->start;
                    int32 count  = edge->end - edge->start;
                    int32 deltaR = 0;
                    int32 deltaG = 0;
                    int32 deltaB = 0;
                    if (count > 0) {
                        deltaR = (edge->endR - edge->startR) / count;
                        deltaG = (edge->endG - edge->startG) / count;
                        deltaB = (edge->endB - edge->startB) / count;
                    }
                    int32 startR = edge->startR;
                    int32 startG = edge->startG;
                    int32 startB = edge->startB;

                    if (start > currentScreen->clipBound_X2) {
                        edge->start = currentScreen->clipBound_X2;
                    }

                    if (start < currentScreen->clipBound_X1) {
                        startR += deltaR * (currentScreen->clipBound_X1 - edge->start);
                        startG += deltaG * (currentScreen->clipBound_X1 - edge->start);
                        startB += deltaB * (currentScreen->clipBound_X1 - edge->start);
                        count -= (currentScreen->clipBound_X1 - edge->start);
                        edge->start = currentScreen->clipBound_X1;
                    }

                    if (edge->end < currentScreen->clipBound_X1) {
                        edge->end = currentScreen->clipBound_X1;
                    }

                    if (edge->end > currentScreen->clipBound_X2) {
                        edge->end = currentScreen->clipBound_X2;
                        count     = currentScreen->clipBound_X2 - edge->start;
                    }

                    for (int32 x = 0; x < count; ++x) {
                        uint16 color = (startB >> 19) + ((startG >> 13) & 0x7E0) + ((startR >> 8) & 0xF800);
                        setPixelSubtractive(color, frameBuffer[edge->start + x]);

                        startR += deltaR;
                        startG += deltaG;
                        startB += deltaB;
                    }

                    ++edge;
                    frameBuffer += currentScreen->pitch;
                }
                break;
            }

            case INK_TINT:
                for (int32 s = topScreen; s <= bottomScreen; ++s) {
                    int32 start  = edge->start;
                    int32 count  = edge->end - edge->start;

#if RETRO_USE_ORIGINAL_CODE
                    // Unused, ifdef'd out to help out ports for weaker hardware
                    int32 deltaR = 0;
                    int32 deltaG = 0;
                    int32 deltaB = 0;
                    if (count > 0) {
                        deltaR = (edge->endR - edge->startR) / count;
                        deltaG = (edge->endG - edge->startG) / count;
                        deltaB = (edge->endB - edge->startB) / count;
                    }
                    int32 startR = edge->startR;
                    int32 startG = edge->startG;
                    int32 startB = edge->startB;
#endif

                    if (start > currentScreen->clipBound_X2) {
                        edge->start = currentScreen->clipBound_X2;
                    }

                    if (start < currentScreen->clipBound_X1) {
#if RETRO_USE_ORIGINAL_CODE
                        // Unused, ifdef'd out to help out ports for weaker hardware
                        startR += deltaR * (currentScreen->clipBound_X1 - edge->start);
                        startG += deltaG * (currentScreen->clipBound_X1 - edge->start);
                        startB += deltaB * (currentScreen->clipBound_X1 - edge->start);
#endif

                        count -= (currentScreen->clipBound_X1 - edge->start);
                        edge->start = currentScreen->clipBound_X1;
                    }

                    if (edge->end < currentScreen->clipBound_X1) {
                        edge->end = currentScreen->clipBound_X1;
                    }

                    if (edge->end > currentScreen->clipBound_X2) {
                        edge->end = currentScreen->clipBound_X2;
                        count     = currentScreen->clipBound_X2 - edge->start;
                    }

                    for (int32 x = 0; x < count; ++x) {
                        frameBuffer[edge->start + x] = tintLookupTable[frameBuffer[edge->start + x]];

#if RETRO_USE_ORIGINAL_CODE
                        // Unused, ifdef'd out to help out ports for weaker hardware
                        startR += deltaR;
                        startG += deltaG;
                        startB += deltaB;
#endif
                    }

                    ++edge;
                    frameBuffer += currentScreen->pitch;
                }
                break;

            case INK_MASKED:
                for (int32 s = topScreen; s <= bottomScreen; ++s) {
                    int32 start  = edge->start;
                    int32 count  = edge->end - edge->start;
                    int32 deltaR = 0;
                    int32 deltaG = 0;
                    int32 deltaB = 0;
                    if (count > 0) {
                        deltaR = (edge->endR - edge->startR) / count;
                        deltaG = (edge->endG - edge->startG) / count;
                        deltaB = (edge->endB - edge->startB) / count;
                    }
                    int32 startR = edge->startR;
                    int32 startG = edge->startG;
                    int32 startB = edge->startB;

                    if (start > currentScreen->clipBound_X2) {
                        edge->start = currentScreen->clipBound_X2;
                    }

                    if (start < currentScreen->clipBound_X1) {
                        startR += deltaR * (currentScreen->clipBound_X1 - edge->start);
                        startG += deltaG * (currentScreen->clipBound_X1 - edge->start);
                        startB += deltaB * (currentScreen->clipBound_X1 - edge->start);
                        count -= (currentScreen->clipBound_X1 - edge->start);
                        edge->start = currentScreen->clipBound_X1;
                    }

                    if (edge->end < currentScreen->clipBound_X1 || edge->end > currentScreen->clipBound_X2) {
                        edge->end = currentScreen->clipBound_X2;
                        count     = currentScreen->clipBound_X2 - edge->start;
                    }

                    for (int32 x = 0; x < count; ++x) {
                        if (frameBuffer[edge->start + x] == maskColor)
                            frameBuffer[edge->start + x] = (startB >> 19) + ((startG >> 13) & 0x7E0) + ((startR >> 8) & 0xF800);

                        startR += deltaR;
                        startG += deltaG;
                        startB += deltaB;
                    }

                    ++edge;
                    frameBuffer += currentScreen->pitch;
                }
                break;

            case INK_UNMASKED:
                for (int32 s = topScreen; s <= bottomScreen; ++s) {
                    int32 start  = edge->start;
                    int32 count  = edge->end - edge->start;
                    int32 deltaR = 0;
                    int32 deltaG = 0;
                    int32 deltaB = 0;
                    if (count > 0) {
                        deltaR = (edge->endR - edge->startR) / count;
                        deltaG = (edge->endG - edge->startG) / count;
                        deltaB = (edge->endB - edge->startB) / count;
                    }
                    int32 startR = edge->startR;
                    int32 startG = edge->startG;
                    int32 startB = edge->startB;

                    if (start > currentScreen->clipBound_X2) {
                        edge->start = currentScreen->clipBound_X2;
                    }

                    if (start < currentScreen->clipBound_X1) {
                        startR += deltaR * (currentScreen->clipBound_X1 - edge->start);
                        startG += deltaG * (currentScreen->clipBound_X1 - edge->start);
                        startB += deltaB * (currentScreen->clipBound_X1 - edge->start);
                        count -= (currentScreen->clipBound_X1 - edge->start);
                        edge->start = currentScreen->clipBound_X1;
                    }

                    if (edge->end < currentScreen->clipBound_X1) {
                        edge->end = currentScreen->clipBound_X1;
                    }

                    if (edge->end > currentScreen->clipBound_X2) {
                        edge->end = currentScreen->clipBound_X2;
                        count     = currentScreen->clipBound_X2 - edge->start;
                    }

                    for (int32 x = 0; x < count; ++x) {
                        if (frameBuffer[edge->start + x] != maskColor)
                            frameBuffer[edge->start + x] = (startB >> 19) + ((startG >> 13) & 0x7E0) + ((startR >> 8) & 0xF800);

                        startR += deltaR;
                        startG += deltaG;
                        startB += deltaB;
                    }

                    ++edge;
                    frameBuffer += currentScreen->pitch;
                }
                break;
        }
    }
#endif
}

#if RETRO_PLATFORM == RETRO_KALLISTIOS && defined(KOS_HARDWARE_RENDERER)
void RSDK::Draw3DSprite(Animator *animator, Vector4f *position, bool32 screenRelative) {
    Vector2 pos;
    pos.x = (int)position->x;
    pos.y = (int)position->y;
    float oldDepth = RenderDevice::GetDepth();

    RenderDevice::SetDepth(oldDepth + position->z);
    DrawSprite(animator, &pos, screenRelative);
    RenderDevice::SetDepth(oldDepth);
}
#endif
void RSDK::DrawSprite(Animator *animator, Vector2 *position, bool32 screenRelative)
{
    if (animator && animator->frames) {
        SpriteFrame *frame = &animator->frames[animator->frameID];
        Vector2 pos;
        if (!position)
            pos = sceneInfo.entity->position;
        else
            pos = *position;

        pos.x >>= 0x10;
        pos.y >>= 0x10;
        if (!screenRelative) {
            pos.x -= currentScreen->position.x;
            pos.y -= currentScreen->position.y;
        }

        int32 rotation = sceneInfo.entity->rotation;
        int32 drawFX   = sceneInfo.entity->drawFX;
        if (sceneInfo.entity->drawFX & FX_ROTATE) {
            switch (animator->rotationStyle) {
                case ROTSTYLE_NONE:
                    rotation = 0;
                    if ((sceneInfo.entity->drawFX & FX_ROTATE) != FX_NONE)
                        drawFX ^= FX_ROTATE;
                    break;

                case ROTSTYLE_FULL:
                    rotation = sceneInfo.entity->rotation & 0x1FF;
                    if (rotation == 0)
                        drawFX ^= FX_ROTATE;
                    break;

                case ROTSTYLE_45DEG: // 0x00, 0x40, 0x80, 0xC0, 0x100, 0x140, 0x180, 0x1C0
                    rotation = (sceneInfo.entity->rotation + 0x20) & 0x1C0;
                    if (rotation == 0)
                        drawFX ^= FX_ROTATE;
                    break;

                case ROTSTYLE_90DEG: // 0x00, 0x80, 0x100, 0x180
                    rotation = (sceneInfo.entity->rotation + 0x40) & 0x180;
                    if (rotation == 0)
                        drawFX ^= FX_ROTATE;
                    break;

                case ROTSTYLE_180DEG: // 0x00, 0x100
                    rotation = (sceneInfo.entity->rotation + 0x80) & 0x100;
                    if (rotation == 0)
                        drawFX ^= FX_ROTATE;
                    break;

                case ROTSTYLE_STATICFRAMES:
                    if (sceneInfo.entity->rotation >= 0x100)
                        rotation = 0x08 - ((0x214 - sceneInfo.entity->rotation) >> 6);
                    else
                        rotation = (sceneInfo.entity->rotation + 20) >> 6;

                    switch (rotation) {
                        case 0: // 0 deg
                        case 8: // 360 deg
                            rotation = 0x00;
                            if ((sceneInfo.entity->drawFX & FX_SCALE) != FX_NONE)
                                drawFX ^= FX_ROTATE;
                            break;

                        case 1: // 45 deg
                            rotation = 0x80;
                            frame += animator->frameCount;
                            if (sceneInfo.entity->direction)
                                rotation = 0x00;
                            break;

                        case 2: // 90 deg
                            rotation = 0x80;
                            break;

                        case 3: // 135 deg
                            rotation = 0x100;
                            frame += animator->frameCount;
                            if (sceneInfo.entity->direction)
                                rotation = 0x80;
                            break;

                        case 4: // 180 deg
                            rotation = 0x100;
                            break;

                        case 5: // 225 deg
                            rotation = 0x180;
                            frame += animator->frameCount;
                            if (sceneInfo.entity->direction)
                                rotation = 0x100;
                            break;

                        case 6: // 270 deg
                            rotation = 0x180;
                            break;

                        case 7: // 315 deg
                            rotation = 0x180;
                            frame += animator->frameCount;
                            if (!sceneInfo.entity->direction)
                                rotation = 0;
                            break;

                        default: break;
                    }
                    break;

                default: break;
            }
        }

        switch (drawFX) {
            case FX_NONE:
                DrawSpriteFlipped(pos.x + frame->pivotX, pos.y + frame->pivotY, frame->width, frame->height, frame->sprX, frame->sprY, FLIP_NONE,
                                  sceneInfo.entity->inkEffect, sceneInfo.entity->alpha, frame->sheetID);
                break;

            case FX_FLIP:
                switch (sceneInfo.entity->direction) {
                    case FLIP_NONE:
                        DrawSpriteFlipped(pos.x + frame->pivotX, pos.y + frame->pivotY, frame->width, frame->height, frame->sprX, frame->sprY,
                                          FLIP_NONE, sceneInfo.entity->inkEffect, sceneInfo.entity->alpha, frame->sheetID);
                        break;

                    case FLIP_X:
                        DrawSpriteFlipped(pos.x - frame->width - frame->pivotX, pos.y + frame->pivotY, frame->width, frame->height, frame->sprX,
                                          frame->sprY, FLIP_X, sceneInfo.entity->inkEffect, sceneInfo.entity->alpha, frame->sheetID);
                        break;

                    case FLIP_Y:
                        DrawSpriteFlipped(pos.x + frame->pivotX, pos.y - frame->height - frame->pivotY, frame->width, frame->height, frame->sprX,
                                          frame->sprY, FLIP_Y, sceneInfo.entity->inkEffect, sceneInfo.entity->alpha, frame->sheetID);
                        break;

                    case FLIP_XY:
                        DrawSpriteFlipped(pos.x - frame->width - frame->pivotX, pos.y - frame->height - frame->pivotY, frame->width, frame->height,
                                          frame->sprX, frame->sprY, FLIP_XY, sceneInfo.entity->inkEffect, sceneInfo.entity->alpha, frame->sheetID);
                        break;

                    default: break;
                }
                break;

            case FX_ROTATE:
                DrawSpriteRotozoom(pos.x, pos.y, frame->pivotX, frame->pivotY, frame->width, frame->height, frame->sprX, frame->sprY, 0x200, 0x200,
                                   FLIP_NONE, rotation, sceneInfo.entity->inkEffect, sceneInfo.entity->alpha, frame->sheetID);
                break;

            case FX_ROTATE | FX_FLIP:
                DrawSpriteRotozoom(pos.x, pos.y, frame->pivotX, frame->pivotY, frame->width, frame->height, frame->sprX, frame->sprY, 0x200, 0x200,
                                   sceneInfo.entity->direction & FLIP_X, rotation, sceneInfo.entity->inkEffect, sceneInfo.entity->alpha,
                                   frame->sheetID);
                break;

            case FX_SCALE:
                DrawSpriteRotozoom(pos.x, pos.y, frame->pivotX, frame->pivotY, frame->width, frame->height, frame->sprX, frame->sprY,
                                   sceneInfo.entity->scale.x, sceneInfo.entity->scale.y, FLIP_NONE, 0, sceneInfo.entity->inkEffect,
                                   sceneInfo.entity->alpha, frame->sheetID);
                break;

            case FX_SCALE | FX_FLIP:
                DrawSpriteRotozoom(pos.x, pos.y, frame->pivotX, frame->pivotY, frame->width, frame->height, frame->sprX, frame->sprY,
                                   sceneInfo.entity->scale.x, sceneInfo.entity->scale.y, sceneInfo.entity->direction & FLIP_X, 0,
                                   sceneInfo.entity->inkEffect, sceneInfo.entity->alpha, frame->sheetID);
                break;

            case FX_SCALE | FX_ROTATE:
                DrawSpriteRotozoom(pos.x, pos.y, frame->pivotX, frame->pivotY, frame->width, frame->height, frame->sprX, frame->sprY,
                                   sceneInfo.entity->scale.x, sceneInfo.entity->scale.y, FLIP_NONE, rotation, sceneInfo.entity->inkEffect,
                                   sceneInfo.entity->alpha, frame->sheetID);
                break;

            case FX_SCALE | FX_ROTATE | FX_FLIP:
                DrawSpriteRotozoom(pos.x, pos.y, frame->pivotX, frame->pivotY, frame->width, frame->height, frame->sprX, frame->sprY,
                                   sceneInfo.entity->scale.x, sceneInfo.entity->scale.y, sceneInfo.entity->direction & FLIP_X, rotation,
                                   sceneInfo.entity->inkEffect, sceneInfo.entity->alpha, frame->sheetID);
                break;

            default: break;
        }

#if RETRO_PLATFORM == RETRO_KALLISTIOS
        // DC_SILHOUETTE: re-draw as solid purple silhouette clipped to each overlapping region
        if (silhouetteRegionCount > 0) {
            int32 camX = currentScreen->position.x;
            int32 camY = currentScreen->position.y;

            int32 sprLeft, sprTop, sprRight, sprBottom;
            if (drawFX & FX_ROTATE) {
                int32 maxExt = MAX(frame->width + abs(frame->pivotX), frame->height + abs(frame->pivotY));
                sprLeft   = pos.x - maxExt;
                sprTop    = pos.y - maxExt;
                sprRight  = pos.x + maxExt;
                sprBottom = pos.y + maxExt;
            } else {
                sprLeft   = pos.x - frame->width - abs(frame->pivotX);
                sprTop    = pos.y - frame->height - abs(frame->pivotY);
                sprRight  = pos.x + frame->width + abs(frame->pivotX);
                sprBottom = pos.y + frame->height + abs(frame->pivotY);
            }

            for (int32 r = 0; r < silhouetteRegionCount; ++r) {
                SilhouetteRegion *region = &silhouetteRegions[r];
                if (sceneInfo.entity->drawGroup >= region->drawGroup)
                    continue;

                int32 rgnX1 = region->x1 - camX;
                int32 rgnY1 = region->y1 - camY;
                int32 rgnX2 = region->x2 - camX;
                int32 rgnY2 = region->y2 - camY;

                if (sprRight <= rgnX1 || sprLeft >= rgnX2 || sprBottom <= rgnY1 || sprTop >= rgnY2)
                    continue;

                int32 saveX1 = currentScreen->clipBound_X1;
                int32 saveY1 = currentScreen->clipBound_Y1;
                int32 saveX2 = currentScreen->clipBound_X2;
                int32 saveY2 = currentScreen->clipBound_Y2;

                currentScreen->clipBound_X1 = MAX(saveX1, rgnX1);
                currentScreen->clipBound_Y1 = MAX(saveY1, rgnY1);
                currentScreen->clipBound_X2 = MIN(saveX2, rgnX2);
                currentScreen->clipBound_Y2 = MIN(saveY2, rgnY2);

                switch (drawFX) {
                    case FX_NONE:
                        DrawSpriteFlipped(pos.x + frame->pivotX, pos.y + frame->pivotY, frame->width, frame->height, frame->sprX, frame->sprY,
                                          FLIP_NONE, INK_UNMASKED, 0xFF, frame->sheetID);
                        break;

                    case FX_FLIP:
                        switch (sceneInfo.entity->direction) {
                            case FLIP_NONE:
                                DrawSpriteFlipped(pos.x + frame->pivotX, pos.y + frame->pivotY, frame->width, frame->height, frame->sprX,
                                                  frame->sprY, FLIP_NONE, INK_UNMASKED, 0xFF, frame->sheetID);
                                break;
                            case FLIP_X:
                                DrawSpriteFlipped(pos.x - frame->width - frame->pivotX, pos.y + frame->pivotY, frame->width, frame->height,
                                                  frame->sprX, frame->sprY, FLIP_X, INK_UNMASKED, 0xFF, frame->sheetID);
                                break;
                            case FLIP_Y:
                                DrawSpriteFlipped(pos.x + frame->pivotX, pos.y - frame->height - frame->pivotY, frame->width, frame->height,
                                                  frame->sprX, frame->sprY, FLIP_Y, INK_UNMASKED, 0xFF, frame->sheetID);
                                break;
                            case FLIP_XY:
                                DrawSpriteFlipped(pos.x - frame->width - frame->pivotX, pos.y - frame->height - frame->pivotY, frame->width,
                                                  frame->height, frame->sprX, frame->sprY, FLIP_XY, INK_UNMASKED, 0xFF, frame->sheetID);
                                break;
                            default: break;
                        }
                        break;

                    case FX_ROTATE:
                    case FX_ROTATE | FX_FLIP:
                    case FX_SCALE:
                    case FX_SCALE | FX_FLIP:
                    case FX_SCALE | FX_ROTATE:
                    case FX_SCALE | FX_ROTATE | FX_FLIP:
                        if (frame->width * frame->height < 16384
                            && pos.x >= rgnX1 && pos.x < rgnX2 && pos.y >= rgnY1 && pos.y < rgnY2)
                            DrawSpriteRotozoom(pos.x, pos.y, frame->pivotX, frame->pivotY, frame->width, frame->height, frame->sprX, frame->sprY,
                                               (drawFX & FX_SCALE) ? sceneInfo.entity->scale.x : 0x200,
                                               (drawFX & FX_SCALE) ? sceneInfo.entity->scale.y : 0x200,
                                               (drawFX & FX_FLIP) ? (sceneInfo.entity->direction & FLIP_X) : FLIP_NONE,
                                               (drawFX & FX_ROTATE) ? rotation : 0, INK_UNMASKED, 0xFF, frame->sheetID);
                        break;

                    default: break;
                }

                currentScreen->clipBound_X1 = saveX1;
                currentScreen->clipBound_Y1 = saveY1;
                currentScreen->clipBound_X2 = saveX2;
                currentScreen->clipBound_Y2 = saveY2;
            }
        }
#endif
    }
}
void RSDK::DrawSpriteFlipped(int32 x, int32 y, int32 width, int32 height, int32 sprX, int32 sprY, int32 direction, int32 inkEffect, int32 alpha,
                             int32 sheetID)
{
    switch (inkEffect) {
        default: break;
        case INK_ALPHA:
            if (alpha > 0xFF)
                inkEffect = INK_NONE;
            else if (alpha <= 0)
                return;
            break;

        case INK_ADD:
        case INK_SUB:
            if (alpha > 0xFF)
                alpha = 0xFF;
            else if (alpha <= 0)
                return;
            break;

        case INK_TINT:
            if (!tintLookupTable)
                return;
            break;
    }
#if RETRO_PLATFORM == RETRO_KALLISTIOS
    GFXSurface *surface = &gfxSurface[sheetID];

    if (surface->texture == nullptr) {
        return;
    }

    if (!RenderDevice::SupportedInk(inkEffect)) {
        return;
    }

    if (direction < FLIP_NONE || direction > FLIP_XY) {
        return;
    }

    int32 marginLeft   = 0;
    int32 marginRight  = 0;
    int32 marginTop    = 0;
    int32 marginBottom = 0;

    if (x + width > currentScreen->clipBound_X2) {
        marginRight = width - (currentScreen->clipBound_X2 - x);
    }

    if (x < currentScreen->clipBound_X1) {
        marginLeft = currentScreen->clipBound_X1 - x;
    }

    if (y + height > currentScreen->clipBound_Y2) {
        marginBottom = height - (currentScreen->clipBound_Y2 - y);
    }

    if (y < currentScreen->clipBound_Y1) {
        marginTop = currentScreen->clipBound_Y1 - y;
    }

    const int32 widthClipped = width - (marginLeft + marginRight);
    const int32 heightClipped = height - (marginTop + marginBottom);

    if (widthClipped <= 0 || heightClipped <= 0) {
        return;
    }

    validDraw = true;

    int32 sprX0;
    int32 sprX1;
    int32 sprY0;
    int32 sprY1;

    if (direction & FLIP_X) {
        sprX0 = sprX + (width - marginLeft);
        sprX1 = sprX + marginRight;
    } else {
        sprX0 = sprX + marginLeft;
        sprX1 = sprX + (width - marginRight);
    }

    if (direction & FLIP_Y) {
        sprY0 = sprY + (height - marginTop);
        sprY1 = sprY + marginBottom;
    } else {
        sprY0 = sprY + marginTop;
        sprY1 = sprY + (height - marginBottom);
    }

    const int32 xClipped = x + marginLeft;
    const int32 yClipped = y + marginTop;

    if (inkEffect == INK_NONE || inkEffect == INK_FLASH) {
        alpha = 0xFF;
    }
    if (inkEffect == INK_FLASH_GIGA) {
        lastFlashGigaAlpha = (uint8)alpha;
        alpha = 0xFF;
    }

    if ((inkEffect != INK_SUB) && (inkEffect != INK_ADD) && (alpha == 0xFF)) {
        RenderDevice::PrepareTexturedPolyPT(yClipped, inkEffect, surface);
        RenderDevice::DrawTexturedPolyPT(
                xClipped, yClipped,
                xClipped, yClipped,
                widthClipped, heightClipped,
                sprX0, sprX1,
                sprY0, sprY1,
                0,
                alpha,
                surface);
    } else {
        RenderDevice::PrepareTexturedPolyTR(yClipped, inkEffect, surface);
        RenderDevice::DrawTexturedPolyTR(
                xClipped, yClipped,
                xClipped, yClipped,
                widthClipped, heightClipped,
                sprX0, sprX1,
                sprY0, sprY1,
                0,
                alpha,
                surface);
    }
#else
    int32 widthFlip  = width;
    int32 heightFlip = height;

    if (width + x > currentScreen->clipBound_X2)
        width = currentScreen->clipBound_X2 - x;

    if (x < currentScreen->clipBound_X1) {
        int32 val = x - currentScreen->clipBound_X1;
        sprX -= val;
        width += val;
        widthFlip += 2 * val;
        x = currentScreen->clipBound_X1;
    }

    if (height + y > currentScreen->clipBound_Y2)
        height = currentScreen->clipBound_Y2 - y;

    if (y < currentScreen->clipBound_Y1) {
        int32 val = y - currentScreen->clipBound_Y1;
        sprY -= val;
        height += val;
        heightFlip += 2 * val;
        y = currentScreen->clipBound_Y1;
    }

    if (width <= 0 || height <= 0)
        return;

    GFXSurface *surface = &gfxSurface[sheetID];
    validDraw           = true;
    int32 pitch         = currentScreen->pitch - width;
    int32 gfxPitch      = 0;
    uint8 *lineBuffer   = NULL;
    uint8 *pixels       = NULL;
    uint16 *frameBuffer = NULL;

    switch (direction) {
        default: break;

        case FLIP_NONE:
            gfxPitch    = surface->width - width;
            lineBuffer  = &gfxLineBuffer[y];
            pixels      = &surface->pixels[sprX + surface->width * sprY];
            frameBuffer = &currentScreen->frameBuffer[x + currentScreen->pitch * y];
            switch (inkEffect) {
                case INK_NONE:
                    while (height--) {
                        uint16 *activePalette = fullPalette[*lineBuffer];
                        lineBuffer++;
                        int32 w = width;
                        while (w--) {
                            if (*pixels > 0)
                                *frameBuffer = activePalette[*pixels];
                            ++pixels;
                            ++frameBuffer;
                        }
                        frameBuffer += pitch;
                        pixels += gfxPitch;
                    }
                    break;

                case INK_BLEND:
                    while (height--) {
                        uint16 *activePalette = fullPalette[*lineBuffer];
                        lineBuffer++;
                        int32 w = width;
                        while (w--) {
                            if (*pixels > 0)
                                setPixelBlend(activePalette[*pixels], *frameBuffer);
                            ++pixels;
                            ++frameBuffer;
                        }
                        frameBuffer += pitch;
                        pixels += gfxPitch;
                    }
                    break;

                case INK_ALPHA: {
                    uint16 *fbufferBlend = &blendLookupTable[0x20 * (0xFF - alpha)];
                    uint16 *pixelBlend   = &blendLookupTable[0x20 * alpha];

                    while (height--) {
                        uint16 *activePalette = fullPalette[*lineBuffer];
                        lineBuffer++;
                        int32 w = width;
                        while (w--) {
                            if (*pixels > 0) {
                                uint16 color = activePalette[*pixels];
                                setPixelAlpha(color, *frameBuffer, alpha);
                            }
                            ++pixels;
                            ++frameBuffer;
                        }
                        frameBuffer += pitch;
                        pixels += gfxPitch;
                    }
                    break;
                }

                case INK_ADD: {
                    uint16 *blendTablePtr = &blendLookupTable[0x20 * alpha];
                    while (height--) {
                        uint16 *activePalette = fullPalette[*lineBuffer];
                        lineBuffer++;
                        int32 w = width;
                        while (w--) {
                            if (*pixels > 0) {
                                uint16 color = activePalette[*pixels];
                                setPixelAdditive(color, *frameBuffer);
                            }
                            ++pixels;
                            ++frameBuffer;
                        }
                        frameBuffer += pitch;
                        pixels += gfxPitch;
                    }
                    break;
                }

                case INK_SUB: {
                    uint16 *subBlendTable = &subtractLookupTable[0x20 * alpha];
                    while (height--) {
                        uint16 *activePalette = fullPalette[*lineBuffer];
                        lineBuffer++;
                        int32 w = width;
                        while (w--) {
                            if (*pixels > 0) {
                                uint16 color = activePalette[*pixels];
                                setPixelSubtractive(color, *frameBuffer);
                            }
                            ++pixels;
                            ++frameBuffer;
                        }
                        frameBuffer += pitch;
                        pixels += gfxPitch;
                    }
                    break;
                }

                case INK_TINT:
                    while (height--) {
                        lineBuffer++;
                        int32 w = width;
                        while (w--) {
                            if (*pixels > 0)
                                *frameBuffer = tintLookupTable[*frameBuffer];
                            ++pixels;
                            ++frameBuffer;
                        }
                        frameBuffer += pitch;
                        pixels += gfxPitch;
                    }
                    break;

                case INK_MASKED:
                    while (height--) {
                        uint16 *activePalette = fullPalette[*lineBuffer];
                        lineBuffer++;
                        int32 w = width;
                        while (w--) {
                            if (*pixels > 0 && *frameBuffer == maskColor)
                                *frameBuffer = activePalette[*pixels];
                            ++pixels;
                            ++frameBuffer;
                        }
                        frameBuffer += pitch;
                        pixels += gfxPitch;
                    }
                    break;

                case INK_UNMASKED:
                    while (height--) {
                        uint16 *activePalette = fullPalette[*lineBuffer];
                        lineBuffer++;
                        int32 w = width;
                        while (w--) {
                            if (*pixels > 0 && *frameBuffer != maskColor)
                                *frameBuffer = activePalette[*pixels];
                            ++pixels;
                            ++frameBuffer;
                        }
                        frameBuffer += pitch;
                        pixels += gfxPitch;
                    }
                    break;
            }
            break;

        case FLIP_X:
            gfxPitch    = width + surface->width;
            lineBuffer  = &gfxLineBuffer[y];
            pixels      = &surface->pixels[widthFlip - 1 + sprX + surface->width * sprY];
            frameBuffer = &currentScreen->frameBuffer[x + currentScreen->pitch * y];
            switch (inkEffect) {
                case INK_NONE:
                    while (height--) {
                        uint16 *activePalette = fullPalette[*lineBuffer];
                        lineBuffer++;
                        int32 w = width;
                        while (w--) {
                            if (*pixels > 0)
                                *frameBuffer = activePalette[*pixels];
                            --pixels;
                            ++frameBuffer;
                        }
                        frameBuffer += pitch;
                        pixels += gfxPitch;
                    }
                    break;

                case INK_BLEND:
                    while (height--) {
                        uint16 *activePalette = fullPalette[*lineBuffer];
                        lineBuffer++;
                        int32 w = width;
                        while (w--) {
                            if (*pixels > 0)
                                setPixelBlend(activePalette[*pixels], *frameBuffer);
                            --pixels;
                            ++frameBuffer;
                        }
                        frameBuffer += pitch;
                        pixels += gfxPitch;
                    }
                    break;

                case INK_ALPHA: {
                    uint16 *fbufferBlend = &blendLookupTable[0x20 * (0xFF - alpha)];
                    uint16 *pixelBlend   = &blendLookupTable[0x20 * alpha];

                    while (height--) {
                        uint16 *activePalette = fullPalette[*lineBuffer];
                        lineBuffer++;
                        int32 w = width;
                        while (w--) {
                            if (*pixels > 0) {
                                uint16 color = activePalette[*pixels];
                                setPixelAlpha(color, *frameBuffer, alpha);
                            }
                            --pixels;
                            ++frameBuffer;
                        }
                        frameBuffer += pitch;
                        pixels += gfxPitch;
                    }
                    break;
                }

                case INK_ADD: {
                    uint16 *blendTablePtr = &blendLookupTable[0x20 * alpha];
                    while (height--) {
                        uint16 *activePalette = fullPalette[*lineBuffer];
                        lineBuffer++;
                        int32 w = width;
                        while (w--) {
                            if (*pixels > 0) {
                                uint16 color = activePalette[*pixels];
                                setPixelAdditive(color, *frameBuffer);
                            }
                            --pixels;
                            ++frameBuffer;
                        }
                        frameBuffer += pitch;
                        pixels += gfxPitch;
                    }
                    break;
                }

                case INK_SUB: {
                    uint16 *subBlendTable = &subtractLookupTable[0x20 * alpha];
                    while (height--) {
                        uint16 *activePalette = fullPalette[*lineBuffer];
                        lineBuffer++;
                        int32 w = width;
                        while (w--) {
                            if (*pixels > 0) {
                                uint16 color = activePalette[*pixels];
                                setPixelSubtractive(color, *frameBuffer);
                            }
                            --pixels;
                            ++frameBuffer;
                        }
                        frameBuffer += pitch;
                        pixels += gfxPitch;
                    }
                    break;
                }

                case INK_TINT:
                    while (height--) {
                        lineBuffer++;
                        int32 w = width;
                        while (w--) {
                            if (*pixels > 0)
                                *frameBuffer = tintLookupTable[*frameBuffer];
                            --pixels;
                            ++frameBuffer;
                        }
                        frameBuffer += pitch;
                        pixels += gfxPitch;
                    }
                    break;

                case INK_MASKED:
                    while (height--) {
                        uint16 *activePalette = fullPalette[*lineBuffer];
                        lineBuffer++;
                        int32 w = width;
                        while (w--) {
                            if (*pixels > 0 && *frameBuffer == maskColor)
                                *frameBuffer = activePalette[*pixels];
                            --pixels;
                            ++frameBuffer;
                        }
                        frameBuffer += pitch;
                        pixels += gfxPitch;
                    }
                    break;

                case INK_UNMASKED:
                    while (height--) {
                        uint16 *activePalette = fullPalette[*lineBuffer];
                        lineBuffer++;
                        int32 w = width;
                        while (w--) {
                            if (*pixels > 0 && *frameBuffer != maskColor)
                                *frameBuffer = activePalette[*pixels];
                            --pixels;
                            ++frameBuffer;
                        }
                        frameBuffer += pitch;
                        pixels += gfxPitch;
                    }
                    break;
            }
            break;

        case FLIP_Y:
            gfxPitch    = width + surface->width;
            lineBuffer  = &gfxLineBuffer[y];
            pixels      = &surface->pixels[sprX + surface->width * (sprY + heightFlip - 1)];
            frameBuffer = &currentScreen->frameBuffer[x + currentScreen->pitch * y];
            switch (inkEffect) {
                case INK_NONE:
                    while (height--) {
                        uint16 *activePalette = fullPalette[*lineBuffer];
                        lineBuffer++;
                        int32 w = width;
                        while (w--) {
                            if (*pixels > 0)
                                *frameBuffer = activePalette[*pixels];
                            ++pixels;
                            ++frameBuffer;
                        }
                        frameBuffer += pitch;
                        pixels -= gfxPitch;
                    }
                    break;

                case INK_BLEND:
                    while (height--) {
                        uint16 *activePalette = fullPalette[*lineBuffer];
                        lineBuffer++;
                        int32 w = width;
                        while (w--) {
                            if (*pixels > 0)
                                setPixelBlend(activePalette[*pixels], *frameBuffer);
                            ++pixels;
                            ++frameBuffer;
                        }
                        frameBuffer += pitch;
                        pixels -= gfxPitch;
                    }
                    break;

                case INK_ALPHA: {
                    uint16 *fbufferBlend = &blendLookupTable[0x20 * (0xFF - alpha)];
                    uint16 *pixelBlend   = &blendLookupTable[0x20 * alpha];

                    while (height--) {
                        uint16 *activePalette = fullPalette[*lineBuffer];
                        lineBuffer++;
                        int32 w = width;
                        while (w--) {
                            if (*pixels > 0) {
                                uint16 color = activePalette[*pixels];
                                setPixelAlpha(color, *frameBuffer, alpha);
                            }
                            ++pixels;
                            ++frameBuffer;
                        }
                        frameBuffer += pitch;
                        pixels -= gfxPitch;
                    }
                    break;
                }

                case INK_ADD: {
                    uint16 *blendTablePtr = &blendLookupTable[0x20 * alpha];
                    while (height--) {
                        uint16 *activePalette = fullPalette[*lineBuffer];
                        lineBuffer++;
                        int32 w = width;
                        while (w--) {
                            if (*pixels > 0) {
                                uint16 color = activePalette[*pixels];
                                setPixelAdditive(color, *frameBuffer);
                            }
                            ++pixels;
                            ++frameBuffer;
                        }
                        frameBuffer += pitch;
                        pixels -= gfxPitch;
                    }
                    break;
                }

                case INK_SUB: {
                    uint16 *subBlendTable = &subtractLookupTable[0x20 * alpha];
                    while (height--) {
                        uint16 *activePalette = fullPalette[*lineBuffer];
                        lineBuffer++;
                        int32 w = width;
                        while (w--) {
                            if (*pixels > 0) {
                                uint16 color = activePalette[*pixels];
                                setPixelSubtractive(color, *frameBuffer);
                            }
                            ++pixels;
                            ++frameBuffer;
                        }
                        frameBuffer += pitch;
                        pixels -= gfxPitch;
                    }
                    break;
                }

                case INK_TINT:
                    while (height--) {
                        lineBuffer++;
                        int32 w = width;
                        while (w--) {
                            if (*pixels > 0)
                                *frameBuffer = tintLookupTable[*frameBuffer];
                            ++pixels;
                            ++frameBuffer;
                        }
                        frameBuffer += pitch;
                        pixels -= gfxPitch;
                    }
                    break;

                case INK_MASKED:
                    while (height--) {
                        uint16 *activePalette = fullPalette[*lineBuffer];
                        lineBuffer++;
                        int32 w = width;
                        while (w--) {
                            if (*pixels > 0 && *frameBuffer == maskColor)
                                *frameBuffer = activePalette[*pixels];
                            ++pixels;
                            ++frameBuffer;
                        }
                        frameBuffer += pitch;
                        pixels -= gfxPitch;
                    }
                    break;

                case INK_UNMASKED:
                    while (height--) {
                        uint16 *activePalette = fullPalette[*lineBuffer];
                        lineBuffer++;
                        int32 w = width;
                        while (w--) {
                            if (*pixels > 0 && *frameBuffer != maskColor)
                                *frameBuffer = activePalette[*pixels];
                            ++pixels;
                            ++frameBuffer;
                        }
                        frameBuffer += pitch;
                        pixels -= gfxPitch;
                    }
                    break;
            }
            break;

        case FLIP_XY:
            gfxPitch    = surface->width - width;
            lineBuffer  = &gfxLineBuffer[y];
            pixels      = &surface->pixels[widthFlip - 1 + sprX + surface->width * (sprY + heightFlip - 1)];
            frameBuffer = &currentScreen->frameBuffer[x + currentScreen->pitch * y];
            switch (inkEffect) {
                case INK_NONE:
                    while (height--) {
                        uint16 *activePalette = fullPalette[*lineBuffer];
                        lineBuffer++;
                        int32 w = width;
                        while (w--) {
                            if (*pixels > 0)
                                *frameBuffer = activePalette[*pixels];
                            --pixels;
                            ++frameBuffer;
                        }
                        frameBuffer += pitch;
                        pixels -= gfxPitch;
                    }
                    break;

                case INK_BLEND:
                    while (height--) {
                        uint16 *activePalette = fullPalette[*lineBuffer];
                        lineBuffer++;
                        int32 w = width;
                        while (w--) {
                            if (*pixels > 0)
                                setPixelBlend(activePalette[*pixels], *frameBuffer);
                            --pixels;
                            ++frameBuffer;
                        }
                        frameBuffer += pitch;
                        pixels -= gfxPitch;
                    }
                    break;

                case INK_ALPHA: {
                    uint16 *fbufferBlend = &blendLookupTable[0x20 * (0xFF - alpha)];
                    uint16 *pixelBlend   = &blendLookupTable[0x20 * alpha];

                    while (height--) {
                        uint16 *activePalette = fullPalette[*lineBuffer];
                        lineBuffer++;
                        int32 w = width;
                        while (w--) {
                            if (*pixels > 0) {
                                uint16 color = activePalette[*pixels];
                                setPixelAlpha(color, *frameBuffer, alpha);
                            }
                            --pixels;
                            ++frameBuffer;
                        }
                        frameBuffer += pitch;
                        pixels -= gfxPitch;
                    }
                    break;
                }

                case INK_ADD: {
                    uint16 *blendTablePtr = &blendLookupTable[0x20 * alpha];
                    while (height--) {
                        uint16 *activePalette = fullPalette[*lineBuffer];
                        lineBuffer++;
                        int32 w = width;
                        while (w--) {
                            if (*pixels > 0) {
                                uint16 color = activePalette[*pixels];
                                setPixelAdditive(color, *frameBuffer);
                            }
                            --pixels;
                            ++frameBuffer;
                        }
                        frameBuffer += pitch;
                        pixels -= gfxPitch;
                    }
                    break;
                }

                case INK_SUB: {
                    uint16 *subBlendTable = &subtractLookupTable[0x20 * alpha];
                    while (height--) {
                        uint16 *activePalette = fullPalette[*lineBuffer];
                        lineBuffer++;
                        int32 w = width;
                        while (w--) {
                            if (*pixels > 0) {
                                uint16 color = activePalette[*pixels];
                                setPixelSubtractive(color, *frameBuffer);
                            }
                            --pixels;
                            ++frameBuffer;
                        }
                        frameBuffer += pitch;
                        pixels -= gfxPitch;
                    }
                    break;
                }

                case INK_TINT:
                    while (height--) {
                        lineBuffer++;
                        int32 w = width;
                        while (w--) {
                            if (*pixels > 0)
                                *frameBuffer = tintLookupTable[*frameBuffer];
                            --pixels;
                            ++frameBuffer;
                        }
                        frameBuffer += pitch;
                        pixels -= gfxPitch;
                    }
                    break;

                case INK_MASKED:
                    while (height--) {
                        uint16 *activePalette = fullPalette[*lineBuffer];
                        lineBuffer++;
                        int32 w = width;
                        while (w--) {
                            if (*pixels > 0 && *frameBuffer == maskColor)
                                *frameBuffer = activePalette[*pixels];
                            --pixels;
                            ++frameBuffer;
                        }
                        frameBuffer += pitch;
                        pixels -= gfxPitch;
                    }
                    break;

                case INK_UNMASKED:
                    while (height--) {
                        uint16 *activePalette = fullPalette[*lineBuffer];
                        lineBuffer++;
                        int32 w = width;
                        while (w--) {
                            if (*pixels > 0 && *frameBuffer != maskColor)
                                *frameBuffer = activePalette[*pixels];
                            --pixels;
                            ++frameBuffer;
                        }
                        frameBuffer += pitch;
                        pixels -= gfxPitch;
                    }
                    break;
            }
            break;
    }
#endif
}
void RSDK::DrawSpriteRotozoom(int32 x, int32 y, int32 pivotX, int32 pivotY, int32 width, int32 height, int32 sprX, int32 sprY, int32 scaleX,
                              int32 scaleY, int32 direction, int16 rotation, int32 inkEffect, int32 alpha, int32 sheetID)
{
    switch (inkEffect) {
        default: break;
        case INK_ALPHA:
            if (alpha > 0xFF)
                inkEffect = INK_NONE;
            else if (alpha <= 0)
                return;
            break;

        case INK_ADD:
        case INK_SUB:
            if (alpha > 0xFF)
                alpha = 0xFF;
            else if (alpha <= 0)
                return;
            break;

        case INK_TINT:
            if (!tintLookupTable)
                return;
            break;
    }

    int32 angle = 0x200 - (rotation & 0x1FF);
    if (!(rotation & 0x1FF))
        angle = rotation & 0x1FF;

#if RETRO_PLATFORM == RETRO_KALLISTIOS
    GFXSurface *surface = &gfxSurface[sheetID];

    if (surface->texture == nullptr) {
        return;
    }

    if (!RenderDevice::SupportedInk(inkEffect)) {
        return;
    }

    if (direction < FLIP_NONE || direction > FLIP_XY) {
        return;
    }

    // DCFIXME: we should avoid using floating point for scaling if possible
    const auto scaleXf = (float)scaleX / 512.0f;
    const auto scaleYf = (float)scaleY / 512.0f;

    const auto scaledPivotX = (int32)((float)pivotX * scaleXf);
    const auto scaledPivotY = (int32)((float)pivotY * scaleYf);

    const auto scaledWidth = (int32)((float)width * scaleXf);
    const auto scaledHeight = (int32)((float)height * scaleYf);

    int32 newX;
    int32 newY;

    if (direction & FLIP_X) {
        newX = x - scaledWidth - scaledPivotX;
    } else {
        newX = x + scaledPivotX;
    }

    if (direction & FLIP_Y) {
        newY = y - scaledHeight - scaledPivotY;
    } else {
        newY = y + scaledPivotY;
    }

    int32 marginLeft   = 0;
    int32 marginRight  = 0;
    int32 marginTop    = 0;
    int32 marginBottom = 0;

#if RETRO_PLATFORM == RETRO_KALLISTIOS && defined(KOS_HARDWARE_RENDERER)
    if (angle != 0) {
        // Rotated sprites can extend well beyond their unrotated bounds.
        // Use the rotation center (x, y) and a generous radius for the cull test.
        int32 maxExtent = scaledWidth + scaledHeight + abs(scaledPivotX) + abs(scaledPivotY);
        if (x + maxExtent <= currentScreen->clipBound_X1 || x - maxExtent >= currentScreen->clipBound_X2
            || y + maxExtent <= currentScreen->clipBound_Y1 || y - maxExtent >= currentScreen->clipBound_Y2)
            return;
    } else
#endif
    {
        if (newX + scaledWidth > currentScreen->clipBound_X2)
            marginRight = (newX + scaledWidth) - currentScreen->clipBound_X2;
        if (newX < currentScreen->clipBound_X1)
            marginLeft = currentScreen->clipBound_X1 - newX;
        if (newY + scaledHeight > currentScreen->clipBound_Y2)
            marginBottom = (newY + scaledHeight) - currentScreen->clipBound_Y2;
        if (newY < currentScreen->clipBound_Y1)
            marginTop = currentScreen->clipBound_Y1 - newY;
    }

    int32 widthClipped  = scaledWidth - marginLeft - marginRight;
    int32 heightClipped = scaledHeight - marginTop - marginBottom;

    if (widthClipped <= 0 || heightClipped <= 0)
        return;

    validDraw = true;

    int32 sprX0;
    int32 sprX1;
    int32 sprY0;
    int32 sprY1;

    if (angle == 0) {
        // Non-rotated: apply clip margins to position and UVs
        if (direction & FLIP_X) {
            sprX0 = sprX + width  - marginLeft * width / scaledWidth;
            sprX1 = sprX          + marginRight * width / scaledWidth;
        } else {
            sprX0 = sprX          + marginLeft * width / scaledWidth;
            sprX1 = sprX + width  - marginRight * width / scaledWidth;
        }

        if (direction & FLIP_Y) {
            sprY0 = sprY + height - marginTop * height / scaledHeight;
            sprY1 = sprY          + marginBottom * height / scaledHeight;
        } else {
            sprY0 = sprY          + marginTop * height / scaledHeight;
            sprY1 = sprY + height - marginBottom * height / scaledHeight;
        }

        int32 drawX = newX + marginLeft;
        int32 drawY = newY + marginTop;

        if (inkEffect == INK_NONE || inkEffect == INK_FLASH)
            alpha = 0xFF;
        if (inkEffect == INK_FLASH_GIGA) {
            lastFlashGigaAlpha = (uint8)alpha;
            alpha = 0xFF;
        }

        if ((inkEffect != INK_SUB) && (inkEffect != INK_ADD) && (alpha == 0xFF)) {
            RenderDevice::PrepareTexturedPolyPT(drawY, inkEffect, surface);
            RenderDevice::DrawTexturedPolyPT(
                    drawX, drawY,
                    drawX, drawY,
                    widthClipped, heightClipped,
                    sprX0, sprX1,
                    sprY0, sprY1,
                    0,
                    alpha,
                    surface);
        } else {
            RenderDevice::PrepareTexturedPolyTR(drawY, inkEffect, surface);
            RenderDevice::DrawTexturedPolyTR(
                    drawX, drawY,
                    drawX, drawY,
                    widthClipped, heightClipped,
                    sprX0, sprX1,
                    sprY0, sprY1,
                    0,
                    alpha,
                    surface);
        }
    } else {
        // Rotated: can't clip to bounds, draw full sprite if any part is visible
        if (direction & FLIP_X) {
            sprX0 = sprX + width;
            sprX1 = sprX;
        } else {
            sprX0 = sprX;
            sprX1 = sprX + width;
        }

        if (direction & FLIP_Y) {
            sprY0 = sprY + height;
            sprY1 = sprY;
        } else {
            sprY0 = sprY;
            sprY1 = sprY + height;
        }

        if (inkEffect == INK_NONE || inkEffect == INK_FLASH)
            alpha = 0xFF;
        if (inkEffect == INK_FLASH_GIGA) {
            lastFlashGigaAlpha = (uint8)alpha;
            alpha = 0xFF;
        }

        if ((inkEffect != INK_SUB) && (inkEffect != INK_ADD) && (alpha == 0xFF)) {
            RenderDevice::PrepareTexturedPolyPT(newY + marginTop, inkEffect, surface);
            RenderDevice::DrawTexturedPolyPT(
                    newX, newY,
                    x, y,
                    scaledWidth, scaledHeight,
                    sprX0, sprX1,
                    sprY0, sprY1,
                    512 - angle,
                    alpha,
                    surface);
        } else {
            RenderDevice::PrepareTexturedPolyTR(newY + marginTop, inkEffect, surface);
            RenderDevice::DrawTexturedPolyTR(
                    newX, newY,
                    x, y,
                    scaledWidth, scaledHeight,
                    sprX0, sprX1,
                    sprY0, sprY1,
                    512 - angle,
                    alpha,
                    surface);
        }
    }
#else
    int32 sine        = sin512LookupTable[angle];
    int32 cosine      = cos512LookupTable[angle];
    int32 fullScaleXS = scaleX * sine >> 9;
    int32 fullScaleXC = scaleX * cosine >> 9;
    int32 fullScaleYS = scaleY * sine >> 9;
    int32 fullScaleYC = scaleY * cosine >> 9;

    int32 posX[4];
    int32 posY[4];
    int32 sprXPos = TO_FIXED(sprX - pivotX);
    int32 sprYPos = TO_FIXED(sprY - pivotY);

    int32 xMax     = 0;
    int32 scaledX1 = 0;
    int32 scaledX2 = 0;
    int32 scaledY1 = 0;
    int32 scaledY2 = 0;
    switch (direction) {
        default:
        case FLIP_NONE: {
            scaledX1 = fullScaleXS * (pivotX - 2);
            scaledX2 = fullScaleXC * (pivotX - 2);
            scaledY1 = fullScaleYS * (pivotY - 2);
            scaledY2 = fullScaleYC * (pivotY - 2);
            xMax     = pivotX + 2 + width;
            posX[0]  = x + ((scaledX2 + scaledY1) >> 9);
            posY[0]  = y + ((fullScaleYC * (pivotY - 2) - scaledX1) >> 9);
            break;
        }

        case FLIP_X: {
            scaledX1 = fullScaleXS * (2 - pivotX);
            scaledX2 = fullScaleXC * (2 - pivotX);
            scaledY1 = fullScaleYS * (pivotY - 2);
            scaledY2 = fullScaleYC * (pivotY - 2);
            xMax     = -2 - pivotX - width;
            posX[0]  = x + ((scaledX2 + scaledY1) >> 9);
            posY[0]  = y + ((fullScaleYC * (pivotY - 2) - scaledX1) >> 9);
            break;
        }

        case FLIP_Y:
        case FLIP_XY: break;
    }

    int32 scaledXMaxS = fullScaleXS * xMax;
    int32 scaledXMaxC = fullScaleXC * xMax;
    int32 scaledYMaxC = fullScaleYC * (pivotY + 2 + height);
    int32 scaledYMaxS = fullScaleYS * (pivotY + 2 + height);
    posX[1]           = x + ((scaledXMaxC + scaledY1) >> 9);
    posY[1]           = y + ((scaledY2 - scaledXMaxS) >> 9);
    posX[2]           = x + ((scaledYMaxS + scaledX2) >> 9);
    posY[2]           = y + ((scaledYMaxC - scaledX1) >> 9);
    posX[3]           = x + ((scaledXMaxC + scaledYMaxS) >> 9);
    posY[3]           = y + ((scaledYMaxC - scaledXMaxS) >> 9);

    int32 left = currentScreen->pitch;
    for (int32 i = 0; i < 4; ++i) {
        if (posX[i] < left)
            left = posX[i];
    }
    if (left < currentScreen->clipBound_X1)
        left = currentScreen->clipBound_X1;

    int32 right = 0;
    for (int32 i = 0; i < 4; ++i) {
        if (posX[i] > right)
            right = posX[i];
    }
    if (right > currentScreen->clipBound_X2)
        right = currentScreen->clipBound_X2;

    int32 top = currentScreen->size.y;
    for (int32 i = 0; i < 4; ++i) {
        if (posY[i] < top)
            top = posY[i];
    }
    if (top < currentScreen->clipBound_Y1)
        top = currentScreen->clipBound_Y1;

    int32 bottom = 0;
    for (int32 i = 0; i < 4; ++i) {
        if (posY[i] > bottom)
            bottom = posY[i];
    }
    if (bottom > currentScreen->clipBound_Y2)
        bottom = currentScreen->clipBound_Y2;

    int32 xSize = right - left;
    int32 ySize = bottom - top;
    if (xSize >= 1 && ySize >= 1) {
        GFXSurface *surface = &gfxSurface[sheetID];

        int32 fullX         = TO_FIXED(sprX + width);
        int32 fullY         = TO_FIXED(sprY + height);
        validDraw           = true;
        int32 fullScaleX    = (int32)((512.0 / (float)scaleX) * 512.0);
        int32 fullScaleY    = (int32)((512.0 / (float)scaleY) * 512.0);
        int32 deltaXLen     = fullScaleX * sine >> 2;
        int32 deltaX        = fullScaleX * cosine >> 2;
        int32 pitch         = currentScreen->pitch - xSize;
        int32 deltaYLen     = fullScaleY * cosine >> 2;
        int32 deltaY        = fullScaleY * sine >> 2;
        int32 lineSize      = surface->lineSize;
        uint8 *lineBuffer   = &gfxLineBuffer[top];
        int32 xLen          = left - x;
        int32 yLen          = top - y;
        uint8 *pixels       = surface->pixels;
        uint16 *frameBuffer = &currentScreen->frameBuffer[left + (top * currentScreen->pitch)];
        int32 fullSprX      = TO_FIXED(sprX) - 1;
        int32 fullSprY      = TO_FIXED(sprY) - 1;

        int32 drawX = 0, drawY = 0;
        if (direction == FLIP_X) {
            drawX     = sprXPos + deltaXLen * yLen - deltaX * xLen - (fullScaleX >> 1);
            drawY     = sprYPos + deltaYLen * yLen + deltaY * xLen;
            deltaX    = -deltaX;
            deltaXLen = -deltaXLen;
        }
        else if (!direction) {
            drawX = sprXPos + deltaX * xLen - deltaXLen * yLen;
            drawY = sprYPos + deltaYLen * yLen + deltaY * xLen;
        }

        switch (inkEffect) {
            case INK_NONE:
                for (int32 y = 0; y < ySize; ++y) {
                    uint16 *activePalette = fullPalette[*lineBuffer++];
                    int32 drawXPos        = drawX;
                    int32 drawYPos        = drawY;
                    for (int32 x = 0; x < xSize; ++x) {
                        if (drawXPos >= fullSprX && drawXPos < fullX && drawYPos >= fullSprY && drawYPos < fullY) {
                            uint8 index = pixels[(FROM_FIXED(drawYPos) << lineSize) + FROM_FIXED(drawXPos)];
                            if (index)
                                *frameBuffer = activePalette[index];
                        }

                        ++frameBuffer;
                        drawXPos += deltaX;
                        drawYPos += deltaY;
                    }

                    drawX -= deltaXLen;
                    drawY += deltaYLen;
                    frameBuffer += pitch;
                }
                break;

            case INK_BLEND:
                for (int32 y = 0; y < ySize; ++y) {
                    uint16 *activePalette = fullPalette[*lineBuffer++];
                    int32 drawXPos        = drawX;
                    int32 drawYPos        = drawY;
                    for (int32 x = 0; x < xSize; ++x) {
                        if (drawXPos >= fullSprX && drawXPos < fullX && drawYPos >= fullSprY && drawYPos < fullY) {
                            uint8 index = pixels[(FROM_FIXED(drawYPos) << lineSize) + FROM_FIXED(drawXPos)];
                            if (index)
                                setPixelBlend(activePalette[index], *frameBuffer);
                        }

                        ++frameBuffer;
                        drawXPos += deltaX;
                        drawYPos += deltaY;
                    }

                    drawX -= deltaXLen;
                    drawY += deltaYLen;
                    frameBuffer += pitch;
                }
                break;

            case INK_ALPHA: {
                uint16 *fbufferBlend = &blendLookupTable[0x20 * (0xFF - alpha)];
                uint16 *pixelBlend   = &blendLookupTable[0x20 * alpha];

                for (int32 y = 0; y < ySize; ++y) {
                    uint16 *activePalette = fullPalette[*lineBuffer++];
                    int32 drawXPos        = drawX;
                    int32 drawYPos        = drawY;
                    for (int32 x = 0; x < xSize; ++x) {
                        if (drawXPos >= fullSprX && drawXPos < fullX && drawYPos >= fullSprY && drawYPos < fullY) {
                            uint8 index = pixels[(FROM_FIXED(drawYPos) << lineSize) + FROM_FIXED(drawXPos)];
                            if (index) {
                                setPixelAlpha(activePalette[index], *frameBuffer, alpha);
                            }
                        }

                        ++frameBuffer;
                        drawXPos += deltaX;
                        drawYPos += deltaY;
                    }

                    drawX -= deltaXLen;
                    drawY += deltaYLen;
                    frameBuffer += pitch;
                }
                break;
            }

            case INK_ADD: {
                uint16 *blendTablePtr = &blendLookupTable[0x20 * alpha];
                for (int32 y = 0; y < ySize; ++y) {
                    uint16 *activePalette = fullPalette[*lineBuffer++];
                    int32 drawXPos        = drawX;
                    int32 drawYPos        = drawY;
                    for (int32 x = 0; x < xSize; ++x) {
                        if (drawXPos >= fullSprX && drawXPos < fullX && drawYPos >= fullSprY && drawYPos < fullY) {
                            uint8 index = pixels[(FROM_FIXED(drawYPos) << lineSize) + FROM_FIXED(drawXPos)];
                            if (index) {
                                setPixelAdditive(activePalette[index], *frameBuffer);
                            }
                        }

                        ++frameBuffer;
                        drawXPos += deltaX;
                        drawYPos += deltaY;
                    }

                    drawX -= deltaXLen;
                    drawY += deltaYLen;
                    frameBuffer += pitch;
                }
                break;
            }

            case INK_SUB: {
                uint16 *subBlendTable = &subtractLookupTable[0x20 * alpha];
                for (int32 y = 0; y < ySize; ++y) {
                    uint16 *activePalette = fullPalette[*lineBuffer++];
                    int32 drawXPos        = drawX;
                    int32 drawYPos        = drawY;
                    for (int32 x = 0; x < xSize; ++x) {
                        if (drawXPos >= fullSprX && drawXPos < fullX && drawYPos >= fullSprY && drawYPos < fullY) {
                            uint8 index = pixels[(FROM_FIXED(drawYPos) << lineSize) + FROM_FIXED(drawXPos)];
                            if (index) {
                                setPixelSubtractive(activePalette[index], *frameBuffer);
                            }
                        }

                        ++frameBuffer;
                        drawXPos += deltaX;
                        drawYPos += deltaY;
                    }

                    drawX -= deltaXLen;
                    drawY += deltaYLen;
                    frameBuffer += pitch;
                }
                break;
            }

            case INK_TINT:
                for (int32 y = 0; y < ySize; ++y) {
                    int32 drawXPos = drawX;
                    int32 drawYPos = drawY;
                    for (int32 x = 0; x < xSize; ++x) {
                        if (drawXPos >= fullSprX && drawXPos < fullX && drawYPos >= fullSprY && drawYPos < fullY) {
                            uint8 index = pixels[(FROM_FIXED(drawYPos) << lineSize) + FROM_FIXED(drawXPos)];
                            if (index)
                                *frameBuffer = tintLookupTable[*frameBuffer];
                        }

                        ++frameBuffer;
                        drawXPos += deltaX;
                        drawYPos += deltaY;
                    }

                    drawX -= deltaXLen;
                    drawY += deltaYLen;
                    frameBuffer += pitch;
                }
                break;

            case INK_MASKED:
                for (int32 y = 0; y < ySize; ++y) {
                    uint16 *activePalette = fullPalette[*lineBuffer++];
                    int32 drawXPos        = drawX;
                    int32 drawYPos        = drawY;
                    for (int32 x = 0; x < xSize; ++x) {
                        if (drawXPos >= fullSprX && drawXPos < fullX && drawYPos >= fullSprY && drawYPos < fullY) {
                            uint8 index = pixels[(FROM_FIXED(drawYPos) << lineSize) + FROM_FIXED(drawXPos)];
                            if (index && *frameBuffer == maskColor)
                                *frameBuffer = activePalette[index];
                        }

                        ++frameBuffer;
                        drawXPos += deltaX;
                        drawYPos += deltaY;
                    }

                    drawX -= deltaXLen;
                    drawY += deltaYLen;
                    frameBuffer += pitch;
                }
                break;

            case INK_UNMASKED:
                for (int32 y = 0; y < ySize; ++y) {
                    uint16 *activePalette = fullPalette[*lineBuffer++];
                    int32 drawXPos        = drawX;
                    int32 drawYPos        = drawY;
                    for (int32 x = 0; x < xSize; ++x) {
                        if (drawXPos >= fullSprX && drawXPos < fullX && drawYPos >= fullSprY && drawYPos < fullY) {
                            uint8 index = pixels[(FROM_FIXED(drawYPos) << lineSize) + FROM_FIXED(drawXPos)];
                            if (index && *frameBuffer != maskColor)
                                *frameBuffer = activePalette[index];
                        }

                        ++frameBuffer;
                        drawXPos += deltaX;
                        drawYPos += deltaY;
                    }

                    drawX -= deltaXLen;
                    drawY += deltaYLen;
                    frameBuffer += pitch;
                }
                break;
        }
    }
#endif
}

void RSDK::DrawDeformedSprite(uint16 sheetID, int32 inkEffect, int32 alpha)
{
    switch (inkEffect) {
        default: break;
        case INK_ALPHA:
            if (alpha > 0xFF)
                inkEffect = INK_NONE;
            else if (alpha <= 0)
                return;
            break;

        case INK_ADD:
        case INK_SUB:
            if (alpha > 0xFF)
                alpha = 0xFF;
            else if (alpha <= 0)
                return;
            break;

        case INK_TINT:
            if (!tintLookupTable)
                return;
            break;
    }

#if RETRO_PLATFORM == RETRO_KALLISTIOS && defined(KOS_HARDWARE_RENDERER)
    validDraw = true;

    if (inkEffect == INK_MASKED)
        return;

    GFXSurface *surface = &gfxSurface[sheetID];
    if (!surface->texture)
        return;

    int32 clipY1  = currentScreen->clipBound_Y1;
    int32 clipY2  = currentScreen->clipBound_Y2;
    int32 screenW = currentScreen->pitch;

    uint32 argb = ((uint32)alpha << 24) | 0x00FFFFFF;

    RenderDevice::PrepareTexturedPolyTREX(clipY1, inkEffect, surface);

    float z      = RenderDevice::GetDepth();
    int32 stripH = 4;
    float texW   = (float)surface->width;
    float texH   = (float)surface->height;

    float baseV = fmodf((float)scanlines[clipY1].position.y / 65536.0f, texH);
    if (baseV < 0.0f)
        baseV += texH;

    for (int32 y = clipY1; y < clipY2; y += stripH) {
        int32 botY = y + stripH;
        if (botY > clipY2)
            botY = clipY2;

        int32 midY = (y + botY) >> 1;
        ScanlineInfo *midScan = &scanlines[midY];

        float rawX0 = (float)midScan->position.x / 65536.0f;
        float scale = (float)midScan->deform.x / 65536.0f;
        float span  = (float)screenW * scale;
        float sprX0 = fmodf(rawX0, texW);
        if (sprX0 < 0.0f)
            sprX0 += texW;
        float sprX1 = sprX0 + span;

        float sprY0 = fmodf(baseV + (float)(y - clipY1), texH);
        if (sprY0 < 0.0f)
            sprY0 += texH;
        float sprY1 = sprY0 + (float)(botY - y);

        Vector4f ul = { 0.0f, (float)y, z, 1.0f };
        Vector4f ur = { (float)screenW, (float)y, z, 1.0f };
        Vector4f ll = { 0.0f, (float)botY, z, 1.0f };
        Vector4f lr = { (float)screenW, (float)botY, z, 1.0f };

        RenderDevice::DrawFloorTexturedPolyTREx(
            ul, ur, ll, lr,
            sprX0, sprX1, sprY0, sprY1,
            surface, argb, 0x00000000
        );
    }
#else
    validDraw              = true;
    GFXSurface *surface    = &gfxSurface[sheetID];
    uint8 *pixels          = surface->pixels;
    int32 clipY1           = currentScreen->clipBound_Y1;
    ScanlineInfo *scanline = &scanlines[clipY1];
    uint16 *frameBuffer    = &currentScreen->frameBuffer[clipY1 * currentScreen->pitch];
    uint8 *lineBuffer      = &gfxLineBuffer[clipY1];
    int32 width            = surface->width - 1;
    int32 height           = surface->height - 1;
    int32 lineSize         = surface->lineSize;

    switch (inkEffect) {
        case INK_NONE:
            for (; clipY1 < currentScreen->clipBound_Y2; ++clipY1) {
                uint16 *activePalette = fullPalette[*lineBuffer++];
                int32 lx              = scanline->position.x;
                int32 ly              = scanline->position.y;
                int32 dx              = scanline->deform.x;
                int32 dy              = scanline->deform.y;
                for (int32 i = 0; i < currentScreen->pitch; ++i) {
                    uint8 palIndex = pixels[((FROM_FIXED(ly) & height) << lineSize) + (FROM_FIXED(lx) & width)];
                    if (palIndex)
                        *frameBuffer = activePalette[palIndex];

                    lx += dx;
                    ly += dy;
                    ++frameBuffer;
                }
                ++scanline;
            }
            break;

        case INK_BLEND:
            for (; clipY1 < currentScreen->clipBound_Y2; ++clipY1) {
                uint16 *activePalette = fullPalette[*lineBuffer++];
                int32 lx              = scanline->position.x;
                int32 ly              = scanline->position.y;
                int32 dx              = scanline->deform.x;
                int32 dy              = scanline->deform.y;
                for (int32 i = 0; i < currentScreen->pitch; ++i) {
                    uint8 palIndex = pixels[((FROM_FIXED(ly) & height) << lineSize) + (FROM_FIXED(lx) & width)];
                    if (palIndex)
                        setPixelBlend(activePalette[palIndex], *frameBuffer);

                    lx += dx;
                    ly += dy;
                    ++frameBuffer;
                }
                ++scanline;
            }
            break;

        case INK_ALPHA: {
            uint16 *fbufferBlend = &blendLookupTable[0x20 * (0xFF - alpha)];
            uint16 *pixelBlend   = &blendLookupTable[0x20 * alpha];

            for (; clipY1 < currentScreen->clipBound_Y2; ++clipY1) {
                uint16 *activePalette = fullPalette[*lineBuffer++];
                int32 lx              = scanline->position.x;
                int32 ly              = scanline->position.y;
                int32 dx              = scanline->deform.x;
                int32 dy              = scanline->deform.y;
                for (int32 i = 0; i < currentScreen->pitch; ++i) {
                    uint8 palIndex = pixels[((FROM_FIXED(ly) & height) << lineSize) + (FROM_FIXED(lx) & width)];
                    if (palIndex) {
                        setPixelAlpha(activePalette[palIndex], *frameBuffer, alpha);
                    }

                    lx += dx;
                    ly += dy;
                    ++frameBuffer;
                }
                ++scanline;
            }
            break;
        }

        case INK_ADD: {
            uint16 *blendTablePtr = &blendLookupTable[0x20 * alpha];

            for (; clipY1 < currentScreen->clipBound_Y2; ++clipY1) {
                uint16 *activePalette = fullPalette[*lineBuffer++];
                int32 lx              = scanline->position.x;
                int32 ly              = scanline->position.y;
                int32 dx              = scanline->deform.x;
                int32 dy              = scanline->deform.y;
                for (int32 i = 0; i < currentScreen->pitch; ++i) {
                    uint8 palIndex = pixels[((FROM_FIXED(ly) & height) << lineSize) + (FROM_FIXED(lx) & width)];
                    if (palIndex) {
                        setPixelAdditive(activePalette[palIndex], *frameBuffer);
                    }

                    lx += dx;
                    ly += dy;
                    ++frameBuffer;
                }
                ++scanline;
            }
            break;
        }

        case INK_SUB: {
            uint16 *subBlendTable = &subtractLookupTable[0x20 * alpha];

            for (; clipY1 < currentScreen->clipBound_Y2; ++clipY1) {
                uint16 *activePalette = fullPalette[*lineBuffer++];
                int32 lx              = scanline->position.x;
                int32 ly              = scanline->position.y;
                int32 dx              = scanline->deform.x;
                int32 dy              = scanline->deform.y;
                for (int32 i = 0; i < currentScreen->pitch; ++i) {
                    uint8 palIndex = pixels[((FROM_FIXED(ly) & height) << lineSize) + (FROM_FIXED(lx) & width)];
                    if (palIndex) {
                        setPixelSubtractive(activePalette[palIndex], *frameBuffer);
                    }
                    lx += dx;
                    ly += dy;
                    ++frameBuffer;
                }
                ++scanline;
            }
            break;
        }

        case INK_TINT:
            for (; clipY1 < currentScreen->clipBound_Y2; ++clipY1) {
                int32 lx = scanline->position.x;
                int32 ly = scanline->position.y;
                int32 dx = scanline->deform.x;
                int32 dy = scanline->deform.y;
                for (int32 i = 0; i < currentScreen->pitch; ++i) {
                    uint8 palIndex = pixels[((FROM_FIXED(ly) & height) << lineSize) + (FROM_FIXED(lx) & width)];
                    if (palIndex)
                        *frameBuffer = tintLookupTable[*frameBuffer];
                    lx += dx;
                    ly += dy;
                    ++frameBuffer;
                }
                ++scanline;
            }
            break;

        case INK_MASKED:
            for (; clipY1 < currentScreen->clipBound_Y2; ++clipY1) {
                uint16 *activePalette = fullPalette[*lineBuffer++];
                int32 lx              = scanline->position.x;
                int32 ly              = scanline->position.y;
                int32 dx              = scanline->deform.x;
                int32 dy              = scanline->deform.y;
                for (int32 i = 0; i < currentScreen->pitch; ++i) {
                    uint8 palIndex = pixels[((FROM_FIXED(ly) & height) << lineSize) + (FROM_FIXED(lx) & width)];
                    if (palIndex && *frameBuffer == maskColor)
                        *frameBuffer = activePalette[palIndex];
                    lx += dx;
                    ly += dy;
                    ++frameBuffer;
                }
                ++scanline;
            }
            break;

        case INK_UNMASKED:
            for (; clipY1 < currentScreen->clipBound_Y2; ++clipY1) {
                uint16 *activePalette = fullPalette[*lineBuffer++];
                int32 lx              = scanline->position.x;
                int32 ly              = scanline->position.y;
                int32 dx              = scanline->deform.x;
                int32 dy              = scanline->deform.y;
                for (int32 i = 0; i < currentScreen->pitch; ++i) {
                    uint8 palIndex = pixels[((FROM_FIXED(ly) & height) << lineSize) + (FROM_FIXED(lx) & width)];
                    if (palIndex && *frameBuffer != maskColor)
                        *frameBuffer = activePalette[palIndex];
                    lx += dx;
                    ly += dy;
                    ++frameBuffer;
                }
                ++scanline;
            }
            break;
    }
#endif
}

void RSDK::DrawTile(uint16 *tiles, int32 countX, int32 countY, Vector2 *position, Vector2 *offset, bool32 screenRelative)
{
    if (tiles) {
        if (!position)
            position = &sceneInfo.entity->position;

        int32 x = FROM_FIXED(position->x);
        int32 y = FROM_FIXED(position->y);
        if (!screenRelative) {
            x -= currentScreen->position.x;
            y -= currentScreen->position.y;
        }

        int32 pivotX = 0;
        int32 pivotY = 0;
        switch (sceneInfo.entity->drawFX) {
            case FX_NONE:
            case FX_FLIP:
                if (offset) {
                    pivotX = x - (offset->x >> 17);
                    pivotY = y - (offset->y >> 17);
                }
                else {
                    pivotX = x - (countX * (TILE_SIZE >> 1));
                    pivotY = y - (countY * (TILE_SIZE >> 1));
                }

#if RETRO_PLATFORM == RETRO_KALLISTIOS
                for (int32 ty = 0; ty < countY; ++ty) {
                    const int32 screenY = (ty * TILE_SIZE) + pivotY;

                    for (int32 tx = 0; tx < countX; ++tx) {
                        uint16 tile = tiles[tx + (ty * countX)];

                        if (tile >= 0xFFFF) {
                            continue;
                        }

                        // DCFIXME: copy/pasted code for tile atlas stuff
                        tile &= 0xFFF;
                        const int32 flip = tile / TILE_COUNT;
                        tile %= TILE_COUNT;

                        int32 sheetID;
                        int32 tilesetX;
                        int32 tilesetY;

                        const AniTileState* aniTileState = AniTileTracker::GetAniTile(tile);

                        if (aniTileState != nullptr) {
                            sheetID = aniTileState->sheetID;
                            tilesetX = aniTileState->u;
                            tilesetY = aniTileState->v;
                        }
                        else {
                            sheetID = 0;
                            tilesetX = TILE_SIZE * (static_cast<int32>(tile) % KOS_ATLAS_WIDTH_TILES);
                            tilesetY = TILE_SIZE * (static_cast<int32>(tile) / KOS_ATLAS_WIDTH_TILES);
                        }

                        const int32 screenX = (tx * TILE_SIZE) + pivotX;

                        DrawSpriteFlipped(screenX, screenY,
                                          TILE_SIZE, TILE_SIZE,
                                          tilesetX, tilesetY,
                                          flip,
                                          sceneInfo.entity->inkEffect,
                                          sceneInfo.entity->alpha,
                                          sheetID);
                    }
                }
#else
                for (int32 ty = 0; ty < countY; ++ty) {
                    for (int32 tx = 0; tx < countX; ++tx) {
                        uint16 tile = tiles[tx + (ty * countX)];
                        if (tile < 0xFFFF) {
                            DrawSpriteFlipped((tx * TILE_SIZE) + pivotX, (ty * TILE_SIZE) + pivotY, TILE_SIZE, TILE_SIZE, 0,
                                              TILE_SIZE * (tile & 0xFFF), FLIP_NONE, sceneInfo.entity->inkEffect, sceneInfo.entity->alpha, 0);
                        }
                    }
                }
#endif
                break;

            case FX_ROTATE: // Flip
            case FX_ROTATE | FX_FLIP:
                if (offset) {
                    pivotX = -(offset->x >> 17);
                    pivotY = -(offset->y >> 17);
                }
                else {
                    pivotX = -(countX * (TILE_SIZE >> 1));
                    pivotY = -(countY * (TILE_SIZE >> 1));
                }

#if RETRO_PLATFORM == RETRO_KALLISTIOS
                for (int32 ty = 0; ty < countY; ++ty) {
                    const int32 screenY = y + (ty * TILE_SIZE);

                    for (int32 tx = 0; tx < countX; ++tx) {
                        uint16 tile = tiles[tx + (ty * countX)];

                        if (tile >= 0xFFFF) {
                            continue;
                        }

                        // DCFIXME: copy/pasted code for tile atlas stuff
                        tile &= 0xFFF;
                        int32 flip = tile / TILE_COUNT;
                        tile %= TILE_COUNT;

                        int32 sheetID;
                        int32 tilesetX;
                        int32 tilesetY;

                        const AniTileState* aniTileState = AniTileTracker::GetAniTile(tile);

                        if (aniTileState != nullptr) {
                            sheetID = aniTileState->sheetID;
                            tilesetX = aniTileState->u;
                            tilesetY = aniTileState->v;
                        }
                        else {
                            sheetID = 0;
                            tilesetX = TILE_SIZE * (static_cast<int32>(tile) % KOS_ATLAS_WIDTH_TILES);
                            tilesetY = TILE_SIZE * (static_cast<int32>(tile) / KOS_ATLAS_WIDTH_TILES);
                        }

                        const int32 screenX = x + (tx * TILE_SIZE);

                        int32 rotation = sceneInfo.entity->rotation;

                        if (flip & FLIP_Y) {
                            rotation += 0x100;
                            flip ^= FLIP_Y;
                        }

                        DrawSpriteRotozoom(screenX, screenY,
                                           pivotX, pivotY,
                                           TILE_SIZE, TILE_SIZE,
                                           tilesetX, tilesetY,
                                           0x200, 0x200,
                                           flip & FLIP_X,
                                           static_cast<int16>(rotation),
                                           sceneInfo.entity->inkEffect,
                                           sceneInfo.entity->alpha,
                                           sheetID);
                    }
                }
#else
                for (int32 ty = 0; ty < countY; ++ty) {
                    for (int32 tx = 0; tx < countX; ++tx) {
                        uint16 tile = tiles[tx + (ty * countX)];
                        if (tile < 0xFFFF) {
                            switch ((tile >> 10) & 3) {
                                case FLIP_NONE:
                                    DrawSpriteRotozoom(x + (tx * TILE_SIZE), y + (ty * TILE_SIZE), pivotX, pivotY, TILE_SIZE, TILE_SIZE, 0,
                                                       TILE_SIZE * (tile & 0x3FF), 0x200, 0x200, FLIP_NONE, sceneInfo.entity->rotation,
                                                       sceneInfo.entity->inkEffect, sceneInfo.entity->alpha, 0);
                                    break;

                                case FLIP_X:
                                    DrawSpriteRotozoom(x + (tx * TILE_SIZE), y + (ty * TILE_SIZE), pivotX, pivotY, TILE_SIZE, TILE_SIZE, 0,
                                                       TILE_SIZE * (tile & 0x3FF), 0x200, 0x200, FLIP_X, sceneInfo.entity->rotation,
                                                       sceneInfo.entity->inkEffect, sceneInfo.entity->alpha, 0);
                                    break;

                                case FLIP_Y:
                                    DrawSpriteRotozoom(x + (tx * TILE_SIZE), y + (ty * TILE_SIZE), pivotX, pivotY, TILE_SIZE, TILE_SIZE, 0,
                                                       TILE_SIZE * (tile & 0x3FF), 0x200, 0x200, FLIP_X, sceneInfo.entity->rotation + 0x100,
                                                       sceneInfo.entity->inkEffect, sceneInfo.entity->alpha, 0);
                                    break;

                                case FLIP_XY:
                                    DrawSpriteRotozoom(x + (tx * TILE_SIZE), y + (ty * TILE_SIZE), pivotX, pivotY, TILE_SIZE, TILE_SIZE, 0,
                                                       TILE_SIZE * (tile & 0x3FF), 0x200, 0x200, FLIP_NONE, sceneInfo.entity->rotation + 0x100,
                                                       sceneInfo.entity->inkEffect, sceneInfo.entity->alpha, 0);
                                    break;
                            }
                        }
                    }
                }
#endif
                break;

            case FX_SCALE: // Scale
            case FX_SCALE | FX_FLIP:
                if (offset) {
                    pivotX = -(offset->x >> 17);
                    pivotY = -(offset->y >> 17);
                }
                else {
                    pivotX = -(countX * (TILE_SIZE >> 1));
                    pivotY = -(countY * (TILE_SIZE >> 1));
                }

#if RETRO_PLATFORM == RETRO_KALLISTIOS
                for (int32 ty = 0; ty < countY; ++ty) {
                    const int32 screenY = y + (ty * TILE_SIZE);

                    for (int32 tx = 0; tx < countX; ++tx) {
                        uint16 tile = tiles[tx + (ty * countX)];

                        if (tile >= 0xFFFF) {
                            continue;
                        }

                        // DCFIXME: copy/pasted code for tile atlas stuff
                        tile &= 0xFFF;
                        int32 flip = tile / TILE_COUNT;
                        tile %= TILE_COUNT;

                        int32 sheetID;
                        int32 tilesetX;
                        int32 tilesetY;

                        const AniTileState* aniTileState = AniTileTracker::GetAniTile(tile);

                        if (aniTileState != nullptr) {
                            sheetID = aniTileState->sheetID;
                            tilesetX = aniTileState->u;
                            tilesetY = aniTileState->v;
                        }
                        else {
                            sheetID = 0;
                            tilesetX = TILE_SIZE * (static_cast<int32>(tile) % KOS_ATLAS_WIDTH_TILES);
                            tilesetY = TILE_SIZE * (static_cast<int32>(tile) / KOS_ATLAS_WIDTH_TILES);
                        }

                        const int32 screenX = x + (tx * TILE_SIZE);
                        int32 rotation;

                        if (flip & FLIP_Y) {
                            rotation = 0x100;
                            flip ^= FLIP_Y;
                        } else {
                            rotation = 0;
                        }

                        DrawSpriteRotozoom(screenX, screenY,
                                           pivotX, pivotY,
                                           TILE_SIZE, TILE_SIZE,
                                           tilesetX, tilesetY,
                                           sceneInfo.entity->scale.x, sceneInfo.entity->scale.y,
                                           flip & FLIP_X,
                                           static_cast<int16>(rotation),
                                           sceneInfo.entity->inkEffect,
                                           sceneInfo.entity->alpha,
                                           sheetID);
                    }
                }
#else
                for (int32 ty = 0; ty < countY; ++ty) {
                    for (int32 tx = 0; tx < countX; ++tx) {
                        uint16 tile = tiles[tx + (ty * countX)];
                        if (tile < 0xFFFF) {
                            switch ((tile >> 10) & 3) {
                                case FLIP_NONE:
                                    DrawSpriteRotozoom(x + (tx * TILE_SIZE), y + (ty * TILE_SIZE), pivotX, pivotY, TILE_SIZE, TILE_SIZE, 0,
                                                       TILE_SIZE * (tile & 0x3FF), sceneInfo.entity->scale.x, sceneInfo.entity->scale.y, FLIP_NONE,
                                                       0x000, sceneInfo.entity->inkEffect, sceneInfo.entity->alpha, 0);
                                    break;

                                case FLIP_X:
                                    DrawSpriteRotozoom(x + (tx * TILE_SIZE), y + (ty * TILE_SIZE), pivotX, pivotY, TILE_SIZE, TILE_SIZE, 0,
                                                       TILE_SIZE * (tile & 0x3FF), sceneInfo.entity->scale.x, sceneInfo.entity->scale.y, FLIP_X,
                                                       0x000, sceneInfo.entity->inkEffect, sceneInfo.entity->alpha, 0);
                                    break;

                                case FLIP_Y:
                                    DrawSpriteRotozoom(x + (tx * TILE_SIZE), y + (ty * TILE_SIZE), pivotX, pivotY, TILE_SIZE, TILE_SIZE, 0,
                                                       TILE_SIZE * (tile & 0x3FF), sceneInfo.entity->scale.x, sceneInfo.entity->scale.y, FLIP_X,
                                                       0x100, sceneInfo.entity->inkEffect, sceneInfo.entity->alpha, 0);
                                    break;

                                case FLIP_XY:
                                    DrawSpriteRotozoom(x + (tx * TILE_SIZE), y + (ty * TILE_SIZE), pivotX, pivotY, TILE_SIZE, TILE_SIZE, 0,
                                                       TILE_SIZE * (tile & 0x3FF), sceneInfo.entity->scale.x, sceneInfo.entity->scale.y, FLIP_NONE,
                                                       0x100, sceneInfo.entity->inkEffect, sceneInfo.entity->alpha, 0);
                                    break;
                            }
                        }
                    }
                }
#endif
                break;

            case FX_SCALE | FX_ROTATE: // Flip + Scale + Rotation
            case FX_SCALE | FX_ROTATE | FX_FLIP:
                if (offset) {
                    pivotX = -(offset->x >> 17);
                    pivotY = -(offset->y >> 17);
                }
                else {
                    pivotX = -(countX * (TILE_SIZE >> 1));
                    pivotY = -(countY * (TILE_SIZE >> 1));
                }

#if RETRO_PLATFORM == RETRO_KALLISTIOS
                for (int32 ty = 0; ty < countY; ++ty) {
                    const int32 screenY = y + (ty * TILE_SIZE);

                    for (int32 tx = 0; tx < countX; ++tx) {
                        uint16 tile = tiles[tx + (ty * countX)];

                        if (tile >= 0xFFFF) {
                            continue;
                        }

                        // DCFIXME: copy/pasted code for tile atlas stuff
                        tile &= 0xFFF;
                        int32 flip = tile / TILE_COUNT;
                        tile %= TILE_COUNT;

                        int32 sheetID;
                        int32 tilesetX;
                        int32 tilesetY;

                        const AniTileState* aniTileState = AniTileTracker::GetAniTile(tile);

                        if (aniTileState != nullptr) {
                            sheetID = aniTileState->sheetID;
                            tilesetX = aniTileState->u;
                            tilesetY = aniTileState->v;
                        }
                        else {
                            sheetID = 0;
                            tilesetX = TILE_SIZE * (static_cast<int32>(tile) % KOS_ATLAS_WIDTH_TILES);
                            tilesetY = TILE_SIZE * (static_cast<int32>(tile) / KOS_ATLAS_WIDTH_TILES);
                        }

                        const int32 screenX = x + (tx * TILE_SIZE);
                        int32 rotation = sceneInfo.entity->rotation;

                        if (flip & FLIP_Y) {
                            rotation += 0x100;
                            flip ^= FLIP_Y;
                        }

                        DrawSpriteRotozoom(screenX, screenY,
                                           pivotX, pivotY,
                                           TILE_SIZE, TILE_SIZE,
                                           tilesetX, tilesetY,
                                           sceneInfo.entity->scale.x, sceneInfo.entity->scale.y,
                                           flip & FLIP_X,
                                           static_cast<int16>(rotation),
                                           sceneInfo.entity->inkEffect,
                                           sceneInfo.entity->alpha,
                                           sheetID);
                    }
                }
#else
                for (int32 ty = 0; ty < countY; ++ty) {
                    for (int32 tx = 0; tx < countX; ++tx) {
                        uint16 tile = tiles[tx + (ty * countX)];
                        if (tile < 0xFFFF) {
                            switch ((tile >> 10) & 3) {
                                case FLIP_NONE:
                                    DrawSpriteRotozoom(x + (tx * TILE_SIZE), y + (ty * TILE_SIZE), pivotX, pivotY, TILE_SIZE, TILE_SIZE, 0,
                                                       TILE_SIZE * (tile & 0x3FF), sceneInfo.entity->scale.x, sceneInfo.entity->scale.y, FLIP_NONE,
                                                       sceneInfo.entity->rotation, sceneInfo.entity->inkEffect, sceneInfo.entity->alpha, 0);
                                    break;

                                case FLIP_X:
                                    DrawSpriteRotozoom(x + (tx * TILE_SIZE), y + (ty * TILE_SIZE), pivotX, pivotY, TILE_SIZE, TILE_SIZE, 0,
                                                       TILE_SIZE * (tile & 0x3FF), sceneInfo.entity->scale.x, sceneInfo.entity->scale.y, FLIP_X,
                                                       sceneInfo.entity->rotation, sceneInfo.entity->inkEffect, sceneInfo.entity->alpha, 0);
                                    break;

                                case FLIP_Y:
                                    DrawSpriteRotozoom(x + (tx * TILE_SIZE), y + (ty * TILE_SIZE), pivotX, pivotY, TILE_SIZE, TILE_SIZE, 0,
                                                       TILE_SIZE * (tile & 0x3FF), sceneInfo.entity->scale.x, sceneInfo.entity->scale.y, FLIP_X,
                                                       sceneInfo.entity->rotation + 0x100, sceneInfo.entity->inkEffect, sceneInfo.entity->alpha, 0);
                                    break;

                                case FLIP_XY:
                                    DrawSpriteRotozoom(x + (tx * TILE_SIZE), y + (ty * TILE_SIZE), pivotX, pivotY, TILE_SIZE, TILE_SIZE, 0,
                                                       TILE_SIZE * (tile & 0x3FF), sceneInfo.entity->scale.x, sceneInfo.entity->scale.y, FLIP_NONE,
                                                       sceneInfo.entity->rotation + 0x100, sceneInfo.entity->inkEffect, sceneInfo.entity->alpha, 0);
                                    break;
                            }
                        }
                    }
                }
#endif
                break;
        }
    }
}
void RSDK::DrawAniTile(uint16 sheetID, uint16 tileIndex, uint16 srcX, uint16 srcY, uint16 width, uint16 height)
{
#if RETRO_PLATFORM == RETRO_KALLISTIOS && defined(KOS_HARDWARE_RENDERER)
    AniTileTracker::UpdateAniTile(sheetID, tileIndex, srcX, srcY, width, height);
#else
    if (sheetID < SURFACE_COUNT && tileIndex < TILE_COUNT) {
        GFXSurface *surface = &gfxSurface[sheetID];

        // FLIP_NONE
        uint8 *tilePixels = &tilesetPixels[tileIndex << 8];
        int32 cnt         = 0;
        for (int32 fy = 0; fy < height; fy += TILE_SIZE) {
            uint8 *pixels = &surface->pixels[((fy + srcY) << surface->lineSize) + srcX];
            cnt += ((width - 1) / TILE_SIZE) + 1;
            for (int32 fx = 0; fx < width; fx += TILE_SIZE) {
                uint8 *pixelsPtr = &pixels[fx];
                for (int32 ty = 0; ty < TILE_SIZE; ++ty) {
                    for (int32 tx = 0; tx < TILE_SIZE; ++tx) *tilePixels++ = *pixelsPtr++;

                    pixelsPtr += surface->width - TILE_SIZE;
                }
            }
        }

        // FLIP_X
        uint8 *srcTilePixels = &tilesetPixels[tileIndex << 8];
        if (cnt * TILE_SIZE > 0) {
            tilePixels = &tilesetPixels[(tileIndex << 8) + (FLIP_X * TILESET_SIZE) + (TILE_SIZE - 1)];

            for (int32 i = 0; i < cnt * TILE_SIZE; ++i) {
                for (int32 p = 0; p < TILE_SIZE; ++p) *tilePixels-- = *srcTilePixels++;

                tilePixels += (TILE_SIZE * 2);
            }
        }

        // FLIP_Y
        srcTilePixels = &tilesetPixels[tileIndex << 8];
        if (cnt * TILE_SIZE > 0) {
            int32 index = tileIndex;
            for (int32 i = 0; i < cnt; ++i) {
                tilePixels = &tilesetPixels[(index << 8) + (FLIP_Y * TILESET_SIZE) + (TILE_DATASIZE - TILE_SIZE)];
                for (int32 y = 0; y < TILE_SIZE; ++y) {
                    for (int32 x = 0; x < TILE_SIZE; ++x) *tilePixels++ = *srcTilePixels++;

                    tilePixels -= (TILE_SIZE * 2);
                }
                ++index;
            }
        }

        // FLIP_XY
        srcTilePixels = &tilesetPixels[(tileIndex << 8) + (FLIP_Y * TILESET_SIZE)];
        if (cnt * TILE_SIZE > 0) {
            tilePixels = &tilesetPixels[(tileIndex << 8) + (FLIP_XY * TILESET_SIZE) + (TILE_SIZE - 1)];

            for (int32 i = 0; i < cnt * TILE_SIZE; ++i) {
                for (int32 p = 0; p < TILE_SIZE; ++p) *tilePixels-- = *srcTilePixels++;

                tilePixels += (TILE_SIZE * 2);
            }
        }
    }
#endif
}

void RSDK::DrawString(Animator *animator, Vector2 *position, String *string, int32 startFrame, int32 endFrame, int32 align, int32 spacing,
                      void *unused, Vector2 *charOffsets, bool32 screenRelative)
{
    if (animator && string && animator->frames) {
        if (!position)
            position = &sceneInfo.entity->position;

        Entity *entity = sceneInfo.entity;

        int32 x = FROM_FIXED(position->x);
        int32 y = FROM_FIXED(position->y);
        if (!screenRelative) {
            x -= currentScreen->position.x;
            y -= currentScreen->position.y;
        }

        startFrame = CLAMP(startFrame, 0, string->length - 1);

        if (endFrame <= 0 || endFrame > string->length)
            endFrame = string->length;

        switch (align) {
            case ALIGN_LEFT:
                if (charOffsets) {
                    for (; startFrame < endFrame; ++startFrame) {
                        uint16 curChar = string->chars[startFrame];
                        if (curChar < animator->frameCount) {
                            SpriteFrame *frame = &animator->frames[curChar];
                            DrawSpriteFlipped(x + FROM_FIXED(charOffsets->x), y + frame->pivotY + FROM_FIXED(charOffsets->y), frame->width,
                                              frame->height, frame->sprX, frame->sprY, FLIP_NONE, entity->inkEffect, entity->alpha, frame->sheetID);
                            x += spacing + frame->width;
                            ++charOffsets;
                        }
                    }
                }
                else {
                    for (; startFrame < endFrame; ++startFrame) {
                        uint16 curChar = string->chars[startFrame];
                        if (curChar < animator->frameCount) {
                            SpriteFrame *frame = &animator->frames[curChar];
                            DrawSpriteFlipped(x, y + frame->pivotY, frame->width, frame->height, frame->sprX, frame->sprY, FLIP_NONE,
                                              entity->inkEffect, entity->alpha, frame->sheetID);
                            x += spacing + frame->width;
                        }
                    }
                }
                break;

            case ALIGN_RIGHT: break;

            case ALIGN_CENTER:
                --endFrame;
                if (charOffsets) {
                    for (Vector2 *charOffset = &charOffsets[endFrame]; endFrame >= startFrame; --endFrame) {
                        uint16 curChar = string->chars[endFrame];
                        if (curChar < animator->frameCount) {
                            SpriteFrame *frame = &animator->frames[curChar];
                            DrawSpriteFlipped(x - frame->width + FROM_FIXED(charOffset->x), y + frame->pivotY + FROM_FIXED(charOffset->y),
                                              frame->width, frame->height, frame->sprX, frame->sprY, FLIP_NONE, entity->inkEffect, entity->alpha,
                                              frame->sheetID);
                            x = (x - frame->width) - spacing;
                            --charOffset;
                        }
                    }
                }
                else {
                    for (; endFrame >= startFrame; --endFrame) {
                        uint16 curChar = string->chars[endFrame];
                        if (curChar < animator->frameCount) {
                            SpriteFrame *frame = &animator->frames[curChar];
                            DrawSpriteFlipped(x - frame->width, y + frame->pivotY, frame->width, frame->height, frame->sprX, frame->sprY, FLIP_NONE,
                                              entity->inkEffect, entity->alpha, frame->sheetID);
                            x = (x - frame->width) - spacing;
                        }
                    }
                }
                break;
        }
    }
}
void RSDK::DrawDevString(const char *string, int32 x, int32 y, int32 align, uint32 color)
{
#if RETRO_PLATFORM != RETRO_KALLISTIOS || RETRO_USE_ORIGINAL_CODE
    uint16 color16 = rgb32To16_B[(color >> 0) & 0xFF] | rgb32To16_G[(color >> 8) & 0xFF] | rgb32To16_R[(color >> 16) & 0xFF];
#else
#if RETRO_PLATFORM == RETRO_KALLISTIOS
    uint32 color16 = PACK_RGB888_32(color);
    color16 |= (color16 << 16);
#else
    uint16 color16 = PACK_RGB888_32(color);
#endif
#endif

    int32 charOffset   = 0;
    bool32 linesRemain = true;
    while (linesRemain) {
        linesRemain = false;

        int32 lineSize = 0;
        char cur       = string[charOffset];
        if (cur != '\n') {
            while (cur) {
                cur = string[++charOffset];
                lineSize++;
                if (cur == '\n') {
                    linesRemain = true;
                    break;
                }
            }
        }

        if (y >= 0 && y < currentScreen->size.y - 7) {
            int32 offset = 0;
            switch (align) {
                default:
                case ALIGN_LEFT: offset = 0; break;

                case ALIGN_CENTER: offset = 4 * lineSize; break;

                case ALIGN_RIGHT: offset = 8 * lineSize; break;
            }
            int32 drawX = x - offset;

            const char *curChar = &string[charOffset++ - lineSize];

            for (int32 c = 0; c < lineSize; ++c) {
                if (drawX >= 0 && drawX < currentScreen->size.x - 7) {
#if RETRO_PLATFORM == RETRO_KALLISTIOS && defined(KOS_HARDWARE_RENDERER)
#if DO_480
                    // DCFIXME: vram_s used to avoid creating another texture
                    uint32 *frameBuffer = (uint32_t *)&vram_s[(drawX*2) + ((y*2)) * 640];
                    uint32 *frameBuffer2 = (uint32_t *)&vram_s[(drawX*2) + ((y*2)+1) * 640];

                    if ((*curChar < '\t' || *curChar > '\n') && *curChar != ' ') {
                        uint8 *textStencilPtr = &devTextStencil[8 * *curChar];

                        for (int32 h = 0; h < 8; ++h) {
                            for (int32 w = 0; w < 8; ++w) {
                                if (((*textStencilPtr >> w) & 1) != 0) {
                                    *frameBuffer = color16;
                                    *frameBuffer2 = color16;
                                }
                                ++frameBuffer;
                                ++frameBuffer2;
                            }

                            ++textStencilPtr;
                            frameBuffer += 640 - 8;
                            frameBuffer2 += 640 - 8;
                        }
                    }
#else
                    // DCFIXME: vram_s used to avoid creating another texture
                    uint16 *frameBuffer = &vram_s[drawX + y * 320];

                    if ((*curChar < '\t' || *curChar > '\n') && *curChar != ' ') {
                        uint8 *textStencilPtr = &devTextStencil[8 * *curChar];

                        for (int32 h = 0; h < 8; ++h) {
                            for (int32 w = 0; w < 8; ++w) {
                                if (((*textStencilPtr >> w) & 1) != 0)
                                    *frameBuffer = color16;

                                ++frameBuffer;
                            }

                            ++textStencilPtr;
                            frameBuffer += 320 - 8;
                        }
                    }
#endif
#else
                    uint16 *frameBuffer = &currentScreen->frameBuffer[drawX + y * currentScreen->pitch];

                    if ((*curChar < '\t' || *curChar > '\n') && *curChar != ' ') {
                        uint8 *textStencilPtr = &devTextStencil[0x40 * *curChar];

                        for (int32 h = 0; h < 8; ++h) {
                            for (int32 w = 0; w < 8; ++w) {
                                if (*textStencilPtr)
                                    *frameBuffer = color16;

                                ++textStencilPtr;
                                ++frameBuffer;
                            }

                            frameBuffer += currentScreen->pitch - 8;
                        }
                    }
#endif

                    ++curChar;
                    drawX += 8;
                }
            }
        }

        y += 8;
    }
}
