#pragma once

#include <deque>
#include <memory>
#include <string>

#include "../IWebView.h"

class QWidget;

namespace EdgeViewer {

//------------------------------------------------------------------------
// One process-wide pool of pre-built QtWebEngineBackends. The Linux
// build constructs one spare view during DllMain DLL_PROCESS_ATTACH
// and keeps N_active + 1 spares resident thereafter, so that every
// ListLoadW finds a warm view ready and never has to wait for Chromium
// to acquire its compositor surface during the user's Ctrl+Q hot path
// (the surface-promotion root cause documented in
// openspec/changes/mitigate-wayland-ctrlq-jump and Readme.md).
//
// Threading: every method must be called on the Qt main (GUI) thread.
// All WLX callbacks arrive there (Decision 7). The pool does no
// internal locking.
//
// Linux-only: the CMakeLists builds and links this file only when
// Qt WebEngine headers are on the include path (i.e. for the Linux
// .wlx64). The Windows build does not reference it.
//------------------------------------------------------------------------
class BrowserPool
{
public:
	struct AcquireResult
	{
		QWidget* container = nullptr;   // The plugin handle to return to DC.
		QWidget* view = nullptr;         // The QWebEngineView (in the container).
		std::shared_ptr<IWebView> backend; // typed at the interface for
		                                  // convenient DllMain::DoListLoad
		                                  // insertion into gs_Views.
	};

	// Idempotent. Builds the first spare synchronously. Called from
	// DllMain DLL_PROCESS_ATTACH on Linux.
	static void Initialize();

	// Idempotent. Closes every cached view (terminating Chromium
	// subprocesses), destroys the stash widget, leaves the pool empty
	// so a subsequent Initialize() rebuilds it. Called from DllMain
	// DLL_PROCESS_DETACH on Linux.
	static void Shutdown();

	// Hands the caller (a ListLoadW call) a `(Container, View,
	// Backend)` triplet. Reparents the view from the stash widget to
	// the new container (whose parent is `parentWindow`), shows it,
	// fires Navigator::Open(fileToLoad). Schedules an async spare
	// build so the next Acquire finds a fresh spare ready. Blocks
	// briefly (synchronous spare build) only if no spare is currently
	// available.
	static AcquireResult Acquire(QWidget* parentWindow,
	                             const std::wstring& fileToLoad);

	// Returns the triplet's view to the stash: reparents the view back
	// to the stash widget, hides it, clears the page via
	// `setUrl(about:blank)`. Does NOT call `Close` (Close is reserved
	// for Shutdown). Called from ListCloseWindow.
	static void Release(QWidget* container);

private:
	BrowserPool(const BrowserPool&) = delete;
	BrowserPool& operator=(const BrowserPool&) = delete;

public:
	BrowserPool() = default;
	~BrowserPool() = default;

	// Stored in the spare deque. Holds IWebView (not QtWebEngineBackend)
	// so the header does not force QtWebEngineBackend to be a complete
	// type here — BrowserPool.cpp casts at the boundary.
	struct SpareEntry
	{
		std::shared_ptr<IWebView> backend;
		QWidget* view = nullptr;
	};

	// Synchronous spare build. Constructs a QtWebEngineBackend + its
	// QWebEngineView, parents it to the stash widget, and pushes it
	// onto `m_spares`. Returns the pushed entry, or null on failure.
	SpareEntry* buildSpare();

	// Schedules an async spare build via QTimer::singleShot(0, ...).
	// No-op if a build is already scheduled or running, or if the pool
	// is shutting down.
	void scheduleBuildSpare();

	// True when at least one spare is currently being built (in
	// flight) or scheduled (waiting on the event loop). Prevents
	// recursive scheduling.
	bool m_spareBuildScheduled = false;

	QWidget* m_stashWidget = nullptr;             // hidden top-level container.
	std::deque<SpareEntry> m_spares;             // ready spares.
};

} // namespace EdgeViewer
//------------------------------------------------------------------------
