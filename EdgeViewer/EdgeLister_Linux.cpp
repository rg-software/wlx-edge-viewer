// Linux-only — compiled only with Qt Web Engine headers on the include path.
#include "EdgeLister.h"
#include "Globals.h"
#include "IWebView.h"
#include "Navigator.h"
#include "WebView/QtWebEngineBackend.h"

#include <QEvent>
#include <QKeyEvent>
#include <QObject>
#include <QWebEnginePage>
#include <QWebEngineView>
#include <QWidget>
#include <QWindow>
#include <QCoreApplication>
#include <QVBoxLayout>

#include <cstdio>
#include <memory>
#include <mutex>

extern "C" void ListCloseWindow(void* ListWin);

//------------------------------------------------------------------------
// EdgeLister on Linux: the parent window that DC passes to ListLoadW is
// a QWidget* (Double Commander's Qt6 build). The plugin creates its
// own `QWidget*` container parented to DC's parent (mirroring the
// `j2969719/doublecmd-plugins/wlx/qtpdfview_qt` plugin's pattern:
// `new QFrame((QWidget*)ParentWin)` + `QVBoxLayout` + `show()`), embeds
// the `QWebEngineView` into that container via a layout, and returns the
// container as the plugin handle. DC's subsequent `ResizeWindow` on the
// returned handle then sizes OUR container (not DC's own widget),
// keeping the plugin's geometry management under plugin control.
//
// `gs_Views` maps `void*` (= container QWidget*) -> `shared_ptr<IWebView>`.
// `ListLoadNext` and friends on DC arrive on the main Qt thread, so we
// call `Navigator::Open` directly on the `QtWebEngineBackend` — no
// `WM_COPYDATA` indirection.
//
// Known native-Wayland limitation (Ctrl+Q quick view): the first
// Ctrl+Q open of a session escapes DC's panel tree - empirically the
// lister appears at screen center and DC's main window jumps to match
// it; subsequent opens and F3 are unaffected. Instrumentation falsified
// the earlier `TQtMainWindow.ChangeParent`/`Qt::Window` explanation (no
// widget in the chain carries `Qt::Window`): the escape is a
// re-created ancestor toplevel (DC main window's `xdg_toplevel`
// destroyed and re-created on first Ctrl+Q; Chromium's EGL compositor
// attaches to the new surface). Shipped: documentation-only Branch C -
// software rendering (`QT_QUICK_BACKEND=software` alone) eliminates the
// jump; XWayland remains the fallback. Full record:
// `openspec/changes/revisit-wayland-ctrlq-jump/evidence.md`. Workaround
// (fallback): `QT_QPA_PLATFORM=xcb doublecmd`. Documented in `Readme.md`
// and `AGENTS.md`.
//------------------------------------------------------------------------

void EdgeLister::RegisterClass()
{
	// No Win32 class registration on Linux; the WLX exports are declared
	// via the CMake linker version script (design Decision 11). This is a
	// deliberate no-op.
}

//------------------------------------------------------------------------
// Guards `gs_Views` access. All WLX callbacks arrive on the main Qt
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

// Forward declarations implemented in QtWebEngineBackend.cpp. The Linux
// backend uses these to wire the JS bridge that handles ESC: Chromium
// intercepts keyboard events at a level below Qt's QObject event system
// (a QObject::eventFilter on QWebEngineView sees many events but never
// KeyPress — confirmed by instrumentation). The bridge instead listens
// for keydown inside the page and dispatches a per-instance URL
// `ev://_close/<id>` that the global EvSchemeHandler routes back to
// a synthetic Q keypress on DC's viewer panel, which triggers DC's
// cm_ExitViewer close path. The plugin does not destroy the container
// itself; that would race DC's close logic and trigger its
// parent-widget destruction hooks (in some Qt6 widgetsets this exits
// the application).
uint64_t AllocateContainerId();
void RegisterContainer(uint64_t id, QWidget* container, QWebEnginePage* page);
void UnregisterContainer(uint64_t id);

