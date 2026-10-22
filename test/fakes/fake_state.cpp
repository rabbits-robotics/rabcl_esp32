#include "fake_state.hpp"

#include <algorithm>
#include <cstring>

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

std::map<int, UartRecord> & uart()
{
  static std::map<int, UartRecord> records;
  return records;
}

esp_err_t & uart_install_result()
{
  static esp_err_t result = ESP_OK;
  return result;
}

std::vector<std::unique_ptr<FakeQueue>> & queues()
{
  static std::vector<std::unique_ptr<FakeQueue>> list;
  return list;
}

std::deque<PendedCall> & pended_calls()
{
  static std::deque<PendedCall> calls;
  return calls;
}

bool & pend_result()
{
  static bool result = true;
  return result;
}

int RunPendedFunctions()
{
  int n = 0;
  while (!pended_calls().empty()) {
    PendedCall c = pended_calls().front();
    pended_calls().pop_front();
    c.function(c.arg1, c.arg2);
    ++n;
  }
  return n;
}

void Reset()
{
  for (auto * n : twai_nodes()) {
    delete n;
  }
  twai_nodes().clear();
  twai_new_node_result() = ESP_OK;
  gpio().clear();
  uart().clear();
  uart_install_result() = ESP_OK;
  queues().clear();
  pended_calls().clear();
  pend_result() = true;
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
  if (tx_pending.empty()) {
    return;
  }
  TxRecord rec = tx_pending.front();
  tx_pending.pop_front();
  // 本物のドライバはこの時点までフレームのポインタ越しに内容を読む
  rec.id = rec.frame->header.id;
  rec.dlc = rec.frame->header.dlc;
  rec.ide = rec.frame->header.ide != 0;
  rec.rtr = rec.frame->header.rtr != 0;
  rec.data.assign(rec.frame->buffer, rec.frame->buffer + rec.frame->buffer_len);
  tx_done.push_back(rec);
  if (callbacks.on_tx_done != nullptr) {
    twai_tx_done_event_data_t edata = {};
    edata.is_tx_success = success;
    edata.done_tx_frame = rec.frame;
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
  // 本物と同じく、ハードのキュー（tx_queue_depth）+ 送信中 1 つまでしか受け付けない
  if (node->tx_pending.size() >= node->config.tx_queue_depth + 1) {
    return ESP_ERR_TIMEOUT;
  }
  twai_node_t::TxRecord rec = {};
  rec.frame = frame;
  rec.timeout_ms = timeout_ms;
  node->tx_pending.push_back(rec);
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

// --- driver/uart.h ---

esp_err_t uart_driver_install(uart_port_t uart_num, int rx_buffer_size, int tx_buffer_size, int, QueueHandle_t *, int)
{
  if (fake_state::uart_install_result() != ESP_OK) {
    return fake_state::uart_install_result();
  }
  auto & u = fake_state::uart()[uart_num];
  u.installed = true;
  u.rx_buffer_size = rx_buffer_size;
  u.tx_buffer_size = tx_buffer_size;
  return ESP_OK;
}

esp_err_t uart_driver_delete(uart_port_t uart_num)
{
  auto & u = fake_state::uart()[uart_num];
  u.installed = false;
  u.delete_calls++;
  return ESP_OK;
}

bool uart_is_driver_installed(uart_port_t uart_num)
{
  return fake_state::uart()[uart_num].installed;
}

esp_err_t uart_param_config(uart_port_t uart_num, const uart_config_t * uart_config)
{
  fake_state::uart()[uart_num].config = *uart_config;
  return ESP_OK;
}

esp_err_t uart_set_pin(uart_port_t uart_num, int tx_io_num, int rx_io_num, int rts_io_num, int cts_io_num)
{
  auto & u = fake_state::uart()[uart_num];
  u.tx_pin = tx_io_num;
  u.rx_pin = rx_io_num;
  u.rts_pin = rts_io_num;
  u.cts_pin = cts_io_num;
  return ESP_OK;
}

int uart_write_bytes(uart_port_t uart_num, const void * src, size_t size)
{
  auto & u = fake_state::uart()[uart_num];
  if (!u.installed) {
    return -1;
  }
  const auto * p = static_cast<const uint8_t *>(src);
  u.written.insert(u.written.end(), p, p + size);
  return static_cast<int>(size);
}

int uart_read_bytes(uart_port_t uart_num, void * buf, uint32_t length, TickType_t ticks_to_wait)
{
  auto & u = fake_state::uart()[uart_num];
  if (!u.installed) {
    return -1;
  }
  u.last_read_timeout_ticks = ticks_to_wait;
  auto * p = static_cast<uint8_t *>(buf);
  uint32_t n = 0;
  while (n < length && !u.rx_pending.empty()) {
    p[n++] = u.rx_pending.front();
    u.rx_pending.pop_front();
  }
  return static_cast<int>(n);
}

esp_err_t uart_flush_input(uart_port_t uart_num)
{
  fake_state::uart()[uart_num].rx_pending.clear();
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

namespace
{

BaseType_t Push(QueueHandle_t queue, const void * item)
{
  auto * q = static_cast<fake_state::FakeQueue *>(queue);
  if (q->items.size() >= q->length) {
    return pdFALSE;
  }
  const auto * bytes = static_cast<const uint8_t *>(item);
  q->items.emplace_back(bytes, bytes + q->item_size);
  return pdTRUE;
}

}  // namespace

BaseType_t xQueueSend(QueueHandle_t queue, const void * item, TickType_t)
{
  return Push(queue, item);
}

BaseType_t xQueueSendFromISR(QueueHandle_t queue, const void * item, BaseType_t * higher_priority_task_woken)
{
  if (higher_priority_task_woken != nullptr) {
    *higher_priority_task_woken = pdFALSE;
  }
  return Push(queue, item);
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

UBaseType_t uxQueueMessagesWaiting(QueueHandle_t queue)
{
  return static_cast<UBaseType_t>(static_cast<fake_state::FakeQueue *>(queue)->items.size());
}

// --- freertos/timers.h ---

BaseType_t xTimerPendFunctionCallFromISR(
  PendedFunction_t function, void * arg1, uint32_t arg2, BaseType_t * higher_priority_task_woken)
{
  if (higher_priority_task_woken != nullptr) {
    *higher_priority_task_woken = pdFALSE;
  }
  if (!fake_state::pend_result()) {
    return pdFAIL;
  }
  fake_state::pended_calls().push_back({function, arg1, arg2});
  return pdPASS;
}
