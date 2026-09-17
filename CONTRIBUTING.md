# Contributing to Nadir

Thanks for considering contributing to Nadir. This project is deliberately small and human-driven. Every line of code must be understood by its author. Please read this guide and the [README](README.md) before opening a pull request.

## AI use policy

AI assistance is allowed, but it must stay **minimal**. Nadir is a craft project. The majority of the code must be written by a human.

Please stick to the following rules if you plan to use AI in your contributions:
- The majority of the code must be authored by a human. Use AI as a tutor, reviewer, or rubber duck if you need help. Do not as a bulk code generator.
- Never paste large AI-generated chunks into the tree. Small and reviewable changes only.
- **You are responsible for every committed line.** If you cannot explain an AI-suggested snippet, rewrite it until you can. Do not commit code you do not understand.
- **Everything must be documented.** Committed code, human or AI-authored, must be well documented. Undocumented code WILL be regected.
- **Attribution is not welcome, it's required.** If a commit contains meaningful AI-assisted work, add a `Co-Authored-By:` trailer or mention it in the PR description.

## Ground rules

- Bare-metal only: `-ffreestanding -fno-builtin`, no libc, no host headers, no syscalls, no `printf()`/`malloc()` from the host.
- Never commit generated artifacts: `*.o`, `*.a`, `*.elf`, `*.bin`, `*.iso`, `*.img`, `build/` (see `.gitignore`).
- One subsystem per file. Architecture-specific code never leaks into `kernel/core/`.
- Keep the kernel bootable. Every change must still build and boot in QEMU.

## Code style

- C11 or C17, plus NASM/GAS assembly. No K&R-style code.
- Compile-clean discipline: `-Wall -Wextra -Werror`.
- Match the surrounding style.

## Documentation

- Every function, type, and file gets a doc comment explaining its purpose and, when relevant, the *why*, not just the obvious *what*.
- Every privileged primitive (GDT, IDT, paging, context switch) must document the Ring / CPL it operates in and its fault behavior.

## Building and running

Build from the repository root. See the [README](README.md) for prerequisites and troubleshooting:

```
make        # builds nadir.img (boot sector + kernel)
make run    # boots it in QEMU
```

The toolchain is not yet finalized (candidates: `gcc` cross + `ld`, or `clang` + `lld` + `nasm`; target `x86-64-elf`).

## License

By contributing, you agree that your contributions are licensed under the project's license (MIT — see [LICENSE](LICENSE)).