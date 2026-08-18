#pragma once

// Tiny standalone declaration of the Eager URI-scheme registration
// step for QtWebEngineBackend. Implemented in QtWebEngineBackend.cpp
// but declared here so DllMain.cpp (and other translation units) can
// call it without pulling in QtWebEngineBackend's PIMPL'd type (which
// requires the complete Impl type for unique_ptr's default deleter).
//
// Linux-only — the Windows build does not link this.

namespace EdgeViewer {

class QtWebEngineBackend;

void QtWebEngineBackend_RegisterSchemeOnce_Linux();
// Alias to QtWebEngineBackend::RegisterSchemeOnce without requiring
// the full QtWebEngineBackend type at the call site.
static inline void RegisterSchemeOnce()
{
	QtWebEngineBackend_RegisterSchemeOnce_Linux();
}

} // namespace EdgeViewer
