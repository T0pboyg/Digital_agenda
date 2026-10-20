#include "alarm_renderer.h"
#include <cstdio>
#include <algorithm>
#include "core/utf8.h"
namespace {
std::string timeText(TimeOfDay t){char b[16];std::snprintf(b,sizeof b,"%02d:%02d",t.hour,t.minute);return b;}
std::string dateText(Date d){char b[24];std::snprintf(b,sizeof b,"%02d/%02d/%04d",d.day,d.month,d.year);return b;}
void text(DrawSurface&s,UiRect r,std::string_view t,UiTextStyle style=UiTextStyle::Body,UiColor c=AgendaUiColors::PrimaryText){s.drawText(r,t,style,c,UiTextAlignment::Center);}
void button(DrawSurface&s,UiRect r,std::string_view t,bool accent=false){s.fillRoundedRectangle(r,6,accent?AgendaUiColors::Accent:AgendaUiColors::Surface);text(s,r,t,UiTextStyle::Body,accent?AgendaUiColors::Background:AgendaUiColors::PrimaryText);}
bool contains(UiRect r,UiPoint p){return p.x>=r.x&&p.x<r.x+r.width&&p.y>=r.y&&p.y<r.y+r.height;}
AgendaUiAction action(AgendaUiActionType t){return AgendaUiAction{t};}
std::string errorText(AlarmError e){
 switch(e){
 case AlarmError::None:return "";
 case AlarmError::InvalidAlarm:return "ΕΛΕΓΞΕ ΗΜΕΡΑ/ΩΡΑ";
 case AlarmError::WriteFailed:return "ΣΦΑΛΜΑ ΕΓΓΡΑΦΗΣ";
 case AlarmError::Full:return "ΕΩΣ 24 ΞΥΠΝΗΤΗΡΙΑ";
 case AlarmError::InvalidData:return "ΣΦΑΛΜΑ ΔΕΔΟΜΕΝΩΝ";
 default:return "ΣΦΑΛΜΑ ΑΝΑΓΝΩΣΗΣ";
 }
}
void battery(const AgendaUiView&v,DrawSurface&s,int x,int y){
 const UiColor color=v.batteryPercent&&*v.batteryPercent<=15?AgendaUiColors::Error:AgendaUiColors::SecondaryText;
 s.drawRectangle({x,y+3,17,10},color,1);s.fillRectangle({x+17,y+6,2,4},color);
 if(v.batteryPercent){const int w=13*std::clamp(*v.batteryPercent,0,100)/100;if(w)s.fillRectangle({x+2,y+5,w,6},color);}
 text(s,{x+21,y,36,16},v.batteryPercent?std::to_string(*v.batteryPercent)+"%":"--%",UiTextStyle::Caption,color);
}
void clockIcon(DrawSurface&s){
 s.fillRoundedRectangle(AlarmLayout::Open,6,AgendaUiColors::Surface);
 s.drawCircle({120,198},9,AgendaUiColors::Accent,1);
 s.drawLine({120,198},{120,191},AgendaUiColors::Accent,1);
 s.drawLine({120,198},{125,201},AgendaUiColors::Accent,1);
 s.drawLine({111,187},{114,189},AgendaUiColors::Accent,2);
 s.drawLine({126,189},{129,187},AgendaUiColors::Accent,2);
}
}
void renderDiaryStatus(const AgendaUiView&v,DrawSurface&s){
 if(!v.alarmFeature || v.overlay!=AgendaUiOverlay::None) return;
 const auto now=v.currentTime?timeText(v.currentTime->time):"--:--";
 if(v.screen==AgendaUiScreen::Month || v.screen==AgendaUiScreen::AlarmList){
   text(s,{68,216,42,16},now,UiTextStyle::Caption);battery(v,s,114,216);
   if(v.screen==AgendaUiScreen::Month) {
     clockIcon(s);
     if(v.alarmError!=AlarmError::None)s.fillCircle({133,185},3,AgendaUiColors::Error);
   }
 } else if(v.screen==AgendaUiScreen::Day){
   text(s,{96,16,48,18},now,UiTextStyle::Caption);
   battery(v,s,92,216);
 } else if(v.screen==AgendaUiScreen::EventDetails){
   s.fillRectangle(AgendaUiLayout::Header,AgendaUiColors::Background);
   text(s,{68,18,42,16},now,UiTextStyle::Caption);battery(v,s,114,18);
 }
}
bool renderAlarmUi(const AgendaUiView&v,DrawSurface&s){
 if(v.overlay==AgendaUiOverlay::AlarmSaveError){
   text(s,{36,70,168,24},"ΣΦΑΛΜΑ ΕΓΓΡΑΦΗΣ",UiTextStyle::Heading,AgendaUiColors::Error);
   text(s,{36,100,168,20},"ΔΕΝ ΑΠΟΘΗΚΕΥΤΗΚΕ");
   text(s,{36,122,168,20},"Η ΔΙΑΚΟΠΗ");
   button(s,AlarmLayout::Stop,"ΕΠΑΝΑΛΗΨΗ",true);
   return true;
 }
 if(v.overlay==AgendaUiOverlay::Alarm){
   text(s,{44,30,152,24},"ΞΥΠΝΗΤΗΡΙ",UiTextStyle::Heading,AgendaUiColors::Accent);
   const std::string name=v.ringingAlarmNames.empty()?"Ξυπνητήρι":v.ringingAlarmNames.front();
   const auto first=utf8Prefix(name,15);
   text(s,{40,57,160,20},first);
   text(s,{40,78,160,20},name.substr(first.size()));
   if(v.ringingAlarmNames.size()>1) text(s,{76,202,88,18},"+"+std::to_string(v.ringingAlarmNames.size()-1),UiTextStyle::Caption);
   text(s,{42,100,156,48},v.currentTime?timeText(v.currentTime->time):"--:--",UiTextStyle::Clock);
   button(s,AlarmLayout::Stop,"Διακοπή",true);
   return true;
 }
 if(v.screen!=AgendaUiScreen::AlarmList && v.screen!=AgendaUiScreen::AlarmEditor)return false;
 button(s,AlarmLayout::Back,"<");text(s,{78,18,84,24},"ΞΥΠΝΗΤΗΡΙ",UiTextStyle::Caption);
 if(v.alarmConfirmDelete){
   text(s,{42,90,156,32},"ΔΙΑΓΡΑΦΗ;",UiTextStyle::Heading);
   button(s,AlarmLayout::Yes,"ΝΑΙ",true);button(s,AlarmLayout::No,"ΟΧΙ");return true;
 }
 if(v.screen==AgendaUiScreen::AlarmList) text(s,{36,43,168,16},errorText(v.alarmError),UiTextStyle::Caption,AgendaUiColors::Error);
 if(v.screen==AgendaUiScreen::AlarmList){
   button(s,AlarmLayout::Add,"+",true);
   if(v.alarms.empty()){
     if(v.alarmError==AlarmError::ReadFailed||v.alarmError==AlarmError::InvalidData||v.alarmError==AlarmError::NotReady)
       button(s,{52,104,136,36},"ΕΠΑΝΑΛΗΨΗ");
     else text(s,{40,90,160,42},"ΠΑΤΗΣΕ +");
   }
   for(std::size_t i=0;i<v.alarms.size();++i){
     const auto&a=v.alarms[i];int y=62+40*static_cast<int>(i);
     s.fillRoundedRectangle({30,y,180,36},6,AgendaUiColors::Surface);
     text(s,{38,y,52,18},timeText(a.time),UiTextStyle::Body,a.enabled?AgendaUiColors::Accent:AgendaUiColors::Disabled);
     std::string date=a.daily?"ΚΑΘΕ ΜΕΡΑ":dateText(a.date).substr(0,5);
     text(s,{94,y,72,16},date,UiTextStyle::Caption,AgendaUiColors::SecondaryText);
     const auto label=a.name.empty()?"Ξυπνητήρι":a.name;
     text(s,{36,y+18,132,16},utf8Prefix(label,15)+(utf8CodePointCount(label)>15?"…":""),UiTextStyle::Caption);
     button(s,{172,y+3,32,30},a.enabled?"ON":"OFF",a.enabled);
   }
   button(s,AlarmLayout::Prev,"<");button(s,AlarmLayout::Next,">");
   text(s,{92,184,56,28},std::to_string(v.alarmPageCount?v.alarmPage+1:0)+"/"+std::to_string(v.alarmPageCount),UiTextStyle::Caption);
   renderDiaryStatus(v,s);return true;
 }
 button(s,AlarmLayout::Name,v.alarmError==AlarmError::None?(v.alarmDraft.name.empty()?"ΟΝΟΜΑ":utf8Prefix(v.alarmDraft.name,17)):errorText(v.alarmError));
 if(v.alarmDraft.id)button(s,AlarmLayout::Add,"X");
 if(!v.alarmDraft.daily){button(s,AlarmLayout::DatePrevious,"<");button(s,AlarmLayout::DateNext,">");}
 text(s,{54,62,132,26},v.alarmDraft.daily?"ΚΑΘΕ ΜΕΡΑ":dateText(v.alarmDraft.date));
 button(s,AlarmLayout::HourUp,"+");button(s,AlarmLayout::HourDown,"-");
 button(s,AlarmLayout::MinuteUp,"+");button(s,AlarmLayout::MinuteDown,"-");
 text(s,{52,120,136,30},timeText(v.alarmDraft.time),UiTextStyle::Heading);
 button(s,AlarmLayout::Daily,v.alarmDraft.daily?"ΚΑΘΗΜΕΡΙΝΟ":"ΜΙΑ ΦΟΡΑ");
 button(s,AlarmLayout::Enabled,v.alarmDraft.enabled?"ON":"OFF",v.alarmDraft.enabled);
 button(s,AlarmLayout::Cancel,"ΠΙΣΩ");button(s,AlarmLayout::Save,"OK",true);return true;
}
std::optional<AgendaUiAction> mapAlarmTouch(const AgendaUiView&v,UiPoint p){
 using T=AgendaUiActionType;
 if(v.overlay==AgendaUiOverlay::AlarmSaveError) return contains(AlarmLayout::Stop,p)?std::optional<AgendaUiAction>(action(T::RetryAlarmSave)):std::nullopt;
 if(v.overlay==AgendaUiOverlay::Alarm) return contains(AlarmLayout::Stop,p)?std::optional<AgendaUiAction>(action(T::StopAlarm)):std::nullopt;
 if(v.alarmConfirmDelete){if(contains(AlarmLayout::Yes,p))return action(T::ConfirmAlarmDelete);if(contains(AlarmLayout::No,p))return action(T::CancelAlarmDelete);return std::nullopt;}
 if(contains(AlarmLayout::Back,p))return action(T::Back);
 if(v.screen==AgendaUiScreen::AlarmList){
   if(contains(AlarmLayout::Add,p))return action(T::CreateAlarm);
   if(contains(AlarmLayout::Prev,p))return action(T::AlarmPreviousPage);
   if(contains(AlarmLayout::Next,p))return action(T::AlarmNextPage);
   if(v.alarms.empty()&&contains({52,104,136,36},p))return action(T::RetryAlarms);
   for(std::size_t i=0;i<v.alarms.size();++i){int y=62+40*static_cast<int>(i);if(contains({30,y,180,36},p)){
     auto a=action(contains({172,y+3,32,30},p)?T::ToggleAlarm:T::SelectAlarm);a.eventId=v.alarms[i].id;return a;
   }}
   return std::nullopt;
 }
 if(v.screen!=AgendaUiScreen::AlarmEditor)return std::nullopt;
 if(contains(AlarmLayout::Name,p))return action(T::EditAlarmName);
 if(v.alarmDraft.id&&contains(AlarmLayout::Add,p))return action(T::DeleteAlarm);
 if(!v.alarmDraft.daily&&contains(AlarmLayout::DatePrevious,p))return action(T::AlarmPreviousDay);
 if(!v.alarmDraft.daily&&contains(AlarmLayout::DateNext,p))return action(T::AlarmNextDay);
 const std::pair<UiRect,T> buttons[]{
 {AlarmLayout::HourUp,T::AlarmNextHour},{AlarmLayout::HourDown,T::AlarmPreviousHour},
 {AlarmLayout::MinuteUp,T::AlarmNextMinute},{AlarmLayout::MinuteDown,T::AlarmPreviousMinute},
 {AlarmLayout::Daily,T::AlarmToggleDaily},{AlarmLayout::Enabled,T::AlarmToggleEnabled},
 {AlarmLayout::Cancel,T::Back},{AlarmLayout::Save,T::SaveAlarm}};
 for(const auto&b:buttons)if(contains(b.first,p))return action(b.second);
 return std::nullopt;
}
