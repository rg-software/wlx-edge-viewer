// Linux-only — compiled only when Qt Web Engine headers are on the include path.
#include "QtWebEngineBackend.h"
#include "../Globals.h"

#include <QBuffer>
#include <QByteArray>
#include <QFile>
#include <QMultiMap>
#include <QUrl>
#include <QWebEngineProfile>
#include <QWebEngineScript>
#include <QWebEngineScriptCollection>
#include <QWebEngineUrlRequestJob>
#include <QWebEngineUrlScheme>
#include <QWebEngineUrlSchemeHandler>
#include <QWebEngineView>

#include <filesystem>
#include <map>
#include <mutex>
#include <string>

//------------------------------------------------------------------------
// Per OpenSpec design decisions baked into this implementation:
//   Decision 3 Fallback A: register a custom scheme 'ev' (EdgeViewer).
//     Registering 'http' globally is blocked by WebKitGTK 2.38+; Qt
//     WebEngine likewise forbids registering 'http'. The 'ev' scheme
//     is registered once, before the first QWebEngineView is created.
//   Decision 9: NavigateToString uses setHtml with a base URI so the
//     loader's relative <link>/<script src> refs resolve via the
//     registered scheme handler.
//   Decision 6: setup helpers (SetColorProfile, AddAccleratorKeyHandler,
//     ParseAndPostMessage, Zoom hotkey handling) are Windows-only —
//     they stay in WebView2Backend.cpp. This Linux file only
//     implements the IWebView interface.
//
// Empirical notes (spike, /tmp/opencode/qtspike):
//   - QWebEngineUrlScheme::registerScheme("ev") works when called AFTER
//     QApplication exists; Qt only needs the scheme registered before
//     its first page is created, and this plugin owns the only page in
//     DC's process.
//   - The scheme must include CorsEnabled for fetch()/XHR to a custom
//     scheme to be permitted by Chromium. The loaders' fetch() calls
//     are fallbacks for builds without pre-fetch (always active here),
//     but CorsEnabled + Access-Control-Allow-Origin: * keeps that path
//     working and matches Decision 3 finding 1.
//   - Top-level setUrl() navigation through the scheme handler works,
//     so the HTML/Other processors' http://local.example Navigate calls
//     resolve after the http:// -> ev:// rewrite below.
//------------------------------------------------------------------------

// Process-wide host->folder map for the global URI-scheme handler.
// Linux is single-instance-per-process for DC plugins; one global map
// is fine. (Multi-instance would need per-backend scheme registration —
// not in scope for this change.)
namespace { std::mutex g_schemeMutex; }
std::map<std::string, std::filesystem::path> g_schemeHosts;

//------------------------------------------------------------------------
// Global URI-scheme handler. Serves files under the registered
// host->folder mapping — the Linux equivalent of WebView2's
// SetVirtualHostNameToFolderMapping. Dispatched on the Qt main thread
// by Qt Web Engine. No Q_OBJECT (no signals/slots/metadata needed), so
// no automoc is required.
class EvSchemeHandler : public QWebEngineUrlSchemeHandler
{
public:
	void requestStarted(QWebEngineUrlRequestJob* job) override
	{
		const QUrl url = job->requestUrl();

		std::filesystem::path folder;
		{
			std::lock_guard<std::mutex> lock(g_schemeMutex);
			auto it = g_schemeHosts.find(url.host().toStdString());
			if (it != g_schemeHosts.end())
				folder = it->second;
		}

		if (folder.empty())
		{
			job->fail(QWebEngineUrlRequestJob::UrlNotFound);
			return;
		}

		// url.path() is percent-decoded and starts with '/'; drop the
		// leading slash so the join is relative to the mapped folder
		// (fs::path "/" + "/x" would otherwise be the absolute "/x").
		// Matches the urlPathW() encoding the loaders substitute for
		// filenames with spaces (Decision 3, finding 2).
		std::string rel = url.path().toStdString();
		if (!rel.empty() && rel[0] == '/')
			rel.erase(0, 1);
		std::filesystem::path file = folder / rel;

		QFile f(QString::fromStdString(file.string()));
		if (!f.open(QIODevice::ReadOnly))
		{
			job->fail(QWebEngineUrlRequestJob::UrlNotFound);
			return;
		}
		const QByteArray data = f.readAll();

		// Detect MIME type by extension; fall back to text/html.
		QByteArray mime = "text/html";
		auto ext = file.extension();
		if      (ext == ".css")  mime = "text/css";
		else if (ext == ".js")   mime = "application/javascript";
		else if (ext == ".png")  mime = "image/png";
		else if (ext == ".svg")  mime = "image/svg+xml";
		else if (ext == ".json") mime = "application/json";

		// Decision 3 finding 1: loaders cross-origin fetch() between
		// ev://assets.example and ev://local.example; every response
		// carries Access-Control-Allow-Origin so those reads succeed.
		QMultiMap<QByteArray, QByteArray> headers;
		headers.insert("Access-Control-Allow-Origin", "*");
		job->setAdditionalResponseHeaders(headers);

		// Job takes ownership of the buffer (parented to the job).
		auto* buf = new QBuffer(job);
		buf->setData(data);
		job->reply(mime, buf);
	}
};

