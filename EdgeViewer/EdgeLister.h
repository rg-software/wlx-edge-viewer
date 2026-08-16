#pragma once

#ifdef _WIN32
#include <windows.h>  // for HINSTANCE on Windows
#endif

//------------------------------------------------------------------------
// Platform-neutral EdgeLister surface. The Win32 implementation lives
// in EdgeLister_Win.cpp; the Linux implementation lives in
// EdgeLister_Linux.cpp. WLX exports call into this surface.
//
// RegisterClass takes the HINSTANCE on Windows (Win32 class
// registration needs it); on Linux it's a no-op so we overload the
// signature under #ifdef _WIN32 to avoid dragging <windows.h> into
// the Linux build.
class EdgeLister
{
public:
#ifdef _WIN32
	static void RegisterClass(HINSTANCE hinst);
#else
	static void RegisterClass();
#endif
};
//------------------------------------------------------------------------