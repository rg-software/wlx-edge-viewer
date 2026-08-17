#pragma once

#include <string>

#ifdef _WIN32
#include <windows.h>  // for HINSTANCE on Windows
#endif

class ProcessorInterface;

//------------------------------------------------------------------------
// Platform-neutral EdgeLister surface. The Win32 implementation lives
// in EdgeLister_Win.cpp; the Linux implementation lives in
// EdgeLister_Linux.cpp. WLX exports call into this surface.
//
// RegisterClass takes the HINSTANCE on Windows (Win32 class
// registration needs it); on Linux it's a no-op so we overload the
// signature under #ifdef _WIN32 to avoid dragging <windows.h> into
// the Linux build.
//
// On Linux the lister handle / parent is a QWidget* (Double Commander's
// Qt5/Qt6 builds), which DllMain.cpp passes as void* to keep this header
// platform-type-free.
class EdgeLister
{
public:
#ifdef _WIN32
	static void RegisterClass(HINSTANCE hinst);
#else
	static void RegisterClass();

	// Create: instantiate the backend, embed its WebView into the
	// parent widget, run the initial Navigator::Open. Returns the
	// plugin handle to pass back to DC (nullptr on failure).
	static void* Create(void* parentWindow, const std::wstring& fileToLoad, const ProcessorInterface* processor);

	// OpenIn: Navigator::Open for a stored backend (Decision 7 —
	// callbacks arrive on the main Qt thread, no WM_COPYDATA).
	static void OpenIn(void* listWin, const std::wstring& fileToLoad);
#endif
};
//------------------------------------------------------------------------
