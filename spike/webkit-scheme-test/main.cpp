// Spike 1 (Fallback A): Custom URI scheme for WebKitGTK.
//
// WebKitGTK no longer allows registering 'http' as a custom URI scheme.
// Fallback A: register a custom scheme 'ev' (short for EdgeViewer) and
// dispatch by host exactly as the primary approach intended:
//   ev://assets.example/...  -> plugin's Resources/assets/
//   ev://local.example/...   -> the file's root directory
//
// The loader HTML templates need the scheme to be configurable (http on
// Windows via SetVirtualHostNameToFolderMapping, ev on Linux via
// register_uri_scheme). This is handled via a __SCHEME__ placeholder
// added to each loader — processors substitute "http" on Windows and
// "ev" on Linux.
//
// Build on Linux:
//   cmake -B build spike/webkit-scheme-test
//   cmake --build build
//
// Run from the repo root:
//   WEBKIT_DISABLE_DMABUF_RENDERER=1 ./build/webkit_scheme_test
//   (or: GDK_BACKEND=x11 ./build/webkit_scheme_test if Wayland fails)

#include <gtk/gtk.h>
#include <webkit2/webkit2.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <iostream>

namespace fs = std::filesystem;

static const char* ASSETS_DIR = "Resources/assets";
static const char* LOCAL_ROOT = "Examples";

// --- Scheme handler -----------------------------------------------------

static void
scheme_callback(WebKitURISchemeRequest* request, gpointer user_data)
{
    const gchar* uri = webkit_uri_scheme_request_get_uri(request);
    std::string uri_str(uri);

    // URI form: "ev://HOST/PATH"
    auto scheme_end = uri_str.find("://");
    auto host_start = scheme_end + 3;
    auto host_end = uri_str.find('/', host_start);

    std::string host = (host_end != std::string::npos)
        ? uri_str.substr(host_start, host_end - host_start)
        : uri_str.substr(host_start);

    auto path_start = (host_end != std::string::npos) ? host_end + 1 : std::string::npos;
    std::string path = (path_start != std::string::npos) ? uri_str.substr(path_start) : "";

    std::cout << "[SCHEME] host=" << host << " path=" << path << std::endl;

    std::string file_path;
    if (host == "assets.example") {
        file_path = std::string(ASSETS_DIR) + "/" + path;
    } else if (host == "local.example") {
        file_path = std::string(LOCAL_ROOT) + "/" + path;
    } else {
        GError* error = g_error_new(G_IO_ERROR, G_IO_ERROR_NOT_FOUND,
            "Unknown host: %s", host.c_str());
        webkit_uri_scheme_request_finish_error(request, error);
        g_error_free(error);
        return;
    }

    if (!fs::exists(file_path) || fs::is_directory(file_path)) {
        GError* error = g_error_new(G_IO_ERROR, G_IO_ERROR_NOT_FOUND,
            "File not found: %s", file_path.c_str());
        webkit_uri_scheme_request_finish_error(request, error);
        g_error_free(error);
        return;
    }

    std::ifstream file(file_path, std::ios::binary);
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();

    std::string mime_type = "application/octet-stream";
    auto ext = fs::path(file_path).extension().string();
    if (ext == ".html") mime_type = "text/html; charset=utf-8";
    else if (ext == ".js") mime_type = "application/javascript; charset=utf-8";
    else if (ext == ".css") mime_type = "text/css; charset=utf-8";
    else if (ext == ".png") mime_type = "image/png";
    else if (ext == ".svg") mime_type = "image/svg+xml";
    else if (ext == ".json") mime_type = "application/json; charset=utf-8";
    else if (ext == ".md") mime_type = "text/markdown; charset=utf-8";

    GBytes* bytes = g_bytes_new(content.data(), content.size());
    GInputStream* stream = g_memory_input_stream_new_from_bytes(bytes);

    // Use finish_with_response to add CORS headers (WebKitGTK 2.36+).
    // The loader HTML's fetch() from ev://assets.example to ev://local.example
    // is cross-origin; without Access-Control-Allow-Origin it's blocked silently.
    WebKitURISchemeResponse* response =
        webkit_uri_scheme_response_new(stream, content.size());
    webkit_uri_scheme_response_set_content_type(response, mime_type.c_str());
    webkit_uri_scheme_response_set_http_header(response,
        "Access-Control-Allow-Origin", "*");
    webkit_uri_scheme_request_finish_with_response(request, response);

    g_object_unref(response);
    g_object_unref(stream);
    g_bytes_unref(bytes);
}

// --- Helpers ------------------------------------------------------------

