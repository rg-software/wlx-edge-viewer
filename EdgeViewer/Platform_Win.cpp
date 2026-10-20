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
#include <vector>

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
// Network shares come back from GetPhysicalPathForLink as \\?\UNC\... and,
// after prefix stripping, as plain \\server\share\... — both start with a
// double backslash. The extended-length local form \\?\C:\... is stripped
// first so it is never misclassified as a network path (production callers
// pass post-normalization paths, but keep this robust anyway).
bool IsNetworkPath(const fs::path& path)
{
	std::wstring s = path.wstring();
	if (s.starts_with(L"\\\\?\\UNC\\"))
		return true;
	if (s.starts_with(L"\\\\?\\"))
		s = s.substr(4);                            // \\?\C:\... -> C:\...
	return s.starts_with(L"\\\\");
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
// Resolve symlinks/junctions, then normalize the result for rendering.
std::wstring GetPhysicalPath(const fs::path& path)
{
	std::wstring realPath = GetPhysicalPathForLink(path);

	if (!fs::path(realPath).is_absolute())
		realPath = fs::absolute(realPath).wstring();

	const std::wstring uncPrefix = L"\\\\?\\UNC\\";
	const std::wstring extendedPrefix = L"\\\\?\\";

	// Stripped prefixes reveal the real location: \\server\share\... for
	// UNC files, plain drive paths for local ones. UNC files are NOT copied
	// to temp anymore: they are served in place through the host-side evh://
	// scheme (see IsNetworkPath + the processors), which reads the bytes in
	// the plugin's own process (the renderer has no share credentials and
	// the local.example virtual host cannot map a UNC folder).
	if (realPath.starts_with(uncPrefix))
		return L"\\\\" + realPath.substr(uncPrefix.length());

    return realPath.starts_with(extendedPrefix) ? realPath.substr(extendedPrefix.length()) : realPath;
}
//------------------------------------------------------------------------
namespace
{
// Case-insensitive replace of every occurrence of `from` with `to`.
void replaceAllCaseInsensitive(std::wstring& s, const std::wstring& from, const std::wstring& to)
{
    if (from.empty())
        return;
    size_t pos = 0;
    while (true)
    {
        size_t found = std::wstring::npos;
        for (size_t i = pos; i + from.size() <= s.size(); ++i)
        {
            bool match = true;
            for (size_t j = 0; j < from.size(); ++j)
            {
                if (towlower(s[i + j]) != towlower(from[j]))
                {
                    match = false;
                    break;
                }
            }
            if (match)
            {
                found = i;
                break;
            }
        }
        if (found == std::wstring::npos)
            break;
        s.replace(found, from.size(), to);
        pos = found + to.size();
    }
}
}
//------------------------------------------------------------------------
std::wstring ExpandEnv(const std::wstring& path)
{
    const std::wstring pluginDir = GetModulePath();

    // Resolve the configured UserDir (with its own %PluginDir% + real env
    // expansion, but NOT %UserDir%, so self-reference can't recurse).
    std::wstring userDirRaw = to_utf16(GlobalSettings()["WebView"]["UserDir"]);
    if (userDirRaw.empty())
        userDirRaw = pluginDir;
    std::wstring userDirExpanded = userDirRaw;
    // Resolve %PluginDir% inside UserDir first (UserDir may itself be
    // %PluginDir%\\...). Bounded for the same self-reference safety as
    // above.
    for (int pass = 0; pass < 4; ++pass)
    {
        std::wstring prev = userDirExpanded;
        replaceAllCaseInsensitive(userDirExpanded, L"%PluginDir%", pluginDir);
        if (userDirExpanded == prev)
            break;
    }
    {
        DWORD needed = ExpandEnvironmentStrings(userDirExpanded.c_str(), nullptr, 0);
        if (needed > 0)
        {
            std::vector<wchar_t> buf(needed);
            if (ExpandEnvironmentStrings(userDirExpanded.c_str(), buf.data(), needed) > 0)
                userDirExpanded = buf.data();
        }
    }
    if (userDirExpanded.empty())
        userDirExpanded = pluginDir;

    // Expand pseudo-vars in the requested value. Up to 4 passes so that
    // chained references (A -> B -> C -> D -> %PluginDir%) fully resolve;
    // the loop stops early as soon as a pass makes no change, which is
    // what keeps a self-referential value (UserDir=%UserDir%) from looping
    // forever instead of hanging.
    std::wstring out = path;
    for (int pass = 0; pass < 4; ++pass)
    {
        std::wstring prev = out;
        replaceAllCaseInsensitive(out, L"%PluginDir%", pluginDir);
        replaceAllCaseInsensitive(out, L"%UserDir%", userDirExpanded);
        if (out == prev)
            break;
    }
    // Real environment variables (%ENV%).
    DWORD needed = ExpandEnvironmentStrings(out.c_str(), nullptr, 0);
    if (needed > 0)
    {
        std::vector<wchar_t> buf(needed);
        if (ExpandEnvironmentStrings(out.c_str(), buf.data(), needed) > 0)
            out = buf.data();
    }
    return out;
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
