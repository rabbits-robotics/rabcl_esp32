// fake の内部状態とテスト用の操作。テストからコールバックを起こし、呼び出しの記録を読む。
#ifndef FAKES__FAKE_STATE_HPP_
#define FAKES__FAKE_STATE_HPP_

#include <cstdint>
#include <deque>
#include <map>
#include <memory>
#include <vector>

#include "driver/gpio.h"
#include "esp_twai.h"
#include "esp_twai_onchip.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

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

  struct TxRecord
  {
    uint32_t id;
    uint16_t dlc;
    bool ide;
    bool rtr;
    std::vector<uint8_t> data;
    int timeout_ms;
  };
  std::vector<TxRecord> tx;

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
  void FireRx(const RxFrame & frame);                                   // on_rx_done（受信フレームを 1 つ積んでから呼ぶ）
  void FireTxDone(bool success);                                        // on_tx_done
  void FireError(uint32_t flags);                                       // on_error
  void FireStateChange(twai_error_state_t from, twai_error_state_t to); // on_state_change
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

// キュー
struct FakeQueue
{
  UBaseType_t length;
  UBaseType_t item_size;
  std::deque<std::vector<uint8_t>> items;
};
std::vector<std::unique_ptr<FakeQueue>> & queues();

}  // namespace fake_state

#endif  // FAKES__FAKE_STATE_HPP_
