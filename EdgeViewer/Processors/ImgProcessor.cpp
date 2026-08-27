#include "ImgProcessor.h"
#include "../Globals.h"
//------------------------------------------------------------------------
namespace { ImgProcessor img; }
//------------------------------------------------------------------------
std::vector<ProcessorInterface::WStrPair> ImgProcessor::extraPlaceholders() const
{
	// FitToScreen drives the loader's initial screen class and boolean
	// (F toggles between full-screen and real-size at runtime).
	const auto& imgIni = GlobalSettings().get("Images");
	const auto isFullscreen = to_int(imgIni.get("FitToScreen"));
	return {
		{L"__SCREEN_CLASS__", to_utf16(isFullscreen ? "full-screen" : "real-size")},
		{L"__IS_FULSCREEN__", std::to_wstring(isFullscreen)},
	};
}
//------------------------------------------------------------------------