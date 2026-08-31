# Overlay triplet mirroring vcpkg's built-in x64-windows-static, but
# pinning the MSVC platform toolset to v143.
#
# See triplets/x86-windows-static.cmake for the rationale (vcpkg
# auto-detects the newest MSVC, e.g. VS 2026/v145, which mismatches the
# project's pinned v143 and yields ___std_* LNK2001s when linking
# catch2).
set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE static)
set(VCPKG_LIBRARY_LINKAGE static)
set(VCPKG_PROVIDED_FORTRAN ON)
VCPKG_PLATFORM_TOOLSET("v143")
