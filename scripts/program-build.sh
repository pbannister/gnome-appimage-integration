#!/bin/sh
#
# program-build.sh: configure and build the C++ readers and command-line tools.
# The build tree is generated output and lives in dataflow.out/build.
set -eu

DIRECTORY_SCRIPT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
REPOSITORY_ROOT=$(CDPATH= cd -- "$DIRECTORY_SCRIPT/.." && pwd)
DIRECTORY_SOURCE="$REPOSITORY_ROOT/sources"
DIRECTORY_BUILD="$REPOSITORY_ROOT/dataflow.out/build"
BUILD_TYPE=${BUILD_TYPE:-Release}

mkdir -p "$DIRECTORY_BUILD"
sh "$DIRECTORY_SCRIPT/version-generate.sh"
cmake -S "$DIRECTORY_SOURCE" -B "$DIRECTORY_BUILD" -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
cmake --build "$DIRECTORY_BUILD" --parallel
