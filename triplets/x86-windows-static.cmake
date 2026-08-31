# Overlay triplet mirroring vcpkg's built-in x86-windows-static, but
# pinning the MSVC platform toolset to v143.
#
# vcpkg auto-detects the newest installed MSVC toolchain to build deps
# (it ignores the toolset activated by vcvarsall; see microsoft/vcpkg
# #39455). The runner has both VS 2022 (v143) and VS 2026 (v145), and
# the project pins v143. Without this pin, vcpkg builds catch2 with
# v145; catch2's objects reference v145-only STL-internal symbols
# (__std_find_*_trivial_pos, __std_regex_transform_primary_char) that
# are absent from v143's libcpmt, producing LNK2001s at link time.
#
# Keep this in lockstep with the built-in triplet (arch + static CRT +
# static linkage) so the overlay behaves identically except for the
# toolset.
set(VCPKG_TARGET_ARCHITECTURE x86)
set(VCPKG_CRT_LINKAGE static)
set(VCPKG_LIBRARY_LINKAGE static)
set(VCPKG_PROVIDED_FORTRAN ON)
VCPKG_PLATFORM_TOOLSET("v143")
