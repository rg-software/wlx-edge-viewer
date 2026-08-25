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
#include <QMenu>
#include <QMultiMap>
#include <QTemporaryFile>
#include <QTextCodec>
#include <QUrl>
#include <QWidget>

#include <atomic>
#include <QWebEngineProfile>
#include <QWebEngineScript>
#include <QWebEngineScriptCollection>
#include <QWebEngineUrlRequestInfo>
#include <QWebEngineUrlRequestInterceptor>
#include <QWebEngineUrlRequestJob>
#include <QWebEngineUrlScheme>
#include <QWebEngineUrlSchemeHandler>
#include <QWebEngineView>

#include "../EncodingList.h"
#include "../CharsetOverride.h"
#include "../WebPolicy.h"

#include <filesystem>
#include <map>
#include <mutex>
#include <stdexcept>
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
// Per-instance lister container registry used by the ESC JS bridge and
// the image-viewer zoom bridge.  The bridge listens for `keydown`
// Escape in the page and issues `ev://_close/<id>` (via
// `new Image().src = url`, since Chromium's fetch() rejects custom
// schemes even when registered).  EvSchemeHandler looks up the lister
// container by id and either posts a synthetic Q keypress (ESC) or
// adjusts the zoom factor (CMD_ZOOM).  The container's
// QObject::destroyed signal drives unregistration, so the registry
// never holds a dangling pointer even if multiple ESC presses race.
namespace {
    struct ContainerInfo {
        QWidget* container = nullptr;
        QWebEnginePage* page = nullptr;
    };
    std::atomic<uint64_t> g_nextContainerId{1};
    std::mutex g_containersMutex;
    std::map<uint64_t, ContainerInfo> g_containers;
}

uint64_t AllocateContainerId()
{
    return g_nextContainerId.fetch_add(1);
}

void RegisterContainer(uint64_t id, QWidget* container, QWebEnginePage* page)
{
	std::lock_guard<std::mutex> lock(g_containersMutex);
	g_containers[id] = {container, page};
}

void UnregisterContainer(uint64_t id)
{
    std::lock_guard<std::mutex> lock(g_containersMutex);
    g_containers.erase(id);
}

