#include "ZoomHotkey.h"
#include <algorithm>
#include <vector>

//------------------------------------------------------------------------
bool ZoomHotkeyHandled(UINT key, bool ctrlHeld, double currentZoom, double& newZoom)
{
	if (!ctrlHeld)
		return false;

	if (!(key == VK_OEM_PLUS || key == VK_OEM_MINUS || key == '0' ||
		  key == VK_ADD || key == VK_SUBTRACT || key == VK_NUMPAD0))
		return false;

	static const std::vector zoomSteps = { 0.25, 0.33, 0.5, 0.67, 0.75, 0.8, 0.9, 1.0,
											1.1, 1.25, 1.5, 1.75, 2.0, 2.5, 3.0, 4.0, 5.0 };

	if (key == '0' || key == VK_NUMPAD0)
	{
		newZoom = 1.0;
		return true;
	}

	if (key == VK_OEM_PLUS || key == VK_ADD)
	{
		auto it = std::upper_bound(zoomSteps.begin(), zoomSteps.end(), currentZoom + 0.001);
		if (it != zoomSteps.end())
			newZoom = *it;
		else
			newZoom = currentZoom;	// ceiling
		return true;
	}

	// VK_OEM_MINUS or VK_SUBTRACT
	auto it = std::lower_bound(zoomSteps.begin(), zoomSteps.end(), currentZoom - 0.001);
	if (it != zoomSteps.begin())
		newZoom = *(--it);
	else
		newZoom = currentZoom;	// floor
	return true;
}
//------------------------------------------------------------------------