#pragma once
#include <cstddef>
#include <cstdint>
class AlarmMelody {
public:
    static constexpr unsigned SampleRate=16000;
    void reset(){frame_=0;phase_=0;}
    void fillStereo(std::int16_t* output,std::size_t frames);
private:
    unsigned frame_=0;
    unsigned phase_=0;
};
