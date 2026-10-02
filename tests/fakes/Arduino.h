#pragma once
#include "audio_test_state.h"
constexpr int OUTPUT=1,LOW=0;
inline unsigned long millis(){return FakeAudio::now;}
inline void delay(unsigned n){FakeAudio::now+=n;}
inline void pinMode(int,int){}
inline void digitalWrite(int,int){}
