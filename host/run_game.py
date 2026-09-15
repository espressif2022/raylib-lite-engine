#!/usr/bin/env python3
"""Deterministic host smoke runner for Mosaico game projects.

The browser transport is deliberately kept outside game code. The initial
runner validates the shared C game model and emits a deterministic RGB image;
the GSP Canvas simulator transport can replace this presenter without changing
the project ABI.
"""
from __future__ import annotations

import argparse
import ctypes
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import io
import json
from pathlib import Path
import struct
import subprocess
import tempfile
import threading
import time
import zlib

from PIL import Image, ImageDraw, ImageFont


MAX_BULLETS = 64
MAX_ENEMIES = 32
TOWER_MAX_ENEMIES = 48
TOWER_MAX_PROJECTILES = 64
TOWER_PAD_COUNT = 9
SKY_MAX_COINS = 10
SKY_MAX_ENEMIES = 4

class Actor(ctypes.Structure):
    _fields_ = [("x", ctypes.c_float), ("y", ctypes.c_float),
                ("vx", ctypes.c_float), ("vy", ctypes.c_float),
                ("kind", ctypes.c_uint8), ("active", ctypes.c_bool)]

class Game(ctypes.Structure):
    _fields_ = [("phase", ctypes.c_int), ("player", Actor),
                ("bullets", Actor * MAX_BULLETS),
                ("enemies", Actor * MAX_ENEMIES),
                ("score", ctypes.c_uint32), ("tick", ctypes.c_uint32),
                ("rng", ctypes.c_uint32), ("fire_cooldown", ctypes.c_uint16),
                ("spawn_cooldown", ctypes.c_uint16), ("lives", ctypes.c_uint8)]

class TowerEnemy(ctypes.Structure):
    _fields_ = [("x", ctypes.c_float), ("y", ctypes.c_float),
                ("hp", ctypes.c_float), ("max_hp", ctypes.c_float),
                ("speed", ctypes.c_float), ("waypoint", ctypes.c_uint16),
                ("kind", ctypes.c_uint8), ("slow_ticks", ctypes.c_uint8),
                ("active", ctypes.c_bool)]

class TowerProjectile(ctypes.Structure):
    _fields_ = [("x", ctypes.c_float), ("y", ctypes.c_float),
                ("vx", ctypes.c_float), ("vy", ctypes.c_float),
                ("target", ctypes.c_uint16), ("damage", ctypes.c_uint8),
                ("kind", ctypes.c_uint8), ("ttl", ctypes.c_uint8),
                ("active", ctypes.c_bool)]

class TowerSlot(ctypes.Structure):
    _fields_ = [("x", ctypes.c_int16), ("y", ctypes.c_int16),
                ("cooldown", ctypes.c_uint16), ("type", ctypes.c_uint8),
                ("level", ctypes.c_uint8), ("occupied", ctypes.c_bool)]

class TowerGame(ctypes.Structure):
    _fields_ = [("phase", ctypes.c_int),
                ("enemies", TowerEnemy * TOWER_MAX_ENEMIES),
                ("projectiles", TowerProjectile * TOWER_MAX_PROJECTILES),
                ("towers", TowerSlot * TOWER_PAD_COUNT),
                ("tick", ctypes.c_uint32), ("rng", ctypes.c_uint32),
                ("score", ctypes.c_uint32), ("kills", ctypes.c_uint32),
                ("shots", ctypes.c_uint32), ("credits", ctypes.c_uint16),
                ("wave", ctypes.c_uint16), ("wave_spawned", ctypes.c_uint16),
                ("wave_total", ctypes.c_uint16),
                ("spawn_cooldown", ctypes.c_uint16),
                ("intermission", ctypes.c_uint16), ("base_hp", ctypes.c_uint8),
                ("selected_type", ctypes.c_uint8),
                ("pointer_down", ctypes.c_bool)]

class HostResult(ctypes.Structure):
    _fields_ = [("frames", ctypes.c_uint32), ("wave", ctypes.c_uint32),
                ("score", ctypes.c_uint32), ("credits", ctypes.c_uint32),
                ("base_hp", ctypes.c_uint32), ("kills", ctypes.c_uint32),
                ("state_hash", ctypes.c_uint32)]

class HostEvent(ctypes.Structure):
    _fields_ = [("frame", ctypes.c_uint32), ("type", ctypes.c_uint8),
                ("x", ctypes.c_int16), ("y", ctypes.c_int16)]

EVENT_TYPES = {"tap": 1, "pause": 2, "resume": 3, "step": 4, "reset": 5}
GENERIC_EVENT_TYPES = {*EVENT_TYPES, "pointer", "action", "imu"}

class SkyCoin(ctypes.Structure):
    _fields_ = [("x", ctypes.c_float), ("y", ctypes.c_float),
                ("collected", ctypes.c_bool)]

