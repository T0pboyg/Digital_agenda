#include "alarm_ui_controller.h"
#include <algorithm>
#include "utf8.h"

void AlarmUiController::clampPage() {
    if(!alarms_) return;
    const auto count=alarms_->alarms().size();
    pageIndex_=std::min(pageIndex_,count? (count-1)/3:0);
}
bool AlarmUiController::handle(const AgendaUiAction& a,const std::optional<DateTime>& now,bool allowOpen) {
    if(!alarms_) return false;
    if(a.type==AgendaUiActionType::OpenAlarms && allowOpen) {page_=Page::List;clampPage();return true;}
    if(!active()) return false;
    if(page_==Page::Name) {
        using T=AgendaUiActionType;
        switch(a.type) {
        case T::T9Digit: {
            auto next=nameInput_;next.pressDigit(a.digit,a.timestampMilliseconds);
            if(utf8CodePointCount(next.text())<=AlarmService::MaxNameLength) nameInput_=std::move(next);
            break;
        }
        case T::ToggleT9Language:nameInput_.toggleLanguage();break;
        case T::ToggleT9Mode:nameInput_.toggleMode();break;
        case T::ToggleT9Case:nameInput_.toggleCase();break;
        case T::T9Backspace:nameInput_.backspace();break;
        case T::EditorNext:nameInput_.commitPending();draft_.name=nameInput_.text();page_=Page::Editor;break;
        case T::Back:case T::EditorBack:page_=Page::Editor;break;
        default:break;
        }
        return true;
    }
    if(confirmDelete_) {
        if(a.type==AgendaUiActionType::CancelAlarmDelete) confirmDelete_=false;
        if(a.type==AgendaUiActionType::ConfirmAlarmDelete) {
            if(alarms_->remove(draft_.id)) {page_=Page::List;confirmDelete_=false;clampPage();error_=AlarmError::None;}
            else {error_=alarms_->error();confirmDelete_=false;}
        }
        return true;
    }
    if(a.type==AgendaUiActionType::Back) {page_=page_==Page::Editor?Page::List:Page::Closed;error_=AlarmError::None;return true;}
    if(a.type==AgendaUiActionType::RetryAlarms && !alarms_->ready()) {alarms_->load();error_=alarms_->error();return true;}
    if(!alarms_->ready()) return true;
    if(!now) {error_=AlarmError::InvalidAlarm;return true;}
    error_=AlarmError::None;
    if(page_==Page::List) {
        switch(a.type) {
        case AgendaUiActionType::AlarmPreviousPage: if(pageIndex_) --pageIndex_; break;
        case AgendaUiActionType::AlarmNextPage: ++pageIndex_;clampPage();break;
        case AgendaUiActionType::CreateAlarm:
            draft_=Alarm{};draft_.date=now->date;draft_.time=now->time; page_=Page::Editor;break;
        case AgendaUiActionType::SelectAlarm:
        case AgendaUiActionType::ToggleAlarm:
            for(const auto& alarm:alarms_->alarms()) if(a.eventId && alarm.id==*a.eventId) {
                draft_=alarm;
                if(a.type==AgendaUiActionType::SelectAlarm) page_=Page::Editor;
                else {draft_.enabled=!draft_.enabled;if(!alarms_->save(draft_,*now))error_=alarms_->error();}
                break;
            }
            break;
        default:break;
        }
        return true;
    }
    switch(a.type) {
    case AgendaUiActionType::EditAlarmName:
        nameInput_=T9Engine{};nameInput_.toggleLanguage();nameInput_.setText(draft_.name);page_=Page::Name;break;
    case AgendaUiActionType::AlarmPreviousDay: if(auto d=previousDate(draft_.date)) draft_.date=*d;break;
    case AgendaUiActionType::AlarmNextDay: if(auto d=nextDate(draft_.date)) draft_.date=*d;break;
    case AgendaUiActionType::AlarmPreviousHour: draft_.time.hour=(draft_.time.hour+23)%24;break;
    case AgendaUiActionType::AlarmNextHour: draft_.time.hour=(draft_.time.hour+1)%24;break;
    case AgendaUiActionType::AlarmPreviousMinute: draft_.time.minute=(draft_.time.minute+59)%60;break;
    case AgendaUiActionType::AlarmNextMinute: draft_.time.minute=(draft_.time.minute+1)%60;break;
    case AgendaUiActionType::AlarmToggleDaily: draft_.daily=!draft_.daily;break;
    case AgendaUiActionType::AlarmToggleEnabled: draft_.enabled=!draft_.enabled;break;
    case AgendaUiActionType::SaveAlarm:
        if(alarms_->save(draft_,*now)){page_=Page::List;clampPage();}else error_=alarms_->error();break;
    case AgendaUiActionType::DeleteAlarm: if(draft_.id) confirmDelete_=true;break;
    default:break;
    }
    return true;
}
void AlarmUiController::populate(AgendaUiView& v) const {
    if(!alarms_) return;
    v.alarmError=error_==AlarmError::None?alarms_->error():error_;
    for(const auto& alarm:alarms_->alarms()) if(alarm.pending) v.ringingAlarmNames.push_back(alarm.name.empty()?"Ξυπνητήρι":alarm.name);
    if(!active()) return;
    // Agenda overlays stay underneath the alarm pages and resume when they close.
    v.overlay=AgendaUiOverlay::None;
    v.screen=page_==Page::List?AgendaUiScreen::AlarmList:AgendaUiScreen::AlarmEditor;
    if(page_==Page::Name) {
        v.screen=AgendaUiScreen::EditorText;v.alarmNameEditor=true;v.editor.text=nameInput_.text();
        v.t9Language=nameInput_.language();v.t9Mode=nameInput_.mode();v.t9Uppercase=nameInput_.uppercase();
    }
    v.alarmDraft=draft_;v.alarmConfirmDelete=confirmDelete_;v.alarmPage=pageIndex_;
    v.alarmPageCount=(alarms_->alarms().size()+2)/3;
    for(std::size_t i=pageIndex_*3;i<alarms_->alarms().size() && i<(pageIndex_+1)*3;++i) v.alarms.push_back(alarms_->alarms()[i]);
}
