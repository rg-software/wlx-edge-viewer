// Linux-only — compiled only with Qt Web Engine headers on the include path.
#include "EdgeLister.h"
#include "Globals.h"
#include "IWebView.h"
#include "Navigator.h"
#include "WebView/QtWebEngineBackend.h"

#include <QWidget>
#include <QWindow>
#include <QVBoxLayout>

#include <cstdio>
#include <memory>
#include <mutex>

//------------------------------------------------------------------------
// EdgeLister on Linux: the parent window that DC passes to ListLoadW is
// a QWidget* (Double Commander's Qt6 build). Under F3, that parent is a
// genuine top-level QMainWindow with no parentWidget() — it stays a
// normal window. Under Ctrl+Q, DC reparents the same kind of form into
// the quick-view panel but LCL's `TQtMainWindow.ChangeParent`
// (`lcl/interfaces/qt6/qtwidgets.pas:7459-7484`) preserves the form's
// `Qt::Window` flag. On native Wayland that combination makes the form
// its own top-level `wl_surface`, separate from the panel; the
// QWebEngineView's compositor subsurface then attaches to that escaped
// surface, and the compositor positions both independently — the
// observable symptom is the lister appearing at screen center and DC's
// main window jumping to follow it. The `ev://` custom scheme, the
// QWidget* container with QVBoxLayout, and the own-container return
// value cover load-bearing and geometry concerns but cannot dissolve
// that separation because the escape is on DC's form, not on our
// widget. We therefore detect the Ctrl+Q case and strip `Qt::Window`
// from the parent before creating the container so the form becomes a
// regular child widget sharing the panel's `wl_surface`. Under F3 the
// heuristic is false and we leave the genuine top-level alone.
//
// gs_Views maps void* (= container QWidget*) -> shared_ptr<IWebView>.
// ListLoadNext and friends on DC arrive on the main Qt thread, so we
// call Navigator::Open directly on the QtWebEngineBackend — no
// WM_COPYDATA indirection.
//------------------------------------------------------------------------

void EdgeLister::RegisterClass()
{
	// No Win32 class registration on Linux; the WLX exports are declared
	// via the CMake linker version script (design Decision 11). This is a
	// deliberate no-op.
}

//------------------------------------------------------------------------
// Guards gs_Views access. All WLX callbacks arrive on the main Qt
// thread (Decision 7), so this is belt-and-braces rather than a
// required synchronization primitive.
namespace { std::mutex g_viewsMutex; }

//------------------------------------------------------------------------
// Per-lister-instance state: the container QWidget we own + its
// embedded QWebEngineView + the QtWebEngineBackend that drives it.
namespace {
struct LinuxBackend {
	QWidget* container = nullptr;
	QWidget* view = nullptr;
	std::shared_ptr<IWebView> backend;
};
}