class SkyEnemy(ctypes.Structure):
    _fields_ = [("x", ctypes.c_float), ("y", ctypes.c_float),
                ("left", ctypes.c_float), ("right", ctypes.c_float),
                ("speed", ctypes.c_float), ("active", ctypes.c_bool)]

class SkyGame(ctypes.Structure):
    _fields_ = [("phase", ctypes.c_int),
                ("player_x", ctypes.c_float), ("player_y", ctypes.c_float),
                ("velocity_x", ctypes.c_float), ("velocity_y", ctypes.c_float),
                ("camera_x", ctypes.c_float),
                ("move_left", ctypes.c_bool), ("move_right", ctypes.c_bool),
                ("jump_held", ctypes.c_bool), ("grounded", ctypes.c_bool),
                ("tick", ctypes.c_uint32), ("phase_tick", ctypes.c_uint32),
                ("score", ctypes.c_uint16), ("lives", ctypes.c_uint8),
                ("level", ctypes.c_uint8), ("coin_count", ctypes.c_uint8),
                ("enemy_count", ctypes.c_uint8),
                ("coins", SkyCoin * SKY_MAX_COINS),
                ("enemies", SkyEnemy * SKY_MAX_ENEMIES)]

class SkyBlock(ctypes.Structure):
    _fields_ = [("x", ctypes.c_float), ("y", ctypes.c_float),
                ("width", ctypes.c_float), ("height", ctypes.c_float)]

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

def rgb565_png(path: Path, framebuffer: ctypes.Array[ctypes.c_uint16]) -> None:
    width = height = 480
    pixels = bytearray(width * height * 3)
    for index, value in enumerate(framebuffer):
        pixels[index * 3] = ((value >> 11) & 31) * 255 // 31
        pixels[index * 3 + 1] = ((value >> 5) & 63) * 255 // 63
        pixels[index * 3 + 2] = (value & 31) * 255 // 31
    def chunk(kind: bytes, data: bytes) -> bytes:
        return (struct.pack(">I", len(data)) + kind + data +
                struct.pack(">I", zlib.crc32(kind + data) & 0xffffffff))
    rows = b"".join(b"\0" + pixels[y * width * 3:(y + 1) * width * 3]
                    for y in range(height))
    path.write_bytes(b"\x89PNG\r\n\x1a\n" +
        chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)) +
        chunk(b"IDAT", zlib.compress(rows, 9)) + chunk(b"IEND", b""))

def png(path: Path, game: Game) -> None:
    width = height = 480
    pixels = bytearray(bytes((5, 10, 28)) * width * height)
    def rect(x: int, y: int, w: int, h: int, color: tuple[int, int, int]) -> None:
        for py in range(max(0, y), min(height, y + h)):
            for px in range(max(0, x), min(width, x + w)):
                offset = (py * width + px) * 3
                pixels[offset:offset + 3] = bytes(color)
    rect(int(game.player.x), int(game.player.y), 36, 36, (80, 190, 255))
    for actor in game.bullets:
        if actor.active: rect(int(actor.x), int(actor.y), 6, 12, (255, 210, 50))
    colors = ((255, 140, 40), (255, 50, 190), (70, 230, 110))
    for actor in game.enemies:
        if actor.active: rect(int(actor.x), int(actor.y), 28, 28, colors[actor.kind])
    def chunk(kind: bytes, data: bytes) -> bytes:
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xffffffff)
    rows = b"".join(b"\0" + pixels[y * width * 3:(y + 1) * width * 3] for y in range(height))
    path.write_bytes(b"\x89PNG\r\n\x1a\n" +
        chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)) +
        chunk(b"IDAT", zlib.compress(rows, 9)) + chunk(b"IEND", b""))

