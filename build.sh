#!/usr/bin/env bash

set -euo pipefail

print_usage() {
  cat <<'USAGE'
Usage:
  ./build.sh [debug|release|all]

Examples:
  ./build.sh debug
  ./build.sh release
  ./build.sh all

Default:
  ./build.sh debug
USAGE
}

build_preset() {
  local preset="$1"

  echo "==> Configuring ${preset}"
  cmake --preset "${preset}"

  echo "==> Building ${preset}"
  cmake --build --preset "${preset}"
}

build_mode="${1:-debug}"

case "${build_mode}" in
  debug)
    build_preset debug
    ;;
  release)
    build_preset release
    ;;
  all)
    build_preset debug
    build_preset release
    ;;
  -h|--help|help)
    print_usage
    ;;
  *)
    echo "Unknown build mode: ${build_mode}" >&2
    print_usage >&2
    exit 2
    ;;
esac
