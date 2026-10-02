#include <cassert>
#include <iostream>
#include "core/alarm_melody.h"
int main(){
 AlarmMelody melody;std::int16_t buffer[512];bool nonzero=false;
 for(int j=0;j<16;++j){melody.fillStereo(buffer,256);for(int i=0;i<256;++i){assert(buffer[2*i]==buffer[2*i+1]);assert(buffer[2*i]>=-2200&&buffer[2*i]<=2200);nonzero|=buffer[2*i]!=0;}}
 assert(nonzero);melody.reset();std::int16_t a[64],b[64];melody.fillStereo(a,32);melody.reset();melody.fillStereo(b,32);for(int i=0;i<64;++i)assert(a[i]==b[i]);
 // Last quarter-second of each phrase is silent, then pattern restarts.
 melody.reset();for(int i=0;i<16000/256;++i)melody.fillStereo(buffer,256);
 melody.fillStereo(buffer,128); // reach frame 16000
 melody.fillStereo(buffer,256);for(auto sample:buffer)assert(sample==0);
 std::cout<<"All alarm melody tests passed.\n";
}
