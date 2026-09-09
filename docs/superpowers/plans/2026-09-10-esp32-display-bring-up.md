# ESP32 Display Bring-Up Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build, upload, and physically verify a minimal GC9A01A LCD smoke test on the Waveshare ESP32-S3-Touch-LCD-1.28 Rev3 without integrating the diary renderer or touch yet.

**Architecture:** A root PlatformIO environment compiles only `src/platform/esp32/`. `DisplayHardware` owns the Arduino_GFX SPI bus and GC9A01A display, keeps the backlight off during initialization, and exposes the initialized graphics object to a small diagnostic `main.cpp`. Existing desktop core, UI, renderer, storage, and tests remain outside the embedded build.

**Tech Stack:** C++17, PlatformIO Core 6.2.0, Arduino framework for ESP32-S3, pioarduino platform 55.03.39 with Arduino-ESP32 3.3.9, Arduino_GFX 1.6.7, CH343 serial bridge on COM4.

**Spec:** `docs/superpowers/specs/2026-09-10-esp32-display-bring-up-design.md`

## Global Constraints

- Target hardware is Waveshare ESP32-S3-Touch-LCD-1.28 Rev3 with ESP32-S3, 16 MB quad flash, 2 MB embedded PSRAM, GC9A01A 240 x 240 LCD, and CH343 on COM4.
- Use USB power and data only. Keep the battery disconnected until connector polarity has been independently verified.
- Preserve the factory backup at `C:\Users\gnaki\Desktop\digital_diary-hardware-backups\waveshare-factory-16mb.bin` and its SHA-256 checksum file.
- Do not run `erase_flash`, `write_flash`, or factory recovery commands during this plan.
- Stop for explicit user approval immediately before the first PlatformIO upload.
- LCD pins are BL GPIO2, DC GPIO8, CS GPIO9, CLK GPIO10, MOSI GPIO11, MISO GPIO12, and RST GPIO14.
- Do not initialize the CST816S touch controller, QMI8658 IMU, battery ADC, Wi-Fi, Bluetooth, NTP, LittleFS, `AgendaRenderer`, or application state.
- The smoke frame uses only ASCII text because the Greek U8g2 font belongs to the next hardware stage.
- Keep the established inline workflow on `main`; do not create worktrees or dispatch subagents.
- Do not add production-code comments.
- Commit implementation only after the PlatformIO build, serial output, physical LCD image, and RESET repeat have all passed.

## File Map

- Modify `.gitignore` to exclude PlatformIO and generated VS Code metadata.
- Create `platformio.ini` to define the isolated ESP32-S3 build and pinned graphics dependency.
- Create `src/platform/esp32/board_pins.h` as the single source of truth for Rev3 LCD pins, dimensions, and rotation.
- Create `src/platform/esp32/display_hardware.h` for the small LCD hardware interface.
- Create `src/platform/esp32/display_hardware.cpp` for SPI, GC9A01A, rotation, and backlight initialization.
- Create `src/platform/esp32/main.cpp` for serial diagnostics and the static smoke frame.

---

### Task 1: Establish the isolated PlatformIO build boundary

**Files:**
- Modify: `.gitignore`
- Create: `platformio.ini`
- Create: `src/platform/esp32/board_pins.h`

**Interfaces:**
- Consumes: PlatformIO executable at `C:\Users\gnaki\.platformio\penv\Scripts\platformio.exe`.
- Produces: environment `waveshare_esp32_s3_touch_lcd_1_28` and constants in namespace `Esp32Board`.

- [ ] **Step 1: Reconfirm the clean starting point and tool version**

Run:

```powershell
git status --short
git log --oneline -3

$pio = 'C:\Users\gnaki\.platformio\penv\Scripts\platformio.exe'
& $pio --version
```

Expected:

```text
010b77c docs: design ESP32 display bring-up
PlatformIO Core, version 6.2.0
```

`git status --short` must print nothing. If the commit hash differs only because the implementation plan was committed after `010b77c`, continue only when the working tree is still clean.

- [ ] **Step 2: Replace `.gitignore` with the complete generated-file policy**

Use this exact content:

```gitignore
build/
data/
*.exe
.worktrees/
.pio/
.vscode/c_cpp_properties.json
.vscode/launch.json
.vscode/ipch/
```

- [ ] **Step 3: Create `platformio.ini`**

Use this exact content:

```ini
[platformio]
default_envs = waveshare_esp32_s3_touch_lcd_1_28

[env:waveshare_esp32_s3_touch_lcd_1_28]
platform = https://github.com/pioarduino/platform-espressif32/releases/download/55.03.39/platform-espressif32.zip
board = esp32-s3-devkitc-1
framework = arduino
board_upload.flash_size = 16MB
board_upload.maximum_size = 16777216
board_build.partitions = default_16MB.csv
monitor_speed = 115200
lib_deps =
    moononournation/GFX Library for Arduino@1.6.7
build_src_filter =
    -<*>
    +<platform/esp32/*.cpp>
```

- [ ] **Step 4: Create `src/platform/esp32/board_pins.h`**

Use this exact content:

```cpp
#pragma once

#include <cstdint>

namespace Esp32Board
{
constexpr std::int8_t LcdBacklight = 2;
constexpr std::int8_t LcdDataCommand = 8;
constexpr std::int8_t LcdChipSelect = 9;
constexpr std::int8_t LcdClock = 10;
constexpr std::int8_t LcdMosi = 11;
constexpr std::int8_t LcdMiso = 12;
constexpr std::int8_t LcdReset = 14;

constexpr std::int16_t DisplayWidth = 240;
constexpr std::int16_t DisplayHeight = 240;
constexpr std::uint8_t DisplayRotation = 0;
}
```

- [ ] **Step 5: Run the configuration boundary check before an entry point exists**

Run:

```powershell
$pio = 'C:\Users\gnaki\.platformio\penv\Scripts\platformio.exe'
& $pio run -e waveshare_esp32_s3_touch_lcd_1_28
$LASTEXITCODE
```

Expected: PlatformIO downloads or reuses the pinned platform and Arduino_GFX, then the build fails because `setup()` and `loop()` do not exist yet. `$LASTEXITCODE` must be nonzero. This negative build proves that only the new embedded source directory is being compiled.

- [ ] **Step 6: Check the task diff without staging or committing**

Run:

```powershell
git diff --check
git status --short
```

Expected tracked/untracked project changes:

```text
 M .gitignore
?? platformio.ini
?? src/platform/esp32/
```

The generated `.pio` directory must not appear.

---

### Task 2: Add the GC9A01A hardware wrapper and boot path

**Files:**
- Create: `src/platform/esp32/display_hardware.h`
- Create: `src/platform/esp32/display_hardware.cpp`
- Create: `src/platform/esp32/main.cpp`

**Interfaces:**
- Consumes: `Esp32Board` constants from `board_pins.h`, `Arduino_ESP32SPI`, and `Arduino_GC9A01`.
- Produces: `bool DisplayHardware::begin()`, `void enableBacklight()`, `void disableBacklight()`, and `Arduino_GFX& graphics()`.

- [ ] **Step 1: Create `src/platform/esp32/display_hardware.h`**

Use this exact content:

```cpp
#pragma once

#include <Arduino_GFX_Library.h>

class DisplayHardware
{
public:
    DisplayHardware();

    bool begin();
    void enableBacklight();
    void disableBacklight();

    Arduino_GFX& graphics();

private:
    Arduino_ESP32SPI bus_;
    Arduino_GC9A01 display_;
};
```

- [ ] **Step 2: Create `src/platform/esp32/display_hardware.cpp`**

Use this exact content:

```cpp
#include <Arduino.h>

#include "board_pins.h"
#include "display_hardware.h"

DisplayHardware::DisplayHardware()
    : bus_(
          Esp32Board::LcdDataCommand,
          Esp32Board::LcdChipSelect,
          Esp32Board::LcdClock,
          Esp32Board::LcdMosi,
          Esp32Board::LcdMiso
      ),
      display_(
          &bus_,
          Esp32Board::LcdReset,
          Esp32Board::DisplayRotation,
          true
      )
{
}

bool DisplayHardware::begin()
{
    pinMode(
        Esp32Board::LcdBacklight,
        OUTPUT
    );

    disableBacklight();

    if (!display_.begin())
    {
        return false;
    }

    display_.setRotation(
        Esp32Board::DisplayRotation
    );

    return true;
}

void DisplayHardware::enableBacklight()
{
    digitalWrite(
        Esp32Board::LcdBacklight,
        HIGH
    );
}

void DisplayHardware::disableBacklight()
{
    digitalWrite(
        Esp32Board::LcdBacklight,
        LOW
    );
}

Arduino_GFX& DisplayHardware::graphics()
{
    return display_;
}
```

- [ ] **Step 3: Create the minimal `src/platform/esp32/main.cpp`**

