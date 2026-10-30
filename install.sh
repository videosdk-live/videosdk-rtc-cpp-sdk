#!/usr/bin/env sh
# Install the VideoSDK C++ SDK on Linux or macOS:
#   curl -fsSL https://raw.githubusercontent.com/videosdk-live/videosdk-rtc-cpp-sdk/main/install.sh | sudo sh
# Windows: download the .zip from the Releases page instead (see README.md).
# Env: VIDEOSDK_VERSION (default latest), PREFIX (default /usr/local), SKIP_DEPS=1
set -eu

REPO="videosdk-live/videosdk-rtc-cpp-sdk"
PREFIX="${PREFIX:-/usr/local}"
VERSION="${VIDEOSDK_VERSION:-latest}"
# VIDEOSDK_TARBALL installs a .tar.gz you already have instead of downloading a
# release. Everything else — dependencies, extract, ldconfig — runs the same.
#   sudo VIDEOSDK_TARBALL=./videosdk-cpp-v0.0.1-beta.6-linux-arm64.tar.gz sh install.sh
TARBALL="${VIDEOSDK_TARBALL:-}"
case "$(uname -s)" in
  Linux)  OS="linux" ;;
  Darwin) OS="macos" ;;
  *) echo "unsupported OS: $(uname -s). On Windows, download the .zip from" >&2
     echo "https://github.com/$REPO/releases" >&2
     exit 1 ;;
esac
case "$(uname -m)" in
  x86_64|amd64)  ARCH="x86_64" ;;
  aarch64|arm64) ARCH="arm64"  ;;
  *) echo "unsupported architecture: $(uname -m)" >&2; exit 1 ;;
esac
PLATFORM="${OS}-${ARCH}"

# The macOS package bundles its own SDL2. On an Intel Mac, /usr/local/lib is
# also Homebrew's, and extracting there would replace Homebrew's SDL2 link.
if [ "$OS" = "macos" ] && [ -L "$PREFIX/lib/libSDL2-2.0.0.dylib" ]; then
  echo "error: $PREFIX/lib/libSDL2-2.0.0.dylib is a link, most likely Homebrew's SDL2," >&2
  echo "       and installing here would replace it. Choose another prefix:" >&2
  echo "       curl -fsSL https://raw.githubusercontent.com/$REPO/main/install.sh | sudo PREFIX=/opt/videosdk sh" >&2
  exit 1
fi

if command -v curl >/dev/null 2>&1; then
  fetch() { curl -fsSL "$1"; }
  download() { curl -fsSL "$1" -o "$2"; }
elif command -v wget >/dev/null 2>&1; then
  fetch() { wget -qO - "$1"; }
  download() { wget -qO "$2" "$1"; }
else
  echo "curl or wget required" >&2; exit 1
fi

if [ -z "$TARBALL" ] && [ "$VERSION" = "latest" ]; then
  VERSION="$(fetch "https://api.github.com/repos/$REPO/releases/latest" \
    | grep '"tag_name"' | head -n1 | cut -d'"' -f4)"
  [ -n "$VERSION" ] || { echo "could not resolve latest release" >&2; exit 1; }
fi

# macOS needs nothing: SDL2 is bundled and the rest is part of the OS.
if [ "$OS" = "linux" ] && [ "${SKIP_DEPS:-0}" != "1" ] && command -v apt-get >/dev/null 2>&1; then
  apt-get update -qq || true
  apt-get install -y --no-install-recommends libpulse0 libsdl2-2.0-0 libglib2.0-0 libxtst6 || true
  # Only releases before v0.0.1-beta.6 link libjpeg dynamically. Its package
  # name differs between Ubuntu and Debian.
  apt-get install -y --no-install-recommends libjpeg-turbo8 2>/dev/null \
    || apt-get install -y --no-install-recommends libjpeg62-turbo 2>/dev/null \
    || true
fi

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

if [ -n "$TARBALL" ]; then
  [ -f "$TARBALL" ] || { echo "VIDEOSDK_TARBALL not found: $TARBALL" >&2; exit 1; }
  ASSET="$(basename "$TARBALL")"
  cp "$TARBALL" "$TMP/$ASSET"
  echo "using local tarball: $TARBALL"
else
  ASSET="videosdk-cpp-${VERSION}-${PLATFORM}.tar.gz"
  URL="https://github.com/$REPO/releases/download/${VERSION}/${ASSET}"
  download "$URL" "$TMP/$ASSET" || { echo "download failed: $URL" >&2; exit 1; }
fi

mkdir -p "$PREFIX"
tar -xzf "$TMP/$ASSET" -C "$PREFIX"
command -v ldconfig >/dev/null 2>&1 && ldconfig "$PREFIX/lib" 2>/dev/null || true

echo "VideoSDK C++ SDK ($ASSET) installed to $PREFIX"
