[简体中文](frog-voice-score.zh_CN.md) · **English**

# Frog Voice Score

An offline, original pond interface for the ESP32-C3 AI Passport. Hold OK to
speak, release to receive a random entertainment score from **0 to 100**, then
watch the pixel frog jump higher for a higher score. The implementation interprets
the requested height adjustment as jump height. There is no speech recognition,
semantic assessment, cloud service, voice storage, or playback.

## Controls and behavior

| Input | Behavior |
| --- | --- |
| Hold OK | Capture microphone PCM at 16 kHz, mono, 16-bit; show elapsed time and a live level meter. |
| Release OK | Finish capture; after 550 ms, reveal the random score and play a 1.1-second jump. |
| UP after landing | Replay the same score's jump. |
| DOWN outside recording | Return to the ready screen; retain the session best. |

Recording is limited to 10 seconds and finishes automatically at the limit.
Release the key before beginning another attempt. Holds shorter than 400 ms or
captures with fewer than 3,200 samples do not score. A simple silence guard
requires at least 640 samples from chunks with peak-to-peak amplitude of at least
300; silence/DC-only input prompts another attempt. This guard is not a speech
detector and requires real-board sensitivity validation. Audible background noise
can satisfy it. The score is independent of volume, duration and content.

At 0 points the frog stays on the lily pad; at 100 points it reaches 88 pixels
above its fixed ground position. All heights stay inside the screen. Scores and
the best result are held only in RAM and reset on reboot. Audio failures discard
the round; release and hold OK to retry. An unavailable battery shows `--%`.

After 45 seconds without activity, the backlight dims to 12%; after 120 seconds
it turns off. When codec suspend has succeeded, the worker uses 100 ms timer-wake
light-sleep intervals. Hold OK to wake and speak; very short taps while sleeping
may be missed. Runtime consumption and USB behavior during sleep need hardware
measurement. No GPIO wake wiring or terminal deep-sleep sequence is introduced.

## Implementation and resources

The application starts directly in `main/frog_ui.c`; baseline menu/demo sources
remain as references but are excluded from the firmware component. Hardware is
handled by the existing BSP. Its button API gains the appended
`BSP_BTN_RELEASE` event without changing previous enum values. Button callbacks
only overwrite a latest-state mailbox, so a full event queue cannot strand a
held recording. One permanent worker owns the model, audio and battery I/O;
all UI calls take the LVGL lock. There are no transient screens or UI timers
requiring teardown. Recording uses one 640-byte PCM buffer, not an utterance-sized
allocation. The worker has a 6,144-byte stack. LVGL uses the shared C allocator;
the BSP's single 240 x 40 RGB565 DMA buffer remains unchanged. Bluetooth is disabled.

The two Chinese font subsets are Noto Sans CJK SC at 14 and 20 px, 4 bpp,
uncompressed. Every fixed UI string plus printable ASCII is included; the large
numeric score uses Montserrat 36. The UI never displays recognized/user-provided
text. See [font assets](../assets/README.md#frog-voice-score-fonts) for reproducible
generation and licensing. Both runtime startup and the native simulator check
glyph descriptors, including a known-missing negative case. The simulator also
checks the actual font of each displayed label and its bounds.

## Reproduce validation

Activate ESP-IDF **5.5.3**, using the Python version used during installation. On
this macOS setup Python 3.13 was used; do not accidentally activate a 3.14 venv.

```sh
./tools/validate.sh --static
./tools/validate.sh
```

The complete gate builds from isolated tracked defaults, executes the real LVGL
9.5 application renderer on the host, validates the merged image and archives
matching ELF/MAP/images. The native renderer can also run separately after the
Managed Components are downloaded:

```sh
./tools/test_frog_ui.sh
```

Review `build/preview/index.html` for actual LVGL images of ready, listening,
thinking, 0/50/100-point jumps, landed results and all error states. This runs
the firmware's UI and model code on the host, not ESP32 hardware drivers. Tests
cover all 101 score outcomes, endpoints, timing, silence/DC, maximum duration,
retry, replay, reset, wraparound, animation bounds, font inventory/source identity,
BSP release registration/rollback and the existing upstream host suite. Rendering
includes 20 repeated complete trajectories. Host tests cannot prove electrical
behavior, scheduling or physical display output.

The delivered merged firmware is `build/FoloToy-AI-Passport-full.bin`, flashed
at **0x0**. Its content-addressed bundle is under `build/firmware/<sha256>/` with
the exact ELF/MAP and `manifest.json`. Validate with
`python3 tools/archive_firmware.py verify <bundle-directory>` before using it.
The default 8 MB partition layout is unchanged. Merged flashing can reset NVS
and replace existing settings; a compatible segmented flash is needed if those
must be retained. Flashing requires explicit user confirmation. No erase or
flash command is run by the validation flow.

## Hardware acceptance still required

- Cold boot, restart and supply-voltage variation; actual Chinese text on every
  state, colors, clipping, battery indication and 100-point layout.
- Hold/release latency, quick presses, long holds, simultaneous keys, repeated
  rounds, microphone level/silence threshold and microphone failure recovery.
- Measured jump heights at 0, 50 and 100; deterministic native snapshots already
  exercise those values, while normal firmware scores remain random.
- Continuous audio/display stability, minimum free heap/largest free block,
  stack margin and at least 100 repeated rounds.
- Backlight dim/off, hold-to-wake, post-wake recording, USB re-enumeration and
  actual current consumption. A successful software suspend does not establish
  the power draw of the board's external amplifier.

Before authorization to flash, report device tests as **NOT RUN** and these
items as unverified. Build and host checks are reported separately.
