# Public API and application contract

## Installation and compatibility

The component exports `include/raylib_lite/` and the separate Raylib facade under
`compat/raylib/include/`. Prefer neutral `raylib_lite_*` types for new integrations.
Public Engine headers do not expose BSP or ESP-IDF types. See [the English guide](README.md).
The historical `mosaico_*` extraction requires source migration. Public result
values are explicit; new values are appended. Host/module ABI versions are
separate from the component release version; see [Host ABI](host/README.md).

## Lifecycle and threads

1. The application creates its clock, input queue and video backend. Initialize
   display workers and optional audio before invoking the game app.
2. Call `raylib_lite_game_app_run()` on the application's game thread. The Engine
   does not create that thread or choose its CPU affinity/priority.
3. `on_start` initializes game resources. After it has been entered, `on_stop`
   runs exactly once on every exit path, including partial startup failure.
4. `on_first_present` runs only after an accepted first frame has been flushed.
   It is the application hook for device health confirmation.
5. After the app returns, stop producers/workers, drain outstanding video/audio
   work, then destroy the application-owned backend and input queue.

Callbacks run on the calling game thread. Renderer state, facade globals and game
resources require serialized access; they are not an independent context per task.
Use the input queue to cross producer/game thread boundaries rather than calling
render or gameplay functions from interrupt/USB workers. Supply both queue
lock/unlock callbacks for cross-thread use; without them every queue call must
be externally serialized. The lock implementation must fit the producer context;
ordinary task locks do not authorize calling queue operations from an ISR.
Keep callbacks bounded.

## Frame ownership and errors

`get_info` describes a fixed native-endian RGB565 surface. Stride is in pixels
and must be at least width. The backend performs any transfer byte swapping.
A successful `acquire` lends one exclusively writable frame; its opaque token
belongs to the backend. The Engine holds at most one acquired frame.

`present` consumes the frame on **every** return path, including BUSY, TIMEOUT
and failures. Never access pixels after present or discard. Accepted asynchronous
frames may still be in flight; `flush` waits for the documented consumer release
point. An accepted frame is not proof that a human has seen it on the display.
`discard` consumes an unpresented frame and must not block.

Check `raylib_lite_result_t`; invalid configuration and lifecycle errors require
correction. Temporary backpressure must not be reported as successful presentation.
Use a finite timeout unless an operation explicitly permits
`RAYLIB_LITE_WAIT_FOREVER`. The public header comments are authoritative for each
function's specific ownership and return behavior.

## Scheduling, input, assets, audio and save

- `raylib_lite_runner_run` can be used without a display. It separates logic Hz
  from target presentation FPS, limits catch-up to three updates per pass, and
  discards stale credits after stalls. The clock must be monotonic.
- The launcher owns input producers and shutdown ordering. Gameplay consumes
  queued events on its own thread. Define a policy for queue overflow.
- Mounted/registered asset memory must remain valid while loaded resources or
  streams reference it. Close streams and unload textures before unmounting.
  Do not mutate backing data while it is being rendered.
- Audio is optional: a NULL platform audio backend is a supported configuration.
  The application owns the output device and audio worker. Stop playback and
  workers before freeing their memory; the Board implements the game facade.
- The IDF save backend uses NVS. The application owns NVS initialization and
  its partition choice. Handle save failures; Engine installation does not
  create or replace the application's partition table.

## Product integrations

BSP, display DMA, JPEG acceleration, haptics, Iris and Recovery are application
or example Board policy. Installing this component alone does not create USB
services or provision Recovery. The minimal offscreen
example exercises installation without these product dependencies.

## Native application services

The shared example launcher can explicitly select an application service provider
through `RAYLIB_LITE_NATIVE_SERVICE_COMPONENT`. Its neutral contract is
[`raylib_lite_native_services.h`](include/raylib_lite/raylib_lite_native_services.h).
Boot runs before Board creation, attach borrows the Board video/input interfaces,
and first-present runs after frame submission/flush and input startup. Detach must
finish before the Board is destroyed; failure retains resources. A selected provider
is a required link dependency. Ordinary native examples select no provider.
