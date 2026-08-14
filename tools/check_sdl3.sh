#!/bin/sh
set -eu

mode=${1:-required}
pkg_config=${PKG_CONFIG:-pkg-config}

case "$mode" in
    required|probe) ;;
    *) echo "error: check_sdl3.sh expects required or probe" >&2; exit 2 ;;
esac

case "$pkg_config" in
    */*) available=$([ -x "$pkg_config" ] && echo yes || echo no) ;;
    *) available=$(command -v "$pkg_config" >/dev/null 2>&1 && echo yes || echo no) ;;
esac

if [ "$available" = no ]; then
    if [ "$mode" = probe ]; then
        echo "SKIP SDL3 check: pkg-config is unavailable ($pkg_config)" >&2
        exit 1
    fi
    echo "error: SDL3 build requires pkg-config; command unavailable: $pkg_config" >&2
    exit 2
fi

if ! "$pkg_config" --exists sdl3; then
    if [ "$mode" = probe ]; then
        echo "SKIP SDL3 check: pkg-config cannot find sdl3" >&2
        exit 1
    fi
    echo "error: SDL3 development files not found; pkg-config --exists sdl3 failed" >&2
    exit 2
fi

if [ "$mode" = probe ]; then
    echo "SDL3 check: available"
fi