//------------------------------------------------------------------------
// Create: instantiate a QtWebEngineBackend, build a container QWidget
// parented to DC's parent, lay out the QWebEngineView inside, store
// the container in gs_Views, return the container as the plugin
// handle, then run the initial Navigator::Open.
void* EdgeLister::Create(void* parentWindow, const std::wstring& fileToLoad, const ProcessorInterface* processor)
{
	auto* parent = static_cast<QWidget*>(parentWindow);
	if (!parent)
		return nullptr;

#if defined(EDGEVIEWER_LINUX_DEBUG)
	// Debug-log the widget tree DC hands us so we can verify the
	// Ctrl+Q vs F3 heuristic against real output. This block is
	// compiled in only when EDGEVIEWER_LINUX_DEBUG is defined (e.g.
	// `-DEDGEVIEWER_LINUX_DEBUG` on the Linux CMake build line);
	// otherwise it is a no-op. The Windows-side Log.h is not used
	// because it pulls in <windows.h>.
	{
		std::fprintf(stderr,
			"[edgeviewer] EdgeLister::Create: parent=%p class=%s "
			"windowFlags=0x%x geometry=(%d,%d %dx%d) "
			"parentWidget=%p parentWidgetChain=[",
			static_cast<const void*>(parent),
			parent->metaObject()->className(),
			static_cast<unsigned>(parent->windowFlags()),
			parent->geometry().x(), parent->geometry().y(),
			parent->geometry().width(), parent->geometry().height(),
			static_cast<const void*>(parent->parentWidget()));
		for (QWidget* p = parent->parentWidget(); p; p = p->parentWidget())
		{
			QWidget* w = p->window();
			std::fprintf(stderr, " (%s@%p top=%p)",
				p->metaObject()->className(),
				static_cast<const void*>(p),
				static_cast<const void*>(w));
		}
		QWindow* wh = parent->windowHandle();
		std::fprintf(stderr, "] windowHandle=%p\n", static_cast<const void*>(wh));
		std::fflush(stderr);
	}
#endif

	// Detect Ctrl+Q (quick view): DC reparents the form into the quick-view
	// panel but `TQtMainWindow.ChangeParent` preserves `Qt::Window`.
	// That `parentWidget() != nullptr && (flags & Qt::Window)` conjunction
	// is the only condition that distinguishes this from an F3 genuine
	// top-level form (`parentWidget()` is null there). Strip the flag and
	// re-show so the form becomes a regular child of the panel's
	// `wl_surface` on native Wayland (no escaped top-level surface for
	// the QWebEngineView's compositor subsurface to be misplaced onto).
	const bool isQuickView = (parent->parentWidget() != nullptr)
		&& ((parent->windowFlags() & Qt::Window) != 0);
	if (isQuickView)
	{
		parent->setWindowFlags(parent->windowFlags() & ~Qt::Window);
		parent->show();
	}

	auto* impl = new LinuxBackend();

	// The baseUri tells QtWebEngineBackend which virtual host to use for
	// relative refs in NavigateToString (Decision 9). We pick a generic
	// assets host since the actual type directory is encoded in the
	// loader's own <link> refs (which are absolute anyway).
	impl->backend = std::make_shared<QtWebEngineBackend>("ev://assets.example/loader.html");
	if (!impl->backend) { delete impl; return nullptr; }

	auto* qt = dynamic_cast<QtWebEngineBackend*>(impl->backend.get());
	if (!qt) { delete impl; return nullptr; }

	impl->view = static_cast<QWidget*>(qt->GetWidget());
	if (!impl->view) { delete impl; return nullptr; }

	// Build our own container QWidget parented to DC's viewer-form
	// container. Mirroring qtpdfview_qt's pattern: a plain QWidget with
	// a QVBoxLayout that holds the actual content widget. DC's
	// ResizeWindow(GetListerRect) will operate on this container (the
	// handle we return), so we own the geometry. The layout takes care
	// of forwarding the container's resize to the embedded QWebEngineView.
	impl->container = new QWidget(parent);
	QVBoxLayout* layout = new QVBoxLayout(impl->container);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(0);
	impl->view->setParent(impl->container);
	layout->addWidget(impl->view);
	impl->container->setFocusProxy(impl->view);
	impl->container->show();

	{
		std::scoped_lock lock(g_viewsMutex);
		gs_Views[impl->container] = impl->backend;
	}

	Navigator nav(*impl->backend);
	nav.Open(fileToLoad);
	return impl->container;
}

//------------------------------------------------------------------------
// Direct-Navigator paths (Decision 7): on Linux, ListLoadNext and
// friends arrive on the main Qt thread, so we skip WM_COPYDATA and
// call Navigator::Open directly on the stored backend.
void EdgeLister::OpenIn(void* listWin, const std::wstring& fileToLoad)
{
	std::scoped_lock lock(g_viewsMutex);
	auto it = gs_Views.find(listWin);
	if (it == gs_Views.end()) return;
	Navigator nav(*it->second);
	nav.Open(fileToLoad);
}
//------------------------------------------------------------------------