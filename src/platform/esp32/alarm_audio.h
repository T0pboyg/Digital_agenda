#pragma once
#include <atomic>
#include "core/alarm_melody.h"
#ifdef ARDUINO
#include <driver/i2s_std.h>
#endif
class Esp32AlarmAudio {
public:
    // False reports a hardware/driver error, never suppresses the visual alarm.
    bool setRinging(bool ringing);
    void stop();
private:
    bool start();
    static void audioTask(void* self);
    bool requested_=false;
    std::atomic<bool> running_{false};
    std::atomic<bool> finished_{true};
    std::atomic<bool> failed_{false};
    AlarmMelody melody_;
#ifdef ARDUINO
    i2s_chan_handle_t channel_=nullptr;
#endif
};
