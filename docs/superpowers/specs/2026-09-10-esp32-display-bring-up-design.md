# ESP32 Display Bring-Up Design

Date: 2026-09-10

Status: Approved

## Goal

Introduce the first embedded build for the Digital Diary and verify the
physical GC9A01A display on the Waveshare ESP32-S3-Touch-LCD-1.28 Rev3.

The first firmware is an isolated LCD smoke test. It verifies the board
configuration, SPI communication, display pins, backlight, colors,
geometry, text, and physical orientation before the existing renderer or
other hardware subsystems are introduced.

## Confirmed hardware

The connected device was identified through esptool as:

- ESP32-S3 QFN56 revision 0.2;
- dual-core CPU with LP core at up to 240 MHz;
- 2 MB embedded PSRAM;
- 16 MB external Flash;
- 40 MHz crystal;
- quad Flash interface at 3.3 V;
- CH343 USB-to-serial adapter on COM4.

The factory firmware confirmed that these components work:

- circular 240 x 240 LCD;
- backlight;
- capacitive touch and scrolling;
- 16 MB Flash detection;
- QMI8658 motion sensing through changing Angular Deflection values;
- USB data connection and serial enumeration.

## Factory firmware backup

A complete read-only backup of the 16 MB factory Flash was created
before any custom firmware upload.

Backup location:

    C:\Users\gnaki\Desktop\digital_diary-hardware-backups\
    waveshare-factory-16mb.bin

Expected size:

    16777216 bytes

SHA-256:

    CE5650464B4FDE97AEFA9E35FE41B360F0767D7C690F27766B3600BE0AD482A7

The checksum is also stored beside the binary in:

    waveshare-factory-16mb.sha256.txt

The backup must not be committed to the Git repository. Restoring it is
a separate destructive operation and requires an explicit checkpoint.

## Development environment

The embedded build uses:

- Visual Studio Code;
- PlatformIO IDE 3.3.4;
- PlatformIO Core 6.2.0;
- Arduino framework for ESP32-S3;
- Arduino_GFX as the display library;
- U8g2 integration in a later Greek-font stage.

Normal Digital Diary operation remains completely offline. Internet is
needed only while PlatformIO initially downloads toolchains,
frameworks, and pinned library dependencies.

## Architectural approach

PlatformIO is configured at the repository root.

The existing project layout remains unchanged. Desktop code and tests
continue to use the existing manual C++17 g++ build commands.

The embedded implementation is placed under:

    src/platform/esp32/

A PlatformIO source filter initially compiles only that directory. This
prevents desktop-only source files, including DesktopFileStorage, from
entering the embedded build.

Later stages will deliberately extend the source filter to include the
required files from:

    src/core/
    src/ui/

This avoids copying shared code, creating Windows symlinks, or
reorganizing already tested modules.

## Proposed files

The first implementation introduces:

    platformio.ini
    src/platform/esp32/board_pins.h
    src/platform/esp32/display_hardware.h
    src/platform/esp32/display_hardware.cpp
    src/platform/esp32/main.cpp

No existing production source file needs to change during the initial
LCD smoke-test stage.

## PlatformIO configuration

The PlatformIO environment uses an ESP32-S3 board profile with the
Arduino framework.

The configuration records:

- 16 MB Flash;
- 115200 baud serial monitoring;
- Arduino_GFX as a pinned dependency;
- an explicit embedded source filter;
- a named Waveshare display environment.

COM4 is supplied at upload or monitor time rather than permanently
stored in the repository. This keeps the configuration portable if
Windows assigns a different COM port later.

PSRAM is not required by the smoke test. Its use is deferred until a
later stage has a demonstrated memory requirement.

## Display pin map

The Waveshare Rev3 display uses:

| Signal | GPIO |
|---|---:|
| Backlight | 2 |
| Data/Command | 8 |
| Chip Select | 9 |
| SPI Clock | 10 |
| SPI MOSI | 11 |
| SPI MISO | 12 |
| Reset | 14 |

The display controller is GC9A01A with a resolution of 240 x 240 pixels.

Touch pins are documented but excluded from this stage:

| Signal | GPIO |
|---|---:|
| Touch Interrupt | 5 |
| I2C SDA | 6 |
| I2C SCL | 7 |
| Touch Reset | 13 |

The QMI8658 motion sensor shares the I2C bus on GPIO6 and GPIO7. Neither
touch nor motion sensing is initialized by the smoke firmware.

## Component responsibilities

### board_pins.h

This file contains only named constants for the verified Rev3 pins,
display dimensions, backlight active level, and initial rotation.

No application logic or drawing behavior belongs in this file.

### display_hardware.h and display_hardware.cpp

This component owns:

- the Arduino SPI display bus;
- the Arduino_GFX GC9A01A object;
- display initialization;
- display rotation;
- backlight enable and disable;
- access to the initialized graphics surface.

It does not own:

- application state;
- AgendaRenderer;
- touch handling;
- persistence;
- clock behavior;
- reminder behavior.

The backlight starts disabled. It is enabled only after the display has
initialized and the first complete frame has been drawn.

### main.cpp

The embedded entry point owns only the smoke-test sequence.

