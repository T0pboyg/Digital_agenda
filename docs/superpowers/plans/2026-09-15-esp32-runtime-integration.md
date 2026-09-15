# ESP32 Runtime Integration Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the ESP32 display smoke screen with the complete offline Digital Diary runtime, including touch input, clock setup, primary/backup persistence, rendering, UI actions, and minute-based reminder checks.

**Architecture:** Keep `src/platform/esp32/main.cpp` as a thin composition root. A testable `AgendaRuntime` coordinates a generic touch input, release detection, the existing touch mapper/state machine/renderer, and unsigned `millis()` scheduling; concrete CST816S, LittleFS, ESP32 clock, and Arduino_GFX adapters stay under `src/platform/esp32/`.

**Tech Stack:** C++17, assert-based desktop tests with MSYS2 `g++`, PlatformIO Core 6.2.0, pioarduino ESP32 platform 55.03.39, Arduino framework 3.3.9, Arduino_GFX 1.6.7, U8g2 2.36.18, ESP32 LittleFS, GC9A01A, CST816S, Waveshare ESP32-S3-Touch-LCD-1.28 Rev3.

**Spec:** `docs/superpowers/specs/2026-09-15-esp32-runtime-integration-design.md`

## Global Constraints

- Start from clean commit `3561a67` (`docs: fix runtime design formatting`).
- Keep normal Digital Diary operation fully offline: no Wi-Fi, NTP, Bluetooth, or other online service.
- Use C++17 and compile every desktop test with `-Wall -Wextra -pedantic -Isrc`.
- Compile each changed test executable before running it; never trust an older `.exe`.
- Poll touch at 10 ms intervals and call `AgendaUiStateMachine::tick()` at 60,000 ms intervals.
- Use unsigned 32-bit subtraction for all `millis()` interval checks so wraparound remains correct.
- Dispatch at most one `AgendaUiAction` for each touch release.
- Keep the existing 240×240 UI geometry and display rotation `0`; valid coordinates are `0..239` on each axis.
- Reserve GPIO5 as `TouchInterrupt`; do not enable interrupts, sleep, or wake-on-touch in this milestone.
- Use CST816S address `0x15`, first data register `0x02`, exactly five data bytes, SDA GPIO6, SCL GPIO7, and reset GPIO13.
- Use `/agenda.bin` as primary storage and `/agenda.backup.bin` as backup storage.
- Mount LittleFS with `LittleFS.begin(false)` only; never format or erase Flash automatically.
- Use compiler date/time only as the initial `ClockSetup` draft. Invalid compiler text falls back to `2026-01-01 12:00`.
- Keep the current `AgendaRepository`, `AgendaController`, `AgendaUiStateMachine`, `AgendaRenderer`, and `AgendaTouchMapper` interfaces unchanged.
- Keep Arduino, Wire, LittleFS, and Arduino_GFX dependencies outside `src/core/`.
- Keep the battery disconnected throughout implementation and USB acceptance testing.
- Run `git diff --check` before staging and `git diff --cached --check` before each commit.
- Do not upload firmware until the user explicitly approves the upload command.

## File Map

### New platform-independent files

- Create `src/core/compile_date_time.h` and `.cpp`: parse `__DATE__`/`__TIME__` and return the safe default on invalid input.
- Create `src/ui/touch_release_detector.h` and `.cpp`: retain the latest valid contact and emit one release point.
- Create `src/ui/touch_input.h`: define the hardware-neutral touch read result and interface.
- Create `src/ui/agenda_runtime.h` and `.cpp`: schedule touch polling, action dispatch, minute ticks, and redraws.

### Modified ESP32 files

- Modify `src/platform/esp32/board_pins.h`: add the verified GPIO5 touch-interrupt constant without using it yet.
- Modify `src/platform/esp32/touch_hardware.h` and `.cpp`: replace ambiguous `bool read(TouchSample&)` with testable `Cst816sTouchHardware` and explicit error results.
- Modify `src/platform/esp32/littlefs_byte_storage.h` and `.cpp`: add the production path-only constructor.
- Replace the smoke-screen code in `src/platform/esp32/main.cpp`: compose and boot the real application.
- Modify `platformio.ini`: compile root touch decoding, all core/UI sources, and ESP32 platform sources while excluding desktop storage.

### New and modified tests

- Create `tests/test_compile_date_time.cpp`.
- Create `tests/test_touch_release_detector.cpp`.
- Create `tests/test_cst816s_touch_hardware.cpp`.
- Modify `tests/test_littlefs_byte_storage.cpp`.
- Create `tests/test_agenda_runtime.cpp`.

---

### Task 1: Parse the firmware compilation timestamp

**Files:**
- Create: `src/core/compile_date_time.h`
- Create: `src/core/compile_date_time.cpp`
- Test: `tests/test_compile_date_time.cpp`

**Interfaces:**
- Consumes: `DateTime`, `Date`, `TimeOfDay`, `isValidDate()`, and `isValidTime()` from `src/core/date_time.h`.
- Produces: `DateTime compileDateTimeOrFallback(std::string_view dateText, std::string_view timeText)`.

- [ ] **Step 1: Write three failing parser tests**

Create `tests/test_compile_date_time.cpp`:

```cpp
#include <cassert>
#include <iostream>

#include "core/compile_date_time.h"

namespace
{
void assertDateTime(
    const DateTime& actual,
    int year,
    int month,
    int day,
    int hour,
    int minute
)
{
    assert(actual.date.year == year);
    assert(actual.date.month == month);
    assert(actual.date.day == day);
    assert(actual.time.hour == hour);
    assert(actual.time.minute == minute);
}

void test_parses_normal_compiler_date_and_time()
{
    const DateTime value =
        compileDateTimeOrFallback(
            "Sep 15 2026",
            "14:37:52"
        );

    assertDateTime(value, 2026, 9, 15, 14, 37);
}

void test_parses_space_padded_single_digit_day()
{
    const DateTime value =
        compileDateTimeOrFallback(
            "Jan  7 2026",
            "08:05:09"
        );

    assertDateTime(value, 2026, 1, 7, 8, 5);
}

void test_invalid_compiler_text_returns_safe_fallback()
{
    const DateTime value =
        compileDateTimeOrFallback(
            "Bad input",
            "99:88:77"
        );

    assertDateTime(value, 2026, 1, 1, 12, 0);
}
}

int main()
{
    test_parses_normal_compiler_date_and_time();
    test_parses_space_padded_single_digit_day();
    test_invalid_compiler_text_returns_safe_fallback();

    std::cout
        << "All compile date/time tests passed.\n";

    return 0;
}
```

- [ ] **Step 2: Compile and confirm RED**

```powershell
New-Item -ItemType Directory -Force build | Out-Null
g++ -std=c++17 -Wall -Wextra -pedantic -Isrc tests\test_compile_date_time.cpp src\core\date_time.cpp -o build\test_compile_date_time.exe
```

Expected: compilation fails with `core/compile_date_time.h: No such file or directory`.

- [ ] **Step 3: Add the parser declaration**

Create `src/core/compile_date_time.h`:

```cpp
#pragma once

#include <string_view>

#include "date_time.h"

DateTime compileDateTimeOrFallback(
    std::string_view dateText,
    std::string_view timeText
);
```

- [ ] **Step 4: Add the minimal validated parser**

Create `src/core/compile_date_time.cpp`:

```cpp
#include "compile_date_time.h"

#include <array>

namespace
{
constexpr DateTime SafeFallback{
    Date{2026, 1, 1},
    TimeOfDay{12, 0}
};

int digitValue(char value)
{
    if (value < '0' || value > '9')
    {
        return -1;
    }

    return value - '0';
}

int parseTwoDigits(
    std::string_view text,
    std::size_t offset
)
{
    const int first = digitValue(text[offset]);
    const int second = digitValue(text[offset + 1]);

    if (first < 0 || second < 0)
    {
        return -1;
    }

    return first * 10 + second;
}

int parseFourDigits(
    std::string_view text,
    std::size_t offset
)
{
    int value = 0;

    for (std::size_t index = 0; index < 4; ++index)
    {
        const int digit =
            digitValue(text[offset + index]);

        if (digit < 0)
        {
            return -1;
        }

        value = value * 10 + digit;
    }

    return value;
}

int parseMonth(std::string_view abbreviation)
{
    constexpr std::array<std::string_view, 12>
        Months{
            "Jan", "Feb", "Mar", "Apr",
            "May", "Jun", "Jul", "Aug",
            "Sep", "Oct", "Nov", "Dec"
        };

    for (
        std::size_t index = 0;
        index < Months.size();
        ++index
    )
    {
        if (Months[index] == abbreviation)
        {
            return static_cast<int>(index) + 1;
        }
    }

    return 0;
}
}

DateTime compileDateTimeOrFallback(
    std::string_view dateText,
    std::string_view timeText
)
{
    if (
        dateText.size() != 11 ||
        timeText.size() != 8 ||
        dateText[3] != ' ' ||
        dateText[6] != ' ' ||
        timeText[2] != ':' ||
        timeText[5] != ':'
    )
    {
        return SafeFallback;
    }

    const int month =
        parseMonth(dateText.substr(0, 3));

    const int day =
        dateText[4] == ' '
            ? digitValue(dateText[5])
            : parseTwoDigits(dateText, 4);

    const int year =
        parseFourDigits(dateText, 7);

    const int hour =
        parseTwoDigits(timeText, 0);

    const int minute =
        parseTwoDigits(timeText, 3);

    const int second =
        parseTwoDigits(timeText, 6);

    const DateTime value{
        Date{year, month, day},
        TimeOfDay{hour, minute}
    };

    if (
        !isValidDate(value.date) ||
        !isValidTime(value.time) ||
        second < 0 ||
        second > 59
    )
    {
        return SafeFallback;
    }

    return value;
}
```

- [ ] **Step 5: Compile again, then run GREEN**

```powershell
g++ -std=c++17 -Wall -Wextra -pedantic -Isrc tests\test_compile_date_time.cpp src\core\compile_date_time.cpp src\core\date_time.cpp -o build\test_compile_date_time.exe
if ($LASTEXITCODE -ne 0) { throw 'Compile date/time test compile failed.' }
.\build\test_compile_date_time.exe
if ($LASTEXITCODE -ne 0) { throw 'Compile date/time tests failed.' }
```

Expected: `All compile date/time tests passed.`

- [ ] **Step 6: Check and commit**

```powershell
git diff --check
git add -- src/core/compile_date_time.h src/core/compile_date_time.cpp tests/test_compile_date_time.cpp
git diff --cached --check
git commit -m "feat: parse firmware compilation time"
```

---

### Task 2: Emit exactly one valid point per touch release

**Files:**
- Create: `src/ui/touch_release_detector.h`
- Create: `src/ui/touch_release_detector.cpp`
- Test: `tests/test_touch_release_detector.cpp`

**Interfaces:**
- Consumes: `TouchSample` from `src/touch_sample.h` and `UiPoint`/display dimensions from `src/ui/ui_geometry.h`.
- Produces: `std::optional<UiPoint> TouchReleaseDetector::update(const TouchSample& sample)`.

- [ ] **Step 1: Write three failing release-detector tests**

Create `tests/test_touch_release_detector.cpp`:

```cpp
#include <cassert>
#include <iostream>

#include "ui/touch_release_detector.h"

namespace
{
void test_press_move_release_emits_final_position()
{
    TouchReleaseDetector detector;

    assert(!detector.update({true, 30, 40}));
    assert(!detector.update({true, 35, 47}));

    const auto released =
        detector.update({false, 0, 0});

    assert(released.has_value());
    assert(released->x == 35);
    assert(released->y == 47);
}

void test_release_is_emitted_only_once()
{
    TouchReleaseDetector detector;

    assert(!detector.update({true, 100, 120}));
    assert(detector.update({false, 0, 0}));
    assert(!detector.update({false, 0, 0}));
    assert(!detector.update({false, 0, 0}));
}

void test_out_of_range_touch_does_not_create_release()
{
    TouchReleaseDetector detector;

    assert(!detector.update({true, 240, 20}));
    assert(!detector.update({true, 20, 240}));
    assert(!detector.update({false, 0, 0}));
}
}

int main()
{
    test_press_move_release_emits_final_position();
    test_release_is_emitted_only_once();
    test_out_of_range_touch_does_not_create_release();

    std::cout
        << "All touch release detector tests passed.\n";

    return 0;
}
```

- [ ] **Step 2: Compile and confirm RED**

```powershell
g++ -std=c++17 -Wall -Wextra -pedantic -Isrc tests\test_touch_release_detector.cpp -o build\test_touch_release_detector.exe
```

Expected: compilation fails with `ui/touch_release_detector.h: No such file or directory`.

- [ ] **Step 3: Add the detector declaration**

Create `src/ui/touch_release_detector.h`:

```cpp
#pragma once

#include <optional>

#include "touch_sample.h"
#include "ui_geometry.h"

class TouchReleaseDetector
{
public:
    std::optional<UiPoint> update(
        const TouchSample& sample
    );

private:
    std::optional<UiPoint> latestPoint_;
};
```

- [ ] **Step 4: Add the release state machine**

Create `src/ui/touch_release_detector.cpp`:

```cpp
#include "touch_release_detector.h"

std::optional<UiPoint> TouchReleaseDetector::update(
    const TouchSample& sample
)
{
    if (sample.touched)
    {
        if (
            sample.x < AGENDA_UI_DISPLAY_WIDTH &&
            sample.y < AGENDA_UI_DISPLAY_HEIGHT
        )
        {
            latestPoint_ = UiPoint{
                static_cast<int>(sample.x),
                static_cast<int>(sample.y)
            };
        }

        return std::nullopt;
    }

    if (!latestPoint_.has_value())
    {
        return std::nullopt;
    }

    const UiPoint released = *latestPoint_;
    latestPoint_.reset();

    return released;
}
```

- [ ] **Step 5: Compile again, then run GREEN**

```powershell
g++ -std=c++17 -Wall -Wextra -pedantic -Isrc tests\test_touch_release_detector.cpp src\ui\touch_release_detector.cpp -o build\test_touch_release_detector.exe
if ($LASTEXITCODE -ne 0) { throw 'Touch release detector compile failed.' }
.\build\test_touch_release_detector.exe
if ($LASTEXITCODE -ne 0) { throw 'Touch release detector tests failed.' }
```

Expected: `All touch release detector tests passed.`

- [ ] **Step 6: Check and commit**

```powershell
git diff --check
git add -- src/ui/touch_release_detector.h src/ui/touch_release_detector.cpp tests/test_touch_release_detector.cpp
git diff --cached --check
git commit -m "feat: detect touch releases"
```

---

### Task 3: Make the CST816S adapter explicit and host-testable

**Files:**
- Create: `src/ui/touch_input.h`
- Modify: `src/platform/esp32/board_pins.h`
- Modify: `src/platform/esp32/touch_hardware.h`
- Modify: `src/platform/esp32/touch_hardware.cpp`
- Test: `tests/test_cst816s_touch_hardware.cpp`

**Interfaces:**
- Produces: `TouchInputError`, `TouchInputReadResult`, and abstract `TouchInput::read()` for `AgendaRuntime`.
- Produces: `TouchInputError Cst816sTouchHardware::begin()` and `TouchInputReadResult Cst816sTouchHardware::read()`.
- Reuses: `decodeCst816sSample()` and `cst816sDisableAutoSleepCommand()`.

- [ ] **Step 1: Write three failing CST816S adapter tests**

Create `tests/test_cst816s_touch_hardware.cpp`:

```cpp
#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <vector>

#include "platform/esp32/board_pins.h"
#include "platform/esp32/touch_hardware.h"

namespace
{
std::int8_t configuredResetPin = -1;
std::vector<bool> resetLevels;
std::vector<std::uint32_t> delays;
std::int8_t receivedSda = -1;
std::int8_t receivedScl = -1;
std::uint32_t receivedClock = 0;
std::uint8_t probedAddress = 0;
std::uint8_t writtenAddress = 0;
std::uint8_t writtenRegister = 0;
std::uint8_t writtenValue = 0;
std::uint8_t readAddress = 0;
std::uint8_t readFirstRegister = 0;
std::size_t readCount = 0;
bool readSucceeds = true;
std::array<std::uint8_t, 5> registerBytes{};

void fakeConfigureResetPin(std::int8_t pin)
{
    configuredResetPin = pin;
}

void fakeWriteResetPin(
    std::int8_t,
    bool high
)
{
    resetLevels.push_back(high);
}

void fakeDelay(std::uint32_t milliseconds)
{
    delays.push_back(milliseconds);
}

bool fakeBeginBus(
    std::int8_t sda,
    std::int8_t scl,
    std::uint32_t clock
)
{
    receivedSda = sda;
    receivedScl = scl;
    receivedClock = clock;
    return true;
}

bool fakeProbe(std::uint8_t address)
{
    probedAddress = address;
    return true;
}

bool fakeWriteRegister(
    std::uint8_t address,
    std::uint8_t registerAddress,
    std::uint8_t value
)
{
    writtenAddress = address;
    writtenRegister = registerAddress;
    writtenValue = value;
    return true;
}

bool fakeReadRegisters(
    std::uint8_t address,
    std::uint8_t firstRegister,
    std::uint8_t* bytes,
    std::size_t count
)
{
    readAddress = address;
    readFirstRegister = firstRegister;
    readCount = count;

    if (!readSucceeds)
    {
        return false;
    }

    std::copy(
        registerBytes.begin(),
        registerBytes.end(),
        bytes
    );

    return true;
}

void resetFakes()
{
    configuredResetPin = -1;
    resetLevels.clear();
    delays.clear();
    receivedSda = -1;
    receivedScl = -1;
    receivedClock = 0;
    probedAddress = 0;
    writtenAddress = 0;
    writtenRegister = 0;
    writtenValue = 0;
    readAddress = 0;
    readFirstRegister = 0;
    readCount = 0;
    readSucceeds = true;
    registerBytes = {};
}

Cst816sTouchHardware makeHardware()
{
    return Cst816sTouchHardware{
        Cst816sTouchApi{
            fakeConfigureResetPin,
            fakeWriteResetPin,
            fakeDelay,
            fakeBeginBus,
            fakeProbe,
            fakeWriteRegister,
            fakeReadRegisters
        }
    };
}

void test_begin_resets_and_configures_the_verified_bus()
{
    resetFakes();
    Cst816sTouchHardware hardware = makeHardware();

    assert(hardware.begin() == TouchInputError::None);
    assert(configuredResetPin == Esp32Board::TouchReset);
    assert((resetLevels == std::vector<bool>{false, true}));
    assert((delays == std::vector<std::uint32_t>{10, 50}));
    assert(receivedSda == Esp32Board::TouchSda);
    assert(receivedScl == Esp32Board::TouchScl);
    assert(receivedClock == 400000);
    assert(probedAddress == 0x15);
    assert(writtenAddress == 0x15);
    assert(writtenRegister == 0xFE);
    assert(writtenValue == 0x01);
    static_assert(Esp32Board::TouchInterrupt == 5);
}

void test_read_fetches_exactly_five_bytes_and_decodes_them()
{
    resetFakes();
    registerBytes = {0x01, 0xA0, 0x7B, 0xB0, 0x95};

    Cst816sTouchHardware hardware = makeHardware();
    assert(hardware.begin() == TouchInputError::None);

    const TouchInputReadResult result = hardware.read();

    assert(result.error == TouchInputError::None);
    assert(result.sample.touched);
    assert(result.sample.x == 123);
    assert(result.sample.y == 149);
    assert(readAddress == 0x15);
    assert(readFirstRegister == 0x02);
    assert(readCount == 5);
}

void test_i2c_failure_is_not_reported_as_finger_release()
{
    resetFakes();
    Cst816sTouchHardware hardware = makeHardware();
    assert(hardware.begin() == TouchInputError::None);

    readSucceeds = false;

    const TouchInputReadResult result = hardware.read();

    assert(result.error == TouchInputError::ReadFailed);
    assert(!result.sample.touched);
}
}

int main()
{
    test_begin_resets_and_configures_the_verified_bus();
    test_read_fetches_exactly_five_bytes_and_decodes_them();
    test_i2c_failure_is_not_reported_as_finger_release();

    std::cout
        << "All CST816S touch hardware tests passed.\n";

    return 0;
}
```

- [ ] **Step 2: Compile and confirm RED**

```powershell
g++ -std=c++17 -Wall -Wextra -pedantic -Isrc tests\test_cst816s_touch_hardware.cpp src\platform\esp32\touch_hardware.cpp src\touch_sample.cpp -o build\test_cst816s_touch_hardware.exe
```

Expected: compilation fails because `Cst816sTouchHardware`, `Cst816sTouchApi`, and `TouchInputError` do not exist.

- [ ] **Step 3: Add the platform-neutral touch contract**

Create `src/ui/touch_input.h`:

```cpp
#pragma once

#include "touch_sample.h"

enum class TouchInputError
{
    None,
    Unavailable,
    ReadFailed
};

struct TouchInputReadResult
{
    TouchInputError error =
        TouchInputError::Unavailable;

    TouchSample sample{
        false,
        0,
        0
    };
};

class TouchInput
{
public:
    virtual ~TouchInput() = default;

    virtual TouchInputReadResult read() = 0;
};
```

- [ ] **Step 4: Add GPIO5 and replace the adapter declaration**

Add this beside the other touch constants in `src/platform/esp32/board_pins.h`:

```cpp
constexpr std::int8_t TouchInterrupt = 5;
```

Replace `src/platform/esp32/touch_hardware.h` with:

```cpp
#pragma once

#include <cstddef>
#include <cstdint>

#include "ui/touch_input.h"

struct Cst816sTouchApi
{
    using ConfigureResetPinFunction =
        void (*)(std::int8_t pin);

    using WriteResetPinFunction =
        void (*)(std::int8_t pin, bool high);

    using DelayFunction =
        void (*)(std::uint32_t milliseconds);

    using BeginBusFunction =
        bool (*)(
            std::int8_t sda,
            std::int8_t scl,
            std::uint32_t clock
        );

    using ProbeFunction =
        bool (*)(std::uint8_t address);

    using WriteRegisterFunction =
        bool (*)(
            std::uint8_t address,
            std::uint8_t registerAddress,
            std::uint8_t value
        );

    using ReadRegistersFunction =
        bool (*)(
            std::uint8_t address,
            std::uint8_t firstRegister,
            std::uint8_t* bytes,
            std::size_t count
        );

    ConfigureResetPinFunction configureResetPin = nullptr;
    WriteResetPinFunction writeResetPin = nullptr;
    DelayFunction delayMilliseconds = nullptr;
    BeginBusFunction beginBus = nullptr;
    ProbeFunction probe = nullptr;
    WriteRegisterFunction writeRegister = nullptr;
    ReadRegistersFunction readRegisters = nullptr;
};

class Cst816sTouchHardware final : public TouchInput
{
public:
    Cst816sTouchHardware();

    explicit Cst816sTouchHardware(
        Cst816sTouchApi api
    );

    TouchInputError begin();

    TouchInputReadResult read() override;

private:
    Cst816sTouchApi api_;
    bool initialized_ = false;
};
```

- [ ] **Step 5: Replace the adapter implementation**

Replace `src/platform/esp32/touch_hardware.cpp` with:

```cpp
#include "touch_hardware.h"

#include <array>

#include "board_pins.h"
#include "cst816s_protocol.h"

#ifdef ARDUINO
#include <Arduino.h>
#include <Wire.h>
#endif

namespace
{
constexpr std::uint8_t TouchAddress = 0x15;
constexpr std::uint8_t TouchDataRegister = 0x02;
constexpr std::size_t TouchRegisterCount = 5;
constexpr std::uint32_t I2cClock = 400000;

#ifdef ARDUINO
void configureResetPin(std::int8_t pin)
{
    pinMode(pin, OUTPUT);
}

void writeResetPin(
    std::int8_t pin,
    bool high
)
{
    digitalWrite(pin, high ? HIGH : LOW);
}

void delayMilliseconds(std::uint32_t milliseconds)
{
    delay(milliseconds);
}

bool beginBus(
    std::int8_t sda,
    std::int8_t scl,
    std::uint32_t clock
)
{
    if (!Wire.begin(sda, scl))
    {
        return false;
    }

    Wire.setClock(clock);
    return true;
}

bool probe(std::uint8_t address)
{
    Wire.beginTransmission(address);
    return Wire.endTransmission() == 0;
}

bool writeRegister(
    std::uint8_t address,
    std::uint8_t registerAddress,
    std::uint8_t value
)
{
    Wire.beginTransmission(address);
    Wire.write(registerAddress);
    Wire.write(value);
    return Wire.endTransmission() == 0;
}

bool readRegisters(
    std::uint8_t address,
    std::uint8_t firstRegister,
    std::uint8_t* bytes,
    std::size_t count
)
{
    Wire.beginTransmission(address);
    Wire.write(firstRegister);

    if (Wire.endTransmission() != 0)
    {
        return false;
    }

    const auto received =
        Wire.requestFrom(
            address,
            static_cast<std::uint8_t>(count)
        );

    if (received != count)
    {
        while (Wire.available() > 0)
        {
            Wire.read();
        }

        return false;
    }

    for (std::size_t index = 0; index < count; ++index)
    {
        bytes[index] =
            static_cast<std::uint8_t>(Wire.read());
    }

    return true;
}
#else
void configureResetPin(std::int8_t)
{
}

void writeResetPin(std::int8_t, bool)
{
}

void delayMilliseconds(std::uint32_t)
{
}

bool beginBus(std::int8_t, std::int8_t, std::uint32_t)
{
    return false;
}

bool probe(std::uint8_t)
{
    return false;
}

bool writeRegister(std::uint8_t, std::uint8_t, std::uint8_t)
{
    return false;
}

bool readRegisters(
    std::uint8_t,
    std::uint8_t,
    std::uint8_t*,
    std::size_t
)
{
    return false;
}
#endif

Cst816sTouchApi defaultTouchApi()
{
    return Cst816sTouchApi{
        configureResetPin,
        writeResetPin,
        delayMilliseconds,
        beginBus,
        probe,
        writeRegister,
        readRegisters
    };
}
}

Cst816sTouchHardware::Cst816sTouchHardware()
    : Cst816sTouchHardware(defaultTouchApi())
{
}

Cst816sTouchHardware::Cst816sTouchHardware(
    Cst816sTouchApi api
)
    : api_(api)
{
}

TouchInputError Cst816sTouchHardware::begin()
{
    initialized_ = false;

    if (
        api_.configureResetPin == nullptr ||
        api_.writeResetPin == nullptr ||
        api_.delayMilliseconds == nullptr ||
        api_.beginBus == nullptr ||
        api_.probe == nullptr ||
        api_.writeRegister == nullptr ||
        api_.readRegisters == nullptr
    )
    {
        return TouchInputError::Unavailable;
    }

    api_.configureResetPin(Esp32Board::TouchReset);
    api_.writeResetPin(Esp32Board::TouchReset, false);
    api_.delayMilliseconds(10);
    api_.writeResetPin(Esp32Board::TouchReset, true);
    api_.delayMilliseconds(50);

    if (
        !api_.beginBus(
            Esp32Board::TouchSda,
            Esp32Board::TouchScl,
            I2cClock
        ) ||
        !api_.probe(TouchAddress)
    )
    {
        return TouchInputError::Unavailable;
    }

    constexpr Cst816sRegisterWrite disableAutoSleep =
        cst816sDisableAutoSleepCommand();

    if (
        !api_.writeRegister(
            TouchAddress,
            disableAutoSleep.registerAddress,
            disableAutoSleep.value
        )
    )
    {
        return TouchInputError::Unavailable;
    }

    initialized_ = true;
    return TouchInputError::None;
}

TouchInputReadResult Cst816sTouchHardware::read()
{
    if (!initialized_)
    {
        return TouchInputReadResult{
            TouchInputError::Unavailable,
            TouchSample{false, 0, 0}
        };
    }

    std::array<std::uint8_t, TouchRegisterCount>
        registers{};

    if (
        !api_.readRegisters(
            TouchAddress,
            TouchDataRegister,
            registers.data(),
            registers.size()
        )
    )
    {
        return TouchInputReadResult{
            TouchInputError::ReadFailed,
            TouchSample{false, 0, 0}
        };
    }

    return TouchInputReadResult{
        TouchInputError::None,
        decodeCst816sSample(registers)
    };
}
```

- [ ] **Step 6: Compile again, then run GREEN**

```powershell
g++ -std=c++17 -Wall -Wextra -pedantic -Isrc tests\test_cst816s_touch_hardware.cpp src\platform\esp32\touch_hardware.cpp src\touch_sample.cpp -o build\test_cst816s_touch_hardware.exe
if ($LASTEXITCODE -ne 0) { throw 'CST816S touch hardware compile failed.' }
.\build\test_cst816s_touch_hardware.exe
if ($LASTEXITCODE -ne 0) { throw 'CST816S touch hardware tests failed.' }
```

Expected: `All CST816S touch hardware tests passed.`

- [ ] **Step 7: Check and commit**

```powershell
git diff --check
git add -- src/ui/touch_input.h src/platform/esp32/board_pins.h src/platform/esp32/touch_hardware.h src/platform/esp32/touch_hardware.cpp tests/test_cst816s_touch_hardware.cpp
git diff --cached --check
git commit -m "feat: add reliable CST816S touch input"
```

---

### Task 4: Support distinct production LittleFS paths

**Files:**
- Modify: `src/platform/esp32/littlefs_byte_storage.h`
- Modify: `src/platform/esp32/littlefs_byte_storage.cpp`
- Test: `tests/test_littlefs_byte_storage.cpp`

**Interfaces:**
- Preserves: `LittleFsByteStorage()` with `/agenda.bin` as its default path.
- Produces: `explicit LittleFsByteStorage(std::string path)` using the existing default Arduino LittleFS API.
- Preserves: injected `LittleFsByteStorage(std::string path, LittleFsApi api)` for host tests.

- [ ] **Step 1: Add two focused storage-path tests**

Add this include to `tests/test_littlefs_byte_storage.cpp`:

```cpp
#include <type_traits>
```

Add this helper immediately before the existing `makeStorage()` helper:

```cpp
LittleFsApi makeFakeApi()
{
    return LittleFsApi{
        fakeMount,
        fakeExists,
        fakeReadFile,
        fakeWriteFile,
        fakeRenameFile,
        fakeRemoveFile
    };
}
```

Replace the body of `makeStorage()` with:

```cpp
return LittleFsByteStorage{
    "/agenda.bin",
    makeFakeApi()
};
```

Add these two tests before the namespace closes:

```cpp
void test_production_constructor_accepts_an_explicit_path()
{
    static_assert(
        std::is_constructible_v<
            LittleFsByteStorage,
            std::string
        >
    );

    LittleFsByteStorage primary{
        std::string{"/agenda.bin"}
    };

    LittleFsByteStorage backup{
        std::string{"/agenda.backup.bin"}
    };

    (void)primary;
    (void)backup;
}

void test_primary_and_backup_paths_store_independent_bytes()
{
    resetFakeFileSystem();

    LittleFsByteStorage primary{
        "/agenda.bin",
        makeFakeApi()
    };

    LittleFsByteStorage backup{
        "/agenda.backup.bin",
        makeFakeApi()
    };

    assert(primary.begin());
    assert(backup.begin());

    assert(
        primary.write({1, 2, 3}) ==
        ByteStorageError::None
    );

    assert(
        backup.write({7, 8}) ==
        ByteStorageError::None
    );

    assert((files.at("/agenda.bin") ==
            std::vector<std::uint8_t>{1, 2, 3}));

    assert((files.at("/agenda.backup.bin") ==
            std::vector<std::uint8_t>{7, 8}));
}
```

Call them at the beginning of `main()`:

```cpp
test_production_constructor_accepts_an_explicit_path();
test_primary_and_backup_paths_store_independent_bytes();
```

- [ ] **Step 2: Compile and confirm RED**

```powershell
g++ -std=c++17 -Wall -Wextra -pedantic -Isrc tests\test_littlefs_byte_storage.cpp src\platform\esp32\littlefs_byte_storage.cpp -o build\test_littlefs_byte_storage.exe
```

Expected: compilation fails because the one-argument `std::string` constructor does not exist.

- [ ] **Step 3: Add the production constructor**

Add this declaration after the default constructor in `src/platform/esp32/littlefs_byte_storage.h`:

```cpp
explicit LittleFsByteStorage(
    std::string path
);
```

Replace the default-constructor block in `src/platform/esp32/littlefs_byte_storage.cpp` with:

```cpp
LittleFsByteStorage::LittleFsByteStorage()
    : LittleFsByteStorage(
          std::string{"/agenda.bin"}
      )
{
}

LittleFsByteStorage::LittleFsByteStorage(
    std::string path
)
    : LittleFsByteStorage(
          std::move(path),
          defaultLittleFsApi()
      )
{
}
```

Leave the existing two-argument injected constructor immediately after this block.

- [ ] **Step 4: Compile again, then run GREEN**

