# Whetstone

<p align="center">
  <img src="Resources/logo.png" alt="Whetstone logo" width="128" />
</p>

On-device JIT enabler for **iOS 12 / 13, all devices (A7–A13)**. No PC needed
after install, no jailbreak installed.

Pick any installed app, tap one button, and it can run in debug mode —
which is what emulators and runtimes (Dolphin, PPSSPP, LinIniOS's
engine, …) need, and what stock iOS refuses them.

No full jailbreak: Whetstone uses an exploit normally used for
jailbreaking purely to grant itself the permission to flip apps into
debug mode. Only Whetstone ever holds elevated permissions; nothing else
is modified or installed.

Inspired by [DirtyJIT](https://github.com/haxi0/DirtyJIT) (no code shared —
see `THIRD_PARTY_NOTICES.md`). The exploit code is vendored from Amethyst
(MIT).

| | DirtyJIT (haxi0) | Whetstone |
|---|---|---|
| Works on | iOS 15.5–16.1.2 | iOS 12.x–13.x |
| Core bug | MacDirtyCow CVE-2022-46689 (file overwrite; does not exist on iOS 12) | Amethyst kernel exploits (hemlock cold-start, trigon fast path) |
| How JIT is granted | Replace `iPhoneDebug.pem` + mount a Developer Disk Image + attach debugserver | Patch the target's `proc` flags (`CS_DEBUGGED`, …) directly in the kernel |
| Needs a PC? | **Yes, every reboot** (mount the DDI with `ideviceimagemounter`) | **No** — only the one-time sideload |
| UI | SwiftUI (needs iOS 13+) | UIKit (runs on iOS 12) |

## One honest caveat

Getting *any* code onto stock iOS requires a signature, so the first install
needs a computer (AltStore/Sideloadly) or an on-device signer — that is
unavoidable and true of DirtyJIT too. The difference: once Whetstone is on
your device, enabling JIT never touches a PC again.

## Usage

Three screens, no typing:

1. **Tap to start** — one button.
2. **Spinner** — runs the exploit (Amethyst's hemlock/trigon + its
   verbatim self-jailbreak: uid0, sandbox escape, platformize) with a
   live stage line. A clean failure shows one line plus Back; a restart
   returns via relaunch routing.
3. **Pick an app to debug** — the list; tap to enable. Whetstone patches
   it and foregrounds it.

After a reboot or if Whetstone is killed, the exploit dies with the
process — relaunch and tap to start again, same as every
semi-untethered jailbreak. Nothing persists, nothing else is installed.

`whetstone://enable-jit?bundle-id=<bid>` URL scheme included, so client apps
can deep-link here with their bundle ID prefilled.

## If the iPad restarts on step 1

Expected on some devices (observed on A7 / iOS 12.5.8, where official
Amethyst restarts the same way): the puaf spray can destabilize the kernel
instead of failing cleanly. Whetstone notices the interrupted attempt on
relaunch, counts retries, and tells you when to reboot-and-retry versus just
re-tap. Two things keep a bad run from doing damage:

- Every kernel address is validated (proc↔task round-trip, mobile uid check)
  *before* anything is written. A mismatch aborts with "nothing was written"
  instead of risking a wild write.
- Only `proc` flag + uid/sandbox writes exist. There is no filesystem,
  trustcache, or code-patch stage to leave half-applied.

## Building

```sh
sh build.sh            # Whetstone.ipa for a real device
sh JITProbe/build.sh   # JITProbe.ipa for a real device
MIN_IOS=12.4 sh build.sh
```

Or open `Whetstone.xcodeproj` in Xcode: same sources, flags (12.2 target,
`-Wno-everything` on vendored C, IOKit/IOSurface, bridging header) and
bundle ID, with automatic signing — pick your team, pick the iPad, build.
See `docs/XCODE_INSTALL.md`. Note: Xcode 26 ships no iOS 12 Developer Disk
Image, so it can build and install but not *debug-run* on the iPad; install
via Window → Devices and Simulators.

No Xcode project and no Simulator target (this Mac has no iOS 12/13
runtimes, and the exploit can't run on macOS) — `build.sh` compiles the
vendored C exploit with clang and the UIKit app with `swiftc`, same approach
as LinIniOS's `app/build.sh`.

Installing without burning new sideload slots: `docs/XCODE_INSTALL.md`
(Xcode-fetched profiles for the existing bundle IDs + `install-device.sh`).

## Status

Early scaffold: app shell, icon, build, exploit driver wiring, and the
JIT flag surgery are in. Not yet tested on a real iOS 12 device — kernel
offsets and the flag set come straight from Amethyst's tested jailbreak path,
but expect a round or two of on-device debugging (success rate, PID lookup
edge cases, `task_for_pid` verification).

## Proving it: JITProbe

`JITProbe/` is a second, tiny app (`dev.whetstone.jitprobe`) built the same
way (`sh JITProbe/build.sh device`). It runs three checks in-process and
shows PASS/BLOCKED for each, plus its pid and `CS_DEBUGGED` state:

- `mmap` RWX, write a 2-instruction ARM64 function, call it
- `mmap` RW → `mprotect` RX, write, call
- `mmap` `MAP_JIT`, write, `mprotect` RX, call

The generated code is verified by execution (it must return `0x1234`), and
the probe logic itself is host-tested: run natively on Apple Silicon it
reports RWX blocked with `EACCES` while the other two pass, which is exactly
the shape a locked-down OS gives. On stock iOS 12 expect all three BLOCKED;
after a successful Whetstone run expect at least RW→RX to flip to PASS.

Test order on the iPad: sideload both IPAs, open JITProbe (leave it
backgrounded), patch `dev.whetstone.jitprobe` from Whetstone, come back —
JITProbe re-tests itself on foreground and should read JIT WORKS.

## Scope: Whetstone only, nothing else

This is a standard Amethyst-style self-jailbreak with everything past
`init_permissions` cut away: no patchfinder, no PPL bypass, no trustcache,
no remount, no daemons, no bootstrap, no package managers, no userspace
reboot. Whetstone jailbreaks only itself so it can patch a target's `proc`
flags directly, in-process — enabling JIT is its sole function. (An earlier
revision tried a persistent holder daemon that outlived the app across a
respring; it was scrapped in favor of this direct model.)

## Credits

- staturnz + Amethyst contributors — hemlock/trigon exploits, kernel offsets
  (MIT, vendored under `Exploit/`).
- haxi0 / DirtyJIT — the on-device JIT-enabler UX this reimagines for iOS 12.
- LinIniOS — the iOS 12 engine whose missing JIT path motivated this.

## License

MIT — see `LICENSE`. Vendored exploit code keeps its own MIT copyright
(`THIRD_PARTY_NOTICES.md`).
