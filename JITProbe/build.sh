#!/bin/sh
# Builds JITProbe.app for a real iPhone/iPad, as an .ipa.
#
# No Simulator target: this Mac has no iOS 12/13 simulator runtimes. Same
# no-Xcode-project approach as ../build.sh.
#
#   MIN_IOS / IDENTITY / PROFILE as in ../build.sh.
set -eu
here=$(cd "$(dirname "$0")" && pwd)
min=${MIN_IOS:-12.2}
sdk=iphoneos
target=arm64-apple-ios$min
build="$here/build-device"
app="$build/JITProbe.app"
mkdir -p "$build/obj"

xcrun -sdk $sdk clang -target "$target" -O2 \
    -Wno-everything -c "$here/Sources/jitprobe.c" -o "$build/obj/jitprobe.o"

rm -rf "$app"
mkdir -p "$app"
cp "$here/Resources/Info.plist" "$app/Info.plist"
/usr/libexec/PlistBuddy -c "Set :MinimumOSVersion $min" "$app/Info.plist"

ibtool --errors --warnings --notices --output-format human-readable-text \
    --compile "$app/LaunchScreen.storyboardc" "$here/Resources/LaunchScreen.storyboard" \
    >/dev/null

cp "$here/Resources/icons/"Icon-*.png "$app/"

xcrun -sdk $sdk swiftc -target "$target" \
    -module-name JITProbe \
    -import-objc-header "$here/Sources/Bridging.h" \
    "$build/obj/jitprobe.o" \
    -framework Foundation -framework UIKit -framework CoreGraphics \
    -O -o "$app/JITProbe" \
    "$here/Sources/AppDelegate.swift" "$here/Sources/ProbeViewController.swift"

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
    --entitlements "$here/Resources/JITProbe.entitlements" "$app"

rm -rf "$build/Payload" "$build/JITProbe.ipa"
mkdir -p "$build/Payload"
cp -R "$app" "$build/Payload/"
(cd "$build" && zip -qry JITProbe.ipa Payload)
rm -rf "$build/Payload"
echo "built $build/JITProbe.ipa"
