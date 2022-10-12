#pragma once

#include "Globals.h"

class EdgeLister
{
public:
	static void RegisterClass(HINSTANCE hinst);

private:
	static LRESULT CALLBACK pluginWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
};