//------------------------------------------------------------------------
// Save an EML attachment on Linux. `args` is the tail of the CMD_SAVE
// message after the command token: "<sanitized-filename>|<url-safe-base64>".
// Runs the native folder picker, writes the decoded bytes, and reports
// the result back to the loader's `window.__emlSaveResult` callback.
// The 1 MB raw-byte guard keeps the payload safely under the transport's
// URL length ceiling; oversized attachments are declined explicitly,
// never silently truncated.
void HandleLinuxSave(const ContainerInfo& info, const std::string& args)
{
	auto reply = [&](const std::wstring& status, const std::wstring& message)
	{
		if (!info.page)
			return;
		std::wstring script = BuildSaveResultScript(status, message);
		info.page->runJavaScript(QString::fromUtf8(to_utf8(script).c_str()));
	};

	const auto bar = args.find('|');
	if (bar == std::string::npos)
	{
		reply(L"error", L"Malformed save request.");
		return;
	}
	std::wstring filename = to_utf16(args.substr(0, bar));
	std::string b64 = args.substr(bar + 1);

	// 1 MB raw-byte guard. base64 inflates ~4/3, so cap the encoded
	// payload at ceil(1MB * 4/3) + headroom for padding.
	const size_t rawLimit = 1024 * 1024;
	const size_t encLimit = rawLimit * 4 / 3 + 4;
	if (b64.size() > encLimit)
	{
		reply(L"error", L"Attachment is too large to save with this build.");
		return;
	}

	std::vector<uint8_t> bytes = DecodeBase64UrlSafe(b64);
	if (bytes.empty())
	{
		reply(L"error", L"Attachment payload is empty or corrupt.");
		return;
	}

	std::wstring folder = PickFolder(info.container);
	if (folder.empty())
	{
		reply(L"cancel", L"");
		return;
	}

	std::wstring target = folder + L"/" + SanitizeAttachmentName(filename);
	if (SaveAttachmentToFolder(folder, filename, bytes))
		reply(L"ok", L"Saved to " + target);
	else
		reply(L"error", L"Could not write the attachment to the chosen folder.");
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
					container = it->second.container;
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

		// Image-viewer zoom bridge: `ev://_cmd/<id>/<message>` routes
		// CMD_ZOOM messages from the imgview loader's postMessage shim
		// to QWebEnginePage::setZoomFactor.  The loader's JS calls
		// window.chrome.webview.postMessage("CMD_ZOOM|<scale>"), which
		// the injected shim rewrites to an Image.src navigation to this
		// handler.  The handler parses the scale factor and applies it.
		if (url.host() == QLatin1String("_cmd"))
		{
			const std::string pathStr = url.path().toStdString();
			// path is /<id>/<message>; find the second slash
			const auto secondSlash = pathStr.find('/', 1);
			if (secondSlash != std::string::npos)
			{
				const std::string idPart = pathStr.substr(1, secondSlash - 1);
				const std::string message = pathStr.substr(secondSlash + 1);
				try
				{
					const uint64_t id = std::stoull(idPart);
					// Copy the container info out while holding the lock,
					// then release it before handling CMD_SAVE: the save
					// flow shows a modal folder dialog, which runs a
					// nested event loop. Holding the mutex across that
					// loop would deadlock a second ev://_cmd arriving on
					// the same thread (std::mutex is not recursive).
					ContainerInfo info;
					bool found = false;
					{
						std::lock_guard<std::mutex> lock(g_containersMutex);
						auto it = g_containers.find(id);
						if (it != g_containers.end())
						{
							info = it->second;
							found = true;
						}
					}
					if (!found || !info.page)
						throw std::runtime_error("no container");

					// Parse "CMD_ZOOM|<scale>" or "CMD_SAVE|<name>|<url-safe-base64>"
					const auto bar = message.find('|');
					if (bar != std::string::npos)
					{
						const std::string cmd = message.substr(0, bar);
						if (cmd == "CMD_ZOOM")
						{
							const std::string arg = message.substr(bar + 1);
							const double scale = std::stod(arg);
							info.page->setZoomFactor(scale);
						}
						else if (cmd == "CMD_SAVE")
						{
							HandleLinuxSave(info, message.substr(bar + 1));
						}
					}
				}
				catch (...) {}
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
		else if (ext == ".pdf")  mime = "application/pdf";
		else if (ext == ".zip")  mime = "application/zip";
		else if (ext == ".docx") mime = "application/vnd.openxmlformats-officedocument.wordprocessingml.document";
		else if (ext == ".xlsx") mime = "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet";
		else if (ext == ".odt")  mime = "application/vnd.oasis.opendocument.text";
		else if (ext == ".epub") mime = "application/epub+zip";

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
	bool encodingOverrideSupported = false; // set per-OpenIn (issue #66)
	bool encodingOverrideHtml = false;      // HTML (host-side) vs loader (page-side)
	std::string rawFileBytesB64; // pre-fetched file bytes for encoding override
	std::vector<uint8_t> rawFileBytes; // pristine source bytes for host-side splice
	std::string baseHref;   // <base href> the HTML processor spliced
	std::wstring activeEncodingTag; // "" = auto-detect (default); checked in menu
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
		                | QWebEngineUrlScheme::CorsEnabled
		                | QWebEngineUrlScheme::FetchApiAllowed);
		QWebEngineUrlScheme::registerScheme(scheme);

		QWebEngineProfile::defaultProfile()->installUrlSchemeHandler(
			QByteArray("ev"), new EvSchemeHandler());

		// [WebView] OfflineMode: block every request that does not resolve
		// to plugin-local content before any network access (the Linux
		// counterpart of WebViewFactory's WebResourceRequested handler;
		// the same IsLocalUri policy decides on both platforms). The ev://
		// scheme — including the _close/_cmd JS->host bridges — is allowed
		// by the policy. Profile-level so every page of the shared default
		// profile is covered; installed once because the ini parse is
		// cached for the process lifetime anyway. Parented to the profile:
		// unlike installUrlSchemeHandler, setUrlRequestInterceptor does
		// not take ownership.
		if (to_int(GlobalSettings()["WebView"]["OfflineMode"]))
		{
			class EvOfflineInterceptor : public QWebEngineUrlRequestInterceptor
			{
			public:
				explicit EvOfflineInterceptor(QObject* parent = nullptr)
					: QWebEngineUrlRequestInterceptor(parent) {}
				void interceptRequest(QWebEngineUrlRequestInfo& info) override
				{
					if (!IsLocalUri(info.requestUrl().toString().toStdString()))
						info.block(true);
				}
			};
			QWebEngineProfile::defaultProfile()->setUrlRequestInterceptor(
				new EvOfflineInterceptor(QWebEngineProfile::defaultProfile()));
		}
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
		AddNamedScript(js, "edgeviewer-esc-bridge");

		// WebView2 postMessage shim: the imgview loader calls
		// window.chrome.webview.postMessage("CMD_ZOOM|<scale>") to
		// report zoom changes back to the host.  On Qt Web Engine
		// chrome.webview doesn't exist, so we define a minimal shim
		// that routes messages through ev://_cmd/<id>/<msg> (same
		// Image.src trick as the ESC bridge).  The scheme handler
		// parses CMD_ZOOM and calls page->setZoomFactor().
		const auto shim = std::format(LR"(
			if (!window.chrome) window.chrome = {{}};
			if (!window.chrome.webview) {{
			  window.chrome.webview = {{
			    postMessage: function(msg) {{
			      new Image().src = 'ev://_cmd/{}/' + encodeURIComponent(msg);
			    }}
			  }};
			}}
		)", containerId);
		AddNamedScript(shim, "edgeviewer-webview-shim");
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
				    if (!document.getElementById('ev-html-style-link')) {{
				    const link = document.createElement('link');
				    link.id = 'ev-html-style-link';
				    link.rel = 'stylesheet';
				    link.href = '{}';
				    (document.head || document.documentElement).appendChild(link);}}
				  }}
				}});)", cssUrl);
			AddNamedScript(js, "edgeviewer-css-apply");
		}
	}

	// HTML encoding override: the page-side executor is intentionally NOT
	// injected. Every in-page re-decode strategy tried here (document.write,
	// head/body innerHTML swap, body-only swap) reliably blanked the render
	// and killed the host context menu on Qt Web Engine. Re-decode is
	// performed host-side instead — charset meta-splice into the pristine
	// raw bytes + fresh setHtml render (html-charset-override change).
	// Picks from the Encoding submenu below route through
	// ApplyCharsetOverride (host-side splice for HTML, loader JS for MHT).

	// Mirror Windows's WebViewFactory::AddNativeEncodingMenu: extend Qt
	// Web Engine's BUILT-IN context menu with an "Encoding" submenu
	// (createStandardContextMenu + extra actions) instead of replacing it
	// with a DOM overlay. The processor reports whether its view can
	// re-decode its source bytes (HTML/MHT via supportsEncodingOverride())
	// through SetEncodingOverrideSupported during OpenIn, so we gate on
	// that flag rather than the page URL: all loader-based processors
	// (Markdown, RST, AsciiDoc, MHT, EML) share the same
	// ev://assets.example/loader.html URL, so URL matching cannot tell
	// MHT apart from the rest. The native "Encoding" submenu is added
	// only when supported; every other view keeps the stock menu
	// untouched (and the stock menu is always shown, unlike the old
	// code which silently suppressed it on non-encoding views).
	m_impl->view->setContextMenuPolicy(Qt::CustomContextMenu);
	QObject::connect(m_impl->view, &QWidget::customContextMenuRequested, m_impl->view,
		[this](const QPoint& pos)
		{
			QMenu* menu = m_impl->view->createStandardContextMenu();

			if (m_impl->encodingOverrideSupported)
			{
				menu->addSeparator();
				QMenu* encodingMenu = menu->addMenu(QStringLiteral("Encoding"));
				for (const auto& entry : EncodingList::kItems)
				{
					QAction* action = encodingMenu->addAction(QString::fromWCharArray(entry.display));
					action->setCheckable(true);
					action->setChecked(std::wstring(entry.tag) == m_impl->activeEncodingTag);
					action->setData(QString::fromWCharArray(entry.tag));
				}
				QObject::connect(encodingMenu, &QMenu::triggered, encodingMenu,
					[this](QAction* action)
					{
						// Single dispatch point: the backend routes the pick
						// to the host-side splice (HTML) or the loader's
						// page-side executor (MHT) via ApplyCharsetOverride.
						const QString tag = action->data().toString();
						ApplyCharsetOverride(tag.isEmpty()
							? std::wstring()
							: to_utf16(tag.toStdString()));
					});
			}

			menu->exec(m_impl->view->mapToGlobal(pos));
			menu->deleteLater();
		});
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
void QtWebEngineBackend::NavigateToString(const std::wstring& html,
                                           const std::string& baseUri)
{
	if (!m_impl->view)
		return;

	// Every new document load resets the encoding-override capability;
	// processors that support re-decode (HTML, MHT) re-assert it via
	// SetEncodingOverrideSupported(true) during OpenIn. This prevents a
	// stale true from a previous MHT/HTML view leaking onto an
	// image/directory/PDF view that reuses the same backend.
	m_impl->encodingOverrideSupported = false;
	// Same reset for the HTML-vs-loader re-decode mode; processors
	// re-assert via SetEncodingOverrideHtml during OpenIn.
	m_impl->encodingOverrideHtml = false;
	// Every fresh document starts back in "auto-detect" (engine sniffing);
	// the Encoding radio menu must not show a stale checked entry.
	m_impl->activeEncodingTag.clear();

	// Do NOT clear the raw-bytes script here: HtmlProcessor calls
	// SetRawFileBytes BEFORE NavigateToString, and SetRawFileBytes owns
	// the script lifecycle (removes the previous one, inserts the new
	// one). Clearing here would remove the just-injected bytes.

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
	// An explicit override lets HtmlProcessor point the base at
	// ev://local.example/ so the CSS-injection and encoding-override
	// scripts (which gate on that host) fire correctly.
	const std::string& base = baseUri.empty() ? m_impl->baseUri : baseUri;
	// Retain the <base href> the HTML processor spliced, for a later
	// host-side charset override re-render to reuse.
	if (!baseUri.empty())
		m_impl->baseHref = baseUri;
	m_impl->view->setHtml(QString::fromUtf8(out.c_str()),
	                      QUrl(QString::fromStdString(base)));
}

