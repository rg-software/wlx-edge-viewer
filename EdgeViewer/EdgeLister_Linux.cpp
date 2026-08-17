// Linux-only — compiled only with Qt Web Engine headers on the include path.
#include "EdgeLister.h"
#include "Globals.h"
#include "IWebView.h"
#include "Navigator.h"
#include "WebView/QtWebEngineBackend.h"

#include <QWidget>
#include <QVBoxLayout>

#include <memory>
#include <mutex>

//------------------------------------------------------------------------
// EdgeLister on Linux: the parent window that DC passes to ListLoadW is
// a QWidget* (Double Commander's Qt6 build). The plugin creates its own
// container QWidget (mirroring the qtpdfview_qt plugin's pattern:
// new QFrame((QWidget*)ParentWin) + QVBoxLayout + show()), embeds the
// QWebEngineView into that container via a layout, and returns the
// container as the plugin handle. DC's subsequent ResizeWindow on the
// returned handle then sizes OUR container (not DC's own widget), which
// keeps the plugin's geometry management under plugin control.
//
// This differs from the earlier return-ParentWin approach, where DC's
// ResizeWindow operated on DC's own viewer-form widget. Returning our
// own container also avoids creating the QWebEngineView's compositor
// surface directly under DC's widget — instead the surface is parented
// to our plain QWidget, which Qt can manage as a normal child without
// the Wayland embedded-QMainWindow surface-promotion behavior that
// makes Ctrl+Q (quick view) jump the DC main window.
//
// gs_Views maps void* (= container QWidget*) -> shared_ptr<IWebView>.
// ListLoadNext and friends on DC arrive on the main Qt thread, so we
// call Navigator::Open directly on the QtWebEngineBackend — no
// WM_COPYDATA indirection (design Decision 7).
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