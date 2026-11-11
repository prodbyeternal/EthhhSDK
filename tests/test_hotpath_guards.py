"""Source-level regression guards for pinned-player and inactive-patch hot paths."""
from pathlib import Path
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]


class HotPathGuards(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.game = (ROOT / "src/core/game.cpp").read_text(encoding="utf-8")
        cls.hooks = (ROOT / "src/core/hooks.cpp").read_text(encoding="utf-8")
        cls.patch = (ROOT / "src/core/ethhhpatch.cpp").read_text(encoding="utf-8")
        cls.runtime = (ROOT / "src/core/ethhhpatch_runtime.cpp").read_text(encoding="utf-8")
        cls.util = (ROOT / "src/core/util.cpp").read_text(encoding="utf-8")
        cls.hl = (ROOT / "mods/HalfLifeMovement/src/HalfLifeMovementMod.cs").read_text(encoding="utf-8")
        cls.input = (ROOT / "src/core/input.cpp").read_text(encoding="utf-8")
        cls.entity = (ROOT / "src/core/ethhhpatch_entity_interpolation.h").read_text(encoding="utf-8")
        cls.reader = (ROOT / "src/core/ethhhpatch_entity_reader.h").read_text(encoding="utf-8")

    def test_identity_does_not_snapshot_every_strat(self):
        start = self.game.index("bool ReadStratIdentity(")
        end = self.game.index("std::vector<StratView> SnapshotStrats", start)
        body = self.game[start:end]
        self.assertNotIn("SnapshotStrats(", body)
        self.assertIn("LiveAddressMatches", body)
        self.assertIn("ResolveOwnerHandle", body)

    def test_pinned_tracker_bypasses_candidate_refresh(self):
        start = self.game.index("void PlayerTracker::Update()")
        end = self.game.index("uintptr_t PlayerTracker::Addr()", start)
        body = self.game[start:end]
        pinned = body.index("if (PinnedMode(mode_)")
        pinned_return = body.index("if (manualValid_ || mode_ == PlayerMode::Manual) return;", pinned)
        refresh = body.index("RefreshCandidates();", pinned)
        self.assertLess(pinned_return, refresh)
        self.assertIn("if (updateTimer_ > 0.0f) return;", body[pinned:refresh])
        self.assertIn("return manual_ && manualValid_", self.game)

    def test_inactive_patch_has_no_culler_detour(self):
        install_tick = self.hooks[self.hooks.index("bool InstallTickHook()"):self.hooks.index("void StartGameWatcher()")]
        self.assertNotIn("runtime::InstallHooks", install_tick)
        request = self.runtime[self.runtime.index("bool RequestDisplayPlan("):self.runtime.index("bool RequestRestore(")]
        self.assertIn("InstallHooks()", request)
        self.assertIn("UninstallHooks()", self.runtime)

    def test_projection_diagnostics_are_throttled(self):
        self.assertIn("frame - previous < 30", self.patch)
        self.assertIn("never validate every render", self.patch)

    def test_hl_camera_defaults_and_gui_cache(self):
        self.assertIn('GetFloat("eye_height", 69.0f)', self.hl)
        self.assertIn('GetFloat("forward_offset", 24.0f)', self.hl)
        self.assertIn("private float _eyeHeight = 69.0f", self.hl)
        self.assertIn("private float _forwardOffset = 24.0f", self.hl)
        self.assertIn("Bound(_eyeHeight, 0.0f, 300.0f, 69.0f)", self.hl)
        self.assertIn("Bound(_forwardOffset, 0.0f, 60.0f, 24.0f)", self.hl)
        gui = self.hl[self.hl.index("public override void OnGui()") :]
        self.assertNotIn("Game.GetStratClassName", gui)

    def test_writable_game_data_avoids_page_protection_churn(self):
        start = self.util.index("bool SafeWriteBytes(")
        body = self.util[start:self.util.index("uintptr_t FindPattern", start)]
        direct = body.index("if (writable)")
        protect = body.index("VirtualProtect((void*)addr, size, PAGE_EXECUTE_READWRITE")
        self.assertLess(direct, protect)
        self.assertIn("if (protection == PAGE_EXECUTE_READWRITE", body)

    def test_orbit_render_does_not_walk_full_identity_and_bounds_queries(self):
        body = self.runtime[self.runtime.index("void ApplyRenderMouseLook()"):self.runtime.index("float CurrentRenderFraction()")]
        self.assertNotIn("ReadStratIdentity(", body)
        self.assertNotIn("SnapshotStrats(", body)
        self.assertIn("counter - g_lastCameraTraceCounter >= g_counterFrequency / 60", body)
        self.assertIn("game::TraceSegment segments[5]", body)
        self.assertNotIn("VirtualProtect", body)

    def test_reacquire_enforces_native_prior_pin_before_scanning(self):
        body = self.game[self.game.index("bool PlayerTracker::ReacquireExactNinja()"):self.game.index("void PlayerTracker::RefreshCandidates()")]
        self.assertLess(body.index("pinConsent_.AllowsReacquire(PinnedMode(mode_))"), body.index("SnapshotStrats("))
        self.assertIn("mode_=previousMode;", body)
        select = self.game[self.game.index("void PlayerTracker::Select("):self.game.index("void PlayerTracker::BeginLearning()")]
        self.assertIn("pinConsent_.Selected(complete && UniqueNinja(live) == addr)", select)
        for start, end in [("void PlayerTracker::Reset()", "void PlayerTracker::SetMode("),
                           ("void PlayerTracker::SetMode(", "void PlayerTracker::Select("),
                           ("void PlayerTracker::Select(", "void PlayerTracker::BeginLearning()"),
                           ("void PlayerTracker::BeginLearning()", "bool PlayerTracker::ReacquireExactNinja()")]:
            self.assertIn("pinConsent_.Revoke()", self.game[self.game.index(start):self.game.index(end)])

    def test_ninja_facing_does_not_read_a_nonexistent_fourth_angle(self):
        body = self.game[self.game.index("bool WriteNinjaFacing("):self.game.index("std::vector<StratView> SnapshotStrats")]
        self.assertIn("WriteNinjaFacingPose", body)
        self.assertNotIn("angles[3]", body)
        self.assertNotIn("angles[4]", body)

    def test_owner_replacement_does_not_clear_scripted_camera_latch(self):
        sample = self.runtime[self.runtime.index("void SampleOrbitCamera()"):self.runtime.index("void ApplyRenderMouseLook()")]
        self.assertIn("if (changed) { g_orbit.OwnerChanged();", sample)
        self.assertNotIn("g_orbit.Reset()", sample)

    def test_hl_flight_does_not_use_positive_height_as_ownership(self):
        self.assertNotIn("_jumpCarry > 0", self.hl)
        self.assertNotIn("HlJump.Step(", self.hl)
        self.assertIn("!_hlJumpActive && !_nativeVerticalActive", self.hl)
        self.assertIn("HlJump.Integrate(", self.hl)

    def test_hl_position_write_follows_all_collision_queries(self):
        tick = self.hl[self.hl.index("public override void OnTick("):self.hl.index("private void HandleHotkeys()")]
        self.assertNotIn("? _lastWritten : gamePos", tick)
        self.assertNotIn("ny=groundY", tick)
        self.assertLess(tick.index("TryHorizontal("), tick.index("TryVertical("))
        self.assertLess(tick.index("TryVertical("), tick.index("ProbeGround(finalPos"))
        self.assertLess(tick.index("ProbeGround(finalPos"), tick.index("Game.WritePlayerPosition("))

    def test_native_wasd_lease_is_scoped_to_native_tick(self):
        tick = self.hooks[self.hooks.index("void __fastcall TickDetour"):self.hooks.index("void __fastcall KeyboardTickDetour")]
        self.assertLess(tick.index("BeginNativeTick()"), tick.index("g_tickOrig(self"))
        self.assertLess(tick.index("g_tickOrig(self"), tick.index("EndNativeTick()"))
        self.assertLess(tick.index("EndNativeTick()"), tick.index("ModManager::Get().Tick"))
        body = self.input[self.input.index("void BeginNativeTick()"):self.input.index("void EndNativeTick()")]
        self.assertLess(body.index("if (!g_movementLease.Pending()) return"), body.index("ReadStratIdentity"))
        self.assertNotIn("SnapshotStrats", body)
        hook = self.hooks[self.hooks.index("bool InstallMovementInputHook()"):self.hooks.index("void Shutdown()")]
        self.assertLess(hook.index("memcmp(bytes, expected"), hook.index("MH_CreateHook"))

    def test_hl_traversal_grace_cannot_refresh_from_walking(self):
        self.assertNotIn("(gamePos-_lastGame).Length", self.hl)
        self.assertIn("_nativeTraversalTicks--;", self.hl)
        self.assertIn("_traversalSupportTicks >= 2", self.hl)
        self.assertIn("_nativeVerticalActive = !onGround", self.hl)

    def test_entity_render_only_inputs_and_fixed_tick_sampling(self):
        render = self.runtime[self.runtime.index("void __fastcall NodeSubmitDetour"):self.runtime.index("void __fastcall ViewRenderDetour")]
        self.assertNotIn("SnapshotStrats(", render)
        self.assertNotIn("ReadStratIdentity(", render)
        self.assertNotIn("SafeWrite", render)
        self.assertNotIn("0x413B30", self.runtime)
        self.assertNotIn("0x4132D0", self.runtime)
        self.assertIn("pose->renderNodes.data()", render)
        self.assertIn("pose ? &pose->renderWorld : world", render)
        sample = self.runtime[self.runtime.index("void SampleEntityPoses()"):self.runtime.index("void __fastcall NodeSubmitDetour")]
        self.assertIn("BeginTick(tick)", sample)
        self.assertIn("if (!complete)", sample)
        tick = self.runtime[self.runtime.index("void OnTickCompleted()"):self.runtime.index("void BeforeGamePresent()")]
        self.assertIn("SampleEntityPoses()", tick)

    def test_entity_submission_reentry_and_pause_fallback(self):
        scoped = self.runtime[self.runtime.index("void SubmitEntityScoped"):self.runtime.index("void __fastcall ViewRenderDetour")]
        self.assertIn("SubmitEntityScoped(self, owner, instance, world, nullptr)", scoped)
        self.assertIn("g_submittedPose = previousPose; g_entitySubmission = previousSubmission", scoped)
        node = self.runtime[self.runtime.index("void __fastcall NodeSubmitDetour"):self.runtime.index("void SubmitEntityScoped")]
        self.assertIn("else suppliedWorld = &pose->nativeWorld", node)
        fraction = self.runtime[self.runtime.index("float CurrentRenderFraction()"):self.runtime.index("void SampleEntityPoses()")]
        self.assertIn("step <= 0 || input::OverlayOpen()", fraction)

    def test_entity_hotpaths_use_phase_scoped_protected_copies(self):
        sample = self.runtime[self.runtime.index("void SampleEntityPoses()"):self.runtime.index("bool ReadEntityRender(")]
        self.assertNotIn("SnapshotStrats", sample)
        self.assertNotIn("ReadStrat", sample)
        self.assertNotIn("SafeReadBytes", sample)
        self.assertIn("g_entityOwners.Collect", sample)
        self.assertIn("g_entitySampleReads.Reset()", sample)
        render = self.runtime[self.runtime.index("void __fastcall NodeSubmitDetour"):self.runtime.index("void __fastcall ViewRenderDetour")]
        self.assertNotIn("SafeReadBytes", render)
        self.assertNotIn("lock_guard", render)
        self.assertNotIn("ETH_INFO", render)
        self.assertIn("ReadEntityRender", render)
        self.assertIn("__try { memcpy", self.reader)
        self.assertIn("__except (EXCEPTION_EXECUTE_HANDLER)", self.reader)
        self.assertIn("VirtualQuery", self.reader)
        self.assertIn("BeginEntityFrame()", self.runtime)
        self.assertIn("FinishEntityFrame()", self.runtime)
        finish = self.runtime[self.runtime.index("void FinishEntityFrame()"):self.runtime.index("void __fastcall NodeSubmitDetour")]
        self.assertIn("if (g_entityFrameAlpha < 1)", finish)

    def test_entity_work_is_bounded_and_buffers_reused(self):
        self.assertIn("kEntityWorkBudgetMilliseconds = 2", self.runtime)
        self.assertIn("if (!g_entityRenderBudget.Available())", self.runtime)
        self.assertIn("if (!withinBudget()) break", self.runtime)
        self.assertIn("p.previousNodes.swap(p.nodes)", self.entity)
        self.assertIn("std::array<Matrix,kMaxNodes> current_", self.entity)
        mesh = self.entity[self.entity.index("bool MeshFits("):self.entity.index("struct Pose")]
        self.assertNotIn("std::find", mesh)
        self.assertIn("length++ == 512", mesh)
        self.assertIn("counter-g_lastOrbitLogCounter >= g_counterFrequency", self.runtime)

    def test_mouse_attacks_keep_native_owner_focus_and_tick_gates(self):
        body = self.input[self.input.index("void FilterNativeKeyboard("):self.input.index("bool KeyPressed(")]
        for guard in ("g_nativeTickActive", "g_attackOwner.addr", "Focused()", "!g_overlayOpen",
                      "!KeyDown(VK_F9)", "pressureKeyboard == addr", "!keyboardSuppressed",
                      "handle == g_attackOwner.selfHandle", "AttackBindingsSupported(table)"):
            self.assertIn(guard, body)
        self.assertNotIn("ReadStratIdentity", body)
        self.assertNotIn("VirtualProtect", body)
        self.assertNotIn("SendInput", self.input)
        self.assertNotIn("keybd_event", self.input)
        self.assertIn("input::ResetTransient();", self.hooks)
        self.assertIn("__finally { patch::runtime::EndNativeMovementTick(); input::EndNativeTick(); }", self.hooks)
        sample = self.input[self.input.index("void SampleAttackOwner()"):self.input.index("void BeginNativeTick()")]
        self.assertNotIn("tracker.Valid()", sample)
        self.assertNotIn("SnapshotStrats", sample)
        self.assertIn("identity.role == game::StratRole::Ninja", sample)

    def test_fov_only_sampling_does_not_require_mouse_orbit(self):
        sample = self.runtime[self.runtime.index("void SampleOrbitCamera()"):self.runtime.index("void ApplyRenderMouseLook()")]
        self.assertIn("!g_mouseLookEnabled && !g_normalFovEnabled.load()", sample)
        fov = self.runtime[self.runtime.index("void ApplyNormalRenderFov("):self.runtime.index("void SampleEntityPoses()")]
        self.assertNotIn("g_mouseLookEnabled", fov)
        self.assertNotIn("ReadStratIdentity", fov)
        self.assertNotIn("SetCapture", fov)

    def test_camera_relative_heading_is_native_tick_and_exact_vm_scoped(self):
        body = self.runtime[self.runtime.index("uint32_t __fastcall BuiltinFieldScoped"):self.runtime.index("float CurrentRenderFraction()")]
        for guard in ("field != 0x6A", "kBuiltinFieldReturn", "other != self", "g_headingPolicy.Eligible()",
                      "input::OverlayOpen()", "input::KeyDown(VK_CONTROL)", "NativeMovementKeysOwned()",
                      "g_externalCameraOwned.load()", "NativeOrbitStillPlayable(self)"):
            self.assertIn(guard, body)
        for expensive in ("SnapshotStrats", "ReadStratIdentity", "VirtualProtect", "ETH_INFO", "lock_guard", "SafeWrite"):
            self.assertNotIn(expensive, body)
        self.assertLess(body.index("g_builtinFieldOrig(self, output, other, field)"),body.index("movement::Route"))
        tick=self.hooks[self.hooks.index("void __fastcall TickDetour"):self.hooks.index("void __fastcall KeyboardTickDetour")]
        self.assertLess(tick.index("input::BeginNativeTick()"),tick.index("BeginNativeMovementTick()"))
        self.assertLess(tick.index("BeginNativeMovementTick()"),tick.index("g_tickOrig(self"))
        self.assertLess(tick.index("EndNativeMovementTick()"),tick.index("input::EndNativeTick()"))
        self.assertIn("g_headingPolicy.Rendered",self.runtime)
        scratch=self.runtime[self.runtime.index("bool WriteHeadingScratch"):self.runtime.index("bool NativeOrbitStillPlayable")]
        self.assertNotIn("VirtualProtect(",scratch)
        self.assertIn("__except (EXCEPTION_EXECUTE_HANDLER)",scratch)


if __name__ == "__main__":
    unittest.main(testRunner=unittest.TextTestRunner(stream=sys.stdout, verbosity=2))