//------------------------------------------------------------------------
void QtWebEngineBackend::Navigate(const std::wstring& uri)
{
	if (!m_impl->view)
		return;

	m_impl->encodingOverrideSupported = false;
	m_impl->activeEncodingTag.clear();

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
// Like AddScriptToExecuteOnDocumentCreated but with a caller-chosen name
// so multiple scripts survive (QWebEngineScriptCollection rejects
// duplicate names).
void QtWebEngineBackend::AddNamedScript(const std::wstring& js,
                                        const char* name)
{
	if (!m_impl->view)
		return;

	std::string utf8str = to_utf8(js);

	QWebEngineScript script;
	script.setName(QString::fromLatin1(name));
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
void QtWebEngineBackend::SetEncodingOverrideSupported(bool supported)
{
	m_impl->encodingOverrideSupported = supported;
}

//------------------------------------------------------------------------
void QtWebEngineBackend::SetEncodingOverrideHtml(bool isHtml)
{
	m_impl->encodingOverrideHtml = isHtml;
}

//------------------------------------------------------------------------
void QtWebEngineBackend::RemoveRawFileBytesScript()
{
	if (!m_impl->view)
		return;

	auto& scripts = m_impl->view->page()->scripts();
	auto found = scripts.find("edgeviewer-raw-file-bytes");
	if (!found.isEmpty())
		scripts.remove(found.first());
	m_impl->rawFileBytesB64.clear();
}

//------------------------------------------------------------------------
void QtWebEngineBackend::SetRawFileBytes(const std::vector<uint8_t>& bytes)
{
	if (!m_impl->view)
		return;

	// Keep the pristine source bytes for the host-side charset override
	// to re-splice (never a previously-spliced output).
	m_impl->rawFileBytes = bytes;

	// Replace any previously-inserted raw-bytes script so re-opening an
	// HTML file injects the fresh bytes rather than a stale previous
	// load's (QWebEngineScriptCollection rejects duplicate names).
	RemoveRawFileBytesScript();

	m_impl->rawFileBytesB64 =
		QByteArray(reinterpret_cast<const char*>(bytes.data()),
		           static_cast<int>(bytes.size())).toBase64().toStdString();

	// DocumentCreation (not DocumentReady): matches the working
	// encoding-bootstrap script and is registered before navigation, so
	// it reliably runs when the next document is created — DocumentReady
	// fired too late because the script was inserted after setHtml().
	// The script only sets a window property (needs no DOM), so
	// DocumentCreation is safe and deterministic.
	QWebEngineScript script;
	script.setName("edgeviewer-raw-file-bytes");
	script.setInjectionPoint(QWebEngineScript::DocumentCreation);
	script.setWorldId(QWebEngineScript::MainWorld);
	script.setSourceCode(QString::fromStdString(
		"window.__evRawFileBytesB64 = '" + m_impl->rawFileBytesB64 + "';"));
	m_impl->view->page()->scripts().insert(script);
}

//------------------------------------------------------------------------
void QtWebEngineBackend::ApplyCharsetOverride(const std::wstring& tag)
{
	if (!m_impl->view)
		return;

	// MHT (and any loader-based) views re-decode PAGE-SIDE through their
	// own window.__evEncodingApply; never splice/transcode host-side into
	// MIME bytes. HTML views take the host-side transcode below.
	if (!m_impl->encodingOverrideHtml)
	{
		const std::string js = tag.empty()
			? "window.__evEncodingApply && window.__evEncodingApply(null);"
			: "window.__evEncodingApply && window.__evEncodingApply('" + to_utf8(tag) + "');";
		m_impl->view->page()->runJavaScript(QString::fromStdString(js));
		m_impl->activeEncodingTag = tag; // checked entry in the menu
		return;
	}

	if (m_impl->rawFileBytes.empty())
		return;

	// HTML: decode the pristine bytes into Unicode host-side. Embedded
	// loads (setHtml) always re-encode to UTF-8, so a spliced <meta
	// charset> cannot force a code page; the only way is to transcode the
	// bytes before rendering. Empty tag = Auto-detect = pristine render.
	std::wstring decoded;
	if (!tag.empty())
	{
		const QByteArray raw(reinterpret_cast<const char*>(m_impl->rawFileBytes.data()),
		                     static_cast<int>(m_impl->rawFileBytes.size()));
		if (auto* codec = QTextCodec::codecForName(QByteArray(to_utf8(tag).c_str())))
			decoded = codec->toUnicode(raw).toStdWString();
	}

	if (!decoded.empty())
	{
		const std::wstring doc = L"<base href=\"" + to_utf16(m_impl->baseHref) + L"\">\n" + decoded;
		NavigateToString(doc, m_impl->baseHref);
		m_impl->activeEncodingTag = tag; // NavigateToString reset it; re-assert
		return;
	}

	// Unknown/undecodable label or Auto-detect: fall back to the pristine
	// Latin-1 render (identical to the initial sniffed view — never blank).
	NavigateToString(CharsetOverride::BytesToLatin1(m_impl->rawFileBytes),
	                 m_impl->baseHref);
	m_impl->activeEncodingTag = tag; // NavigateToString reset it; re-assert
}

//------------------------------------------------------------------------
std::wstring QtWebEngineBackend::GetActiveEncodingTag() const
{
	return m_impl->activeEncodingTag;
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
