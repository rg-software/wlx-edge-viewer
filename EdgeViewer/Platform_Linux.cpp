// Linux-only — compiled only when building with POSIX/glibc headers.
// Windows builds (EdgeViewer.vcxproj) do not include this TU.
#include "Platform.h"
#include "Globals.h"

#include <dlfcn.h>
#include <unistd.h>
#include <sys/stat.h>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <vector>

//------------------------------------------------------------------------
// GetModulePath: dladdr() on the address of a local function gives
// the path of the .so the function lives in. The plugin's plugin
// directory is the directory containing the .so.
std::wstring GetModulePath()
{
	Dl_info info;
	if (dladdr(reinterpret_cast<void*>(&GetModulePath), &info) && info.dli_fname)
	{
		std::filesystem::path p(info.dli_fname);
		return p.parent_path().wstring() + L"/";
	}
	// Fallback: $CWD/plugin.
	std::filesystem::path p = std::filesystem::current_path();
	return (p / L"plugin").wstring() + L"/";
}

//------------------------------------------------------------------------
// ExpandEnv: substitute $VAR and ${VAR} using std::getenv. Matches
// the Win32 ExpandEnvironmentStringsW semantics closely enough for the
// loader's purposes.
std::wstring ExpandEnv(const std::wstring& path)
{
	std::string src(path.begin(), path.end());
	std::string out;
	out.reserve(src.size());

	for (size_t i = 0; i < src.size(); ++i)
	{
		if (src[i] != '$') { out += src[i]; continue; }

		size_t nameStart = i + 1;
		size_t nameEnd = nameStart;
		bool braced = (nameEnd < src.size() && src[nameEnd] == '{');
		if (braced) nameEnd++;
		while (nameEnd < src.size() &&
		       ((braced && src[nameEnd] != '}') ||
		        (!braced && (std::isalnum(static_cast<unsigned char>(src[nameEnd])) || src[nameEnd] == '_'))))
			++nameEnd;

		if (braced && nameEnd >= src.size())
		{
			out += src.substr(i);
			break;
		}
		if (braced) ++nameEnd; // skip closing '}'

		std::string name = src.substr(nameStart, nameEnd - nameStart - (braced ? 1 : 0));
		const char* val = std::getenv(name.c_str());
		if (val) out += val;
		i = nameEnd - 1;
	}

	return std::wstring(out.begin(), out.end());
}

//------------------------------------------------------------------------
// GetPhysicalPathForLink: read /proc/self/fd (Linux equivalent of
// Windows GetFinalPathNameByHandle). For symlinks, follow them once.
std::wstring GetPhysicalPathForLink(const fs::path& path)
{
	std::error_code ec;
	auto real = std::filesystem::read_symlink(path, ec);
	if (!ec)
		return real.wstring();
	return path.wstring(); // not a symlink or unreadable — return as-is
}

//------------------------------------------------------------------------
// GetPhysicalPath: resolve symlinks, ensure absolute. Linux has no
// equivalent of `\\?\` long-path prefix or `\\?\UNC\` so the logic
// is simpler than the Win32 version.
std::wstring GetPhysicalPath(const fs::path& path)
{
	fs::path p = path;
	if (!p.is_absolute())
		p = std::filesystem::absolute(p);

	std::error_code ec;
	auto resolved = std::filesystem::weakly_canonical(p, ec);
	if (!ec)
		return resolved.wstring();

	return p.wstring();
}

//------------------------------------------------------------------------
// GenTempFile: copy `path` to a temp file with the requested extension.
// Linux equivalent of the Win32 GetTempPathW + GetTempFileNameW path.
// Uses std::filesystem::temp_directory_path() (XDG_RUNTIME_DIR or
// TMPDIR, per task 4.1) and mkstemp-style unique suffix.
std::wstring GenTempFile(const fs::path& path, const std::wstring& ext)
{
	auto tmpDir = std::filesystem::temp_directory_path();

	// mkstemp wants the template writable; use a unique_path instead.
	std::error_code ec;
	auto unique = tmpDir / (std::filesystem::path)L"edXXXXXX.tmp";
	for (int i = 0; i < 10; ++i)
	{
		auto candidate = unique;
		auto s = candidate.string();
		int fd = mkstemp(const_cast<char*>(s.c_str()));
		if (fd < 0) continue;
		close(fd);
		candidate = s;
		if (ext.empty())
			candidate.replace_extension(); // .tmp -> ""
		else
			candidate.replace_extension(ext);
		std::filesystem::copy_file(path, candidate, ec);
		if (ec) continue;
		gs_tempFiles.push_back(candidate.wstring());
		return candidate.wstring();
	}
	return L""; // failure
}
//------------------------------------------------------------------------