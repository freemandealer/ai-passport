[简体中文](key-radar.zh_CN.md) · **English**

# Sonic Key Radar

An offline musical-key estimator for AI Passport. Hold OK while humming a melody
or playing music, then release it to see the estimated major/minor key, a second
candidate, and a profile similarity score. This is musical-key analysis, not a
single-note tuner. The feature branch is `feature/pitch-radar`.

## Controls and display

| Control | Action |
| --- | --- |
| Hold OK | Start a fresh listening session; the microphone operates during the hold. |
| Release OK | Finish the captured phrase and show its estimated key. |
| UP | Cancel capture, switch humming/music mode and clear the previous result. |
| DOWN | Cancel capture or an error and return to a cleared ready screen. |
| Continue holding for 30 seconds | Finish automatically; release before starting again. |

The 240 × 320 portrait UI uses dark green panels, phosphor-green typography,
radar rings, a level indicator and twelve pitch-class bars ordered C through B.
The battery reading occupies the top right; an unavailable reading shows `--%`.
Startup goes directly to this screen. Baseline demo screens are retained as
reference source but are not compiled into this application.

Use a complete 8–20 second phrase; at least 3 seconds of analyzed audio is required.
A 128 ms warm-up discards codec transients and queued samples. Keep individual
hummed notes steady for about 0.3 seconds. A single sustained note cannot establish
musical key. Release preserves the result; pressing again discards the previous
phrase. Mode/clear commands cancel even if OK has not yet emitted its release event.
The shared ADC ladder supports one physical button at a time; simultaneous button combinations are not
an application gesture.

## Estimation and limitations

