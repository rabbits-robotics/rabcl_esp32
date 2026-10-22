// fake: esp_twai_onchip.h（ユニットテスト用）
#ifndef FAKES__ESP_TWAI_ONCHIP_H_
#define FAKES__ESP_TWAI_ONCHIP_H_

#include <cstdint>

#include "driver/gpio.h"
#include "esp_twai.h"

typedef int twai_clock_source_t;

typedef struct {
  uint32_t bitrate;
  uint16_t sp_permill;
  uint16_t ssp_permill;
} twai_timing_basic_config_t;

typedef struct {
  struct {
    gpio_num_t tx;
    gpio_num_t rx;
    gpio_num_t quanta_clk_out;
    gpio_num_t bus_off_indicator;
  } io_cfg;
  twai_clock_source_t clk_src;
  twai_timing_basic_config_t bit_timing;
  twai_timing_basic_config_t data_timing;
  uint32_t timestamp_resolution_hz;
  int8_t fail_retry_cnt;
  uint32_t tx_queue_depth;
  int intr_priority;
  struct {
    uint32_t enable_self_test : 1;
    uint32_t enable_loopback : 1;
    uint32_t enable_listen_only : 1;
    uint32_t no_receive_rtr : 1;
    uint32_t sleep_allow_pd : 1;
  } flags;
} twai_onchip_node_config_t;

esp_err_t twai_new_node_onchip(const twai_onchip_node_config_t * node_config, twai_node_handle_t * node_ret);

#endif  // FAKES__ESP_TWAI_ONCHIP_H_
