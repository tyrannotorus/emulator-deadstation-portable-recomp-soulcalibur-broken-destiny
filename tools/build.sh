#!/bin/sh
# The module for the Brick: tools/build.sh WORK   (WORK: sdk/tools/prepare-game.sh's output, never in git)
# TG5040_ROOT: the TrimUI toolchain. The Brick's Vulkan loader is linked: NK_BRICK_LIBS (a folder with libvulkan.so), else
# build-brick/brick-libs, pulled once from the Brick ANDROID_SERIAL names.
cd "$(dirname "$0")/.." || exit 1
WORK=${1:?usage: tools/build.sh WORK}
TC=${TG5040_ROOT:?set TG5040_ROOT to the TrimUI toolchain}
LIBS=${NK_BRICK_LIBS:-$PWD/build-brick/brick-libs}
mkdir -p "$LIBS" build-brick || exit 1
if [ ! -f "$LIBS/libvulkan.so" ]; then
	[ -n "$ANDROID_SERIAL" ] || { echo "build: no $LIBS/libvulkan.so: set NK_BRICK_LIBS, or ANDROID_SERIAL to pull it from that Brick"; exit 2; }
	adb -s "$ANDROID_SERIAL" pull /usr/lib/libvulkan.so.1 "$LIBS/libvulkan.so.1" > /dev/null || { echo "build: libvulkan.so not pulled"; exit 2; }
	ln -sf libvulkan.so.1 "$LIBS/libvulkan.so"
fi
[ -f sdk/nakagawa-recomp/tools/codegen.py ] || { echo "build: sdk/nakagawa-recomp is empty (git -C sdk submodule update --init)"; exit 2; }
sh sdk/tools/prepare-nakagawa.sh || exit 3
rm -f build-brick/Soulcalibur_Broken_Destiny_recomp_module	# a failed build leaves no executable
python3 -I tools/patch-gen.py "$WORK/gen" build-brick/gen || exit 3
# optional: WORK/hot-order.txt, the hot functions' link order (made from a profile; derived from the game, so never in git)
ORDER=""
[ -f "$WORK/hot-order.txt" ] && ORDER="$(realpath "$WORK/hot-order.txt")"
cmake -S . -B build-brick -G Ninja -DGEN="$PWD/build-brick/gen" -DCMAKE_TOOLCHAIN_FILE="$PWD/sdk/tools/toolchain/tg5040.cmake" \
	-DTG5040_ROOT="$TC" -DNK_BRICK_LIBS="$LIBS" -DNK_SYMBOL_ORDER="$ORDER" > build-brick/configure.log || { tail -20 build-brick/configure.log; exit 3; }
ninja -C build-brick > build-brick/build.log 2>&1 || { grep -E "error|undefined" build-brick/build.log | head -20; exit 3; }
ls -la build-brick/Soulcalibur_Broken_Destiny_recomp_module
