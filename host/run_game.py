#!/usr/bin/env python3
"""Deterministic RGB565 Host simulator for Raylib Lite game projects."""
from __future__ import annotations

import argparse
import ctypes
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import io
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import threading
import time

from PIL import Image


GENERIC_EVENT_TYPES = {
    "tap", "pause", "resume", "step", "reset", "pointer", "action", "imu",
}

ENGINE_ROOT = Path(__file__).resolve().parents[1]


def _host_compiler() -> str:
    compiler = next((value for name in ("cc", "gcc", "clang")
                     if (value := shutil.which(name))), None)
    if compiler is None:
        raise RuntimeError("no C compiler found (tried cc, gcc, and clang)")
    return compiler


class HostGameDescriptor(ctypes.Structure):
    _fields_ = [("abi_version", ctypes.c_uint32), ("game_id", ctypes.c_char_p),
                ("title", ctypes.c_char_p), ("width", ctypes.c_uint16),
                ("height", ctypes.c_uint16), ("tick_hz", ctypes.c_uint16),
                ("max_pointers", ctypes.c_uint16)]

class HostGameInput(ctypes.Structure):
    _fields_ = [("type", ctypes.c_uint32), ("code", ctypes.c_int32),
                ("x", ctypes.c_int32), ("y", ctypes.c_int32),
                ("track_id", ctypes.c_int32), ("pressed", ctypes.c_bool),
                ("value_x", ctypes.c_float), ("value_y", ctypes.c_float),
                ("value_z", ctypes.c_float)]

class RasterStats(ctypes.Structure):
    _fields_ = [
        ("opaque_copy_calls", ctypes.c_uint32),
        ("opaque_copy_pixels", ctypes.c_uint32),
        ("opaque_scale_calls", ctypes.c_uint32),
        ("opaque_scale_pixels", ctypes.c_uint32),
        ("binary_alpha_calls", ctypes.c_uint32),
        ("binary_alpha_pixels", ctypes.c_uint32),
        ("binary_copy_calls", ctypes.c_uint32),
        ("binary_copy_pixels", ctypes.c_uint32),
        ("binary_scale_calls", ctypes.c_uint32),
        ("binary_scale_pixels", ctypes.c_uint32),
        ("tile_row_calls", ctypes.c_uint32),
        ("tile_row_pixels", ctypes.c_uint32),
        ("alpha_calls", ctypes.c_uint32),
        ("alpha_pixels", ctypes.c_uint32),
        ("rotated_calls", ctypes.c_uint32),
        ("rotated_pixels", ctypes.c_uint32),
        ("frame_lookup_hits", ctypes.c_uint32),
        ("frame_lookup_misses", ctypes.c_uint32),
        ("column_calls", ctypes.c_uint32),
        ("column_pixels", ctypes.c_uint32),
        ("span_calls", ctypes.c_uint32),
        ("span_pixels", ctypes.c_uint32),
        ("sky_us", ctypes.c_uint32),
        ("floor_us", ctypes.c_uint32),
        ("wall_us", ctypes.c_uint32),
        ("enemy_us", ctypes.c_uint32),
        ("hud_us", ctypes.c_uint32),
    ]

def load_replay(path: Path | None) -> list[dict[str, object]]:
    if path is None:
        return []
    value = json.loads(path.read_text(encoding="utf-8"))
    events = value.get("events") if isinstance(value, dict) else value
    if not isinstance(events, list):
        raise ValueError("replay must be an event array or an object containing events")
    normalized = []
    previous = -1
    for item in events:
        if not isinstance(item, dict) or item.get("type") not in GENERIC_EVENT_TYPES:
            raise ValueError("replay contains an unsupported event")
        frame = int(item.get("frame", -1))
        if frame < previous or frame < 0:
            raise ValueError("replay events must use non-decreasing non-negative frames")
        previous = frame
        normalized.append({
            "frame": frame, "type": str(item["type"]),
            "code": item.get("code", item.get("action", 0)),
            "pressed": bool(item.get("pressed", True)),
            "track_id": int(item.get("track_id", item.get("track", 0))),
            "x": float(item.get("x", 0)), "y": float(item.get("y", 0)),
            "z": float(item.get("z", 0)),
        })
    return normalized