def tower_png(path: Path, game: TowerGame) -> None:
    width = height = 480
    pixels = bytearray(bytes((12, 30, 35)) * width * height)
    def rect(x: int, y: int, w: int, h: int, color: tuple[int, int, int]) -> None:
        for py in range(max(0, y), min(height, y + h)):
            start = (py * width + max(0, x)) * 3
            count = max(0, min(width, x + w) - max(0, x))
            pixels[start:start + count*3] = bytes(color) * count
    rect(0, 0, 480, 69, (5, 10, 20))
    rect(0, 65, 480, 4, (13, 72, 83))
    for x, y, w, h in ((0, 82, 149, 44), (104, 82, 44, 131),
                        (104, 168, 273, 44), (332, 168, 44, 133),
                        (72, 256, 305, 44), (72, 256, 44, 127),
                        (72, 338, 408, 44)):
        rect(x, y, w, h, (18, 70, 78))
    for x, y, w, h in ((0, 91, 139, 26), (113, 91, 26, 112),
                        (113, 177, 254, 26), (341, 177, 26, 114),
                        (81, 265, 286, 26), (81, 265, 26, 108),
                        (81, 347, 399, 26)):
        rect(x, y, w, h, (32, 42, 52))
    for x in range(0, 470, 26):
        for y in (101, 187, 275, 357): rect(x, y, 10, 2, (75, 196, 196))
    tower_colors = ((42, 224, 231), (255, 193, 61), (110, 145, 255))
    for tower in game.towers:
        rect(tower.x - 24, tower.y - 20, 48, 44, (3, 12, 17))
        rect(tower.x - 20, tower.y - 20, 40, 40, (15, 42, 48))
        rect(tower.x - 14, tower.y - 14, 28, 28,
             tower_colors[tower.type] if tower.occupied else (24, 63, 68))
        if tower.occupied:
            rect(tower.x - 6, tower.y - 5, 12, 10, (222, 252, 250))
    enemy_colors = ((244, 103, 74), (172, 91, 218), (239, 174, 52))
    for enemy in game.enemies:
        if enemy.active:
            size = 22 if enemy.kind == 1 else 14 if enemy.kind == 2 else 18
            rect(int(enemy.x)-size//2, int(enemy.y)-size//2, size, size,
                 (105, 180, 245) if enemy.slow_ticks else enemy_colors[enemy.kind])
    for projectile in game.projectiles:
        if projectile.active:
            color = tower_colors[projectile.kind]
            rect(int(projectile.x)-2, int(projectile.y)-2, 5, 5, color)
    rect(449, 335, 31, 48, (3, 12, 20))
    rect(454, 330, 26, 48, (21, 63, 75))
    rect(464, 344, 7, 22, (42, 224, 231))
    rect(0, 392, 480, 88, (3, 10, 16))
    for index, x in enumerate((8, 164, 320)):
        rect(x, 402, 146, 67, (19, 47, 56) if game.selected_type == index else (10, 25, 32))
        rect(x+8, 411, 35, 42, (5, 16, 24))
        rect(x+13, 419, 25, 25, tower_colors[index])
    def chunk(kind: bytes, data: bytes) -> bytes:
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xffffffff)
    rows = b"".join(b"\0" + pixels[y*width*3:(y+1)*width*3] for y in range(height))
    path.write_bytes(b"\x89PNG\r\n\x1a\n" +
        chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)) +
        chunk(b"IDAT", zlib.compress(rows, 9)) + chunk(b"IEND", b""))

def run_tower(source: Path, directory: Path, frames: int, output: Path,
              replay: list[dict[str, int | str]] | None = None) -> dict[str, object]:
    repository = Path(__file__).resolve().parents[2]
    project = source.parents[1]
    library = directory / "tower_game.so"
    sources = [
        repository / "game_sdk/host/tower_host_renderer.c",
        source,
        repository / "game_sdk/components/mosaico_game_2d/mosaico_game_2d.c",
        repository / "game_sdk/components/mosaico_game_tilemap/mosaico_game_tilemap.c",
    ]
    includes = [
        repository / "game_sdk/host/include",
        repository / "game_sdk/components/mosaico_game_assets/include",
        repository / "game_sdk/components/mosaico_game_2d/include",
        repository / "game_sdk/components/mosaico_game_tilemap/include",
        project / "main",
        project / "managed_components/georgik__raylib/include",
        project / "managed_components/georgik__raylib/raylib/src",
    ]
    if not (includes[-1] / "raylib.h").is_file():
        raise RuntimeError("host preview needs project dependencies; run 'game build' first")
    command = ["cc", "-shared", "-fPIC", "-O2", "-std=c11", "-Wall", "-Werror"]
    command.extend(str(item) for item in sources)
    for include in includes:
        command.extend(("-I", str(include)))
    command.extend(("-lm", "-o", str(library)))
    subprocess.run(command, check=True)
    api = ctypes.CDLL(str(library))
    framebuffer = (ctypes.c_uint16 * (480 * 480))()
    result = HostResult()
    api.mosaico_tower_host_render_replay.argtypes = [ctypes.c_char_p, ctypes.c_uint,
        ctypes.POINTER(HostEvent), ctypes.c_size_t, ctypes.POINTER(ctypes.c_uint16),
        ctypes.POINTER(HostResult)]
    api.mosaico_tower_host_render_replay.restype = ctypes.c_int
    assets = project / "assets/generated"
    values = replay or []
    encoded = (HostEvent * len(values))(*[
        HostEvent(int(item["frame"]), EVENT_TYPES[str(item["type"])],
                  int(item["x"]), int(item["y"])) for item in values])
    status = api.mosaico_tower_host_render_replay(str(assets).encode(), max(0, frames),
        encoded if values else None, len(values), framebuffer, ctypes.byref(result))
    if status:
        raise RuntimeError(f"shared C renderer failed: {status}")
    output.parent.mkdir(parents=True, exist_ok=True)
    rgb565_png(output, framebuffer)
    return {"frames": result.frames, "wave": result.wave, "score": result.score,
            "credits": result.credits, "base_hp": result.base_hp,
            "kills": result.kills,
            "state_hash": f"{result.state_hash:08x}",
            "frame": str(output)}

