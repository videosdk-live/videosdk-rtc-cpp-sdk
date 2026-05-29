#!/usr/bin/env sh
# curl -fsSL https://raw.githubusercontent.com/videosdk-live/videosdk-rtc-cpp-sdk/main/install.sh | sudo sh
# Env: VIDEOSDK_VERSION (default latest), PREFIX (default /usr/local), SKIP_DEPS=1
set -eu

REPO="videosdk-live/videosdk-rtc-cpp-sdk"
PREFIX="${PREFIX:-/usr/local}"
VERSION="${VIDEOSDK_VERSION:-latest}"

case "$(uname -m)" in
  x86_64|amd64)        ARCH="linux-x86_64" ;;
  aarch64|arm64)       ARCH="linux-arm64"  ;;
  armv7l|armv7|armhf)  ARCH="linux-armv7"  ;;
  *) echo "unsupported architecture: $(uname -m)" >&2; exit 1 ;;
esac

if command -v curl >/dev/null 2>&1; then
  fetch() { curl -fsSL "$1"; }
  download() { curl -fsSL "$1" -o "$2"; }
elif command -v wget >/dev/null 2>&1; then
  fetch() { wget -qO - "$1"; }
  download() { wget -qO "$2" "$1"; }
else
  echo "curl or wget required" >&2; exit 1
fi

if [ "$VERSION" = "latest" ]; then
  VERSION="$(fetch "https://api.github.com/repos/$REPO/releases/latest" \
    | grep '"tag_name"' | head -n1 | cut -d'"' -f4)"
  [ -n "$VERSION" ] || { echo "could not resolve latest release" >&2; exit 1; }
fi

ASSET="videosdk-cpp-${VERSION}-${ARCH}.tar.gz"
URL="https://github.com/$REPO/releases/download/${VERSION}/${ASSET}"

if [ "${SKIP_DEPS:-0}" != "1" ] && command -v apt-get >/dev/null 2>&1; then
  apt-get update -qq || true
  apt-get install -y --no-install-recommends libpulse0 libsdl2-2.0-0 libjpeg-turbo8 \
    || apt-get install -y --no-install-recommends libpulse0 libsdl2-2.0-0 libjpeg62-turbo \
    || true
fi

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
download "$URL" "$TMP/$ASSET" || { echo "download failed: $URL" >&2; exit 1; }

mkdir -p "$PREFIX"
tar -xzf "$TMP/$ASSET" -C "$PREFIX"
command -v ldconfig >/dev/null 2>&1 && ldconfig "$PREFIX/lib" 2>/dev/null || true

echo "VideoSDK C++ SDK ${VERSION} installed to $PREFIX"
