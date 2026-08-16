// Windows-only: GDI+/shell-based directory thumbnail generation.
// Listed explicitly in EdgeViewer.vcxproj; excluded from the Linux
// CMake build. The #ifdef guard is defensive only. DirProcessor.cpp
// keeps the shared HTML generation.
#include "DirProcessor.h"

#ifdef _WIN32
#include <windows.h>
#include <shobjidl.h>   // For IShellItem
#include <gdiplus.h>   // For image manipulation
#include <wincrypt.h>  // For Base64 encoding (CryptBinaryToString)
#include <string>      // For std::wstring

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "propsys.lib")
#pragma comment(lib, "crypt32.lib") // Needed for Base64 encoding
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
std::wstring DirProcessor::genDirThumbnail(const std::filesystem::path& folderPath, int thumbSize) const
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
#endif // _WIN32
