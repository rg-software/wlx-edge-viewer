#!/usr/bin/env bash
# Linux release packaging for EdgeViewer WLX plugin (Double Commander).
# Mirrors BuildMakeSetup.bat: builds Release, assembles plugin + assets + ini,
# produces Release-YYYYMMDD-Linux.zip next to this script.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

echo "Building Linux Release..."
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build -j

echo "Assembling package..."
rm -rf winbuild/Release
mkdir -p winbuild/Release
cp build/EdgeViewer.wlx64 winbuild/Release/
cp -r Resources/assets winbuild/Release/
cp Resources/edgeviewer.ini winbuild/Release/

echo "Creating zip..."
cd winbuild/Release
ZIP_NAME="Release-$(date +%Y%m%d)-Linux.zip"
zip -r "../../${ZIP_NAME}" .
cd ../..

echo "Done: ${ZIP_NAME}"