// Linux-only — compiled only with Qt Web Engine headers on the include path.
#include "EdgeLister.h"
#include "Globals.h"
#include "IWebView.h"
#include "Navigator.h"
#include "WebView/QtWebEngineBackend.h"

#include <QWidget>
#include <QWindow>
#include <QEvent>
#include <QObject>
#include <QCoreApplication>
#include <QVBoxLayout>

#include <cstdio>
#include <memory>
#include <mutex>

//------------------------------------------------------------------------
// EdgeLister on Linux: the parent window that DC passes to ListLoadW is
// a QWidget* (Double Commander's Qt6 build).
//
// DC calls `ListLoadW` BEFORE it shows the parent form (whether it is
// a fresh F3 lister or a Ctrl+Q quick-view): the form is created,
// parented in, and only then displayed. Inside `Create`, we build our
// own container with a QVBoxLayout holding the `QWebEngineView`, then
// call `Navigator::Open(file)` which calls `QWebEngineView::setHtml`
// — and that triggers Qt Web Engine's Chromium compositor to spin up
// its native window. If the parent form's native window does not yet
// exist (the parent is still hidden because DC's `Show` has not run
// yet), Qt Web Engine creates a new top-level `wl_surface` for the
// compositor on native Wayland. That surface is independent of the
// panel's `wl_surface`; once the parent form finally gets shown and
// DC's panel/tab tree establishes its own surface hierarchy, the
// compositor surface stays parked as the escape — the lister appears
// at screen center and DC's main window jumps to match it.
//
// The empirical observable is "only the first creation is problematic"
// — once the panel/tree has been established by any prior Ctrl+Q
// open, subsequent opens reuse the established surface tree and embed
// cleanly. The minimal plugin-side fix is therefore to defer the very
// first `Navigator::Open` until the parent form's `QShowEvent` has
// fired, so the Chromium compositor initializes against an already-
// realized parent surface. Implemented via `FirstShowHook` below:
// on the first `QShowEvent` of the observed parent, fire
// `Navigator::Open(fileToLoad)`; otherwise the hook self-cancels
// (cancellation is driven from `OpenIn`, which ListLoadNextW reaches
// before the first show — a deferred fire after a navigation would
// otherwise overwrite the new file with the original).
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

//------------------------------------------------------------------------
// One-shot event filter that defers the very first Navigator::Open for
// a lister until its parent form has actually been shown. The hooks are
// parented to the container QWidget so Qt's QObject ownership takes
// care of cleanup when the container is destroyed (no explicit
// ListCloseWindow cancellation is needed on the shared DllMain.cpp
// path). OpenIn() (the ListLoadNextW/PrintW/SearchW entry points)
// cancels any still-pending hook so a navigation that races ahead of
// the first show is not then overwritten with the deferred file.
class FirstShowHook : public QObject
{
public:
	FirstShowHook(QWidget* observed, std::shared_ptr<IWebView> backend,
	              std::wstring file, QObject* parentObject)
		: QObject(parentObject),
		  m_observed(observed),
		  m_backend(std::move(backend)),
		  m_file(std::move(file))
	{
		if (m_observed) m_observed->installEventFilter(this);
#if defined(EDGEVIEWER_LINUX_DEBUG)
		std::fprintf(stderr,
			"[edgeviewer] FirstShowHook: installed on observed=%p "
			"(parent->isVisible=%d) — awaiting QEvent::Show\n",
			static_cast<const void*>(m_observed),
			m_observed ? (int)m_observed->isVisible() : -1);
		std::fflush(stderr);
#endif
	}

