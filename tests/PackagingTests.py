import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "packaging"))
import release


class PackagingTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.folder = Path(self.directory.name)
        self.packages = ["linux.deb", "linux.rpm", "linux.AppImage", "intel.dmg", "silicon.dmg",
                         "windows-x64.exe", "windows-x86.exe", "windows-x64.zip", "windows-x86.zip"]
        for name in self.packages:
            (self.folder / name).write_bytes(name.encode())
        subprocess.run([sys.executable, str(ROOT / "packaging/checksums.py"), str(self.folder)], check=True)
        self.environment = patch.dict(os.environ, GITHUB_SHA="build-commit")
        self.environment.start()
        self.addCleanup(self.environment.stop)

    def test_checksums_include_every_package_and_both_installers(self):
        entries = (self.folder / "SHA256SUMS").read_text().splitlines()
        self.assertEqual(len(entries), 9)
        for line in entries:
            digest, name = line.split("  ")
            self.assertEqual(digest, hashlib.sha256((self.folder / name).read_bytes()).hexdigest())

    @patch("release.subprocess.run")
    def test_new_release_stays_a_draft_and_contains_all_assets(self, run):
        run.return_value = subprocess.CompletedProcess([], 1, stdout="")
        release.draft_release(self.folder)
        command = run.call_args_list[-1].args[0]
        self.assertEqual(command[:3], ["gh", "release", "create"])
        self.assertIn("--draft", command)
        self.assertIn("build-commit", command)
        for name in self.packages + ["SHA256SUMS"]:
            self.assertIn(str(self.folder / name), command)

    @patch("release.subprocess.run")
    def test_rerun_updates_the_draft_and_replaces_its_assets(self, run):
        run.return_value = subprocess.CompletedProcess([], 0, stdout=json.dumps({"isDraft": True}))
        release.draft_release(self.folder)
        self.assertEqual(run.call_args_list[1].args[0][:3], ["gh", "release", "edit"])
        self.assertEqual(run.call_args_list[2].args[0][:3], ["gh", "release", "upload"])
        self.assertIn("--clobber", run.call_args_list[2].args[0])

    @patch("release.subprocess.run")
    def test_published_release_is_never_changed(self, run):
        run.return_value = subprocess.CompletedProcess([], 0, stdout=json.dumps({"isDraft": False}))
        release.draft_release(self.folder)
        self.assertEqual(run.call_count, 1)

    @patch("release.subprocess.run")
    def test_incomplete_platform_builds_do_not_create_a_release(self, run):
        (self.folder / "windows-x86.exe").unlink()
        with self.assertRaisesRegex(SystemExit, "all five platform builds"):
            release.draft_release(self.folder)
        run.assert_not_called()

    @patch("release.subprocess.run")
    def test_missing_checksums_do_not_create_a_release(self, run):
        (self.folder / "SHA256SUMS").unlink()
        with self.assertRaisesRegex(SystemExit, "checksums are missing"):
            release.draft_release(self.folder)
        run.assert_not_called()


if __name__ == "__main__":
    unittest.main()
