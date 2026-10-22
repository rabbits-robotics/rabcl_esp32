#include "rabcl_esp32/serial_port.hpp"

namespace rabcl_esp32
{

SerialPort::SerialPort(const SerialPortConfig & config) : config_(config) {}

SerialPort::~SerialPort()
{
  if (started_) {
    Stop();
  }
}

esp_err_t SerialPort::Start()
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
    config_.port, static_cast<int>(config_.rx_buffer), static_cast<int>(config_.tx_buffer),
    0, nullptr, 0);
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

esp_err_t SerialPort::Stop()
{
  if (!started_) {
    return ESP_ERR_INVALID_STATE;
  }
  started_ = false;
  return uart_driver_delete(config_.port);
}

int SerialPort::Read(uint8_t * buf, size_t len, TickType_t timeout)
{
  if (!started_) {
    return -ESP_ERR_INVALID_STATE;
  }
  return uart_read_bytes(config_.port, buf, static_cast<uint32_t>(len), timeout);
}

int SerialPort::Write(const uint8_t * buf, size_t len)
{
  if (!started_) {
    return -ESP_ERR_INVALID_STATE;
  }
  return uart_write_bytes(config_.port, buf, len);
}

}  // namespace rabcl_esp32
