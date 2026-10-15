# Lists the direct call sites of functions in the unpacked game exe.
# Usage: Tools\.venv\Scripts\python -I Tools\RE\callers.py <address> ...
# Example: Tools\.venv\Scripts\python -I Tools\RE\callers.py B988E0
# Finds `call rel32` (E8) instructions only. Calls through registers or vtables are not found.
import os
import struct
import sys

import pefile

EXE = os.path.join(os.path.dirname(__file__), "..", "..", "Engine", "bin", "FalloutNV_1.4.0.525_unpacked.exe")


def main():
    pe = pefile.PE(EXE, fast_load=True)
    base = pe.OPTIONAL_HEADER.ImageBase
    targets = {int(spec, 16) for spec in sys.argv[1:]}

    for section in pe.sections:
        if not section.Characteristics & 0x20000000:  # IMAGE_SCN_MEM_EXECUTE
            continue
        data = section.get_data()
        start = base + section.VirtualAddress
        for offset in range(len(data) - 5):
            if data[offset] != 0xE8:
                continue
            address = start + offset
            target = (address + 5 + struct.unpack_from("<i", data, offset + 1)[0]) & 0xFFFFFFFF
            if target in targets:
                print(f"{address:#x} -> {target:#x}")


if __name__ == "__main__":
    main()