def _rgb565_png_bytes(framebuffer: object, width: int, height: int) -> bytes:
    # Pillow's raw decoder performs the RGB565 expansion in native code. The
    # previous Python pixel loop cost 120-200 ms for a 480x480 frame and held
    # the simulation lock long enough to stall the fixed 30 Hz game clock.
    pixels = ctypes.string_at(ctypes.addressof(framebuffer), width * height * 2)
    image = Image.frombytes("RGB", (width, height), pixels, "raw", "BGR;16")
    output = io.BytesIO()
    image.save(output, format="PNG", compress_level=1)
    return output.getvalue()

def prepare_project_assets(project: Path) -> None:
    """Build deterministic Host assets before compiling the game module."""
    def run(command: list[str]) -> None:
        result = subprocess.run(command, cwd=project, capture_output=True,
                                text=True)
        if result.returncode:
            details = (result.stdout or "") + (result.stderr or "")
            raise RuntimeError(
                f"asset preparation failed ({result.returncode}):\n{details}"
            )

    assets = project / "assets_src"
    hooks = [*sorted(assets.glob("prepare_*.py")),
             *sorted(assets.glob("generate_*.py"))]
    for hook in hooks:
        run([sys.executable, str(hook)])
    manifest = assets / "game_assets.json"
    if manifest.is_file():
        packer = ENGINE_ROOT / "tools/pack_game_assets.py"
        run([
            sys.executable, str(packer), "--source", str(assets),
            "--output", str(project / "assets/generated"),
        ])

