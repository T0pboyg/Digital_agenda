#include <cassert>
#include <iostream>
#include "core/alarm_service.h"
class Memory final : public ByteStorage {
public:
 mutable std::vector<std::uint8_t> bytes;
 mutable int writes=0;
 bool fail=false;
 ByteReadResult read() const override { return {bytes.empty()?ByteStorageError::NotFound:ByteStorageError::None,bytes}; }
 ByteStorageError write(const std::vector<std::uint8_t>& b) const override { ++writes; if(fail) return ByteStorageError::WriteFailed; bytes=b; return ByteStorageError::None; }
};
int main() {
 Memory memory; AlarmService s(memory); assert(s.load());
 DateTime now{{2026,10,2},{8,0}};
 Alarm a; a.time={8,1}; a.date=now.date; a.daily=true;
 assert(s.save(a,now)); assert(s.alarms().size()==1);
 auto wake=s.nextWake(now); assert(wake && wake->time.minute==1);
 const int writes=memory.writes; s.poll(now); s.poll(now); assert(memory.writes==writes);
 now.time.minute=1; s.poll(now); assert(s.ringing());
 AlarmService reboot(memory); assert(reboot.load()); assert(reboot.ringing());
 assert(reboot.stop(now)); reboot.poll(now); assert(!reboot.ringing());
 AlarmService stopped(memory); assert(stopped.load()); stopped.poll(now); assert(!stopped.ringing());
 assert(stopped.nextWake(now)->date.day==3);
 now.date.day=3; now.time={9,0}; stopped.poll(now); assert(stopped.ringing()); assert(stopped.stop(now));
 // Moving clock backwards cannot duplicate today's occurrence.
 now.time={8,1}; stopped.poll(now); assert(!stopped.ringing());
 // One-time alarm and simultaneous alarms.
 a.id=0; a.daily=false; a.date=now.date; a.time={8,2}; assert(stopped.save(a,now));
 a.id=0; assert(stopped.save(a,now));
 now.time.minute=2; stopped.poll(now); assert(stopped.ringing()); assert(stopped.stop(now));
 assert(!stopped.alarms()[1].enabled && !stopped.alarms()[2].enabled);
 // Failed edits do not change live alarms.
 memory.fail=true; a=stopped.alarms()[0]; a.time={10,0};
 assert(!stopped.save(a,now)); assert(stopped.alarms()[0].time.hour==8);
 memory.fail=false;
 // Stop on failed persistence silences now, but forbids sleep until retry.
 now.date.day=4; stopped.poll(now); assert(stopped.ringing());
 memory.fail=true; assert(!stopped.stop(now)); assert(!stopped.ringing()); assert(!stopped.canSleep());
 memory.fail=false; stopped.poll(now); assert(stopped.canSleep()); assert(!stopped.ringing());
 // CRC corruption is not silently replaced with empty data.
 memory.bytes[8]^=1; AlarmService corrupt(memory); assert(!corrupt.load()); assert(!corrupt.save(a,now));
 Memory m2; AlarmService t(m2); assert(t.load());
 a=Alarm{}; a.daily=false; a.date=now.date; a.time={7,0}; assert(!t.save(a,now));
 a.daily=true; assert(t.save(a,now)); t.poll(now); assert(!t.ringing()); assert(t.nextWake(now)->date.day==5);
 // Daily alarm in skipped spring hour catches up at 04:00.
 Memory m3; AlarmService dst(m3); assert(dst.load());
 now={{2026,3,28},{12,0}}; a=Alarm{}; a.time={3,30}; assert(dst.save(a,now));
 now={{2026,3,29},{4,0}}; dst.poll(now); assert(dst.ringing()); assert(dst.stop(now));
 // Repeated autumn hour is one occurrence per date, including reboot.
 now={{2026,10,25},{3,30}}; dst.poll(now); assert(dst.ringing()); assert(dst.stop(now));
 AlarmService repeat(m3); assert(repeat.load()); repeat.poll(now); assert(!repeat.ringing());
 assert(repeat.remove(repeat.alarms()[0].id)); assert(repeat.alarms().empty());
 std::cout << "All alarm service tests passed.\n";
}
