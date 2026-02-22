#!/bin/bash

clear

SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &> /dev/null && pwd)

CONFIG=${1:-ALL}

if [ -z "$VCPKG_ROOT" ]; then
	echo "[WARN] Setup environment variable: VCPKG_ROOT"
	exit 1
fi

if [[ "$OSTYPE" == "msys" || "$OSTYPE" == "cygwin" ]]; then
	VCPKG_CMAKE_PATH=$(cygpath -m "$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake")
	FLAGS="-DVCPKG_TARGET_TRIPLET=x64-windows-static -DZLIB_USE_STATIC_LIBS=ON"
else
	VCPKG_CMAKE_PATH="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
	FLAGS="-DZLIB_USE_STATIC_LIBS=ON"
fi

pushd $SCRIPT_DIR > /dev/null

mkdir -p build
cd build

cmake .. -DCMAKE_TOOLCHAIN_FILE="$VCPKG_CMAKE_PATH" $FLAGS

if [ $? -ne 0 ]; then
	echo "CMake configuration failed"
	exit 1
fi

if [ "$CONFIG" = "ALL" ]; then
	cmake --build . --config Debug
	cmake --build . --config Release
else
	cmake --build . --config "$CONFIG"
fi

popd > /dev/null