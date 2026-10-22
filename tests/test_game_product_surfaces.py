"""Player-facing contracts of the remaining released games, through real modules."""
from pathlib import Path
import subprocess
import sys
import unittest

ROOT=Path(__file__).resolve().parents[1]
SCRIPT=r'''
import json,os,sys,tempfile,array
from pathlib import Path
from host.run_game import GenericHostRuntime,prepare_project_assets
name,size=sys.argv[1:];wide=size=="320x240"
os.environ["RAYLIB_LITE_HOST_SIZE"]=size
project=Path.cwd()/"examples"/name
prepare_project_assets(project)
with tempfile.TemporaryDirectory() as directory:
    r=GenericHostRuntime(project,Path(directory))
    try:
        def step(n=1):
            for _ in range(n): r.api.raylib_lite_host_game_update_v1(r.context)
        def tap(x,y,track=1):
            r.pointer(track,x,y,True);r.pointer(track,x,y,False)
        def centered(x,y):return (int((x+80)*.5),int(y*.5)) if wide else (x,y)
        r.snapshot_rgb565()
        if name=="last_zone_extraction":
            tap(*centered(240,240));step()
            assert r.metadata()["phase"]=="playing"
            assert r.metadata()["shots"]==0
            left=(41,196) if wide else (82,392)
            left_move=(41,170) if wide else (82,340)
            right=(270,175) if wide else (380,350)
            right_move=(290,175) if wide else (420,350)
            r.pointer(1,*left,True);r.pointer(1,*left_move,True)
            r.pointer(2,*right,True)
            before=r.metadata()
            r.pointer(2,*right_move,True);step(8)
            held=r.metadata()
            assert held["heading"]!=before["heading"]
            assert held["x"]!=before["x"] or held["y"]!=before["y"]
            assert held["shots"]==0
            r.pointer(2,0,0,False);step()
            assert r.metadata()["shots"]==1
            r.pointer(1,0,0,False);step(25)
            # A complete right tap arriving between updates is still one shot.
            tap(*right,track=2);step()
            assert r.metadata()["shots"]==2
            assert r.metadata()["objective_collected"] is False
        elif name=="sky_hop":
            tap(160 if wide else 240,120 if wide else 240);step()
            before=r.metadata()
            # Ground is above the controls. Touches there cannot move the hero.
            tap(110 if wide else 220,190 if wide else 380);step(5)
            assert r.metadata()["player_x"]==before["player_x"]
            r.pointer(1,110 if wide else 220,215 if wide else 430,True)
            for _ in range(12):r.step(False,False,False)
            assert r.metadata()["player_x"]>before["player_x"]
            r.pointer(2,275 if wide else 350,215 if wide else 430,True)
            r.step(False,False,False)
            assert r.metadata()["player_y"]<before["player_y"]
            r.pointer(2,0,0,False);r.pointer(1,0,0,False)
        else:
            assert r.metadata()["control_mode"]=="touch" and r.metadata()["auto_throttle"]
            step(100);assert r.metadata()["speed"]>0
            mode=centered(80,90)
            # Existing steering capture cannot turn into a mode-button press.
            r.pointer(1,*centered(80,400),True)
            r.pointer(1,*mode,True)
            assert r.metadata()["control_mode"]=="touch"
            r.pointer(1,0,0,False)
            r.pointer(1,*mode,True);r.pointer(1,*mode,True)
            assert r.metadata()["control_mode"]=="tilt"
            r.pointer(1,0,0,False)
            r.imu(0,0,1);step()
            for _ in range(5):r.imu(.6,0,.8);step()
            assert r.metadata()["steering"]<0
            tap(*centered(400,90))
            for _ in range(20):r.imu(.6,0,.8);step()
            assert abs(r.metadata()["steering"])<.01
            tap(*mode);assert r.metadata()["control_mode"]=="keyboard"
            assert not r.metadata()["auto_throttle"]
            r.control(3);assert r.metadata()["control_mode"]=="keyboard"
            step(100);assert r.metadata()["speed"]==0
            r.action(2,True);step(10);assert r.metadata()["speed"]>0
            r.action(2,False)
            r.action(10,True);assert r.metadata()["control_mode"]=="touch"
            r.action(11,True);assert r.metadata()["control_mode"]=="tilt"
            r.action(12,True);assert r.metadata()["control_mode"]=="keyboard"
        pixels,w,h=r.snapshot_rgb565()
        assert (w,h)==tuple(map(int,size.split("x")))
        capture=os.environ.get("RLE_UX_CAPTURE_DIR")
        if capture:
            from PIL import Image
            values=array.array("H",pixels)
            rgb=bytes(c for v in values for c in (((v>>11)&31)*255//31,((v>>5)&63)*255//63,(v&31)*255//31))
            out=Path(capture);out.mkdir(parents=True,exist_ok=True)
            Image.frombytes("RGB",(w,h),rgb).save(out/(name+"-"+size+".png"))
        print(json.dumps(r.metadata()))
    finally:r.close()
'''


class GameProductSurfaceTests(unittest.TestCase):
    def test_player_controls_at_both_device_sizes(self):
        for game in ("last_zone_extraction","sky_hop","neon_rift_rally"):
            for size in ("480x480","320x240"):
                with self.subTest(game=game,size=size):
                    result=subprocess.run([sys.executable,"-c",SCRIPT,game,size],cwd=ROOT,
                                          text=True,capture_output=True,timeout=120)
                    self.assertEqual(result.returncode,0,result.stdout+result.stderr)


if __name__=="__main__":unittest.main()
