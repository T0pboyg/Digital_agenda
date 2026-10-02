#include <Arduino.h>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "arduino_gfx_surface.h"
#include "alarm_audio.h"
#include "battery_hardware.h"
#include "core/alarm_service.h"
#include "core/agenda_controller.h"
#include "core/agenda_repository.h"
#include "core/agenda_ui_state_machine.h"
#include "core/compile_date_time.h"
#include "display_hardware.h"
#include "esp32_clock.h"
#include "littlefs_byte_storage.h"
#include "power_hardware.h"
#include "touch_hardware.h"
#include "ui/agenda_power_coordinator.h"
#include "ui/agenda_power_manager.h"
#include "ui/agenda_renderer.h"
#include "ui/agenda_runtime.h"
#include "ui/agenda_touch_mapper.h"
#include "ui/touch_release_detector.h"

namespace
{
    constexpr std::uint32_t TouchErrorReportIntervalMilliseconds = 5000;
    constexpr bool DeepSleepHardwareVerified = true;
    constexpr std::uint64_t DeepSleepProbeSafetyTimerMicroseconds = 0;

    DisplayHardware displayHardware;
    ArduinoGfxSurface drawSurface{displayHardware.graphics()};
    Cst816sTouchHardware touchHardware;
    Esp32PowerHardware powerHardware;
    Esp32Clock clock;
    Esp32BatteryHardware batteryHardware;
    Esp32AlarmAudio alarmAudio;
    LittleFsByteStorage alarmStorage{std::string{"/alarms.bin"}};
    AlarmService alarms(alarmStorage);
    std::uint32_t lastBatterySample = 0;
    bool audioFailureReported = false;

    LittleFsByteStorage primaryStorage{std::string{"/agenda.bin"}};
    LittleFsByteStorage backupStorage{std::string{"/agenda.backup.bin"}};
    AgendaRepository repository(primaryStorage, backupStorage);
    AgendaController controller(repository);

    const DateTime clockSetupDefault =
        compileDateTimeOrFallback(__DATE__, __TIME__);

    AgendaUiStateMachine stateMachine(
        controller,
        clock,
        clockSetupDefault,
        &alarms);

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

    AgendaPowerManager powerManager;
    AgendaPowerCoordinator powerCoordinator(
        powerManager,
        controller,
        clock,
        clock,
        &alarms);

    bool runtimeReady = false;
    bool touchReadFailureActive = false;
    std::uint32_t lastTouchErrorReportMilliseconds = 0;

    void renderCurrentView()
    {
        renderer.render(stateMachine.view(), drawSurface);
    }

    void showCurrentView()
    {
        renderCurrentView();
        displayHardware.enableBacklight();
    }

    void drawFatalScreen(std::string_view message)
    {
        drawSurface.clear(AgendaUiColors::Background);
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
        const char* serialMarker)
    {
        runtimeReady = false;
        drawFatalScreen(message);
        displayHardware.enableBacklight();
        Serial.println(serialMarker);
    }

