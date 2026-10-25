#include "rabcl_esp32/half_duplex_serial.hpp"

#include <freertos/FreeRTOS.h>

namespace rabcl_esp32
{

HalfDuplexSerial::HalfDuplexSerial(const HalfDuplexSerialConfig & config)
: config_(config)
{
}

HalfDuplexSerial::~HalfDuplexSerial()
{
  if (started_) {
    Stop();
  }
}

esp_err_t HalfDuplexSerial::Start()
{
  if (started_) {
    return ESP_ERR_INVALID_STATE;
  }
  uart_config_t cfg = {};
  cfg.baud_rate = static_cast<int>(config_.baud);
  cfg.data_bits = UART_DATA_8_BITS;
  cfg.parity = UART_PARITY_DISABLE;
  cfg.stop_bits = UART_STOP_BITS_1;
  cfg.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
  cfg.source_clk = UART_SCLK_DEFAULT;

  esp_err_t err = uart_driver_install(
    config_.port, static_cast<int>(config_.rx_buffer), 0, 0, nullptr, 0);
  if (err != ESP_OK) {
    return err;
  }
  err = uart_param_config(config_.port, &cfg);
  if (err != ESP_OK) {
    uart_driver_delete(config_.port);
    return err;
  }
  err = uart_set_pin(config_.port, config_.tx, config_.rx, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
  if (err != ESP_OK) {
    uart_driver_delete(config_.port);
    return err;
  }
  started_ = true;
  return ESP_OK;
}

esp_err_t HalfDuplexSerial::Stop()
{
  if (!started_) {
    return ESP_ERR_INVALID_STATE;
  }
  started_ = false;
  return uart_driver_delete(config_.port);
}

int HalfDuplexSerial::Transfer(
  const uint8_t * tx, size_t tx_len, uint8_t * rx, size_t rx_len, uint32_t timeout_ms)
{
  if (!started_) {
    return -ESP_ERR_INVALID_STATE;
  }
  const TickType_t timeout = pdMS_TO_TICKS(timeout_ms);
  uart_flush_input(config_.port);  // 前回の残り（遅れて来た応答など）を捨てる
  const int written = uart_write_bytes(config_.port, tx, tx_len);
  if (written < 0 || static_cast<size_t>(written) != tx_len) {
    return -ESP_FAIL;
  }
  uart_wait_tx_done(config_.port, timeout);
  // 合流回路のエコーを送信バイト数分読み捨てる
  uint8_t echo[64];
  size_t skipped = 0;
  while (skipped < tx_len) {
    const size_t chunk = tx_len - skipped < sizeof(echo) ? tx_len - skipped : sizeof(echo);
    const int n = uart_read_bytes(config_.port, echo, static_cast<uint32_t>(chunk), timeout);
    if (n <= 0) {
      return -1;
    }
    skipped += static_cast<size_t>(n);
  }
  if (rx_len == 0) {
    return 0;
  }
  const int got = uart_read_bytes(config_.port, rx, static_cast<uint32_t>(rx_len), timeout);
  return got < 0 ? -1 : got;
}

}  // namespace rabcl_esp32