//------------------------------------------------------------------------
struct QtWebEngineBackend::Impl
{
	QWebEngineView* view = nullptr;
	std::string baseUri;   // passed to setHtml (Decision 9)
};

//------------------------------------------------------------------------
void QtWebEngineBackend::RegisterSchemeOnce()
{
	// Must run before any QWebEngineView is constructed anywhere in
	// the process (so Qt Web Engine's URL parser knows the scheme when
	// the first profile is created). Idempotent — multiple calls are
	// no-ops. Called eagerly from DllMain::DLL_PROCESS_ATTACH on Linux
	// so the registration has fully taken effect by the time the first
	// ListLoadW fires; the per-QtWebEngineBackend call below is a
	// safety net for any future caller that forgets to invoke this
	// from its own load path.
	static std::once_flag schemeOnce;
	std::call_once(schemeOnce, [] {
		QWebEngineUrlScheme scheme("ev");
		// URLs in the loaders are `ev://host/path` (no port). The
		// Path syntax accepts that form; HostAndPort would emit a
		// "Scheme ev needs a default port" warning and may reject
		// registration on stricter Qt builds.
		scheme.setSyntax(QWebEngineUrlScheme::Syntax::Path);
		scheme.setFlags(QWebEngineUrlScheme::SecureScheme
		                | QWebEngineUrlScheme::LocalAccessAllowed
		                | QWebEngineUrlScheme::CorsEnabled);
		QWebEngineUrlScheme::registerScheme(scheme);

		QWebEngineProfile::defaultProfile()->installUrlSchemeHandler(
			QByteArray("ev"), new EvSchemeHandler());
	});
}

//------------------------------------------------------------------------
QtWebEngineBackend::QtWebEngineBackend(const std::string& baseUriForLoadHtml)
	: m_impl(std::make_unique<Impl>())
{
	m_impl->baseUri = baseUriForLoadHtml;

	// Idempotent — DllMain::DLL_PROCESS_ATTACH already calls this on
	// Linux. Kept as a safety net for callers that don't.
	RegisterSchemeOnce();

	m_impl->view = new QWebEngineView();
}

//------------------------------------------------------------------------
QtWebEngineBackend::~QtWebEngineBackend()
{
	// The view is a QWidget child of the lister container; if Close()
	// already deleted it, this is a no-op.
	if (m_impl->view)
	{
		m_impl->view->stop();
		m_impl->view->deleteLater();
		m_impl->view = nullptr;
	}
}

