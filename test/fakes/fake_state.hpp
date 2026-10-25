// fake の内部状態とテスト用の操作。テストからコールバックを起こし、呼び出しの記録を読む。
#ifndef FAKES__FAKE_STATE_HPP_
#define FAKES__FAKE_STATE_HPP_

#include <cstdint>
#include <deque>
#include <map>
#include <memory>
#include <vector>

#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_twai.h"
#include "esp_twai_onchip.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/timers.h"

// twai_node_handle_t の実体
struct twai_node_t
{
  twai_onchip_node_config_t config = {};
  twai_event_callbacks_t callbacks = {};
  void * user_ctx = nullptr;
  bool enabled = false;
  bool deleted = false;
  int recover_calls = 0;
  int wait_all_done_calls = 0;
  esp_err_t transmit_result = ESP_OK;  // テストから失敗を仕込む
  twai_node_status_t status = {};
  twai_node_record_t record = {};

  // 本物のドライバと同じく、transmit はフレームをコピーせずポインタを保持する。
  // 送信完了（FireTxDone）の時点でポインタ越しに読んだ内容を記録する
  struct TxRecord
  {
    const twai_frame_t * frame;  // transmit に渡されたポインタ
    int timeout_ms;
    // FireTxDone の時点で読んだ内容
    uint32_t id;
    uint16_t dlc;
    bool ide;
    bool rtr;
    std::vector<uint8_t> data;
  };
  std::deque<TxRecord> tx_pending;  // transmit 済みでまだ完了していない順
  std::vector<TxRecord> tx_done;    // FireTxDone で完了した順

  struct RxFrame
  {
    uint32_t id;
    uint16_t dlc;
    bool ide;
    bool rtr;
    uint64_t timestamp;
    std::vector<uint8_t> data;
  };
  std::deque<RxFrame> rx_pending;  // receive_from_isr が返すフレーム

  // --- テストからコールバックを起こす ---
  void FireRx(const RxFrame & frame);                                    // on_rx_done（受信フレームを 1 つ積んでから呼ぶ）
  void FireTxDone(bool success);                                         // on_tx_done（tx_pending の先頭を完了にする）
  void FireError(uint32_t flags);                                        // on_error
  void FireStateChange(twai_error_state_t from, twai_error_state_t to);  // on_state_change
};

namespace fake_state
{

void Reset();  // 全記録を消す。各テストの最初に呼ぶ

// TWAI
std::vector<twai_node_t *> & twai_nodes();  // twai_new_node_onchip で作られた順
esp_err_t & twai_new_node_result();         // twai_new_node_onchip の戻り値（既定 ESP_OK）

// GPIO
struct GpioRecord
{
  gpio_config_t config = {};
  std::vector<uint32_t> levels;  // gpio_set_level の履歴
};
std::map<int, GpioRecord> & gpio();

// UART（ポート番号ごと）
struct UartRecord
{
  bool installed = false;
  int delete_calls = 0;
  int rx_buffer_size = 0;
  int tx_buffer_size = 0;
  uart_config_t config = {};
  int tx_pin = UART_PIN_NO_CHANGE;
  int rx_pin = UART_PIN_NO_CHANGE;
  int rts_pin = UART_PIN_NO_CHANGE;
  int cts_pin = UART_PIN_NO_CHANGE;
  std::vector<uint8_t> written;    // uart_write_bytes で書かれたバイト列（累積）
  std::deque<uint8_t> rx_pending;  // uart_read_bytes が返すバイト列（テストが仕込む）
  std::deque<uint8_t> rx_after_write;  // 次の uart_write_bytes のあとに rx_pending へ移るバイト列（半二重のエコー + 応答）
  TickType_t last_read_timeout_ticks = 0;
  int flush_calls = 0;         // uart_flush_input の回数
  int wait_tx_done_calls = 0;  // uart_wait_tx_done の回数
};
std::map<int, UartRecord> & uart();
esp_err_t & uart_install_result();  // uart_driver_install の戻り値（既定 ESP_OK）

// キュー
struct FakeQueue
{
  UBaseType_t length;
  UBaseType_t item_size;
  std::deque<std::vector<uint8_t>> items;
};
std::vector<std::unique_ptr<FakeQueue>> & queues();

// タイマータスクへの遅延呼び出し（xTimerPendFunctionCallFromISR）
struct PendedCall
{
  PendedFunction_t function;
  void * arg1;
  uint32_t arg2;
};
std::deque<PendedCall> & pended_calls();
int RunPendedFunctions();  // 積まれた関数を順に実行し、実行した数を返す
bool & pend_result();      // xTimerPendFunctionCallFromISR の戻り値（既定 true = pdPASS）

}  // namespace fake_state

#endif  // FAKES__FAKE_STATE_HPP_
