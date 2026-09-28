**English** · [简体中文](guitar-score.zh_CN.md)

# GREEN ROOM: Guitar Score and Metronome

This application lives on `feature/guitar-score-metronome`, branched from the
local `main` hardware baseline. Existing applications remain on their original
branches. It uses the BSP display, ADC buttons, audio and battery gauge, with a newly
designed dark green score interface. Startup opens the score directly; the
baseline demo menu and pages are not compiled into the application.

## Edit a score over Wi-Fi

1. Hold OK for about 800 ms to open song selection. Select the last entry,
   **Wi-Fi score editor**, and press OK.
2. Connect a phone or computer to the displayed `Guitar-XXXX` open hotspot;
   no password is required. The hotspot accepts one client and does not provide Internet;
   keep the connection if the phone asks.
3. Open `http://192.168.4.1` in a browser. The page loads the saved text. Edit,
   validate, import or export a UTF-8 text file, then save to the device.
4. A successful save pauses playback, replaces the library and selects the first
   song. An invalid upload reports a line number without replacing the score.
   Failed requests keep the edited text in the browser for retry.

The service runs directly on the Passport. No router, account, cloud, BLE
provisioning, external assets or API subscription is needed. Reconnect to the
hotspot and reload the page after restarting the device. Automatic captive-portal
popups are not part of this application.

Older firmware's saved hotspot password is ignored; no NVS erase is needed to
enable open access. If a phone still asks for a password, forget its saved
`Guitar-XXXX` network and reconnect.

## Score format

```text
---
Green Practice
C
80
C(light strum) C Am(arpeggio) Am
F F G G
---
Minor Walk
Am
70
1 4 V7 1
6 3 7 1
```

Each song starts with `---` on its own line. The next three lines are strictly
title, key and default BPM; they are never interpreted as chords. Keep these
three lines nonempty. Every subsequent chord, separated by spaces, tabs or
line breaks, occupies exactly four quarter-note beats. Repeated chords remain
separate bars. LF, CRLF and UTF-8 BOM are accepted. An empty song is rejected.

| Field | Supported values |
| --- | --- |
| Key | C through B, optionally sharp/flat, with `m` for minor; examples: F#, Bb, Am, F#m |
| Default BPM | Any integer from 30 to 120 |
| Chord root | Letter name, or numeric/ Roman scale degree, optionally sharp/flat |
| Qualities | Major, m, 7, maj7, m7, dim, dim7, m7b5, aug, sus2, sus4, 5, 6, m6, add9 |
| Explicit major | `maj` or `M`; `M7` is an alias for `maj7`, `min` for `m` |
| Limits | 4096 UTF-8 bytes, 16 songs, 512 bars total, 24 characters per title |

Bare numeric degrees infer natural major/minor diatonic triads. Thus `6` in C
is Am and `1` in Am is Am. Minor keys are **tonic relative**, not relative-major
numbering: `3` in Am is C. Roman uppercase explicitly selects major; lowercase
selects minor. `V7` in Am is E7, while `ii7` in C is Dm7. Numeric quality suffixes
are explicit: use `2m7` for Dm7 in C and `27` for D7. Alterations are measured
against the selected scale; use `bVII` for a major flat-seventh chord in C.

