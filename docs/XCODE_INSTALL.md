# Installing via Xcode (no new sideload slots)

Sideloadly and Xcode draw from the same free-Apple-ID pool, and the pool's
scarce resource is *new App ID registrations*. Reinstalling the existing
bundle IDs (`dev.whetstone.app`, `dev.whetstone.jitprobe`) consumes nothing
new. Xcode's real advantages: no IPA re-sign dance on every build, and live
console plus device logs while tapping Run exploit.

## Whetstone: just open the project

`Whetstone.xcodeproj` is checked in and mirrors `build.sh` (same files,
12.2 target, bridging header, IOKit/IOSurface). It holds two targets —
`Whetstone` (`dev.whetstone.app`) and `JITProbe`
(`dev.whetstone.jitprobe`, automatic signing fetches its profile on first
build with `-allowProvisioningUpdates`). Open it, select your team in
Signing & Capabilities if it isn't already set, pick the iPad as
destination, ⌘B. Install the built app via Window → Devices and
Simulators → Installed Apps (drag the `.app` from Products).

Caveat found while verifying: Xcode 26 ships no iOS 12 Developer Disk
Image, so Xcode cannot *debug-run* on the iPad ("Could not locate Developer
Disk Image"). Build + install works; debugging doesn't. For logs use
`idevicesyslog` or the Devices window's View Device Logs.

## One-time: fetch profiles for shell-script builds (optional)

If you'd rather keep using `build.sh` + `install-device.sh`, fetch a profile
once per bundle ID via a throwaway Xcode project (any template) with that
bundle ID, run destination = the iPad, Product → Build, then delete the temp
app from the iPad to free the slot:

       grep -l "dev.whetstone.app" \
         ~/Library/MobileDevice/Provisioning\ Profiles/*.mobileprovision

Free profiles expire (~7 days). When installs start failing, repeat step 2 —
the bundle IDs persist, so still nothing new is consumed.

## Build + install

    PROFILE="$HOME/Library/MobileDevice/Provisioning Profiles/<uuid>.mobileprovision" sh build.sh
    sh install-device.sh            # uses the attached device
    sh install-device.sh <UDID>     # or pick one explicitly
    UDID=<UDID> sh install-device.sh

Or drag `build-device/Whetstone.app` onto the iPad in Xcode → Window →
Devices and Simulators → Installed Apps.

After installing, trust it in Settings → General → Device Management if
asked (same as any sideload).

## Migrating off Sideloadly copies

Sideloadly mangles bundle IDs (it installed ours as
`…dev.whetstone.app.VD857MV9ME`), so a clean Xcode install is rejected as
an "upgrade" with `MismatchedApplicationIdentifierEntitlement`. Uninstall
the Sideloadly copy first (this also frees one of the 3 app slots), then
install — app data (attempt counters, caches) resets, which is harmless.

## Fully scripted path (proven)

With the iPad attached, this builds signed (fetching the profile
automatically on first run), packages, and installs with no GUI and no
Sideloadly:

    xcodebuild -project Whetstone.xcodeproj -scheme Whetstone -sdk iphoneos \
      -configuration Debug -allowProvisioningUpdates build
    APP=$(ls -d ~/Library/Developer/Xcode/DerivedData/Whetstone-*/Build/Products/Debug-iphoneos/Whetstone.app)
    rm -rf build-xcode && mkdir -p build-xcode/Payload
    cp -R "$APP" build-xcode/Payload/ && (cd build-xcode && zip -qry Whetstone.ipa Payload && rm -rf Payload)
    ideviceinstaller -u <UDID> install build-xcode/Whetstone.ipa

## Live logs while testing the exploit

- Xcode console, if launched via Run.
- Otherwise: `idevicesyslog | grep -i -E "whetstone|SpringBoard|backboardd"`.
- Crash/panic logs: Xcode Devices window → View Device Logs, or
  `idevicecrashreport -u <UDID> -k <dir>` (`-k` keeps copies on the device).
  A new `panic-full-*.ips` per attempt means true kernel panics; none means
  the restart is userspace-only, which is a different bug class — report
  which one you see.
