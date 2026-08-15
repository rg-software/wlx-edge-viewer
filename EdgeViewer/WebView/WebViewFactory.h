#pragma once

#include <memory>
#include <string>

#include "IWebView.h"

class ProcessorInterface;

//------------------------------------------------------------------------
// Platform abstraction. On Windows returns a WebView2Backend; on Linux
// returns a WebKitBackend. The factory is the only source-tree place
// where the choice is made (Decision 4).
//
// The factory owns the view lifetime: it returns a std::unique_ptr
// that, on success, has been registered into the global view map and
// is ready to take Open/Search/Print calls. The factory sets up
// everything that the backend needs to be standalone (event handlers,
// accelerators, color profile, etc.) before returning.
std::unique_ptr<IWebView> CreateWebView(void* parentWindow,
                                         const std::wstring& fileToLoad,
                                         const ProcessorInterface* processor);
//------------------------------------------------------------------------
