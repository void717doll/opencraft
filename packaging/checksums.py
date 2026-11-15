#!/usr/bin/env python3
import hashlib
import sys
from pathlib import Path

folder = Path(sys.argv[1])
packages = sorted(path for path in folder.iterdir()
                  if path.suffix in {".deb", ".rpm", ".AppImage", ".dmg", ".exe", ".zip"})
if not packages:
    raise SystemExit("no packages found")
with (folder / "SHA256SUMS").open("w", newline="\n") as output:
    for path in packages:
        digest = hashlib.sha256()
        with path.open("rb") as package:
            for chunk in iter(lambda: package.read(1024 * 1024), b""):
                digest.update(chunk)
        output.write(f"{digest.hexdigest()}  {path.name}\n")