//------------------------------------------------------------------------
void QtWebEngineBackend::NavigateToString(const std::wstring& html)
{
	if (!m_impl->view)
		return;

	std::string utf8str = to_utf8(html);

	// Decision 3 Fallback A: rewrite 'http://' to 'ev://' in the HTML
	// before passing it to Qt Web Engine, so the loaders' existing
	// <link href='http://assets.example/...'> etc. get intercepted by
	// our 'ev' scheme handler (registered in the constructor). Without
	// this rewrite, Chromium would try to fetch the URLs from the
	// network (since http:// is a real scheme) and fail.
	const std::string from = "http://";
	const std::string to   = "ev://";
	std::string out;
	out.reserve(utf8str.size() + 32);
	size_t pos = 0;
	for (;;)
	{
		size_t hit = utf8str.find(from, pos);
		if (hit == std::string::npos) { out.append(utf8str, pos, std::string::npos); break; }
		out.append(utf8str, pos, hit - pos);
		out.append(to);
		pos = hit + from.size();
	}

	// Decision 9: pass the base URI so the loader's relative refs (e.g.
	// <link href="./css.css">) resolve via the registered scheme handler.
	m_impl->view->setHtml(QString::fromUtf8(out.c_str()),
	                      QUrl(QString::fromStdString(m_impl->baseUri)));
}

//------------------------------------------------------------------------
void QtWebEngineBackend::Navigate(const std::wstring& uri)
{
	if (!m_impl->view)
		return;

	std::string utf8str = to_utf8(uri);

	// The HTML/Other processors navigate to http://local.example/<rel>.
	// Rewrite those (and http://assets.example) to ev:// so they resolve
	// through the scheme handler; real external URLs (UrlProcessor)
	// pass through untouched. "http://" is 7 chars: replace(0,7,...)
	// drops the whole prefix, so the replacement must re-add the "//".
	if (utf8str.rfind("http://local.example", 0) == 0
	    || utf8str.rfind("http://assets.example", 0) == 0)
	{
		utf8str.replace(0, 7, "ev://");
	}

	m_impl->view->setUrl(QUrl(QString::fromUtf8(utf8str.c_str())));
}

//------------------------------------------------------------------------
void QtWebEngineBackend::ExecuteScript(const std::wstring& js)
{
	if (!m_impl->view)
		return;

	std::string utf8str = to_utf8(js);
	m_impl->view->page()->runJavaScript(QString::fromUtf8(utf8str.c_str()));
}

//------------------------------------------------------------------------
void QtWebEngineBackend::AddScriptToExecuteOnDocumentCreated(const std::wstring& js)
{
	if (!m_impl->view)
		return;

	std::string utf8str = to_utf8(js);

	QWebEngineScript script;
	script.setName("edgeviewer-document-created");
	script.setInjectionPoint(QWebEngineScript::DocumentCreation);
	script.setWorldId(QWebEngineScript::MainWorld);
	script.setSourceCode(QString::fromUtf8(utf8str.c_str()));
	m_impl->view->page()->scripts().insert(script);
}

//------------------------------------------------------------------------
void QtWebEngineBackend::RegisterVirtualHost(const std::wstring& host,
                                            const std::filesystem::path& folder)
{
	// The global URI-scheme handler (EvSchemeHandler) is process-wide
	// and doesn't know which backend instance it belongs to. So we keep
	// the host->folder mapping in a single process-wide map.
	// (For a single-instance DC plugin this is fine; multi-instance
	// would need a per-backend scheme registration.)
	std::lock_guard<std::mutex> lock(g_schemeMutex);
	std::string h(host.begin(), host.end());
	g_schemeHosts[h] = folder;
}

//------------------------------------------------------------------------
void QtWebEngineBackend::Close()
{
	if (!m_impl->view)
		return;

	// The view is a child widget of the lister container; detach it and
	// let Qt reclaim it on the next event loop pass.
	m_impl->view->stop();
	m_impl->view->setParent(nullptr);
	m_impl->view->deleteLater();
	m_impl->view = nullptr;
}

//------------------------------------------------------------------------
// Linux-only accessor for EdgeLister_Linux.cpp.
void* QtWebEngineBackend::GetWidget() const
{
	return m_impl->view ? static_cast<QWidget*>(m_impl->view) : nullptr;
}

//------------------------------------------------------------------------
