#include "alarm_service.h"
#include "crc32.h"
#include <algorithm>
#include <limits>
#include <string_view>

namespace {
constexpr std::uint32_t Magic = 0x314D4C41; // ALM1, little endian
void put(std::vector<std::uint8_t>& b, std::uint32_t n) {
    for(int i=0;i<4;++i) b.push_back(static_cast<std::uint8_t>(n>>(8*i)));
}
std::uint32_t get(const std::vector<std::uint8_t>& b, std::size_t offset) {
    std::uint32_t n=0; for(int i=0;i<4;++i) n|=std::uint32_t(b[offset+i])<<(8*i); return n;
}
std::uint32_t checksum(const std::vector<std::uint8_t>& b, std::size_t length) {
    return crc32(std::string_view(reinterpret_cast<const char*>(b.data()),length));
}
bool valid(const DateTime& n) { return isValidDate(n.date)&&isValidTime(n.time); }
int minute(const TimeOfDay& t) { return t.hour*60+t.minute; }
bool due(const Alarm& a,const DateTime& n) {
    if(!a.enabled || a.pending) return false;
    if(a.daily) return a.lastFiredDay < serialDay(n.date) && minute(a.time)<=minute(n.time);
    return a.lastFiredDay<0 && serialMinute({a.date,a.time})<=serialMinute(n);
}
}

bool AlarmService::write(const std::vector<Alarm>& alarms) {
    std::vector<std::uint8_t> b; put(b,Magic); put(b,1); put(b,static_cast<std::uint32_t>(alarms.size()));
    for(const auto& a:alarms) {
        put(b,a.id); put(b,a.date.year); put(b,a.date.month); put(b,a.date.day);
        put(b,a.time.hour); put(b,a.time.minute);
        put(b,(a.daily?1u:0u)|(a.enabled?2u:0u)|(a.pending?4u:0u));
        put(b,static_cast<std::uint32_t>(a.lastFiredDay+1));
    }
    put(b,checksum(b,b.size()));
    if(storage_.write(b)!=ByteStorageError::None) { error_=AlarmError::WriteFailed; return false; }
    error_=AlarmError::None; return true;
}

bool AlarmService::load() {
    const auto r=storage_.read(); ready_=false;
    if(r.error==ByteStorageError::NotFound) { alarms_.clear(); dirty_=false; ready_=true; error_=AlarmError::None; return true; }
    if(r.error!=ByteStorageError::None) { error_=AlarmError::ReadFailed; return false; }
    const auto& b=r.bytes;
    auto invalid=[this](){error_=AlarmError::InvalidData; return false;};
    if(b.size()<16 || get(b,0)!=Magic || get(b,4)!=1) return invalid();
    const auto count=get(b,8);
    if(count>MaxAlarms || b.size()!=16+32*count || checksum(b,b.size()-4)!=get(b,b.size()-4)) return invalid();
    std::vector<Alarm> loaded;
    for(std::size_t i=0;i<count;++i) {
        const std::size_t o=12+32*i; Alarm a;
        a.id=get(b,o); const auto year=get(b,o+4), month=get(b,o+8), day=get(b,o+12);
        const auto hour=get(b,o+16), min=get(b,o+20), flags=get(b,o+24), fired=get(b,o+28);
        if(a.id==0 || year>2099 || month>12 || day>31 || hour>23 || min>59 || flags>7 || fired>static_cast<unsigned>(serialDay({2099,12,31})+1)) return invalid();
        a.date={static_cast<int>(year),static_cast<int>(month),static_cast<int>(day)};
        a.time={static_cast<int>(hour),static_cast<int>(min)};
        a.daily=flags&1; a.enabled=flags&2; a.pending=flags&4; a.lastFiredDay=static_cast<int>(fired)-1;
        if(!isValidDate(a.date) || (a.pending && (!a.enabled || a.lastFiredDay<0))) return invalid();
        if(std::any_of(loaded.begin(),loaded.end(),[&](const Alarm& x){return x.id==a.id;})) return invalid();
        loaded.push_back(a);
    }
    alarms_=std::move(loaded); ready_=true; dirty_=false; error_=AlarmError::None; return true;
}

