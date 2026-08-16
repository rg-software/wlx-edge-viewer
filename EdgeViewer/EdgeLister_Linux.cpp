// Linux-only — compiled only with GTK + WebKitGTK headers on the include path.
#include "EdgeLister.h"
#include "Globals.h"
#include "IWebView.h"
#include "Navigator.h"
#include "WebView/WebKitBackend.h"

#include <gtk/gtk.h>
#include <webkit2/webkit2.h>

#include <map>
#include <mutex>

//------------------------------------------------------------------------
// EdgeLister on Linux: the parent HWND that DC passes to ListLoadW is
// a GtkWidget*. The plugin embeds a WebKitWebView into that widget as a
// child. The "Class" / "Register" steps are GTK no-ops (GTK classes are
// auto-registered at link time); only "Create" is meaningful.
//
// gs_Views maps HWND (= GtkWidget*) -> shared_ptr<IWebView>. ListLoadNext
// and friends on DC arrive on the main GTK thread, so we can call
// Navigator::Open directly on the WebKitBackend — no WM_COPYDATA
// indirection (design Decision 7).
//------------------------------------------------------------------------

void EdgeLister::RegisterClass()
{
	// GTK classes auto-register at link time. The plugin's WLX exports
	// are declared via CMake visibility (design Decision 11); no runtime
	// registration needed. This is a deliberate no-op.
}

//------------------------------------------------------------------------
// Per-lister-instance state: a single WebKitBackend + its WebView's
// GtkWidget + the parent widget for size-allocate forwarding.
namespace {
struct LinuxBackend {
	GtkWidget* widget = nullptr;
	std::shared_ptr<IWebView> backend;
};
}

//------------------------------------------------------------------------
// Forward parent's size-allocate to the WebKitWebView. WebKitGTK
// auto-resizes to its allocated area, so this is mostly a no-op; it
// exists as a hook point for future custom resize handling.
static void on_parent_size_allocate(GtkWidget* /*w*/, GdkRectangle* /*rect*/, gpointer user_data)
{
	auto* impl = static_cast<LinuxBackend*>(user_data);
	if (impl && impl->widget)
		gtk_widget_queue_resize(impl->widget);  // ask WebKit to recompute
}

//------------------------------------------------------------------------
// Create: instantiate a WebKitBackend, embed its WebView into the
// parent GtkWidget*, store in gs_Views, then run the initial
// Navigator::Open. (Linux port of task 4.2's "Create" step.)
HRESULT EdgeLister_Create(GtkWidget* parentWindow, const std::wstring& fileToLoad, const ProcessorInterface* processor)
{
	if (!parentWindow) return E_INVALIDARG;

	auto* impl = new LinuxBackend();

	// The baseUri tells WebKitBackend which virtual host to use for
	// relative refs in NavigateToString (Decision 9). We pick a
	// generic assets host since the actual type directory is encoded
	// in the loader's own <link> refs (which are absolute anyway).
	impl->backend = CreateWebView("ev://assets.example/loader.html");
	if (!impl->backend) { delete impl; return E_FAIL; }

	auto* wk = dynamic_cast<WebKitBackend*>(impl->backend.get());
	if (!wk) { delete impl; return E_FAIL; }

	impl->widget = static_cast<GtkWidget*>(wk->GetWidget());
	if (!impl->widget) { delete impl; return E_FAIL; }

	gtk_container_add(GTK_CONTAINER(parentWindow), impl->widget);
	gtk_widget_show_all(parentWindow);

	g_signal_connect(G_OBJECT(parentWindow), "size-allocate",
	                 G_CALLBACK(on_parent_size_allocate), impl);

	{
		std::scoped_lock lock(g_viewsMutex);
		gs_Views[reinterpret_cast<HWND>(parentWindow)] = impl->backend;
	}

	Navigator nav(*impl->backend);
	nav.Open(fileToLoad);
	return S_OK;
}

//------------------------------------------------------------------------
// Direct-Navigator paths (Decision 7): on Linux, ListLoadNext and
// friends arrive on the main GTK thread, so we skip WM_COPYDATA and
// call Navigator::Open directly on the stored backend.
static void listLoadNext(GtkWidget* listWin, const std::wstring& fileToLoad)
{
	std::scoped_lock lock(g_viewsMutex);
	auto it = gs_Views.find(reinterpret_cast<HWND>(listWin));
	if (it == gs_Views.end()) return;
	Navigator nav(*it->second);
	nav.Open(fileToLoad);
}
//------------------------------------------------------------------------