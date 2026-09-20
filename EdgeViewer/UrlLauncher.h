#pragma once

#include <string>

//------------------------------------------------------------------------
// Platform-agnostic "open a URL in the user's default web browser"
// surface for the link context-menu actions (VirusTotal URL scan).
// The platform-specific backing lives in UrlLauncher_Win.cpp / 
// UrlLauncher_Linux.cpp, mirroring how Platform.{h,_Win,_Linux}.cpp
// split the filesystem helpers.
//
// Only web links (http/https) are ever handed to an external service;
// the plugin's virtual-host references (local.example, assets.example,
// lister.example, ev://, evh://) and file: refs are local and never
// scannable. The local-vs-web classification is shared with WebPolicy.
bool IsScannableWebLink(const std::wstring& link);

// Deep link that opens the VirusTotal URL-analysis page for `link`
// (e.g. https://www.virustotal.com/gui/url/<encoded>/detection).
std::wstring BuildVirusTotalUrl(const std::wstring& link);

// Open `url` in the OS default web browser. Returns false on failure.
bool OpenInDefaultBrowser(const std::wstring& url);
//------------------------------------------------------------------------