```powershell
g++ -std=c++17 -Wall -Wextra -pedantic -Isrc tests\test_littlefs_byte_storage.cpp src\platform\esp32\littlefs_byte_storage.cpp -o build\test_littlefs_byte_storage.exe
if ($LASTEXITCODE -ne 0) { throw 'LittleFS byte storage compile failed.' }
.\build\test_littlefs_byte_storage.exe
if ($LASTEXITCODE -ne 0) { throw 'LittleFS byte storage tests failed.' }
```

Expected: `All LittleFS byte storage tests passed.`

- [ ] **Step 5: Prove formatting is still disabled and commit**

```powershell
rg -n "LittleFS\.begin\(true\)" src platformio.ini
```

Expected: no matches.

```powershell
git diff --check
git add -- src/platform/esp32/littlefs_byte_storage.h src/platform/esp32/littlefs_byte_storage.cpp tests/test_littlefs_byte_storage.cpp
git diff --cached --check
git commit -m "feat: configure LittleFS storage paths"
```

---

### Task 5: Coordinate touch actions and minute ticks in `AgendaRuntime`

**Files:**
- Create: `src/ui/agenda_runtime.h`
- Create: `src/ui/agenda_runtime.cpp`
- Test: `tests/test_agenda_runtime.cpp`

**Interfaces:**
- Consumes: `TouchInput`, `TouchReleaseDetector`, `AgendaTouchMapper`, `AgendaUiStateMachine`, `AgendaRenderer`, and `DrawSurface` by reference.
- Produces: `void AgendaRuntime::begin(std::uint32_t nowMilliseconds)`.
- Produces: `AgendaRuntimeUpdateResult AgendaRuntime::update(std::uint32_t nowMilliseconds)`.
- Produces: one consolidated redraw per `update()` call, even if an action and minute tick become due together.

- [ ] **Step 1: Write two failing runtime tests**

Create `tests/test_agenda_runtime.cpp`:

```cpp
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string_view>
#include <utility>
#include <vector>

#include "core/agenda_controller.h"
#include "core/agenda_repository.h"
#include "core/agenda_ui_state_machine.h"
#include "ui/agenda_renderer.h"
#include "ui/agenda_runtime.h"
#include "ui/agenda_touch_mapper.h"
#include "ui/touch_release_detector.h"

namespace
{
class RuntimeStorage final : public ByteStorage
{
public:
    ByteReadResult read() const override
    {
        return ByteReadResult{
            ByteStorageError::NotFound,
            {}
        };
    }

    ByteStorageError write(
        const std::vector<std::uint8_t>&
    ) const override
    {
        return ByteStorageError::None;
    }
};

class RuntimeClock final : public Clock
{
public:
    mutable std::size_t readCount = 0;

    DateTime value{
        Date{2026, 9, 15},
        TimeOfDay{10, 0}
    };

    ClockReadResult read() const override
    {
        ++readCount;
        return ClockReadResult{
            ClockError::None,
            value
        };
    }

    ClockError set(const DateTime& newValue) override
    {
        value = newValue;
        return ClockError::None;
    }
};

class ScriptedTouchInput final : public TouchInput
{
public:
    explicit ScriptedTouchInput(
        std::vector<TouchInputReadResult> results
    )
        : results_(std::move(results))
    {
    }

    TouchInputReadResult read() override
    {
        if (next_ >= results_.size())
        {
            return TouchInputReadResult{
                TouchInputError::None,
                TouchSample{false, 0, 0}
            };
        }

        return results_[next_++];
    }

private:
    std::vector<TouchInputReadResult> results_;
    std::size_t next_ = 0;
};

class CountingDrawSurface final : public DrawSurface
{
public:
    std::size_t clearCount = 0;

    void clear(UiColor) override
    {
        ++clearCount;
    }

    void fillRectangle(const UiRect&, UiColor) override
    {
    }

    void drawRectangle(
        const UiRect&,
        UiColor,
        int
    ) override
    {
    }

    void fillRoundedRectangle(
        const UiRect&,
        int,
        UiColor
    ) override
    {
    }

    void drawRoundedRectangle(
        const UiRect&,
        int,
        UiColor,
        int
    ) override
    {
    }

    void fillCircle(UiPoint, int, UiColor) override
    {
    }

    void drawCircle(
        UiPoint,
        int,
        UiColor,
        int
    ) override
    {
    }

    void drawLine(
        UiPoint,
        UiPoint,
        UiColor,
        int
    ) override
    {
    }

    void drawText(
        const UiRect&,
        std::string_view,
        UiTextStyle,
        UiColor,
        UiTextAlignment
    ) override
    {
    }
};

void test_one_release_handles_one_action_and_redraws_once()
{
    RuntimeStorage storage;
    RuntimeClock clock;
    AgendaRepository repository(storage);
    AgendaController controller(repository);
    AgendaUiStateMachine stateMachine(
        controller,
        clock,
        clock.value
    );

    stateMachine.start();

    ScriptedTouchInput touch({
        {TouchInputError::None, {true, 200, 20}},
        {TouchInputError::None, {false, 0, 0}},
        {TouchInputError::None, {false, 0, 0}}
    });

    TouchReleaseDetector releaseDetector;
    AgendaTouchMapper touchMapper;
    AgendaRenderer renderer;
    CountingDrawSurface surface;

    AgendaRuntime runtime(
        touch,
        releaseDetector,
        touchMapper,
        stateMachine,
        renderer,
        surface
    );

    runtime.begin(0);

    const auto pressed = runtime.update(10);
    const auto released = runtime.update(20);
    const auto idle = runtime.update(30);

    assert(!pressed.actionHandled);
    assert(!pressed.rendered);
    assert(released.actionHandled);
    assert(released.rendered);
    assert(!idle.actionHandled);
    assert(!idle.rendered);
    assert(surface.clearCount == 1);
    assert(stateMachine.view().displayedMonth.year == 2026);
    assert(stateMachine.view().displayedMonth.month == 10);
}

void test_minute_tick_uses_wrap_safe_elapsed_time()
{
    RuntimeStorage storage;
    RuntimeClock clock;
    AgendaRepository repository(storage);
    AgendaController controller(repository);
    AgendaUiStateMachine stateMachine(
        controller,
        clock,
        clock.value
    );

    stateMachine.start();
    assert(clock.readCount == 1);

    ScriptedTouchInput touch(
        std::vector<TouchInputReadResult>{}
    );
    TouchReleaseDetector releaseDetector;
    AgendaTouchMapper touchMapper;
    AgendaRenderer renderer;
    CountingDrawSurface surface;

    AgendaRuntime runtime(
        touch,
        releaseDetector,
        touchMapper,
        stateMachine,
        renderer,
        surface
    );

    constexpr std::uint32_t start =
        UINT32_MAX - 20U;

    runtime.begin(start);

    const auto before =
        runtime.update(start + 59999U);

    assert(!before.ticked);
    assert(surface.clearCount == 0);
    assert(clock.readCount == 1);

    const auto due =
        runtime.update(start + 60000U);

    assert(due.ticked);
    assert(due.rendered);
    assert(surface.clearCount == 1);
    assert(clock.readCount == 2);
}
}

int main()
{
    test_one_release_handles_one_action_and_redraws_once();
    test_minute_tick_uses_wrap_safe_elapsed_time();

    std::cout
        << "All agenda runtime tests passed.\n";

    return 0;
}
```

- [ ] **Step 2: Compile and confirm RED**

```powershell
$runtimeCore = @(
    'src\core\date_time.cpp',
    'src\core\utf8.cpp',
    'src\core\event.cpp',
    'src\core\agenda_state.cpp',
    'src\core\agenda_manager.cpp',
    'src\core\crc32.cpp',
    'src\core\event_codec.cpp',
    'src\core\agenda_repository.cpp',
    'src\core\reminder_engine.cpp',
    'src\core\agenda_controller.cpp',
    'src\core\calendar_engine.cpp',
    'src\core\t9_engine.cpp',
    'src\core\agenda_ui_state_machine.cpp'
)

$runtimeUi = @(
    'src\ui\agenda_renderer.cpp',
    'src\ui\agenda_touch_mapper.cpp',
    'src\ui\touch_release_detector.cpp'
)

g++ -std=c++17 -Wall -Wextra -pedantic -Isrc tests\test_agenda_runtime.cpp @runtimeCore @runtimeUi -o build\test_agenda_runtime.exe
```

Expected: compilation fails with `ui/agenda_runtime.h: No such file or directory`.

- [ ] **Step 3: Add the runtime interface**

