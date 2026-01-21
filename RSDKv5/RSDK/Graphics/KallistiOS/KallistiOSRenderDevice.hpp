#pragma once
#define KOS_HARDWARE_RENDERER
using ShaderEntry = ShaderEntryBase; // DCFIXME

class RenderDevice : public RenderDeviceBase
{
public:
    struct WindowInfo {
        union {
            struct {
                int32 width;
                int32 height;
                int32 refresh_rate;
            };
        } * displays;
    };
    static WindowInfo displayInfo;

    static bool Init();
    static void CopyFrameBuffer();
    static void FlipScreen();
    static void Release(bool32 isRefresh);

    static void RefreshWindow();
    static void GetWindowSize(int32 *width, int32 *height);

    static void SetupImageTexture(int32 width, int32 height, uint8 *imagePixels);
    static void SetupVideoTexture_YUV420(int32 width, int32 height, uint8 *imagePixels);
    static void SetupVideoTexture_YUV422(int32 width, int32 height, uint8 *imagePixels);
    static void SetupVideoTexture_YUV424(int32 width, int32 height, uint8 *imagePixels);

    static bool ProcessEvents();

    static void InitFPSCap();
    static bool CheckFPSCap();
    static void UpdateFPSCap();

    // Public since it's needed for the ModAPI
    static void LoadShader(const char *fileName, bool32 linear);

    static void SetWindowTitle();
    static bool GetCursorPos(void*);
    static void ShowCursor(bool);

    // KallistiOS only!!!
    static void BeginScene();
    static void EndScene();
    static float GetDepth();
    static void SetDepth(uint32 depth);

#if defined(KOS_HARDWARE_RENDERER)
    static uint32 GetGamePaletteBankIndex(int32 y);
    static uint32 GameToPvrPaletteBankIndex(uint32 gamePaletteBankIndex);
    static void PopulatePvrPalette(uint32 gamePaletteBankIndex, uint32 pvrPaletteBankIndex);
    static bool InkToBlendModes(int inkEffect, int* srcBlend, int* dstBlend);

private:
    static bool PreparePrimitive(int primitiveType,
                                 uint32 gamePaletteBankIndex,
                                 uint32 pvrPaletteBankIndex,
                                 /* int srcBlend,
                                 int dstBlend, */
                                 int inkEffect,
                                 pvr_ptr_t texture);

public:
    static void PrepareTexturedQuadDR(int32 y, const GFXSurface* surface);
    static void DrawTexturedQuadDR(
            int32 x, int32 y,
            int32 width, int32 height,
            int32 sprX0, int32 sprX1,
            int32 sprY0, int32 sprY1,
            const GFXSurface* surface
    );

    static void PrepareTexturedQuadDMA(int32 y, const GFXSurface* surface);
    static void DrawTexturedQuadDMA(
            int32 x, int32 y,
            int32 width, int32 height,
            int32 sprX0, int32 sprX1,
            int32 sprY0, int32 sprY1,
            const GFXSurface* surface
    );

    static void PrepareTexturedPolyDR(int32 y, int inkEffect, const GFXSurface* surface);
    static void DrawTexturedPolyDR(
            int32 x, int32 y,
            int32 ox, int32 oy,
            int32 width, int32 height,
            int32 sprX0, int32 sprX1,
            int32 sprY0, int32 sprY1,
            int32 rotation,
            int32 alpha,
            const GFXSurface *surface
    );

    static void PrepareTexturedPolyDMA(int32 y, int inkEffect, const GFXSurface* surface);
    static void DrawTexturedPolyDMA(
            int32 x, int32 y,
            int32 ox, int32 oy,
            int32 width, int32 height,
            int32 sprX0, int32 sprX1,
            int32 sprY0, int32 sprY1,
            int32 rotation,
            int32 alpha,
            const GFXSurface *surface
    );

    static void PrepareColoredPolyDR(int32 y, int inkEffect);
    static void DrawColoredPolyDR(
            int32 x, int32 y,
            int32 width, int32 height,
            uint32 color
    );

    static void PrepareColoredPolyDMA(int32 y, int inkEffect);
    static void DrawColoredPolyDMA(
            int32 x, int32 y,
            int32 width, int32 height,
            uint32 color
    );

    static void PrepareLinePolyDR(int inkEffect);
    static void DrawLinePolyDR(int x1, int y1, int x2, int y2, int color);

    static void PrepareLinePolyDMA(int inkEffect);
    static void DrawLinePolyDMA(int x1, int y1, int x2, int y2, int color);


    static void PrepareRotoPoly(int inkEffect, pvr_ptr_t texture);
    static void DrawRotoPoly(int x, int y, int w, int h, float u, float v);

    static void PrepareFacePolyDR(int inkEffect);
    static void DrawFacePolyDR(
            Vector2 *vertices, int32 vertCount, 
            int32 faceColor, int32 alpha, 
            uint32 *colors
    );

    static void PrepareFacePolyDMA(int inkEffect);
    static void DrawFacePolyDMA(
            Vector2 *vertices, int32 vertCount, 
            int32 faceColor, int32 alpha, 
            uint32 *colors
    );

    static void DrawTexturedQuadEx(
        const Vector2& upperLeft, const Vector2& upperRight,
        const Vector2& lowerLeft, const Vector2& lowerRight,
        int32 sprX0, int32 sprX1,
        int32 sprY0, int32 sprY1,
        const GFXSurface* surface
    );
#endif

private:
    static bool InitShaders();
    static bool SetupRendering();
    static void InitVertexBuffer();
    static bool InitGraphicsAPI();

    static void GetDisplays();
};
