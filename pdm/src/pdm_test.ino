#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "driver/i2s.h"
#include <math.h>

#define SAMPLE_RATE     (48000)
#define I2S_NUM         (I2S_NUM_0)
//#define I2S_NUM         (0)
#define CHANNELS        (2)
#define DMA_BUF_COUNT   (6)
#define DMA_BUF_LEN     (32)

typedef struct {
    unsigned int tx_frame;
    unsigned int rx_val[CHANNELS];
    unsigned int last_rx_val[CHANNELS];
    unsigned int delta;
    int tx_error;
    int rx_error;
    int ramp_errors;
    int framing_errors;
    int channel_sync_errors;
} ramp_state_t;

ramp_state_t ramp_state;

void setup() {
    i2s_config_t i2s_config = {
      .mode = (i2s_mode_t) (I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_RX | I2S_MODE_PDM),
      .sample_rate = SAMPLE_RATE,
      .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
      .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,                           //2-channels
      .communication_format = (i2s_comm_format_t) (I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB),
      .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,                                //Interrupt level 1
      .dma_buf_count = DMA_BUF_COUNT,
      .dma_buf_len = DMA_BUF_LEN,                                                      //
      .use_apll = 1,                                                          // I2S using APLL as main I2S clock, enable it to get accurate clock
    };
    i2s_pin_config_t pin_config = {
        .bck_io_num = 26,
        .ws_io_num = 25,
        .data_out_num = 22,
        .data_in_num = 23,
    };

    ramp_state.tx_frame = 0;
    ramp_state.ramp_errors = 0;
    ramp_state.framing_errors = 0;
    ramp_state.channel_sync_errors = 0;
    nvs_flash_init();
    
    i2s_driver_install(I2S_NUM, &i2s_config, 0, NULL);
    i2s_set_pin(I2S_NUM, &pin_config);
    Serial.begin(115200);
    Serial.println("delay tx_err rx_err ramp_err framing_err channel_sync_err");
}

void loop_sin()
{
#define SAMPLE_PER_CYCLE (SAMPLE_RATE/WAVE_FREQ_HZ)
#define WAVE_FREQ_HZ    (100)
    unsigned int i, sample_val;
    float sin_float, triangle_float, triangle_step = 65536.0 / SAMPLE_PER_CYCLE;

    triangle_float = -32767;
    for(i = 0; i < SAMPLE_PER_CYCLE; i++) {
        sin_float = sin(i * PI / 180.0);
        if(sin_float >= 0)
            triangle_float += triangle_step;
        else
            triangle_float -= triangle_step;

        sin_float *= 32767;
        sample_val = 0;
        sample_val += (short)triangle_float;
        sample_val = sample_val << 16;
        sample_val += (short) sin_float;
	
        i2s_push_sample(I2S_NUM, (char *)&sample_val, portMAX_DELAY);
    }

}
void loop_ramp()
{
    unsigned int tx_frame[CHANNELS];
    int i;
    for (i = 0; i < CHANNELS; i++) {
	tx_frame[i] = (ramp_state.tx_frame & 0x00ffffff) + (i<<24);
    }
    tx_frame[0] = 0xAAAAAAAA;
    tx_frame[1] = 0xFFFFFFFF;
    ramp_state.tx_frame++;
    if (i2s_push_sample(I2S_NUM, (char *)tx_frame, portMAX_DELAY) == ESP_FAIL)
	ramp_state.tx_error = 1;
    if (i2s_pop_sample (I2S_NUM, (char *)ramp_state.rx_val, portMAX_DELAY) == ESP_FAIL)
	ramp_state.rx_error = 1;

    for (i = 1; i < CHANNELS; i++) {
	if  ((ramp_state.rx_val[i] & 0x00FFFFFF) !=
	     (ramp_state.rx_val[0] & 0x00FFFFFF))
	    ramp_state.framing_errors++;
    }

    for (i = 0; i < CHANNELS; i++) {
	if  (((ramp_state.rx_val[i] & 0xFF000000) >> 24) != i)
	    ramp_state.channel_sync_errors++;
    }
    ramp_state.delta = tx_frame[0] - ramp_state.rx_val[0];
    memcpy(ramp_state.last_rx_val, ramp_state.rx_val, sizeof(ramp_state.rx_val));
}
void loop()
{
    loop_ramp();
    if (((ramp_state.tx_frame) % SAMPLE_RATE) == 0) {
	Serial.print(ramp_state.delta);
	Serial.print(" ");
	Serial.print(ramp_state.tx_error);
	Serial.print(" ");
	Serial.print(ramp_state.rx_error);
	Serial.print(" ");
	Serial.print(ramp_state.ramp_errors);
	Serial.print(" ");
	Serial.print(ramp_state.framing_errors);
	Serial.print(" ");
	Serial.print(ramp_state.channel_sync_errors);
	Serial.println();
	ramp_state.tx_error = 0;
	ramp_state.rx_error = 0;
	ramp_state.ramp_errors = 0;
	ramp_state.framing_errors = 0;
	ramp_state.channel_sync_errors = 0;
    }
}
