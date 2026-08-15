#pragma once

#include <windows.h>

//------------------------------------------------------------------------
// Platform-neutral EdgeLister surface. The Win32 implementation lives
// in EdgeLister_Win.cpp; the Linux implementation lives in
// EdgeLister_Linux.cpp. WLX exports call into this surface.
class EdgeLister
{
public:
	static void RegisterClass(HINSTANCE hinst);
};
//------------------------------------------------------------------------
