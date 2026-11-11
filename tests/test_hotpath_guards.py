"""Source-level regression guards for pinned-player and inactive-patch hot paths."""
from pathlib import Path
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

    def test_identity_does_not_snapshot_every_strat(self):
        start = self.game.index("bool ReadStratIdentity(")
        end = self.game.index("std::vector<StratView> SnapshotStrats", start)
        body = self.game[start:end]
        self.assertNotIn("SnapshotStrats(", body)
        self.assertIn("LiveAddressMatches", body)
        self.assertIn("ResolveOwnerHandle", body)

    def test_manual_tracker_bypasses_candidate_refresh(self):
        start = self.game.index("void PlayerTracker::Update()")
        end = self.game.index("uintptr_t PlayerTracker::Addr()", start)
        body = self.game[start:end]
        manual = body.index("if (mode_ == PlayerMode::Manual)")
        manual_return = body.index("Manual pinning never needs a full candidate snapshot", manual)
        refresh = body.index("RefreshCandidates();", manual)
        self.assertLess(manual_return, refresh)
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


if __name__ == "__main__":
    unittest.main(verbosity=2)