//------------------------------------------------------------------------
// Create: instantiate a QtWebEngineBackend, build a container QWidget
// parented to DC's parent, lay out the QWebEngineView inside, store
// the container in `gs_Views`, return the container as the plugin
// handle, then run the initial `Navigator::Open`.
void* EdgeLister::Create(void* parentWindow, const std::wstring& fileToLoad, const ProcessorInterface* processor)
{
	auto* parent = static_cast<QWidget*>(parentWindow);
	if (!parent)
		return nullptr;

#if defined(EDGEVIEWER_LINUX_DEBUG)
	// Optional debug log of the widget tree DC hands us. Gated by the
	// `EDGEVIEWER_LINUX_DEBUG` cmake definition; the
	// `-DEDGEVIEWER_LINUX_DEBUG_LOGGING=ON` cmake option sets it.
	// Active output:
	//   - top-level QWidget forming `parent->window()`, its flags;
	//   - the form (`parent->parentWidget()`), its parent / window;
	//   - the full `parentWidget()` chain with each hop's class, flags,
	//     window(), and isWindow().
	{
		QWidget* form = parent->parentWidget();
		QWidget* top = parent->window();
		std::fprintf(stderr,
			"[edgeviewer] EdgeLister::Create: qpa=%s parent=%p class=%s "
			"flags=0x%x (parent->window=%p topClass=%s topFlags=0x%x) "
			"parent->parentWidget=%p formClass=%s formFlags=0x%x "
			"form->parentWidget=%p form->window=%p parent->isVisible=%d\n",
			QCoreApplication::instance()
				? QCoreApplication::instance()->property("platformName").toString().toUtf8().constData()
				: "?",
			static_cast<const void*>(parent),
			parent->metaObject()->className(),
			static_cast<unsigned>(parent->windowFlags()),
			static_cast<const void*>(top),
			top ? top->metaObject()->className() : "(null-top)",
			top ? static_cast<unsigned>(top->windowFlags()) : 0u,
			static_cast<const void*>(form),
			form ? form->metaObject()->className() : "(null-form)",
			form ? static_cast<unsigned>(form->windowFlags()) : 0u,
			form ? static_cast<const void*>(form->parentWidget()) : nullptr,
			form ? static_cast<const void*>(form->window()) : nullptr,
			(int)parent->isVisible());
		for (QWidget* p = parent; p; p = p->parentWidget())
		{
			QWidget* w = p->window();
			std::fprintf(stderr,
				"[edgeviewer]   chain %p class=%s flags=0x%x "
				"window=%p topFlags=0x%x isWindow=%d\n",
				static_cast<const void*>(p),
				p->metaObject()->className(),
				static_cast<unsigned>(p->windowFlags()),
				static_cast<const void*>(w),
				w ? static_cast<unsigned>(w->windowFlags()) : 0u,
				(int)p->isWindow());
		}
		std::fflush(stderr);
	}
#endif

	auto* impl = new LinuxBackend();

	// The baseUri tells QtWebEngineBackend which virtual host to use for
	// relative refs in NavigateToString (Decision 9). We pick a generic
	// assets host since the actual type directory is encoded in the
	// loader's own <link> refs (which are absolute anyway).
	// Allocate the ESC close-bridge container id BEFORE constructing the
	// backend: the backend's constructor embeds the id into the JS it
	// injects into every page the lister opens.
	const uint64_t containerId = AllocateContainerId();
	if (containerId == 0) { delete impl; return nullptr; }

	impl->backend = std::make_shared<QtWebEngineBackend>("ev://assets.example/loader.html", containerId, processor);
	if (!impl->backend) { delete impl; return nullptr; }
	if (!impl->backend) { delete impl; return nullptr; }

	auto* qt = dynamic_cast<QtWebEngineBackend*>(impl->backend.get());
	if (!qt) { delete impl; return nullptr; }

	impl->view = static_cast<QWidget*>(qt->GetWidget());
	if (!impl->view) { delete impl; return nullptr; }

	// Build our own container QWidget parented to DC's viewer-form
	// container, mirroring the qtpdfview_qt plugin's pattern. The
	// QVBoxLayout takes care of forwarding the container's resize to
	// the embedded QWebEngineView, so we don't need a separate
	// resize-relay event filter (which would risk being a bug
	// magnet on its own).
	impl->container = new QWidget(parent);
	QVBoxLayout* layout = new QVBoxLayout(impl->container);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(0);
	impl->view->setParent(impl->container);
	layout->addWidget(impl->view);
	impl->container->setFocusProxy(impl->view);

	// Register the container with the global scheme-handler registry so
	// ESC's `ev://_close/<id>` request can dispatch straight to
	// ListCloseWindow on this container. Auto-unregister when the
	// container is destroyed (regardless of who destroys it), so the
	// registry never holds a dangling pointer.
	RegisterContainer(containerId, impl->container,
		static_cast<QWebEngineView*>(impl->view)->page());
	QObject::connect(impl->container, &QObject::destroyed,
		[containerId]() { UnregisterContainer(containerId); });

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
