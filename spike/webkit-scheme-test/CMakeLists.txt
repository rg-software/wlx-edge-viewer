cmake_minimum_required(VERSION 3.20)
project(webkit_scheme_test CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(WEBKIT REQUIRED webkit2gtk-4.1)
pkg_check_modules(GTK REQUIRED gtk+-3.0)

add_executable(webkit_scheme_test main.cpp)

target_include_directories(webkit_scheme_test PRIVATE
    ${WEBKIT_INCLUDE_DIRS}
    ${GTK_INCLUDE_DIRS}
)

target_link_directories(webkit_scheme_test PRIVATE
    ${WEBKIT_LIBRARY_DIRS}
    ${GTK_LIBRARY_DIRS}
)

target_link_libraries(webkit_scheme_test PRIVATE
    ${WEBKIT_LIBRARIES}
    ${GTK_LIBRARIES}
)