    void reportTouchReadStatus(
        const AgendaRuntimeUpdateResult& result,
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
                lastTouchErrorReportMilliseconds) >=
                TouchErrorReportIntervalMilliseconds)
        {
            Serial.println("TOUCH_READ_FAILED");
            lastTouchErrorReportMilliseconds = nowMilliseconds;
        }

        touchReadFailureActive = true;
    }

    void reportCoordinatorError(
        AgendaPowerCoordinatorError error)
    {
        switch (error)
        {
        case AgendaPowerCoordinatorError::None:
            return;
        case AgendaPowerCoordinatorError::ClockReadFailed:
            Serial.println("POWER_CLOCK_READ_FAILED");
            return;
        case AgendaPowerCoordinatorError::ReminderScheduleFailed:
            Serial.println("REMINDER_SCHEDULE_FAILED");
            return;
        case AgendaPowerCoordinatorError::AlarmScheduleFailed:
            Serial.println("ALARM_STORAGE_ERROR");
            return;
        case AgendaPowerCoordinatorError::WakeDelayFailed:
            Serial.println("WAKE_DELAY_FAILED");
            return;
        }
    }

    void reportPowerHardwareError(PowerHardwareError error)
    {
        switch (error)
        {
        case PowerHardwareError::None:
            return;
        case PowerHardwareError::Unavailable:
            Serial.println("POWER_HARDWARE_UNAVAILABLE");
            return;
        case PowerHardwareError::TouchActive:
            Serial.println("TOUCH_ACTIVE_SLEEP_DEFERRED");
            return;
        case PowerHardwareError::TouchWakeConfigurationFailed:
            Serial.println("TOUCH_WAKE_CONFIG_FAILED");
            return;
        case PowerHardwareError::TimerWakeConfigurationFailed:
            Serial.println("TIMER_WAKE_CONFIG_FAILED");
            return;
        case PowerHardwareError::LightSleepFailed:
            Serial.println("LIGHT_SLEEP_FAILED");
            return;
        case PowerHardwareError::DeepSleepReturned:
            Serial.println("DEEP_SLEEP_FAILED");
            return;
        }
    }

    void recoverFromPowerFailure(
        PowerHardwareError error,
        std::uint32_t nowMilliseconds)
    {
        reportPowerHardwareError(error);

        if (error == PowerHardwareError::TouchActive)
        {
            runtime.suppressActionsUntilRelease();
        }

        powerCoordinator.recordActivity(nowMilliseconds);
        showCurrentView();
    }

    std::optional<std::uint64_t> timerForSleep(
        AgendaSleepMode mode,
        const std::optional<std::uint64_t>& requested)
    {
        if (
            mode != AgendaSleepMode::Deep ||
            DeepSleepProbeSafetyTimerMicroseconds == 0)
        {
            return requested;
        }

        if (
            !requested.has_value() ||
            DeepSleepProbeSafetyTimerMicroseconds < *requested)
        {
            return DeepSleepProbeSafetyTimerMicroseconds;
        }

        return requested;
    }

    void handleLightSleepWake()
    {
        const PowerWakeCause cause = powerHardware.wakeCause();
        powerHardware.releaseWakePin();

        const std::uint32_t nowMilliseconds =
            static_cast<std::uint32_t>(millis());

        if (cause == PowerWakeCause::Touch)
        {
            Serial.println("WAKE_TOUCH");
            runtime.suppressActionsUntilRelease();
            powerCoordinator.recordActivity(nowMilliseconds);
            showCurrentView();
            return;
        }

        if (cause == PowerWakeCause::Timer)
        {
            Serial.println("WAKE_TIMER");
            stateMachine.tick();

            if (
                (stateMachine.view().overlay == AgendaUiOverlay::Reminder ||
                 stateMachine.view().overlay == AgendaUiOverlay::Alarm))
            {
                powerCoordinator.recordActivity(nowMilliseconds);
                showCurrentView();
            }

            return;
        }

        Serial.println("WAKE_UNKNOWN");
        powerCoordinator.recordActivity(nowMilliseconds);
        showCurrentView();
    }

    void executePowerPlan(
        const AgendaPowerPlan& plan,
        std::uint32_t nowMilliseconds)
    {
        AgendaSleepMode mode = plan.mode;

        if (
            mode == AgendaSleepMode::Deep &&
            !DeepSleepHardwareVerified)
        {
            mode = AgendaSleepMode::Light;
        }

        const std::optional<std::uint64_t> timer =
            timerForSleep(mode, plan.timerMicroseconds);

        alarmAudio.stop();
        displayHardware.disableBacklight();

        const PowerHardwareError prepared =
            powerHardware.prepareWake(timer);

        if (prepared != PowerHardwareError::None)
        {
            recoverFromPowerFailure(prepared, nowMilliseconds);
            return;
        }

        if (mode == AgendaSleepMode::Deep)
        {
            Serial.println("DEEP_SLEEP");
            Serial.flush();

            const PowerHardwareError slept =
                powerHardware.enterDeepSleep();

            recoverFromPowerFailure(
                slept,
                static_cast<std::uint32_t>(millis()));
            return;
        }

        Serial.println("LIGHT_SLEEP");
        Serial.flush();

        const PowerHardwareError slept =
            powerHardware.enterLightSleep();

        if (slept != PowerHardwareError::None)
        {
            recoverFromPowerFailure(
                slept,
                static_cast<std::uint32_t>(millis()));
            return;
        }

        handleLightSleepWake();
    }

    void processPowerPlan(
        const AgendaPowerPlan& plan,
        std::uint32_t nowMilliseconds)
    {
        reportCoordinatorError(plan.error);

        if (plan.reminderDueNow)
        {
            stateMachine.tick();
            powerCoordinator.recordActivity(nowMilliseconds);
            showCurrentView();
            return;
        }

        if (plan.mode == AgendaSleepMode::None)
        {
            if (plan.error != AgendaPowerCoordinatorError::None)
            {
                powerCoordinator.recordActivity(nowMilliseconds);
                showCurrentView();
            }

            return;
        }

        executePowerPlan(plan, nowMilliseconds);
    }

    void handleStartupWake(
        PowerWakeCause wakeCause,
        std::uint32_t nowMilliseconds)
    {
        if (wakeCause == PowerWakeCause::Touch)
        {
            Serial.println("WAKE_TOUCH");
            runtime.suppressActionsUntilRelease();
            powerCoordinator.recordActivity(nowMilliseconds);
            showCurrentView();
            return;
        }

        if (wakeCause == PowerWakeCause::Timer)
        {
            Serial.println("WAKE_TIMER");
            stateMachine.tick();

            if (
                (stateMachine.view().overlay == AgendaUiOverlay::Reminder ||
                 stateMachine.view().overlay == AgendaUiOverlay::Alarm))
            {
                powerCoordinator.recordActivity(nowMilliseconds);
                showCurrentView();
                return;
            }

            const AgendaPowerPlan plan =
                powerCoordinator.planImmediateDeepSleep(
                    stateMachine.view());

            if (
                DeepSleepHardwareVerified &&
                plan.mode == AgendaSleepMode::Deep)
            {
                processPowerPlan(plan, nowMilliseconds);
                return;
            }

            reportCoordinatorError(plan.error);
            powerCoordinator.recordActivity(nowMilliseconds);
            showCurrentView();
            return;
        }

        if (wakeCause == PowerWakeCause::Unknown)
        {
            Serial.println("WAKE_UNKNOWN");
        }

        showCurrentView();
    }
}