Create `src/ui/agenda_runtime.h`:

```cpp
#pragma once

#include <cstdint>

#include "agenda_renderer.h"
#include "agenda_touch_mapper.h"
#include "core/agenda_ui_state_machine.h"
#include "touch_input.h"
#include "touch_release_detector.h"

struct AgendaRuntimeUpdateResult
{
    bool touchPolled = false;
    TouchInputError touchError =
        TouchInputError::None;
    bool actionHandled = false;
    bool ticked = false;
    bool rendered = false;
};

class AgendaRuntime
{
public:
    AgendaRuntime(
        TouchInput& touchInput,
        TouchReleaseDetector& releaseDetector,
        AgendaTouchMapper& touchMapper,
        AgendaUiStateMachine& stateMachine,
        AgendaRenderer& renderer,
        DrawSurface& surface
    );

    void begin(
        std::uint32_t nowMilliseconds
    );

    AgendaRuntimeUpdateResult update(
        std::uint32_t nowMilliseconds
    );

private:
    TouchInput& touchInput_;
    TouchReleaseDetector& releaseDetector_;
    AgendaTouchMapper& touchMapper_;
    AgendaUiStateMachine& stateMachine_;
    AgendaRenderer& renderer_;
    DrawSurface& surface_;

    bool started_ = false;
    std::uint32_t lastTouchPollMilliseconds_ = 0;
    std::uint32_t lastTickMilliseconds_ = 0;
};
```

- [ ] **Step 4: Implement wrap-safe scheduling and one consolidated redraw**

Create `src/ui/agenda_runtime.cpp`:

```cpp
#include "agenda_runtime.h"

namespace
{
constexpr std::uint32_t TouchPollIntervalMilliseconds = 10;
constexpr std::uint32_t UiTickIntervalMilliseconds = 60000;

bool intervalElapsed(
    std::uint32_t now,
    std::uint32_t previous,
    std::uint32_t interval
)
{
    return
        static_cast<std::uint32_t>(now - previous) >=
        interval;
}
}

AgendaRuntime::AgendaRuntime(
    TouchInput& touchInput,
    TouchReleaseDetector& releaseDetector,
    AgendaTouchMapper& touchMapper,
    AgendaUiStateMachine& stateMachine,
    AgendaRenderer& renderer,
    DrawSurface& surface
)
    : touchInput_(touchInput),
      releaseDetector_(releaseDetector),
      touchMapper_(touchMapper),
      stateMachine_(stateMachine),
      renderer_(renderer),
      surface_(surface)
{
}

void AgendaRuntime::begin(
    std::uint32_t nowMilliseconds
)
{
    lastTouchPollMilliseconds_ = nowMilliseconds;
    lastTickMilliseconds_ = nowMilliseconds;
    started_ = true;
}

AgendaRuntimeUpdateResult AgendaRuntime::update(
    std::uint32_t nowMilliseconds
)
{
    AgendaRuntimeUpdateResult result;

    if (!started_)
    {
        return result;
    }

    bool renderRequired = false;

    if (
        intervalElapsed(
            nowMilliseconds,
            lastTouchPollMilliseconds_,
            TouchPollIntervalMilliseconds
        )
    )
    {
        lastTouchPollMilliseconds_ = nowMilliseconds;
        result.touchPolled = true;

        const TouchInputReadResult input =
            touchInput_.read();

        result.touchError = input.error;

        if (input.error == TouchInputError::None)
        {
            const std::optional<UiPoint> released =
                releaseDetector_.update(input.sample);

            if (released.has_value())
            {
                const std::optional<AgendaUiAction> action =
                    touchMapper_.map(
                        stateMachine_.view(),
                        *released,
                        nowMilliseconds
                    );

                if (action.has_value())
                {
                    stateMachine_.handle(*action);
                    result.actionHandled = true;
                    renderRequired = true;
                }
            }
        }
    }

    if (
        intervalElapsed(
            nowMilliseconds,
            lastTickMilliseconds_,
            UiTickIntervalMilliseconds
        )
    )
    {
        lastTickMilliseconds_ = nowMilliseconds;
        stateMachine_.tick();
        result.ticked = true;
        renderRequired = true;
    }

    if (renderRequired)
    {
        renderer_.render(
            stateMachine_.view(),
            surface_
        );

        result.rendered = true;
    }

    return result;
}
```

- [ ] **Step 5: Compile again, then run GREEN**

Append the runtime implementation to the UI sources and compile before running:

```powershell
$runtimeUi += 'src\ui\agenda_runtime.cpp'

g++ -std=c++17 -Wall -Wextra -pedantic -Isrc tests\test_agenda_runtime.cpp @runtimeCore @runtimeUi -o build\test_agenda_runtime.exe
if ($LASTEXITCODE -ne 0) { throw 'Agenda runtime compile failed.' }
.\build\test_agenda_runtime.exe
if ($LASTEXITCODE -ne 0) { throw 'Agenda runtime tests failed.' }
```

Expected: `All agenda runtime tests passed.`

- [ ] **Step 6: Check and commit**

```powershell
git diff --check
git add -- src/ui/agenda_runtime.h src/ui/agenda_runtime.cpp tests/test_agenda_runtime.cpp
git diff --cached --check
git commit -m "feat: coordinate agenda runtime updates"
```

---

### Task 6: Replace the smoke screen with the real ESP32 application

**Files:**
- Modify: `src/platform/esp32/main.cpp`
- Modify: `platformio.ini`

**Interfaces:**
- Consumes: all components produced by Tasks 1–5 plus the existing repository, controller, state machine, renderer, clock, display surface, and display hardware.
- Produces: the final Arduino `setup()` boot sequence and non-blocking `loop()`.
- Produces: Serial markers `AGENDA_START`, `DISPLAY_INIT_FAILED`, `STORAGE_INIT_FAILED`, `TOUCH_INIT_FAILED`, `TOUCH_READ_FAILED`, and `AGENDA_READY`.

- [ ] **Step 1: Replace `main.cpp` with the complete dependency composition**

Replace `src/platform/esp32/main.cpp` with:

```cpp
#include <Arduino.h>

#include <cstdint>
#include <string>
#include <string_view>

#include "arduino_gfx_surface.h"
#include "core/agenda_controller.h"
#include "core/agenda_repository.h"
#include "core/agenda_ui_state_machine.h"
#include "core/compile_date_time.h"
#include "display_hardware.h"
#include "esp32_clock.h"
#include "littlefs_byte_storage.h"
#include "touch_hardware.h"
#include "ui/agenda_renderer.h"
#include "ui/agenda_runtime.h"
#include "ui/agenda_touch_mapper.h"
#include "ui/touch_release_detector.h"

namespace
{
constexpr std::uint32_t TouchErrorReportIntervalMilliseconds =
    5000;

DisplayHardware displayHardware;

ArduinoGfxSurface drawSurface{
    displayHardware.graphics()
};

Cst816sTouchHardware touchHardware;
Esp32Clock clock;

LittleFsByteStorage primaryStorage{
    std::string{"/agenda.bin"}
};

LittleFsByteStorage backupStorage{
    std::string{"/agenda.backup.bin"}
};

AgendaRepository repository(
    primaryStorage,
    backupStorage
);

AgendaController controller(repository);

const DateTime clockSetupDefault =
    compileDateTimeOrFallback(
        __DATE__,
        __TIME__
    );

AgendaUiStateMachine stateMachine(
    controller,
    clock,
    clockSetupDefault
);

AgendaRenderer renderer;
AgendaTouchMapper touchMapper;
TouchReleaseDetector releaseDetector;

AgendaRuntime runtime(
    touchHardware,
    releaseDetector,
    touchMapper,
    stateMachine,
    renderer,
    drawSurface
);

bool runtimeReady = false;
bool touchReadFailureActive = false;
std::uint32_t lastTouchErrorReportMilliseconds = 0;

void drawFatalScreen(std::string_view message)
{
    drawSurface.clear(
        AgendaUiColors::Background
    );

    drawSurface.drawCircle(
        UiPoint{120, 120},
        112,
        AgendaUiColors::Error,
        2
    );

    drawSurface.drawText(
        UiRect{34, 64, 172, 34},
        "ΣΦΑΛΜΑ",
        UiTextStyle::Heading,
        AgendaUiColors::Error,
        UiTextAlignment::Center
    );

    drawSurface.drawText(
        UiRect{30, 108, 180, 58},
        message,
        UiTextStyle::Body,
        AgendaUiColors::PrimaryText,
        UiTextAlignment::Center
    );
}

void stopWithVisibleError(
    std::string_view message,
    const char* serialMarker
)
{
    runtimeReady = false;
    drawFatalScreen(message);
    displayHardware.enableBacklight();
    Serial.println(serialMarker);
}

void reportTouchReadStatus(
    const AgendaRuntimeUpdateResult& result,
    std::uint32_t nowMilliseconds
)
{
    if (!result.touchPolled)
    {
        return;
    }

    if (result.touchError == TouchInputError::None)
    {
        touchReadFailureActive = false;
        return;
    }

    if (
        !touchReadFailureActive ||
        static_cast<std::uint32_t>(
            nowMilliseconds -
            lastTouchErrorReportMilliseconds
        ) >= TouchErrorReportIntervalMilliseconds
    )
    {
        Serial.println("TOUCH_READ_FAILED");
        lastTouchErrorReportMilliseconds =
            nowMilliseconds;
    }

    touchReadFailureActive = true;
}
}

void setup()
{
    Serial.begin(115200);
    delay(250);
    Serial.println("AGENDA_START");

    if (!displayHardware.begin())
    {
        Serial.println("DISPLAY_INIT_FAILED");
        return;
    }

    if (
        !primaryStorage.begin() ||
        !backupStorage.begin()
    )
    {
        stopWithVisibleError(
            "ΣΦΑΛΜΑ ΑΠΟΘΗΚΕΥΣΗΣ",
            "STORAGE_INIT_FAILED"
        );

        return;
    }

    if (
        touchHardware.begin() !=
        TouchInputError::None
    )
    {
        stopWithVisibleError(
            "ΣΦΑΛΜΑ ΑΦΗΣ",
            "TOUCH_INIT_FAILED"
        );

        return;
    }

    stateMachine.start();

    renderer.render(
        stateMachine.view(),
        drawSurface
    );

    displayHardware.enableBacklight();

    runtime.begin(
        static_cast<std::uint32_t>(millis())
    );

    runtimeReady = true;
    Serial.println("AGENDA_READY");
}

void loop()
{
    if (!runtimeReady)
    {
        delay(100);
        return;
    }

    const std::uint32_t nowMilliseconds =
        static_cast<std::uint32_t>(millis());

    const AgendaRuntimeUpdateResult result =
        runtime.update(nowMilliseconds);

    reportTouchReadStatus(
        result,
        nowMilliseconds
    );

    delay(1);
}
```

- [ ] **Step 2: Expand the embedded source filter**

Replace only the `build_src_filter` block in `platformio.ini` with:

```ini
build_src_filter =
    -<*>
    +<touch_sample.cpp>
    +<core/*.cpp>
    +<ui/*.cpp>
    +<platform/esp32/*.cpp>
```

This continues to exclude `src/platform/desktop_file_storage.cpp`.

- [ ] **Step 3: Build the integrated firmware without uploading**

```powershell
$pio = "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe"
& $pio run -e waveshare_esp32_s3_touch_lcd_1_28
if ($LASTEXITCODE -ne 0) { throw 'Integrated ESP32 firmware build failed.' }
```

Expected: PlatformIO ends with `[SUCCESS]` and creates the ESP32-S3 firmware image.

- [ ] **Step 4: Verify the architecture and failure constraints**

```powershell
rg -n "LittleFS\.begin\(true\)|WiFi|NTP|Bluetooth" src platformio.ini
```

Expected: no matches.

```powershell
rg -n "TouchInterrupt" src
```

Expected: exactly one match, the GPIO5 constant in `src/platform/esp32/board_pins.h`.

```powershell
rg -n "Arduino|Wire|LittleFS|Arduino_GFX" src/core
```

Expected: no matches.

- [ ] **Step 5: Check and commit**

```powershell
git diff --check
git add -- platformio.ini src/platform/esp32/main.cpp
git diff --cached --check
git commit -m "feat: integrate agenda runtime on ESP32"
```

---

### Task 7: Run complete regression and USB-powered device acceptance

**Files:**
- Verify: all `src/` production files and all `tests/test_*.cpp` suites.
- Modify only if a failing test identifies a concrete defect; use the debugging and RED-GREEN workflow for that defect before returning here.

**Interfaces:**
- Verifies: every existing core/UI/platform contract plus all interfaces introduced in Tasks 1–6.
- Produces: objective desktop-test, clean-firmware-build, and physical-device evidence for this milestone.

- [ ] **Step 1: Compile every desktop suite with one checked PowerShell helper**

Run this entire block from the repository root:

```powershell
New-Item -ItemType Directory -Force build | Out-Null

$common = @(
    '-std=c++17',
    '-Wall',
    '-Wextra',
    '-pedantic',
    '-Isrc'
)

function Build-HostTest {
    param(
        [Parameter(Mandatory)]
        [string]$Output,

        [Parameter(Mandatory)]
        [string[]]$Sources
    )

    & g++ @common @Sources -o $Output

    if ($LASTEXITCODE -ne 0)
    {
        throw "Compilation failed: $Output"
    }
}

Build-HostTest build\test_date_time.exe @(
    'tests\test_date_time.cpp',
    'src\core\date_time.cpp'
)

Build-HostTest build\test_compile_date_time.exe @(
    'tests\test_compile_date_time.cpp',
    'src\core\compile_date_time.cpp',
    'src\core\date_time.cpp'
)

Build-HostTest build\test_calendar_engine.exe @(
    'tests\test_calendar_engine.cpp',
    'src\core\date_time.cpp',
    'src\core\calendar_engine.cpp'
)

Build-HostTest build\test_utf8.exe @(
    'tests\test_utf8.cpp',
    'src\core\utf8.cpp'
)

Build-HostTest build\test_event.exe @(
    'tests\test_event.cpp',
    'src\core\date_time.cpp',
    'src\core\utf8.cpp',
    'src\core\event.cpp'
)

Build-HostTest build\test_t9_engine.exe @(
    'tests\test_t9_engine.cpp',
    'src\core\utf8.cpp',
    'src\core\t9_engine.cpp'
)

Build-HostTest build\test_agenda_state.exe @(
    'tests\test_agenda_state.cpp',
    'src\core\date_time.cpp',
    'src\core\utf8.cpp',
    'src\core\event.cpp',
    'src\core\agenda_state.cpp'
)

Build-HostTest build\test_agenda_manager.exe @(
    'tests\test_agenda_manager.cpp',
    'src\core\date_time.cpp',
    'src\core\utf8.cpp',
    'src\core\event.cpp',
    'src\core\agenda_state.cpp',
    'src\core\agenda_manager.cpp'
)

Build-HostTest build\test_crc32.exe @(
    'tests\test_crc32.cpp',
    'src\core\crc32.cpp'
)

Build-HostTest build\test_event_codec.exe @(
    'tests\test_event_codec.cpp',
    'src\core\date_time.cpp',
    'src\core\utf8.cpp',
    'src\core\event.cpp',
    'src\core\agenda_state.cpp',
    'src\core\crc32.cpp',
    'src\core\event_codec.cpp'
)

Build-HostTest build\test_desktop_file_storage.exe @(
    'tests\test_desktop_file_storage.cpp',
    'src\platform\desktop_file_storage.cpp'
)

Build-HostTest build\test_agenda_repository.exe @(
    'tests\test_agenda_repository.cpp',
    'src\core\date_time.cpp',
    'src\core\utf8.cpp',
    'src\core\event.cpp',
    'src\core\agenda_state.cpp',
    'src\core\crc32.cpp',
    'src\core\event_codec.cpp',
    'src\core\agenda_repository.cpp'
)

Build-HostTest build\test_reminder_engine.exe @(
    'tests\test_reminder_engine.cpp',
    'src\core\date_time.cpp',
    'src\core\utf8.cpp',
    'src\core\event.cpp',
    'src\core\agenda_state.cpp',
    'src\core\agenda_manager.cpp',
    'src\core\reminder_engine.cpp'
)

Build-HostTest build\test_agenda_controller.exe @(
    'tests\test_agenda_controller.cpp',
    'src\core\date_time.cpp',
    'src\core\utf8.cpp',
    'src\core\event.cpp',
    'src\core\agenda_state.cpp',
    'src\core\agenda_manager.cpp',
    'src\core\crc32.cpp',
    'src\core\event_codec.cpp',
    'src\core\agenda_repository.cpp',
    'src\core\reminder_engine.cpp',
    'src\core\agenda_controller.cpp'
)

Build-HostTest build\test_agenda_persistence.exe @(
    'tests\test_agenda_persistence.cpp',
    'src\core\date_time.cpp',
    'src\core\utf8.cpp',
    'src\core\event.cpp',
    'src\core\agenda_state.cpp',
    'src\core\agenda_manager.cpp',
    'src\core\crc32.cpp',
    'src\core\event_codec.cpp',
    'src\core\agenda_repository.cpp',
    'src\core\reminder_engine.cpp',
    'src\core\agenda_controller.cpp',
    'src\platform\desktop_file_storage.cpp'
)

Build-HostTest build\test_agenda_ui_state_machine.exe @(
    'tests\test_agenda_ui_state_machine.cpp',
    'src\core\date_time.cpp',
    'src\core\utf8.cpp',
    'src\core\event.cpp',
    'src\core\agenda_state.cpp',
    'src\core\agenda_manager.cpp',
    'src\core\crc32.cpp',
    'src\core\event_codec.cpp',
    'src\core\agenda_repository.cpp',
    'src\core\reminder_engine.cpp',
    'src\core\agenda_controller.cpp',
    'src\core\calendar_engine.cpp',
    'src\core\t9_engine.cpp',
    'src\core\agenda_ui_state_machine.cpp'
)

Build-HostTest build\test_agenda_ui_persistence.exe @(
    'tests\test_agenda_ui_persistence.cpp',
    'src\core\date_time.cpp',
    'src\core\utf8.cpp',
    'src\core\event.cpp',
    'src\core\agenda_state.cpp',
    'src\core\agenda_manager.cpp',
    'src\core\crc32.cpp',
    'src\core\event_codec.cpp',
    'src\core\agenda_repository.cpp',
    'src\core\reminder_engine.cpp',
    'src\core\agenda_controller.cpp',
    'src\core\calendar_engine.cpp',
    'src\core\t9_engine.cpp',
    'src\core\agenda_ui_state_machine.cpp',
    'src\platform\desktop_file_storage.cpp'
)

Build-HostTest build\test_agenda_renderer.exe @(
    'tests\test_agenda_renderer.cpp',
    'src\ui\agenda_renderer.cpp',
    'src\core\utf8.cpp',
    'src\core\date_time.cpp',
    'src\core\calendar_engine.cpp'
)

Build-HostTest build\test_agenda_touch_mapper.exe @(
    'tests\test_agenda_touch_mapper.cpp',
    'src\ui\agenda_touch_mapper.cpp'
)

Build-HostTest build\test_touch_release_detector.exe @(
    'tests\test_touch_release_detector.cpp',
    'src\ui\touch_release_detector.cpp'
)

Build-HostTest build\test_draw_surface_math.exe @(
    'tests\test_draw_surface_math.cpp'
)

Build-HostTest build\test_esp32_clock.exe @(
    'tests\test_esp32_clock.cpp',
    'src\platform\esp32\esp32_clock.cpp',
    'src\core\date_time.cpp'
)

Build-HostTest build\test_littlefs_byte_storage.exe @(
    'tests\test_littlefs_byte_storage.cpp',
    'src\platform\esp32\littlefs_byte_storage.cpp'
)

Build-HostTest build\test_cst816s_protocol.exe @(
    'tests\test_cst816s_protocol.cpp'
)

Build-HostTest build\test_touch_sample.exe @(
    'tests\test_touch_sample.cpp',
    'src\touch_sample.cpp'
)

Build-HostTest build\test_cst816s_touch_hardware.exe @(
    'tests\test_cst816s_touch_hardware.cpp',
    'src\platform\esp32\touch_hardware.cpp',
    'src\touch_sample.cpp'
)

$runtimeCore = @(
    'src\core\date_time.cpp',
    'src\core\utf8.cpp',
    'src\core\event.cpp',
    'src\core\agenda_state.cpp',
    'src\core\agenda_manager.cpp',
    'src\core\crc32.cpp',
    'src\core\event_codec.cpp',
    'src\core\agenda_repository.cpp',
    'src\core\reminder_engine.cpp',
    'src\core\agenda_controller.cpp',
    'src\core\calendar_engine.cpp',
    'src\core\t9_engine.cpp',
    'src\core\agenda_ui_state_machine.cpp'
)

$runtimeUi = @(
    'src\ui\agenda_renderer.cpp',
    'src\ui\agenda_touch_mapper.cpp',
    'src\ui\touch_release_detector.cpp',
    'src\ui\agenda_runtime.cpp'
)

Build-HostTest build\test_agenda_runtime.exe @(
    'tests\test_agenda_runtime.cpp'
    $runtimeCore
    $runtimeUi
)
```

Expected: every compilation returns exit code `0` and emits no warning.

- [ ] **Step 2: Run every freshly compiled executable**

```powershell
Get-ChildItem .\build\test_*.exe |
    Sort-Object Name |
    ForEach-Object {
        Write-Host "Running $($_.Name)"
        & $_.FullName

        if ($LASTEXITCODE -ne 0)
        {
            throw "$($_.Name) failed."
        }
    }
```

Expected: all suites print their success message and the loop completes without throwing.

- [ ] **Step 3: Run a clean firmware build**

```powershell
$pio = "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe"
& $pio run -e waveshare_esp32_s3_touch_lcd_1_28 -t clean
if ($LASTEXITCODE -ne 0) { throw 'PlatformIO clean failed.' }

& $pio run -e waveshare_esp32_s3_touch_lcd_1_28
if ($LASTEXITCODE -ne 0) { throw 'Clean PlatformIO build failed.' }
```

Expected: `[SUCCESS]` after a full rebuild, not an incremental stale build.

- [ ] **Step 4: Inspect the final source and Git state**

```powershell
git diff --check
git status --short
git log --oneline -8
```

Expected: no diff errors, an empty status, and the six implementation commits from Tasks 1–6 above `3561a67`.

- [ ] **Step 5: Stop and obtain explicit approval before upload**

Show the user this exact command and wait for a new affirmative response before running it:

```powershell
$pio = "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe"
& $pio run -e waveshare_esp32_s3_touch_lcd_1_28 -t upload
```

- [ ] **Step 6: Upload through USB and inspect startup output**

After approval, keep the battery disconnected and run:

```powershell
$pio = "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe"
& $pio run -e waveshare_esp32_s3_touch_lcd_1_28 -t upload
if ($LASTEXITCODE -ne 0) { throw 'Firmware upload failed.' }

& $pio device monitor -b 115200
```

Expected normal startup markers:

```text
AGENDA_START
AGENDA_READY
```

Normal startup must not contain `DISPLAY_INIT_FAILED`, `STORAGE_INIT_FAILED`, `TOUCH_INIT_FAILED`, or repeated `TOUCH_READ_FAILED` lines.

- [ ] **Step 7: Complete the USB-powered physical acceptance checklist**

Keep the battery disconnected and verify these items in order:

1. The real `ClockSetup` or Month screen replaces `AGENDA 123 / DISPLAY TEST`.
2. The compilation date and time appear as the initial draft only when the ESP32 clock is unset.
3. Clock date, hour, minute, and confirmation controls each respond once per tap.
4. Touch targets near top, center, bottom, left, and right activate the matching visible control.
5. A held touch does not repeat an action; one release changes the UI at most once.
6. A timed event containing Greek letters and digits can be created, saved, and viewed.
7. An all-day event can be created, saved, and viewed.
8. After RESET, both events still exist.
9. After unplugging USB, waiting five seconds, and reconnecting USB, both events still exist.
10. An existing event can be edited and then deleted.
11. A due reminder appears within one minute and can be acknowledged without deleting its event.
12. The display remains stable when untouched and Serial does not flood with touch errors.

If the physical coordinates are inverted or rotated, record the observed corner mapping, stop this checklist, and open a focused debugging/TDD change that modifies only `Cst816sTouchHardware` coordinate conversion plus its regression test. Do not change `ui_geometry.h` or `agenda_touch_mapper.cpp` to compensate for hardware orientation.

## Milestone Boundary

Passing Task 7 completes the ESP32 runtime-integration milestone, not the whole Digital Diary. The remaining final milestone is power management and battery integration: verify the MX1.25 connector polarity and charger compatibility physically, then add battery connection, voltage monitoring, and sleep/wake behavior under a separate approved design. Until that work is complete, do not connect the battery and do not describe the overall device as finished.
