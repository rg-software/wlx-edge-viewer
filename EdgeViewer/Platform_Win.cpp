#include "Platform.h"
#include "Globals.h"

#include <windows.h>
#include <shlwapi.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <wil/com.h>
#include <string>
#include <format>
#include <regex>

//------------------------------------------------------------------------
// Windows implementation of the platform helpers declared in Platform.h.
// This file is the only TU that pulls in <windows.h> + <shlwapi> for
// the filesystem/environment helpers; everything else talks to the
// platform-agnostic Platform.h surface.
std::wstring GetModulePath()	// keep backslash at the end
{
	wchar_t iniFilePath[MAX_PATH];
	GetModuleFileName(gs_PluginInstance, iniFilePath, MAX_PATH);
	wcsrchr(iniFilePath, L'\\')[1] = L'\0';

	return iniFilePath;
}
//------------------------------------------------------------------------
// resolve links to make sure we have the actual target location
// return the original path in case of errors
std::wstring GetPhysicalPathForLink(const fs::path& path)
{
	HANDLE hFile = CreateFileW(path.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
		                       nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);

	if (hFile == INVALID_HANDLE_VALUE)
		return path.wstring();

	wchar_t buffer[MAX_PATH];
	DWORD result = GetFinalPathNameByHandleW(hFile, buffer, MAX_PATH, FILE_NAME_NORMALIZED);

    if (result == 0 || result >= MAX_PATH)
        result = GetFinalPathNameByHandleW(hFile, buffer, MAX_PATH, FILE_NAME_OPENED);

	CloseHandle(hFile);

	if (result == 0 || result >= MAX_PATH)
		return path.wstring();

    return std::wstring(buffer);
}
//------------------------------------------------------------------------
std::wstring GenTempFile(const fs::path& path, const std::wstring& ext)
{
    wchar_t tempPath[MAX_PATH], tempFile[MAX_PATH];
    GetTempPathW(MAX_PATH, tempPath);
    GetTempFileNameW(tempPath, L"UNC", 0, tempFile);
    wcscat_s(tempFile, ext.c_str());
    gs_tempFiles.push_back(tempFile);

    return CopyFileW(path.c_str(), tempFile, FALSE) ? std::wstring(tempFile) : L"";
}
//------------------------------------------------------------------------
// If UNC path or forced HTML file is returned, copy to temp location and return local path
std::wstring GetPhysicalPath(const fs::path& path)
{
	std::wstring realPath = GetPhysicalPathForLink(path);

	if (!fs::path(realPath).is_absolute())
		realPath = fs::absolute(realPath).wstring();

	const std::wstring uncPrefix = L"\\\\?\\UNC\\";
	const std::wstring extendedPrefix = L"\\\\?\\";

    if (!fs::is_directory(realPath))
    {
        if (realPath.starts_with(uncPrefix)) // for UNC files return a path to a temp copy
            return GenTempFile(path, fs::path(realPath).extension());
    }

	// Strip "\\?\"
	if (realPath.starts_with(uncPrefix))
		return L"\\\\" + realPath.substr(uncPrefix.length());

    return realPath.starts_with(extendedPrefix) ? realPath.substr(extendedPrefix.length()) : realPath;
}
//------------------------------------------------------------------------
std::wstring ExpandEnv(const std::wstring& path)
{
    wchar_t pathFinal[MAX_PATH];
    ExpandEnvironmentStrings(path.c_str(), pathFinal, MAX_PATH); // so we can use any %ENV_VAR%
    return pathFinal;
}
//------------------------------------------------------------------------
// Native folder picker for the attachment-save flow. `parentWindow` is
// the owner HWND (the lister window) so the dialog is modal to TC, or
// nullptr for a toplevel dialog. `defaultFolder` is the initially
// selected directory (empty = no default, the OS/session location).
// Returns the chosen folder with a trailing backslash, or empty string
// if the user cancels.
std::wstring PickFolder(const void* parentWindow, const std::wstring& defaultFolder)
{
	// The old SHBrowseForFolder + BFFM_SETSELECTIONW approach could not
	// reliably preselect the requested folder (it often opened on the
	// OS/user default location), so use the modern IFileOpenDialog which
	// honors SetFolder() for the initial view.
	HRESULT coInit = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
	// Only uninitialize if THIS call actually initialized COM; S_FALSE
	// means it was already initialized elsewhere on this thread.
	const bool ownsCoInit = (coInit == S_OK);

	wil::com_ptr<IFileDialog> dialog;
	HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
	                              IID_PPV_ARGS(&dialog));
	if (SUCCEEDED(hr))
	{
		// Folder picker that only accepts real filesystem folders.
		hr = dialog->SetOptions(FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
		if (SUCCEEDED(hr))
		{
			HWND owner = parentWindow
				? static_cast<HWND>(const_cast<void*>(parentWindow)) : nullptr;

			if (!defaultFolder.empty())
			{
				// Resolve to an absolute path (the picker's folder is the
				// directory of the opened EML, so it always exists) and
				// open the dialog pointed at it. SetFolder is the reliable
				// initial-location API on the modern dialog.
				std::wstring expanded = ExpandEnv(defaultFolder);
				WCHAR full[MAX_PATH];
				if (GetFullPathNameW(expanded.c_str(), MAX_PATH, full, nullptr) > 0)
				{
					wil::com_ptr<IShellItem> item;
					if (SUCCEEDED(SHCreateItemFromParsingName(full, nullptr, IID_PPV_ARGS(&item))))
						dialog->SetFolder(item.get());
				}
			}

			// Show(owner) makes the dialog modal to the lister (or a
			// toplevel dialog when there is no parent).
			if (dialog->Show(owner) == S_OK)
			{
				wil::com_ptr<IShellItem> result;
				if (SUCCEEDED(dialog->GetResult(&result)))
				{
					wil::unique_cotaskmem_string path;
					if (SUCCEEDED(result->GetDisplayName(SIGDN_FILESYSPATH, &path)))
					{
						std::wstring folder(path.get());
						if (!folder.empty() && folder.back() != L'\\')
							folder += L'\\';
						if (ownsCoInit) CoUninitialize();
						return folder;
					}
				}
			}
		}
	}

	if (ownsCoInit) CoUninitialize();
	return L"";
}
//------------------------------------------------------------------------
