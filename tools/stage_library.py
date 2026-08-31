#!/usr/bin/env python3
"""Find a CMake-built Box3D static library and stage it for one Defold platform."""

from __future__ import annotations

import argparse
import shutil
from pathlib import Path


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--build-dir", required=True, type=Path)
    parser.add_argument("--platform", required=True)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()

    expected_name = "box3d.lib" if args.platform == "x86_64-win32" else "libbox3d.a"
    candidates = [
        path
        for path in args.build_dir.rglob(expected_name)
        if path.is_file() and "CMakeFiles" not in path.parts
    ]
    if len(candidates) != 1:
        rendered = "\n".join(str(path) for path in candidates) or "<none>"
        raise SystemExit(f"Expected one {expected_name}, found:\n{rendered}")

    destination = args.output / args.platform / expected_name
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(candidates[0], destination)
    print(f"Staged {candidates[0]} -> {destination}")


if __name__ == "__main__":
    main()
