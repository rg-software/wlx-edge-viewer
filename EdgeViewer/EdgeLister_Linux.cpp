// Linux-only — compiled only with Qt Web Engine headers on the include path.
#include "EdgeLister.h"
#include "Globals.h"
#include "IWebView.h"
#include "Navigator.h"
#include "WebView/QtWebEngineBackend.h"

#include <QEvent>
#include <QWidget>

#include <memory>
#include <mutex>

//------------------------------------------------------------------------
// EdgeLister on Linux: the parent window that DC passes to ListLoadW is
// a QWidget* (Double Commander's Qt6 build). The plugin embeds a
// QWebEngineView into that widget as a child. The "Class" / "Register"
// steps are no-ops for a Qt plugin (no Win32 class registration); only
// "Create" is meaningful.
//
// gs_Views maps void* (= QWidget*) -> shared_ptr<IWebView>. ListLoadNext
// and friends on DC arrive on the main Qt thread, so we can call
// Navigator::Open directly on the QtWebEngineBackend — no WM_COPYDATA
// indirection (design Decision 7).
//------------------------------------------------------------------------

void EdgeLister::RegisterClass()
{
	// No Win32 class registration on Linux; the WLX exports are declared
	// via the CMake linker version script (design Decision 11). This is
	// a deliberate no-op.
}

//------------------------------------------------------------------------
// Guards gs_Views access. All WLX callbacks arrive on the main Qt
// thread (Decision 7), so this is belt-and-braces rather than a
// required synchronization primitive.
namespace { std::mutex g_viewsMutex; }

//------------------------------------------------------------------------
// Per-lister-instance state: a single QtWebEngineBackend + its view
// QWidget + the parent widget for size-forwarding.
namespace {
struct LinuxBackend {
	QWidget* widget = nullptr;
	std::shared_ptr<IWebView> backend;
};
}

//------------------------------------------------------------------------
// Forward the parent's resize to the QWebEngineView. The resizer is a
// child of the view widget: when the view is destroyed (Close() ->
// deleteLater, or the parent widget is torn down) it is destroyed too,
// and Qt automatically removes it from the parent's event-filter list.
class ViewResizer : public QObject
{
public:
	ViewResizer(QWidget* view, QObject* parent) : QObject(parent), m_view(view) {}

	bool eventFilter(QObject* watched, QEvent* event) override
	{
		if (event->type() == QEvent::Resize && m_view)
		{
			auto* parent = static_cast<QWidget*>(watched);
			m_view->setGeometry(parent->rect());
		}
		return QObject::eventFilter(watched, event);
	}

private:
	QWidget* m_view;
};

//------------------------------------------------------------------------
// Defer the QWebEngineView's show() until the parent QWidget is actually
// shown. On native Wayland (and to a lesser degree under XWayland), calling
// show() on a QWebEngineView while its parent is hidden causes the view's
// compositor surface to be created before the parent's native surface is
// realized, which:
//   - forces an extra invalidation/expose on the parent's region once it
//     does get shown (the "repaint" observed under QT_QPA_PLATFORM=xcb);
//   - on native Wayland, interacts badly with the embedded QMainWindow
//     form's Qt::Window retention (the DC widgetset's TQtMainWindow.
//     ChangeParent preserves Qt::Window), making the promoted toplevel
//     surface more likely to be positioned by the compositor instead of
//     attaching as a subsurface.
//
// DC's ListLoadW arrives BEFORE the viewer form is shown (uquickviewpanel
// .pas:159-160 and ShowViewer's Viewer.LoadFile before Viewer.Show), so
// the parent is hidden at the moment we run. Waiting for the parent's
// QEvent::Show lets the view acquire its surface at the same time as its
// parent — embedded, not promoted.
class DeferredShow : public QObject
{
public:
	DeferredShow(QWidget* view, QWidget* parent) : QObject(view), m_view(view), m_parent(parent)
	{
		m_parent->installEventFilter(this);
	}

	bool eventFilter(QObject* watched, QEvent* event) override
	{
		if (event->type() == QEvent::Show && watched == m_parent && m_view && !m_view->isVisible())
		{
			m_view->show();
		}
		return QObject::eventFilter(watched, event);
	}

private:
	QWidget* m_view;
	QWidget* m_parent;
};

//------------------------------------------------------------------------
// Create: instantiate a QtWebEngineBackend, embed its view into the
// parent QWidget*, store in gs_Views, then run the initial
// Navigator::Open. (Linux port of task 4.2's "Create" step.)
bool EdgeLister::Create(void* parentWindow, const std::wstring& fileToLoad, const ProcessorInterface* processor)
{
	auto* parent = static_cast<QWidget*>(parentWindow);
	if (!parent)
		return false;

	auto* impl = new LinuxBackend();

	// The baseUri tells QtWebEngineBackend which virtual host to use for
	// relative refs in NavigateToString (Decision 9). We pick a generic
	// assets host since the actual type directory is encoded in the
	// loader's own <link> refs (which are absolute anyway).
	impl->backend = std::make_shared<QtWebEngineBackend>("ev://assets.example/loader.html");
	if (!impl->backend) { delete impl; return false; }

	auto* qt = dynamic_cast<QtWebEngineBackend*>(impl->backend.get());
	if (!qt) { delete impl; return false; }

	impl->widget = static_cast<QWidget*>(qt->GetWidget());
	if (!impl->widget) { delete impl; return false; }

	// Force the parent's native window to be realized up front. DC calls
	// ListLoadW while the viewer form is still hidden (uquickviewpanel
	// .pas:159-160); without this, the QWebEngineView's compositor
	// surface gets created before its parent's Wayland surface exists,
	// which is the timing that interacts with the embedded QMainWindow's
	// retained Qt::Window flag.
	parent->createWinId();

	impl->widget->setParent(parent);
	impl->widget->setGeometry(parent->rect());
	// Defer show() until the parent is actually shown. See the
	// DeferredShow class comment above for the rationale.
	new DeferredShow(impl->widget, parent);

	parent->installEventFilter(new ViewResizer(impl->widget, impl->widget));

	{
		std::scoped_lock lock(g_viewsMutex);
		gs_Views[parent] = impl->backend;
	}

	Navigator nav(*impl->backend);
	nav.Open(fileToLoad);
	return true;
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