class GenericHostRuntime:
    """Versioned C module; Python never mirrors project-owned game structs."""
    def __init__(self, project: Path, directory: Path, generation: int = 0) -> None:
        manifest_path = project / "game.sim.json"
        suffix = ".dll" if os.name == "nt" else ".so"
        library = directory / f"host_game_{generation}{suffix}"
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
        if manifest.get("schema") != "mosaico-game-sim/v1":
            raise RuntimeError(f"unsupported simulator manifest: {manifest_path}")
        project_sources = []
        for value in manifest.get("sources", []):
            source = (project / str(value)).resolve()
            try:
                source.relative_to(project.resolve())
            except ValueError as error:
                raise RuntimeError(f"simulator source leaves project: {value}") from error
            if not source.is_file():
                raise RuntimeError(f"simulator source is missing: {source}")
            project_sources.append(source)
        if not project_sources:
            raise RuntimeError(f"simulator manifest has no sources: {manifest_path}")
        sources = [
            *project_sources,
            ENGINE_ROOT / "host/host_module_bridge.c",
            ENGINE_ROOT / "host/host_raylib_port.c",
            ENGINE_ROOT / "host/host_asset_runtime.c",
            ENGINE_ROOT / "components/mosaico_game_2d/mosaico_game_2d.c",
            ENGINE_ROOT / "components/mosaico_raylib_fast/mosaico_raylib_fast.c",
            ENGINE_ROOT / "components/mosaico_game_fx/mosaico_game_fx.c",
            ENGINE_ROOT / "components/mosaico_game_tilemap/mosaico_game_tilemap.c",
        ]
        includes = [ENGINE_ROOT / "host/include", ENGINE_ROOT / "host",
                    ENGINE_ROOT / "components/mosaico_game_assets/include",
                    ENGINE_ROOT / "components/mosaico_game_2d/include",
                    ENGINE_ROOT / "components/mosaico_raylib_fast/include",
                    ENGINE_ROOT / "components/mosaico_game_fx/include",
                    ENGINE_ROOT / "components/mosaico_game_tilemap/include",
                    project / "main", project / "assets/generated",
                    project / "managed_components/georgik__raylib/include",
                    project / "managed_components/georgik__raylib/raylib/src"]
        command = [_host_compiler(), "-shared", "-O2", "-std=c11", "-Wall",
                   "-Wextra", "-Werror", "-DMOSAICO_HOST_SIMULATION=1",
                   *(str(path) for path in sources)]
        if os.name != "nt":
            command.insert(2, "-fPIC")
        for include in includes: command.extend(("-I", str(include)))
        command.extend(("-lm", "-o", str(library)))
        compiled = subprocess.run(command, capture_output=True, text=True)
        if compiled.returncode:
            raise RuntimeError(
                "host module compilation failed:\n" +
                (compiled.stdout or "") + (compiled.stderr or "")
            )
        self.api = ctypes.CDLL(str(library))
        self.api.mosaico_host_game_v1.restype = ctypes.POINTER(HostGameDescriptor)
        descriptor = self.api.mosaico_host_game_v1().contents
        if descriptor.abi_version != 1:
            raise RuntimeError(f"unsupported host game ABI {descriptor.abi_version}")
        self.descriptor = descriptor
        self.api.mosaico_host_game_create_v1.argtypes = [ctypes.c_char_p]
        self.api.mosaico_host_game_create_v1.restype = ctypes.c_void_p
        self.api.mosaico_host_game_destroy_v1.argtypes = [ctypes.c_void_p]
        self.api.mosaico_host_game_input_v1.argtypes = [ctypes.c_void_p,
                                                        ctypes.POINTER(HostGameInput)]
        self.api.mosaico_host_game_update_v1.argtypes = [ctypes.c_void_p]
        self.api.mosaico_host_game_render_rgb565_v1.argtypes = [ctypes.c_void_p,
            ctypes.POINTER(ctypes.c_uint16), ctypes.c_size_t]
        self.api.mosaico_host_game_state_json_v1.argtypes = [ctypes.c_void_p,
            ctypes.c_char_p, ctypes.c_size_t]
        self.api.mosaico_game_2d_get_raster_stats.argtypes = [ctypes.POINTER(RasterStats)]
        self.api.mosaico_game_2d_reset_raster_stats.argtypes = []
        self.context = self.api.mosaico_host_game_create_v1(
            str(project / "assets/generated").encode())
        if not self.context: raise RuntimeError("host adapter create failed")
        self.framebuffer = (ctypes.c_uint16 * (descriptor.width * descriptor.height))()
        self.frames = 0
        self.render_count = 0
        self.render_ns = 0
        self.encode_ns = 0
        self.last_render_ns = 0
        self.last_encode_ns = 0
        self.api.mosaico_game_2d_reset_raster_stats()
        self.lock = threading.Lock()

    def close(self) -> None:
        if self.context:
            self.api.mosaico_host_game_destroy_v1(self.context)
            self.context = None
        if os.name == "nt" and self.api is not None:
            handle = self.api._handle
            self.api = None
            ctypes.windll.kernel32.FreeLibrary(ctypes.c_void_p(handle))

    def _input(self, kind: int, code: int, pressed: bool = True,
               x: int = 0, y: int = 0, track_id: int = 0,
               value_x: float = 0, value_y: float = 0,
               value_z: float = 0) -> None:
        value = HostGameInput(kind, code, x, y, track_id, pressed,
                              value_x, value_y, value_z)
        self.api.mosaico_host_game_input_v1(self.context, ctypes.byref(value))

    def step(self, left: bool, right: bool, jump: bool,
             restart: bool = False, pause: bool = False) -> None:
        for code, pressed in ((0,left),(1,right),(2,jump)):
            self._input(1, code, pressed)
        if restart: self._input(1, 4)
        if pause: self._input(1, 3)
        self.api.mosaico_host_game_update_v1(self.context); self.frames += 1

    def control(self, code: int) -> None: self._input(3, code)
    def action(self, code: int, pressed: bool) -> None:
        self._input(1, code, pressed)
    def pointer(self, track_id: int, x: int, y: int, pressed: bool) -> None:
        self._input(2, 0, pressed, x, y, track_id)
    def imu(self, x: float, y: float, z: float) -> None:
        self._input(4, 0, True, value_x=x, value_y=y, value_z=z)
    def metadata(self) -> dict[str, object]:
        output = ctypes.create_string_buffer(2048)
        if self.api.mosaico_host_game_state_json_v1(self.context, output, len(output)) < 0:
            return {"error": "state unavailable"}
        value = json.loads(output.value)
        raster = RasterStats()
        self.api.mosaico_game_2d_get_raster_stats(ctypes.byref(raster))
        value.update({"frames": self.frames, "abi": 1,
                      "game_id": self.descriptor.game_id.decode(),
                      "title": self.descriptor.title.decode(),
                      "width": self.descriptor.width,
                      "height": self.descriptor.height,
                      "tick_hz": self.descriptor.tick_hz,
                      "max_pointers": self.descriptor.max_pointers,
                      "host_render_ms": self.last_render_ns / 1_000_000.0,
                      "host_render_mean_ms": self.render_ns /
                          max(1, self.render_count) / 1_000_000.0,
                      "host_encode_ms": self.last_encode_ns / 1_000_000.0,
                      "raster": {name: getattr(raster, name)
                          for name, _ctype in RasterStats._fields_}})
        return value
    def frame(self) -> bytes:
        started = time.perf_counter_ns()
        status = self.api.mosaico_host_game_render_rgb565_v1(
            self.context, self.framebuffer, self.descriptor.width)
        if status: raise RuntimeError(f"host render failed: {status}")
        rendered = time.perf_counter_ns()
        frame = _rgb565_png_bytes(self.framebuffer, self.descriptor.width,
                                  self.descriptor.height)
        encoded = time.perf_counter_ns()
        self.last_render_ns = rendered - started
        self.last_encode_ns = encoded - rendered
        self.render_ns += self.last_render_ns
        self.encode_ns += self.last_encode_ns
        self.render_count += 1
        return frame

