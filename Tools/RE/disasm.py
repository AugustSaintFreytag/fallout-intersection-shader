# Disassembles functions of the unpacked game exe.
# Usage: Tools\.venv\Scripts\python -I Tools\RE\disasm.py <address>[:<max instructions>] ...
# Example: Tools\.venv\Scripts\python -I Tools\RE\disasm.py B97D90 B650C0:40
# Stops at the first `ret` after the max count is reached, or at the max count plus 200.
import os
import sys

import capstone
import pefile

EXE = os.path.join(os.path.dirname(__file__), "..", "..", "Engine", "bin", "FalloutNV_1.4.0.525_unpacked.exe")


def main():
    pe = pefile.PE(EXE, fast_load=True)
    image = pe.get_memory_mapped_image()
    base = pe.OPTIONAL_HEADER.ImageBase
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)

    for spec in sys.argv[1:]:
        address, _, count = spec.partition(":")
        address = int(address, 16)
        count = int(count) if count else 60
        print(f"==== {address:#x}")
        code = image[address - base:address - base + (count + 200) * 15]
        for i, ins in enumerate(md.disasm(code, address)):
            print(f"  {ins.address:#x}: {ins.bytes.hex():<20} {ins.mnemonic} {ins.op_str}")
            if (i + 1 >= count and ins.mnemonic == "ret") or i >= count + 200:
                break


if __name__ == "__main__":
    main()
