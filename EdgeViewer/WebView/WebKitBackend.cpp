// Linux-only — compiled only when WebKitGTK + GTK headers are on the include path.
#include "WebKitBackend.h"
#include "../Globals.h"

#include <gtk/gtk.h>
#include <webkit2/webkit2.h>

#include <cstring>
#include <map>
#include <mutex>
#include <string>

//------------------------------------------------------------------------
// Per OpenSpec design decisions baked into this implementation:
//   Decision 3 Fallback A: register a custom scheme 'ev' (EdgeViewer).
//     Registering 'http' globally is blocked by WebKitGTK 2.38+
//     ("Registering special uri scheme http is no longer allowed").
//   Decision 9: NavigateToString uses load_html with a base_uri
//     so the loader's relative <link>/<script src> refs resolve via
//     the registered scheme handler.
//   Decision 6: setup helpers (SetColorProfile, AddAccleratorKeyHandler,
//     ParseAndPostMessage, Zoom hotkey handling) are Windows-only —
//     they stay in WebView2Backend.cpp. This Linux file only
//     implements the IWebView interface.

// Forward-declare the URI-scheme callback. WebKitGTK invokes it
// through C linkage.
extern "C" void uri_scheme_request_cb(WebKitURISchemeRequest* request,
                                     gpointer user_data);

//------------------------------------------------------------------------
struct WebKitBackend::Impl
{
	WebKitWebView* webView = nullptr;
	WebKitUserContentManager* contentManager = nullptr;
	std::string baseUri;             // passed to webkit_web_view_load_html
	std::mutex mu;                    // guards webView lifetime
};

//------------------------------------------------------------------------
// Process-wide host→folder map for the global URI-scheme handler.
// Linux is single-instance-per-process for DC plugins; one global map
// is fine. (Multi-instance would need per-WebKitBackend scheme
// registration — not in scope for this change.)
namespace { std::mutex g_schemeMutex; }
std::map<std::string, std::filesystem::path> g_schemeHosts;

//------------------------------------------------------------------------
WebKitBackend::WebKitBackend(const std::string& baseUriForLoadHtml)
	: m_impl(std::make_unique<Impl>())
{
	m_impl->baseUri = baseUriForLoadHtml;

	// Register the 'ev' URI scheme handler globally, once per process.
	// WebKitGTK 2.38+ blocks 'http' as a custom scheme (Decision 3
	// Fallback A spike-confirmed).
	static std::once_flag schemeOnce;
	std::call_once(schemeOnce, [] {
		WebKitWebContext* defaultCtx = webkit_web_context_get_default();
		WebKitSecurityManager* secMgr = webkit_web_context_get_security_manager(defaultCtx);
		webkit_security_manager_register_uri_scheme_as_secure(secMgr, "ev");
		webkit_web_context_register_uri_scheme(
			defaultCtx, "ev", uri_scheme_request_cb, nullptr, nullptr);
	});

	m_impl->contentManager = webkit_user_content_manager_new();
	m_impl->webView = WEBKIT_WEB_VIEW(g_object_new(WEBKIT_TYPE_WEB_VIEW,
		"user-content-manager", m_impl->contentManager,
		nullptr));
}

//------------------------------------------------------------------------
WebKitBackend::~WebKitBackend()
{
	std::scoped_lock lock(m_impl->mu);
	if (m_impl->webView)
		g_object_unref(m_impl->webView);
	if (m_impl->contentManager)
		g_object_unref(m_impl->contentManager);
}

//------------------------------------------------------------------------
void WebKitBackend::NavigateToString(const std::wstring& html)
{
	std::scoped_lock lock(m_impl->mu);
	if (!m_impl->webView)
		return;

	// Wide→UTF-8 for the GLib API.
	std::string utf8str = to_utf8(html);
	gchar* utf8 = g_strdup(utf8str.c_str());
	if (!utf8)
		return;

	// Decision 3 Fallback A: rewrite 'http://' to 'ev://' in the HTML
	// before passing it to WebKitGTK, so the loaders' existing
	// <link href='http://assets.example/...'> etc. get intercepted by
	// our 'ev' scheme handler (registered in the constructor). Without
	// this rewrite, WebKitGTK would try to fetch the URLs from the
	// network (since http:// is a real scheme) and fail.
	gchar* rewritten = nullptr;
	if (utf8)
	{
		const std::string from = "http://";
		const std::string to   = "ev://";
		const std::string src(utf8);
		std::string out;
		out.reserve(src.size() + 32);
		size_t pos = 0;
		for (;;)
		{
			size_t hit = src.find(from, pos);
			if (hit == std::string::npos) { out.append(src, pos, std::string::npos); break; }
			out.append(src, pos, hit - pos);
			out.append(to);
			pos = hit + from.size();
		}
		rewritten = g_strdup(out.c_str());
	}

	// Decision 9: pass baseUri so the loader's relative refs (e.g. <link
	// href="./css.css">) resolve via the registered scheme handler.
	webkit_web_view_load_html(
		WEBKIT_WEB_VIEW(m_impl->webView),
		rewritten ? rewritten : utf8,
		m_impl->baseUri.c_str());

	g_free(rewritten);
	g_free(utf8);
}

//------------------------------------------------------------------------
void WebKitBackend::Navigate(const std::wstring& uri)
{
	std::scoped_lock lock(m_impl->mu);
	if (!m_impl->webView)
		return;

	std::string utf8str = to_utf8(uri);
	gchar* utf8 = g_strdup(utf8str.c_str());
	if (!utf8)
		return;

	webkit_web_view_load_uri(WEBKIT_WEB_VIEW(m_impl->webView), utf8);
	g_free(utf8);
}

