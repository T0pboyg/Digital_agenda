#pragma once
#include <cstddef>
#include "audio_test_state.h"
using gpio_num_t=int;
using i2s_chan_handle_t=void*;
constexpr int ESP_OK=0,I2S_NUM_AUTO=-1,I2S_ROLE_MASTER=0,I2S_DATA_BIT_WIDTH_16BIT=16,I2S_SLOT_MODE_STEREO=2,I2S_GPIO_UNUSED=-1;
struct i2s_chan_config_t {int dma_desc_num=0,dma_frame_num=0;};
struct ClockConfig {unsigned rate;};
struct SlotConfig {int width,mode;};
struct GpioConfig {int mclk,bclk,ws,dout,din;};
struct i2s_std_config_t {ClockConfig clk_cfg;SlotConfig slot_cfg;GpioConfig gpio_cfg;};
#define I2S_CHANNEL_DEFAULT_CONFIG(port,role) i2s_chan_config_t{}
#define I2S_STD_CLK_DEFAULT_CONFIG(rate) ClockConfig{rate}
#define I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(width,mode) SlotConfig{width,mode}
inline int i2s_new_channel(const i2s_chan_config_t*,i2s_chan_handle_t*out,void*){*out=reinterpret_cast<void*>(1);return ESP_OK;}
inline int i2s_del_channel(i2s_chan_handle_t){++FakeAudio::deletedChannels;return ESP_OK;}
inline int i2s_channel_init_std_mode(i2s_chan_handle_t,const i2s_std_config_t*){return ESP_OK;}
inline int i2s_channel_enable(i2s_chan_handle_t){return ESP_OK;}
inline int i2s_channel_disable(i2s_chan_handle_t){return ESP_OK;}
inline int i2s_channel_write(i2s_chan_handle_t,const void*,std::size_t,std::size_t*written,unsigned){++FakeAudio::writes;*written=0;return -1;}
