"""Read-only, fingerprint-checked PestControl lifecycle evidence. No process access."""
import argparse
import importlib.util
import json
from pathlib import Path
import struct
from capstone import Cs, CS_ARCH_X86, CS_MODE_32

SPEC = importlib.util.spec_from_file_location("patch_image", Path(__file__).with_name("inspect-ethhhpatch.py"))
IMAGE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(IMAGE)
RANGES = [
    ("native_128_deletion_request", 0x416F10, 0x416F1C),
    ("manager_native_deferred_teardown", 0x424760, 0x424869),
    ("manager_reference_and_handle_cleanup", 0x4243B0, 0x424464),
    ("generation_handle_invalidation", 0x411580, 0x4115A7),
    ("script_and_lifetime_destructor", 0x413AE0, 0x413B2D),
    ("model_collision_and_resource_destructor", 0x412F40, 0x413031),
    ("asset_wide_sweep_NOT_safe_for_Rat", 0x424470, 0x4244EF),
]

def evidence(data):
    image = IMAGE.PEImage(data)
    md = Cs(CS_ARCH_X86, CS_MODE_32)
    rows, slices = {}, []
    for name, start, end in RANGES:
        offset = image.offset(start, end-start, executable=True)
        instructions = []
        cursor = start
        for ins in md.disasm(data[offset:offset+end-start], start):
            if ins.address != cursor:
                raise ValueError("non-contiguous lifecycle decode")
            row = {"va": ins.address, "bytes": ins.bytes.hex(), "op": ins.mnemonic, "args": ins.op_str}
            rows[ins.address] = (ins.mnemonic, ins.op_str)
            instructions.append(row)
            cursor += ins.size
        if cursor != end:
            raise ValueError("incomplete lifecycle decode")
        slices.append({"name": name, "start": start, "end_exclusive": end, "instructions": instructions})
    native = struct.unpack_from("<I", data, image.offset(0x4C7E70+128*4, 4))[0]
    if native != 0x416F10 or rows[0x416F14] != ("or", "dword ptr [eax + 0x190], 5"):
        raise ValueError("unsupported per-strat deletion native")
    expected = {
        0x42477B: ("test", "cl, 1"), 0x42477E: ("mov", "ecx, edi"),
        0x424780: ("je", "0x4247ff"), 0x424782: ("call", "0x410860"),
        0x4247BF: ("call", "0x423c50"), 0x4247D4: ("call", "0x4243b0"),
        0x4247DF: ("call", "0x42b610"), 0x4247E6: ("call", "0x413ae0"),
        0x4247F7: ("mov", "dword ptr [esi + 0x18a4], edi"),
        0x424410: ("call", "0x411580"), 0x411594: ("mov", "dword ptr [eax + 0x51019c], edx"),
        0x413AE4: ("call", "0x412f40"),
        0x424496: ("cmp", "dword ptr [eax], ebp"),
    }
    for va, instruction in expected.items():
        if rows.get(va) != instruction:
            raise ValueError(f"lifecycle anchor mismatch at {va:08X}")
    return {"source_sha256": IMAGE.SHA256, "no_process_access": True, "no_binary_changes": True,
            "request_native": 128, "request_va": native, "pending_flags": 5,
            "deferred_to_native_tick": True, "unsafe_asset_sweep": 0x424470,
            "rat_name": "Rat", "rat_name_evidence": "read-only live identity snapshot; not inferred from executable strings",
            "slices": slices}

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", type=Path)
    args = parser.parse_args()
    result = evidence(IMAGE.EXE.read_bytes())
    text = json.dumps(result, indent=2)
    if args.out:
        out = args.out.resolve()
        if not out.is_relative_to(IMAGE.TEMP.resolve()):
            parser.error("output must be a NEW file in the approved research directory")
        with out.open("x", encoding="utf-8") as stream:
            stream.write(text)
        print(f"PASS verified native-128 deletion lifecycle -> {out}")
    else:
        print(text)
