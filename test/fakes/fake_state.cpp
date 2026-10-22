#include "fake_state.hpp"

#include <algorithm>
#include <cstring>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

namespace fake_state
{

std::vector<twai_node_t *> & twai_nodes()
{
  static std::vector<twai_node_t *> nodes;
  return nodes;
}

esp_err_t & twai_new_node_result()
{
  static esp_err_t result = ESP_OK;
  return result;
}

std::map<int, GpioRecord> & gpio()
{
  static std::map<int, GpioRecord> records;
  return records;
}

std::vector<std::unique_ptr<FakeQueue>> & queues()
{
  static std::vector<std::unique_ptr<FakeQueue>> list;
  return list;
}

void Reset()
{
  for (auto * n : twai_nodes()) {
    delete n;
  }
  twai_nodes().clear();
  twai_new_node_result() = ESP_OK;
  gpio().clear();
  queues().clear();
}

}  // namespace fake_state

// --- twai_node_t: テストからコールバックを起こす ---

void twai_node_t::FireRx(const RxFrame & frame)
{
  rx_pending.push_back(frame);
  if (callbacks.on_rx_done != nullptr) {
    twai_rx_done_event_data_t edata = {};
    callbacks.on_rx_done(this, &edata, user_ctx);
  }
}

void twai_node_t::FireTxDone(bool success)
{
  if (callbacks.on_tx_done != nullptr) {
    twai_frame_t frame = {};
    twai_tx_done_event_data_t edata = {};
    edata.is_tx_success = success;
    edata.done_tx_frame = &frame;
    callbacks.on_tx_done(this, &edata, user_ctx);
  }
}

void twai_node_t::FireError(uint32_t flags)
{
  if (callbacks.on_error != nullptr) {
    twai_error_event_data_t edata = {};
    edata.err_flags.val = flags;
    callbacks.on_error(this, &edata, user_ctx);
  }
}

void twai_node_t::FireStateChange(twai_error_state_t from, twai_error_state_t to)
{
  status.state = to;
  if (callbacks.on_state_change != nullptr) {
    twai_state_change_event_data_t edata = {};
    edata.old_sta = from;
    edata.new_sta = to;
    callbacks.on_state_change(this, &edata, user_ctx);
  }
}

// --- esp_twai_onchip.h / esp_twai.h ---

esp_err_t twai_new_node_onchip(const twai_onchip_node_config_t * node_config, twai_node_handle_t * node_ret)
{
  if (fake_state::twai_new_node_result() != ESP_OK) {
    return fake_state::twai_new_node_result();
  }
  auto * node = new twai_node_t();
  node->config = *node_config;
  fake_state::twai_nodes().push_back(node);
  *node_ret = node;
  return ESP_OK;
}

esp_err_t twai_node_register_event_callbacks(twai_node_handle_t node, const twai_event_callbacks_t * cbs, void * user_data)
{
  node->callbacks = *cbs;
  node->user_ctx = user_data;
  return ESP_OK;
}

esp_err_t twai_node_enable(twai_node_handle_t node)
{
  node->enabled = true;
  return ESP_OK;
}

esp_err_t twai_node_disable(twai_node_handle_t node)
{
  node->enabled = false;
  return ESP_OK;
}

esp_err_t twai_node_recover(twai_node_handle_t node)
{
  node->recover_calls++;
  return ESP_OK;
}

esp_err_t twai_node_delete(twai_node_handle_t node)
{
  node->deleted = true;
  return ESP_OK;
}

esp_err_t twai_node_transmit(twai_node_handle_t node, const twai_frame_t * frame, int timeout_ms)
{
  if (node->transmit_result != ESP_OK) {
    return node->transmit_result;
  }
  twai_node_t::TxRecord rec;
  rec.id = frame->header.id;
  rec.dlc = frame->header.dlc;
  rec.ide = frame->header.ide != 0;
  rec.rtr = frame->header.rtr != 0;
  rec.data.assign(frame->buffer, frame->buffer + frame->buffer_len);
  rec.timeout_ms = timeout_ms;
  node->tx.push_back(rec);
  return ESP_OK;
}

esp_err_t twai_node_transmit_wait_all_done(twai_node_handle_t node, int)
{
  node->wait_all_done_calls++;
  return ESP_OK;
}

esp_err_t twai_node_receive_from_isr(twai_node_handle_t node, twai_frame_t * rx_frame)
{
  if (node->rx_pending.empty()) {
    return ESP_FAIL;
  }
  const auto f = node->rx_pending.front();
  node->rx_pending.pop_front();
  rx_frame->header.id = f.id;
  rx_frame->header.dlc = f.dlc;
  rx_frame->header.ide = f.ide ? 1 : 0;
  rx_frame->header.rtr = f.rtr ? 1 : 0;
  rx_frame->header.timestamp = f.timestamp;
  const size_t n = std::min(f.data.size(), rx_frame->buffer_len);
  std::memcpy(rx_frame->buffer, f.data.data(), n);
  return ESP_OK;
}

esp_err_t twai_node_get_info(twai_node_handle_t node, twai_node_status_t * status_ret, twai_node_record_t * statistics_ret)
{
  if (status_ret != nullptr) {
    *status_ret = node->status;
  }
  if (statistics_ret != nullptr) {
    *statistics_ret = node->record;
  }
  return ESP_OK;
}

// --- driver/gpio.h ---

esp_err_t gpio_config(const gpio_config_t * cfg)
{
  for (int pin = 0; pin < 64; ++pin) {
    if (cfg->pin_bit_mask & (1ULL << pin)) {
      fake_state::gpio()[pin].config = *cfg;
    }
  }
  return ESP_OK;
}

esp_err_t gpio_set_level(gpio_num_t pin, uint32_t level)
{
  fake_state::gpio()[static_cast<int>(pin)].levels.push_back(level);
  return ESP_OK;
}

// --- freertos/queue.h ---

QueueHandle_t xQueueCreate(UBaseType_t length, UBaseType_t item_size)
{
  auto q = std::make_unique<fake_state::FakeQueue>();
  q->length = length;
  q->item_size = item_size;
  fake_state::queues().push_back(std::move(q));
  return fake_state::queues().back().get();
}

void vQueueDelete(QueueHandle_t)
{
  // Reset() でまとめて解放する
}

BaseType_t xQueueSendFromISR(QueueHandle_t queue, const void * item, BaseType_t * higher_priority_task_woken)
{
  auto * q = static_cast<fake_state::FakeQueue *>(queue);
  if (higher_priority_task_woken != nullptr) {
    *higher_priority_task_woken = pdFALSE;
  }
  if (q->items.size() >= q->length) {
    return pdFALSE;
  }
  const auto * bytes = static_cast<const uint8_t *>(item);
  q->items.emplace_back(bytes, bytes + q->item_size);
  return pdTRUE;
}

BaseType_t xQueueReceive(QueueHandle_t queue, void * buffer, TickType_t)
{
  auto * q = static_cast<fake_state::FakeQueue *>(queue);
  if (q->items.empty()) {
    return pdFALSE;
  }
  std::memcpy(buffer, q->items.front().data(), q->item_size);
  q->items.pop_front();
  return pdTRUE;
}
