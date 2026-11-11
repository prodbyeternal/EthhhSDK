"""Offline reproducibility and refusal tests for static EthhhPatch evidence."""
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

TOOL = Path(__file__).resolve().parents[1] / "tools" / "inspect-ethhhpatch.py"
SPEC = importlib.util.spec_from_file_location("ethhhpatch_evidence", TOOL)
INSPECTOR = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(INSPECTOR)


class EvidenceTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.data = INSPECTOR.EXE.read_bytes()
        cls.image = INSPECTOR.PEImage(cls.data)
        cls.result = INSPECTOR.evidence(cls.data)

    def test_static_anchors(self):
        self.assertEqual(len(self.result["checked_instruction_anchors"]), len(INSPECTOR.ANCHORS))
        self.assertEqual(len(self.result["static_slices"]), len(INSPECTOR.RANGES))
        self.assertTrue(self.result["no_process_access"])
        self.assertTrue(self.result["no_binary_changes"])

    def test_present_callers(self):
        self.assertEqual(self.result["present_return_vas"], {"game": 0x44A485, "movie": 0x496CB6})
        hooks = (TOOL.parent.parent / "src" / "core" / "hooks.cpp").read_text(encoding="utf-8")
        self.assertIn("ret == game::A(0x44A485)", hooks)
        self.assertIn("ret == game::A(0x496CB6)", hooks)

    def test_fragments_are_not_complete_functions(self):
        self.assertFalse(self.result["static_slices"][0]["known_function_boundaries"])
        self.assertEqual(self.result["static_slices"][0]["end_exclusive"], 0x43D9FC)

    def test_vertical_fov_projection_and_observer_site(self):
        contract = self.result["projection_contract"]
        self.assertEqual(contract["view_matrix_offset"], 0x120)
        self.assertEqual(contract["set_transform_return_va"], 0x43F2DE)
        self.assertEqual(contract["fov_convention"], "full vertical angle, radians")
        hooks = (TOOL.parent.parent / "src" / "core" / "hooks.cpp").read_text(encoding="utf-8")
        self.assertIn("ret == game::A(0x43F2DE)", hooks)
        self.assertIn("state == D3DTS_PROJECTION", hooks)
        self.assertIn("vtbl[44]", hooks)

    def test_culler_has_no_aspect_operand(self):
        section = next(s for s in self.result["static_slices"] if s["label"] == "renderer_sphere_cull")
        # EDI is loaded from pView; it is not reassigned in this function. Its
        # only loads are +4 (far plane) and +0x10 (sphere/FOV margin), not +0.
        operands = [i["operands"] for i in section["instructions"]]
        self.assertIn("edi, dword ptr [0x51ab64]", operands)
        self.assertFalse(any("[edi]" in operand for operand in operands))
        self.assertTrue(any("[edi + 4]" in operand for operand in operands))
        self.assertTrue(any("[edi + 0x10]" in operand for operand in operands))
        self.assertFalse(self.result["culling_blocker"]["aspect_read_in_this_routine"])

    def test_render_flag_and_ui_pulse_increment(self):
        effect = self.result["render_side_effect"]
        self.assertEqual(effect["call"], 0x40C859)
        self.assertAlmostEqual(effect["phase_increment_per_render"], .1, places=7)
        section = next(s for s in self.result["static_slices"] if s["label"] == "shared_UI_tick_render")
        rows = {i["va"]: i for i in section["instructions"]}
        self.assertEqual(rows[0x40C853]["operands"], "bl, bl")
        self.assertEqual(rows[0x40C855]["operands"], "0x40c872")
        self.assertEqual(rows[0x40C859]["operands"], "0x4483b0")

    def test_explicit_render_edges_not_raw_scan_candidates(self):
        edges = {(e["site"], e["target"]) for e in self.result["checked_render_edges"]}
        self.assertEqual(edges, set(INSPECTOR.EDGES))
        self.assertEqual(len(edges), len(INSPECTOR.EDGES))
        self.assertIn((0x43E14A, 0x40C620), edges)  # tick, arg 0
        self.assertIn((0x44A400, 0x40C620), edges)  # render, arg 1

    def test_display_lifecycle_imports_and_mode_request(self):
        lifecycle = self.result["display_lifecycle"]
        self.assertEqual(self.image.import_name(0x4C720C), "PostMessageA")
        self.assertEqual(self.image.import_name(0x4C71F4), "GetWindowInfo")
        self.assertEqual(lifecycle["posted_message"], "WM_SIZE")
        self.assertEqual(lifecycle["mode_pending_offset"], 0x46)
        self.assertEqual(lifecycle["inactive_block_offset"], 0x45)
        self.assertEqual(lifecycle["constructor_request"], {"width": 640, "height": 480, "refresh": 60})
        section = next(s for s in self.result["static_slices"] if s["label"] == "wrapper_configure_display")
        rows = {i["va"]: i for i in section["instructions"]}
        self.assertEqual(rows[0x45230C]["operands"], "al")
        self.assertEqual(rows[0x45234E]["operands"], "dword ptr [0x4c720c]")

    def test_windowed_and_fullscreen_dimensions_are_separate(self):
        lifecycle = self.result["display_lifecycle"]
        self.assertIn("rcClient", lifecycle["windowed_dimensions"])
        self.assertIn("bounded mode", lifecycle["fullscreen_dimensions"])
        self.assertEqual(lifecycle["windowed_refresh"], 0)
        self.assertEqual(lifecycle["windowed_interval"], 0)
        self.assertEqual(lifecycle["fullscreen_interval"], 1)
        section = next(s for s in self.result["static_slices"] if s["label"] == "fullscreen_mode_chooser")
        rows = {i["va"]: i for i in section["instructions"]}
        self.assertEqual(rows[0x453027]["mnemonic"], "ja")
        self.assertEqual(rows[0x453030]["mnemonic"], "ja")
        self.assertEqual(rows[0x453039]["mnemonic"], "ja")

    def test_render_lifecycle_teardown_reset_rebuild(self):
        edges = {(e["site"], e["target"]) for e in self.result["checked_render_edges"]}
        for edge in [(0x43E1D7, 0x44AFD0), (0x44AFEE, 0x44ABE0),
                     (0x44AFF9, 0x452420), (0x44B00B, 0x44AB00)]:
            self.assertIn(edge, edges)
        hooks = (TOOL.parent.parent / "src" / "core" / "hooks.cpp").read_text(encoding="utf-8")
        self.assertIn("device == game::Device()", hooks)
        self.assertIn("patch::OnReset(captured ? &requested : nullptr, hr)", hooks)

    def test_invalid_import_lookup_refused(self):
        with self.assertRaisesRegex(ValueError, "IAT entry not found"):
            self.image.import_name(0x401000)

    def test_zero_tick_render_split_reaches_existing_tick_skip(self):
        split = self.result["runtime_render_split"]
        self.assertEqual(split["wait_branch_va"], 0x43D96C)
        self.assertEqual(split["original_bytes"], "7cf2")
        self.assertEqual(split["inactive_bytes"], "9090")
        self.assertEqual(split["zero_due_tick_skip"], 0x43D995)
        self.assertTrue(split["timer_consumer_retains_fractional_debt_when_zero_due"])
        app = next(s for s in self.result["static_slices"] if s["label"] == "App_Run_message_timer_tick_render_fragment")
        rows = {i["va"]: i for i in app["instructions"]}
        self.assertEqual(rows[0x43D995]["mnemonic"], "jle")
        self.assertEqual(rows[0x43D995]["operands"], "0x43d9a0")
        self.assertEqual(rows[0x43D9C4]["operands"], "0x43e170")

    def test_runtime_split_is_reversible_and_guarded(self):
        root = TOOL.parent.parent
        runtime = (root / "src" / "core" / "ethhhpatch_runtime.cpp").read_text(encoding="utf-8")
        ui = (root / "src" / "core" / "ui_pages.cpp").read_text(encoding="utf-8")
        self.assertIn("kWaitBytes[2] = { 0x7C, 0xF2 }", runtime)
        self.assertIn("kZeroTickBytes[2] = { 0x90, 0x90 }", runtime)
        self.assertIn("ExactCodeWrite(game::A(kZeroTickWaitBranch), kZeroTickBytes, kWaitBytes)", runtime)
        self.assertNotIn("ActivateRenderSplit(", ui)  # activation follows verified Reset, never a direct UI write
        self.assertIn("acknowledgeExperimental", ui)
        self.assertIn("RequestDisplayPlan(plan, monitor.hz, error)", ui)
        self.assertIn("Restore previous display", ui)
        self.assertTrue(self.result["runtime_render_split"]["activation_exposed_to_ui"])

    def test_movie_surface_and_uniform_fit_hook(self):
        movie = self.result["movie_scaling"]
        self.assertEqual((movie["surface_width"], movie["surface_height"]), (640, 480))
        self.assertEqual(movie["original_behavior"], "independent X/Y fill scaling")
        self.assertIn("uniform min fit", movie["compatibility_policy"])
        self.assertEqual(movie["draw_dispatch"], 0x497D40)
        runtime = (TOOL.parent.parent / "src" / "core" / "ethhhpatch_runtime.cpp").read_text(encoding="utf-8")
        self.assertIn("MovieDrawDetour", runtime)
        self.assertIn("FitCentered(sourceWidth, sourceHeight", runtime)
        self.assertIn("kMovieDraw = 0x497D40", runtime)

    def test_hud_projection_sites_are_bounded(self):
        sites = self.result["runtime_render_split"]["hud_identity_projection_return_vas"]
        self.assertEqual(sites, [0x44726B, 0x4474D6, 0x44884A])
        runtime = (TOOL.parent.parent / "src" / "core" / "ethhhpatch_runtime.cpp").read_text(encoding="utf-8")
        for site in sites:
            self.assertIn(f"0x{site:X}", runtime)
        self.assertIn("i % 5 == 0 ? 1.0f : 0.0f", runtime)

    def test_display_transaction_uses_engine_reset_and_rollback(self):
        root = TOOL.parent.parent
        runtime = (root / "src" / "core" / "ethhhpatch_runtime.cpp").read_text(encoding="utf-8")
        ui = (root / "src" / "core" / "ui_pages.cpp").read_text(encoding="utf-8")
        self.assertIn("ExactFullscreenModeSupported", runtime)
        self.assertIn("ResizeClient", runtime)
        self.assertIn("StartRollback", runtime)
        self.assertIn("AppliedModeMatches", runtime)
        self.assertIn("ConfigureDisplay(previous.Windowed != FALSE", runtime)
        self.assertNotIn("->Reset(", runtime)
        self.assertIn("launcherMode || !valid || !acknowledgeExperimental", ui)
        self.assertIn("RequestRestore(error)", ui)

    def test_collision_batch_contract_and_sdk_guards(self):
        trace = self.result["collision_trace"]
        self.assertEqual(trace["query_bytes"], 0x44)
        self.assertEqual(trace["queue_capacity"], 0x400)
        self.assertEqual(trace["result_hit_count_offset"], 0x0E)
        self.assertEqual(trace["result_normal_offset"], 0x10)
        game = (TOOL.parent.parent / "src/core/game.cpp").read_text(encoding="utf-8")
        self.assertIn("count > 16", game)
        self.assertIn("queued != 0", game)
        self.assertIn("identity.role != StratRole::Ninja", game)
        self.assertIn("query + 0x20", game)
        self.assertIn("result + 0x0E", game)

    def test_per_render_camera_yields_to_hl_owner(self):
        hooks = self.result["runtime_render_split"]["compatibility_hooks"]
        self.assertEqual(hooks["per_render_camera"], 0x43F290)
        runtime = (TOOL.parent.parent / "src/core/ethhhpatch_runtime.cpp").read_text(encoding="utf-8")
        mod = (TOOL.parent.parent / "mods/HalfLifeMovement/src/HalfLifeMovementMod.cs").read_text(encoding="utf-8")
        self.assertIn("ApplyRenderMouseLook", runtime)
        self.assertIn("g_externalCameraOwned.load()", runtime)
        self.assertIn("Game.SetExternalCameraOwned(_cameraEnabled)", mod)
        self.assertIn("Game.SetExternalCameraOwned(false)", mod)

    def test_changed_executable_refused(self):
        changed = bytearray(self.data)
        changed[self.image.offset(0x43D969, 3)] ^= 1
        with self.assertRaisesRegex(ValueError, "fingerprint"):
            INSPECTOR.evidence(changed)

    def test_invalid_pe_reads(self):
        for va, size in [(0, 1), (0x400000, 0), (0x401000, 0x4001), (0xFFFFFFFF, 4)]:
            with self.subTest(va=va, size=size), self.assertRaises(ValueError):
                self.image.offset(va, size)

    def test_mapped_raw_offsets(self):
        for va, expected in INSPECTOR.ANCHORS.items():
            size = len(bytes.fromhex(expected))
            offset = self.image.offset(va, size, executable=True)
            self.assertEqual(self.data[offset:offset + size].hex(), expected)

    def test_new_json_and_overwrite_refusal(self):
        with tempfile.TemporaryDirectory(prefix="ethhhpatch-test-", dir=INSPECTOR.TEMP) as directory:
            path = Path(directory) / "new.json"
            INSPECTOR.write_new(path, {"test": True})
            self.assertEqual(json.loads(path.read_text()), {"test": True})
            with self.assertRaises(FileExistsError):
                INSPECTOR.write_new(path, {"test": False})
            self.assertEqual(json.loads(path.read_text()), {"test": True})

    def test_outside_temp_and_binary_output_refused(self):
        for path in [TOOL.parent / "forbidden.json", INSPECTOR.TEMP / "forbidden.exe"]:
            with self.subTest(path=path), self.assertRaises(ValueError):
                INSPECTOR.write_new(path, {})


if __name__ == "__main__":
    unittest.main(verbosity=2)
