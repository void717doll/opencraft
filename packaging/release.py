#!/usr/bin/env python3
import json
import os
import subprocess
import sys
from collections import Counter
from pathlib import Path

from version import game_version


def draft_release(folder):
    expected = {".deb": 1, ".rpm": 1, ".AppImage": 1, ".dmg": 2, ".exe": 2, ".zip": 2}
    packages = sorted(path for path in folder.iterdir() if path.suffix in expected and path.is_file())
    if Counter(path.suffix for path in packages) != expected:
        raise SystemExit("release requires packages from all five platform builds")
    checksums = folder / "SHA256SUMS"
    if not checksums.is_file():
        raise SystemExit("release checksums are missing")

    version = game_version()
    tag = "v" + version
    target = os.environ["GITHUB_SHA"]
    assets = [str(path) for path in packages] + [str(checksums)]
    release = subprocess.run(["gh", "release", "view", tag, "--json", "isDraft"],
                             capture_output=True, text=True)
    if release.returncode == 0:
        if not json.loads(release.stdout)["isDraft"]:
            print("release is already published; its files were left unchanged")
            return
        subprocess.run(["gh", "release", "edit", tag, "--target", target], check=True)
        subprocess.run(["gh", "release", "upload", tag, *assets, "--clobber"], check=True)
    else:
        subprocess.run(["gh", "release", "create", tag, *assets, "--draft", "--target", target,
                        "--title", "OpenCraft " + version,
                        "--notes", "packages for linux, macos, and windows; checksums are in SHA256SUMS."],
                       check=True)


if __name__ == "__main__":
    draft_release(Path(sys.argv[1]))
