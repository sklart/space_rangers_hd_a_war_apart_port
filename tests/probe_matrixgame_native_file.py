"""Run real CFile API against controlled disk and valid PKG fixtures."""
import argparse
import os
import sys
from pathlib import Path
import struct
import subprocess
import zlib


def fixtures(output):
    output.mkdir(parents=True, exist_ok=True)
    disk = output / "disk"
    disk.mkdir(exist_ok=True)
    data = bytes((i * 17) % 251 for i in range(1100003))
    payload = bytearray()
    for offset in range(0, len(data), 65536):
        block = data[offset:offset + 65536]
        packed = b"ZL02" + struct.pack("<I", len(block)) + zlib.compress(block, 0)
        payload += struct.pack("<I", len(packed)) + packed
    record = struct.pack("<II63s63siiIIII", len(payload), len(data), b"MUSIC", b"MUSIC", 2, 2, 0, 0, 174, 0)
    package = output / "fixture.pkg"
    image = struct.pack("<IIII", 4, 170, 1, 158) + record + bytes(4) + payload
    package.write_bytes(image)
    corrupt = bytearray(image)
    corrupt[-1] ^= 0xff
    (output / "corrupt.pkg").write_bytes(corrupt)
    return disk, package


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--runner-binary", type=Path, help="Target ELF when --binary is qemu-aarch64")
    parser.add_argument("--qemu-sysroot", type=Path)
    parser.add_argument("--package", type=Path)
    parser.add_argument("--entry")
    args = parser.parse_args()
    if bool(args.package) != bool(args.entry):
        parser.error("--package and --entry must be supplied together")
    disk, package = fixtures(args.output.resolve())
    command = [str(args.binary.resolve())]
    if args.runner_binary:
        if args.qemu_sysroot:
            command += ["-L", str(args.qemu_sysroot.resolve())]
        command.append(str(args.runner_binary.resolve()))
    elif args.qemu_sysroot:
        parser.error("--qemu-sysroot requires --runner-binary")
    command += [str(disk), str(package)]
    if args.package:
        command += [str(args.package.resolve()), args.entry]
    if os.name == "nt":
        import ctypes
        # Child inherits this error mode: a failed probe must not open a WER UI.
        ctypes.windll.kernel32.SetErrorMode(0x0001 | 0x0002 | 0x8000)
    result = subprocess.run(command, capture_output=True, text=True, timeout=120)
    print(result.stdout, end="")
    print(result.stderr, end="", file=sys.stderr)
    result.check_returncode()


if __name__ == "__main__":
    main()
