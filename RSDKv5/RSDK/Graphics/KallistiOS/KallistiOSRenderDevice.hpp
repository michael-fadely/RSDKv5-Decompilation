#pragma once
#define KOS_HARDWARE_RENDERER

#if RETRO_PLATFORM == RETRO_KALLISTIOS
#define DO_480 0
#endif

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

    static void ReleaseImageTexture(void);
    static void DrawImageTexture(float dim);
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
    static void SetDepth(float depth);

#if defined(KOS_HARDWARE_RENDERER)
    static uint32 GetGamePaletteBankIndex(int32 y);
    static uint32 GameToPvrPaletteBankIndex(uint32 gamePaletteBankIndex);
    static void PopulatePvrPalette(uint32 gamePaletteBankIndex, uint32 pvrPaletteBankIndex);
    static bool SupportedInk(int inkEffect);
    static bool InkToBlendModes(int inkEffect, int* srcBlend, int* dstBlend);

private:
    static bool PreparePrimitive(int primitiveType,
                                 uint32 gamePaletteBankIndex,
                                 uint32 pvrPaletteBankIndex,
                                 int inkEffect,
                                 pvr_ptr_t texture);

public:
    static void PrepareTexturedQuadPT(int32 y, const GFXSurface* surface);
    static void DrawTexturedQuadPT(
            int32 x, int32 y,
            int32 width, int32 height,
            int32 sprX0, int32 sprX1,
            int32 sprY0, int32 sprY1,
            const GFXSurface* surface
    );

    static void PrepareTexturedQuadTR(int32 y, const GFXSurface* surface);
    static void DrawTexturedQuadTR(
            int32 x, int32 y,
            int32 width, int32 height,
            int32 sprX0, int32 sprX1,
            int32 sprY0, int32 sprY1,
            const GFXSurface* surface
    );

    static void PrepareTexturedPolyPT(int32 y, int inkEffect, const GFXSurface* surface);
    static void PrepareTexturedPolyPTEX(int32 y, int inkEffect, const GFXSurface* surface);
    static void DrawTexturedPolyPT(
            int32 x, int32 y,
            int32 ox, int32 oy,
            int32 width, int32 height,
            int32 sprX0, int32 sprX1,
            int32 sprY0, int32 sprY1,
            int32 rotation,
            int32 alpha,
            const GFXSurface *surface
    );
    static void DrawTexturedPolyPTEx(
        const Vector2& upperLeft, const Vector2& upperRight,
        const Vector2& lowerLeft, const Vector2& lowerRight,
        int32 sprX0, int32 sprX1,
        int32 sprY0, int32 sprY1,
        const GFXSurface* surface
    );
    static void DrawFloorTexturedPolyPTEx(
        const Vector4f& upperLeft, const Vector4f& upperRight,
        const Vector4f& lowerLeft, const Vector4f& lowerRight,
        float sprX0, float sprX1,
        float sprY0, float sprY1,
        const GFXSurface* surface, uint32 color
    );
    static void PrepareTexturedPolyTR(int32 y, int inkEffect, const GFXSurface* surface);
    static void DrawTexturedPolyTR(
            int32 x, int32 y,
            int32 ox, int32 oy,
            int32 width, int32 height,
            int32 sprX0, int32 sprX1,
            int32 sprY0, int32 sprY1,
            int32 rotation,
            int32 alpha,
            const GFXSurface *surface
    );

    static void DrawFloorTexturedTriStripPTEx(
        const Vector4f& upperLeft, const Vector4f& upperRight,
        const Vector4f& lowerLeft, const Vector4f& lowerRight, const Vector4f &fif,
        float sprX0, float sprX1, float sprX2, float sprX3, float sprx4,
        float sprY0, float sprY1, float sprY2, float sprY3, float sprY4,
        const GFXSurface* surface, uint32 color, int count
    );

    static void PrepareColoredPolyPT(int32 y, int inkEffect);
    static void DrawColoredPolyPT(
            int32 x, int32 y,
            int32 width, int32 height,
            uint32 color
    );

    static void PrepareColoredPolyTR(int32 y, int inkEffect);
    static void DrawColoredPolyTR(
            int32 x, int32 y,
            int32 width, int32 height,
            uint32 color
    );

    static void PrepareLinePolyPT(int inkEffect);
    static void DrawLinePolyPT(int x1, int y1, int x2, int y2, int color);

    static void SetLinePolyThickness(int thickness);
    static void PrepareLinePolyTR(int inkEffect);
    static void DrawLinePolyTR(int x1, int y1, int x2, int y2, int color);

    static void PrepareFacePolyPT(int inkEffect);
    static void DrawFacePolyPT(
            const Vector2 *vertices, const int32 vertCount,
            const int32 faceColor, const int32 alpha,
            const uint32 *colors
    );
    static void Draw3DFacePolyPT(
            const Vector4f *vertices, const int32 vertCount,
            const int32 faceColor, const int32 alpha,
            const uint32 *colors
    );

    static void PrepareFacePolyTR(int inkEffect);
    static void DrawFacePolyTR(
            const Vector2 *vertices, const int32 vertCount,
            const int32 faceColor, const int32 alpha,
            const uint32 *colors
    );
    static void Draw3DFacePolyTR(
            const Vector4f *vertices, const int32 vertCount,
            const int32 faceColor, const int32 alpha,
            const uint32 *colors
    );

    static void Prepare3DStripPT(int inkEffect);
    static void Draw3DStripPT(pvr_vertex_t *verts, int count);
    static void Prepare3DStripTR(int inkEffect);
    static void Draw3DStripTR(pvr_vertex_t *verts, int count);

    static void DrawTexturedQuadPTEx(
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