Use this exact content:

```cpp
#include <Arduino.h>

#include "display_hardware.h"

namespace
{
DisplayHardware displayHardware;
}

void setup()
{
    Serial.begin(115200);
    delay(250);

    Serial.println(
        "DISPLAY_SMOKE_START"
    );

    if (!displayHardware.begin())
    {
        Serial.println(
            "DISPLAY_INIT_FAILED"
        );

        return;
    }

    displayHardware.enableBacklight();

    Serial.println(
        "DISPLAY_INIT_OK"
    );
}

void loop()
{
    delay(1000);
}
```

- [ ] **Step 4: Build the boot path**

Run:

```powershell
$pio = 'C:\Users\gnaki\.platformio\penv\Scripts\platformio.exe'
& $pio run -e waveshare_esp32_s3_touch_lcd_1_28

if ($LASTEXITCODE -ne 0)
{
    throw 'ESP32 boot-path build failed.'
}
```

Expected: `SUCCESS` for environment `waveshare_esp32_s3_touch_lcd_1_28` with no linker error and no missing Arduino_GFX header.

- [ ] **Step 5: Verify that desktop modules remain outside the embedded build**

Run:

```powershell
Select-String `
    -Path .pio\build\waveshare_esp32_s3_touch_lcd_1_28\idedata.json `
    -Pattern 'desktop_file_storage|agenda_renderer|agenda_controller'
```

Expected: no matches. If this PlatformIO version does not create `idedata.json`, use:

```powershell
$pio = 'C:\Users\gnaki\.platformio\penv\Scripts\platformio.exe'

& $pio run -e waveshare_esp32_s3_touch_lcd_1_28 -v 2>&1 |
    Select-String 'desktop_file_storage|agenda_renderer|agenda_controller'
```

Expected: no matches.

---

### Task 3: Draw the complete static diagnostic frame

**Files:**
- Modify: `src/platform/esp32/main.cpp`

**Interfaces:**
- Consumes: an initialized `Arduino_GFX&` from `DisplayHardware::graphics()`.
- Produces: a black 240 x 240 frame with turquoise border, RGB squares, and the labels `TOP`, `DIGITAL DIARY`, and `USB`.

- [ ] **Step 1: Replace `src/platform/esp32/main.cpp` with the final smoke program**

Use this exact content:

```cpp
#include <Arduino.h>

#include <cstdint>

#include "board_pins.h"
#include "display_hardware.h"

namespace
{
constexpr std::uint16_t BackgroundColor =
    0x0000;

constexpr std::uint16_t PrimaryTextColor =
    0xFFFF;

constexpr std::uint16_t AccentColor =
    0x06F7;

constexpr std::uint16_t RedColor =
    0xF800;

constexpr std::uint16_t GreenColor =
    0x07E0;

constexpr std::uint16_t BlueColor =
    0x001F;

DisplayHardware displayHardware;

void drawSmokeFrame(
    Arduino_GFX& graphics
)
{
    graphics.fillScreen(
        BackgroundColor
    );

    graphics.drawCircle(
        120,
        120,
        112,
        AccentColor
    );

    graphics.drawCircle(
        120,
        120,
        111,
        AccentColor
    );

    graphics.fillRect(
        49,
        55,
        38,
        38,
        RedColor
    );

    graphics.fillRect(
        101,
        55,
        38,
        38,
        GreenColor
    );

    graphics.fillRect(
        153,
        55,
        38,
        38,
        BlueColor
    );

    graphics.setTextWrap(false);
    graphics.setTextColor(
        PrimaryTextColor
    );
    graphics.setTextSize(2);
    graphics.setCursor(102, 22);
    graphics.print("TOP");

    graphics.setTextColor(
        AccentColor
    );
    graphics.setCursor(42, 112);
    graphics.print("DIGITAL DIARY");

    graphics.drawFastHLine(
        60,
        145,
        120,
        AccentColor
    );

    graphics.setTextColor(
        PrimaryTextColor
    );
    graphics.setCursor(102, 174);
    graphics.print("USB");
}
}

void setup()
{
    Serial.begin(115200);
    delay(250);

    Serial.println(
        "DISPLAY_SMOKE_START"
    );

    if (!displayHardware.begin())
    {
        Serial.println(
            "DISPLAY_INIT_FAILED"
        );

        return;
    }

    Serial.println(
        "DISPLAY_INIT_OK"
    );

    drawSmokeFrame(
        displayHardware.graphics()
    );

    displayHardware.enableBacklight();

    Serial.println(
        "DISPLAY_SMOKE_READY"
    );
}

void loop()
{
    delay(1000);
}
```