Its setup function:

1. starts Serial at 115200 baud;
2. prints the smoke-test startup message;
3. disables the backlight;
4. initializes the display hardware;
5. reports initialization success or failure;
6. draws the complete diagnostic frame;
7. enables the backlight;
8. reports that the display is ready.

Its loop function remains idle. It performs no animation, continuous
redraw, touch polling, storage access, Wi-Fi access, or reminder polling.

## Diagnostic frame

The smoke-test frame uses a black background and contains:

- a turquoise circular outline near the physical display edge;
- a red shape;
- a green shape;
- a blue shape;
- the text `TOP` near the physical top;
- the text `DIGITAL DIARY` near the center;
- the text `USB` near the physical USB-C connector.

The test uses only built-in ASCII rendering. U8g2 and Greek glyphs are
introduced after the basic display path succeeds.

The diagnostic frame verifies:

- red, green, and blue channel order;
- black background clearing;
- circle and shape geometry;
- basic text drawing;
- clipping near the circular boundary;
- display orientation.

## Orientation

The intended physical orientation places:

- `TOP` opposite the USB-C connector;
- `USB` beside the USB-C connector.

If these labels are inverted or rotated, only the display rotation
constant changes. Pin assignments and drawing coordinates remain
unchanged.

## Serial output

A successful startup prints:

    Digital Diary LCD smoke test
    Display initialization started
    Display ready

An initialization failure prints:

    Digital Diary LCD smoke test
    Display initialization started
    Display initialization failed

If initialization fails, the backlight remains disabled and the loop
stays idle.

## Error handling

The smoke firmware performs no automatic retry.

A failed PlatformIO build stops before any upload.

A failed upload does not trigger an erase command. Diagnosis checks:

1. whether the board still appears as a serial device;
2. whether Windows still assigns COM4;
3. whether another serial monitor owns the port;
4. whether manual download mode is required.

The BOOT and RESET buttons are used only when automatic bootloader entry
fails.

The factory backup is restored only if the custom firmware cannot be
recovered normally and only after a separate explicit decision.

## Verification checkpoints

Implementation follows these checkpoints in order:

1. verify a clean Git working tree;
2. add the PlatformIO configuration and embedded files;
3. resolve and pin dependencies;
4. run a PlatformIO build without uploading;
5. confirm that the ESP32-S3 firmware binary was produced;
6. run the desktop regression suites;
7. run `git diff --check`;
8. inspect the complete Git diff;
9. request explicit approval for the first upload;
10. upload through USB with the battery disconnected;
11. inspect the physical diagnostic frame;
12. verify Serial output;
13. press RESET and verify the same frame appears again;
14. commit only after build, regression, and physical checks pass.

## Physical acceptance criteria

The LCD smoke test passes only when all of these are observed:

- the display lights after initialization;
- the background is black;
- the turquoise outline is visible and circular;
- red, green, and blue are not swapped;
- all three text labels are readable;
- `TOP` is physically above the center;
- `USB` points toward the USB-C connector;
- no repeated flicker or reboot occurs;
- the successful Serial messages appear;
- pressing RESET reproduces the same result.

## Safety constraints

During this stage:

- power comes only from USB;
- the external battery remains disconnected;
- the expansion cable remains disconnected;
- no Flash erase command is used;
- no factory-backup restore is attempted;
- the board is handled by its edges;
- upload begins only after the build checkpoint passes.

Battery integration is deferred until connector polarity has been
independently verified.

## Testing strategy

Desktop tests continue to validate all hardware-independent behavior.

The embedded smoke test has three verification layers:

1. PlatformIO compile and link verification;
2. Serial startup output;
3. physical inspection of the 240 x 240 display.

No automated desktop test is added for Arduino_GFX initialization
because it depends on the physical SPI controller and panel. The
existing AgendaRenderer recording-surface tests remain the automated
source of truth for hardware-independent drawing behavior.

## Version-control strategy

Design, implementation plan, and implementation use separate commits.

The implementation commit contains only:

- PlatformIO configuration;
- ESP32 Rev3 pin definitions;
- display hardware initialization;
- smoke-test entry point.

Generated PlatformIO directories and firmware binaries must remain
untracked.

## Non-goals

This stage does not include:

- AgendaRenderer integration;
- DrawSurface implementation for Arduino_GFX;
- U8g2 or Greek fonts;
- CST816S touch initialization;
- touch-coordinate mapping;
- QMI8658 integration;
- LittleFS persistence;
- application controller startup;
- offline clock implementation;
- reminders;
- battery operation;
- sleep and wake behavior;
- Wi-Fi, Bluetooth, or NTP.

## Handoff after the smoke test

After the LCD smoke test passes, the next stages are:

1. implement an Arduino_GFX-backed DrawSurface;
2. render the existing AgendaRenderer on the physical display;
3. add a Greek UTF-8 font through U8g2;
4. initialize and test CST816S touch;
5. map touch regions to AgendaUiAction;
6. integrate LittleFS, clock, controller, and the full application loop;
7. perform battery, sleep/wake, and physical-device QA.

The smoke test is the first hardware checkpoint, not the final device.
