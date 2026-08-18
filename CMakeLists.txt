# Linux build for EdgeViewer WLX plugin (Double Commander).
# Section 4 tasks 4.4 + 4.5 of openspec/changes/port-to-double-commander-linux.
#
# Build:
#   cmake -B build -S .
#   cmake --build build -j
#
# Output: build/EdgeViewer.wlx64  (the plugin shared object)
# Install:
#   cmake --install build --prefix ~/.local
#   # or copy manually to ~/.doublecmd/plugins/edgeviewer/

cmake_minimum_required(VERSION 3.16)
project(EdgeViewer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Optional: emit verbose stderr debug logging from
# EdgeLister_Linux.cpp (the Ctrl+Q vs F3 detection log). Defaults
# off so production builds are silent. Enable with
# -DEDGEVIEWER_LINUX_DEBUG_LOGGING=ON on the cmake line.
option(EDGEVIEWER_LINUX_DEBUG_LOGGING
       "Emit verbose Ctrl+Q / Wayland embedding debug log to stderr"
       OFF)
if(EDGEVIEWER_LINUX_DEBUG_LOGGING)
    add_compile_definitions(EDGEVIEWER_LINUX_DEBUG=1)
endif()

# Decision 11: Linux uses a linker version script (no .def file). The
# version script's `local: *` hides every symbol not on the export list,
# so we must NOT compile with -fvisibility=hidden — hidden-visibility
# symbols stay hidden even when listed in a version script (GNU ld).
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# The Linux backend embeds a QWebEngineView (Qt Web Engine). Double
# Commander ships official builds for GTK2, Qt5 and Qt6; the Qt builds
# pass a QWidget* as the lister parent, which a QWebEngineView can be
# parented to. (The GTK2 build cannot host a GTK3/WebKitGTK widget, and
# WebKitGTK widgets cannot be embedded into a Qt widget tree.)
find_package(Qt6 6.4 REQUIRED COMPONENTS WebEngineWidgets Widgets)

# Shared sources: processors, navigator, wlxdetect, base file
# processor, platform_linux — no Windows-specific includes. The
# Windows-only TUs (Log.cpp, ZoomHotkey.cpp, DirProcessor_Win.cpp,
# WebView2Backend.cpp, EdgeLister_Win.cpp) are intentionally excluded;
# they are compiled by EdgeViewer.vcxproj on Windows only.
file(GLOB EDGEVIEWER_SHARED_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/EdgeViewer/DllMain.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/EdgeViewer/Globals.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/EdgeViewer/Navigator.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/EdgeViewer/WlxDetect.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/EdgeViewer/Platform_Linux.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/EdgeViewer/EdgeLister_Linux.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/EdgeViewer/Processors/AdocProcessor.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/EdgeViewer/Processors/BaseFileProcessor.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/EdgeViewer/Processors/DirProcessor.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/EdgeViewer/Processors/EmProcessor.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/EdgeViewer/Processors/HtmlProcessor.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/EdgeViewer/Processors/ImgProcessor.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/EdgeViewer/Processors/MdProcessor.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/EdgeViewer/Processors/MhtProcessor.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/EdgeViewer/Processors/OtherProcessor.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/EdgeViewer/Processors/ProcessorInterface.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/EdgeViewer/Processors/ProcessorRegistry.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/EdgeViewer/Processors/RstProcessor.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/EdgeViewer/Processors/UrlProcessor.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/EdgeViewer/WebView/QtWebEngineBackend.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/EdgeViewer/WebView/BrowserPool.cpp"
)

# Build the plugin shared library.
add_library(EdgeViewer MODULE ${EDGEVIEWER_SHARED_SOURCES})

target_include_directories(EdgeViewer PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/EdgeViewer"
    "${CMAKE_CURRENT_SOURCE_DIR}/mINI/src"
)

target_link_libraries(EdgeViewer PRIVATE
    Qt6::WebEngineWidgets
    Qt6::Widgets
)

# Manual export list — DC's WLX symbols. Avoids depending on a
# visibility pragma in every source file. The exported names match
# Double Commander's wlxplugin.h declarations verbatim (ListLoadW,
# ListLoadNextW, etc.).
set_target_properties(EdgeViewer PROPERTIES
    POSITION_INDEPENDENT_CODE ON
)

set(EDGEVIEWER_EXPORTS
    ListLoadW
    ListLoad
    ListLoadNextW
    ListLoadNext
    ListCloseWindow
    ListGetDetectString
    ListSearchTextW
    ListSearchText
    ListPrintW
    ListPrint
    ListSetDefaultParams
    ListSendCommand
)
string(REPLACE ";" "; " EDGEVIEWER_EXPORTS_VERBATIM "${EDGEVIEWER_EXPORTS}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/EdgeViewer.version"
     "{ global: ${EDGEVIEWER_EXPORTS_VERBATIM}; local: *; };")

set_target_properties(EdgeViewer PROPERTIES
    LINK_FLAGS "-Wl,--version-script=${CMAKE_CURRENT_BINARY_DIR}/EdgeViewer.version"
)

# Output naming. DC expects the x64-style `EdgeViewer.wlx64` suffix on
# Linux too (matching the 64-bit Windows deployment name).
set_target_properties(EdgeViewer PROPERTIES
    OUTPUT_NAME "EdgeViewer"
    PREFIX ""
    SUFFIX ".wlx64"
)

# Install rules (task 4.5): lay the plugin and its resources out the
# way the code resolves them. assetsPath()/edgeviewer.ini live directly
# in the plugin directory (GetModulePath()/assets), mirroring the
# Windows package layout (BuildMakeSetup.bat robocopy flattens
# Resources\ contents next to the DLL). Use LIBRARY DESTINATION (not
# RUNTIME) because EdgeViewer is a MODULE library (a shared library
# loaded by another process — DC) — RUNTIME DESTINATION is for
# executables only. Both land under the same <prefix>/share/doublecmd
# tree.
include(GNUInstallDirs)
install(TARGETS EdgeViewer
    LIBRARY DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/doublecmd/plugins/edgeviewer
)
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/Resources/assets"
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/doublecmd/plugins/edgeviewer
)
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/Resources/edgeviewer.ini"
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/doublecmd/plugins/edgeviewer
)