- [ ] **Step 2: Run a clean PlatformIO build**

Run:

```powershell
$pio = 'C:\Users\gnaki\.platformio\penv\Scripts\platformio.exe'

& $pio run `
    -e waveshare_esp32_s3_touch_lcd_1_28 `
    -t clean

if ($LASTEXITCODE -ne 0)
{
    throw 'PlatformIO clean failed.'
}

& $pio run `
    -e waveshare_esp32_s3_touch_lcd_1_28

if ($LASTEXITCODE -ne 0)
{
    throw 'ESP32 display smoke build failed.'
}
```

Expected: both commands finish successfully and the final environment summary says `SUCCESS`.

- [ ] **Step 3: Run every desktop regression executable**

Run:

```powershell
$tests = @(
    'test_date_time',
    'test_calendar_engine',
    'test_utf8',
    'test_event',
    'test_t9_engine',
    'test_agenda_manager',
    'test_agenda_state',
    'test_crc32',
    'test_event_codec',
    'test_desktop_file_storage',
    'test_agenda_repository',
    'test_agenda_controller',
    'test_reminder_engine',
    'test_agenda_persistence',
    'test_agenda_ui_state_machine',
    'test_agenda_ui_persistence',
    'test_agenda_renderer'
)

foreach ($test in $tests)
{
    $executable = ".\build\$test.exe"

    if (-not (Test-Path $executable))
    {
        throw "Missing test executable: $test"
    }

    & $executable

    if ($LASTEXITCODE -ne 0)
    {
        throw "Test failed: $test"
    }
}
```

Expected: all 17 suites print their success message.

- [ ] **Step 4: Inspect the complete pre-upload diff**

Run:

```powershell
git diff --check
git status --short
git diff -- .gitignore platformio.ini src/platform/esp32
```

Expected status:

```text
 M .gitignore
?? platformio.ini
?? src/platform/esp32/
```

Do not stage or commit yet.

---

### Task 4: Upload safely and verify the physical LCD

**Files:**
- Potentially modify: `src/platform/esp32/board_pins.h` only if physical orientation requires a rotation correction.

**Interfaces:**
- Consumes: COM4 through the CH343 bridge and the previously verified factory backup.
- Produces: a repeatable physical display smoke result and final commit `feat: add ESP32 display smoke test`.

- [ ] **Step 1: Stop and obtain explicit upload approval**

Before running the next command, state that uploading will replace the currently running Waveshare factory demo while leaving the verified 16 MB backup untouched. Proceed only after the user explicitly approves.

- [ ] **Step 2: Reconfirm the safe physical setup**

Verify all of the following:

- The board is connected only by the known USB data cable.
- Windows still shows `USB-Enhanced-SERIAL CH343 (COM4)`.
- The battery connector is empty.
- The supplied rainbow cable is disconnected.
- No serial monitor or other program is holding COM4.

Run:

```powershell
Get-PnpDevice -PresentOnly |
    Where-Object {
        $_.FriendlyName -match 'CH343|Serial|UART|ESP'
    } |
    Select-Object Status, Class, FriendlyName, InstanceId |
    Format-Table -AutoSize
```

Expected: an `OK` entry for `USB-Enhanced-SERIAL CH343 (COM4)`.

- [ ] **Step 3: Upload the already verified build**

Run:

```powershell
$pio = 'C:\Users\gnaki\.platformio\penv\Scripts\platformio.exe'

& $pio run `
    -e waveshare_esp32_s3_touch_lcd_1_28 `
    -t upload `
    --upload-port COM4

if ($LASTEXITCODE -ne 0)
{
    throw 'ESP32 display smoke upload failed.'
}
```

Expected: upload reaches 100%, verification succeeds, and PlatformIO reports `SUCCESS`. If the command fails, stop and preserve the complete output; do not erase flash.

- [ ] **Step 4: Open the serial monitor and press RESET once**

Run:

```powershell
$pio = 'C:\Users\gnaki\.platformio\penv\Scripts\platformio.exe'
& $pio device monitor --port COM4 --baud 115200
```

With the monitor open, press the physical RESET button once. Expected application lines:

```text
DISPLAY_SMOKE_START
DISPLAY_INIT_OK
DISPLAY_SMOKE_READY
```