class ReloadableHostRuntime:
    def __init__(self, project: Path, directory: Path) -> None:
        self.project, self.directory = project, directory
        self.current = GenericHostRuntime(project, directory)
        self.lock = threading.RLock()
        self.generation = 0
        self.reload_error = ""
        self.stamp = self._stamp()

    def _watched(self) -> list[Path]:
        assets = self.project / "assets_src"
        prepare = assets / "prepare_sprites.py"
        images = (assets.glob("*source*.png") if prepare.is_file()
                  else assets.glob("*.png"))
        return [*self.project.joinpath("main").glob("*.[ch]"),
                self.project / "game.sim.json", *images,
                *assets.glob("*.json"), prepare]
    def _stamp(self) -> int:
        return max((path.stat().st_mtime_ns for path in self._watched()
                    if path.is_file()), default=0)
    def _reload(self) -> None:
        stamp = self._stamp()
        if stamp == self.stamp: return
        try:
            prepare_project_assets(self.project)
            self.generation += 1
            replacement = GenericHostRuntime(self.project, self.directory, self.generation)
            previous = self.current
            self.current = replacement
            previous.close()
            self.reload_error = ""
        except (OSError, subprocess.CalledProcessError, RuntimeError) as error:
            self.reload_error = str(error)
        # Asset preparation may rewrite a watched PNG. Record the resulting
        # source state, otherwise every simulation tick starts another build.
        self.stamp = self._stamp()
    def step(self, *args: object, **kwargs: object) -> None:
        self._reload(); self.current.step(*args, **kwargs)
    def control(self, code: int) -> None: self.current.control(code)
    def action(self, code: int, pressed: bool) -> None:
        self.current.action(code, pressed)
    def pointer(self, *args: object) -> None: self.current.pointer(*args)
    def imu(self, *args: object) -> None: self.current.imu(*args)
    def frame(self) -> bytes: return self.current.frame()
    def metadata(self) -> dict[str, object]:
        return {**self.current.metadata(), "reload_error": self.reload_error,
                "reload_generation": self.generation}

