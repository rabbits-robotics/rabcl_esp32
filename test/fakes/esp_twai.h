// fake: esp_twai.h（ユニットテスト用。esp_driver_twai の型と関数の最小集合。hal/twai_types.h の分も含む）
#ifndef FAKES__ESP_TWAI_H_
#define FAKES__ESP_TWAI_H_

#include <cstddef>
#include <cstdint>

#include "esp_err.h"

// 本物のヘッダと同じ無名構造体を使うため、この偽ヘッダの中だけ pedantic を外す
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"

typedef struct twai_node_t * twai_node_handle_t;

typedef enum {
  TWAI_ERROR_ACTIVE,
  TWAI_ERROR_WARNING,
  TWAI_ERROR_PASSIVE,
  TWAI_ERROR_BUS_OFF,
} twai_error_state_t;

typedef union {
  struct {
    uint32_t arb_lost : 1;
    uint32_t bit_err : 1;
    uint32_t form_err : 1;
    uint32_t stuff_err : 1;
    uint32_t ack_err : 1;
  };
  uint32_t val;
} twai_error_flags_t;

typedef struct {
  uint32_t id;
  uint16_t dlc;
  struct {
    uint32_t ide : 1;
    uint32_t rtr : 1;
    uint32_t fdf : 1;
    uint32_t brs : 1;
    uint32_t esi : 1;
  };
  union {
    uint64_t timestamp;
    uint64_t trigger_time;
  };
} twai_frame_header_t;

typedef struct {
  twai_frame_header_t header;
  uint8_t * buffer;
  size_t buffer_len;
} twai_frame_t;

typedef struct {
  twai_error_state_t state;
  uint16_t tx_error_count;
  uint16_t rx_error_count;
  uint32_t tx_queue_remaining;
} twai_node_status_t;

typedef struct {
  uint32_t bus_err_num;
} twai_node_record_t;

typedef struct {
  bool is_tx_success;
  const twai_frame_t * done_tx_frame;
} twai_tx_done_event_data_t;

typedef struct {
} twai_rx_done_event_data_t;

typedef struct {
  twai_error_state_t old_sta;
  twai_error_state_t new_sta;
} twai_state_change_event_data_t;

typedef struct {
  twai_error_flags_t err_flags;
} twai_error_event_data_t;

typedef struct {
  bool (*on_tx_done)(twai_node_handle_t handle, const twai_tx_done_event_data_t * edata, void * user_ctx);
  bool (*on_rx_done)(twai_node_handle_t handle, const twai_rx_done_event_data_t * edata, void * user_ctx);
  bool (*on_state_change)(twai_node_handle_t handle, const twai_state_change_event_data_t * edata, void * user_ctx);
  bool (*on_error)(twai_node_handle_t handle, const twai_error_event_data_t * edata, void * user_ctx);
} twai_event_callbacks_t;

esp_err_t twai_node_enable(twai_node_handle_t node);
esp_err_t twai_node_disable(twai_node_handle_t node);
esp_err_t twai_node_recover(twai_node_handle_t node);
esp_err_t twai_node_delete(twai_node_handle_t node);
esp_err_t twai_node_register_event_callbacks(twai_node_handle_t node, const twai_event_callbacks_t * cbs, void * user_data);
esp_err_t twai_node_transmit(twai_node_handle_t node, const twai_frame_t * frame, int timeout_ms);
esp_err_t twai_node_transmit_wait_all_done(twai_node_handle_t node, int timeout_ms);
esp_err_t twai_node_receive_from_isr(twai_node_handle_t node, twai_frame_t * rx_frame);
esp_err_t twai_node_get_info(twai_node_handle_t node, twai_node_status_t * status_ret, twai_node_record_t * statistics_ret);

#pragma GCC diagnostic pop

#endif  // FAKES__ESP_TWAI_H_
