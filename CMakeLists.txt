# Linux build for EdgeViewer WLX plugin (Double Commander).
# Section 4 tasks 4.4 + 4.5 of openspec/changes/port-to-double-commander-linux.
#
# Build:
#   cmake -B build -S .
#   cmake --build build -j
#
# Output: build/EdgeViewer.wlx.so  (the plugin shared object)
# Install:
#   cmake --install build --prefix ~/.local
#   # or copy manually to ~/.doublecmd/plugins/edgeviewer/

cmake_minimum_required(VERSION 3.16)
project(EdgeViewer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Decision 11: Linux uses CMake visibility (no .def file).
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Decision 8: find WebKitGTK 4.1 + GTK 3 via pkg-config. These are
# standard system packages; no vcpkg equivalent is needed on Linux.
find_package(PkgConfig REQUIRED)
pkg_check_modules(WEBKIT REQUIRED IMPORTED_TARGET webkit2gtk-4.1)
pkg_check_modules(GTK    REQUIRED IMPORTED_TARGET gtk+-3.0)

# Shared sources: processors, navigator, wlxdetect, base file
# processor, log, platform_linux — no Windows-specific includes.
file(GLOB EDGEVIEWER_SHARED_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/EdgeViewer/DllMain.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/EdgeViewer/Globals.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/EdgeViewer/Navigator.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/EdgeViewer/WlxDetect.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/EdgeViewer/ZoomHotkey.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/EdgeViewer/Log.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/EdgeViewer/IWebView.cpp"
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
    "${CMAKE_CURRENT_SOURCE_DIR}/EdgeViewer/WebView/WebKitBackend.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/EdgeViewer/WebView/WebViewFactory.cpp"
)

# Build the plugin shared library.
add_library(EdgeViewer MODULE ${EDGEVIEWER_SHARED_SOURCES})

target_include_directories(EdgeViewer PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/EdgeViewer"
    "${CMAKE_CURRENT_SOURCE_DIR}/mINI/src"
)

target_link_libraries(EdgeViewer PRIVATE
    PkgConfig::WEBKIT
    PkgConfig::GTK
)

# Manual export list — DC's WLX symbols. Avoids depending on a
# visibility pragma in every source file. The exported names match
# Double Commander's wlxplugin.h declarations verbatim (ListLoadW,
# ListLoadNextW, etc.).
set_target_properties(EdgeViewer PROPERTIES
    CXX_VISIBILITY_PRESET hidden
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

# Output naming. DC expects EdgeViewer.wlx.so (the Linux plugin
# shared object name).
set_target_properties(EdgeViewer PROPERTIES
    OUTPUT_NAME "EdgeViewer.wlx"
    PREFIX ""
)

# Install rules (task 4.5): lay out the plugin + Resources/ next to
# each other, mirroring the Windows installer layout.
include(GNUInstallDirs)
install(TARGETS EdgeViewer
    RUNTIME DESTINATION ${CMAKE_INSTALL_LIBDIR}/doublecmd/plugins/edgeviewer
)
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/Resources/assets"
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/doublecmd/plugins/edgeviewer/Resources
)
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/Resources/edgeviewer.ini"
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/doublecmd/plugins/edgeviewer/Resources
)
