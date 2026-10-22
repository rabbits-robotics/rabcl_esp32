#include "rabcl_esp32/can_bus.hpp"

#include <cstring>

#include <esp_attr.h>
#include <esp_twai_onchip.h>
#include <freertos/timers.h>

namespace rabcl_esp32
{

namespace
{

constexpr uint32_t MAX_TX_SLOTS = 255;  // スロット番号を uint8_t で運ぶため

}  // namespace

CanBus::CanBus(const CanBusConfig & config) : config_(config) {}

CanBus::~CanBus()
{
  if (started_) {
    Stop();
  }
  if (rx_queue_ != nullptr) {
    vQueueDelete(rx_queue_);
    rx_queue_ = nullptr;
  }
  if (free_tx_slots_ != nullptr) {
    vQueueDelete(free_tx_slots_);
    free_tx_slots_ = nullptr;
  }
}

esp_err_t CanBus::InitTxSlots()
{
  // ドライバのキュー（tx_queue_depth）+ 送信中の 1 枚
  uint32_t count = config_.tx_queue_depth + 1;
  if (count > MAX_TX_SLOTS) {
    count = MAX_TX_SLOTS;
  }
  if (tx_slots_ == nullptr || tx_slot_count_ != count) {
    tx_slots_.reset(new (std::nothrow) TxSlot[count]);
    if (tx_slots_ == nullptr) {
      return ESP_ERR_NO_MEM;
    }
    tx_slot_count_ = count;
    if (free_tx_slots_ != nullptr) {
      vQueueDelete(free_tx_slots_);
      free_tx_slots_ = nullptr;
    }
  }
  if (free_tx_slots_ == nullptr) {
    free_tx_slots_ = xQueueCreate(tx_slot_count_, sizeof(uint8_t));
    if (free_tx_slots_ == nullptr) {
      return ESP_ERR_NO_MEM;
    }
  }
  // Stop() でドライバに残っていたフレームは捨てられるので、空きを全部入れ直す
  uint8_t dummy;
  while (xQueueReceive(free_tx_slots_, &dummy, 0) == pdTRUE) {
  }
  for (uint32_t i = 0; i < tx_slot_count_; ++i) {
    const uint8_t idx = static_cast<uint8_t>(i);
    xQueueSend(free_tx_slots_, &idx, 0);
  }
  return ESP_OK;
}

esp_err_t CanBus::Start()
{
  if (started_) {
    return ESP_ERR_INVALID_STATE;
  }
  if (rx_queue_ == nullptr) {
    rx_queue_ = xQueueCreate(config_.rx_queue_depth, sizeof(CanFrame));
    if (rx_queue_ == nullptr) {
      return ESP_ERR_NO_MEM;
    }
  }
  esp_err_t err = InitTxSlots();
  if (err != ESP_OK) {
    return err;
  }

  twai_onchip_node_config_t cfg = {};
  cfg.io_cfg.tx = config_.tx;
  cfg.io_cfg.rx = config_.rx;
  cfg.io_cfg.quanta_clk_out = GPIO_NUM_NC;
  cfg.io_cfg.bus_off_indicator = GPIO_NUM_NC;
  cfg.bit_timing.bitrate = config_.bitrate;
  cfg.fail_retry_cnt = config_.fail_retry_cnt;
  cfg.tx_queue_depth = config_.tx_queue_depth;
  cfg.timestamp_resolution_hz = 1000000;
  err = twai_new_node_onchip(&cfg, &node_);
  if (err != ESP_OK) {
    return err;
  }

  twai_event_callbacks_t cbs = {};
  cbs.on_rx_done = OnRxDone;
  cbs.on_tx_done = OnTxDone;
  cbs.on_error = OnError;
  cbs.on_state_change = OnStateChange;
  err = twai_node_register_event_callbacks(node_, &cbs, this);
  if (err != ESP_OK) {
    twai_node_delete(node_);
    node_ = nullptr;
    return err;
  }
  err = twai_node_enable(node_);
  if (err != ESP_OK) {
    twai_node_delete(node_);
    node_ = nullptr;
    return err;
  }
  recover_requested_ = false;
  started_ = true;
  return ESP_OK;
}

esp_err_t CanBus::Stop()
{
  if (!started_) {
    return ESP_ERR_INVALID_STATE;
  }
  twai_node_disable(node_);
  esp_err_t err = twai_node_delete(node_);
  node_ = nullptr;
  started_ = false;
  return err;
}

esp_err_t CanBus::Send(const CanFrame & frame, TickType_t timeout)
{
  if (frame.dlc > sizeof(frame.data)) {
    return ESP_ERR_INVALID_ARG;
  }
  if (!started_) {
    return ESP_ERR_INVALID_STATE;
  }
  MaybeRecover();

  uint8_t idx = 0;
  if (xQueueReceive(free_tx_slots_, &idx, timeout) != pdTRUE) {
    return ESP_ERR_TIMEOUT;
  }
  TxSlot & slot = tx_slots_[idx];
  std::memcpy(slot.data, frame.data, sizeof(slot.data));
  slot.frame = {};
  slot.frame.header.id = frame.id;
  slot.frame.header.dlc = frame.dlc;
  slot.frame.header.ide = frame.ext ? 1 : 0;
  slot.frame.header.rtr = frame.rtr ? 1 : 0;
  slot.frame.buffer = slot.data;
  slot.frame.buffer_len = frame.dlc;

  const int timeout_ms = (timeout == portMAX_DELAY) ? -1 : static_cast<int>(pdTICKS_TO_MS(timeout));
  esp_err_t err = twai_node_transmit(node_, &slot.frame, timeout_ms);
  if (err != ESP_OK) {
    xQueueSend(free_tx_slots_, &idx, 0);  // 積めなかったスロットは返す
    return err;
  }
  return ESP_OK;
}

bool CanBus::Receive(CanFrame & frame, TickType_t timeout)
{
  if (!started_) {
    return false;
  }
  MaybeRecover();
  return xQueueReceive(rx_queue_, &frame, timeout) == pdTRUE;
}

CanBusCounters CanBus::GetCounters() const
{
  CanBusCounters c;
  c.tx_fail = tx_fail_;
  c.bus_error = bus_error_;
  c.rx_dropped = rx_dropped_;
  c.bus_off = bus_off_;
  c.recovered = recovered_;
  return c;
}

esp_err_t CanBus::GetStatus(twai_node_status_t & status) const
{
  if (!started_) {
    return ESP_ERR_INVALID_STATE;
  }
  return twai_node_get_info(node_, &status, nullptr);
}

esp_err_t CanBus::Recover()
{
  if (!started_) {
    return ESP_ERR_INVALID_STATE;
  }
  recover_requested_ = false;
  return twai_node_recover(node_);
}

void CanBus::MaybeRecover()
{
  if (recover_requested_ && config_.auto_recover) {
    Recover();
  }
}

void CanBus::RecoverDeferred(void * arg, uint32_t)
{
  auto * self = static_cast<CanBus *>(arg);
  self->Recover();
}

// --- ISR コールバック。ブロックする API とログは使わない ---

bool IRAM_ATTR CanBus::OnRxDone(twai_node_handle_t node, const twai_rx_done_event_data_t *, void * ctx)
{
  auto * self = static_cast<CanBus *>(ctx);
  CanFrame frame;
  twai_frame_t rx = {};
  rx.buffer = frame.data;
  rx.buffer_len = sizeof(frame.data);
  if (twai_node_receive_from_isr(node, &rx) != ESP_OK) {
    return false;
  }
  frame.id = rx.header.id;
  frame.dlc = static_cast<uint8_t>(rx.header.dlc);
  frame.ext = rx.header.ide != 0;
  frame.rtr = rx.header.rtr != 0;
  frame.timestamp_us = rx.header.timestamp;
  BaseType_t woken = pdFALSE;
  if (xQueueSendFromISR(self->rx_queue_, &frame, &woken) != pdTRUE) {
    self->rx_dropped_ = self->rx_dropped_ + 1;
  }
  return woken == pdTRUE;
}

bool IRAM_ATTR CanBus::OnTxDone(twai_node_handle_t, const twai_tx_done_event_data_t * edata, void * ctx)
{
  auto * self = static_cast<CanBus *>(ctx);
  if (!edata->is_tx_success) {
    self->tx_fail_ = self->tx_fail_ + 1;
  }
  // frame は TxSlot の先頭メンバなので、ポインタからスロット番号が引ける
  const auto * slot = reinterpret_cast<const TxSlot *>(edata->done_tx_frame);
  const TxSlot * base = self->tx_slots_.get();
  BaseType_t woken = pdFALSE;
  if (slot >= base && slot < base + self->tx_slot_count_) {
    const uint8_t idx = static_cast<uint8_t>(slot - base);
    xQueueSendFromISR(self->free_tx_slots_, &idx, &woken);
  }
  return woken == pdTRUE;
}

bool IRAM_ATTR CanBus::OnError(twai_node_handle_t, const twai_error_event_data_t *, void * ctx)
{
  auto * self = static_cast<CanBus *>(ctx);
  self->bus_error_ = self->bus_error_ + 1;
  return false;
}

bool IRAM_ATTR CanBus::OnStateChange(
  twai_node_handle_t, const twai_state_change_event_data_t * edata, void * ctx)
{
  auto * self = static_cast<CanBus *>(ctx);
  BaseType_t woken = pdFALSE;
  if (edata->new_sta == TWAI_ERROR_BUS_OFF) {
    self->bus_off_ = self->bus_off_ + 1;
    if (self->config_.auto_recover) {
      // ISR からはドライバを呼べないので、タイマータスクに回復を頼む。
      // タイマーのコマンドキューが一杯なら、次の Send / Receive で回復する
      if (xTimerPendFunctionCallFromISR(RecoverDeferred, self, 0, &woken) != pdPASS) {
        self->recover_requested_ = true;
      }
    }
  } else if (edata->old_sta == TWAI_ERROR_BUS_OFF && edata->new_sta == TWAI_ERROR_ACTIVE) {
    self->recovered_ = self->recovered_ + 1;
  }
  return woken == pdTRUE;
}

}  // namespace rabcl_esp32
