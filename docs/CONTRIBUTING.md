# Contributing to Nadir

Thanks for considering contributing to Nadir. This project is deliberately small and human-driven. Every line of code must be understood by its author. Please read this guide and the [README](../README.md) before opening a pull request.

## AI use policy

AI assistance is allowed, but it must stay **minimal**

Please stick to the following rules if you plan to use AI in your contributions:
- The majority of the code must be authored by a human. Use AI as a tutor or reviewer if you need help.
- Never paste large AI-generated chunks into the tree.
- You are responsible for every committed line.
- Everything must be documented. Committed code, human or AI-authored, must be well documented. Undocumented code WILL be regected.
- Attribution is not welcome, it's required. If a commit contains meaningful AI-assisted work, add a `Co-Authored-By:` trailer in the PR description.

## Ground rules

- Never commit generated artifacts: `*.o`, `*.a`, `*.elf`, `*.bin`, `*.iso`, `*.img`, `build/` (see `.gitignore`).
- Every change must still build and boot in QEMU.

## Code style

- C11 or C17, plus NASM/GAS assembly.
- Compile-clean discipline: `-Wall -Wextra -Werror`.
- Match the surrounding style.

## Documentation

- Every function, type, and file gets a doc comment explaining its purpose and, when relevant, the *why*.
- Every privileged primitive (GDT, IDT, paging, context switch) must document the Ring / CPL it operates in and its fault behavior.

## Building and running

Build from the repository root. See the [README](../README.md).

## License

By contributing, you agree that your contributions are licensed under the project's license. See [LICENSE](../LICENSE).