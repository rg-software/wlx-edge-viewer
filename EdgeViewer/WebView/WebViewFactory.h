#pragma once

#ifdef _WIN32
#include <windows.h>  // for HRESULT on Windows
#endif
#include <string>

class ProcessorInterface;

//------------------------------------------------------------------------
// Platform abstraction. On Windows returns a WebView2Backend; on Linux
// returns a WebKitBackend. The factory is the only source-tree place
// where the choice is made (Decision 4).
//
// The factory queues creation; it does NOT return a fully-populated
// IWebView*. WebView2's CreateCoreWebView2EnvironmentWithOptions is
// asynchronous: the synchronous return value only indicates whether
// the call was successfully *queued*. The actual environment + controller
// become available via the completion callbacks, which:
//
//   - on success: move the IWebView shared_ptr into gs_Views[hWnd]
//     (the map owns the lifetime), then run the initial Navigate
//     via Navigator
//   - on failure: DestroyWindow(hWnd)
//
// DllMain should therefore treat CreateWebView's return value as the
// synchronous HRESULT of "was the call queued?" — if FAILED, destroy
// the HWND and return null; if OK, return the HWND to TC and let the
// async callback finish the job.
//
// The parentWindow parameter is HWND on Windows, GtkWidget* on Linux.
// We use `void*` here so this header doesn't need to drag <windows.h>
// or <gtk/gtk.h> into the Linux build.
//
// Linux scheme note (Decision 3 Fallback A): WebKitGTK 2.38+ blocks
// registering 'http' as a custom scheme, so the Linux backend uses a
// custom scheme 'ev' (EdgeViewer) instead. NavigateToString rewrites
// 'http://' to 'ev://' in the loader HTML before passing it to
// WebKitGTK. The loaders' templates can stay using 'http://'
// references — the rewrite happens in C++.
HRESULT CreateWebView(void* parentWindow,
                       const std::wstring& fileToLoad,
                       const ProcessorInterface* processor);
//------------------------------------------------------------------------