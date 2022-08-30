#include <kos.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glut.h>

// static
bool RenderDevice::Init()
{
	pvr_init_defaults();
	glKosInit();
}

// static
void RenderDevice::CopyFrameBuffer()
{
	// DXFIXME: RenderDevice::CopyFrameBuffer
}
// static
void RenderDevice::ProcessDimming()
{
	// DXFIXME: RenderDevice::ProcessDimming
}
// static
void RenderDevice::FlipScreen()
{
	// DXFIXME: RenderDevice::FlipScreen
}
// static
void RenderDevice::Release(bool32 isRefresh)
{
	// DXFIXME: RenderDevice::Release
}

// static
void RenderDevice::RefreshWindow()
{
	// DXFIXME: RenderDevice::RefreshWindow
}
// static
void RenderDevice::GetWindowSize(int32 *width, int32 *height)
{
	// DXFIXME: RenderDevice::GetWindowSize
}

// static
void RenderDevice::SetupImageTexture(int32 width, int32 height, uint8 *imagePixels)
{
	// DXFIXME: RenderDevice::SetupImageTexture
}
// static
void RenderDevice::SetupVideoTexture_YUV420(int32 width, int32 height, uint8 *imagePixels)
{
	// DXFIXME: RenderDevice::SetupVideoTexture_YUV420
}
// static
void RenderDevice::SetupVideoTexture_YUV422(int32 width, int32 height, uint8 *imagePixels)
{
	// DXFIXME: RenderDevice::SetupVideoTexture_YUV422
}
// static
void RenderDevice::SetupVideoTexture_YUV424(int32 width, int32 height, uint8 *imagePixels)
{
	// DXFIXME: RenderDevice::SetupVideoTexture_YUV424
}

// static
bool RenderDevice::ProcessEvents()
{
	// DXFIXME: RenderDevice::ProcessEvents
	return true;
}

// static
void RenderDevice::InitFPSCap()
{
	// DXFIXME: RenderDevice::InitFPSCap
}
// static
bool RenderDevice::CheckFPSCap()
{
	// DXFIXME: RenderDevice::CheckFPSCap
	return true;
}
// static
void RenderDevice::UpdateFPSCap()
{
	// DXFIXME: RenderDevice::UpdateFPSCap
}

// Public since it's needed for the ModAPI
// static
void RenderDevice::LoadShader(const char *fileName, bool32 linear)
{
	// DXFIXME: RenderDevice::LoadShader
}

// static
void RenderDevice::SetWindowTitle()
{
	// DXFIXME: RenderDevice::SetWindowTitle
}
// static
bool RenderDevice::GetCursorPos(void*)
{
	// DXFIXME: RenderDevice::GetCursorPos
	return false;
}
// static
void RenderDevice::ShowCursor(bool)
{
	// DXFIXME: RenderDevice::ShowCursor
}