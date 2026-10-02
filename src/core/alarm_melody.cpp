#include "alarm_melody.h"
#include <algorithm>
void AlarmMelody::fillStereo(std::int16_t* output,std::size_t frames){
    constexpr unsigned notes[]{784,988,1175,988,0};
    for(std::size_t i=0;i<frames;++i){
        const unsigned local=frame_%4000;
        const unsigned hz=notes[frame_/4000];
        if(local==0)phase_=0;
        phase_=(phase_+hz)%SampleRate;
        int value=static_cast<int>(phase_)*8800/static_cast<int>(SampleRate)-4400;
        if(value<0)value=-value;
        value-=2200;
        // Gentle attack/release avoids clicks at the note boundaries.
        const int envelope=static_cast<int>(std::min({local,3999-local,80u}));
        const auto sample=static_cast<std::int16_t>(hz?value*envelope/80:0);
        output[2*i]=sample;output[2*i+1]=sample;
        frame_=(frame_+1)%20000;
    }
}
