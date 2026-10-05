#!/usr/bin/env python3
"""Static regression checks for asynchronous presenter strip display."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
BOARD = ROOT / "examples" / "boards" / "esp-mosaico"
SOURCE = BOARD / "board.c"
STRIP = BOARD / "mosaico_strip_present.c"
HEADER = ROOT / "examples" / "common" / "raylib_lite_example_board.h"


class BoardDisplayContractTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source = SOURCE.read_text(encoding="utf-8")
        cls.strip = STRIP.read_text(encoding="utf-8")
        cls.header = HEADER.read_text(encoding="utf-8")

    def test_board_uses_rgb565_presenter_and_byte_swap(self):
        for fragment in (
            "esp_display_presenter_create(&present_config, &p->presenter)",
            ".panel_type = ESP_DISPLAY_PRESENT_PANEL_IO",
            ".input_pixel_format = ESP_DISPLAY_PRESENT_PIXEL_FORMAT_RGB565",
            ".rotation = ESP_DISPLAY_PRESENT_ROTATE_0",
            ".swap_bytes = true",
            ".mode = ESP_DISPLAY_PRESENT_MODE_NONE",
            ".lines = c->drawbuf_lines ? c->drawbuf_lines : 34",
            ".buffers = c->drawbuf_count ? c->drawbuf_count : 2",
        ):
            with self.subTest(fragment=fragment):
                self.assertIn(fragment, self.source)

    def test_present_copies_and_submits_each_strip_with_full_coverage(self):
        for fragment in (
            "lease.resolve_rows(",
            "memcpy(dst + row * dst_stride, src + row * row_bytes, row_bytes)",
            "esp_display_presenter_submit_buffer(",
            ".y1 = y",
            ".y2 = (int32_t)(y + rows) - 1",
            "ESP_DISPLAY_PRESENT_COVERAGE_FULL",
            "esp_display_presenter_commit_frame",
        ):
            with self.subTest(fragment=fragment):
                self.assertIn(fragment, self.strip)
        self.assertIn("mosaico_strip_present_backend(p->video)", self.source)
        self.assertNotIn("esp_gsp", self.source)
        self.assertNotIn("mosaico_full_present", self.source)

    def test_latest_snapshot_is_coherent(self):
        self.assertIn(".copy_latest = strip_copy_latest", self.strip)
        self.assertIn("memcpy(out_pixels, video->frames[video->latest]", self.strip)
        self.assertIn("video->states[index] = FRAME_LATEST", self.strip)
        self.assertIn("SemaphoreHandle_t mutex", self.strip)
        self.assertIn("xTaskCreatePinnedToCore(strip_worker", self.strip)
        self.assertIn("uint8_t drawbuf_count;", self.header)
        self.assertNotIn("gsp_bundle", self.header)
        self.assertNotIn("canvas_bind", self.header)

    def test_cleanup_keeps_video_and_presenter_retryable(self):
        close = self.source.index("mosaico_strip_present_close(p->video")
        failure = self.source.index("if (rr != RAYLIB_LITE_OK)", close)
        ret = self.source.index("return to_esp(rr);", failure)
        clear_video = self.source.index("p->video = NULL;", ret)
        delete = self.source.index("esp_display_presenter_delete(p->presenter)", clear_video)
        self.assertLess(ret, clear_video)
        self.assertLess(clear_video, delete)


if __name__ == "__main__":
    unittest.main()