def run_generic(project: Path, directory: Path, frames: int, output: Path,
                replay: list[dict[str, object]] | None = None) -> dict[str, object]:
    runtime = GenericHostRuntime(project, directory)
    events = replay or []
    next_event = 0
    paused = False
    actions = {"left": False, "right": False, "jump": False}
    if not replay:
        runtime.action(4, True)
        runtime.action(4, False)
    for frame_index in range(max(0, frames)):
        tap_releases: list[tuple[int, int, int]] = []
        single_step = False
        while next_event < len(events) and int(events[next_event]["frame"]) == frame_index:
            event = events[next_event]
            kind = str(event["type"])
            if kind == "tap":
                track = int(event["track_id"]); x = int(float(event["x"])); y = int(float(event["y"]))
                runtime.pointer(track, x, y, True)
                tap_releases.append((track, x, y))
            elif kind == "pointer":
                runtime.pointer(int(event["track_id"]), int(float(event["x"])),
                                int(float(event["y"])), bool(event["pressed"]))
            elif kind == "action":
                codes = {"left": 0, "right": 1, "jump": 2,
                         "pause": 3, "restart": 4}
                raw_code = event["code"]
                code = codes.get(str(raw_code), int(raw_code) if str(raw_code).lstrip("-").isdigit() else -1)
                if code < 0: raise ValueError(f"unsupported action code: {raw_code}")
                pressed = bool(event["pressed"])
                if code < 3:
                    actions[("left", "right", "jump")[code]] = pressed
                else:
                    runtime.action(code, pressed)
            elif kind == "imu":
                runtime.imu(float(event["x"]), float(event["y"]), float(event["z"]))
            elif kind == "pause":
                runtime.control(1); paused = True
            elif kind == "resume":
                runtime.control(2); paused = False
            elif kind == "reset":
                runtime.control(3)
            elif kind == "step":
                single_step = True
            next_event += 1
        if single_step and paused:
            runtime.control(2)
            runtime.step(actions["left"], actions["right"], actions["jump"])
            runtime.control(1)
        else:
            runtime.step(actions["left"], actions["right"], actions["jump"])
        for track, x, y in tap_releases:
            runtime.pointer(track, x, y, False)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(runtime.frame())
    result = {**runtime.metadata(), "frame": str(output)}
    runtime.close()
    return result

