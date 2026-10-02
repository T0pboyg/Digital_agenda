#pragma once
#include <cstdint>
#include <optional>
#include <vector>
#include "byte_storage.h"
#include "date_time.h"

struct Alarm {
    std::uint32_t id = 0;
    Date date{2000,1,1};
    TimeOfDay time{7,0};
    bool daily = true;
    bool enabled = true;
    int lastFiredDay = -1;
    bool pending = false;
};
enum class AlarmError { None, NotReady, ReadFailed, InvalidData, WriteFailed, InvalidAlarm, Full, NotFound };

// Owns persisted alarm occurrences as well as settings, independent of the agenda file.
class AlarmService {
public:
    static constexpr std::size_t MaxAlarms = 24;
    explicit AlarmService(ByteStorage& storage) : storage_(storage) {}
    bool load();
    bool save(Alarm alarm, const DateTime& now);
    bool remove(std::uint32_t id);
    void poll(const DateTime& now);
    bool stop(const DateTime& now);
    std::optional<DateTime> nextWake(const DateTime& now) const;
    const std::vector<Alarm>& alarms() const { return alarms_; }
    bool ringing() const;
    bool ready() const { return ready_; }
    bool canSleep() const { return ready_ && !dirty_ && !ringing(); }
    AlarmError error() const { return error_; }
private:
    bool write(const std::vector<Alarm>& alarms);
    ByteStorage& storage_;
    std::vector<Alarm> alarms_;
    bool ready_ = false;
    bool dirty_ = false;
    AlarmError error_ = AlarmError::NotReady;
};
