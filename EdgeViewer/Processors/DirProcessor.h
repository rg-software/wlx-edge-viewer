#pragma once

#include "ProcessorInterface.h"
#include <string>
#include <regex>

// Directory:
// Preview thumbnails (generate HTML with thumbnails, then navigate to it)
// note this module does not support UNC paths (like \\localhost\c$\dir)
//------------------------------------------------------------------------
class DirProcessor : public ProcessorInterface
{
public:
	virtual bool InitPath(const fs::path& path);
	virtual void OpenIn(ViewPtr webView) const;

private:
	mutable ULONG_PTR mGdiplusToken;
	mutable	CLSID mPngClsid;
	fs::path mPath;
	
	fs::path stripTwodots(const fs::path& path) const;
	std::wregex extensionsToMaskRegex(const std::string& exts) const;
	std::wstring genBody(const fs::path& path) const;
	int initGenDirThumbnails() const;
	void shutdownGenDirThumbnails() const;
	std::wstring genDirThumbnail(const fs::path& folderPath, int thumbSize) const;
	static int getEncoderClsid(const WCHAR* format, CLSID* pClsid);
	static std::wstring toBase64(const BYTE* data, DWORD dataSize);
};
//------------------------------------------------------------------------
