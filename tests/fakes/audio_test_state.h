#pragma once
#include <cstdint>
namespace FakeAudio {
inline bool runImmediately=true;
inline bool createFails=false;
inline void (*worker)(void*)=nullptr;
inline void* context=nullptr;
inline unsigned now=0;
inline int deletedChannels=0;
inline int writes=0;
inline void finishWorker(){auto fn=worker;worker=nullptr;if(fn)fn(context);}
}
