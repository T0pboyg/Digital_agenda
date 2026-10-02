#pragma once
#include "audio_test_state.h"
inline int xTaskCreate(void(*fn)(void*),const char*,unsigned,void*context,unsigned,void*){
 if(FakeAudio::createFails)return 0;
 FakeAudio::worker=fn;FakeAudio::context=context;
 if(FakeAudio::runImmediately)FakeAudio::finishWorker();
 return 1;
}
inline void vTaskDelete(void*){}