- Humming mode uses a bounded integer-inner-loop [YIN-style pitch detector](https://doi.org/10.1121/1.1458024)
  at 16 kHz and 65–1060 Hz (approximately C2–C6). Two consecutive matching pitch
  windows suppress attacks and brief transitions. Use this mode for an isolated
  voice or monophonic instrument.
- Music mode uses a 4096-point Q15 FFT, 256 ms Hann windows with a 128 ms hop,
  and interpolated pitch-class accumulation, approximately C2–C7. It estimates
  common detuning before note rounding and reduces the weight of peaks aligned
  with observed lower fundamentals (harmonics 2–6). Smooth pitch weighting avoids
  the previous hard 38-cent rejection boundary. The extra low octave includes
  bass notes which the first version discarded. It accepts simple
  polyphonic material. Drums, bass-only material, dense arrangements and strong
  harmonics can bias the estimate; it is not a source-separation system.
- Both modes weight a twelve-class histogram by accepted frame duration and rank
  all 24 major/minor candidates using Pearson correlation against the
  [Krumhansl-Kessler profiles](https://music21.org/music21docs/moduleReference/moduleAnalysisDiscrete.html#krumhanslkessler).
  The displayed percentage is profile similarity, **not a probability of correctness**.
- Fewer than four significant pitch classes, insufficient periodic/tonal sound,
  short capture or clipping produce explicit retry states. A top correlation
  below 0.60, or a lead under 0.08 over the next candidate, is uncertain.
  Relative keys can remain ambiguous even with clean audio. Modes, modulation,
  non-Western tuning and non-tonal music are outside the 24-key model.
- Equal-tempered A4 = 440 Hz is the nominal reference; music mode estimates a
  common fractional-semitone detuning per analysis window. Half-semitone shifts
  cannot be resolved uniquely without an external tuning reference. Enharmonic keys
  use sharp spellings (for example C# rather than Db).

Background on pitch-class weighting and tuning correction: [HPCP](https://essentia.upf.edu/reference/std_HPCP.html)
and [KeyExtractor](https://essentia.upf.edu/reference/std_KeyExtractor.html). This
application implements bounded local analysis and adds no Essentia dependency.

No network service, account, audio playback, recording file or persistent setting
is used. Raw PCM and DSP scratch are cleared when capture stops; only aggregate
pitch-class evidence remains for the result display. No raw microphone data is
logged. A reboot returns to humming mode.

## Implementation and resources

`main/key_analyzer.c`, `pitch_detector.c` and `key_control.c` contain portable
logic. `main/main.c` owns the microphone worker; `main/key_audio.c` pairs codec
initialization, wake, format and suspend; `main/key_ui.c` owns the persistent
LVGL screen. The existing display, audio, battery and shared-bus BSP are reused.
`BSP_BTN_RELEASE` is appended to the public event enum without changing prior
values and maps to the button component's debounced press-up event.

The button callback only overwrites a one-element mailbox containing the complete
held state and a generation number. Release is not lost to a full event queue;
a quick new press invalidates in-flight audio. Blocking PCM reads occur in 256-sample
(16 ms nominal) pieces on one worker. It never changes audio format or sleeps the
codec while a read is active. All application LVGL operations hold the BSP lock.
A wall-clock limit supplements the frame limit. Audio faults become visible retry
states. Device timing still requires measurement.

The application reserves approximately 31 KB of fixed DSP/PCM state and a 6 KB
worker stack; it never allocates a whole recording. FFT tables and constant font
bitmaps live in Flash. The LVGL pool is 32 KB after the 24 KB native-renderer test
hit allocation failure. The native 32 KB run peaked at 21,488 allocated bytes;
host fragmentation/pointer sizes differ from the board. The existing single
240 × 20 RGB565 DMA buffer and minimal 8 MB partition layout remain in use.
Bluetooth is disabled; no radio stack is started. Startup/result logs report free
heap, largest block, maximum DSP time, PCM read gaps and worker stack high-water mark for device
acceptance.

The 14/20 px Chinese fonts cover 200 code points including all fixed UI strings
and printable ASCII. Font sources, license, exact hash, regeneration and inventory
are documented in [assets](../assets/README.md#sonic-key-radar-fonts).
There is no arbitrary user/network text. Every text widget explicitly selects its
font; the large tonic uses Montserrat 48. Glyph checking includes a known-missing
negative case. Font coverage and native rendering do not prove physical LCD output.

## Validation and firmware

```sh
./tools/validate.sh --static
# With ESP-IDF v5.5.3 activated:
./tools/validate.sh --firmware
./tools/validate.sh
# Real pinned LVGL renderer; requires managed_components from an IDF build:
./tools/test_key_ui.sh
```

The shared gate includes pitch/key logic, input-state tests, font inventory/source
checks and the baseline host tests. The gate selects Darwin's `-dead_strip` for
native Apple linking and `--gc-sections` elsewhere. The native UI test checks
actual widget fonts and text bounds across ready/listening/results/errors, all
24 candidate names, missing battery and the duration limit; it repeatedly renders
using a 32 KB LVGL pool. PPM screenshots are written to `build/key-preview/`.

Synthetic tests cover 49 pitches, all 24 profile rotations, 48 transposed major/minor
PCM phrases across both modes, a polyphonic cadence, silence, white noise, clipping,
single-note refusal, flat ambiguous evidence, timeout, reset, repeated presses
and generation wraparound. The real capture loop is exercised against RTOS/I/O
stubs for repeated record/release/clear/re-record, clearing while held, read-error
recovery and LVGL locking. The application audio wrapper runs 30 start/stop cycles
against the real BSP with injected wake/suspend failures.

An additional 84-fixture acoustic regression set covers all 12 transpositions of
+/-42-cent detuning, strong third/fifth harmonics in two registers, and low bass
phrases. The old implementation matched 21/84 first candidates; this revision
matches 84/84, including an E-major phrase the old algorithm labeled G# minor.
This is a targeted synthetic set, not a real-song accuracy percentage. The
reported user song was not supplied, so its exact result remains unverified.
The tests use continuous oscillators across frame boundaries.

Address/undefined-behavior sanitizers are also run.
These establish deterministic behavior, not accuracy on a real music corpus.

The validated merged image is `build/FoloToy-AI-Passport-full.bin`, for offset
`0x0`. Matching ELF/MAP/component images and hashes are retained under
`build/firmware/<full-image-sha256>/manifest.json`. Verify a bundle with:

```sh
python3 tools/archive_firmware.py verify build/firmware/<full-image-sha256>
```

Flashing requires explicit approval. A merged flash replaces the running
application and can reset NVS/PHY data; a full-chip erase is not part of this
workflow. See [firmware layout and stored data](development/engineering/firmware-layout.md#flashing-and-stored-data).

## Fixed failure paths

The first version suspended ES8311 after capture but called only `init` and
`set_format` on the next hold. `init` is idempotent and does not wake the codec;
`set_format` rejects a sleeping codec. The application now calls `bsp_audio_wake()`
before configuring it, including retries after transient failures. The regression
reproduced the old failure on the second capture.

Clear/mode input no longer depends on an OK-held flag which may be stale when
ADC button events arrive in a different order. It cancels the generation, clears
all result/evidence state, and permits the next capture. The UI explains the two
modes at rest and labels uncertain keys as candidates. Worker input/clear logs
allow a physical key-detection failure to be distinguished from state handling.
The user reported that clear never worked; all physical-button scenarios still
need device confirmation. No pin thresholds were guessed or changed.

## Pending device acceptance

1. Confirm startup, all Chinese states, contrast, clipping and battery fallback on
   the 240 × 320 LCD; do not accept a host screenshot as board evidence.
2. Hold/release repeatedly, including a quick tap and a new hold during analysis;
   verify no stuck listening, result mixing or stale audio. Hold beyond 30 seconds.
3. Replay the user's E-major accompaniment which previously displayed G# minor.
   Hum known C-major and A-minor phrases and transpose them. Try a simple recorded
   cadence in music mode, then ambiguous relative-key material. Record disagreements.
4. Test silence, ambient noise, a single note, too-short capture and overload.
5. Observe repeated captures for free-heap/stack stability, watchdogs and codec
   recovery. Measure processing plus UI delay against the 90 ms I2S DMA headroom,
   actual release latency and microphone/clock accuracy.

Build: PASS (repair complete gate, ESP-IDF v5.5.3; merged image/debug archive verified).
Host tests: PASS (synthetic DSP and baseline gate; native LVGL rendering).
Device tests: NOT RUN.
Unverified: real microphone accuracy, music-corpus accuracy, LCD rendering,
physical button latency, sustained DSP/DMA timing, heap/stack and battery behavior.