Exit the monitor with `Ctrl+C`. `DISPLAY_INIT_FAILED` is a failed checkpoint.

- [ ] **Step 5: Verify the physical frame**

Confirm all of these visible results:

- black background;
- double turquoise circular border;
- red, green, and blue squares in that order from left to right;
- white `TOP` label near the upper edge;
- turquoise `DIGITAL DIARY` centered below the squares;
- white `USB` label nearest the USB connector;
- no mirrored text, clipped frame, persistent flicker, or random pixels.

- [ ] **Step 6: Correct rotation only if the physical result requires it**

Keep the USB connector physically at the bottom and compare the text orientation:

| Observed result with `DisplayRotation = 0` | Set `DisplayRotation` to |
|---|---:|
| `TOP` is already at the top and `USB` at the connector | `0` |
| Image is rotated 90 degrees clockwise | `3` |
| Image is upside down | `2` |
| Image is rotated 90 degrees counterclockwise | `1` |

Change only this line in `src/platform/esp32/board_pins.h`:

```cpp
constexpr std::uint8_t DisplayRotation = 0;
```

Replace `0` with the value from the table, then repeat Task 3 Step 2 and Task 4 Steps 3 through 5. Do not change the drawing coordinates.

- [ ] **Step 7: Verify repeatability with RESET**

Press RESET once more without rebuilding or uploading. Expected:

- the backlight blanks briefly during initialization;
- the same frame reappears;
- the same three serial application lines appear once;
- the display remains stable for at least 30 seconds.

- [ ] **Step 8: Run final build, regression, and whitespace checks**

Run the clean PlatformIO build from Task 3 Step 2 and the 17-test loop from Task 3 Step 3 again, then run:

```powershell
git diff --check
git status --short
```

Expected: every build and test passes, `git diff --check` prints nothing, and only `.gitignore`, `platformio.ini`, and `src/platform/esp32/` are changed.

- [ ] **Step 9: Stage and inspect exactly the intended implementation**

Run:

```powershell
git add .gitignore platformio.ini src/platform/esp32
git diff --cached --check
git status --short
git diff --cached --stat
```

Expected staged paths:

```text
M  .gitignore
A  platformio.ini
A  src/platform/esp32/board_pins.h
A  src/platform/esp32/display_hardware.cpp
A  src/platform/esp32/display_hardware.h
A  src/platform/esp32/main.cpp
```

- [ ] **Step 10: Commit the verified hardware checkpoint**

Run:

```powershell
git commit -m "feat: add ESP32 display smoke test"
git status
git log --oneline -5
```

Expected: commit succeeds and the working tree is clean.

- [ ] **Step 11: Create and verify a complete post-smoke bundle**

Run:

```powershell
git bundle create C:\Users\gnaki\Desktop\digital_diary-display-smoke-complete.bundle --all
git bundle verify C:\Users\gnaki\Desktop\digital_diary-display-smoke-complete.bundle
```

Expected: Git says the bundle records a complete history and is okay.

## Completion Criteria

This plan is complete only when:

- the isolated PlatformIO environment builds from a clean state;
- existing desktop tests all still pass;
- upload through CH343 COM4 succeeds without erase commands;
- serial reports start, successful display initialization, and frame readiness;
- the physical 240 x 240 frame has correct colors, labels, orientation, and stability;
- RESET reproduces the same result;
- Git contains only the reviewed implementation in one clean commit;
- the complete post-smoke bundle verifies successfully.

The next separate stage will adapt the existing `AgendaRenderer` to this physical display. Greek U8g2 text and CST816S touch remain later checkpoints.

## References

- Arduino_GFX ESP32 SPI constructor: `https://github.com/moononournation/Arduino_GFX/blob/v1.6.7/src/databus/Arduino_ESP32SPI.h`
- Arduino_GFX GC9A01A constructor and `begin()`: `https://github.com/moononournation/Arduino_GFX/blob/v1.6.7/src/display/Arduino_GC9A01.h`
- pioarduino PlatformIO platform: `https://github.com/pioarduino/platform-espressif32`
- Waveshare Rev3 schematic: `https://files.waveshare.com/wiki/ESP32-S3-Touch-LCD-1.28/ESP32-S3-Touch-LCD-1.28-Sch.pdf`

## Execution Handoff

The user has selected inline execution for this project stage. Execute this plan with `superpowers:executing-plans`, in order, with a checkpoint after each build batch and before the first upload. Do not use worktrees or subagents unless the user explicitly changes that instruction.
