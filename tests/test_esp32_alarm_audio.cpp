#include <cassert>
#include <iostream>
#include <string>
#define ARDUINO 1
#include "../src/platform/esp32/alarm_audio.cpp"
int main(int argc,char**argv){
 const std::string mode=argc>1?argv[1]:"all";
 if(mode=="race"||mode=="all"){
  FakeAudio::runImmediately=true;Esp32AlarmAudio audio;
  assert(!audio.setRinging(true)); // A worker failure before start() returns must survive.
  assert(FakeAudio::writes==1);audio.setRinging(false);assert(FakeAudio::deletedChannels==1);
 }
 if(mode=="cleanup"||mode=="all"){
  const int old=FakeAudio::deletedChannels;
  FakeAudio::runImmediately=false;Esp32AlarmAudio audio;assert(audio.setRinging(true));
  audio.setRinging(false);assert(FakeAudio::deletedChannels==old); // Worker still owns DMA.
  FakeAudio::finishWorker();audio.setRinging(false);assert(FakeAudio::deletedChannels==old+1);
 }
 std::cout<<"ESP32 audio lifecycle tests passed: "<<mode<<"\n";
}
