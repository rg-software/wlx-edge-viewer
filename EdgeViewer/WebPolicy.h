#pragma once

#include <string>

//------------------------------------------------------------------------
// Shared URI-classification policy for [WebView] OfflineMode. Both
// backends (WebView2 on Windows, Qt Web Engine on Linux) call IsLocalUri
// to decide whether a request may proceed while offline mode is active,
// so the two platforms block identically by construction. Malformed URIs
// fail closed (classified non-local).
bool IsLocalUri(const std::wstring& uri);
bool IsLocalUri(const std::string& uri);
//------------------------------------------------------------------------
