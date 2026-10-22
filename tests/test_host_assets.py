# SPDX-License-Identifier: Apache-2.0
"""Host file loading exercises the native asset core, not alternate API stubs."""
import ctypes
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class View(ctypes.Structure):
    _fields_ = [('id', ctypes.c_uint32), ('name', ctypes.c_char_p),
                ('data', ctypes.c_void_p), ('size', ctypes.c_size_t)]


class StoreConfig(ctypes.Structure):
    _fields_ = [('partition_label', ctypes.c_char_p), ('max_files', ctypes.c_int),
                ('checksum', ctypes.c_uint16), ('mmap_enable', ctypes.c_bool),
                ('image', ctypes.c_void_p), ('image_size', ctypes.c_size_t)]


class HostAssetsTests(unittest.TestCase):
    def test_first_id_lookup_root_switch_stream_and_registration(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)
            library = path / 'assets.so'
            subprocess.run(['cc', '-shared', '-fPIC', '-std=c11', '-Wall', '-Wextra', '-Werror',
                            '-I', str(ROOT/'include/raylib_lite'), '-I', str(ROOT/'src/assets'),
                            str(ROOT/'host/host_asset_runtime.c'), str(ROOT/'src/assets/raylib_lite_assets.c'),
                            '-o', str(library)], check=True)
            api = ctypes.CDLL(str(library))
            api.raylib_lite_asset_id.argtypes = [ctypes.c_char_p]
            api.raylib_lite_asset_id.restype = ctypes.c_uint32
            api.raylib_lite_asset_open_id.argtypes = [ctypes.c_uint32, ctypes.POINTER(View)]
            api.raylib_lite_host_assets_set_root.argtypes = [ctypes.c_char_p]
            api.raylib_lite_asset_open.argtypes = [ctypes.c_char_p, ctypes.POINTER(View)]
            api.raylib_lite_asset_release.argtypes = [ctypes.POINTER(View)]
            api.raylib_lite_asset_stream_open.argtypes = [ctypes.c_char_p, ctypes.c_uint32, ctypes.POINTER(ctypes.c_void_p)]
            api.raylib_lite_asset_stream_read.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_size_t, ctypes.POINTER(ctypes.c_size_t)]
            api.raylib_lite_asset_stream_close.argtypes = [ctypes.c_void_p]
            roots = [path/'first', path/'second']
            name = b'custom-font-metrics.bin'
            identifier = api.raylib_lite_asset_id(name)
            for index, root in enumerate(roots):
                root.mkdir()
                (root/name.decode()).write_bytes(bytes([index, 42, 255]))
                (root/'empty.bin').write_bytes(b'')
                api.raylib_lite_host_assets_set_root(str(root).encode())
                view = View()
                self.assertEqual(api.raylib_lite_asset_open_id(identifier, ctypes.byref(view)), 0)
                self.assertEqual(ctypes.string_at(view.data, view.size), bytes([index, 42, 255]))
                api.raylib_lite_asset_release(ctypes.byref(view))
                self.assertEqual(view.size, 0)
                stream = ctypes.c_void_p()
                self.assertEqual(api.raylib_lite_asset_stream_open(name, 1, ctypes.byref(stream)), 0)
                output = ctypes.create_string_buffer(10)
                count = ctypes.c_size_t()
                self.assertEqual(api.raylib_lite_asset_stream_read(stream, output, 10, ctypes.byref(count)), 0)
                self.assertEqual(output.raw[:count.value], bytes([42, 255]))
                api.raylib_lite_asset_stream_close(stream)
                self.assertEqual(api.raylib_lite_asset_open(b'empty.bin', ctypes.byref(view)), 0)
                self.assertEqual(view.size, 0)
                api.raylib_lite_asset_release(ctypes.byref(view))
                self.assertEqual(api.raylib_lite_asset_open(b'../assets.so', ctypes.byref(view)), 10)
            api.raylib_lite_assets_unmount()
            self.assertEqual(api.raylib_lite_assets_is_mounted(), 0)
            self.assertEqual(api.raylib_lite_asset_open_id(identifier, ctypes.byref(view)), 10)

            # Mounting a file store must not discard an independently registered
            # memory resource. Explicit root switching still resets all owners.
            api.raylib_lite_asset_register_memory.argtypes = [ctypes.c_char_p, ctypes.c_void_p, ctypes.c_size_t]
            api.raylib_lite_assets_mount.argtypes = [ctypes.POINTER(StoreConfig)]
            payload = ctypes.create_string_buffer(b'memory')
            memory_name = b'embedded-before-mount.bin'
            self.assertEqual(api.raylib_lite_asset_register_memory(memory_name, payload, 6), 0)
            config = StoreConfig(partition_label=str(roots[0]).encode(), max_files=16)
            self.assertEqual(api.raylib_lite_assets_mount(ctypes.byref(config)), 0)
            self.assertEqual(api.raylib_lite_asset_open(memory_name, ctypes.byref(view)), 0)
            self.assertEqual(ctypes.string_at(view.data, view.size), b'memory')
            api.raylib_lite_asset_release(ctypes.byref(view))
            api.raylib_lite_host_assets_set_root(str(roots[1]).encode())
            self.assertEqual(api.raylib_lite_asset_open(memory_name, ctypes.byref(view)), 10)
            api.raylib_lite_assets_unmount()
