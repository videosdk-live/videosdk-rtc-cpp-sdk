#!/usr/bin/env bash
# Refresh this repo's headers and example from a checkout of the SDK monorepo,
# so they match the binaries a release ships.
#
#   bash scripts/sync-from-monorepo.sh ~/videosdk-rust-sdk
#
# Pulls only what the monorepo owns:
#
#   include/videosdk/**     the public headers
#   include/videosdk_ffi.h  the C ABI the headers above sit on
#   examples/main.cpp       the example
#
# Everything else here is owned by THIS repo and is never overwritten:
# README.md, examples/README.md, examples/CMakeLists.txt, install.sh, LICENSE.
#
# teleop.hpp is dropped: teleoperation is a separate product with its own
# library, and nothing in this SDK includes it.
#
# Changes files only; review the diff, then commit and push yourself.
set -euo pipefail

SRC="${1:?usage: sync-from-monorepo.sh <videosdk-rust-sdk checkout>}"
CPP="$SRC/platforms/cpp"
DEST="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

[ -d "$CPP/include/videosdk" ] \
  || { echo "error: $SRC does not look like the SDK monorepo (no platforms/cpp/include/videosdk)" >&2; exit 1; }

if [ -n "$(git -C "$DEST" status --porcelain --untracked-files=no)" ]; then
  git -C "$DEST" status --short --untracked-files=no >&2
  echo "error: commit or stash the changes above first — the result is reviewed as a diff" >&2
  exit 1
fi

# Delete tracked headers first, so one removed upstream disappears here too.
git -C "$DEST" ls-files -z -- include | (cd "$DEST" && xargs -0 rm -f)
find "$DEST/include" -type d -empty -delete 2>/dev/null || true

mkdir -p "$DEST/include" "$DEST/examples"
cp -R "$CPP/include/videosdk" "$DEST/include/"
cp "$CPP/include/videosdk_ffi.h" "$DEST/include/"
rm -f "$DEST/include/videosdk/teleop.hpp"
cp "$CPP/examples/main.cpp" "$DEST/examples/main.cpp"

# The copies describe the API without naming what is behind it. Fails loudly if
# a file gains wording the substitution table does not know about.
python3 "$DEST/scripts/scrub.py" "$DEST/include" "$DEST/examples"

echo "==> $DEST"
git -C "$DEST" status --short
echo "Review with 'git -C $DEST diff', then commit and push."
