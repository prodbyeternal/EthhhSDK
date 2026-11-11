"""Read-only, fingerprinted Ninja ASL heading evidence; never opens a process.

Reads existing decoded research copies, not copyrighted data in the SDK tree.
Package offsets are NOT executable or live process addresses. Aligned site
counts do not claim whole-program control-flow or gameplay acceptance.
"""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import struct

SPEC = importlib.util.spec_from_file_location("patch_evidence", Path(__file__).with_name("inspect-ethhhpatch.py"))
PATCH = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(PATCH)
DECODED = PATCH.TEMP / "ininja-data-20261110/selected-02"
BLOCK_HASHES = (
    "68c7d2af9e307c50646867984b9db33a811807fe68516826f10048e1e349befd",
    "5ebf726156b438c50e914c6212f525e94e124958f24273a2e5c3b3f8437fc04a")
WORDS = (0x4275106A, 0x42761091, 0x4276500E, 0x0B747576, 0x4374701F)

def bounded(path):
    if not 0 < path.stat().st_size <= 16 * 1024 * 1024:
        raise ValueError("invalid bounded decoded input")
    return path.read_bytes()

def u32(data, offset):
    if offset < 0 or offset + 4 > len(data):
        raise ValueError("decoded dword outside block")
    return struct.unpack_from("<I", data, offset)[0]

def decoded_evidence(first, second, manifest):
    selected = next(x for x in manifest["decoded"] if x["archive"] == "09.vol" and x["entry"] == "9-95.LCS")
    for index, data in enumerate((first, second)):
        digest = hashlib.sha256(data).hexdigest()
        if digest != BLOCK_HASHES[index] or digest != selected[f"block{index+1}_sha256"] or len(data) != selected["output_sizes"][index]:
            raise ValueError("decoded movement block fingerprint/manifest mismatch")
    # The fingerprinted class record associates Ninja's entry with unit zero.
    record = 0x4D15C0
    name = u32(first, record+4)
    if first[name:name+6] != b"Ninja\0":
        raise ValueError("expected exact Ninja class")
    start, end = 0x4B5F20, 0x4C3240
    entry = u32(first, record+8)
    if entry != 0x4BA6F0 or not start+8 <= entry < end or start+u32(first,start+4)*4 != end:
        raise ValueError("Ninja shared ASL unit association differs")
    sites, variables = [], []
    for offset in range(start+8,end,4):
        word=u32(first,offset)
        if word>>24 == 0x42 and word&0xFFF == 0x06A:
            selector=(word>>8)&0xF0
            if selector == 0x10:
                sequence=struct.unpack_from("<5I",first,offset)
                if sequence != WORDS:
                    raise ValueError("Ninja built-in heading sequence differs")
                sites.append(offset)
            elif selector == 0xF0:
                variables.append(offset)
    if sites != [0x4B6464]:
        raise ValueError("expected exactly one aligned built-in Ninja heading site")
    return {"no_process_access": True, "no_engine_calls": True, "read_only_sources": True,
            "decoded_sha256": list(BLOCK_HASHES), "class": "Ninja", "class_package_offset": record,
            "entry_package_offset": entry, "unit_package_range": [start,end],
            "heading_sites_package_offsets": sites, "runtime_sequence_words": list(WORDS),
            "unrelated_selector_F0_sites": variables,
            "site_scan_scope": "aligned words in the fingerprinted shared Ninja unit, not a complete ASL CFG",
            "no_live_address_assumption": True, "live_gameplay_accepted": False}

def evidence(decoded=DECODED):
    native=PATCH.evidence(PATCH.EXE.read_bytes())["camera_relative_movement_contract"]
    first=bounded(decoded / "09.vol/9-95.LCS/block1.bin")
    second=bounded(decoded / "09.vol/9-95.LCS/block2.bin")
    result=decoded_evidence(first,second,json.loads(bounded(decoded / "manifest.json")))
    result["native_contract"]=native
    result["decoded_sources"]=str(decoded)
    return result

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--decoded",type=Path,default=DECODED)
    parser.add_argument("--out",required=True)
    args=parser.parse_args()
    result=evidence(args.decoded)
    PATCH.write_new(args.out,result)
    print("PASS fingerprinted native + Ninja ASL heading contract: one scoped built-in heading site;",args.out)

if __name__ == "__main__":
    main()