def serve_interactive_preview(listen: str, port: int, runtime: object) -> None:
    simulation = {"paused": False, "speed": 1.0,
                  "actions": {"left": False, "right": False, "jump": False,
                              "back": False, "fire": False, "sprint": False,
                              "strafe_left": False, "strafe_right": False},
                  "pointers": {},
                  "recording": False, "events": [], "started": time.monotonic(),
                  "ticks": 0}
    tick_hz = max(1, int(runtime.metadata().get("tick_hz", 30)))
    def simulation_loop() -> None:
        deadline = time.monotonic()
        while True:
            if simulation["paused"]:
                time.sleep(.01); deadline = time.monotonic(); continue
            with runtime.lock:
                actions = simulation["actions"]
                runtime.step(actions["left"], actions["right"], actions["jump"])
                simulation["ticks"] += 1
            deadline += 1.0 / (tick_hz * float(simulation["speed"]))
            time.sleep(max(0.0, deadline - time.monotonic()))
    threading.Thread(target=simulation_loop, daemon=True).start()
    page = """<!doctype html><html><head><meta charset=utf-8>
<meta name=viewport content="width=device-width,initial-scale=1,user-scalable=no">
<title>Mosaico game simulator</title><style>
body{margin:0;background:#07111c;color:#dff;font:14px system-ui;display:grid;place-items:center;min-height:100vh}
main{position:relative;padding:16px;background:#0c2030;border:1px solid #299fad;border-radius:16px;box-shadow:0 18px 80px #000;width:min(512px,calc(100vw - 24px))}
img{width:480px;height:480px;max-width:100%;aspect-ratio:1/1;object-fit:contain;display:block;image-rendering:pixelated;touch-action:none}
#state{margin-top:10px;color:#9ee;white-space:pre-wrap;height:4.8em;overflow:hidden;line-height:1.35}
.hint{color:#fff;margin-top:8px}.tools{display:flex;gap:6px;flex-wrap:wrap;margin-bottom:10px}button,select{background:#17364b;color:#dff;border:1px solid #299fad;border-radius:6px;padding:6px 10px}
</style></head><body><main><div class=tools><button id=pause>Pause</button><button id=step>Step</button><button id=reset>Reset</button><button id=shot>Screenshot</button><button id=record>Record</button><select id=speed><option>.25</option><option>.5</option><option selected>1</option><option>2</option></select></div><img id=screen tabindex=0 draggable=false><div id=state></div>
<div class=hint>Keyboard: A/D turn, W/S walk, Shift sprint, Q/E strafe, F fire · Touch: left stick, right look, FIRE ring</div></main>
<script>
const held=new Set(), pointers=new Map(), img=document.querySelector('#screen'), state=document.querySelector('#state'), sfxCache={};
let busy=false, phase='start',paused=false,armedSfx='';
function playSfx(name){if(!name)return;let a=sfxCache[name];if(!a){a=new Audio('/sfx/'+name+'.wav');a.preload='auto';sfxCache[name]=a}a.currentTime=0;a.volume=name.startsWith('step')?.35:.7;a.play().catch(()=>{})}
function unlockAudio(){if(sfxCache._on)return;sfxCache._on=1;['rifle','step_l','alert','hurt','confirm'].forEach(n=>{const a=new Audio('/sfx/'+n+'.wav');a.preload='auto';sfxCache[n]=a})}
function key(e,down){unlockAudio();const k=e.key.toLowerCase();if(['arrowleft','arrowright','arrowup','arrowdown',' ','a','d','w','s','f','control','p','enter','shift','q','e'].includes(k))e.preventDefault();
 if(down&&!held.has(k)&&k==='p')control(paused?'resume':'pause');
 if(down&&!held.has(k)&&k==='enter')control('continue'); down?held.add(k):held.delete(k);sendInput()}
addEventListener('keydown',e=>key(e,true));addEventListener('keyup',e=>key(e,false));
function pointer(e,down){e.preventDefault();const r=img.getBoundingClientRect();
 const p={x:(e.clientX-r.left)*480/r.width,y:(e.clientY-r.top)*480/r.height};down?pointers.set(e.pointerId,p):pointers.delete(e.pointerId)}
img.onpointerdown=e=>{unlockAudio();img.focus();img.setPointerCapture(e.pointerId);pointer(e,true);sendInput()};
img.onpointermove=e=>{if(pointers.has(e.pointerId)){pointer(e,true);sendInput()}};
img.onpointerup=img.onpointercancel=e=>{pointer(e,false);sendInput()};
addEventListener('blur',()=>{held.clear();pointers.clear()});
async function sendInput(){let left=held.has('a')||held.has('arrowleft'),right=held.has('d')||held.has('arrowright'),jump=held.has(' ')||held.has('w')||held.has('arrowup'),back=held.has('s')||held.has('arrowdown'),fire=held.has('f')||held.has('control'),sprint=held.has('shift'),strafe_left=held.has('q'),strafe_right=held.has('e');
 await fetch('/api/v1/input',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({left,right,jump,back,fire,sprint,strafe_left,strafe_right,pointers:[...pointers].map(([track,p])=>({track,...p,pressed:true}))})})}
async function control(command){await fetch('/api/v1/control',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({command})})}
async function tick(){if(busy)return;busy=true;
 try{const res=await fetch('/api/v1/frame');const meta=JSON.parse(res.headers.get('X-Mosaico-State'));const blob=await res.blob();
 phase=meta.phase;if(meta.sfx&&meta.sfx!==armedSfx)playSfx(meta.sfx);armedSfx=meta.sfx||'';const old=img.src,url=URL.createObjectURL(blob);img.onload=()=>{if(old.startsWith('blob:'))URL.revokeObjectURL(old);img.onload=null};img.src=url;
 paused=meta.simulation.paused;const fields=Object.entries(meta).filter(([k])=>!['simulation','reload_error','title'].includes(k)).map(([k,v])=>`${k}=${v}`).join('  ');
 state.textContent=`${meta.title||meta.game_id||'Mosaico game'}\nLogic ${meta.simulation.logic_fps.toFixed(1)} Hz  Raster ${meta.host_render_ms.toFixed(2)} ms  PNG ${meta.host_encode_ms.toFixed(2)} ms\n${fields}`}
 finally{busy=false}}
pause.onclick=()=>control(paused?'resume':'pause');step.onclick=()=>control('step');reset.onclick=()=>control('reset');speed.onchange=()=>control('speed:'+speed.value);shot.onclick=()=>{const a=document.createElement('a');a.href=img.src;a.download='mosaico-game.png';a.click()};record.onclick=async()=>{if(!metaRecording()){await control('record');record.textContent='Stop record'}else{await control('record');const a=document.createElement('a');a.href='/api/v1/recording';a.download='scenario.json';a.click();record.textContent='Record'}};
function metaRecording(){return record.textContent==='Stop record'}
let lastFrame=0;function animate(now){if(now-lastFrame>=32){lastFrame=now;tick()}requestAnimationFrame(animate)}requestAnimationFrame(animate);
</script></body></html>""".encode("utf-8")
    class Handler(BaseHTTPRequestHandler):
        def metadata(self) -> dict[str, object]:
            elapsed = max(.001, time.monotonic() - float(simulation["started"]))
            return {**runtime.metadata(), "simulation": {
                "paused": simulation["paused"], "speed": simulation["speed"],
                "logic_fps": simulation["ticks"] / elapsed,
                "recording": simulation["recording"]}}
        def do_GET(self) -> None:
            if self.path.startswith("/api/v1/frame") or self.path.startswith("/frame"):
                from urllib.parse import parse_qs, urlparse
                values = parse_qs(urlparse(self.path).query)
                flag = lambda name: values.get(name, ["0"])[0] == "1"
                with runtime.lock:
                    body, metadata = runtime.frame(), self.metadata()
                content_type = "image/png"
            elif self.path.startswith("/api/v1/info"):
                body = json.dumps({"abi": 1, "endpoints": ["info","state","frame","input","control","recording"],
                    "controls": ["pause","resume","step","reset","speed","record"]}).encode()
                metadata, content_type = self.metadata(), "application/json"
            elif self.path.startswith("/api/v1/state"):
                metadata = self.metadata(); body = json.dumps(metadata).encode(); content_type="application/json"
            elif self.path.startswith("/api/v1/recording"):
                metadata = self.metadata()
                body = json.dumps({"events": simulation["events"]}, indent=2).encode()
                content_type = "application/json"
            elif self.path.startswith("/sfx/"):
                from urllib.parse import unquote
                name = Path(unquote(self.path.split("?", 1)[0])).name
                project = getattr(runtime, "project", None)
                wav = Path(project) / "assets_src" / name if project else Path()
                if (name.endswith(".wav") and name.replace("_", "").removesuffix(".wav").isalnum()
                        and wav.is_file()):
                    body, metadata, content_type = wav.read_bytes(), self.metadata(), "audio/wav"
                else:
                    self.send_error(404); return
            else:
                body, metadata, content_type = page, self.metadata(), "text/html; charset=utf-8"
            self.send_response(200)
            self.send_header("Content-Type", content_type)
            self.send_header("Content-Length", str(len(body)))
            self.send_header("Cache-Control", "no-store")
            self.send_header("X-Mosaico-State", json.dumps(metadata, separators=(",", ":")))
            self.end_headers(); self.wfile.write(body)
        def do_POST(self) -> None:
            length = min(int(self.headers.get("Content-Length", "0")), 65536)
            try: value = json.loads(self.rfile.read(length) or b"{}")
            except (ValueError, TypeError): self.send_error(400); return
            if self.path == "/api/v1/input":
                previous_actions = simulation["actions"]
                simulation["actions"] = {key: bool(value.get(key, False))
                    for key in ("left", "right", "jump", "back", "fire",
                                "sprint", "strafe_left", "strafe_right")}
                incoming = {int(item.get("track", 0)): item
                            for item in value.get("pointers", [])[:2]}
                if hasattr(runtime, "pointer"):
                    with runtime.lock:
                        for track, old in simulation["pointers"].items():
                            if track not in incoming:
                                runtime.pointer(track, int(old["x"]), int(old["y"]), False)
                        for track, point in incoming.items():
                            runtime.pointer(track, int(point["x"]), int(point["y"]), True)
                        imu = value.get("imu")
                        if isinstance(imu, dict) and hasattr(runtime, "imu"):
                            runtime.imu(float(imu.get("x", 0)), float(imu.get("y", 0)),
                                        float(imu.get("z", 0)))
                        for item in value.get("actions", [])[:16]:
                            runtime.action(int(item.get("code", 0)),
                                           bool(item.get("pressed", True)))
                        runtime.action(5, simulation["actions"]["back"])
                        runtime.action(6, simulation["actions"]["fire"])
                        runtime.action(7, simulation["actions"]["sprint"])
                        runtime.action(8, simulation["actions"]["strafe_left"])
                        runtime.action(9, simulation["actions"]["strafe_right"])
                if simulation["recording"]:
                    for name, pressed in simulation["actions"].items():
                        if pressed != previous_actions.get(name, False):
                            simulation["events"].append({"frame": simulation["ticks"],
                                "type": "action", "code": name, "pressed": pressed})
                    for track, old in simulation["pointers"].items():
                        if track not in incoming:
                            simulation["events"].append({"frame": simulation["ticks"],
                                "type": "pointer", "track": track, "x": int(old["x"]),
                                "y": int(old["y"]), "pressed": False})
                    for track, point in incoming.items():
                        old = simulation["pointers"].get(track)
                        if old != point:
                            simulation["events"].append({"frame": simulation["ticks"],
                                "type": "pointer", "track": track, "x": int(point["x"]),
                                "y": int(point["y"]), "pressed": True})
                    if isinstance(value.get("imu"), dict):
                        simulation["events"].append({"frame": simulation["ticks"],
                            "type": "imu", **value["imu"]})
                simulation["pointers"] = incoming
            elif self.path == "/api/v1/control":
                command = str(value.get("command", ""))
                if command == "pause": simulation["paused"] = True
                elif command == "resume": simulation["paused"] = False
                elif command == "step":
                    with runtime.lock: runtime.step(False, False, False)
                elif command == "reset":
                    with runtime.lock:
                        if hasattr(runtime, "control"): runtime.control(3)
                        else: runtime.step(False,False,False,restart=True)
                elif command == "continue":
                    with runtime.lock: runtime.step(False,False,False,restart=True)
                elif command.startswith("speed:"):
                    speed=float(command.split(":",1)[1])
                    if speed in (.25,.5,1.0,2.0): simulation["speed"] = speed
                elif command == "record":
                    if not simulation["recording"]: simulation["events"] = []
                    simulation["recording"] = not simulation["recording"]
                else: self.send_error(400); return
                if simulation["recording"] and command not in {"record"}:
                    event = command.split(":", 1)[0]
                    if event in {"pause", "resume", "step", "reset"}:
                        simulation["events"].append({"frame": simulation["ticks"],
                                                     "type": event})
            else: self.send_error(404); return
            body=json.dumps(self.metadata()).encode();self.send_response(200)
            self.send_header("Content-Type","application/json");self.send_header("Content-Length",str(len(body)))
            self.end_headers();self.wfile.write(body)
        def log_message(self, fmt: str, *args: object) -> None:
            return
    print(json.dumps({"preview_url": f"http://{listen}:{port}/", **runtime.metadata()}), flush=True)
    ThreadingHTTPServer((listen, port), Handler).serve_forever()

