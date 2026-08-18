// Linux-only — compiled only with Qt Web Engine headers on the include path.
#include "EdgeLister.h"
#include "Globals.h"
#include "IWebView.h"
#include "Navigator.h"
#include "WebView/QtWebEngineBackend.h"

#include <QWidget>
#include <QWindow>
#include <QCoreApplication>
#include <QVBoxLayout>

#include <cstdio>
#include <memory>
#include <mutex>

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
// Known native-Wayland limitation (Ctrl+Q quick view): DC's LCL
// widgetset (`TQtMainWindow.ChangeParent` in
// `lcl/interfaces/qt6/qtwidgets.pas:7459-7484`) retains `Qt::Window`
// on the quick-view form. The `QWebEngineView`'s Chromium compositor
// surface then attaches to a `wl_surface` that the compositor
// positions independently of DC's panel tree — empirically the lister
// appears at screen center and DC's main window jumps to match it.
// Plugin-side mitigations attempted in the `mitigate-wayland-ctrlq-jump`
// change (own container with parent flag-strip, deferred
// `Navigator::Open`, deferred `container->show()` to first `QShowEvent`)
// do not resolve the promotion because the Chromium compositor surface
// is initialized when the QWebEngineView's widget is shown, not when
// `setHtml` is called — and Qt Web Engine's embedding into an
// embedded-vs-toplevel widget chain on Wayland is determined upstream
// in Qt. The recommended workaround is `QT_QPA_PLATFORM=xcb doublecmd`.
// The Ctrl+Q quick-view symptom is documented in `Readme.md` and
// `AGENTS.md`.
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
	impl->backend = std::make_shared<QtWebEngineBackend>("ev://assets.example/loader.html");
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
