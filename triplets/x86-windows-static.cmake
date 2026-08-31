# Overlay triplet mirroring vcpkg's built-in x86-windows-static, but
# pinning the MSVC platform toolset to v143.
#
# The GNU/VC project builds with the v143 toolset (VS 2022). The CI
# runners also have VS 2026 (v145), which vcpkg may otherwise select.
# Catching this build with a newer compiler produces libs that link
# against v143-only-linkage consumers with unresolved MSVC STL-internal
# symbols (__std_find_*_trivial_pos, __std_regex_transform_primary_char)
# — these exist only in newer STL/libcpmt builds, not v143's.
# Pinning VCPKG_PLATFORM_TOOLSET keeps every vcpkg dependency on the
# same toolset the project links, and changes the ABI hash so any stale
# newer-toolset artifact in the shared binary cache is rebuilt.
#
# Keep this in lockstep with the built-in triplet (arch + static CRT +
# static linkage) so the overlay behaves identically except for the
# toolset.
set(VCPKG_TARGET_ARCHITECTURE x86)
set(VCPKG_CRT_LINKAGE static)
set(VCPKG_LIBRARY_LINKAGE static)
set(VCPKG_PROVIDED_FORTRAN ON)
set(VCPKG_PLATFORM_TOOLSET "v143")
