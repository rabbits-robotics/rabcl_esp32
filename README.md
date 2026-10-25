# rabcl_esp32
[![CI](https://github.com/rabbits-robotics/rabcl_esp32/actions/workflows/ci.yml/badge.svg)](https://github.com/rabbits-robotics/rabcl_esp32/actions/workflows/ci.yml)

Rabbits Common Library for ESP32。ESP-IDF の周辺機器ドライバ（TWAI / GPIO / UART、今後 LEDC）を包んで、各 ESP32 ファームウェアから同じ形で使えるようにする ESP-IDF コンポーネント。

[rabcl](https://github.com/rabbits-robotics/rabcl)（プロトコルと制御、プラットフォーム非依存）とは独立で、rabcl が作る CAN ペイロード（`uint8_t[8]`）をそのまま `CanBus::Send` に渡せる。

## 置くもの

| 内容 | 置き場 |
|---|---|
| ヘッダ | `include/rabcl_esp32/` |
| 実装 | `src/` |
| 動作確認用のプロジェクト | `examples/` |
| 設計判断の記録 | `docs/` |

## クラス

| クラス | 包むもの | 役割 |
|---|---|---|
| `CanBus` | esp_driver_twai | TWAI コントローラ 1 つ。`Start()` / `Send()` / `Receive()` / `GetCounters()` / `GetStatus()`。`Send()` はフレームを内部のスロットにコピーしてドライバに積んだ時点で戻る（完了は待たない）。受信は ISR からキューに積む。バスオフは既定で FreeRTOS のタイマータスクから自動回復する |
| `DebugLed` | esp_driver_gpio | LED 1 個。吸い込み / 吐き出しを設定で選ぶ |
| `SerialPort` | esp_driver_uart | UART 1 ポート（8N1、フロー制御なし）。`Start()` / `Read(buf, len, timeout)` / `Write(buf, len)`。受信はドライバのリングバッファに溜まる。PC との通信（rabcl の `protocol/`）に使う |
| `HalfDuplexSerial` | esp_driver_uart | 半二重バス（ST3215 などのバスサーボ）用。TX / RX が抵抗で 1 本の Data 線に合流する配線向けに、`Transfer(tx, tx_len, rx, rx_len, timeout_ms)` が「受信を捨てる → 送る → エコーを送信バイト数分読み捨てる → 応答を待つ」を 1 回で行う。rabcl には依存しない（`St3215Port` への適合は機体側） |

共通の決まり:

- コンストラクタはハードを触らない。`Start()` で初期化し `esp_err_t` を返す
- ISR コールバックはキューとカウンタだけ更新し、ログは呼び出し側に任せる
- 機体固有の値（ピン、CAN ID、ゲイン）は持たない。各ファームの `board.hpp` に置く

## 使い方

ファームのリポジトリで `components/rabcl_esp32` に submodule として入れる。

```bash
git submodule add https://github.com/rabbits-robotics/rabcl_esp32.git components/rabcl_esp32
```

`src/CMakeLists.txt` の `REQUIRES` に `rabcl_esp32` を足す。

```cpp
#include "rabcl_esp32/can_bus.hpp"

rabcl_esp32::CanBusConfig cfg;
cfg.tx = GPIO_NUM_23;
cfg.rx = GPIO_NUM_21;
cfg.bitrate = 1000000;
rabcl_esp32::CanBus can(cfg);
ESP_ERROR_CHECK(can.Start());

rabcl_esp32::CanFrame f;
f.id = 0x101;
f.dlc = 8;
can.Send(f, pdMS_TO_TICKS(100));  // 送信スロットが 100 ms 空かなければ ESP_ERR_TIMEOUT
while (can.Receive(f, 0)) { /* ... */ }
```

## 対応環境

- ESP-IDF 5.5.5（PlatformIO + [pioarduino](https://github.com/pioarduino/platform-espressif32) 55.03.312、framework = espidf）
- 動作確認は ESP32-P4（M5Stack Stamp-P4 v1.3、gamma_turret_pcb）で行っている

## テスト

rabcl 系のリポジトリはユニットテストを必ず書き、CI で実行する（handbook の [common-lib.md](https://github.com/rabbits-robotics/.github/blob/main/docs/common-lib.md)）。新しいクラスを足すときはテストとセットにする。

### ユニットテスト（実機なし）

`test/` に gtest で置いている。`test/fakes/` にある ESP-IDF / FreeRTOS の偽ヘッダ（`esp_twai.h`、`driver/gpio.h`、`driver/uart.h`、`freertos/queue.h` など）をインクルードパスの先頭にして、ライブラリ本体をそのままビルドする。偽物は呼び出しを記録し、テストから ISR コールバック（受信、送信完了、エラー、状態変化）を起こせるので、カウンタやバスオフからの自動回復のような論理を確認できる。

```bash
cmake -S test -B build
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Windows では Docker（rabbits_ws の Dev Container イメージ）か WSL で実行する。CI でも同じコマンドを回している。

### 実機での動作確認

`examples/can_loop` は CAN1 と CAN2 を直結して互いに送受信する試験。`examples/can_loop` で `pio run -t upload` して書き込む。本物のドライバと配線を通した確認はこちらで行う。

## ライセンス

MIT。[LICENSE](./LICENSE) を参照。