bool AlarmService::save(Alarm a,const DateTime& now) {
    if(!ready_ || dirty_ || ringing()) {error_=AlarmError::NotReady; return false;}
    if(!valid(now) || !isValidDate(a.date) || !isValidTime(a.time) ||
        (a.enabled && !a.daily && serialMinute({a.date,a.time})<=serialMinute(now))) {
        error_=AlarmError::InvalidAlarm; return false;
    }
    auto next=alarms_;
    auto it=std::find_if(next.begin(),next.end(),[&](const Alarm& x){return x.id==a.id;});
    if(a.id && it==next.end()) { error_=AlarmError::NotFound; return false; }
    // Enabling/creating/editing a daily time which has passed schedules tomorrow.
    a.pending=false;
    a.lastFiredDay=(a.daily && minute(a.time)<=minute(now.time))?serialDay(now.date):-1;
    // An unchanged daily schedule keeps its acknowledgment through a repeated
    // DST hour (or a backwards clock correction). A changed time is a new schedule.
    if(it!=next.end() && it->daily && a.daily &&
       minute(it->time)==minute(a.time))
        a.lastFiredDay=std::max(a.lastFiredDay,it->lastFiredDay);
    if(!a.id) {
        if(next.size()>=MaxAlarms) {error_=AlarmError::Full; return false;}
        std::uint32_t id=1;
        while(std::any_of(next.begin(),next.end(),[&](const Alarm& x){return x.id==id;})) ++id;
        a.id=id; next.push_back(a);
    } else *it=a;
    if(!write(next)) return false;
    alarms_=std::move(next); return true;
}

bool AlarmService::remove(std::uint32_t id) {
    if(!ready_ || dirty_ || ringing()) {error_=AlarmError::NotReady; return false;}
    auto next=alarms_;
    auto it=std::find_if(next.begin(),next.end(),[&](const Alarm& a){return a.id==id;});
    if(it==next.end()) {error_=AlarmError::NotFound; return false;}
    next.erase(it); if(!write(next)) return false; alarms_=std::move(next); return true;
}

bool AlarmService::ringing() const {
    return ready_ && std::any_of(alarms_.begin(),alarms_.end(),[](const Alarm& a){return a.pending;});
}
void AlarmService::poll(const DateTime& now) {
    if(!ready_ || !valid(now)) return;
    bool changed=false;
    for(auto& a:alarms_) if(due(a,now)) {a.pending=true; a.lastFiredDay=serialDay(now.date); changed=true;}
    if(changed || dirty_) dirty_=!write(alarms_);
}
bool AlarmService::stop(std::optional<DateTime> now) {
    if(!ready_) return false;
    // A failed clock must never prevent stopping an already active alarm.
    // Include newly due alarms only when a trustworthy current time is available.
    if(now && valid(*now)) poll(*now);
    bool changed=false;
    for(auto& a:alarms_) if(a.pending) {a.pending=false; if(!a.daily) a.enabled=false; changed=true;}
    if(changed || dirty_) dirty_=!write(alarms_);
    return !dirty_;
}
bool AlarmService::retryPendingSave() {
    if(!ready_) return false;
    if(dirty_) dirty_=!write(alarms_);
    return !dirty_;
}
std::optional<DateTime> AlarmService::nextWake(const DateTime& now) const {
    if(!ready_ || !valid(now)) return std::nullopt;
    std::optional<DateTime> result;
    for(const auto& a:alarms_) {
        if(!a.enabled) continue;
        DateTime target{a.date,a.time};
        if(a.pending || due(a,now)) return now;
        if(a.daily) {
            target.date=now.date;
            if(a.lastFiredDay>=serialDay(now.date) || minute(a.time)<=minute(now.time)) {
                auto next=nextDate(now.date); if(!next) continue; target.date=*next;
            }
        } else if(a.lastFiredDay>=0) continue;
        if(!result || serialMinute(target)<serialMinute(*result)) result=target;
    }
    return result;
}
