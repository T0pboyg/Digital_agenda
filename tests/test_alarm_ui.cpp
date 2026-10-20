#include <cassert>
#include <iostream>
#include "core/utf8.h"
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
 AgendaTouchMapper nameMapper;
 auto nameButton=nameMapper.map(ui.view(),{120,52},0);
 assert(nameButton); ui.handle(*nameButton);
 assert(ui.view().screen==AgendaUiScreen::EditorText);
 AgendaUiAction digit{AgendaUiActionType::T9Digit};digit.digit='2';digit.timestampMilliseconds=100;
 ui.handle(digit);assert(!ui.view().editor.text.empty());
 // A name is limited by characters, not UTF-8 bytes, and editing can be cancelled.
 ui.handle({AgendaUiActionType::ToggleT9Mode});
 for(int i=0;i<40;++i){digit.timestampMilliseconds+=1000;ui.handle(digit);}
 assert(utf8CodePointCount(ui.view().editor.text)==30);
 ui.handle({AgendaUiActionType::T9Backspace});assert(utf8CodePointCount(ui.view().editor.text)==29);
 const auto enteredName=ui.view().editor.text;
 ui.handle({AgendaUiActionType::EditorNext});assert(ui.view().screen==AgendaUiScreen::AlarmEditor);
 ui.handle(*nameButton);ui.handle({AgendaUiActionType::T9Backspace});
 ui.handle({AgendaUiActionType::EditorBack});assert(ui.view().alarmDraft.name==enteredName);
 ui.handle({AgendaUiActionType::AlarmNextMinute}); ui.handle({AgendaUiActionType::SaveAlarm});
 assert(alarms.alarms()[0].name==enteredName);
 AlarmService reloaded(a);assert(reloaded.load());assert(reloaded.alarms()[0].name==enteredName);
 assert(alarms.alarms().size()==1); assert(ui.view().screen==AgendaUiScreen::AlarmList);
 ui.handle({AgendaUiActionType::Back}); assert(ui.view().screen==AgendaUiScreen::Month);
 // An alarm preempts an unsaved event editor and restores it after stop.
 AgendaUiAction date{AgendaUiActionType::SelectDate};date.date=clock.now.date;ui.handle(date);ui.handle({AgendaUiActionType::StartCreate});
 const auto original=ui.view().screen;
 clock.now.time.minute=1;ui.tick();assert(ui.view().overlay==AgendaUiOverlay::Alarm);
 assert(ui.view().ringingAlarmNames.size()==1 && ui.view().ringingAlarmNames[0]==enteredName);
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
