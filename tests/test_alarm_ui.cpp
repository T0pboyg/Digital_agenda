#include <cassert>
#include <iostream>
#include "core/agenda_ui_state_machine.h"
#include "ui/agenda_touch_mapper.h"
#include "ui/agenda_power_coordinator.h"
class Mem final:public ByteStorage{public: mutable std::vector<std::uint8_t>b; ByteReadResult read()const override{return {b.empty()?ByteStorageError::NotFound:ByteStorageError::None,b};} ByteStorageError write(const std::vector<std::uint8_t>&v)const override{b=v;return ByteStorageError::None;}};
class TestClock final:public Clock,public WakeDelayClock {public: bool failRead=false; DateTime now{{2026,10,2},{8,0}}; ClockReadResult read()const override{return {failRead?ClockError::ReadFailed:ClockError::None,now};} ClockError set(const DateTime&v)override{now=v;return ClockError::None;} WakeDelayResult delayUntil(const DateTime&v)const override {const auto d=serialMinute(v)-serialMinute(now);return {ClockError::None,d<=0,d<=0?0ULL:static_cast<std::uint64_t>(d)*60000000ULL};}};
int main(){
 Mem p,b,a; AgendaRepository repo(p,b); AgendaController c(repo); TestClock clock; AlarmService alarms(a); assert(alarms.load());
 AgendaUiStateMachine ui(c,clock,clock.now,&alarms); ui.start();
 ui.handle({AgendaUiActionType::OpenAlarms}); assert(ui.view().screen==AgendaUiScreen::AlarmList);
 ui.handle({AgendaUiActionType::CreateAlarm}); assert(ui.view().screen==AgendaUiScreen::AlarmEditor);
 ui.handle({AgendaUiActionType::AlarmNextMinute}); ui.handle({AgendaUiActionType::SaveAlarm});
 assert(alarms.alarms().size()==1); assert(ui.view().screen==AgendaUiScreen::AlarmList);
 ui.handle({AgendaUiActionType::Back}); assert(ui.view().screen==AgendaUiScreen::Month);
 // An alarm preempts an unsaved event editor and restores it after stop.
 AgendaUiAction date{AgendaUiActionType::SelectDate};date.date=clock.now.date;ui.handle(date);ui.handle({AgendaUiActionType::StartCreate});
 const auto original=ui.view().screen;
 clock.now.time.minute=1;ui.tick();assert(ui.view().overlay==AgendaUiOverlay::Alarm);
 AgendaTouchMapper mapper;
 assert(!mapper.map(ui.view(),{50,80},0));
 auto stop=mapper.map(ui.view(),{120,170},0);assert(stop && stop->type==AgendaUiActionType::StopAlarm);
 ui.handle({AgendaUiActionType::Back});assert(ui.view().overlay==AgendaUiOverlay::Alarm);
 clock.failRead=true;ui.tick();assert(!ui.view().currentTime);
 ui.handle(*stop);assert(ui.view().overlay==AgendaUiOverlay::None);clock.failRead=false;assert(ui.view().screen==original);
 // Alarm deadline participates in earliest wake planning.
 AgendaPowerManager manager; AgendaPowerCoordinator power(manager,c,clock,clock,&alarms);
 Alarm one;one.daily=false;one.date=clock.now.date;one.time={8,2};assert(alarms.save(one,clock.now));
 AgendaUiView month;month.screen=AgendaUiScreen::Month;
 auto plan=power.planImmediateDeepSleep(month); assert(plan.mode==AgendaSleepMode::Deep);assert(plan.timerMicroseconds && *plan.timerMicroseconds==60000000ULL);
 clock.now.time.minute=2;alarms.poll(clock.now);plan=power.planImmediateDeepSleep(month);assert(plan.mode==AgendaSleepMode::None);
 assert(alarms.stop(clock.now));
 // Battery is carried in view and does not become a fake charging indicator.
 ui.setBatteryPercent(55);assert(ui.view().batteryPercent==55);
 std::cout<<"All alarm UI and sleep tests passed.\n";
}