def _rgb_png_bytes(pixels: bytearray, width: int = 480, height: int = 480) -> bytes:
    def chunk(kind: bytes, data: bytes) -> bytes:
        return (struct.pack(">I", len(data)) + kind + data +
                struct.pack(">I", zlib.crc32(kind + data) & 0xffffffff))
    rows = b"".join(b"\0" + pixels[y*width*3:(y+1)*width*3]
                    for y in range(height))
    return (b"\x89PNG\r\n\x1a\n" +
            chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)) +
            chunk(b"IDAT", zlib.compress(rows, 3)) + chunk(b"IEND", b""))

def _rgb565_png_bytes(framebuffer: object, width: int, height: int) -> bytes:
    # Pillow's raw decoder performs the RGB565 expansion in native code. The
    # previous Python pixel loop cost 120-200 ms for a 480x480 frame and held
    # the simulation lock long enough to stall the fixed 30 Hz game clock.
    pixels = ctypes.string_at(ctypes.addressof(framebuffer), width * height * 2)
    image = Image.frombytes("RGB", (width, height), pixels, "raw", "BGR;16")
    output = io.BytesIO()
    image.save(output, format="PNG", compress_level=1)
    return output.getvalue()

class GenericHostRuntime:
    """Versioned C module; Python never mirrors project-owned game structs."""
    def __init__(self, project: Path, directory: Path, generation: int = 0) -> None:
        repository = Path(__file__).resolve().parents[2]
        adapter = project / "main/host_adapter.c"
        manifest_path = project / "game.sim.json"
        library = directory / f"host_game_{generation}.so"
        if manifest_path.is_file():
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
                repository / "game_sdk/host/host_module_bridge.c",
                repository / "game_sdk/host/host_raylib_port.c",
                repository / "game_sdk/host/host_asset_runtime.c",
                repository / "game_sdk/components/mosaico_game_2d/mosaico_game_2d.c",
                repository / "game_sdk/components/mosaico_raylib_fast/mosaico_raylib_fast.c",
                repository / "game_sdk/components/mosaico_game_fx/mosaico_game_fx.c",
                repository / "game_sdk/components/mosaico_game_tilemap/mosaico_game_tilemap.c",
            ]
        else:
            sources = [adapter, project / "main/platform_game.c",
                       repository / "game_sdk/host/host_asset_runtime.c",
                       repository / "game_sdk/components/mosaico_game_2d/mosaico_game_2d.c"]
        includes = [repository / "game_sdk/host/include", repository / "game_sdk/host",
                    repository / "game_sdk/components/mosaico_game_assets/include",
                    repository / "game_sdk/components/mosaico_game_2d/include",
                    repository / "game_sdk/components/mosaico_raylib_fast/include",
                    repository / "game_sdk/components/mosaico_game_fx/include",
                    repository / "game_sdk/components/mosaico_game_tilemap/include",
                    project / "main", project / "assets/generated",
                    project / "managed_components/georgik__raylib/include",
                    project / "managed_components/georgik__raylib/raylib/src"]
        command = ["cc", "-shared", "-fPIC", "-O2", "-std=c11", "-Wall",
                   "-Wextra", "-Werror", "-DMOSAICO_HOST_SIMULATION=1",
                   *(str(path) for path in sources)]
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
        self.context = self.api.mosaico_host_game_create_v1(
            str(project / "assets/generated").encode())
        if not self.context: raise RuntimeError("host adapter create failed")
        self.framebuffer = (ctypes.c_uint16 * (descriptor.width * descriptor.height))()
        self.frames = 0
        self.lock = threading.Lock()

    def close(self) -> None:
        if self.context:
            self.api.mosaico_host_game_destroy_v1(self.context)
            self.context = None

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
        value.update({"frames": self.frames, "abi": 1,
                      "game_id": self.descriptor.game_id.decode(),
                      "title": self.descriptor.title.decode(),
                      "width": self.descriptor.width,
                      "height": self.descriptor.height,
                      "tick_hz": self.descriptor.tick_hz,
                      "max_pointers": self.descriptor.max_pointers})
        return value
    def frame(self) -> bytes:
        status = self.api.mosaico_host_game_render_rgb565_v1(
            self.context, self.framebuffer, self.descriptor.width)
        if status: raise RuntimeError(f"host render failed: {status}")
        return _rgb565_png_bytes(self.framebuffer, self.descriptor.width,
                                 self.descriptor.height)

