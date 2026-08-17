# docs-linux-backend-qt

Document the switch from WebKitGTK to Qt 6 WebEngine for the Linux backend: the rationale (DC Qt6 needs a QWidget embeddable web view; Qt Web Engine behaves differently from WebKitGTK on Wayland subsurface promotion), the http→ev:// scheme rewrite (Qt Web Engine also blocks registering http), and the native-Wayland Ctrl+Q quick-view symptom (DC widgetset bug, XWayland workaround).
