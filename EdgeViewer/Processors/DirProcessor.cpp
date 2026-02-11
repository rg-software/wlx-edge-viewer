#include "DirProcessor.h"
#include <vector>
#include <algorithm>
#include <windows.h>
#include <shobjidl.h>   // For IShellItem
#include <gdiplus.h>   // For image manipulation
#include <wincrypt.h>  // For Base64 encoding (CryptBinaryToString)
#include <string>      // For std::wstring
#include <fstream>     // For std::wofstream to write the HTML file

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "propsys.lib")
#pragma comment(lib, "crypt32.lib") // Needed for Base64 encoding
//------------------------------------------------------------------------
namespace { DirProcessor dir; }
//------------------------------------------------------------------------
fs::path DirProcessor::stripTwodots(const fs::path& path) const
{
	std::wstring spath = path;
	return spath.ends_with(L"..\\") ? spath.substr(0, spath.length() - 3) : spath;
}
//------------------------------------------------------------------------
bool DirProcessor::InitPath(const fs::path& path)
{
	mPath = GetPhysicalPath(stripTwodots(path));
	return to_int(GlobalSettings()["Extensions"]["Dirs"]) && fs::is_directory(mPath);
}
//------------------------------------------------------------------------
void DirProcessor::OpenIn(ViewPtr webView) const
{ 
	const auto& dirIni = GlobalSettings().get("Directory");
	const auto cssFile = gs_IsDarkMode ? dirIni.get("CSSDark") : dirIni.get("CSS");

	mapDomains(webView, mPath.root_path());
	
	std::wstring wloader(to_utf16(ReadFile(assetsPath() / L"dirviewer" / L"loader.html")));
	wloader = replacePlaceholders(wloader, {
		{L"__BASE_URL__", urlPathW(mPath.relative_path())},
		{L"__BASE_PATH__", mPath},
		{L"__CSS_NAME__", to_utf16(cssFile)},
		{L"__FIT_TO_SCREEN__", to_utf16(dirIni.get("FitToScreen"))},
		{L"__SHOW_NAMES__", to_utf16(dirIni.get("ShowNames"))},
		{L"__TRUNCATE_NAMES__", to_utf16(dirIni.get("TruncateNames"))},
		{L"__NAMES_UNDER_THUMBS__", to_utf16(dirIni.get("NamesUnderThumbnails"))},
		{L"__SHOW_FOLDERS__", to_utf16(dirIni.get("ShowFolders"))},
		{L"__BODY__", genBody(mPath)}
	});

	webView->NavigateToString(wloader.c_str());
}
//------------------------------------------------------------------------
std::wregex DirProcessor::extensionsToMaskRegex(const std::string& exts) const
{
	auto mask = std::format(L".+\\.({})$", to_utf16(exts));
	return std::wregex(mask, std::regex_constants::icase);
}
//------------------------------------------------------------------------
std::wstring DirProcessor::genBody(const fs::path& path) const
{
	const auto& dirIni = GlobalSettings().get("Directory");
	const auto cssFile = gs_IsDarkMode ? dirIni.get("CSSDark") : dirIni.get("CSS");

	auto imageExtRe = extensionsToMaskRegex(dirIni.get("DirImageExt"));
	auto otherExtRe = extensionsToMaskRegex(dirIni.get("DirOtherExt"));

	auto folderThumb = L"http://assets.example/dirviewer/folder.png";
	auto fileThumb = L"http://assets.example/dirviewer/file.png";

	auto shouldGenDirThumbs = to_int(dirIni.get("GenDirThumbs"));
	auto dirThumbSize = to_int(dirIni.get("DirThumbSize"));
	
	std::wstringstream ss;

	std::vector<fs::directory_entry> entries;
	for (auto const& dir_entry : fs::directory_iterator(path))
		entries.push_back(dir_entry);

	// place directories before files, sort alphabetically
	std::sort(entries.begin(), entries.end(), 
		[](const fs::directory_entry& a, const fs::directory_entry& b)
		{
			if (a.is_directory() != b.is_directory())
				return a.is_directory();
			return a.path().filename().wstring() < b.path().filename().wstring();
		});
	
	initGenDirThumbnails();
	
	for (auto const& dir_entry : entries)
	{
		auto fname = dir_entry.path().filename().wstring();
		
		// skip . and ..
		if (fname == L"." || fname == L"..")
			;
		else if (dir_entry.is_directory())
		{
			std::wstring curThumb = shouldGenDirThumbs ? genDirThumbnail(dir_entry, dirThumbSize) : folderThumb;
			ss << std::format(L"<div class=\"directory-viewer\" data-name=\"{0}\">" 
							  L"<img class=\"thumbnail\" src=\"{1}\" alt=\"{0}\"></div>", fname, curThumb).c_str();
		}
		else if (dir_entry.is_regular_file() && std::regex_match(fname, imageExtRe))
		{
			ss << std::format(L"<a href=\"{0}\" class=\"thumbnail-viewer\">"
							  L"<img class=\"thumbnail\" src=\"{0}\" alt=\"{0}\" data-name=\"{0}\"></a>", fname).c_str();
		}
		else if (dir_entry.is_regular_file() && std::regex_match(fname, otherExtRe))
		{
			ss << std::format(L"<div class=\"file-viewer\" data-name=\"{0}\">"
				L"<img class=\"thumbnail\" src=\"{1}\" alt=\"{0}\"></div>", fname, fileThumb).c_str();
		}
	}
	
	shutdownGenDirThumbnails();
	
	return ss.str();
}
//------------------------------------------------------------------------
// get the CLSID of a GDI+ image encoder
int DirProcessor::getEncoderClsid(const WCHAR* format, CLSID* pClsid)
{
	UINT num = 0, size = 0;
	Gdiplus::GetImageEncodersSize(&num, &size);
	if (size == 0)
		return -1;
	Gdiplus::ImageCodecInfo* pImageCodecInfo = (Gdiplus::ImageCodecInfo*)(malloc(size));
	if (pImageCodecInfo == NULL)
		return -1;
	Gdiplus::GetImageEncoders(num, size, pImageCodecInfo);
	for (UINT j = 0; j < num; ++j)
	{
		if (wcscmp(pImageCodecInfo[j].MimeType, format) == 0)
		{
			*pClsid = pImageCodecInfo[j].Clsid;
			free(pImageCodecInfo);
			return j;
		}
	}
	free(pImageCodecInfo);
	return -1;
}
//------------------------------------------------------------------------
// convert binary data to a Base64 wstring
std::wstring DirProcessor::toBase64(const BYTE* data, DWORD dataSize)
{
	DWORD strSize = 0;
	// Get the required size for the Base64 string
	if (!CryptBinaryToStringW(data, dataSize, CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, NULL, &strSize))
		return L"";

	std::wstring base64String(strSize, L'\0');
	// Perform the actual encoding
	if (!CryptBinaryToStringW(data, dataSize, CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, &base64String[0], &strSize))
		return L"";

	// The returned size includes the null terminator, so we might need to resize.
	base64String.resize(wcslen(base64String.c_str())); 
	return base64String;
}
//------------------------------------------------------------------------
int DirProcessor::initGenDirThumbnails() const
{
	HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
	if (FAILED(hr))
		return -1;

	Gdiplus::GdiplusStartupInput gdiplusStartupInput;
	Gdiplus::GdiplusStartup(&mGdiplusToken, &gdiplusStartupInput, NULL);
	
	if (getEncoderClsid(L"image/png", &mPngClsid) == -1)
		return -1;
	
	return S_OK;
}
//------------------------------------------------------------------------
void DirProcessor::shutdownGenDirThumbnails() const
{
	Gdiplus::GdiplusShutdown(mGdiplusToken);
	CoUninitialize();
}
//------------------------------------------------------------------------
std::wstring DirProcessor::genDirThumbnail(const fs::path& folderPath, int thumbSize) const
{
    IShellItem* pShellItem = NULL;
    HBITMAP hBitmap = NULL;
	std::wstring result;

    HRESULT hr = SHCreateItemFromParsingName(folderPath.c_str(), NULL, IID_PPV_ARGS(&pShellItem));
    if (SUCCEEDED(hr)) 
    {
        IShellItemImageFactory* pImageFactory = NULL;
        hr = pShellItem->QueryInterface(IID_PPV_ARGS(&pImageFactory));
        if (SUCCEEDED(hr))
        {
            SIZE size = { thumbSize, thumbSize };
            hr = pImageFactory->GetImage(size, SIIGBF_THUMBNAILONLY | SIIGBF_BIGGERSIZEOK, &hBitmap);

            if (SUCCEEDED(hr))
            {
                IStream* pStream = NULL;
                
                if (SUCCEEDED(CreateStreamOnHGlobal(NULL, TRUE, &pStream)))
                {
                    Gdiplus::Bitmap bitmap(hBitmap, NULL);
                    
                    // Save the bitmap to the in-memory stream as PNG
                    if (bitmap.Save(pStream, &mPngClsid, NULL) == Gdiplus::Ok)
                    {
                        HGLOBAL hGlobal = NULL;
                        if (SUCCEEDED(GetHGlobalFromStream(pStream, &hGlobal)))
                        {
                            // Lock the memory to get a pointer to the data
                            LPVOID pData = GlobalLock(hGlobal);
                            if (pData)
                            {
                                std::wstring base64Image = toBase64(static_cast<BYTE*>(pData), (DWORD)GlobalSize(hGlobal));
                                result = L"data:image/png;base64," + base64Image;
                                GlobalUnlock(hGlobal);
                            }
                        }
                    }
                	pStream->Release();
                }
            }
            pImageFactory->Release();
        }
        pShellItem->Release();
    }
	
	if (hBitmap)
		DeleteObject(hBitmap);
	
    return result;
}
//------------------------------------------------------------------------
