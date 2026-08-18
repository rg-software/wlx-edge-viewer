// Linux-only — compiled only with Qt Web Engine headers on the include path.
#include "EdgeLister.h"
#include "Globals.h"
#include "IWebView.h"
#include "Navigator.h"
#include "WebView/QtWebEngineBackend.h"
#include "WebView/BrowserPool.h"

#include <QWidget>
#include <QVBoxLayout>

#include <memory>
#include <mutex>

//------------------------------------------------------------------------
// EdgeLister on Linux: the parent window that DC passes to ListLoadW is
// a QWidget* (Double Commander's Qt6 build). The plugin claims a
// (Container, View, Backend) triplet from the process-wide BrowserPool
// (see openspec/changes/pool-prebuilt-browsers/), which keeps one
// spare QtWebEngineView resident at all times so that the very first
// ListLoadW of a session never has to spin up a Chromium compositor
// against an unrealized parent — the root cause of the Ctrl+Q
// quick-view jump documented in Readme.md / AGENTS.md.
//
// On native Wayland, the spare view's compositor surface attaches to
// DC's main-window surface tree during plugin load, so subsequent
// Acquire calls just reparent the existing view into the lister
// container. The lister handle we return (the container) is the same
// shape as before the pool refactor; DllMain's Linux-only branch
// stores it in `gs_Views[container] = backend` so ListLoadNextW /
// ListCloseWindow work unchanged.
//
// On the offscreen Qt platform-plugin (used by our regression
// harness), the spare-view construction segfaults during app.exec();
// the original per-Create code path does not have this issue. The
// offscreen harness is therefore NOT a reliable proxy for this fix
// — the Ctrl+Q Wayland-jump behavior must be validated on the real
// KDE/Wayland DC desktop.
//------------------------------------------------------------------------

void EdgeLister::RegisterClass()
{
	// No Win32 class registration on Linux; the WLX exports are declared
	// via the CMake linker version script (design Decision 11). This is a
	// deliberate no-op.
}

//------------------------------------------------------------------------
// Guards `gs_Views` access. All WLX callbacks arrive on the main Qt
// thread (Decision 7), so this is belt-and-braces rather than a
// required synchronization primitive.
namespace { std::mutex g_viewsMutex; }

//------------------------------------------------------------------------
// Create: claim a (Container, View, Backend) triplet from the
// BrowserPool. The pool handles backend construction, spare reparent
// into the container, container show, and Navigator::Open.
void* EdgeLister::Create(void* parentWindow, const std::wstring& fileToLoad, const ProcessorInterface* processor)
{
	auto* parent = static_cast<QWidget*>(parentWindow);
	if (!parent)
		return nullptr;

	auto result = EdgeViewer::BrowserPool::Acquire(parent, fileToLoad);
	if (!result.container)
		return nullptr;

	{
		std::scoped_lock lock(g_viewsMutex);
		auto ibase = std::shared_ptr<IWebView>(
			std::move(result.backend),
			static_cast<IWebView*>(result.backend.get()));
		gs_Views[result.container] = ibase;
	}
	return result.container;
}

//------------------------------------------------------------------------
// Direct-Navigator paths (Decision 7): on Linux, ListLoadNext and
// friends arrive on the main Qt thread, so we skip WM_COPYDATA and
// call Navigator::Open directly on the stored backend.
void EdgeLister::OpenIn(void* listWin, const std::wstring& fileToLoad)
{
	std::scoped_lock lock(g_viewsMutex);
	auto it = gs_Views.find(listWin);
	if (it == gs_Views.end()) return;
	Navigator nav(*it->second);
	nav.Open(fileToLoad);
}
//------------------------------------------------------------------------
