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
    bool touchActivity = false;
    bool actionHandled = false;
    bool ticked = false;
    bool rendered = false;
};

class AgendaRuntime
{
public:
    AgendaRuntime(
        TouchInput &touchInput,
        TouchReleaseDetector &releaseDetector,
        AgendaTouchMapper &touchMapper,
        AgendaUiStateMachine &stateMachine,
        AgendaRenderer &renderer,
        DrawSurface &surface);

    void begin(
        std::uint32_t nowMilliseconds);

    void suppressActionsUntilRelease();

    AgendaRuntimeUpdateResult update(
        std::uint32_t nowMilliseconds);

private:
    TouchInput &touchInput_;
    TouchReleaseDetector &releaseDetector_;
    AgendaTouchMapper &touchMapper_;
    AgendaUiStateMachine &stateMachine_;
    AgendaRenderer &renderer_;
    DrawSurface &surface_;

    bool started_ = false;
    bool suppressUntilRelease_ = false;
    std::uint32_t lastTouchPollMilliseconds_ = 0;
    std::uint32_t lastTickMilliseconds_ = 0;
};
