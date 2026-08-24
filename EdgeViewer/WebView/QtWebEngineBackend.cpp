// Linux-only — compiled only when Qt Web Engine headers are on the include path.
#include "QtWebEngineBackend.h"
#include "../Globals.h"

#include <QBuffer>
#include <QByteArray>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QKeyEvent>
#include <QMultiMap>
#include <QTemporaryFile>
#include <QUrl>
#include <QWidget>

#include <atomic>
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

extern "C" void ListCloseWindow(void*);

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
// Per-instance lister container registry used by the ESC JS bridge.
// The bridge listens for `keydown` Escape in the page and issues
// `ev://_close/<id>` (via `new Image().src = url`, since Chromium's
// fetch() rejects custom schemes even when registered). EvSchemeHandler
// looks up the lister container by id and calls ListCloseWindow on it
// directly — the container then closes the backend and deletes itself.
// The container's QObject::destroyed signal drives unregistration, so
// the registry never holds a dangling pointer even if multiple ESC
// presses race.
namespace {
    std::atomic<uint64_t> g_nextContainerId{1};
    std::mutex g_containersMutex;
    std::map<uint64_t, QWidget*> g_containers;
}

uint64_t AllocateContainerId()
{
    return g_nextContainerId.fetch_add(1);
}

void RegisterContainer(uint64_t id, QWidget* container)
{
    std::lock_guard<std::mutex> lock(g_containersMutex);
    g_containers[id] = container;
}

void UnregisterContainer(uint64_t id)
{
    std::lock_guard<std::mutex> lock(g_containersMutex);
    g_containers.erase(id);
}

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

		// ESC propagation bridge: `ev://_close/<id>` (set by the JS
		// injected in QtWebEngineBackend's constructor) routes here,
		// looks up the lister container, and posts a synthetic Q
		// keypress to the container's parent widget (DC's viewer
		// panel).  DC's hotkey handler processes Q identically to a
		// physical press, invoking cm_ExitViewer → lister close.
		// The plugin does not destroy the container itself; that
		// would race DC's close logic and trigger its
		// parent-widget destruction hooks (in some Qt6 widgetsets
		// this exits the application).
		if (url.host() == QLatin1String("_close"))
		{
			const std::string idStr = url.path().toStdString();
			std::string stripped = (!idStr.empty() && idStr[0] == '/')
				? idStr.substr(1) : idStr;

			QWidget* container = nullptr;
			try
			{
				const uint64_t id = std::stoull(stripped);
				std::lock_guard<std::mutex> lock(g_containersMutex);
				auto it = g_containers.find(id);
				if (it != g_containers.end())
					container = it->second;
			}
			catch (...) {}

			if (container)
			{
				// Chromium intercepts ESC below Qt's event system,
				// so ESC never reaches DC's form key handler.  The
				// `Q` key is NOT intercepted by Chromium and reaches
				// DC's cm_ExitViewer through normal Qt dispatch.
				// Post a synthetic Q keypress to the container's
				// parent (DC's viewer panel) so it follows the same
				// path as a physical Q press.  Deferred so the
				// scheme handler can return first.
				QWidget* parent = container->parentWidget();
				if (parent)
				{
					QCoreApplication::postEvent(parent,
						new QKeyEvent(QEvent::KeyPress, Qt::Key_Q,
							Qt::NoModifier));
					QCoreApplication::postEvent(parent,
						new QKeyEvent(QEvent::KeyRelease, Qt::Key_Q,
							Qt::NoModifier));
				}
			}
			auto* buf = new QBuffer(job);
			buf->setData(QByteArray());
			job->reply(QByteArrayLiteral("text/plain"), buf);
			return;
		}

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
	uint64_t containerId = 0; // ESC close-bridge id; 0 disables the bridge
};

