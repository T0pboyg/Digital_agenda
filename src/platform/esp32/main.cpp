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
        displayHardware.graphics()};

    Cst816sTouchHardware touchHardware;
    Esp32Clock clock;

    LittleFsByteStorage primaryStorage{
        std::string{"/agenda.bin"}};

    LittleFsByteStorage backupStorage{
        std::string{"/agenda.backup.bin"}};

    AgendaRepository repository(
        primaryStorage,
        backupStorage);

    AgendaController controller(repository);

    const DateTime clockSetupDefault =
        compileDateTimeOrFallback(
            __DATE__,
            __TIME__);

    AgendaUiStateMachine stateMachine(
        controller,
        clock,
        clockSetupDefault);

    AgendaRenderer renderer;
    AgendaTouchMapper touchMapper;
    TouchReleaseDetector releaseDetector;

    AgendaRuntime runtime(
        touchHardware,
        releaseDetector,
        touchMapper,
        stateMachine,
        renderer,
        drawSurface);

    bool runtimeReady = false;
    bool touchReadFailureActive = false;
    std::uint32_t lastTouchErrorReportMilliseconds = 0;

    void drawFatalScreen(std::string_view message)
    {
        drawSurface.clear(
            AgendaUiColors::Background);

        drawSurface.drawCircle(
            UiPoint{120, 120},
            112,
            AgendaUiColors::Error,
            2);

        drawSurface.drawText(
            UiRect{34, 64, 172, 34},
            "ΣΦΑΛΜΑ",
            UiTextStyle::Heading,
            AgendaUiColors::Error,
            UiTextAlignment::Center);

        drawSurface.drawText(
            UiRect{30, 108, 180, 58},
            message,
            UiTextStyle::Body,
            AgendaUiColors::PrimaryText,
            UiTextAlignment::Center);
    }

    void stopWithVisibleError(
        std::string_view message,
        const char *serialMarker)
    {
        runtimeReady = false;
        drawFatalScreen(message);
        displayHardware.enableBacklight();
        Serial.println(serialMarker);
    }

    void reportTouchReadStatus(
        const AgendaRuntimeUpdateResult &result,
        std::uint32_t nowMilliseconds)
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
                lastTouchErrorReportMilliseconds) >= TouchErrorReportIntervalMilliseconds)
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
        !backupStorage.begin())
    {
        stopWithVisibleError(
            "ΣΦΑΛΜΑ ΑΠΟΘΗΚΕΥΣΗΣ",
            "STORAGE_INIT_FAILED");

        return;
    }

    if (
        touchHardware.begin() !=
        TouchInputError::None)
    {
        stopWithVisibleError(
            "ΣΦΑΛΜΑ ΑΦΗΣ",
            "TOUCH_INIT_FAILED");

        return;
    }

    stateMachine.start();

    renderer.render(
        stateMachine.view(),
        drawSurface);

    displayHardware.enableBacklight();

    runtime.begin(
        static_cast<std::uint32_t>(millis()));

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
        nowMilliseconds);

    delay(1);
}