Chord names, numeric degrees and Roman degrees can be mixed in the same song.
The display always resolves the chord name and a Roman degree, retaining
appropriate note spelling (for example, F# major has E# as its seventh degree).
One guitar voicing is shown for every unannotated chord: familiar open shapes when
available, otherwise a movable shape. The diagram runs from low E at the left
to high e at the right; `x` means muted, `o` open, and the number is the first
displayed fret. Dots specify frets, not finger numbers. Standard EADGBE tuning
and no capo are assumed. Slash inversions, lyrics, repeat shorthand and variable
meters are not supported; unsupported input is rejected instead of skipped.

Titles support printable ASCII, U+4E00 through U+9FEF and punctuation U+3002,
U+300A, U+300B, U+FF01, U+FF08, U+FF09, U+FF0C and U+FF1F. Emoji and extended CJK
are rejected with a title error. Long titles are shortened with an ellipsis in
the compact display, while the full supported title remains in saved text.

Attach an annotation with ASCII parentheses: `C(light strum)` or
`6 (soft arpeggio)`. It belongs to the preceding chord and adds no beats or bars.
The device displays the resolved chord name, one space and the annotation;
it hides that bar's diagram and retains the functional degree below. Long text
scrolls horizontally. Each bar accepts one nonempty annotation of up to 24
characters from the same font-safe set as titles. Spaces are allowed inside;
surrounding spaces are trimmed. Nested parentheses, line breaks inside an
annotation, missing delimiters and annotations before a chord are rejected.

The three score panels occupy most of the display with no bottom key hints or
NOW/NEXT/THEN labels. Chord names use 28 px Latin text, Chinese annotations use
24 px, and the functional degree uses 20 px. Fixed device labels are English
(`COUNT-IN`, `TEMPO`, `CLICK VOLUME`, `SELECT SONG`, `Wi-Fi score editor`);
song titles and annotations retain the user's original language. Tempo and volume
pages omit the step-size and strong/weak-beat captions. The browser editor's
Chinese instructions remain available and describe the current controls.

## Device controls

| Context / input | Behavior |
| --- | --- |
| Score / UP or DOWN | Previous or next bar, clamped at song boundaries |
| Score / double UP | Jump to the first bar |
| Score / double DOWN | Advance two bars |
| Any page / long DOWN | Pause and open tempo selection |
| Any page / long UP | Pause and open metronome volume selection |
| Tempo / UP or DOWN | Increase or decrease by 1 BPM; range 30–120, no wrap |
| Tempo / OK | Apply tempo; press OK again to start with a count-in |
| Volume / UP or DOWN | Increase or decrease by 1; range 0–10, 0 is mute |
| Volume / OK | Apply volume and return to the paused score |
| Score / OK | Start, pause, or resume |
| Any page / long OK | Pause and open song selection |
| Song list / UP or DOWN | Select a song or the Wi-Fi editor information entry |
| Song list / OK | Choose song and its default tempo, or show Wi-Fi information |
| Wi-Fi information / OK | Return to the score |

Single-click recognition waits for the 250 ms double-click window. Double presses
on the tempo/volume/song pages move two steps. Long presses take about 800 ms.
Each start/resume includes one blank
four-beat measure; the selected bar follows at its beginning, regardless of the
pause position. During this count-in the top panel shows the empty measure, then
the selected and next bars. During playback the panels show current, next and
next-after-next, moving once per four beats.

Manual navigation during playback starts the target bar immediately and continues
playing. During count-in it restarts the count-in for the selected target.
Navigation while paused stays paused. The final bar plays its full four beats,
then stops; OK starts again from the beginning with a count-in. Songs do not
automatically loop or advance to another song.

The four side cells alternate bright/dark on each half-beat: 1 bright, 1 dark,
2 bright, 2 dark, and so on. The outlined cell identifies the current beat even
during its dark half. A short click sounds on each eighth note, including the
count-in. Quarter-note clicks are louder and use a 1 kHz tone with a harmonic;
intervening eighth-note clicks are softer, shorter 2 kHz tones. Both follow
playback and the selected tempo, stopping on pause and at song end. Volume
defaults to 5, ranges from 0 to 10, and persists through song changes and score
uploads within the session; reboot resets it to 5. No microphone recording runs.
If codec setup or playback fails, the visual metronome remains available and
`!AUDIO` appears in the score status; restart before retrying audio.

Opening tempo selection retains the exact current BPM. Live tempo overrides
are session-only; choosing a song or
restarting loads the score's default again. Changing the default persistently
requires saving the text in the editor.

## Automatic idle shutdown

After five minutes without interaction while not playing, the device enters
deep sleep. This applies to the paused/finished score, song selection, volume,
tempo, and Wi-Fi information pages. Count-in and playback prevent shutdown;
pausing or finishing starts a fresh five-minute interval. Any physical button
press, browser edit/click/scroll, score read, validation, or save restarts it.
An open browser tab or a connected phone alone does not keep the device awake.

The editor sends activity notifications only following user input, coalesced
over at most three seconds. Inactivity shuts down the hotspot, audio, battery
polling, codec, display and backlight. Saved NVS scores remain intact; the
current bar, temporary tempo/volume and unsaved browser edits are not written
automatically. The browser retains its unsaved text for export or a later retry.

This uses the existing BSP deep-sleep sequence, not physical power removal.
The BSP exposes no software control of the independent hardware power button.
To use the device again, switch hardware power off and on, reconnect to the
hotspot, and reload or retry the editor. No ADC-button wake or timer wake is
enabled. Deep-sleep current and the physical power-cycle procedure must be
verified on the device.

## Implementation and storage

- `guitar_score` parses bounded text and converts scale degrees independently of
  ESP-IDF/LVGL. `guitar_shapes` supplies 180 root/quality voicings.
- `guitar_player` uses absolute monotonic time and integer rational boundaries,
  with no accumulated rounded-period drift at speeds such as 95 BPM.
- `guitar_idle` tests the five-minute boundary independently of ESP-IDF/LVGL.
  A 32-bit atomic activity counter arbitrates user input against the final
  shutdown claim without blocking button callbacks. After the claim, new
  edits are rejected. The shutdown owner releases the model mutex before
  draining HTTP, then requests cooperative worker exits and waits up to eight
  seconds for audio/battery acknowledgments. It suspends CW2017 before ES8311,
  releases I2S then I2C, locks LVGL, sleeps the LCD, and enters deep sleep.
  HTTP stop/worker timeout or failed LVGL locking causes a safe restart;
  peripheral suspend errors are logged and terminal shutdown continues.
- The application worker owns transport; button callbacks only enqueue actions.
  A separate battery worker prevents I2C timeouts from delaying beat calculation.
  It retries failed gauge initialization every five seconds instead of keeping
  the initial failure for the whole session. Valid SOC is polled every 15 seconds;
  unavailable or invalid readings show `--` and retry after five seconds. A
  permanently missing gauge never produces a fabricated percentage. Shared I2C
  initialization and task creation failures still require startup diagnosis.
- `guitar_metronome` calculates eighth-note deadlines and synthesizes bounded
  16 kHz, 16-bit mono PCM without allocation. One audio worker exclusively owns
  BSP audio I/O and receives a one-slot timing mailbox. It uses the same absolute
  origin as the visual player, skips stale beats rather than bursting, and queues
  only 20/35 ms clicks. Real codec latency and sound/display alignment need device
  measurement. Volume uses a quadratic amplitude curve with a fixed codec gain.
- `guitar_ui` draws an application-owned snapshot under LVGL serialization. It
  has one lifetime screen, no per-page background tasks, and no full-screen
  framebuffer. Battery information stays at the top right and degrades to `--`.
- The HTTP worker validates uploads before taking the model mutex, pauses before
  writing Flash, and replaces the active score only after a successful save.
  Request lengths, receive retries and sockets are bounded. The fixed local
  Host and custom edit header reject cross-origin form submissions.
- The original 24 KiB NVS / PHY / single factory partition layout remains intact.
  `guitar/score_v1` is an NVS blob, including a terminator, allowing 4096 text
  bytes beyond the NVS string limit. Existing NVS is never automatically erased
  to recover from an error. Invalid saved data remains stored while a practice
  score is used and a storage warning is shown.
- Annotations share one bounded 4097-byte pool per parsed library; bars store
  offsets. Original score text and the `score_v1` blob format remain compatible.
- The Chinese font is stored in Flash. Its source, exact supported range,
  reproduction command and license are recorded in [assets](../assets/README.md).
  `CONFIG_LV_FONT_FMT_TXT_LARGE=y` is required for its bitmap indices.

## Validation and device acceptance

Activate ESP-IDF 5.5.3 and run the repository gate:

```sh
./tools/validate.sh
```

The static gate includes the actual parser, all voicings, transport boundaries at
every BPM from 30–120, annotation validation, 10,000 malformed inputs, and HTTP/NVS fault injection:
chunked receives, interrupted/oversize requests, dry-run isolation, failed saves,
full-size blobs, annotation save/reload, corrupt data and network startup rollback.
Audio tests cover all 91 tempos, no duplicate/catch-up ticks, mute and volume,
distinct strong/weak PCM, pause/resume/song end, and worker/codec failure paths.
Battery worker tests cover initialization/read recovery, missing gauges, invalid
SOC, 0% and 100%, and bounded retry intervals. Tempo tests cover one-BPM steps
and exact restoration of non-multiple-of-five values.
Idle/shutdown tests cover exact timeout boundaries on every page, playback and
count-in exclusion, activity resets, worker acknowledgments, HTTP shutdown
rejection, peripheral order, and restart on terminal preparation failures.
Browser checks verify interaction notifications cease when the user stops.
It also retains the baseline repository/BSP host tests. macOS uses the matching
linker dead-strip option; Linux keeps its existing section-garbage-collection flag.

Additional rendering and browser checks:

```sh
bash tools/test_guitar_ui.sh
# One-time local browser-test dependencies:
npm install --prefix .tools --no-audit --no-fund playwright@1.58.2
.tools/node_modules/.bin/playwright install chromium
bash tools/test_guitar_web.sh
```

`PLAYWRIGHT_CHROMIUM_EXECUTABLE` can select an already installed Chromium binary.
The browser test uses the shipped HTML and the real C parser with a local test
server; it does not establish Wi-Fi or NVS behavior on hardware. The LVGL test
uses the actual 9.5 renderer, the 24 KiB pool, and the BSP corner mask. It checks
16/24 px font descriptors, a known missing glyph, text bounds, all pages,
annotation marquee rendering and 400 redraws.
Generated PPM/PNG previews are under `build/guitar-preview/`, not device photos.

Before hardware testing, the expected report is `Device tests: NOT RUN`. After
explicit flash approval, verify:

1. Boot, Chinese text, long song names/annotations, diagram legibility and all five pages.
   Check English fixed labels and recovery of the top-right SOC after startup.
2. Single, double and long press discrimination on the ADC ladder.
3. Both 30 and 120 BPM, eight half-beat states, four-beat count-in, scrolling,
   pause/resume, navigation, end-of-song and repeated song changes; audible
   eighth notes, clear quarter-note emphasis, all volume levels and true mute.
4. Hotspot discovery, password-free connection, DHCP, browser access on the target phone, and
   reload after disconnect/reconnect.
5. Valid and invalid uploads, save while playing, repeated 4096-byte saves,
   power-cycle persistence and recovery from an interrupted save.
6. Minimum heap, largest allocation block, task stack high-water marks, button
   responsiveness, audio/display alignment and beat jitter with a phone connected.
7. Leave each non-playing page untouched for five minutes, then verify the
   screen/backlight, audio and hotspot stop. Verify playback stays awake beyond
   five minutes, interaction resets the timer, and merely keeping the browser
   open does not. Measure sleep current; power-cycle and confirm saved scores
   reload. Exercise an upload and button press near the timeout boundary.

Deliver the gate's verified `build/FoloToy-AI-Passport-full.bin` at **offset 0x0**,
with its matching `build/firmware/<sha256>/manifest.json`, ELF and MAP. A merged
flash can reset existing NVS data; export anything that must be kept first.
No full-chip erase or firmware readback is required. Building and USB discovery
do not authorize flashing. See the [flashing policy](development/engineering/firmware-layout.md#flashing-and-stored-data).