void setup()
{
    Serial.begin(115200);
    delay(250);
    Serial.println("AGENDA_START");

    const PowerWakeCause startupWakeCause =
        powerHardware.wakeCause();

    powerHardware.releaseWakePin();

    if (!displayHardware.begin())
    {
        Serial.println("DISPLAY_INIT_FAILED");
        return;
    }

    if (!primaryStorage.begin() || !backupStorage.begin())
    {
        stopWithVisibleError(
            "ΣΦΑΛΜΑ ΑΠΟΘΗΚΕΥΣΗΣ",
            "STORAGE_INIT_FAILED");
        return;
    }

    if (touchHardware.begin() != TouchInputError::None)
    {
        stopWithVisibleError(
            "ΣΦΑΛΜΑ ΑΦΗΣ",
            "TOUCH_INIT_FAILED");
        return;
    }

    if (!alarmStorage.begin() || !alarms.load()) Serial.println("ALARM_STORAGE_ERROR");
    batteryHardware.begin();
    stateMachine.setBatteryPercent(batteryHardware.sample());
    stateMachine.start();

    const std::uint32_t nowMilliseconds =
        static_cast<std::uint32_t>(millis());

    runtime.begin(nowMilliseconds);
    powerCoordinator.begin(nowMilliseconds);
    runtimeReady = true;

    handleStartupWake(startupWakeCause, nowMilliseconds);
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

    reportTouchReadStatus(result, nowMilliseconds);

    if (static_cast<std::uint32_t>(nowMilliseconds - lastBatterySample) >= 30000)
    {
        lastBatterySample = nowMilliseconds;
        const auto before = stateMachine.view().batteryPercent;
        stateMachine.setBatteryPercent(batteryHardware.sample());
        if (before != stateMachine.view().batteryPercent) renderCurrentView();
    }
    const bool audioOk = alarmAudio.setRinging(alarms.ringing());
    if (!audioOk && !audioFailureReported) Serial.println("ALARM_AUDIO_FAILED");
    audioFailureReported = !audioOk;
    if (alarms.ringing()) displayHardware.enableBacklight();

    const AgendaPowerPlan plan =
        powerCoordinator.update(
            nowMilliseconds,
            result.touchActivity,
            stateMachine.view());

    processPowerPlan(plan, nowMilliseconds);
    delay(1);
}
