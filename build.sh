#!/bin/sh
# Builds Whetstone.app for a real iPhone/iPad, as an .ipa.
#
# There is no Simulator target and no Xcode project: this Mac has no iOS
# 12/13 simulator runtimes, and the kernel exploit cannot run on macOS
# anyway, so the bundle is assembled straight with clang and swiftc, the
# same way LinIniOS builds its app.
#
#   MIN_IOS    lowest iOS the build runs on. Default 12.2.
#   IDENTITY   signing identity, from `security find-identity -v -p codesigning`
#   PROFILE    a .mobileprovision naming the device (without it the .ipa
#              still builds, for AltStore or Sideloadly to re-sign)
set -eu
here=$(cd "$(dirname "$0")" && pwd)
min=${MIN_IOS:-12.2}
sdk=iphoneos
target=arm64-apple-ios$min
build="$here/build-device"
app="$build/Whetstone.app"
mkdir -p "$build/obj"

# Kernel exploit + JIT-only driver (vendored Amethyst sources, MIT).
csrc="Exploit/whetstone_jit.c
    Exploit/hemlock/hemlock.c Exploit/hemlock/util.c
    Exploit/trigon/trigon.c Exploit/trigon/util.c Exploit/trigon/aop.c
    Exploit/jailbreak/memory.c Exploit/jailbreak/utils.c"

n=0
for f in $csrc; do
    o="$build/obj/c$n.o"
    # -fblocks: jailbreak/utils.c uses blocks. -Wno-* : third-party code we
    # vendor verbatim; keep our own Sources warning-clean instead.
    xcrun -sdk $sdk clang -target "$target" -O2 -fblocks \
        -I "$here/Exploit" -I "$here/Exploit/hemlock" \
        -I "$here/Exploit/trigon" -I "$here/Exploit/jailbreak" \
        -Wno-everything -c "$here/$f" -o "$o"
    n=$((n + 1))
done
cobjs=""
for i in $(seq 0 $((n - 1))); do cobjs="$cobjs $build/obj/c$i.o"; done

rm -rf "$app"
mkdir -p "$app"
cp "$here/Resources/Info.plist" "$app/Info.plist"
/usr/libexec/PlistBuddy -c "Set :MinimumOSVersion $min" "$app/Info.plist"

ibtool --errors --warnings --notices --output-format human-readable-text \
    --compile "$app/LaunchScreen.storyboardc" "$here/Resources/LaunchScreen.storyboard" \
    >/dev/null

cp "$here/Resources/icons/"Icon-*.png "$app/"

xcrun -sdk $sdk swiftc -target "$target" \
    -module-name Whetstone \
    -import-objc-header "$here/Sources/Bridging.h" \
    -I "$here/Exploit" \
    $cobjs \
    -framework Foundation -framework UIKit -framework CoreGraphics \
    -framework IOKit -framework IOSurface \
    -O -o "$app/Whetstone" \
    "$here/Sources/AppDelegate.swift" "$here/Sources/StartViewController.swift" \
    "$here/Sources/LoadingViewController.swift" "$here/Sources/AppsViewController.swift" \
    "$here/Sources/AppList.swift" "$here/Sources/Whetstone.swift"

if [ -n "${PROFILE:-}" ]; then
    cp "$PROFILE" "$app/embedded.mobileprovision"
fi

identity=${IDENTITY:-$(security find-identity -v -p codesigning \
    | awk '/Apple Develop/ {print $2; exit}')}
if [ -z "$identity" ]; then
    echo "no signing identity: set IDENTITY, or sign the .ipa with a sideloader" >&2
    identity=-
fi
codesign -s "$identity" --force --timestamp=none \
    --entitlements "$here/Resources/Whetstone.entitlements" "$app"

rm -rf "$build/Payload" "$build/Whetstone.ipa"
mkdir -p "$build/Payload"
cp -R "$app" "$build/Payload/"
(cd "$build" && zip -qry Whetstone.ipa Payload)
rm -rf "$build/Payload"
echo "built $build/Whetstone.ipa"
