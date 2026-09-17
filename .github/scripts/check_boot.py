"""QEMU boot check for Nadir CI.

Runs the boot sector headless on a pseudo-terminal with the curses display,
waits for the boot, then asserts the welcome message reached the screen.
A garbage image (SeaBIOS "No bootable device") fails this check.

Usage: python3 .github/scripts/check_boot.py   (from the repository root)
Requires: qemu-system-x86_64 on PATH. Stdlib only.
"""

import fcntl
import os
import pty
import select
import struct
import subprocess
import sys
import termios
import time

IMAGE = sys.argv[1] if len(sys.argv) > 1 else "os.img"
NEEDLE = b"64-bit"
BOOT_WAIT_S = 8


def main() -> int:
    master, slave = pty.openpty()
    fcntl.ioctl(slave, termios.TIOCSWINSZ, struct.pack("HHHH", 24, 80, 0, 0))
    env = dict(os.environ, TERM="xterm")
    proc = subprocess.Popen(
        ["qemu-system-x86_64", "-display", "curses", "-fda", IMAGE],
        stdin=slave,
        stdout=slave,
        stderr=subprocess.DEVNULL,
        env=env,
    )
    os.close(slave)
    time.sleep(BOOT_WAIT_S)
    proc.terminate()
    try:
        proc.wait(timeout=10)
    except subprocess.TimeoutExpired:
        proc.kill()
        proc.wait()
    fcntl.fcntl(master, fcntl.F_SETFL, os.O_NONBLOCK)
    data = b""
    while True:
        ready, _, _ = select.select([master], [], [], 1)
        if not ready:
            break
        try:
            chunk = os.read(master, 65536)
        except OSError:
            break
        if not chunk:
            break
        data += chunk
    os.close(master)
    if NEEDLE not in data:
        print(f"FAIL: {NEEDLE!r} not found on the QEMU screen")
        return 1
    print("OK: QEMU booted and printed the welcome message")
    return 0


if __name__ == "__main__":
    sys.exit(main())
