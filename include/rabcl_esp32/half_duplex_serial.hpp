#ifndef RABCL_ESP32__HALF_DUPLEX_SERIAL_HPP_
#define RABCL_ESP32__HALF_DUPLEX_SERIAL_HPP_

#include <cstddef>
#include <cstdint>

#include <driver/gpio.h>
#include <driver/uart.h>
#include <esp_err.h>

namespace rabcl_esp32
{

struct HalfDuplexSerialConfig
{
  uart_port_t port = UART_NUM_1;
  gpio_num_t tx = GPIO_NUM_NC;
  gpio_num_t rx = GPIO_NUM_NC;
  uint32_t baud = 1000000;
  size_t rx_buffer = 256;  // ドライバの受信リングバッファ [B]
};

// 半二重バス（ST3215 などのバスサーボ）用の UART。TX と RX が 1 本の Data 線に抵抗で合流していて、
// 送ったバイトはそのまま RX に戻る（エコー）。Transfer は「送る → エコーを送信バイト数分読み捨てる →
// 応答を待つ」を 1 回分まとめる。送信リングバッファは 0（書き終わるまでブロック）。rabcl には依存しない
class HalfDuplexSerial
{
public:
  explicit HalfDuplexSerial(const HalfDuplexSerialConfig & config);
  ~HalfDuplexSerial();
  HalfDuplexSerial(const HalfDuplexSerial &) = delete;
  HalfDuplexSerial & operator=(const HalfDuplexSerial &) = delete;

  esp_err_t Start();  // 2 回目は ESP_ERR_INVALID_STATE
  esp_err_t Stop();
  // 受信を捨て、tx を送り、エコーを読み捨て、rx_len バイトを timeout_ms まで待つ。
  // 返り値 = 受けた応答のバイト数（timeout なら少ない数）。エコーが揃わなければ -1、未 Start は -ESP_ERR_INVALID_STATE
  int Transfer(const uint8_t * tx, size_t tx_len, uint8_t * rx, size_t rx_len, uint32_t timeout_ms);

private:
  HalfDuplexSerialConfig config_;
  bool started_ = false;
};

}  // namespace rabcl_esp32

#endif  // RABCL_ESP32__HALF_DUPLEX_SERIAL_HPP_
