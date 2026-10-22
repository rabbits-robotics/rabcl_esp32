#ifndef RABCL_ESP32__SERIAL_PORT_HPP_
#define RABCL_ESP32__SERIAL_PORT_HPP_

#include <cstddef>
#include <cstdint>

#include <driver/gpio.h>
#include <driver/uart.h>
#include <esp_err.h>
#include <freertos/FreeRTOS.h>

namespace rabcl_esp32
{

struct SerialPortConfig
{
  uart_port_t port = UART_NUM_1;
  gpio_num_t tx = GPIO_NUM_NC;
  gpio_num_t rx = GPIO_NUM_NC;
  uint32_t baud = 115200;
  size_t rx_buffer = 1024;  // ドライバの受信リングバッファ [B]
  size_t tx_buffer = 1024;  // 送信リングバッファ [B]（0 だと Write が送り終わるまでブロック）
};

// UART 1 ポート分（8N1、フロー制御なし）。受信はドライバのリングバッファに溜まり、Read で取り出す。
// PC（rabcl::protocol）や半二重でない機器との通信に使う。
class SerialPort
{
public:
  explicit SerialPort(const SerialPortConfig & config);
  ~SerialPort();
  SerialPort(const SerialPort &) = delete;
  SerialPort & operator=(const SerialPort &) = delete;

  esp_err_t Start();  // 2 回目は ESP_ERR_INVALID_STATE
  esp_err_t Stop();
  // 読めた分（timeout 内に 1 バイトも無ければ 0）を返す。負の値は -esp_err_t
  int Read(uint8_t * buf, size_t len, TickType_t timeout);
  // 送信リングバッファに積んだ数を返す。負の値は -esp_err_t
  int Write(const uint8_t * buf, size_t len);

private:
  SerialPortConfig config_;
  bool started_ = false;
};

}  // namespace rabcl_esp32

#endif  // RABCL_ESP32__SERIAL_PORT_HPP_
