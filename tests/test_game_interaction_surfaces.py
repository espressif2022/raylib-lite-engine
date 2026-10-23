"""Exercise player controls through the real module and adaptive view mapping."""
import json
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]

# Each surface gets its own process to isolate the C facade's global state.
SCRIPT = r'''
import json, os, sys, tempfile
from pathlib import Path
from host.run_game import GenericHostRuntime, prepare_project_assets
root=Path.cwd()
name,size=sys.argv[1:]
wide=size=="320x240"
os.environ["RAYLIB_LITE_HOST_SIZE"]=size
project=root/"examples"/name
prepare_project_assets(project)
with tempfile.TemporaryDirectory() as directory:
    runtime=GenericHostRuntime(project,Path(directory))
    try:
        def tap(x,y):
            runtime.pointer(1,x,y,True)
            runtime.pointer(1,x,y,False)
        def frame():
            runtime.step(False,False,False)
            return runtime.snapshot_rgb565()
        frame()
        if name=="tower_defense":
            tap(160 if wide else 240,120 if wide else 220)
            frame()
            pad=(39,83) if wide else (58,154)
            confirm=(140,45) if wide else (200,90)
            tap(*pad)
            frame()
            assert runtime.metadata()["credits"]==190
            if wide:
                # Map coordinates under the HUD cannot alias the separate overlay button.
                tap(133,30)
                assert runtime.metadata()["credits"]==190
                tap(*pad)
            # Re-inspection and unrelated action releases must not spend/reset touch ownership.
            runtime.pointer(1,*pad,True)
            frame()
            runtime.pointer(1,*pad,False)
            assert runtime.metadata()["credits"]==190
            runtime.pointer(1,*confirm,True)
            frame()
            assert runtime.metadata()["credits"]==120
            runtime.pointer(1,*confirm,True)
            frame()
            assert runtime.metadata()["credits"]==120
            runtime.pointer(1,*confirm,False)
            tap(*pad)
            frame()
            # The lower central pad must remain selectable during preparation;
            # START WAVE used to cover it and launch a wave instead.
            tap(240 if wide else 400,45 if wide else 90)
            tap(117 if wide else 176,218 if wide else 356)
            frame()
            assert runtime.metadata()["awaiting_wave"] is True
            tap(*confirm)
            frame()
            assert runtime.metadata()["credits"]==50
            tap(160 if wide else 240,48 if wide else 96)
            frame()
            assert runtime.metadata()["awaiting_wave"] is False
        elif name=="raylib_shooter":
            tap(160 if wide else 240,120 if wide else 240)
            frame()
            start=runtime.metadata()
            assert start["phase"]=="playing" and start["player_y"]==420
            # Keyboard up really moves vertically, as advertised.
            runtime.action(2,True)
            runtime.api.raylib_lite_host_game_update_v1(runtime.context)
            runtime.action(2,False)
            assert runtime.metadata()["player_y"]==413
            pause=(300,45) if wide else (440,90)
            runtime.pointer(1,*pause,True)
            frame()
            assert runtime.metadata()["phase"]=="paused"
            runtime.pointer(1,*pause,True)
            frame()
            assert runtime.metadata()["phase"]=="paused"
            runtime.pointer(1,*pause,False)
            tap(160 if wide else 240,160 if wide else 320)
            frame()
            assert runtime.metadata()["phase"]=="playing"
            assert runtime.metadata()["player_y"]==413
        else:
            toggle=(239,16) if wide else (397,32)
            runtime.pointer(1,*toggle,True)
            frame()
            runtime.pointer(1,*toggle,True)
            frame()
            assert runtime.metadata()["auto_pan"] is False
            runtime.pointer(1,*toggle,False)
            p1,p2=((130,90),(160,110)) if wide else ((180,180),(240,220))
            runtime.pointer(1,*p1,True)
            runtime.pointer(1,*p2,True)
            runtime.pointer(1,0,0,False)
            before=runtime.metadata()
            for _ in range(150): runtime.step(False,False,False)
            after=runtime.metadata()
            assert before["yaw"]==after["yaw"] and before["pitch"]==after["pitch"]
        pixels,width,height=frame()
        assert (width,height)==tuple(map(int,size.split("x")))
        capture=os.environ.get("RLE_UX_CAPTURE_DIR")
        if capture:
            from PIL import Image
            import array
            values=array.array("H",pixels)
            rgb=bytes(c for value in values for c in (
                ((value>>11)&31)*255//31,((value>>5)&63)*255//63,(value&31)*255//31))
            out=Path(capture);out.mkdir(parents=True,exist_ok=True)
            Image.frombytes("RGB",(width,height),rgb).save(out/(name+"-"+size+".png"))
        print(json.dumps(runtime.metadata()))
    finally:
        runtime.close()
'''


class GameInteractionSurfaceTests(unittest.TestCase):
    def test_controls_at_square_and_landscape_sizes(self):
        for game in ("tower_defense", "raylib_shooter", "living_worlds"):
            for size in ("480x480", "320x240"):
                with self.subTest(game=game, size=size):
                    result = subprocess.run(
                        [sys.executable, "-c", SCRIPT, game, size], cwd=ROOT,
                        text=True, capture_output=True, timeout=120,
                    )
                    self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_shooter_chinese_font_coverage_and_exact_pixels(self):
        from examples.raylib_shooter.assets_src.prepare_font import (
            ASCII_GLYPHS, CJK_GLYPHS, check_shooter_view_coverage,
        )
        from host.run_game import prepare_project_assets
        project = ROOT / "examples/raylib_shooter"
        with tempfile.TemporaryDirectory() as td:
            isolated = Path(td) / "assets_src"
            shutil.copytree(project / "assets_src", isolated)
            subprocess.run([sys.executable, str(isolated / "prepare_font.py")], check=True)
        prepare_project_assets(project)
        all_codes = sorted(set(ASCII_GLYPHS) | set(CJK_GLYPHS))
        required = check_shooter_view_coverage(set(all_codes))
        self.assertIn(0x673A, required)  # '机' from "拖动战机 / 自动开火"
        data = (project / "assets/generated/shooter_zh.atlas").read_bytes()
        magic, w, h, frames, flags, rgb_len, alpha_len = struct.unpack("<4sHHHHII", data[:20])
        self.assertEqual((magic, w, h, flags), (b"MSA1", 256, 64, 1))
        offset = 20 + frames * 16
        alpha = data[offset + rgb_len:offset + rgb_len + alpha_len]
        for idx, cp in enumerate(all_codes):
            col, row = idx % 16, idx // 16
            cx, cy = col * 16 + 2, row * 16 + 2
            is_cjk = cp in CJK_GLYPHS
            rows = CJK_GLYPHS[cp] if is_cjk else ASCII_GLYPHS[cp]
            bit_w = 12 if is_cjk else 8
            for ry, row_bits in enumerate(rows):
                for rx in range(bit_w):
                    expected = 255 if (row_bits & (1 << (bit_w - 1 - rx))) else 0
                    actual = alpha[(cy + ry) * w + (cx + rx)]
                    self.assertEqual(actual, expected, f"corrupted pixel at U+{cp:04X} ({rx},{ry})")


if __name__ == "__main__":
    unittest.main()