//------------------------------------------------------------------------
void WebKitBackend::ExecuteScript(const std::wstring& js)
{
	std::scoped_lock lock(m_impl->mu);
	if (!m_impl->webView)
		return;

	std::string utf8str = to_utf8(js);
	gchar* utf8 = g_strdup(utf8str.c_str());
	if (!utf8)
		return;

	webkit_web_view_run_javascript(WEBKIT_WEB_VIEW(m_impl->webView), utf8, nullptr, nullptr, nullptr);
	g_free(utf8);
}

//------------------------------------------------------------------------
void WebKitBackend::AddScriptToExecuteOnDocumentCreated(const std::wstring& js)
{
	std::scoped_lock lock(m_impl->mu);
	if (!m_impl->contentManager)
		return;

	std::string utf8str = to_utf8(js);
	gchar* utf8 = g_strdup(utf8str.c_str());
	if (!utf8)
		return;

	WebKitUserScript* script = webkit_user_script_new(
		utf8,
		WEBKIT_USER_CONTENT_INJECT_TOP_FRAME,
		WEBKIT_USER_SCRIPT_INJECT_AT_DOCUMENT_START,
		nullptr, nullptr);
	webkit_user_content_manager_add_script(m_impl->contentManager, script);
	webkit_user_script_unref(script);
	g_free(utf8);
}

//------------------------------------------------------------------------
void WebKitBackend::RegisterVirtualHost(const std::wstring& host,
                                       const std::filesystem::path& folder)
{
	// The global URI-scheme callback (uri_scheme_request_cb) is process-
	// wide and doesn't know which WebKitBackend instance it belongs to.
	// So we keep the host→folder mapping in a single process-wide map.
	// (For a single-instance DC plugin this is fine; multi-instance
	// would need a per-WebKitBackend scheme registration.)
	std::lock_guard<std::mutex> lock(g_schemeMutex);
	std::string h(host.begin(), host.end());
	g_schemeHosts[h] = folder;
}

//------------------------------------------------------------------------
void WebKitBackend::Close()
{
	std::scoped_lock lock(m_impl->mu);
	if (!m_impl->webView)
		return;

	WebKitWebView* wv = m_impl->webView;
	g_object_ref(wv);  // keep alive across the destroy
	gtk_widget_destroy(GTK_WIDGET(wv));
	g_object_unref(wv);
}

//------------------------------------------------------------------------
// Linux-only accessor for EdgeLister_Linux.cpp.
void* WebKitBackend::GetWidget() const
{
	return m_impl->webView ? GTK_WIDGET(m_impl->webView) : nullptr;
}

//------------------------------------------------------------------------
// Global URI-scheme callback. Dispatched on the main GTK thread by
// WebKitGTK. Looks up the host in the registered scheme→folder map
// and serves the file. This is the Linux equivalent of WebView2's
// SetVirtualHostNameToFolderMapping.
extern "C" void uri_scheme_request_cb(WebKitURISchemeRequest* request,
                                     gpointer /*user_data*/)
{
	// finish_error requires a GError argument; build a generic one.
	auto fail = [request]()
	{
		GError* err = g_error_new_literal(
			g_quark_from_static_string("edgeviewer"), 0, "ev:// resource not found");
		webkit_uri_scheme_request_finish_error(request, err);
		g_error_free(err);
	};

	const gchar* uri = webkit_uri_scheme_request_get_uri(request);
	const gchar* path = webkit_uri_scheme_request_get_path(request);

	if (!uri || !path)
	{
		fail();
		return;
	}

	// Parse host from "ev://host/path...".
	std::string s(uri);
	auto schemeEnd = s.find("://");
	if (schemeEnd == std::string::npos)
	{
		fail();
		return;
	}
	auto hostStart = schemeEnd + 3;
	auto hostEnd   = s.find('/', hostStart);
	std::string host = s.substr(hostStart,
	                            (hostEnd == std::string::npos) ? std::string::npos
	                                                          : hostEnd - hostStart);

	std::filesystem::path folder;
	{
		std::lock_guard<std::mutex> lock(g_schemeMutex);
		auto it = g_schemeHosts.find(host);
		if (it != g_schemeHosts.end())
			folder = it->second;
	}

	if (folder.empty())
	{
		fail();
		return;
	}

	// path begins with '/' — skip it.
	std::filesystem::path file = folder / (path + 1);

	GError* err = nullptr;
	gchar* contents = nullptr;
	gsize length = 0;
	if (!g_file_get_contents(file.string().c_str(), &contents, &length, &err))
	{
		if (err) g_error_free(err);
		fail();
		return;
	}

	// Detect MIME type by extension; fall back to text/html.
	const gchar* mime = "text/html";
	auto ext = file.extension();
	if      (ext == ".css")  mime = "text/css";
	else if (ext == ".js")   mime = "application/javascript";
	else if (ext == ".png")  mime = "image/png";
	else if (ext == ".svg")  mime = "image/svg+xml";
	else if (ext == ".json") mime = "application/json";

	GInputStream* stream = g_memory_input_stream_new_from_data(contents, length, g_free);

	webkit_uri_scheme_request_finish(request, stream, length, mime);

	g_object_unref(stream);
}

//------------------------------------------------------------------------
