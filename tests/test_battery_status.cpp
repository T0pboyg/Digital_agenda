#include <cassert>
#include <iostream>
#include "core/battery_status.h"
int main() {
 assert(!batteryPercent(0)); assert(!batteryPercent(5000));
 assert(batteryPercent(3300)==0); assert(batteryPercent(4200)==100);
 assert(batteryPercent(3800)==50);
 int previous=0; for(int v=3300;v<=4250;++v){auto p=batteryPercent(v);assert(p&&*p>=previous&&*p<=100);previous=*p;}
 BatteryStatus s; s.sampleAdcMillivolts(1267); assert(s.percent() && *s.percent()>=50 && *s.percent()<=51);
 s.sampleAdcMillivolts(1200); assert(s.millivolts()>3600);
 s.sampleAdcMillivolts(0); assert(!s.percent());
 std::cout << "All battery tests passed.\n";
}
