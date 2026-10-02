#include "alarm_audio.h"
#include "board_pins.h"
#ifdef ARDUINO
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#endif
bool Esp32AlarmAudio::setRinging(bool ringing){
    // A timed-out stop leaves the channel owned by the worker. Retry cleanup
    // on later calls even after the requested ringing state has been cleared.
    if(!ringing){stop();requested_=false;return true;}
    if(!requested_){
        requested_=true;
        failed_=false;
        if(!start()) failed_=true;
    }
    return !failed_;
}
bool Esp32AlarmAudio::start(){
#ifdef ARDUINO
    stop();
    if(channel_!=nullptr)return false;
    i2s_chan_config_t config=I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO,I2S_ROLE_MASTER);
    config.dma_desc_num=4;config.dma_frame_num=256;
    if(i2s_new_channel(&config,&channel_,nullptr)!=ESP_OK)return false;
    i2s_std_config_t standard{};
    standard.clk_cfg=I2S_STD_CLK_DEFAULT_CONFIG(AlarmMelody::SampleRate);
    standard.slot_cfg=I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT,I2S_SLOT_MODE_STEREO);
    standard.gpio_cfg.mclk=I2S_GPIO_UNUSED;
    standard.gpio_cfg.bclk=static_cast<gpio_num_t>(Esp32Board::AlarmBclk);
    standard.gpio_cfg.ws=static_cast<gpio_num_t>(Esp32Board::AlarmLrclk);
    standard.gpio_cfg.dout=static_cast<gpio_num_t>(Esp32Board::AlarmData);
    standard.gpio_cfg.din=I2S_GPIO_UNUSED;
    if(i2s_channel_init_std_mode(channel_,&standard)!=ESP_OK){i2s_del_channel(channel_);channel_=nullptr;return false;}
    if(i2s_channel_enable(channel_)!=ESP_OK){i2s_del_channel(channel_);channel_=nullptr;return false;}
    melody_.reset();failed_=false;running_=true;finished_=false;
    if(xTaskCreate(audioTask,"alarm-audio",4096,this,1,nullptr)!=pdPASS){
        running_=false;finished_=true;stop();return false;
    }
    return true;
#else
    return false;
#endif
}
void Esp32AlarmAudio::audioTask(void* context){
#ifdef ARDUINO
    auto& self=*static_cast<Esp32AlarmAudio*>(context);
    std::int16_t buffer[512];
    while(self.running_){
        self.melody_.fillStereo(buffer,256);
        std::size_t written=0;
        const auto error=i2s_channel_write(self.channel_,buffer,sizeof buffer,&written,40);
        if(error!=ESP_OK || written!=sizeof buffer){self.failed_=true;break;}
    }
    self.finished_=true;
    vTaskDelete(nullptr);
#else
    (void)context;
#endif
}
void Esp32AlarmAudio::stop(){
    running_=false;
#ifdef ARDUINO
    // Writes have a 40ms timeout. Never release DMA while the worker is using it.
    const auto started=millis();
    while(!finished_ && static_cast<std::uint32_t>(millis()-started)<100)delay(1);
    if(finished_ && channel_){
        i2s_channel_disable(channel_);i2s_del_channel(channel_);channel_=nullptr;
        pinMode(Esp32Board::AlarmBclk,OUTPUT);digitalWrite(Esp32Board::AlarmBclk,LOW);
        pinMode(Esp32Board::AlarmLrclk,OUTPUT);digitalWrite(Esp32Board::AlarmLrclk,LOW);
        pinMode(Esp32Board::AlarmData,OUTPUT);digitalWrite(Esp32Board::AlarmData,LOW);
    }
#endif
}
