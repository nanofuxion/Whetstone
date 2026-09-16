# Third-party notices

## Amethyst (MIT)

Copyright (c) 2026 staturnz

Vendored under `Exploit/hemlock/`, `Exploit/trigon/` and the
`jailbreak.h` / `memory.*` / `utils.*` / `handoff.h` files in
`Exploit/jailbreak/`, with two Whetstone-marked deviations (both commented
in place):

- `Exploit/hemlock/hemlock.c`: the kernel-task walk is capped and bails
  out instead of spinning forever when the early read primitive fails.
- `Exploit/whetstone_jit.c` (our own driver, not vendored): replicates
  Amethyst `init_device` offsets and the `init_permissions` write sequence
  for the self-jailbreak, stopping where Amethyst continues into
  patchfinder / remount / daemons / bootstrap.

> Permission is hereby granted, free of charge, to any person obtaining a copy
> of this software and associated documentation files (the "Software"), to deal
> in the Software without restriction, including without limitation the rights
> to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
> copies of the Software, and to permit persons to whom the Software is
> furnished to do so, subject to the following conditions:
>
> The above copyright notice and this permission notice shall be included in all
> copies or substantial portions of the Software.
>
> THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
> IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
> FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
> AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
> LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
> OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
> SOFTWARE.

Upstream: https://github.com/staturnzz/amethyst

## DirtyJIT (GPL-3.0) — inspiration only, no code reused

Whetstone's app-picker flow is inspired by https://github.com/haxi0/DirtyJIT
but contains none of its code, so its GPL-3.0 does not apply here.