class ReloadableHostRuntime:
    def __init__(self, project: Path, directory: Path) -> None:
        self.project, self.directory = project, directory
        self.current = GenericHostRuntime(project, directory)
        self.lock = threading.RLock()
        self.generation = 0
        self.reload_error = ""
        self.stamp = self._stamp()

    def _watched(self) -> list[Path]:
        return [*self.project.joinpath("main").glob("*.[ch]"),
                self.project / "game.sim.json",
                *self.project.joinpath("assets_src").glob("*.png"),
                *self.project.joinpath("assets_src").glob("*.json")]
    def _stamp(self) -> int:
        return max((path.stat().st_mtime_ns for path in self._watched()
                    if path.is_file()), default=0)
    def _reload(self) -> None:
        stamp = self._stamp()
        if stamp == self.stamp: return
        try:
            prepare = self.project / "assets_src/prepare_sprites.py"
            if prepare.is_file():
                subprocess.run([sys.executable, str(prepare)], check=True,
                               capture_output=True)
            manifest = self.project / "assets_src/game_assets.json"
            if manifest.is_file():
                packer = Path(__file__).resolve().parents[1] / "tools/pack_game_assets.py"
                subprocess.run([str(packer), "--source", str(manifest.parent),
                    "--output", str(self.project / "assets/generated")], check=True,
                    capture_output=True)
            self.generation += 1
            replacement = GenericHostRuntime(self.project, self.directory, self.generation)
            previous = self.current
            self.current = replacement
            previous.close()
            self.reload_error = ""
        except (OSError, subprocess.CalledProcessError, RuntimeError) as error:
            self.reload_error = str(error)
        self.stamp = stamp
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

