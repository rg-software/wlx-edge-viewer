#pragma once

#include <filesystem>

#include "ProcessorInterface.h"
#include <string>
#include <regex>
#include <windows.h>
#include <gdiplus.h>

// Directory:
// Preview thumbnails (generate HTML with thumbnails, then navigate to it)
// note this module does not support UNC paths (like \\localhost\c$\dir)
//------------------------------------------------------------------------
class DirProcessor : public ProcessorInterface
{
public:
	virtual bool InitPath(const std::filesystem::path& path);
	virtual void OpenIn(IWebView& webView) const;

	std::filesystem::path stripTwodots(const std::filesystem::path& path) const;
	std::wregex extensionsToMaskRegex(const std::string& exts) const;

private:
	mutable ULONG_PTR mGdiplusToken;
	mutable	CLSID mPngClsid;
	std::filesystem::path mPath;
	
	std::wstring genBody(const std::filesystem::path& path) const;
	int initGenDirThumbnails() const;
	void shutdownGenDirThumbnails() const;
	std::wstring genDirThumbnail(const std::filesystem::path& folderPath, int thumbSize) const;
	static int getEncoderClsid(const WCHAR* format, CLSID* pClsid);
	static std::wstring toBase64(const BYTE* data, DWORD dataSize);
};
//------------------------------------------------------------------------
