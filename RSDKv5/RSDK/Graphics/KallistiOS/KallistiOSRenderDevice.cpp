#include <kos.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glut.h>

#include <RSDK/Core/Stub.hpp>

// static
bool RenderDevice::Init()
{
    // DCFIXME: glKosInit is allocating too much memory!
    //glKosInit();
    return true;
}

// static
void RenderDevice::CopyFrameBuffer()
{
    DC_STUB();
}
// static
void RenderDevice::ProcessDimming()
{
    DC_STUB();
}
// static
void RenderDevice::FlipScreen()
{
    DC_STUB();
    // DCFIXME: glKosInit is allocating too much memory!
    //glutSwapBuffers();
    //glClearColor(0.0, 1.0, 0.0, 1.0);
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
    // commented because this becomes very noisy...
    //DC_STUB();
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