class SkyRuntime:
    def __init__(self, source: Path, directory: Path) -> None:
        self.library = directory / "sky_game.so"
        subprocess.run(["cc", "-shared", "-fPIC", "-O2", "-std=c11", "-Wall",
                        "-Wextra", "-Werror", str(source), "-o", str(self.library)],
                       check=True)
        self.api = ctypes.CDLL(str(self.library))
        self.api.platform_game_reset.argtypes = [ctypes.POINTER(SkyGame)]
        self.api.platform_game_set_action.argtypes = [ctypes.POINTER(SkyGame),
                                                       ctypes.c_int, ctypes.c_bool]
        self.api.platform_game_update.argtypes = [ctypes.POINTER(SkyGame)]
        self.api.platform_game_blocks.argtypes = [ctypes.POINTER(SkyGame),
                                                   ctypes.POINTER(ctypes.c_size_t)]
        self.api.platform_game_blocks.restype = ctypes.POINTER(SkyBlock)
        self.api.platform_game_finish_x.argtypes = [ctypes.POINTER(SkyGame)]
        self.api.platform_game_finish_x.restype = ctypes.c_float
        self.api.platform_game_state_hash.argtypes = [ctypes.POINTER(SkyGame)]
        self.api.platform_game_state_hash.restype = ctypes.c_uint32
        self.game = SkyGame()
        self.api.platform_game_reset(ctypes.byref(self.game))
        atlas_path = source.parents[1] / "assets_src" / "sky_hop_atlas.png"
        self.atlas = Image.open(atlas_path).convert("RGBA")
        self.sprites = [self.atlas.crop(((i % 4) * 64, (i // 4) * 64,
                                        (i % 4 + 1) * 64, (i // 4 + 1) * 64))
                        for i in range(8)]
        self.frames = 0
        self.lock = threading.Lock()

    def step(self, left: bool, right: bool, jump: bool,
             restart: bool = False, pause: bool = False) -> None:
        if self.game.phase in (0, 3, 4, 5) and (left or right or jump):
            restart = True
        if restart:
            self.api.platform_game_set_action(ctypes.byref(self.game), 4, True)
        if pause:
            self.api.platform_game_set_action(ctypes.byref(self.game), 3, True)
        self.api.platform_game_set_action(ctypes.byref(self.game), 0, left)
        self.api.platform_game_set_action(ctypes.byref(self.game), 1, right)
        self.api.platform_game_set_action(ctypes.byref(self.game), 2, jump)
        self.api.platform_game_update(ctypes.byref(self.game))
        self.frames += 1

    def metadata(self) -> dict[str, object]:
        phase_names = ("start", "playing", "paused", "level_clear", "won", "game_over")
        phase = self.game.phase
        return {"frames": self.frames, "level": self.game.level + 1,
                "levels": 3, "score": self.game.score, "lives": self.game.lives,
                "phase": phase_names[phase] if 0 <= phase < len(phase_names) else "unknown",
                "state_hash": f"{self.api.platform_game_state_hash(ctypes.byref(self.game)):08x}"}

    def frame(self) -> bytes:
        game = self.game
        camera = int(game.camera_x)
        image = Image.new("RGB", (480, 480), (92, 190, 236))
        draw = ImageDraw.Draw(image)
        draw.rectangle((0, 300, 479, 479), fill=(170, 224, 245))
        for index in range(8):
            cloud_x = index * 210 - (camera // 3) % 210
            cloud_y = 155 + index % 2 * 28
            draw.rectangle((cloud_x, cloud_y, cloud_x+104, cloud_y+17), fill=(235,248,250))
            draw.rectangle((cloud_x+20, cloud_y-12, cloud_x+81, cloud_y+17), fill=(235,248,250))
        resampling = getattr(Image, "Resampling", Image).NEAREST
        def sprite(index: int, x: float, y: float, width: int, height: int,
                   flip: bool = False) -> None:
            value = self.sprites[index].resize((width, height), resampling)
            if flip:
                transpose = getattr(Image, "Transpose", Image)
                value = value.transpose(transpose.FLIP_LEFT_RIGHT)
            image.paste(value, (int(x), int(y)), value)
        count = ctypes.c_size_t()
        blocks = self.api.platform_game_blocks(ctypes.byref(game), ctypes.byref(count))
        for index in range(count.value):
            block = blocks[index]
            x, y = int(block.x)-camera, int(block.y)
            for tile_x in range(x, x + int(block.width), 48):
                sprite(5, tile_x, y-2, 50, 50)
            if block.y >= 390:
                draw.rectangle((x, y+48, x+int(block.width), y+89), fill=(111,73,45))
        for index in range(game.coin_count):
            coin = game.coins[index]
            if not coin.collected:
                pulse = 25 + ((game.tick // 5 + index) % 3) * 2
                sprite(4, int(coin.x)-camera-pulse/2, int(coin.y)-pulse/2, pulse, pulse)
        for index in range(game.enemy_count):
            enemy = game.enemies[index]
            if enemy.active:
                sprite(3, int(enemy.x)-camera-7, int(enemy.y)-12, 44, 44, enemy.speed < 0)
        finish = int(self.api.platform_game_finish_x(ctypes.byref(game))) - camera
        if finish < 500:
            sprite(6, finish-12, 292, 70, 98)
        hero = 2 if not game.grounded else (game.tick // 5) % 2 if (game.move_left or game.move_right) else 0
        sprite(hero, int(game.player_x)-camera-14, int(game.player_y)-22, 58, 64,
               game.velocity_x < 0)
        try:
            font20 = ImageFont.truetype("DejaVuSansMono.ttf", 20)
            font16 = ImageFont.truetype("DejaVuSansMono.ttf", 16)
            font38 = ImageFont.truetype("DejaVuSansMono.ttf", 38)
        except OSError:
            font20 = font16 = font38 = ImageFont.load_default()
        draw.rectangle((0,0,479,41), fill=(22,42,68))
        draw.text((14,9), f"SCORE {game.score:04d}", font=font20, fill="white")
        draw.text((181,10), f"L{game.level+1}/3", font=font16, fill=(129,224,171))
        draw.text((250,12), f"BEST {game.score:04d}", font=font16, fill=(255,220,80))
        draw.text((365,9), f"LIFE {game.lives}", font=font20, fill="white")
        draw.rectangle((438,4,474,36), fill=(52,77,104))
        draw.rectangle((449,11,453,29), fill="white"); draw.rectangle((459,11,463,29), fill="white")
        controls = ((8,408,146,472,(25,43,65),"LEFT"),
                    (154,408,292,472,(25,43,65),"RIGHT"),
                    (300,408,472,472,(226,95,63),"JUMP"))
        for x0,y0,x1,y1,color,label in controls:
            draw.rectangle((x0,y0,x1,y1), fill=color)
            box = draw.textbbox((0,0), label, font=font20)
            draw.text(((x0+x1-(box[2]-box[0]))//2, 427), label, font=font20, fill="white")
        if game.phase != 1:
            titles = {0:"SKY HOP", 2:"PAUSED", 3:"LEVEL CLEAR!", 4:"ALL CLEAR!", 5:"TRY AGAIN"}
            prompts = {0:"CLICK OR PRESS A KEY", 2:"CLICK OR PRESS P", 3:"CLICK FOR NEXT LEVEL",
                       4:"CLICK TO PLAY AGAIN", 5:"CLICK TO TRY AGAIN"}
            draw.rectangle((42,120,438,310), fill=(20,39,65))
            title = titles.get(game.phase, "SKY HOP")
            box = draw.textbbox((0,0), title, font=font38)
            draw.text(((480-(box[2]-box[0]))//2,155), title, font=font38, fill=(255,220,80))
            prompt = prompts.get(game.phase, "CLICK TO START")
            box = draw.textbbox((0,0), prompt, font=font16)
            draw.text(((480-(box[2]-box[0]))//2,255), prompt, font=font16, fill=(129,224,171))
        output = io.BytesIO()
        image.save(output, format="PNG", compress_level=3)
        return output.getvalue()

def run_sky(source: Path, directory: Path, frames: int, output: Path) -> dict[str, object]:
    runtime = SkyRuntime(source, directory)
    runtime.step(False, False, False, restart=True)
    for index in range(max(0, frames)):
        runtime.step(False, True, index % 55 < 8)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(runtime.frame())
    return {**runtime.metadata(), "frame": str(output)}

def run_generic(project: Path, directory: Path, frames: int, output: Path,
                replay: list[dict[str, object]] | None = None) -> dict[str, object]:
    runtime = GenericHostRuntime(project, directory)
    events = replay or []
    next_event = 0
    paused = False
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
                runtime.action(code, bool(event["pressed"]))
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
            runtime.step(False, False, False)
            runtime.control(1)
        else:
            runtime.step(False, False, False)
        for track, x, y in tap_releases:
            runtime.pointer(track, x, y, False)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(runtime.frame())
    result = {**runtime.metadata(), "frame": str(output)}
    runtime.close()
    return result

def serve_interactive_preview(listen: str, port: int, runtime: object) -> None:
    simulation = {"paused": False, "speed": 1.0,
                  "actions": {"left": False, "right": False, "jump": False},
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
<div class=hint>Keyboard: A/D or ←/→, Space action, P pause, Enter start · Touch: up to two tracked points</div></main>
<script>
const held=new Set(), pointers=new Map(), img=document.querySelector('#screen'), state=document.querySelector('#state');
let busy=false, phase='start',paused=false;
function key(e,down){const k=e.key.toLowerCase();if(['arrowleft','arrowright',' ','a','d','p','enter'].includes(k))e.preventDefault();
 if(down&&!held.has(k)&&k==='p')control(paused?'resume':'pause');
 if(down&&!held.has(k)&&k==='enter')control('continue'); down?held.add(k):held.delete(k);sendInput()}
addEventListener('keydown',e=>key(e,true));addEventListener('keyup',e=>key(e,false));
function pointer(e,down){e.preventDefault();const r=img.getBoundingClientRect();
 const p={x:(e.clientX-r.left)*480/r.width,y:(e.clientY-r.top)*480/r.height};down?pointers.set(e.pointerId,p):pointers.delete(e.pointerId)}
img.onpointerdown=e=>{img.focus();img.setPointerCapture(e.pointerId);pointer(e,true);sendInput()};
img.onpointermove=e=>{if(pointers.has(e.pointerId)){pointer(e,true);sendInput()}};
img.onpointerup=img.onpointercancel=e=>{pointer(e,false);sendInput()};
addEventListener('blur',()=>{held.clear();pointers.clear()});
async function sendInput(){let left=held.has('a')||held.has('arrowleft'),right=held.has('d')||held.has('arrowright'),jump=held.has(' ');
 for(const p of pointers.values())if(p.y>=360){left|=p.x<150;right|=p.x>=150&&p.x<300;jump|=p.x>=300}
 await fetch('/api/v1/input',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({left,right,jump,pointers:[...pointers].map(([track,p])=>({track,...p,pressed:true}))})})}
async function control(command){await fetch('/api/v1/control',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({command})})}
async function tick(){if(busy)return;busy=true;
 try{const res=await fetch('/api/v1/frame');const meta=JSON.parse(res.headers.get('X-Mosaico-State'));const blob=await res.blob();
 phase=meta.phase;const old=img.src,url=URL.createObjectURL(blob);img.onload=()=>{if(old.startsWith('blob:'))URL.revokeObjectURL(old);img.onload=null};img.src=url;
 paused=meta.simulation.paused;const fields=Object.entries(meta).filter(([k])=>!['simulation','reload_error','title'].includes(k)).map(([k,v])=>`${k}=${v}`).join('  ');
 state.textContent=`${meta.title||meta.game_id||'Mosaico game'}\nLogic ${meta.simulation.logic_fps.toFixed(1)} Hz  Frame ${meta.simulation.frame_ms.toFixed(1)} ms\n${fields}`}
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
                "frame_ms": 1000.0 / tick_hz, "recording": simulation["recording"]}}
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
                    for key in ("left", "right", "jump")}
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

def serve_preview(listen: str, port: int, frame: Path,
                  metadata: dict[str, object]) -> None:
    page = f"""<!doctype html><html><head><meta charset=utf-8>
<title>Mosaico game preview</title><style>
body{{margin:0;background:#050a12;color:#d9ffff;font:14px system-ui;display:grid;
place-items:center;min-height:100vh}}main{{padding:22px;background:#091722;border:1px solid #1c7b89;
box-shadow:0 18px 80px #000;border-radius:16px}}img{{width:min(78vh,480px);image-rendering:pixelated;
display:block;border:1px solid #299fad}}pre{{color:#82cbd1;margin:12px 0 0}}
</style></head><body><main><img src=/frame.png><pre>{json.dumps(metadata, indent=2)}</pre>
</main></body></html>""".encode()
    image = frame.read_bytes()
    class Handler(BaseHTTPRequestHandler):
        def do_GET(self) -> None:
            body, content_type = ((image, "image/png") if self.path.startswith("/frame.png")
                                  else (page, "text/html; charset=utf-8"))
            self.send_response(200)
            self.send_header("Content-Type", content_type)
            self.send_header("Content-Length", str(len(body)))
            self.send_header("Cache-Control", "no-store")
            self.end_headers()
            self.wfile.write(body)
        def log_message(self, fmt: str, *args: object) -> None:
            return
    print(json.dumps({"preview_url": f"http://{listen}:{port}/", **metadata}), flush=True)
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
    shooter_source = args.project / "main" / "shooter_game.c"
    tower_source = args.project / "main" / "tower_game.c"
    sky_source = args.project / "main" / "platform_game.c"
    generic_adapter = args.project / "main" / "host_adapter.c"
    generic_module = args.project / "game.sim.json"
    if (not generic_module.is_file() and not shooter_source.is_file() and
            not tower_source.is_file() and not sky_source.is_file()):
        parser.error("project does not expose a supported host game model")
    with tempfile.TemporaryDirectory(prefix="mosaico-game-") as directory:
        output = args.project / "build-host" / "frame.png"
        if generic_module.is_file() or generic_adapter.is_file():
            if args.headless:
                result = run_generic(args.project, Path(directory), args.frames,
                                     output, load_replay(args.replay))
                if args.state_output:
                    args.state_output.parent.mkdir(parents=True, exist_ok=True)
                    args.state_output.write_text(json.dumps(result, indent=2, sort_keys=True)+"\n")
                print(json.dumps(result))
            else:
                runtime = ReloadableHostRuntime(args.project, Path(directory))
                try:
                    serve_interactive_preview(args.listen, args.port, runtime)
                except KeyboardInterrupt:
                    pass
            return 0
        if sky_source.is_file():
            if args.headless:
                result = run_sky(sky_source, Path(directory), args.frames, output)
                if args.state_output:
                    args.state_output.parent.mkdir(parents=True, exist_ok=True)
                    args.state_output.write_text(json.dumps(result, indent=2, sort_keys=True) + "\n",
                                                 encoding="utf-8")
                print(json.dumps(result))
            else:
                try:
                    serve_interactive_preview(args.listen, args.port,
                                              SkyRuntime(sky_source, Path(directory)))
                except KeyboardInterrupt:
                    pass
            return 0
        if tower_source.is_file():
            replay = load_replay(args.replay)
            result = run_tower(tower_source, Path(directory), args.frames, output,
                               replay if args.replay else None)
            if args.state_output:
                args.state_output.parent.mkdir(parents=True, exist_ok=True)
                args.state_output.write_text(json.dumps(result, indent=2, sort_keys=True) + "\n",
                                             encoding="utf-8")
            if args.headless:
                print(json.dumps(result))
            else:
                try:
                    serve_preview(args.listen, args.port, output.resolve(), result)
                except KeyboardInterrupt:
                    pass
            return 0
        library = Path(directory) / "game.so"
        subprocess.run(["cc", "-shared", "-fPIC", "-O2", str(shooter_source), "-lm", "-o", str(library)], check=True)
        api = ctypes.CDLL(str(library))
        api.shooter_game_reset.argtypes = [ctypes.POINTER(Game), ctypes.c_uint32]
        api.shooter_game_set_pointer.argtypes = [ctypes.POINTER(Game), ctypes.c_float, ctypes.c_float, ctypes.c_bool]
        api.shooter_game_update.argtypes = [ctypes.POINTER(Game)]
        api.shooter_game_state_hash.argtypes = [ctypes.POINTER(Game)]
        api.shooter_game_state_hash.restype = ctypes.c_uint32
        game = Game()
        api.shooter_game_reset(ctypes.byref(game), 0x4D4F5341)
        api.shooter_game_set_pointer(ctypes.byref(game), 240, 420, True)
        for index in range(max(0, args.frames)):
            x = 240 + ((index // 30) % 5 - 2) * 55
            api.shooter_game_set_pointer(ctypes.byref(game), x, 420, True)
            api.shooter_game_update(ctypes.byref(game))
        output.parent.mkdir(parents=True, exist_ok=True)
        png(output, game)
        print(json.dumps({"frames": args.frames, "score": game.score,
            "lives": game.lives, "state_hash": f"{api.shooter_game_state_hash(ctypes.byref(game)):08x}",
            "frame": str(output)}))
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
