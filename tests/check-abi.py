"""Read-only PE export checks. Does not load the DLL or touch the game."""
import re
import struct
import sys
from pathlib import Path

root = Path(sys.argv[1])
data = (root / 'build/dist/EthhhSDK.dll').read_bytes()
u16 = lambda p: struct.unpack_from('<H', data, p)[0]
u32 = lambda p: struct.unpack_from('<I', data, p)[0]
pe = u32(0x3c)
assert data[pe:pe+4] == b'PE\0\0' and u16(pe+4) == 0x14c, 'expected x86 PE'
optional = pe+24
assert u16(optional) == 0x10b, 'expected PE32'
sections = []
for i in range(u16(pe+6)):
    p = optional+u16(pe+20)+40*i
    sections.append((u32(p+12), max(u32(p+8), u32(p+16)), u32(p+20)))
def offset(rva):
    for va,size,raw in sections:
        if va <= rva < va+size: return raw+rva-va
    raise ValueError('unmapped RVA')
directory = offset(u32(optional+96))
table = offset(u32(directory+32))
exports = set()
for i in range(u32(directory+24)):
    p = offset(u32(table+4*i))
    exports.add(data[p:data.index(b'\0',p)].decode('ascii'))
expected = {'EthhhSDK_EntryGate'}
for file in ('c_api.cpp','imgui_c.cpp','sdk.cpp'):
    text = (root/'src/core'/file).read_text(encoding='utf-8')
    expected.update(re.findall(r'ETH_CAPI\s*\(\s*[^,]+,\s*(\w+)\s*,',text))
assert expected <= exports, f'missing exports: {sorted(expected-exports)}'
imports = set()
for file in (root/'managed/src').glob('*.cs'):
    imports.update(re.findall(r'extern\s+\w+\s+(Eth\w+)\s*\(',file.read_text(encoding='utf-8')))
assert imports <= exports, f'P/Invoke methods without exports: {sorted(imports-exports)}'
assert 'EthMod_IsEnabled' in exports
print(f'PASS x86 export ABI: {len(expected)} required exports, {len(imports)} P/Invoke bindings resolved')