static std::string
read_file(const std::string& path)
{
    std::ifstream f(path);
    if (!f) {
        std::cerr << "Cannot open: " << path << std::endl;
        return "";
    }
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

static void
replace_all(std::string& str, const std::string& from, const std::string& to)
{
    size_t pos = 0;
    while ((pos = str.find(from, pos)) != std::string::npos) {
        str.replace(pos, from.length(), to);
        pos += to.length();
    }
}

// --- Button -------------------------------------------------------------

static void
on_test_external_url(GtkWidget* button, gpointer user_data)
{
    auto* webview = WEBKIT_WEB_VIEW(user_data);
    std::cout << "[BUTTON] Loading https://example.com/" << std::endl;
    webkit_web_view_load_uri(webview, "https://example.com/");
}

// --- Main ---------------------------------------------------------------

int
main(int argc, char* argv[])
{
    gtk_init(&argc, &argv);

    if (!fs::exists(ASSETS_DIR)) {
        std::cerr << "Cannot find " << ASSETS_DIR
                  << " — run from the repo root." << std::endl;
        return 1;
    }

    std::string loader_path = std::string(ASSETS_DIR) + "/markdown/loader.html";
    if (!fs::exists(loader_path)) {
        std::cerr << "Cannot find " << loader_path << std::endl;
        return 1;
    }

    GtkWidget* window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "WebKitGTK Scheme Spike (Fallback A: ev://)");
    gtk_window_set_default_size(GTK_WINDOW(window), 900, 700);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), nullptr);

    GtkWidget* vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(window), vbox);

    // Register the custom "ev" scheme (NOT "http" — that's blocked)
    WebKitWebContext* context = webkit_web_context_get_default();
    webkit_web_context_register_uri_scheme(
        context,
        "ev",
        scheme_callback,
        nullptr,
        nullptr
    );

    std::cout << "[INIT] Registered 'ev' scheme handler" << std::endl;

    WebKitWebView* webview = WEBKIT_WEB_VIEW(webkit_web_view_new());
    gtk_widget_set_vexpand(GTK_WIDGET(webview), TRUE);
    gtk_container_add(GTK_CONTAINER(vbox), GTK_WIDGET(webview));

    GtkWidget* hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_container_set_border_width(GTK_CONTAINER(hbox), 6);
    gtk_container_add(GTK_CONTAINER(vbox), hbox);

    GtkWidget* btn_external = gtk_button_new_with_label("Test External URL");
    g_signal_connect(btn_external, "clicked", G_CALLBACK(on_test_external_url), webview);
    gtk_container_add(GTK_CONTAINER(hbox), btn_external);

    // Read the loader HTML
    std::string loader_html = read_file(loader_path);

    // Find a .md file in Examples/
    std::string sample_md_rel;  // relative path from LOCAL_ROOT
    std::string sample_md_full; // full path on disk
    if (fs::exists("Examples")) {
        for (const auto& entry : fs::directory_iterator("Examples")) {
            if (entry.path().extension() == ".md") {
                sample_md_rel = entry.path().filename().string();
                sample_md_full = entry.path().string();
                break;
            }
        }
    }
    if (sample_md_rel.empty()) {
        std::cerr << "[INIT] No .md file found in Examples/" << std::endl;
        return 1;
    }
    std::cout << "[INIT] Using sample file: " << sample_md_full << std::endl;

    // Replace placeholders matching MdProcessor.cpp
    // __BASE_URL__ is the parent directory relative path (empty for Examples/)
    replace_all(loader_html, "__BASE_URL__", "");
    replace_all(loader_html, "__CSS_NAME__", "github.css");
    replace_all(loader_html, "__MD_FILENAME__", sample_md_rel);

    // Rewrite http:// to ev:// for all plugin-internal URLs
    replace_all(loader_html, "http://assets.example", "ev://assets.example");
    replace_all(loader_html, "http://local.example", "ev://local.example");

    // Add diagnostic overlay to the page so we can see fetch errors in the window
    std::string diag_overlay =
        "<div id='diag' style='position:fixed;bottom:0;left:0;right:0;"
        "background:#fff3cd;color:#856404;padding:8px;font:14px monospace;"
        "border-top:2px solid #ffc107;z-index:9999;'>"
        "Loading...</div>"
        "<script>"
        "const _origFetch=window.fetch;"
        "window.fetch=function(u,o){"
        "  var d=document.getElementById('diag');"
        "  d.textContent='fetch('+u+')...';"
        "  return _origFetch(u,o).then(function(r){"
        "    d.textContent='fetch('+u+') -> '+r.status+' '+r.statusText;"
        "    if(!r.ok) d.style.background='#f8d7da';"
        "    return r;"
        "  }).catch(function(e){"
        "    d.textContent='fetch('+u+') FAILED: '+e;"
        "    d.style.background='#f8d7da';"
        "    throw e;"
        "  });"
        "};"
        "window.addEventListener('unhandledrejection',function(e){"
        "  document.getElementById('diag').textContent='REJECTED: '+e.reason;"
        "  document.getElementById('diag').style.background='#f8d7da';"
        "});"
        "window.addEventListener('error',function(e){"
        "  document.getElementById('diag').textContent='ERROR: '+e.message;"
        "  document.getElementById('diag').style.background='#f8d7da';"
        "});"
        "</script>";

    // Inject the diagnostic overlay right after <body>
    auto body_pos = loader_html.find("<body>");
    if (body_pos != std::string::npos) {
        loader_html.insert(body_pos + 6, diag_overlay);
    }

    // CRITICAL: load_html with base URI on the ev:// scheme
    std::string base_uri = "ev://assets.example/markdown/loader.html";
    std::cout << "[INIT] Loading HTML with base URI: " << base_uri << std::endl;
    webkit_web_view_load_html(webview, loader_html.c_str(), base_uri.c_str());

    gtk_widget_show_all(window);
    gtk_main();

    return 0;
}