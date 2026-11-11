"""Bounded static display/timer evidence. No process, engine call or patching.

Requires Python Capstone. Writes a NEW JSON only under the approved research
temporary directory, never an executable/archive/configuration.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct

from capstone import Cs, CS_ARCH_X86, CS_MODE_32

EXE = Path(r"C:\Users\Administrator\Desktop\ininja\I-Ninja.exe")
TEMP = Path(r"C:\Users\Administrator\AppData\Local\Temp\opencode")
SHA256 = "49099fef26610e84f94f1aa2dd33aa9e83413ee24ff87fed63a8a98cc4f3daf0"


class PEImage:
    def __init__(self, data):
        self.data = data
        if len(data) != 1040384 or hashlib.sha256(data).hexdigest() != SHA256:
            raise ValueError("unsupported executable fingerprint")
        pe = struct.unpack_from("<I", data, 0x3C)[0]
        if data[:2] != b"MZ" or data[pe:pe + 4] != b"PE\0\0":
            raise ValueError("invalid PE signature")
        machine, count = struct.unpack_from("<HH", data, pe + 4)
        optional_size = struct.unpack_from("<H", data, pe + 20)[0]
        optional = pe + 24
        if machine != 0x14C or struct.unpack_from("<H", data, optional)[0] != 0x10B:
            raise ValueError("expected x86 PE32")
        self.base = struct.unpack_from("<I", data, optional + 28)[0]
        self.import_rva, self.import_size = struct.unpack_from("<II", data, optional + 104)
        self.sections = []
        for i in range(count):
            p = optional + optional_size + i * 40
            virtual, rva, raw_size, raw = struct.unpack_from("<IIII", data, p + 8)
            flags = struct.unpack_from("<I", data, p + 36)[0]
            self.sections.append((rva, virtual, raw, raw_size, flags))

    def offset(self, va, size, executable=False):
        if size <= 0 or size > 0x4000 or va < self.base:
            raise ValueError("invalid bounded PE read")
        rva = va - self.base
        for start, virtual, raw, raw_size, flags in self.sections:
            if start <= rva and rva + size <= start + min(virtual, raw_size):
                if executable and not flags & 0x20000000:
                    raise ValueError("expected executable section")
                offset = raw + rva - start
                if offset + size > len(self.data):
                    raise ValueError("PE raw data exceeds file")
                return offset
        raise ValueError("unmapped or section-crossing PE read")

    def cstring(self, va, limit=256):
        value = bytearray()
        for i in range(limit):
            byte = self.data[self.offset(va + i, 1)]
            if not byte:
                return value.decode("ascii")
            value.append(byte)
        raise ValueError("unterminated bounded PE string")

    def import_name(self, iat_va):
        if not self.import_rva or self.import_size < 20:
            raise ValueError("missing import directory")
        for descriptor in range(0, self.import_size - 19, 20):
            p = self.offset(self.base + self.import_rva + descriptor, 20)
            original, _, _, _, first = struct.unpack_from("<IIIII", self.data, p)
            if not any((original, first)):
                break
            names = original or first
            for index in range(4096):
                thunk_va = self.base + names + index * 4
                thunk = struct.unpack_from("<I", self.data, self.offset(thunk_va, 4))[0]
                if not thunk:
                    break
                if self.base + first + index * 4 == iat_va:
                    if thunk & 0x80000000:
                        return f"ordinal:{thunk & 0xffff}"
                    return self.cstring(self.base + thunk + 2)
        raise ValueError(f"IAT entry not found: {iat_va:08X}")


# End-exclusive ranges; fragments are marked and never called a whole function.
RANGES = [
    ("App_Run_message_timer_tick_render_fragment", 0x43D90F, 0x43D9FC, False),
    ("timer_consume_due_ticks", 0x401110, 0x401152, True),
    ("timer_query_due_ticks", 0x401160, 0x401177, True),
    ("View_tick", 0x43EFA0, 0x43F175, True),
    ("wrapper_configure_display", 0x452300, 0x452354, True),
    ("wrapper_build_present_parameters", 0x452360, 0x45241D, True),
    ("wrapper_reset", 0x452420, 0x452489, True),
    ("wrapper_create_device", 0x452490, 0x45251C, True),
    ("renderer_frame", 0x44A340, 0x44A489, True),
    ("script_display_mode_selection", 0x4592D0, 0x459326, True),
    ("App_render_step", 0x43E170, 0x43E298, True),
    ("renderer_prepare", 0x449E00, 0x449E30, True),
    ("View_build_submit_render_matrices", 0x43F290, 0x43F4A2, True),
    ("perspective_fov_lh_builder", 0x45ED0F, 0x45EDA3, True),
    ("renderer_sphere_cull", 0x449420, 0x44952C, True),
    ("shared_UI_tick_render", 0x40C620, 0x40C878, True),
    ("UI_render_pulse", 0x4483B0, 0x44843E, True),
    ("camera_update_inverse_and_product", 0x40B750, 0x40B868, True),
    ("renderer_clear_submission_queues", 0x44C580, 0x44C59B, True),
    ("fixed_simulation_tick", 0x43E0A0, 0x43E16F, True),
    ("wrapper_device_status", 0x4522E0, 0x4522FC, True),
    ("renderer_lost_reset_rebuild", 0x44AFD0, 0x44B012, True),
    ("window_refresh_dimensions", 0x456E90, 0x456EEB, True),
    ("window_member_wndproc", 0x457140, 0x4571DD, True),
    ("fullscreen_mode_chooser", 0x452FF0, 0x45307C, True),
    ("wrapper_constructor_display_defaults_fragment", 0x4526B2, 0x4526E5, False),
    ("per_frame_callback_queue", 0x405390, 0x4053BE, True),
    ("empty_pre_tick_virtual", 0x438FF0, 0x438FF1, True),
    ("movie_surface_640x480_fragment", 0x499FB8, 0x49A01D, False),
    ("movie_frame_scaling_fragment", 0x49A290, 0x49A410, False),
    ("movie_draw_dispatch", 0x497D40, 0x497D5E, True),
    ("movie_draw_surface_dimensions", 0x497B10, 0x497B62, True),
    ("movie_surface_dimension_assignment_fragment", 0x497C30, 0x497CC2, False),
    ("collision_query_submit", 0x437E20, 0x437E86, True),
    ("collision_query_result", 0x437E90, 0x437EBF, True),
    ("collision_query_process", 0x4380F0, 0x43811E, True),
    ("native_down_trace_caller", 0x410AA0, 0x410BEF, True),
]

ANCHORS = {
    0x43D962: "e8f937fcff",  # query due timer 0x401160
    0x43D969: "83fe01",      # compare due ticks with 1
    0x43D96C: "7cf2",        # wait loop, no zero-tick render currently
    0x43D970: "e89b37fcff",  # consume actual timer debt 0x401110
    0x43D987: "83fe04",      # cap catch-up to 4
    0x43D99B: "e800070000",  # first simulation tick
    0x43D9B0: "e8eb0e0000",  # extra-tick strat-frame-stamp preparation
    0x43D9B8: "e8e3060000",  # extra simulation tick
    0x43D9C4: "e8a7070000",  # render step after ticks
    0x43EFE4: "c703abaaaa3f",# view aspect reset to 4/3
    0x45230F: "89510c",      # requested width -> wrapper present params
    0x452334: "894110",      # requested height -> wrapper present params
    0x45247F: "ff5140",      # device Reset
    0x4524C0: "ff5240",      # IDirect3D9 CreateDevice, not device Reset
    0x44A482: "ff5244",      # game Present; return VA 0x44A485
    0x459320: "e8db8fffff",  # script mode choice asks for 60 Hz
    0x43E170: "8b153cab5100",# render frame counter read
    0x43E17A: "89153cab5100",# render frame counter write
    0x43E293: "e9a8c00000",  # tailcall RenderFrame
    0x449E03: "e878270000",  # clear renderer submission queues before rebuilding
    0x449E2A: "e96154ffff",  # tailcall View render-matrix builder
    0x43F2AE: "8b06",        # View+0 aspect argument
    0x43F2B3: "8b4e0c",      # View+0x0C full-angle vertical FOV argument
    0x43F2BE: "8d9e20010000",# projection destination View+0x120
    0x43F2CD: "e83dfa0100",  # call canonical LH perspective builder
    0x43F2D5: "6a03",        # D3DTS_PROJECTION
    0x43F2D8: "ff92b0000000",# SetTransform; observer return VA 0x43F2DE
    0x45ED45: "d94510",      # perspective aspect argument
    0x45ED48: "d8f9",        # x scale = y scale / aspect
    0x45ED60: "d95814",      # y scale = cot(full vertical FOV / 2)
    0x45ED85: "d9582c",      # LH perspective matrix _34 = 1
    0x449485: "d94710",      # sphere culler View+0x10 tan-half-FOV
    0x44948A: "d844240c",    # culling side limit z + tan-half-FOV * radius
    0x40C636: "84db",        # test render flag before fixed-state update branch
    0x40C855: "741b",        # skip pulse call on tick path (render flag == 0)
    0x40C859: "e852bb0300",  # render path calls pulse updater
    0x4483B9: "d805889a4c00",# pulse += constant 0.1 on every eligible render
    0x4483CA: "d996e8000000",# persist render-driven phase
    0x43F006: "e845c7fcff",  # View_tick updates camera before gameplay manager
    0x40B780: "f3a5",        # camera+0xC0 gets inverse world matrix (used by CPU culler)
    0x452306: "84d2",        # requested fullscreen flag
    0x45230C: "0f94c0",      # D3D Windowed = !fullscreen
    0x452316: "c6414601",    # mark display/reset pending
    0x45231A: "89513c",      # requested fullscreen refresh
    0x45232D: "89412c",      # store D3DPRESENT_PARAMETERS.Windowed
    0x45234E: "ff250c724c00",# PostMessageA(hwnd, WM_SIZE, 0, 0)
    0x456E9D: "8b482c",      # window refresh checks D3D Windowed
    0x456EA4: "8b480c",      # fullscreen width from wrapper request
    0x456EBE: "c744240c3c000000", # WINDOWINFO.cbSize
    0x456EC6: "ff15f4714c00",# GetWindowInfo for windowed client dimensions
    0x456EE0: "895610",      # save computed client width
    0x45715B: "83e803",      # WndProc reaches WM_SIZE after excluding WM_DESTROY
    0x45718B: "e800fdffff",  # WM_SIZE refreshes dimensions
    0x457193: "c6424601",    # WM_SIZE marks Reset pending
    0x452367: "8b462c",      # present builder branches on Windowed
    0x4523A3: "897e3c",      # windowed fullscreen-refresh = 0
    0x4523A6: "897e40",      # windowed presentation interval = DEFAULT
    0x4523F5: "89463c",      # fullscreen selected refresh
    0x4523F8: "895e40",      # fullscreen presentation interval = ONE
    0x452423: "8a4645",      # inactive/minimized gate blocks Reset attempt
    0x452430: "c6464600",    # Reset path clears pending marker
    0x44AFDD: "e8fe720000",  # query cooperative/pending status
    0x44AFE4: "8886c4070000",# renderer enabled follows status
    0x44AFEE: "e8edfbffff",  # release renderer resources
    0x44AFF9: "e822740000",  # wrapper Reset
    0x44B00B: "e9f0faffff",  # rebuild renderer resources after success
    0x4526C8: "c7450c80020000", # constructor request width 640
    0x4526CF: "c74510e0010000", # constructor request height 480
    0x4526D6: "c7453c3c000000", # constructor request 60 Hz
    0x453024: "39430c",      # mode width must not exceed request
    0x45302D: "394b10",      # mode height must not exceed request
    0x453036: "395314",      # mode refresh must not exceed request
    0x43D993: "85f6",        # zero due count is already tested
    0x43D995: "7e09",        # zero due count skips the first simulation tick
    0x401121: "d84608",      # consume: elapsed plus partial remainder
    0x401133: "d84e04",      # convert accumulated time to tick count
    0x401136: "e85d2b0b00",  # floor to whole due ticks
    0x401144: "d80e",        # whole ticks back to consumed seconds
    0x401146: "d86c2404",    # retain sub-tick remainder
    0x40114A: "d95e08",      # store remainder even when whole due ticks are zero
    0x438FF0: "c3",          # pre-tick helper called by App_Run is empty in this build
    0x405390: "51",          # per-frame callback queue begins
    0x499FC1: "68e0010000",  # movie render surface height 480
    0x499FC6: "6880020000",  # movie render surface width 640
    0x499FCC: "e83fdeffff",  # create fixed-size movie render surface
    0x49A2AC: "ff5120",      # query current backbuffer description
    0x49A2C1: "d80d589a4c00",# movie X scale *= 1/640
    0x49A2DF: "d80d4c894d00",# movie Y scale *= 1/480
    0x49A3A8: "e8a3c8ffff",  # movie Clear/BeginScene
    0x49A405: "e836d9ffff",  # scaled movie draw dispatch
    0x497B37: "8b4618",      # draw reads surface height
    0x497B3A: "8b4e14",      # draw reads surface width
    0x497C8C: "897714",      # assign surface width
    0x497C97: "897b18",      # assign surface height
    0x497D40: "8b442404",    # movie draw dispatch hook target
    0x437E3D: "b911000000",  # collision query is 17 dwords / 0x44 bytes
    0x437E42: "f3a5",        # copy query into manager queue
    0x437E4E: "c744081c8096184b", # assign collision query callback
    0x437E60: "66c744080e0000", # clear result hit count
    0x437E72: "c1e00a",      # token generation occupies bits 10+
    0x437E99: "c1ee0a",      # result validates token generation
    0x437EA6: "25ff030000",  # token queue index mask 0x3ff
    0x437EB7: "6bc044",      # result index stride 0x44
    0x4380F3: "e8a8a5ffff",  # process collision broadphase preparation
    0x4380FA: "e8c1fdffff",  # process queued collision queries
    0x438101: "e89ae6ffff",  # finalize collision query results
    0x438114: "66894610",    # clear queued count after processing
    0x410B59: "e8c2720200",  # native down trace submit
    0x410B65: "e886750200",  # native down trace process
    0x410B70: "e81b730200",  # native down trace result lookup
    0x410B75: "6639580e",    # native caller tests result+0x0E hit count
}

# Explicit checked edges only; not a guessed whole-program call graph.
EDGES = [
    (0x43E22B, 0x449E00), (0x449E03, 0x44C580), (0x449E2A, 0x43F290),
    (0x43F2CD, 0x45ED0F), (0x43E293, 0x44A340),
    (0x43E14A, 0x40C620), (0x44A400, 0x40C620), (0x44A43D, 0x40C620),
    (0x40C859, 0x4483B0), (0x43F006, 0x40B750),
    (0x43E1D7, 0x44AFD0), (0x44AFDD, 0x4522E0),
    (0x44AFEE, 0x44ABE0), (0x44AFF9, 0x452420), (0x44B00B, 0x44AB00),
    (0x452434, 0x452360), (0x45718B, 0x456E90),
    (0x4592FA, 0x452300), (0x459320, 0x452300), (0x4526E0, 0x452490),
    (0x499FCC, 0x497E10), (0x49A3A8, 0x496C50), (0x49A405, 0x497D40),
    (0x410B59, 0x437E20), (0x410B65, 0x4380F0), (0x410B70, 0x437E90),
    (0x4380F3, 0x4326A0), (0x4380FA, 0x437EC0), (0x438101, 0x4367A0),
]


def evidence(data):
    image = PEImage(data)
    md = Cs(CS_ARCH_X86, CS_MODE_32)
    slices = []
    by_va = {}
    for label, start, end, complete in RANGES:
        offset = image.offset(start, end - start, executable=True)
        rows = []
        cursor = start
        for ins in md.disasm(data[offset:offset + end - start], start):
            if ins.address != cursor:
                raise ValueError("non-contiguous static decode")
            row = {"va": ins.address, "file_offset": image.offset(ins.address, ins.size),
                   "bytes": ins.bytes.hex(), "mnemonic": ins.mnemonic, "operands": ins.op_str}
            rows.append(row)
            by_va[ins.address] = row
            cursor += ins.size
        if cursor != end:
            raise ValueError(f"undecoded range tail at {cursor:08X}")
        slices.append({"label": label, "start_va": start, "end_exclusive": end,
                       "known_function_boundaries": complete, "instructions": rows})
    for va, expected in ANCHORS.items():
        if by_va.get(va, {}).get("bytes") != expected:
            raise ValueError(f"static instruction anchor mismatch at {va:08X}")
    edges = []
    for site, target in EDGES:
        row = by_va.get(site, {})
        if row.get("mnemonic") not in ("call", "jmp") or row.get("operands") != hex(target):
            raise ValueError(f"checked render edge differs at {site:08X}")
        edges.append({"site": site, "target": target, "kind": row["mnemonic"]})
    constant_offset = image.offset(0x4C9A88, 4)
    pulse_step = struct.unpack_from("<f", data, constant_offset)[0]
    if data[constant_offset:constant_offset + 4].hex() != "cdcccc3d":
        raise ValueError("render-path pulse constant differs")
    if image.import_name(0x4C720C) != "PostMessageA" or image.import_name(0x4C71F4) != "GetWindowInfo":
        raise ValueError("display lifecycle imports differ")
    movie = image.offset(0x496CB3, 3, executable=True)
    # Movie Present has a separate call site. Its return VA must match telemetry.
    movie_ins = list(md.disasm(data[movie:movie + 3], 0x496CB3))
    if len(movie_ins) != 1 or movie_ins[0].mnemonic != "call" or movie_ins[0].op_str != "dword ptr [edx + 0x44]":
        raise ValueError("movie Present caller differs")
    return {"no_process_access": True, "no_engine_calls": True, "no_binary_changes": True,
            "source": str(EXE), "source_sha256": SHA256, "source_bytes": len(data),
            "checked_instruction_anchors": ANCHORS,
            "present_return_vas": {"game": 0x44A485, "movie": 0x496CB6},
            "checked_render_edges": edges,
            "projection_contract": {"builder": 0x45ED0F, "view_matrix_offset": 0x120,
                                    "set_transform_return_va": 0x43F2DE,
                                    "fov_convention": "full vertical angle, radians",
                                    "near_plane": 0.01, "far_plane_source": "View+0x04"},
            "culling_blocker": {"routine": 0x449420, "camera_view_matrix_offset": 0xC0,
                                "side_limit": "camera_z + View.tanHalfFov * radius",
                                "aspect_read_in_this_routine": False,
                                "scope": "one verified sphere test; not all engine culling"},
            "render_side_effect": {"call": 0x40C859, "routine": 0x4483B0,
                                   "phase_offset": 0xE8, "phase_increment_per_render": pulse_step,
                                   "conditions": "shared UI mode 5 plus existing visibility/pulse flags"},
            "display_lifecycle": {
                "mode_request": 0x452300, "posted_message": "WM_SIZE",
                "post_message_iat": 0x4C720C, "post_message_import": image.import_name(0x4C720C),
                "windowed_dimensions": "GetWindowInfo rcClient width/height",
                "get_window_info_iat": 0x4C71F4, "get_window_info_import": image.import_name(0x4C71F4),
                "fullscreen_dimensions": "requested width/height then bounded mode selection",
                "mode_pending_offset": 0x46, "inactive_block_offset": 0x45,
                "render_lifecycle": 0x44AFD0, "reset": 0x452420,
                "windowed_refresh": 0, "windowed_interval": 0,
                "fullscreen_interval": 1,
                "constructor_request": {"width": 640, "height": 480, "refresh": 60}},
            "runtime_render_split": {
                "wait_branch_va": 0x43D96C, "original_bytes": "7cf2",
                "inactive_bytes": "9090", "reversible_exact_byte_patch": True,
                "zero_due_tick_skip": 0x43D995, "render_after_tick_block": 0x43D9C4,
                "timer_consumer": 0x401110,
                "timer_consumer_retains_fractional_debt_when_zero_due": True,
                "pre_tick_helper": 0x438FF0, "pre_tick_helper_is_ret_only": True,
                "per_frame_callback_queue": 0x405390,
                "activation_exposed_to_ui": True,
                "compatibility_hooks": {"sphere_culler": 0x449420, "hud_pulse": 0x4483B0,
                    "movie_draw": 0x497D40, "per_render_camera": 0x43F290},
                "hud_identity_projection_return_vas": [0x44726B, 0x4474D6, 0x44884A]},
            "movie_scaling": {
                "surface_creation": 0x499FCC, "surface_width": 640, "surface_height": 480,
                "backbuffer_x_scale_divisor": 640, "backbuffer_y_scale_divisor": 480,
                "original_behavior": "independent X/Y fill scaling",
                "compatibility_policy": "uniform min fit with centered pillarbox/letterbox",
                "draw_dispatch": 0x497D40, "draw_call": 0x49A405},
            "collision_trace": {
                "manager": 0x50EDD8, "submit": 0x437E20, "process": 0x4380F0,
                "result": 0x437E90, "query_bytes": 0x44, "queue_capacity": 0x400,
                "token_index_mask": 0x3ff, "result_hit_count_offset": 0x0E,
                "result_point_offset": 0, "result_normal_offset": 0x10,
                "native_reference_caller": 0x410AA0,
                "sdk_policy": "max 16 exact-Ninja segments; require empty queue before batch"},
            "static_slices": slices,
            "conclusions": [
                "App_Run waits for due >= 1 before rendering; simply faking timer due risks simulation speedup.",
                "Timer consume removes actual whole-tick debt, independent from a fabricated query result.",
                "View_tick writes 4/3 each tick; a post-tick aspect write is not a verified projection/culling repair.",
                "Display wrapper stores requested width/height and builds present parameters before CreateDevice/Reset.",
                "Main world projection uses full vertical FOV and View aspect; SetTransform source is View+0x120.",
                "One camera-space sphere culler has no aspect read and uses the same lateral bound for X/Y.",
                "Shared UI mode-5 render path updates a pulse phase by 0.1 per render, not by fixed tick time.",
                "RenderStep clears submission queues and advances frame stamps; extra Present alone cannot rebuild the scene.",
                "Mode requests mark wrapper+0x46 and post WM_SIZE; the next render lifecycle tears down, Resets, then rebuilds on success.",
                "Windowed dimensions come from the current client rectangle; the mode request itself does not resize that rectangle.",
                "Fullscreen mode selection accepts supported width/height/refresh values not exceeding the request; exact application is not guaranteed.",
                "Replacing only the wait-back branch with NOPs reaches the existing zero-count tick skip, then RenderStep, while the original timer retains fractional debt.",
                "The render-split implementation is reversible and exposed only through an acknowledged in-game transaction with exact Reset verification and rollback.",
                "Movies render through a fixed 640x480 surface and originally scale X/Y independently to the backbuffer; the gated policy uses centered uniform fit.",
                "The collision manager supports bounded 0x44-byte segment batches with point/normal/hit-count results; the SDK refuses non-Ninja or nonempty-queue calls.",
                "Per-render mouse look runs before the verified View matrix builder and yields while the managed HL camera owns raw input.",
                "The manager exposes an acknowledged transactional Apply/Restore path; offline evidence is not a live compatibility claim."]}


def write_new(path, value):
    target = Path(path).resolve()
    if not target.is_relative_to(TEMP.resolve()) or target.suffix.lower() != ".json":
        raise ValueError("evidence output must be a new JSON under approved temp directory")
    with target.open("x", encoding="utf-8") as f:
        json.dump(value, f, indent=2)
        f.write("\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", required=True)
    args = parser.parse_args()
    value = evidence(EXE.read_bytes())
    write_new(args.out, value)
    print(f"PASS static EthhhPatch evidence: {len(ANCHORS)} instruction anchors, {len(RANGES)} bounded slices, {len(EDGES)} render edges; {args.out}")


if __name__ == "__main__":
    main()
