#include <cassert>
#include <iostream>
#include <string>
#include "core/agenda_ui_state_machine.h"
#include "ui/agenda_power_coordinator.h"
#include "ui/agenda_touch_mapper.h"
class Memory final:public ByteStorage {
public:
 mutable std::vector<std::uint8_t> bytes;
 bool fail=false;
 ByteReadResult read()const override{return {bytes.empty()?ByteStorageError::NotFound:ByteStorageError::None,bytes};}
 ByteStorageError write(const std::vector<std::uint8_t>&v)const override{if(fail)return ByteStorageError::WriteFailed;bytes=v;return ByteStorageError::None;}
};
class TestClock final:public Clock,public WakeDelayClock {
public:
 DateTime now{{2026,10,2},{8,0}};
 bool readFailed=false,delayFailed=false;
 ClockReadResult read()const override{return {readFailed?ClockError::ReadFailed:ClockError::None,now};}
 ClockError set(const DateTime&v)override{now=v;return ClockError::None;}
 WakeDelayResult delayUntil(const DateTime&v)const override{auto d=serialMinute(v)-serialMinute(now);return {delayFailed?ClockError::ReadFailed:ClockError::None,d<=0,d<=0?0ULL:std::uint64_t(d)*60000000ULL};}
};
void agendaFailure(){
 Memory p,b,a;p.bytes={1,2,3};AgendaRepository repo(p,b);AgendaController c(repo);TestClock clock;
 AlarmService alarms(a);assert(alarms.load());Alarm alarm;alarm.date=clock.now.date;alarm.time={8,1};assert(alarms.save(alarm,clock.now));
 AgendaUiStateMachine ui(c,clock,clock.now,&alarms);ui.start();assert(ui.view().screen==AgendaUiScreen::StartupError);
 clock.now.time.minute=1;ui.tick();assert(ui.view().overlay==AgendaUiOverlay::Alarm);
 ui.handle({AgendaUiActionType::StopAlarm});assert(ui.view().screen==AgendaUiScreen::StartupError);assert(!alarms.ringing());
}
void powerFailure(){
 Memory p,b,a;AgendaRepository repo(p,b);AgendaController c(repo);TestClock clock;assert(c.start().error==AgendaControllerError::None);
 AlarmService alarms(a);assert(alarms.load());Alarm alarm;alarm.date=clock.now.date;alarm.time={9,0};assert(alarms.save(alarm,clock.now));
 AgendaPowerManager manager;AgendaPowerCoordinator power(manager,c,clock,clock,&alarms);power.begin(0);
 AgendaUiView editor;editor.screen=AgendaUiScreen::EditorText;
 clock.readFailed=true;
 auto plan=power.update(1000000,false,editor);assert(plan.mode==AgendaSleepMode::Light);assert(plan.timerMicroseconds && *plan.timerMicroseconds<=60000000ULL);
 clock.readFailed=false;clock.delayFailed=true;
 plan=power.update(1000001,false,editor);assert(plan.mode==AgendaSleepMode::Light);assert(plan.timerMicroseconds && *plan.timerMicroseconds<=60000000ULL);
}
void repeatedHourEdit(){
 Memory a;AlarmService alarms(a);assert(alarms.load());DateTime now{{2026,10,24},{12,0}};Alarm alarm;alarm.date=now.date;alarm.time={3,30};assert(alarms.save(alarm,now));
 now={{2026,10,25},{3,30}};alarms.poll(now);assert(alarms.ringing());assert(alarms.stop(now));
 now.time={3,10};alarm=alarms.alarms()[0];assert(alarms.save(alarm,now));
 now.time={3,30};alarms.poll(now);assert(!alarms.ringing());
}
void failedStopIsVisible(){
 Memory p,b,a;AgendaRepository repo(p,b);AgendaController c(repo);TestClock clock;
 AlarmService alarms(a);assert(alarms.load());Alarm alarm;alarm.date=clock.now.date;alarm.time={8,1};assert(alarms.save(alarm,clock.now));
 AgendaUiStateMachine ui(c,clock,clock.now,&alarms);ui.start();
 AgendaUiAction date{AgendaUiActionType::SelectDate};date.date=clock.now.date;ui.handle(date);ui.handle({AgendaUiActionType::StartCreate});
 const auto editor=ui.view().screen;
 clock.now.time.minute=1;ui.tick();a.fail=true;ui.handle({AgendaUiActionType::StopAlarm});
 assert(!alarms.ringing());assert(!alarms.canSleep());assert(ui.view().overlay!=AgendaUiOverlay::None);
 AgendaTouchMapper mapper;auto retry=mapper.map(ui.view(),{120,170},0);assert(retry);
 a.fail=false;clock.readFailed=true;ui.handle(*retry);
 assert(alarms.canSleep());assert(ui.view().overlay==AgendaUiOverlay::None);assert(ui.view().screen==editor);
}
int main(int argc,char**argv){
 const std::string which=argc>1?argv[1]:"all";
 if(which=="agenda"||which=="all")agendaFailure();
 if(which=="power"||which=="all")powerFailure();
 if(which=="dst"||which=="all")repeatedHourEdit();
 if(which=="save"||which=="all")failedStopIsVisible();
 std::cout<<"Alarm recovery tests passed: "<<which<<"\n";
}
