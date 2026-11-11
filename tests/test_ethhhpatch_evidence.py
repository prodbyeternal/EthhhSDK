"""Offline reproducibility and refusal tests for static EthhhPatch evidence."""
import importlib.util
import json
import sys
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
        cls.archive = INSPECTOR.keyboard_archive_evidence(INSPECTOR.GENERAL.read_bytes())

    def test_static_anchors(self):
        self.assertEqual(len(self.result["checked_instruction_anchors"]), len(INSPECTOR.ANCHORS))
        self.assertEqual(len(self.result["static_slices"]), len(INSPECTOR.RANGES))
        self.assertTrue(self.result["no_process_access"])
        self.assertTrue(self.result["no_binary_changes"])

    def test_native_keyboard_filter_poll_and_tick_order(self):
        from capstone import Cs, CS_ARCH_X86, CS_MODE_32
        md = Cs(CS_ARCH_X86, CS_MODE_32)
        def rows(start, end):
            p = self.image.offset(start, end-start, executable=True)
            return {i.address: (i.mnemonic, i.op_str) for i in md.disasm(self.data[p:p+end-start], start)}
        p = self.image.offset(0x4427A0, 13, executable=True)
        self.assertEqual(self.data[p:p+13], bytes.fromhex("a1 14 92 52 00 53 56 8b f1 80 78 0d 01"))
        poll = rows(0x4427A0, 0x4428F2)
        self.assertEqual(poll[0x4427FA], ("push", "0x100"))
        self.assertEqual(poll[0x442800], ("call", "dword ptr [edx + 0x24]"))
        self.assertEqual(poll[0x44282A], ("mov", "byte ptr [eax + 0x100], dl"))
        self.assertEqual(poll[0x442830], ("mov", "byte ptr [eax], cl"))
        manager = rows(0x441E60, 0x441E8C)
        self.assertEqual(manager[0x441E63], ("lea", "ecx, [esi + 8]"))
        self.assertEqual(manager[0x441E66], ("call", "0x4427a0"))
        self.assertEqual(rows(0x43FEC0, 0x43FEE4)[0x43FEDF], ("call", "0x441e60"))
        tick = rows(0x43E0A0, 0x43E114)
        self.assertEqual(tick[0x43E0E7], ("call", "0x43fec0"))
        self.assertEqual(tick[0x43E10F], ("call", "0x4246f0"))
        # Poll happens inside native tick, BEFORE the script-object update.
        self.assertLess(0x43E0E7, 0x43E10F)

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

    def test_render_camera_uses_view_pose_and_scoped_restore(self):
        camera = self.result["camera_render_contract"]
        self.assertEqual(camera["view_previous_position"], 0x20)
        self.assertEqual(camera["view_current_quaternion"], 0x50)
        self.assertEqual(camera["view_restored_region"], [0x20, 0x120])
        self.assertEqual(camera["scope"], 0x43E170)
        self.assertFalse(camera["cutscene_flag_verified"])
        runtime = (TOOL.parent.parent / "src/core/ethhhpatch_runtime.cpp").read_text(encoding="utf-8")
        self.assertIn("SafeWriteBytes(view + 0x20, pose, sizeof(pose))", runtime)
        app_render = runtime[runtime.index("void __fastcall AppRenderDetour"):runtime.index("} // namespace\n\nbool InstallHooks")]
        cleanup = app_render[app_render.index("__finally"):]
        self.assertIn("RestoreRenderCamera();", cleanup)
        self.assertIn("FinishEntityFrame();", cleanup)
        self.assertIn("g_entityFrameAlpha = 1;", cleanup)
        self.assertIn("g_inGameRender = false;", cleanup)
        self.assertLess(cleanup.index("RestoreRenderCamera();"), cleanup.index("g_inGameRender = false;"))
        self.assertIn("g_cameraBackup.Capture(camera, view, SafeReadBytes)", runtime)
        self.assertIn("g_cameraBackup.Restore(camera, view, SafeReadBytes, SafeWriteBytes)", runtime)
        hooks = (TOOL.parent.parent / "src/core/hooks.cpp").read_text(encoding="utf-8")
        tick = hooks[hooks.index("void __fastcall TickDetour"):hooks.index("HRESULT WINAPI EndSceneDetour")]
        self.assertLess(tick.index("RunTickCallbacks(tickIndex)"), tick.index("runtime::OnTickCompleted()"))

    def test_primary_view_call_and_secondary_passes_are_distinct(self):
        camera = self.result["camera_render_contract"]
        self.assertEqual(camera["primary_return_va"], camera["primary_call"] + 5)
        self.assertEqual(camera["prepare_tail_jump"], 0x449E2A)
        self.assertEqual(camera["secondary_return_vas"], [0x44EC69, 0x44F576, 0x44FA5C, 0x450953])
        rows = {i["va"]: i for s in self.result["static_slices"] for i in s["instructions"]}
        self.assertEqual(rows[0x449E2A]["mnemonic"], "jmp")
        for address in camera["secondary_return_vas"]:
            self.assertEqual(rows[address - 5]["operands"], "0x43f290")
        runtime = (TOOL.parent.parent / "src/core/ethhhpatch_runtime.cpp").read_text(encoding="utf-8")
        self.assertIn("uintptr_t(_ReturnAddress())", runtime)
        self.assertIn("if (primary) { ApplyRenderMouseLook(); ApplyNormalRenderFov(self); }", runtime)
        self.assertIn("g_primaryViewPass.Reset();", runtime)
        self.assertIn("!(g_cameraBackup.pending && g_cameraBackup.installed)", runtime)

    def test_normal_fov_is_scoped_view_lens_not_native_camera(self):
        lens = self.result["normal_fov_contract"]
        self.assertEqual(lens["view_lens_region"], [0x08, 0x18])
        self.assertEqual(lens["projection_region"], [0x120, 0x160])
        self.assertFalse(lens["native_camera_fov_written"])
        self.assertFalse(lens["default_enabled"])
        runtime = (TOOL.parent.parent / "src/core/ethhhpatch_runtime.cpp").read_text(encoding="utf-8")
        fov = runtime[runtime.index("void ApplyNormalRenderFov("):runtime.index("void SampleEntityPoses()")]
        for guard in ("g_externalCameraOwned.load()", "g_orbit.state != OrbitState::Following",
                      "input::OverlayOpen()", "g_movieUntilCounter.load()", "g_orbitHandle", "g_orbitView"):
            self.assertIn(guard, fov)
        self.assertNotIn("SetCameraFov", fov)
        self.assertIn("g_lensBackup.Restore(view, SafeReadBytes, SafeWriteBytes)", runtime)

    def test_embedded_control_maps_and_held_attack_semantics(self):
        maps = self.archive["maps"]
        keyboard = maps["KEYBOARD.CFM"]["rows"]
        ps2 = maps["PS2.CFM"]["rows"]
        self.assertEqual(keyboard[9], [0, 203, 0, 6, 0, 0, 0])
        self.assertEqual(keyboard[7], [0, 205, 0, 6, 0, 0, 0])
        self.assertEqual(ps2[29], [5, 3, 0, 6, 0, 0, 0])
        self.assertEqual(ps2[27], [5, 1, 0, 6, 0, 0, 0])
        self.assertFalse(self.archive["action_names_verified_in_engine"])
        contract = self.result["native_mouse_attack_contract"]
        self.assertEqual(contract["descriptor_bytes"], 28)
        self.assertEqual(contract["inverted_flag_bytes"], 1)
        self.assertEqual(contract["action_count"], 20)
        self.assertEqual(contract["held_pressure_read"], 0x442FE7)
        self.assertFalse(contract["windows_input_synthesis"])

    def test_native_camera_relative_heading_units_scope_and_calling_convention(self):
        contract=self.result["camera_relative_movement_contract"]
        self.assertEqual(contract["getter"],0x421680)
        self.assertEqual(contract["getter_return"],0x4252EA)
        self.assertEqual(contract["next_ASL_instruction_register"],"ESI")
        self.assertEqual(contract["angle_units_per_turn"],4096)
        self.assertEqual(contract["script_camera_yaw_offset"],0x174)
        self.assertEqual(contract["field_table"],{0x0E:0x421822,0x6A:0x422239,0x91:0x4225F6})
        rows={i["va"]:i for s in self.result["static_slices"] for i in s["instructions"]}
        self.assertEqual(rows[0x424BDE]["operands"],"eax, dword ptr [edi + 0x1b4]")
        self.assertEqual(rows[0x424C41]["operands"],"esi, 4")
        self.assertEqual(rows[0x4252E5]["operands"],"0x421680")
        self.assertEqual(rows[0x422249]["operands"],"0xc")
        self.assertEqual(rows[0x421822]["operands"],"eax, dword ptr [esi + 0x174]")
        self.assertFalse(contract["live_gameplay_accepted"])

    def test_fingerprinted_Ninja_ASL_heading_sequence_and_copy_refusal(self):
        tool=TOOL.with_name("inspect-camera-relative-movement.py")
        spec=importlib.util.spec_from_file_location("movement_evidence",tool)
        module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
        decoded=module.DECODED
        first=module.bounded(decoded/"09.vol/9-95.LCS/block1.bin")
        second=module.bounded(decoded/"09.vol/9-95.LCS/block2.bin")
        manifest=json.loads(module.bounded(decoded/"manifest.json"))
        result=module.decoded_evidence(first,second,manifest)
        self.assertEqual(result["heading_sites_package_offsets"],[0x4B6464])
        self.assertEqual(result["runtime_sequence_words"],self.result["camera_relative_movement_contract"]["runtime_sequence_words"])
        self.assertTrue(result["no_live_address_assumption"])
        self.assertFalse(result["live_gameplay_accepted"])
        for original,index in ((first,0),(second,1)):
            changed=bytearray(original);changed[0]^=1
            args=[first,second];args[index]=changed
            with self.assertRaisesRegex(ValueError,"fingerprint"):
                module.decoded_evidence(*args,manifest)

    def test_changed_control_archive_refused(self):
        changed = bytearray(INSPECTOR.GENERAL.read_bytes())
        changed[0x3800] ^= 1
        with self.assertRaisesRegex(ValueError, "fingerprint"):
            INSPECTOR.keyboard_archive_evidence(changed)

    def test_changed_executable_refused(self):
        changed = bytearray(self.data)
        changed[self.image.offset(0x43D969, 3)] ^= 1
        with self.assertRaisesRegex(ValueError, "fingerprint"):
            INSPECTOR.evidence(changed)

    def test_entity_matrix_submission_is_non_mutating_and_bounded(self):
        contract = self.result["entity_render_contract"]
        self.assertEqual(contract["owner_arguments"], ["owner", "model instance", "world matrix"])
        self.assertEqual(contract["node_arguments"], ["mesh list", "world matrix", "node matrices"])
        self.assertEqual(contract["matrix_bytes"], 64)
        self.assertEqual(contract["count_offset"], 0x140)
        self.assertEqual(contract["world_offset"], 0x10)
        self.assertEqual(contract["model_instance_pair_offsets"], [0x128, 0x12C])
        self.assertEqual(contract["instance_swap_mode_offset"], 0x144)
        self.assertTrue(contract["node_index_sign_extended"])
        copy = next(s for s in self.result["static_slices"] if s["label"] == "entity_renderer_copy_node_matrices")
        rows = {i["va"]: i for i in copy["instructions"]}
        self.assertEqual(rows[0x44A00B]["operands"], "0xc")
        self.assertEqual(rows[0x449FF6]["operands"], "eax, dword ptr [ecx + 0x44]")
        self.assertFalse(any(i["mnemonic"] == "call" for i in copy["instructions"]))

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
    # Successful unittest progress is stdout, not native stderr ErrorRecords in
    # Windows PowerShell's Stop-mode runner. Failures still exit nonzero.
    unittest.main(testRunner=unittest.TextTestRunner(stream=sys.stdout, verbosity=2))
