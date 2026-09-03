from __future__ import annotations

import tempfile
import unittest
import zipfile
from pathlib import Path

from tools import package_release


class PackageReleaseTest(unittest.TestCase):
    def test_archive_has_versioned_root_and_explicit_directories(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            temp = Path(temp_dir)
            artifacts = temp / "artifacts"
            output = temp / "dist"

            for platform in package_release.PLATFORMS:
                name = "box3d.lib" if platform == "x86_64-win32" else "libbox3d.a"
                library = artifacts / f"box3d-{platform}" / name
                library.parent.mkdir(parents=True, exist_ok=True)
                library.write_bytes(platform.encode("ascii"))

            archive = package_release.build_release_archive(
                artifacts, output, "test-version"
            )

            with zipfile.ZipFile(archive) as packaged:
                infos = {info.filename: info for info in packaged.infolist()}

            root = "extension-box3d-test-version/"
            self.assertTrue(infos[root].is_dir())
            self.assertTrue(infos[f"{root}box3d/"].is_dir())
            self.assertTrue(infos[f"{root}box3d/lib/wasm-web/"].is_dir())
            self.assertIn(f"{root}game.project", infos)
            self.assertIn(f"{root}box3d/ext.manifest", infos)
            self.assertIn(f"{root}box3d/lib/wasm-web/libbox3d.a", infos)
            self.assertNotIn("game.project", infos)
            self.assertTrue(all(name.startswith(root) for name in infos))


if __name__ == "__main__":
    unittest.main()
