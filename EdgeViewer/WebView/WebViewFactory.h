#pragma once

#include <windows.h>
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
//   - on success: populate gs_Views[hWnd] = outView.get() and run the
//     initial Navigate via Navigator
//   - on failure: log the HRESULT and DestroyWindow(hWnd)
//
// DllMain should therefore treat CreateWebView's return value as the
// synchronous HRESULT of "was the call queued?" — if FAILED, destroy
// the HWND and return null; if OK, return the HWND to TC and let the
// async callback finish the job.
HRESULT CreateWebView(void* parentWindow,
                       const std::wstring& fileToLoad,
                       const ProcessorInterface* processor);
//------------------------------------------------------------------------