#!/bin/sh
# Installs the device-built Whetstone.app onto the attached iPad.
# The .ipa must carry a provisioning profile for the device (free Apple IDs
# work; reuse the existing bundle ID so no new App ID slot is burned):
#   PROFILE="$HOME/Library/MobileDevice/Provisioning Profiles/<uuid>.mobileprovision" sh build.sh
#   sh install-device.sh [UDID]
# See docs/XCODE_INSTALL.md for getting the profile via Xcode.
set -eu
here=$(cd "$(dirname "$0")" && pwd)
ipa="$here/build-device/Whetstone.ipa"
[ -f "$ipa" ] || { echo "no ipa: build first (PROFILE=... sh build.sh)" >&2; exit 1; }
tmp="$here/build-device/install-tmp"
rm -rf "$tmp" && mkdir -p "$tmp"
if ! unzip -qo "$ipa" -d "$tmp" 'Payload/Whetstone.app/embedded.mobileprovision' 2>/dev/null; then
    echo "no embedded profile in ipa: rebuild with PROFILE=... (see docs/XCODE_INSTALL.md)" >&2
    exit 1
fi
rm -rf "$tmp"
if [ -n "${1:-}" ]; then udid="$1";
elif [ -n "${UDID:-}" ]; then udid="$UDID";
else udid="$(idevice_id -l 2>/dev/null | head -n 1)"; fi
[ -n "$udid" ] || { echo "no device found (plug in + trust this computer)" >&2; exit 1; }
ideviceinstaller -u "$udid" install "$ipa"
echo "installed; trust it in Settings > General > Device Management if asked"
