"""Offline notice integrity and packaging checks. No installer execution."""
import hashlib
from pathlib import Path
import re
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]

class NoticePackagingTests(unittest.TestCase):
    def test_valve_agreement_is_unmodified(self):
        data = (ROOT / "mods/HalfLifeMovement/LICENSE").read_bytes()
        self.assertEqual(hashlib.sha256(data).hexdigest(),
                         "131212c5a670489679af77614fed8089c7ed71ce9ae3aa9333ff7718b643af84")

    def test_lua_notice_matches_vendored_header(self):
        header = (ROOT / "src/third_party/lua/lua.h").read_text()
        notice = header[header.index("* Copyright (C) 1994-2024"):header.index("******************************************************************************/")]
        normalized = "\n".join(line.removeprefix("* ").removeprefix("*") for line in notice.splitlines()).strip()
        self.assertEqual(normalized, (ROOT / "src/third_party/lua/LICENSE.txt").read_text().strip())

    def test_all_staged_notices_match_sources(self):
        script = (ROOT / "tools/stage-notices.ps1").read_text()
        pairs = re.findall(r"@\('([^']+)','([^']+)'\)", script)
        self.assertEqual(len(pairs), 9)
        for source, staged in pairs:
            with self.subTest(source=source, staged=staged):
                self.assertEqual((ROOT / source.replace('\\', '/')).read_bytes(),
                                 (ROOT / "build/dist" / staged.replace('\\', '/')).read_bytes())

    def test_packaging_preserves_distribution_caveats(self):
        notice = (ROOT / "THIRD-PARTY-NOTICES.md").read_text()
        for text in ("not distribution approval", "No external audio license was supplied",
                     "halflife-steam_legacy/pm_shared/pm_shared.c", "does not link FreeType",
                     "SIL OFL 1.1", "CC BY 4.0"):
            self.assertIn(text, notice)
        build = (ROOT / "build.ps1").read_text()
        self.assertIn("tools\\stage-notices.ps1", build)
        install = (ROOT / "install.ps1").read_text()
        section = install[install.index("# Bundle attribution"):]
        self.assertIn("Copy-Item $dst $saved -Force", section)
        self.assertLess(section.index("Copy-Item $dst $saved -Force"), section.index("Copy-Item $file.FullName $dst -Force"))
        self.assertNotIn("Remove-Item", section)

if __name__ == "__main__":
    unittest.main(testRunner=unittest.TextTestRunner(stream=sys.stdout, verbosity=2))