	bool eventFilter(QObject* watched, QEvent* event) override
	{
		if (m_cancelled) return false;
		if (watched == m_observed && event->type() == QEvent::Show)
		{
			cancel();
#if defined(EDGEVIEWER_LINUX_DEBUG)
			std::fprintf(stderr,
				"[edgeviewer] FirstShowHook: QShowEvent received on "
				"observed=%p (parent->window=%p) — firing deferred "
				"Navigator::Open\n",
				static_cast<const void*>(m_observed),
				m_observed ? static_cast<const void*>(m_observed->window()) : nullptr);
			std::fflush(stderr);
#endif
			Navigator nav(*m_backend);
			nav.Open(std::move(m_file));
			deleteLater();
			return true;
		}
#if defined(EDGEVIEWER_LINUX_DEBUG)
		// Surface any unexpected events the filter observes on the
		// watched widget so we can see in the log whether QShowEvent
		// ever arrives (vs. only QEvent::Polish, ChildPolished, etc.).
		if (watched == m_observed)
		{
			static const char* evnames[QEvent::User + 1] = {};
			(void)evnames;
			const char* name = "unknown";
			switch (event->type()) {
			case QEvent::Show: name = "Show"; break;
			case QEvent::Hide: name = "Hide"; break;
			case QEvent::Polish: name = "Polish"; break;
			case QEvent::Move: name = "Move"; break;
			case QEvent::Resize: name = "Resize"; break;
			case QEvent::Create: name = "Create"; break;
			default: break;
			}
			std::fprintf(stderr,
				"[edgeviewer] FirstShowHook: observed=%p got QEvent::%s "
				"(cancelled=%d)\n",
				static_cast<const void*>(m_observed), name, (int)m_cancelled);
			std::fflush(stderr);
		}
#endif
		return false;
	}

	void cancel()
	{
		m_cancelled = true;
		if (m_observed) m_observed->removeEventFilter(this);
#if defined(EDGEVIEWER_LINUX_DEBUG)
		std::fprintf(stderr,
			"[edgeviewer] FirstShowHook: cancelled (observed=%p)\n",
			static_cast<const void*>(m_observed));
		std::fflush(stderr);
#endif
	}

private:
	QWidget* m_observed;
	std::shared_ptr<IWebView> m_backend;
	std::wstring m_file;
	bool m_cancelled = false;
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
		QWidget* form = parent->parentWidget();
		QWidget* top = parent->window();
		int row = 0;
		std::fprintf(stderr,
			"[edgeviewer] EdgeLister::Create: qpa=%s parent=%p class=%s "
			"flags=0x%x (parent->window=%p topClass=%s topFlags=0x%x) "
			"parent->parentWidget=%p formClass=%s formFlags=0x%x "
			"form->parentWidget=%p form->window=%p\n",
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
			form ? static_cast<const void*>(form->window()) : nullptr);
		// Walk the full chain of parentWidget up to window() so we can
		// see which widget in the chain is the actual top-level and
		// whether Qt::Window shows up anywhere.
		for (QWidget* p = parent; p; p = p->parentWidget(), ++row)
		{
			QWidget* w = p->window();
			std::fprintf(stderr,
				"[edgeviewer]   chain[%d] %p class=%s flags=0x%x "
				"window=%p topFlags=0x%x isWindow=%d\n",
				row, static_cast<const void*>(p),
				p->metaObject()->className(),
				static_cast<unsigned>(p->windowFlags()),
				static_cast<const void*>(w),
				w ? static_cast<unsigned>(w->windowFlags()) : 0u,
				(int)p->isWindow());
		}
		std::fflush(stderr);
	}
