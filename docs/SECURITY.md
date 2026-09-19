# Security Policy

## Supported versions

Nadir is a hobby x86-64 kernel. Only the tip of `main` is supported.

| Version | Supported          |
| ------- | ------------------ |
| `main`  | :white_check_mark: |
| Anything else | :x: |

If you report an issue against an old commit, please verify it still reproduces on current `main` first (`make` then `make run` in QEMU).

## Reporting a vulnerability

Use a [private security advisory](https://github.com/Plexi09/Nadir-Kernel/security/advisories/new)
This keeps the details private until a fix is ready.

Please include:

1. What you think is wrong and why it matters
2. Steps to reproduce and serial/VGA output or a `-d int -no-reboot` log.

## Out of scope

- QEMU, SeaBIOS, firmware, host toolchain and host OS
- Denial of service by crashing
- Missing hardening as such
