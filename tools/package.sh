#!/bin/sh
# The module's release archive for Deadstation Portable's Recomp Store: build-brick/package/recomp-<id>-<version>.tar.gz (module.cfg,
# the executable stripped, store.json) and catalog.cfg, its entry for the store's list (the archive at URL, by default this
# repository's release of the version).
#   tools/package.sh [URL]   (TG5040_ROOT: the TrimUI toolchain)
cd "$(dirname "$0")/.." || exit 1
cfg() { sed -n "s/^$1 = //p" module.cfg | head -1; }
ID=$(cfg id); NAME=$(cfg name); TITLE=$(cfg title); VER=$(cfg version); EXEC=$(cfg exec); NEEDS=$(cfg needs); DISC=$(cfg disc)
[ -n "$ID" ] && [ -n "$EXEC" ] || { echo "package: module.cfg has no id or exec"; exit 2; }
[ -f "build-brick/$EXEC" ] || { echo "package: build-brick/$EXEC is not built"; exit 2; }
TC=${TG5040_ROOT:?set TG5040_ROOT to the TrimUI toolchain}
ARCHIVE=recomp-$ID-$VER.tar.gz
URL=${1:-https://github.com/tyrannotorus/emulator-deadstation-portable-recomp-soulcalibur-broken-destiny/releases/download/$VER}
OUT=build-brick/package
rm -rf "$OUT" && mkdir -p "$OUT/root" || exit 3
cp module.cfg store.json "$OUT/root/" || exit 3
"$TC/bin/aarch64-nextui-linux-gnu-strip" -o "$OUT/root/$EXEC" "build-brick/$EXEC" || { echo "package: strip failed"; exit 3; }
chmod +x "$OUT/root/$EXEC"
tar -C "$OUT/root" -czf "$OUT/$ARCHIVE" module.cfg "$EXEC" store.json || exit 3
SHA=$(sha256sum "$OUT/$ARCHIVE" | cut -d' ' -f1)
SIZE=$(stat -c %s "$OUT/$ARCHIVE")
INFO=$(python3 -I -c 'import json,sys; print(json.dumps(json.load(open(sys.argv[1])), separators=(",", ":")))' store.json) || exit 3
cat > "$OUT/catalog.cfg" <<EOM
store = 1
module = $ID
name = $NAME
title = $TITLE
version = $VER
url = $URL/$ARCHIVE
size = $SIZE
sha256 = $SHA
needs = $NEEDS
info = $INFO
disc = $DISC
EOM
echo "package: $OUT/$ARCHIVE ($SIZE bytes, sha256 $SHA)"
