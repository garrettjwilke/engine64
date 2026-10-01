#!/usr/bin/env bash

set -e

# The engine never builds on its own: it is always compiled from a consuming
# project, so what there is to build here are the examples.
#
#   ./build.sh          clean and build every example
#   ./build.sh clean    clean only
cd "$(dirname "$0")"

clean_only=0
[ "$1" = clean ] && clean_only=1

if [ -n "$N64_INST" ]; then
	N64_INST="${N64_INST/#~/$HOME}"
	export N64_INST
	export PATH="$N64_INST/bin:$PATH"
elif [ -d "$HOME/build/libdragon" ]; then
	export N64_INST="$HOME/build/libdragon"
	export PATH="$N64_INST/bin:$PATH"
elif [ -d "../libdragon" ]; then
	export N64_INST="$(cd ../libdragon && pwd)"
	export PATH="$N64_INST/bin:$PATH"
fi

if [ -n "$T3D_INST" ]; then
	T3D_INST="${T3D_INST/#~/$HOME}"
	export T3D_INST
elif [ -d "../tiny3d" ]; then
	export T3D_INST="$(cd ../tiny3d && pwd)"
elif [ -d "$HOME/build/tiny3d" ]; then
	export T3D_INST="$HOME/build/tiny3d"
fi

# The model importer is a host tool of tiny3d, built on its own and shipped as
# source: without it every .glb in an example fails to convert.
importer="$T3D_INST/tools/gltf_importer/gltf_to_t3d"
if [ "$clean_only" = 0 ] && [ -n "$T3D_INST" ] && [ ! -x "$importer" ]; then
	echo "Building the tiny3d model importer"
	make -C "$(dirname "$importer")" -j4
fi

for dir in examples/*/; do
	[ -f "$dir/Makefile" ] || continue

	echo "Cleaning $dir"
	make -C "$dir" clean

	[ "$clean_only" = 1 ] && continue

	echo "Building $dir"
	make -C "$dir" -j4
done

if [ "$clean_only" = 1 ]; then
	echo "Cleaning tools"
	make -C tools/model_importer clean 2>/dev/null || true
	make -C tools/collision_importer clean 2>/dev/null || true
	make -C tools/stage_importer clean 2>/dev/null || true
	echo "Clean done!"
else
	echo "Build done!"
fi
