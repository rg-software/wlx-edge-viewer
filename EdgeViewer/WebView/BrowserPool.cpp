// Linux-only — compiled only with Qt Web Engine headers on the include path.
#include "BrowserPool.h"
#include "QtWebEngineBackend.h"
#include "../Navigator.h"
#include "../Globals.h"

#include <QApplication>
#include <QWidget>
#include <QVBoxLayout>
#include <QTimer>
#include <QUrl>

#include <memory>
#include <utility>

namespace EdgeViewer {

namespace {

BrowserPool g_pool;

// Forward-declared in QtWebEngineScheme.h so DllMain can call this
// from DLL_PROCESS_ATTACH without dragging the QtWebEngineBackend
// PIMPL'd type into a translation unit that doesn't otherwise need it.
void QtWebEngineBackend_RegisterSchemeOnce_Linux()
{
	QtWebEngineBackend::RegisterSchemeOnce();
}

} // anonymous namespace

//------------------------------------------------------------------------
void BrowserPool::Initialize()
{
	if (g_pool.m_stashWidget) return; // idempotent

	// The "stash" is a 1×1 hidden top-level widget that anchors
	// spare QWebEngineViews between ListLoadW / ListCloseWindow
	// cycles. It must be SHOWN (not just created) so Qt Web Engine's
	// compositor surface attaches to a real mapped wl_surface. On the
	// offscreen platform-plugin, an unshown top-level parent crashes
	// app.exec() once the QWebEngineView's native window is realized.
	// On a real DC session this is a 1×1 invisible frame that briefly
	// appears on-screen (acceptable for the embedded plugin context).
	g_pool.m_stashWidget = new QWidget();
	g_pool.m_stashWidget->setObjectName("edgeviewer.browserpool.stash");
	g_pool.m_stashWidget->resize(1, 1);
}

//------------------------------------------------------------------------
void BrowserPool::Shutdown()
{
	if (!g_pool.m_stashWidget) return; // idempotent

	while (!g_pool.m_spares.empty())
	{
		auto& entry = g_pool.m_spares.front();
		if (entry.backend) entry.backend->Close();
		g_pool.m_spares.pop_front();
	}
	g_pool.m_spareBuildScheduled = false;

	if (g_pool.m_stashWidget)
	{
		delete g_pool.m_stashWidget;
		g_pool.m_stashWidget = nullptr;
	}
}

//------------------------------------------------------------------------
BrowserPool::AcquireResult BrowserPool::Acquire(QWidget* parentWindow,
                                              const std::wstring& fileToLoad)
{
	AcquireResult result{};

	if (!g_pool.m_stashWidget)
	{
		Initialize();
	}

	SpareEntry* entry = nullptr;
	if (!g_pool.m_spares.empty())
	{
		entry = &g_pool.m_spares.front();
		g_pool.m_spares.pop_front();
	}
	else
	{
		entry = g_pool.buildSpare();
	}

	if (!entry || !entry->backend || !entry->view)
	{
		g_pool.scheduleBuildSpare();
		return result;
	}

	result.container = new QWidget(parentWindow);
	QVBoxLayout* layout = new QVBoxLayout(result.container);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(0);

	entry->view->setParent(result.container);
	layout->addWidget(entry->view);
	result.container->setFocusProxy(entry->view);

	result.view = entry->view;
	result.backend = std::shared_ptr<IWebView>(
		entry->backend, static_cast<IWebView*>(entry->backend.get()));

	g_pool.scheduleBuildSpare();

	result.container->show();
	Navigator nav(*result.backend);
	nav.Open(fileToLoad);

	return result;
}

//------------------------------------------------------------------------
void BrowserPool::Release(QWidget* container)
{
	if (!container) return;
	if (!g_pool.m_stashWidget) return;

	QWidget* view = container->findChild<QWidget*>();
	std::shared_ptr<IWebView> backend;
	auto it = gs_Views.find(container);
	if (it != gs_Views.end())
	{
		backend = std::static_pointer_cast<QtWebEngineBackend>(it->second);
	}

	if (view)
	{
		view->setParent(g_pool.m_stashWidget);
		view->hide();
		g_pool.m_spares.push_back({backend, view});
	}
}

//------------------------------------------------------------------------
BrowserPool::SpareEntry* BrowserPool::buildSpare()
{
	// BISECT: don't use stash — parent the spare view directly to an
	// existing top-level (DC's main window in production, harness's
	// parent widget in offscreen tests). Hypothesis: the crash on
	// offscreen is caused by the view being parented to a top-level
	// widget that isn't the QApplication's primary top-level. Skipping
	// the stash and using the first existing top-level might avoid the
	// crash on offscreen while still working on real Wayland.
	QWidget* anchor = nullptr;
	const auto tops = QApplication::topLevelWidgets();
	if (!tops.isEmpty())
		anchor = tops.first();

	auto backend = std::make_shared<QtWebEngineBackend>("ev://assets.example/loader.html");
	if (!backend) return nullptr;

	auto* rawView = static_cast<QWidget*>(backend->GetWidget());
	if (!rawView) return nullptr;

	rawView->setParent(anchor);
	rawView->hide();

	auto ibase = std::shared_ptr<IWebView>(
		backend, static_cast<IWebView*>(backend.get()));
	m_spares.push_back({ibase, rawView});
	return &m_spares.back();
}

//------------------------------------------------------------------------
void BrowserPool::scheduleBuildSpare()
{
	if (m_spareBuildScheduled) return;
	if (!m_stashWidget) return;
	m_spareBuildScheduled = true;

	QTimer::singleShot(0, m_stashWidget, [this]() {
		m_spareBuildScheduled = false;
		buildSpare();
	});
}

} // namespace EdgeViewer
//------------------------------------------------------------------------
