// Spike 1: WebKitGTK scheme handler + loader HTML base URI validation.
//
// Tests three assumptions from design.md Decision 3 and Decision 9:
//  1. Registering "http" as a custom URI scheme lets us dispatch by host
//     (assets.example -> plugin assets, local.example -> file root).
//  2. webkit_web_view_load_html(html, "http://assets.example/markdown/loader.html")
//     as base URI makes absolute http://assets.example/... and
//     http://local.example/... URLs in the loaded HTML reach our handler.
//  3. External HTTPS navigation (https://example.com/) is NOT intercepted
//     by the custom "http" scheme handler — only "http" requests are.
//
// Build on Linux:
//   cmake -B build spike/webkit-scheme-test
//   cmake --build build
//
// Run from the repo root (so Resources/assets/ is found):
//   ./build/webkit_scheme_test
//
// Expected output:
//   - A GTK window opens showing rendered Markdown (marked.js + highlight.js)
//   - Console logs each scheme request: [SCHEME] host=... path=...
//   - Click "Test External URL" button -> loads https://example.com/
//     (should NOT trigger our scheme handler; only HTTPS request shows in logs if any)

#include <gtk/gtk.h>
#include <webkit2/webkit2.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <iostream>

namespace fs = std::filesystem;

// --- Configuration ------------------------------------------------------
// Adjust these if running from a different working directory.
static const char* ASSETS_DIR = "Resources/assets";
static const char* LOCAL_ROOT = "Examples";  // serves as local.example root

// --- Scheme handler -----------------------------------------------------

static void
scheme_callback(WebKitURISchemeRequest* request, gpointer user_data)
{
    const gchar* uri = webkit_uri_scheme_request_get_uri(request);
    std::string uri_str(uri);

    // Parse host from URI: "http://HOST/PATH"
    std::string host;
    auto scheme_end = uri_str.find("://");
    if (scheme_end != std::string::npos) {
        auto host_start = scheme_end + 3;
        auto host_end = uri_str.find('/', host_start);
        host = (host_end != std::string::npos)
            ? uri_str.substr(host_start, host_end - host_start)
            : uri_str.substr(host_start);
    }

    // Parse path after host
    auto path_start = uri_str.find('/', scheme_end + 3);
    std::string path = (path_start != std::string::npos)
        ? uri_str.substr(path_start + 1)  // skip leading /
        : "";

    std::cout << "[SCHEME] host=" << host << " path=" << path << std::endl;

    // Dispatch by host
    std::string file_path;
    if (host == "assets.example") {
        file_path = std::string(ASSETS_DIR) + "/" + path;
    } else if (host == "local.example") {
        file_path = std::string(LOCAL_ROOT) + "/" + path;
    } else {
        // Unknown host — return 404
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

    // Read the file
    std::ifstream file(file_path, std::ios::binary);
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();

    // Guess content type from extension
    std::string mime_type = "application/octet-stream";
    auto ext = fs::path(file_path).extension().string();
    if (ext == ".html") mime_type = "text/html; charset=utf-8";
    else if (ext == ".js") mime_type = "application/javascript; charset=utf-8";
    else if (ext == ".css") mime_type = "text/css; charset=utf-8";
    else if (ext == ".png") mime_type = "image/png";
    else if (ext == ".svg") mime_type = "image/svg+xml";
    else if (ext == ".json") mime_type = "application/json; charset=utf-8";
    else if (ext == ".md") mime_type = "text/markdown; charset=utf-8";

    // Create a GInputStream from the file content
    GBytes* bytes = g_bytes_new(content.data(), content.size());
    GInputStream* stream = g_memory_input_stream_new_from_bytes(bytes);

    webkit_uri_scheme_request_finish(request, stream, content.size(),
        mime_type.c_str());

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

// --- Button: test external URL ------------------------------------------

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

    // Verify assets exist
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

    // Create a window
    GtkWidget* window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "WebKitGTK Scheme Spike");
    gtk_window_set_default_size(GTK_WINDOW(window), 900, 700);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), nullptr);

    // Vertical box: webview + button row
    GtkWidget* vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(window), vbox);

    // Register the "http" custom URI scheme on the default web context
    WebKitWebContext* context = webkit_web_context_get_default();
    webkit_web_context_register_uri_scheme(
        context,
        "http",
        scheme_callback,
        nullptr,   // user_data
        nullptr    // destroy_notify
    );

    std::cout << "[INIT] Registered 'http' scheme handler" << std::endl;

    // Create the web view
    WebKitWebView* webview = WEBKIT_WEB_VIEW(webkit_web_view_new());
    gtk_widget_set_vexpand(GTK_WIDGET(webview), TRUE);
    gtk_container_add(GTK_CONTAINER(vbox), GTK_WIDGET(webview));

    // Add a button row
    GtkWidget* hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_container_set_border_width(GTK_CONTAINER(hbox), 6);
    gtk_container_add(GTK_CONTAINER(vbox), hbox);

    GtkWidget* btn_external = gtk_button_new_with_label("Test External URL");
    g_signal_connect(btn_external, "clicked", G_CALLBACK(on_test_external_url), webview);
    gtk_container_add(GTK_CONTAINER(hbox), btn_external);

    // Read the loader HTML
    std::string loader_html = read_file(loader_path);

    // Replace __MD_FILENAME__ with a sample file from Examples/
    std::string sample_md = "test.md";
    // Find a .md file in Examples/
    if (fs::exists("Examples")) {
        for (const auto& entry : fs::directory_iterator("Examples")) {
            if (entry.path().extension() == ".md") {
                sample_md = entry.path().filename().string();
                break;
            }
        }
    }
    std::cout << "[INIT] Using sample file: " << sample_md << std::endl;

    // Replace placeholders (simplified — just MD_FILENAME)
    // The actual loader.html uses __MD_FILENAME__, __BASE_URL__, __CSS_NAME__ etc.
    // For the spike we do a simple string replace.
    auto replace_all = [](std::string& str, const std::string& from, const std::string& to) {
        size_t pos = 0;
        while ((pos = str.find(from, pos)) != std::string::npos) {
            str.replace(pos, from.length(), to);
            pos += to.length();
        }
    };

    // Placeholder replacements matching MdProcessor.cpp
    replace_all(loader_html, "__BASE_URL__", "");   // parent dir relative path (empty for root)
    replace_all(loader_html, "__CSS_NAME__", "github.css");
    replace_all(loader_html, "__MD_FILENAME__", "test.md");

    // CRITICAL: load_html with base URI on assets.example
    // This is the core of Decision 9 — the base URI makes relative
    // references in the loader resolve within the assets.example host.
    std::string base_uri = "http://assets.example/markdown/loader.html";
    std::cout << "[INIT] Loading HTML with base URI: " << base_uri << std::endl;
    webkit_web_view_load_html(webview, loader_html.c_str(), base_uri.c_str());

    // Show everything
    gtk_widget_show_all(window);
    gtk_main();

    return 0;
}