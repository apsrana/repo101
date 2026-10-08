#!/usr/bin/env python3
"""Split every flattened device tree (FDT) out of a binary blob.

Handles DTBs appended to a zImage/Image.gz and Qualcomm QCDT tables inside
a boot image: it scans for the FDT magic and checks each header.

Usage: split-dtb.py <boot.img|kernel> <outdir>
"""
import struct
import sys
from pathlib import Path

FDT_MAGIC = b"\xd0\x0d\xfe\xed"


def split(blob: bytes, outdir: Path) -> int:
    outdir.mkdir(parents=True, exist_ok=True)
    count = 0
    pos = blob.find(FDT_MAGIC)
    while pos != -1:
        if pos + 40 <= len(blob):
            totalsize, off_struct, off_strings, _, version = struct.unpack_from(">5I", blob, pos + 4)
            # Header sanity: plausible size and version, offsets inside the blob.
            if (64 <= totalsize <= 4 << 20 and 16 <= version <= 17
                    and off_struct < totalsize and off_strings < totalsize
                    and pos + totalsize <= len(blob)):
                (outdir / f"{count:02d}_{pos:08x}.dtb").write_bytes(blob[pos:pos + totalsize])
                count += 1
                pos = blob.find(FDT_MAGIC, pos + totalsize)
                continue
        pos = blob.find(FDT_MAGIC, pos + 1)
    return count


def main() -> None:
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    n = split(Path(sys.argv[1]).read_bytes(), Path(sys.argv[2]))
    print(f"{n} device trees written to {sys.argv[2]}")
    if n == 0:
        sys.exit(1)


if __name__ == "__main__":
    main()
