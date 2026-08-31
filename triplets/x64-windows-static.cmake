# Overlay triplet mirroring vcpkg's built-in x64-windows-static, but
# pinning the MSVC platform toolset to v143.
#
# See triplets/x86-windows-static.cmake for the rationale (toolset drift
# to VS 2026/v145 produces libs that don't link against the project's
# pinned v143, yielding ___std_* LNK2001s).
set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE static)
set(VCPKG_LIBRARY_LINKAGE static)
set(VCPKG_PROVIDED_FORTRAN ON)
set(VCPKG_PLATFORM_TOOLSET "v143")
