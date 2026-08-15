#pragma once

#include <windows.h>

// Pure zoom hotkey handler: given the key, whether Ctrl is held, and the
// current zoom factor, returns whether the key is a recognized zoom hotkey
// and, if so, the new zoom factor via the out-parameter.
// Does NOT touch the WebView2 controller — caller reads/writes the zoom.
bool ZoomHotkeyHandled(UINT key, bool ctrlHeld, double currentZoom, double& newZoom);