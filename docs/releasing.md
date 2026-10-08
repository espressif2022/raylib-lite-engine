# Component release preparation

## Source, component and product boundaries

The repository remains one Engine component. Game models and views stay portable.
Board adapters and `examples_common` are application code. Vibe consumes the
Engine plus utils tools; it owns product deployment and Recovery provisioning.
No product deployment service is added to the Engine manifest.

The repository examples intentionally share code. They are **not** uploaded
unchanged. `tools/prepare_release.py` assembles this tree outside the checkout:

```text
component/
  CMakeLists.txt, idf_component.yml, Kconfig, include/, src/, compat/, tools/
  examples/
    minimal/                   # no board or external media
    living_worlds/
      main/, assets_src/
      shared/common_components/examples_common/
      shared/boards/esp-mosaico/
    ...                        # six maintained games
release-manifest.json          # outside component; all file hashes and provenance
```

Each game CMake entry references `shared/` inside its own directory. Its main
manifest uses the Registry dependency and has no checkout-relative override.
Board Git dependencies remain pinned. No sibling Engine checkout is required.
Registry download still needs network access to resolve dependencies and Pillow/
NumPy for asset preparation. Benchmarks and `*_dev` directories are not uploaded.

## Prepare and validate locally

Use an ESP-IDF 6.2 environment with `compote` and Python 3.10 or newer, plus the asset tooling:

```sh
python3 -m pip install Pillow numpy PyYAML
python3 tools/prepare_release.py --output /tmp/raylib-release-candidate
compote component pack --project-dir /tmp/raylib-release-candidate/component \
  --name raylib-lite-engine --dest-dir /tmp/raylib-release-candidate/archives
python3 tools/check_release.py --stage /tmp/raylib-release-candidate
python3 -m unittest discover -s tests -v
python3 tools/check_markdown_links.py
git diff --check
```

The output must be new; the assembler never deletes an existing destination.
`--minimal-only` produces only the no-media example for focused acceptance.
Never upload the repository root: its manifest excludes the unassembled examples.
The staged manifest permits only the assembled examples.

For unpublished-version local verification, install an **actual packed archive**
into a separate test project with its namespaced component directory:

```sh
python3 tools/prepare_release_consumer.py \
  --archive /tmp/raylib-release-candidate/archives/raylib-lite-engine_0.1.0.tgz \
  --example minimal --output /tmp/raylib-minimal-consumer
idf.py -C /tmp/raylib-minimal-consumer -B /tmp/raylib-minimal-build \
  -DIDF_TARGET=esp32s3 build
```

Repeat with `--example last_zone_extraction` and `idf.py --preview` for S31.
This copies one assembled example alone and unpacks the Engine into
`components/espressif2022__raylib-lite-engine`. The helper removes **only** the
Engine Registry dependencies in this test project, so the unpublished local
package is used. BSP/Iris dependencies keep their pins. This substitution must
never appear in published examples.
Build the minimal consumer and an assembled Board game with separate build dirs.
After publication, also verify a clean Registry dependency download with no local
components or overrides.

## Release version and upload

`VERSION`, root `idf_component.yml`, and example Engine constraints must agree.
Current release candidate is `0.1.0`; no release tag is created by preparation.
If that version is already published, select a new unused version and update the
constraints before upload. Registry versions are immutable; do not reuse a version.
The changelog records the namespace migration from historical `mosaico_*` APIs.

1. Select an unused release version and update `VERSION`, the root manifest and
   example constraints together. Commit reviewed source and run the checks.
2. Assemble the candidate locally and inspect `release-manifest.json` and archive
   files; verify namespace ownership (`espressif2022`).
3. If staging acceptance is required, upload the assembled directory with
   `compote component upload --profile staging --project-dir <stage>/component
   --name raylib-lite-engine`, then independently download and build its examples.
4. Merge the reviewed PR into `main`. The resulting push triggers package/build
   validation, then automatic production upload of the checked assembled archive.
5. Retain archives and logs. An optional `v<version>` tag records the source
   release; it is not the publishing trigger.

The release workflow assembles and checks artifacts on PRs, pushes to `main`
and manual runs. Only a push to `main` enables the dependent `upload_components`
job. It verifies the checked archive SHA-256, unpacks it and uploads the assembled
component through `espressif/upload-components-ci-action@v1` to namespace
`espressif2022`. PRs, version tags and manual runs do not upload. Direct pushes
to `main` also trigger publishing. The upload Action skips an already published
version; publishing updated content requires a new version.

Configure repository Actions secret `COMPONENTS_TOKEN` with a Registry token
that can publish to `espressif2022`. Merging this workflow into `main` enables
its automatic publishing behavior; no push is performed by local preparation.
The uploader uses the generated directory `publish/raylib-lite-engine`, while
manifest repository metadata points to the original source commit at repo root.

The workflow does not create tags or flash a device. Credentials stay in registry
profiles or CI secrets; no credentials belong in manifests or release archives.
See [official component publishing](https://docs.espressif.com/projects/idf-component-manager/en/latest/publish/tutorial_to_package_and_upload.html).

## Validation evidence and support scope

Record chip, board, IDF revision, build commands, startup/input/present/shutdown
results and unexercised paths. Host timing is not device FPS. Current verified
Board path is ESP-Mosaico/ESP32-S31 with IDF revision
`7b9cc1ac79f865983f59bb8ff3ff43eb74ff1dbe`; portability to other chips is subject
to focused IDF builds. The minimal consumer has no display/audio/input device
claims. Consult the per-release evidence rather than inferring support from
`idf >=6.2` alone.

## Assets and dependencies

[Asset provenance](../release/asset_provenance.json) records the owner's confirmation
that maintained media was generated with Codex. Assembly emits exact media names
and SHA-256 hashes into each example's `ASSET_PROVENANCE.json`. Procedural Python
scripts and screenshots are kept with their source examples. New third-party
media/fonts require their own source and notice entries before release.

The Engine's Apache-2.0 license does not replace dependency licenses. The exact
Raylib pin preserves the facade's header and utility contract; the private mmap
asset backend has its own version constraint. Review dependency lock files and
collect licenses for firmware distribution; locks are not shipped as component
source. See [third-party notices](../THIRD_PARTY_NOTICES.md).
