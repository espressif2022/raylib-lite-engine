# SPDX-License-Identifier: Apache-2.0
"""Native raylib file callbacks, registered assets and real FNT/PNG rendering."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
import unittest
from PIL import Image
from tests.test_rcore_platform import ROOT, RAYLIB, DEFINES, INCLUDES


class RcoreAssetsTests(unittest.TestCase):
    def test_registered_font_and_file_callback_lifetime(self):
        cc = os.environ.get("CC", "cc")
        flags = shlex.split(os.environ.get("CFLAGS", ""))
        includes = [arg for path in [*INCLUDES, ROOT / "src/assets", ROOT / "tools/frame_compare"]
                    for arg in ("-I", str(path))]
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory)
            objects = []
            upstream = [RAYLIB / "src" / name for name in ("rshapes.c", "rtextures.c", "rtext.c")]
            for source in upstream + [ROOT / "src/rcore/raylib_lite_rcore.c"]:
                obj = output / (source.stem + ".o")
                subprocess.run([cc, "-std=gnu99", "-O1", "-w", *DEFINES, *flags, *includes,
                                "-c", str(source), "-o", str(obj)], check=True)
                objects.append(str(obj))
            lib_sources = [ROOT / path for path in [
                "src/rcore/raylib_lite_rcore_assets.c", "src/assets/raylib_lite_assets.c",
                "tools/frame_compare/frame_host.c", "tools/frame_compare/frame_scene.c",
                "src/runtime/raylib_lite_raylib_port.c", "src/runner/raylib_lite_input_queue.c"]]
            for source in lib_sources:
                obj = output / (source.stem + ".o")
                subprocess.run([cc, "-std=c11", "-O1", "-Wall", "-Wextra", "-Werror", *flags,
                                *includes, "-c", str(source), "-o", str(obj)], check=True)
                objects.append(str(obj))
            archive = output / "libraylib_lite_rcore_test.a"
            subprocess.run(["ar", "rcs", str(archive), *objects], check=True)
            executable = output / "rcore_assets"
            subprocess.run([cc, "-std=c11", "-O1", "-Wall", "-Wextra", "-Werror", *flags,
                            *includes, str(ROOT / "tests/test_rcore_assets.c"),
                            str(archive), "-lm", "-o", str(executable)], check=True)
            image = Image.new("RGBA", (8, 4), (255, 0, 0, 255))
            for y in range(4):
                for x in range(4, 8):
                    image.putpixel((x, y), (0, 255, 0, 255))
            path = output / "source.png"
            image.save(path)
            subprocess.run([str(executable), str(path)], check=True)
            auto_exec = output / "rcore_auto_archive"
            subprocess.run([cc, "-std=c11", "-O1", "-Wall", "-Wextra", "-Werror", *flags,
                            *includes, str(ROOT / "tests/test_rcore_auto_archive.c"),
                            str(archive), "-lm", "-o", str(auto_exec)], check=True)
            subprocess.run([str(auto_exec), str(path)], check=True)


if __name__ == "__main__":
    unittest.main()
