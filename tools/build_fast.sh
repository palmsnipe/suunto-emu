#!/bin/sh

# Opt-in local build profile for cold firmware iteration. The normal Makefile
# defaults remain portable -O2; this profile keeps its artifacts separate and
# leaves compiler/linker support for -flto to fail explicitly.
set -eu

build_dir=${SEMU_FAST_BUILD_DIR:-build-fast}
if [ "$#" -eq 0 ]; then
    set -- all
fi

# A fast SDL build is useful only when its headless snapshot producer is from
# the same source build.  Keep the convenient `sdl` spelling, but rebuild both
# frontends so the cached checkpoint cannot be produced by an older binary.
if [ "$#" -eq 1 ] && [ "$1" = sdl ]; then
    set -- all sdl
fi

exec make BUILD_DIR="$build_dir" CFLAGS='-O2 -flto' "$@"
