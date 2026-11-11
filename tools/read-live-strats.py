"""Single, bounded read-only I-Ninja snapshot. No injection, writes or input.

Addresses are session-local evidence, never persistent player identifiers.
"""
import argparse
import ctypes as c
from ctypes import wintypes as w
import datetime
import hashlib
import json
import math
from pathlib import Path
import struct

EXPECTED_EXE = Path(r"C:\Users\Administrator\Desktop\ininja\I-Ninja.exe")
EXPECTED_SHA = "49099fef26610e84f94f1aa2dd33aa9e83413ee24ff87fed63a8a98cc4f3daf0"


class ReadOnlyProcess:
    def __init__(self, pid):
        self.k = c.WinDLL("kernel32", use_last_error=True)
        self.k.OpenProcess.argtypes = [w.DWORD, w.BOOL, w.DWORD]
        self.k.OpenProcess.restype = w.HANDLE
        self.k.CloseHandle.argtypes = [w.HANDLE]
        self.k.QueryFullProcessImageNameW.argtypes = [w.HANDLE, w.DWORD, w.LPWSTR, c.POINTER(w.DWORD)]
        self.k.ReadProcessMemory.argtypes = [w.HANDLE, c.c_void_p, c.c_void_p, c.c_size_t, c.POINTER(c.c_size_t)]
        # Only PROCESS_QUERY_INFORMATION | PROCESS_VM_READ; no VM_OPERATION/WRITE.
        self.handle = self.k.OpenProcess(0x410, False, pid)
        if not self.handle:
            raise c.WinError(c.get_last_error())
        try:
            path = c.create_unicode_buffer(32768)
            length = w.DWORD(len(path))
            if not self.k.QueryFullProcessImageNameW(self.handle, 0, path, c.byref(length)):
                raise c.WinError(c.get_last_error())
            self.path = Path(path.value)
            if self.path.resolve() != EXPECTED_EXE.resolve():
                raise ValueError(f"refusing another process: {self.path}")
            if hashlib.sha256(self.path.read_bytes()).hexdigest() != EXPECTED_SHA:
                raise ValueError("unsupported executable fingerprint")
        except Exception:
            self.close()
            raise

    def close(self):
        if self.handle:
            self.k.CloseHandle(self.handle)
            self.handle = None

    def read(self, address, size):
        if address < 0x10000 or address + size > 0x100000000 or size > 0x10000:
            return None
        buf = c.create_string_buffer(size)
        count = c.c_size_t()
        ok = self.k.ReadProcessMemory(self.handle, c.c_void_p(address), buf, size, c.byref(count))
        return buf.raw if ok and count.value == size else None

    def u32(self, address):
        data = self.read(address, 4)
        return struct.unpack("<I", data)[0] if data else 0


def hx(value):
    return f"{value:08X}"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("pid", type=int)
    parser.add_argument("--candidate", action="append", default=[], type=lambda s: int(s, 16))
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args()
    process = ReadOnlyProcess(args.pid)
    try:
        begin_tick = process.u32(0x51AB38)
        strats = []
        visited = set()
        address = process.u32(0x511A38)
        while address and address not in visited and len(visited) < 8192:
            visited.add(address)
            data = process.read(address, 0x1C8)
            if not data:
                break
            pos = struct.unpack_from("<4f", data, 0x160)
            if all(math.isfinite(n) for n in pos[:3]):
                strats.append({"address": hx(address), "class": hx(struct.unpack_from("<I", data, 0x1A8)[0]),
                               "pos": pos, "flags": hx(struct.unpack_from("<I", data, 0x190)[0]),
                               "type": hx(struct.unpack_from("<I", data, 0x194)[0])})
            address = struct.unpack_from("<I", data, 0x198)[0]
        wanted = {hx(n) for n in args.candidate}
        classes = {}
        details = {}
        live = {s["address"]: s for s in strats}
        for key in wanted:
            if key not in live:
                details[key] = {"live": False}
                continue
            data = process.read(int(key, 16), 0x310)
            details[key] = {"live": True, "raw_0x310": data.hex() if data else None}
            cls = live[key]["class"]
            if cls not in classes:
                info = process.read(int(cls, 16), 0x80)
                fields = []
                if info:
                    for offset in range(0, len(info), 4):
                        pointer = struct.unpack_from("<I", info, offset)[0]
                        target = process.read(pointer, 128)
                        if target:
                            text = target.split(b"\0", 1)[0]
                            fields.append({"offset": hx(offset), "value": hx(pointer),
                                           "target_hex": target[:64].hex(),
                                           "ascii": text.decode("ascii") if 3 <= len(text) <= 120 and all(32 <= n < 127 for n in text) else None})
                classes[cls] = {"raw_0x80": info.hex() if info else None, "readable_fields": fields}
        cam_addr = process.u32(0x51AB4C)
        cam = process.read(cam_addr + 0x80, 0x40) if cam_addr else None
        result = {"captured": datetime.datetime.now().isoformat(), "pid": args.pid,
                  "exe": str(process.path), "sha256": EXPECTED_SHA, "access": "read-only 0x410",
                  "tick_begin": begin_tick, "tick_end": process.u32(0x51AB38),
                  "note": "Non-atomic snapshot; the game is not suspended.",
                  "strats": strats, "candidates": details, "classes": classes,
                  "camera": {"address": hx(cam_addr), "world": struct.unpack("<16f", cam) if cam else None}}
        args.out.parent.mkdir(parents=True, exist_ok=True)
        args.out.write_text(json.dumps(result, indent=2, allow_nan=False), encoding="utf-8")
        print(f"Read-only snapshot: {len(strats)} live strats, ticks {result['tick_begin']}..{result['tick_end']} -> {args.out}")
        for key in sorted(wanted):
            print(key, live.get(key, "not live"))
        for cls, info in classes.items():
            print("class", cls, "strings", [(f["offset"], f["ascii"]) for f in info["readable_fields"] if f["ascii"]])
    finally:
        process.close()


if __name__ == "__main__":
    main()
