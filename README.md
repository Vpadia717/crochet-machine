# Crochet Machine

An ESP32-based, Wi-Fi controlled machine that crochets a chain stitch automatically, with a phone/laptop controller page and a library of patterns (flower stem, leaf, rose, sunflower, tulip, bow keychain, star keychain) that are sent to the machine as text.

Status: design and firmware are written but have not yet been run on built hardware. Expect to calibrate positions, servo angles and delays (see Calibration). Only chain stitch is executed by the current mechanism; other stitch types are validated and sent but skipped until row and round hardware is added.

## How it works

One crochet hook is moved up and down by a stepper-driven lead screw. A servo rotates the hook to latch the yarn, and a second servo sweeps a yarn arm around the hook. The ESP32 runs a small web server. A pattern is plain text (for example `CH 40`); the controller page or a command-line tool sends it to the ESP32, which runs it step by step and pauses when you need to swap yarn or assemble a part by hand.

## Repository layout

```
firmware/
  crochet_machine/            v1: chain stitch with a simple web page (start/stop)
  crochet_machine_v2/         v2: pattern upload, pause/resume, CORS for the controller page
controller/
  crochet_controller.html     single-file web controller (open locally, no server needed)
patterns/                     flower patterns as .crochet files
tools/
  send_pattern.py             validate and send a pattern from the command line
  test_patterns.py            checks every pattern and the keychain stitch counts
  sync_controller_library.py  copies patterns/ into the controller page's template list
hardware/
  Crochet_Machine_BOM.xlsx    editable bill of materials with purchase search links
docs/
  Crochet_Machine_Build_Guide.pdf / .docx
  Crochet_Machine_BOM.pdf / .docx
  sources/Keychain.docx       original written patterns for the bow and star keychains
```

Use v2 for everything described below. v1 is kept as a minimal reference for bring-up and testing the mechanism.

## Hardware

Main parts: ESP32 DevKit V1, NEMA 17 stepper with A4988 driver, T8 lead screw with linear rods, two MG90S servos, 12 V 3 A supply with an LM2596 buck converter (5 V for servos), and a limit switch for the home position. The full list with quantities, estimated prices (INR) and purchase search links is in `hardware/Crochet_Machine_BOM.xlsx` and `docs/`. Prices are estimates and were not checked live.

Pin map:

| Signal | ESP32 GPIO | Connects to |
| --- | --- | --- |
| STEP | 26 | A4988 STEP |
| DIR | 27 | A4988 DIR |
| ENABLE | 25 | A4988 EN (LOW = driver on) |
| Hook servo | 18 | MG90S signal |
| Yarn arm servo | 19 | MG90S signal |
| Limit switch | 33 | Switch to GND (internal pull-up) |

Power the servos from the 5 V buck converter, not from the ESP32. Join all grounds. Add a 470 uF capacitor across the A4988 motor supply. The full wiring and assembly procedure is in `docs/Crochet_Machine_Build_Guide.pdf`.

## Quick start

1. Install Arduino IDE 2.x and the ESP32 board package. Install the libraries `AccelStepper` and `ESP32Servo`.
2. Open `firmware/crochet_machine_v2/crochet_machine_v2.ino`. Set `WIFI_SSID` and `WIFI_PASS` (2.4 GHz network). Do not commit real credentials to this repository.
3. Select the board "ESP32 Dev Module" and upload.
4. Open Serial Monitor at 115200 baud and note the printed machine IP address.
5. Open `controller/crochet_controller.html` on a phone or laptop on the same Wi-Fi. Enter the IP, press Connect, choose a pattern, press Send pattern, then Start.

Command-line alternative:

```
python tools/send_pattern.py patterns/chain-cord-stem.crochet --check
python tools/send_pattern.py patterns/chain-cord-stem.crochet --ip 192.168.1.50 --start
```

The tool needs Python 3 and no extra packages. It prints progress and asks you to press Enter when a pattern pauses.

## Pattern language

Patterns are `.crochet` text files, one instruction per line. Everything after `#` is a comment. Instruction names are not case sensitive.

| Instruction | Meaning | Runs on current build |
| --- | --- | --- |
| `CH n` | Chain n stitches | Yes |
| `PAUSE message` | Stop and wait for Resume (swap yarn, assemble, fasten off) | Yes |
| `SC n` | Single crochet, n stitches | No, skipped |
| `HDC n` | Half double crochet, n stitches | No, skipped |
| `DC n` | Double crochet, n stitches | No, skipped |
| `SLST n` | Slip stitch, n stitches | No, skipped |
| `INC n` | n increases (two stitches in one stitch) | No, skipped |
| `MR n` | Magic ring with n single crochet | No, skipped |
| `ROUND n` | A round of n stitches | No, skipped |
| `TURN` | Turn the work | No, skipped |
| `JOIN` | Join into a ring | No, skipped |
| `REPEAT n` ... `END` | Repeat the lines between them n times; blocks can be nested | Expanded before sending |

Counts are whole numbers from 1 to 999. `REPEAT` blocks are expanded by the controller page and by `send_pattern.py` before anything is sent, so the firmware only ever sees a flat list. A pattern can expand to at most 200 steps, and the tools report an error if it goes over.

Example: `(sc, inc) x 5` is written as

```
REPEAT 5
SC 1
INC 1
END
```

