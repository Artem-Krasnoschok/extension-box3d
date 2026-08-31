#!/usr/bin/env python3
"""Fetch and verify the exact Box3D source revision recorded in UPSTREAM.json."""

from __future__ import annotations

import argparse
import hashlib
import json
import shutil
import tempfile
import urllib.request
import zipfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, default=ROOT / ".cache" / "box3d")
    args = parser.parse_args()

    metadata = json.loads((ROOT / "UPSTREAM.json").read_text(encoding="utf-8"))
    commit = metadata["commit"]
    expected = metadata["archive_sha256"].lower()
    url = f"https://codeload.github.com/erincatto/box3d/zip/{commit}"

    with tempfile.TemporaryDirectory(prefix="extension-box3d-") as temp_dir:
        archive = Path(temp_dir) / "box3d.zip"
        print(f"Downloading {url}")
        with urllib.request.urlopen(url) as response, archive.open("wb") as output:
            shutil.copyfileobj(response, output)

        actual = hashlib.sha256(archive.read_bytes()).hexdigest()
        if actual != expected:
            raise SystemExit(f"Box3D archive checksum mismatch: expected {expected}, got {actual}")

        extracted = Path(temp_dir) / "source"
        with zipfile.ZipFile(archive) as zip_file:
            zip_file.extractall(extracted)
        roots = [path for path in extracted.iterdir() if path.is_dir()]
        if len(roots) != 1:
            raise SystemExit("Unexpected Box3D archive layout")

        destination = args.output.resolve()
        if destination.exists():
            shutil.rmtree(destination)
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copytree(roots[0], destination)

    print(f"Verified Box3D {commit} at {destination}")


if __name__ == "__main__":
    main()
