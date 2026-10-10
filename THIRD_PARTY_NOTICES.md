# Third-party dependencies

The root [LICENSE](LICENSE) applies to this repository's Apache-2.0-marked source unless a file says otherwise. It does not relicense external components, tools, or assets.

| Dependency | How it is used | Where to check the resolved license and notice |
| --- | --- | --- |
| raylib 6.0 (zlib/libpng), vendored in `third_party/raylib/src` with its `rlsw`, `stb_image`, `stb_image_resize2`, `qoi` and `rprand` headers | Upstream raylib API and software renderer for native games; compatibility types | [third_party/raylib/LICENSE](third_party/raylib/LICENSE) and each header's own notice. Sources are unmodified copies from `georgik/raylib` 6.0.0~2 (raylib commit `5276634372de7bb46e6a93376db15dc4384bd2f2`, whose `rlsw.h` restores `swGetColorBuffer`); `config.h` is this repository's |
| ESP-IDF and Espressif managed components, including `esp_mmap_assets` and `esp_display_present` | Device builds | The product's dependency lock and each resolved component's license/notice |
| Pillow | Host preview and asset conversion | Installed Python package metadata and upstream license |
| Board BSP and product Iris/Recovery | Optional device integrations | Their separate repositories and resolved revisions |

`managed_components/` and build directories are generated dependency copies, not maintained source in this repository. A release or firmware package must collect the actual licenses/notices for the versions it ships. This inventory is a pointer to those sources, not a claim that every transitive dependency is listed here.

## Example media

The repository owner confirmed on 2026-10-08 that maintained example media was
generated with Codex. See [asset provenance](release/asset_provenance.json).
Release assembly produces a per-example file/hash inventory and includes the
Apache-2.0 license. External dependencies retain their own licenses; add separate
notices for any third-party fonts or media introduced later.