#endif

	// Detect Ctrl+Q (quick view): LCL's `WlxPrepareContainer` passes us
	// the form's central widget (QMainWindow::GetContainerWidget → the
	// central widget), not the QMainWindow itself. The discriminator
	// between F3 (genuine top-level form) and Ctrl+Q (reparented form)
	// is not on our parent (the central widget never carries Qt::Window)
	// but one level up: the form's `parentWidget()`. On F3, the form is
	// a top-level window and its `parentWidget()` is null. On Ctrl+Q, DC
	// has reparented the form into the quick-view panel and its
	// `parentWidget()` is the panel. The form *itself* retains Qt::Window
	// via LCL's `TQtMainWindow.ChangeParent`; stripping that flag on the
	// form and re-showing turns it into a normal child widget sharing the
	// panel's `wl_surface`, dissolving the escaped-surface symptom.
	QWidget* formWidget = parent->parentWidget();
	const bool isQuickView = formWidget && (formWidget->parentWidget() != nullptr);
	if (isQuickView && formWidget)
	{
		formWidget->setWindowFlags(formWidget->windowFlags() & ~Qt::Window);
		formWidget->show();
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

	// Defer the initial Navigator::Open until the parent form has
	// been shown. On Wayland, calling QWebEngineView::setHtml while
	// the parent is unmapped creates the compositor surface against
	// the wrong wl_surface tree and triggers the first-creation-only
	// "lister at screen center, DC main window jumps" symptom. Waiting
	// for the parent's first QShowEvent ensures the parent chain's
	// wl_surface is established before Chromium spins up its
	// compositor. The hook is parented to `impl->container` so Qt
	// cleans it up on container destruction. See FirstShowHook for
	// why this is also undone by OpenIn() before ListLoadNextW.
	auto* hook = new FirstShowHook(parent, impl->backend, fileToLoad, impl->container);
	impl->container->setProperty("edgeviewer.firstShowHook",
		QVariant::fromValue(static_cast<void*>(hook)));
	if (parent->isVisible())
	{
#if defined(EDGEVIEWER_LINUX_DEBUG)
		std::fprintf(stderr,
			"[edgeviewer] EdgeLister::Create: parent->isVisible=true at Create, "
			"firing Navigator::Open synchronously (deferred hook abandoned)\n");
		std::fflush(stderr);
#endif
		// Defensive: parent was already mapped when Create ran (rare
		// edge case — e.g. reused widget on a path I haven't seen).
		// Fire synchronously and self-destruct so we don't leak.
		hook->cancel();
		Navigator nav(*impl->backend);
		nav.Open(fileToLoad);
		delete hook;
		impl->container->setProperty("edgeviewer.firstShowHook", QVariant());
	}
	else
	{
#if defined(EDGEVIEWER_LINUX_DEBUG)
		std::fprintf(stderr,
			"[edgeviewer] EdgeLister::Create: parent->isVisible=false at Create, "
			"FirstShowHook installed, awaiting parent QShowEvent\n");
		std::fflush(stderr);
#endif
	}
	return impl->container;
}

//------------------------------------------------------------------------
// Direct-Navigator paths (Decision 7): on Linux, ListLoadNext and
// friends arrive on the main Qt thread, so we skip WM_COPYDATA and
// call Navigator::Open directly on the stored backend.
void EdgeLister::OpenIn(void* listWin, const std::wstring& fileToLoad)
{
	// If a deferred-first-show hook is still pending for this lister
	// (rare: a ListLoadNextW arrived before the parent's QShowEvent),
	// cancel it so this navigation's file wins. The hook is tracked
	// via a QWidget property stored as void* to avoid needing
	// Q_OBJECT / moc on a .cpp-defined class.
	if (auto* container = static_cast<QWidget*>(listWin))
	{
		const QVariant v = container->property("edgeviewer.firstShowHook");
		if (v.isValid() && v.canConvert<void*>())
		{
			if (auto* hook = static_cast<FirstShowHook*>(v.value<void*>()))
				hook->cancel();
		}
#if defined(EDGEVIEWER_LINUX_DEBUG)
		std::fprintf(stderr,
			"[edgeviewer] OpenIn: cancellation check done, property-clear\n");
		std::fflush(stderr);
#endif
	}

	std::scoped_lock lock(g_viewsMutex);
	auto it = gs_Views.find(listWin);
	if (it == gs_Views.end()) return;
	Navigator nav(*it->second);
	nav.Open(fileToLoad);
}
//------------------------------------------------------------------------