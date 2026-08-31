#!/usr/bin/env python3
"""Assemble the Defold dependency ZIP from per-platform CI artifacts."""

from __future__ import annotations

import argparse
import hashlib
import shutil
import tempfile
import zipfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
PLATFORMS = (
    "x86_64-win32",
    "x86_64-linux",
    "arm64-linux",
    "x86_64-osx",
    "arm64-osx",
    "armv7-android",
    "arm64-android",
    "wasm-web",
)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--artifacts", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--version", required=True)
    args = parser.parse_args()

    args.output.mkdir(parents=True, exist_ok=True)
    archive = args.output / f"extension-box3d-{args.version}.zip"

    with tempfile.TemporaryDirectory(prefix="extension-box3d-package-") as temp_dir:
        package = Path(temp_dir) / "extension-box3d"
        for relative in (
            "box3d/ext.manifest",
            "box3d/box3d.script_api",
            "box3d/include",
            "box3d/src",
            "example",
            "game.project",
            "README.md",
            "CHANGELOG.md",
            "CONTRIBUTING.md",
            "LICENSE",
            "NOTICE.md",
            "UPSTREAM.json",
            "third_party",
        ):
            source = ROOT / relative
            destination = package / relative
            if source.is_dir():
                shutil.copytree(source, destination)
            else:
                destination.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(source, destination)

        for platform in PLATFORMS:
            name = "box3d.lib" if platform == "x86_64-win32" else "libbox3d.a"
            source = args.artifacts / f"box3d-{platform}" / name
            if not source.is_file():
                raise SystemExit(f"Missing CI artifact: {source}")
            destination = package / "box3d" / "lib" / platform / name
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source, destination)

        with zipfile.ZipFile(archive, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=9) as output:
            for path in sorted(package.rglob("*")):
                if path.is_file():
                    output.write(path, path.relative_to(package).as_posix())

    digest = hashlib.sha256(archive.read_bytes()).hexdigest()
    checksum = archive.with_suffix(archive.suffix + ".sha256")
    checksum.write_text(f"{digest}  {archive.name}\n", encoding="ascii")
    print(f"Created {archive}")
    print(f"SHA-256 {digest}")


if __name__ == "__main__":
    main()