## Patterns

| File | Product | Runs now | Needs |
| --- | --- | --- | --- |
| `bow-keychain.crochet` | Bow keychain | Foundation chain only | Row hardware |
| `chain-cord-stem.crochet` | Flower stem or cord | Fully | Nothing extra |
| `leaf.crochet` | Small leaf | Foundation chain only | Row hardware |
| `rose-ruffle-strip.crochet` | Rose, rolled by hand from a ruffled strip | Foundation chain only | Row hardware |
| `star-keychain.crochet` | Star keychain (2 pieces, sewn and stuffed by hand) | Nothing yet | Rotating work holder (magic ring and rounds) |
| `sunflower.crochet` | Sunflower | Petal chain only | Rotating work holder for the centre |
| `tulip.crochet` | Tulip (advanced) | Almost nothing | Rotating work holder and controlled shaping |

The rose, sunflower and tulip patterns are starting outlines, not proven designs. Adjust the counts once you can test them on the machine. Assembly (rolling a rose, attaching petals, stuffing a tulip or star, sewing, embroidering a face) stays manual and appears as PAUSE steps.

### Keychain patterns

The bow and star keychains come from `docs/sources/Keychain.docx`, which also links a video for each. The bow is written exactly as in the source: a 56-chain foundation, then four repeats of the row `6 HDC, 6 SLST, 12 HDC, 6 SLST, 12 HDC, 6 SLST, 6 HDC, turn`. The star is made as two pieces that are sewn together, stuffed and given a stitched face; rounds R1 to R4 follow the source exactly (5, 10, 15, 20 stitches).

The source line for star round R5 is ambiguous. The pattern file reads it as one first point, four repeats of (3 slip stitches, then a point of sc, hdc, dc, hdc, sc), and a final 3 slip stitches. That uses all 20 stitches and gives 5 points. Check this against the video before relying on it, and edit `patterns/star-keychain.crochet` if the video shows something different.

### Adding a pattern

1. Create `patterns/your-pattern.crochet`. Start it with a `# Name: ...` line.
2. Check it: `python tools/send_pattern.py patterns/your-pattern.crochet --check`
3. Run `python tools/sync_controller_library.py` so it appears in the controller page's template list.
4. Run the tests: `python tools/test_patterns.py`

In the controller page you can also use Load file to open any `.crochet` file without syncing.

## Tests

```
python tools/test_patterns.py
```

The tests check that every pattern in `patterns/` is valid and within the step limit, that bad input is rejected, and that the keychain patterns have the stitch counts given in the source document (bow: 56 chain, 144 HDC, 72 SLST, 4 turns; star: two pieces, rounds growing 5, 10, 15, 20). The controller page parses patterns with the same rules as `send_pattern.py`; both produce identical step lists for every pattern in the repository.

## Machine API

The ESP32 serves plain HTTP on port 80. Responses include `Access-Control-Allow-Origin: *` so the local controller page can call it.

| Endpoint | Method | Purpose |
| --- | --- | --- |
| `/program` | POST (text/plain) | Load a pattern; refused while running |
| `/start` | GET | Run the loaded pattern. `/start?n=20` runs 20 chain stitches |
| `/stop` | GET | Stop after the current stitch |
| `/resume` | GET | Continue after a PAUSE step |
| `/home` | GET | Re-run homing against the limit switch |
| `/status` | GET | JSON: state, homed, step, steps, done, msg |

There is no authentication. Keep the machine on a trusted home network and never expose it to the internet.

## Calibration

Edit the constants at the top of the sketch: `Z_DOWN` (hook depth in steps), `HOOK_OPEN` and `HOOK_LATCH`, `ARM_PARK` and `ARM_WRAP_END`, and the delays in `stitch()`. Test homing first, then each servo, then a single stitch, then 20 stitches. With full-step mode, a 200-step motor and an 8 mm lead screw move 8 mm per 200 steps. If the carriage moves away from the limit switch during homing, change the sign of `z.setSpeed(-400)` or reverse one motor coil pair.

## Troubleshooting

| Problem | Likely cause and fix |
| --- | --- |
| Controller cannot connect | Same 2.4 GHz network? Correct IP? Open the page as a local file or over http, not from an https site |
| ESP32 resets when servos move | Servos on the wrong rail; use the buck converter and add a capacitor |
| Motor vibrates but does not turn | Coil wires mismatched; identify the coil pairs with a multimeter |
| Homing fails | Check limit switch wiring (COM to GPIO 33, NO to GND) |
| Yarn does not catch | Adjust the arm sweep and `HOOK_LATCH`; use smooth cotton and light tension |

More detail is in the build guide.

## Roadmap

1. Reliable chain stitch (current build).
2. Work holder with a sliding or turning axis for flat single-crochet rows (leaf, rose strip).
3. Rotating axis for rounds (sunflower centre, coasters).
4. Shaped 3D items (tulip) with controlled increases and decreases.
5. Yarn-out detection, OTA firmware updates, quieter TMC2209 driver.

## Safety

Disconnect power before changing wiring. Never connect or disconnect the stepper motor while the driver is powered. Keep fingers clear of the hook and yarn arm while the machine runs. Set the buck converter to 5.0 V with a multimeter before connecting servos.

## License

Private repository. All rights reserved unless a license file is added.