def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--project", type=Path, required=True)
    parser.add_argument("--headless", action="store_true")
    parser.add_argument("--frames", type=int, default=300)
    parser.add_argument("--listen", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=8460)
    parser.add_argument("--replay", type=Path)
    parser.add_argument("--state-output", type=Path)
    args = parser.parse_args()
    manifest = args.project / "game.sim.json"
    if not manifest.is_file():
        parser.error(f"game simulator manifest not found: {manifest}")
    prepare_project_assets(args.project)
    with tempfile.TemporaryDirectory(prefix="mosaico-game-") as directory:
        output = args.project / "build-host" / "frame.png"
        if args.headless:
            result = run_generic(args.project, Path(directory), args.frames,
                                 output, load_replay(args.replay))
            if args.state_output:
                args.state_output.parent.mkdir(parents=True, exist_ok=True)
                args.state_output.write_text(
                    json.dumps(result, indent=2, sort_keys=True) + "\n",
                    encoding="utf-8",
                )
            print(json.dumps(result))
        else:
            runtime = ReloadableHostRuntime(args.project, Path(directory))
            try:
                serve_interactive_preview(args.listen, args.port, runtime)
            except KeyboardInterrupt:
                pass
        return 0


if __name__ == "__main__":
    raise SystemExit(main())