//------------------------------------------------------------------------
QtWebEngineBackend::QtWebEngineBackend(const std::string& baseUriForLoadHtml, uint64_t containerId)
	: m_impl(std::make_unique<Impl>())
{
	m_impl->baseUri = baseUriForLoadHtml;
	m_impl->containerId = containerId;

	// Register the 'ev' URI scheme + handler once per process, before
	// the first QWebEngineView is created. registerScheme must not run
	// twice, and it must run before Qt creates its first page — this
	// backend owns that first page (DC core itself does not use
	// QtWebEngine), so the lazy once-only registration is safe.
	static std::once_flag schemeOnce;
	std::call_once(schemeOnce, [] {
		QWebEngineUrlScheme scheme("ev");
		scheme.setSyntax(QWebEngineUrlScheme::Syntax::HostAndPort);
		scheme.setFlags(QWebEngineUrlScheme::SecureScheme
		                | QWebEngineUrlScheme::LocalAccessAllowed
		                | QWebEngineUrlScheme::CorsEnabled);
		QWebEngineUrlScheme::registerScheme(scheme);

		QWebEngineProfile::defaultProfile()->installUrlSchemeHandler(
			QByteArray("ev"), new EvSchemeHandler());
	});

	m_impl->view = new QWebEngineView();

	// ESC close bridge: inject a keydown listener that triggers a
	// request to `ev://_close/<id>` on Escape. The global scheme
	// handler looks up the lister container in its registry and
	// calls ListCloseWindow on it (which closes the backend and
	// hides the container — see DllMain.cpp).
	//
	// Why `new Image().src = url` and not `fetch(url)`: Chromium's
	// JS fetch API has a server-side allowlist
	// (`URL.supportedSchemes`) that doesn't pick up schemes
	// registered via QWebEngineUrlScheme::registerScheme, even with
	// CorsEnabled. `<img>` requests do not have that restriction.
	if (containerId != 0)
	{
		const auto js = std::format(LR"(
			window.addEventListener('keydown', (e) => {{
			  if (e.key === 'Escape' && !e.defaultPrevented) {{
			    new Image().src = 'ev://_close/{}';
			  }}
			}});)", containerId);
		AddScriptToExecuteOnDocumentCreated(js);
	}

	// Mirror Windows's WebViewFactory::AddApplyStyleScript. The HTML
	// processor loads files via Navigate("http://local.example/...") and
	// expects the plugin to inject its [HTML] CSS into the rendered page.
	// Without this script, HTML files render with only the file's own
	// styling. See openspec/changes/fix-linux-html-css-injection/.
	{
		const auto& htmlIni = GlobalSettings().get("HTML");
		const auto cssFile = gs_IsDarkMode ? htmlIni.get("CSSDark") : htmlIni.get("CSS");
		if (!cssFile.empty())
		{
			const auto cssUrl = L"ev://assets.example/html/" + to_utf16(cssFile);
			const auto js = std::format(LR"(
				window.addEventListener('DOMContentLoaded', () => {{
				  if (window.location.href.toLowerCase().startsWith('ev://local.example')) {{
				    const link = document.createElement('link');
				    link.rel = 'stylesheet';
				    link.href = '{}';
				    (document.head || document.documentElement).appendChild(link);
				  }}
				}});)", cssUrl);
			AddScriptToExecuteOnDocumentCreated(js);
		}
	}
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
void QtWebEngineBackend::Print()
{
	if (!m_impl->view)
		return;

	// Chromium suppresses window.print() when called from runJavaScript
	// (no user-gesture context).  Use QWebEnginePage::printToPdf instead
	// to produce a PDF, then open it in the system's default viewer.
	QTemporaryFile tmp(QDir::tempPath() + "/edgeviewer-print-XXXXXX.pdf");
	tmp.setAutoRemove(false);
	if (!tmp.open())
		return;
	const QString path = tmp.fileName();
	tmp.close();

	m_impl->view->page()->printToPdf(path);
	// printToPdf is async; open the file once the page signals completion.
	QObject::connect(m_impl->view->page(), &QWebEnginePage::pdfPrintingFinished,
		[path](const QString& filePath, bool ok) {
			if (ok)
				QDesktopServices::openUrl(QUrl::fromLocalFile(filePath));
		});
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
