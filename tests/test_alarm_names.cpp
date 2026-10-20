#include <cassert>
#include <iostream>
#include "core/alarm_service.h"
#include "core/crc32.h"
#include "ui/alarm_renderer.h"
class Store:public ByteStorage{public:mutable std::vector<std::uint8_t>b;ByteReadResult read()const override{return {b.empty()?ByteStorageError::NotFound:ByteStorageError::None,b};}ByteStorageError write(const std::vector<std::uint8_t>&v)const override{b=v;return ByteStorageError::None;}};
void put(std::vector<std::uint8_t>&b,std::uint32_t v){for(int i=0;i<4;++i)b.push_back(v>>(8*i));}
void crc(std::vector<std::uint8_t>&b){put(b,crc32(std::string_view(reinterpret_cast<const char*>(b.data()),b.size())));}
class Surface:public DrawSurface{public:std::vector<std::string> texts;
 void clear(UiColor)override{} void fillRectangle(const UiRect&,UiColor)override{} void drawRectangle(const UiRect&,UiColor,int)override{}
 void fillRoundedRectangle(const UiRect&,int,UiColor)override{} void drawRoundedRectangle(const UiRect&,int,UiColor,int)override{}
 void fillCircle(UiPoint,int,UiColor)override{} void drawCircle(UiPoint,int,UiColor,int)override{} void drawLine(UiPoint,UiPoint,UiColor,int)override{}
 void drawText(const UiRect&,std::string_view t,UiTextStyle,UiColor,UiTextAlignment)override{texts.emplace_back(t);}
};
int main(){
 Store store;AlarmService service(store);assert(service.load());DateTime now{{2026,10,20},{8,0}};
 Alarm a;a.date=now.date;a.time={8,1};a.name="Προπόνηση 123";assert(service.save(a,now));
 AlarmService reboot(store);assert(reboot.load());assert(reboot.alarms()[0].name==a.name);
 a.name=std::string(31,'a');assert(!reboot.save(a,now));
 a.name=std::string("\xff");assert(!reboot.save(a,now));
 a.name="";for(int i=0;i<30;++i)a.name+="ά";assert(reboot.save(a,now));
 AlarmService again(store);assert(again.load());assert(again.alarms()[1].name==a.name);
 // Legacy v1 with a pending daily occurrence must retain the acknowledgement.
 Store legacy;for(auto n:{0x314D4C41u,1u,1u,7u,2026u,10u,20u,8u,1u,7u,static_cast<unsigned>(serialDay(now.date)+1)})put(legacy.b,n);crc(legacy.b);
 AlarmService old(legacy);assert(old.load());assert(old.ringing());assert(old.alarms()[0].name.empty());assert(old.stop(now));
 auto migrated=old.alarms()[0];migrated.name="Μάθημα";assert(old.save(migrated,now));
 AlarmService migratedReboot(legacy);assert(migratedReboot.load());assert(migratedReboot.alarms()[0].name=="Μάθημα");
 now.time={8,1};migratedReboot.poll(now);assert(!migratedReboot.ringing());
 // Reject a valid-checksum but truncated v2 name without partial replacement.
 store.b.pop_back();store.b.pop_back();store.b.pop_back();store.b.pop_back();store.b.pop_back();crc(store.b);
 AlarmService corrupt(store);assert(!corrupt.load());
 AgendaUiView v;v.overlay=AgendaUiOverlay::Alarm;v.currentTime=now;v.ringingAlarmNames={a.name};Surface surface;
 assert(renderAlarmUi(v,surface));std::string rendered;for(auto&s:surface.texts)rendered+=s;
 assert(rendered.find(a.name)!=std::string::npos);assert(rendered.find("Διακοπή")!=std::string::npos);
 std::cout<<"Alarm names: persistence, legacy migration, validation and full ringing label passed.\n";
}
