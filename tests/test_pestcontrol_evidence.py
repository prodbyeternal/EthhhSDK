"""Static deletion evidence and integration guard tests; never calls the engine."""
import importlib.util
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("pest_evidence", ROOT / "tools/inspect-pestcontrol.py")
TOOL = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(TOOL)

class PestControlEvidenceTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.data = TOOL.IMAGE.EXE.read_bytes()
        cls.result = TOOL.evidence(cls.data)

    def test_request_and_deferred_cleanup(self):
        self.assertEqual(self.result["request_native"], 128)
        self.assertEqual(self.result["pending_flags"], 5)
        self.assertTrue(self.result["deferred_to_native_tick"])
        self.assertTrue(self.result["no_process_access"])
        self.assertTrue(self.result["no_binary_changes"])
        first = self.result["slices"][0]
        self.assertEqual([i["op"] for i in first["instructions"]], ["mov", "or", "ret"])
        self.assertEqual("".join(i["bytes"] for i in first["instructions"]), "8b44240483889001000005c3")

    def test_fingerprint_refuses_changed_binary(self):
        changed = bytearray(self.data)
        changed[0x200] ^= 1
        with self.assertRaises(ValueError):
            TOOL.evidence(bytes(changed))

    def test_native_request_is_only_mutation_boundary(self):
        source = (ROOT / "src/core/pestcontrol.cpp").read_text()
        self.assertIn("Sha256FileHex(executable)", source)
        self.assertIn("RequestBytesMatch()", source)
        self.assertIn("Validate(layout, t, read)", source)
        self.assertIn("NativeRequest(t)", source)
        self.assertNotIn("SafeWrite", source)
        self.assertNotIn("0x423C50", source)
        self.assertNotIn("game::A(0x424470)", source)
        self.assertNotIn("RequestDisplayPlan", source)
        self.assertNotIn("PlayerTracker", source)
        self.assertIn("std::chrono::milliseconds(2)", source)

    def test_disabled_path_and_tick_phase(self):
        source = (ROOT / "src/core/pestcontrol.cpp").read_text()
        tick = source[source.index("void OnNativeTickCompleted()") :]
        self.assertLess(tick.index("!g_enabled.load"), tick.index("NativeLayout()"))
        hooks = (ROOT / "src/core/hooks.cpp").read_text()
        tick = hooks[hooks.index("void __fastcall TickDetour") : hooks.index("void __fastcall KeyboardTickDetour")]
        self.assertLess(tick.index("g_tickOrig(self, tickIndex)"), tick.index("pest::OnNativeTickCompleted()"))
        self.assertLess(tick.index("pest::OnNativeTickCompleted()"), tick.index("PlayerTracker::Get().Update()"))
        self.assertEqual(hooks.count("pest::OnNativeTickCompleted()"), 1)

    def test_exact_class_and_optional_persistence(self):
        policy = (ROOT / "src/core/pestcontrol_policy.h").read_text()
        self.assertIn('!strcmp(name, "Rat")', policy)
        self.assertIn('strcmp(name, "Rat")', policy)
        self.assertNotIn("strstr(", policy)
        self.assertIn("kScanTicks = 15", policy)
        sdk = (ROOT / "src/core/sdk.cpp").read_text()
        self.assertIn('!safeMode && cfg.GetBool("ethhhpatch", "pestcontrol", false)', sdk)
        ui = (ROOT / "src/core/ui_pages.cpp").read_text()
        self.assertIn('BeginTabItem("PestControl")', ui)
        self.assertIn('cfg.SetBool("ethhhpatch", "pestcontrol", enabled)', ui)

if __name__ == "__main__":
    unittest.main()
