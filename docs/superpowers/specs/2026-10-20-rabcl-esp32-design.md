# rabcl_esp32 設計ドキュメント

- 日付: 2026-10-20
- 対象: 新リポジトリ `rabbits-robotics/rabcl_esp32`（Rabbits Common Library for ESP32）と、それを使う `gamma_turret_fw` の構成
- 前段: [2026-10-18-org-management-design.md](https://github.com/rabbits-robotics/.github/blob/main/docs/superpowers/specs/2026-10-18-org-management-design.md) 4.4 / 4.6

---

## 1. 目的

ESP32 系マイコンのファームウェアを複数作る見込み（別機体の基板、同じ基板の別用途）がある。
ESP-IDF の周辺機器ドライバ（TWAI / UART / LEDC / GPIO）を扱う部分を 1 か所にまとめ、各ファームには「基板のピン割り当て」と「その機体の制御」だけが残るようにする。

rabcl（プロトコルと制御、プラットフォーム非依存）と各ファームの間に ESP32 向けの層を 1 つ足す。rabcl_ros2 が rabcl の ROS 2 向けラッパーであるのと同じ位置づけ。

```
rabcl         CAN ペイロード・UART パケット・制御器。<cstdint> 以外に依存しない（不変）
  ▲
rabcl_esp32   ESP-IDF のドライバを包む。CanBus / SerialPort / PwmOut / DebugLed …
  ▲
各ファーム     board.hpp（ピンと定数）、タスク（制御・通信）、app_main
```

## 2. ゴール・非ゴール

ゴール:

- gamma_turret_fw の main.cpp から ESP-IDF 依存のコードが消え、`board.hpp` と制御・通信のタスクだけになる
- 2 枚目の ESP32 基板のファームが、rabcl_esp32 を submodule で入れて `board.hpp` を書けば動き始められる
- 受信キュー、エラー処理、初期化の決まりごとが rabcl_esp32 側で一度だけ決まる

非ゴール:

- STM32 向けの同等層（beta_main は現状維持）
- ESP32 以外への移植性（Arduino フレームワーク対応を含む）
- 実機を使うテストの自動化（実機の確認は `examples/` を焼いて手動で行う）

ホスト PC でのユニットテストは行う（2026-10-22 に追加）: ESP-IDF と FreeRTOS の API を `test/fakes/` の偽物に差し替え、各クラスの論理（引数の検査、状態遷移、カウンタ、キュー、自動回復）を gtest で検証する。CI でも実行する

やってはいけないこと:

- rabcl に ESP-IDF 依存のコードを入れる（CMake の分岐は可）
- rabcl_esp32 に機体固有の値（CAN ID、ピン番号、ゲイン）を入れる

## 3. 制約

- ESP-IDF 5.5.5（pioarduino 55.03.312）。TWAI は新ドライバ `esp_driver_twai`（`twai_new_node_onchip` 系）を使う。旧 `driver/twai.h` は使わない
- ESP32-P4 v1.3 で動作確認する（gamma_turret_pcb v1.0）。他の ESP32 系でも動く設計にするが、確認は P4 のみ
- FreeRTOS 前提（ESP-IDF に同梱）。`CONFIG_FREERTOS_UNICORE=y` でも動くこと
- C++17。定数は UPPER_SNAKE_CASE、クラス・関数は UpperCamelCase（rabcl と同じ）
- ライセンス MIT、著作権表示 `Rabbits Robotics`

## 4. 決定事項

### 4.1 リポジトリ

- 名前: `rabcl_esp32`。種類は共通ライブラリ（設計書 4.2 の rabcl と同じ扱い）。命名は `rabcl_ros2` に倣う
- 形: ESP-IDF コンポーネント。ルートに `CMakeLists.txt`（`idf_component_register`）を置く
- 各ファームからは `components/rabcl_esp32` に git submodule で入れる
- ローカルの置き場: `rabbits-robotics/fw_ws/rabcl_esp32/`（ファーム側のライブラリなので fw_ws）
- 初回コミットは「`# rabcl_esp32` の 1 行 README」。以後のコミットは 1 行メッセージ

構成:

```
rabcl_esp32/
├── CMakeLists.txt                 idf_component_register(SRCS … INCLUDE_DIRS include REQUIRES esp_driver_twai esp_driver_uart esp_driver_ledc esp_driver_gpio)
├── include/rabcl_esp32/
│   ├── can_bus.hpp
│   ├── serial_port.hpp
│   ├── pwm_out.hpp
│   └── debug_led.hpp
├── src/
├── examples/can_loop/             CAN1 ⇔ CAN2 直結試験（gamma_turret_fw から移す）。PlatformIO プロジェクト。CI のビルド対象
│   ├── platformio.ini             pioarduino、board = boards/m5stack-stamp-p4.json、components に ../../ を指定
│   ├── sdkconfig.defaults         Stamp-P4 v1.3 で動く設定（rev 3 未満、UNICORE、16MB、USB-Serial-JTAG コンソール）
│   └── src/main.cpp               例としてのピン割り当て（gamma_turret の G23/G21/G19/G17、LED G38）は main.cpp 冒頭の定数に置く
├── docs/superpowers/specs/        本ドキュメント
├── .github/workflows/ci.yml       PlatformIO（pioarduino Core）で examples/ をビルド
├── LICENSE（MIT）
└── README.md
```

rabcl_esp32 自体は rabcl に依存しない（CanBus / PwmOut / DebugLed は rabcl のヘッダを使わず、SerialPort のパケット長は設定値で渡す）。
rabcl が作る CAN ペイロードは `uint8_t[8]` なので、そのまま `CanBus::Send` に渡せる。
名前は「Rabbits の共通ライブラリ群の ESP32 向け」という意味で、rabcl_ros2 と同じ命名に揃えている。

将来、rabcl と周辺機器の両方を知る部品（モータへの周期送信のまとめ、rabcl::Uart パケットの送受信など）が欲しくなったら、
機体をまたいで使うものは rabcl_esp32 内の任意の層（例: `bridge/`、その部分だけ `REQUIRES rabcl`）として足し、機体固有のものはファーム側に置く。

### 4.2 rabcl の取り込み方（ファーム側）

rabcl は ament（ROS 2）向けの CMakeLists を持ち、そのままでは ESP-IDF のコンポーネントにならない。
ファームが rabcl を `components/rabcl` として使えるよう、rabcl の `CMakeLists.txt` 先頭に ESP-IDF 用の分岐を足す（コードは変えない）:

```cmake
if(ESP_PLATFORM)
  idf_component_register(
    SRCS src/component/bno055.cpp … src/utils/utils.cpp
    INCLUDE_DIRS include)
  return()
endif()
```

これで各ファームは `components/rabcl` と `components/rabcl_esp32` の 2 つを submodule で持つ。
rabcl のコミット固定はファーム側の submodule で行う（設計書 4.6: ファームの submodule と `.repos` が同じコミットを指す運用を維持する）。rabcl_esp32 は rabcl を内包しない。

### 4.3 rabcl_esp32 の API

名前空間は `rabcl_esp32`（rabcl_ros2 と同じ流儀）。すべてクラスで、設定は構造体で渡す。

| クラス | 包むもの | 役割 |
|---|---|---|
| `CanBus` | esp_driver_twai | 1 つの TWAI コントローラ。`Start()`、`Send(frame, timeout)`、`Receive(frame, timeout)`、`GetStatus()`。受信は ISR コールバックから FreeRTOS キューに積み、タスク側で取り出す。送信失敗・バスエラー・状態変化はカウンタに記録し、ログは呼び出し側に任せる。バスオフになったら既定で自動回復する（設定 `auto_recover` で無効化可）。回復回数もカウンタで公開し、ファーム側が「回復した」ことを知れるようにする |
| `SerialPort` | esp_driver_uart | 上位コントローラ（Jetson）との UART。固定長パケット（長さは設定値。gamma では rabcl::Uart の 28B / 112B）の受信をイベント駆動で行い、`Read(buf, len, timeout)` / `Write(buf, len)` を提供する |
| `HalfDuplexSerial` | esp_driver_uart | ST3215 など半二重バス用。送信中は受信を捨て、送信完了後に応答を待つ（後回し。gamma で装填を実装する段階で足す） |
| `PwmOut` | esp_driver_ledc | ESC / サーボ向けの 50 Hz パルス。`SetPulseUs(us)` |
| `DebugLed` | esp_driver_gpio | 吸い込み / 吐き出しを設定で選べる LED。`On()` / `Off()` / `Toggle()` |

共通の決まり:

- コンストラクタはハードを触らない。`Start()`（または `Init()`）で初期化し、`esp_err_t` を返す。`ESP_ERROR_CHECK` するかは呼び出し側が決める
- ISR コールバックは `IRAM_ATTR`。ISR の中ではログを出さず、キューとカウンタだけ更新する
- フレーム・パケットの型は rabcl_esp32 側で定義する（`CanFrame { id, dlc, data[8], ext, rtr, timestamp }`）。rabcl の `Prepare*` / `UpdateData` に渡すのは `data[8]` なので、変換は不要

### 4.4 各ファーム側の構成（gamma_turret_fw）

```
gamma_turret_fw/
├── components/
│   ├── rabcl/            submodule
│   └── rabcl_esp32/      submodule
├── src/
│   ├── CMakeLists.txt    REQUIRES rabcl rabcl_esp32
│   ├── board.hpp         gamma_turret のピン割り当て、CAN ビットレート、CAN ID、制御周期
│   ├── main.cpp          app_main: 周辺機器の生成と Start、タスク起動
│   └── tasks/            制御タスク、通信タスク（実装が進んでから）
├── sdkconfig.defaults
└── platformio.ini
```

- `board.hpp` は定数だけ。機体ごとに違うものはすべてここに集める
- `main.cpp` は配線（オブジェクトの生成・接続）だけを書き、ロジックはタスクのファイルに置く
- いまの CAN1 ⇔ CAN2 直結試験は gamma_turret_fw には残さず、rabcl_esp32 の `examples/can_loop` に移す。基板の疎通を確認したいときは example を焼く。gamma_turret_fw の main.cpp は、周辺機器を Start してデバッグ LED を点滅させるだけの骨組みから育てる

### 4.5 テストと CI

- ユニットテスト: `test/` に gtest で置く。`test/fakes/` に ESP-IDF / FreeRTOS の偽ヘッダ（`esp_twai.h`、`driver/gpio.h`、`freertos/queue.h` など）を置き、インクルードパスの先頭にしてライブラリ本体をそのままビルドする。偽物は呼び出しを記録し、テストから ISR コールバックを起こせる。ローカルは Docker（`rabbits/ros2-jazzy-dev`）か Linux で `cmake -S test -B build && cmake --build build && ctest --test-dir build`
- rabcl_esp32 の CI は GitHub Actions（ubuntu）で、ユニットテストの実行と、pioarduino 版 PlatformIO Core を pip で入れて `examples/` を `pio run` する 2 ジョブ。実機テストは手動
- gamma_turret_fw の CI も同じ形（別途）
- ローカルでの example の書き込みも PlatformIO で行う（`idf.py` は使わない）

### 4.6 進め方

実際の順（2026-10-22 実施。クラスはまず gamma_turret_fw 内で切り出して実機確認し、動いたものを移した）:

| 順 | 作業 | 確認 |
|---|---|---|
| 1 | gamma_turret_fw 内で `CanBus` / `DebugLed` / `board.hpp` に切り出す | 直結試験が前と同じ結果。受信キュー溢れ、dlc > 8、Start の二重呼び出し、相手不在（error_passive）と復帰を実機で確認 |
| 2 | rabcl に ESP-IDF 用の CMake 分岐を追加 | ROS 2 の colcon build / test が通ること |
| 3 | `rabcl_esp32` を作成してクラスを移す。`examples/can_loop`、README、LICENSE、CI | example を Stamp-P4 に焼いて直結試験が同じ結果。CI が通ること |
| 4 | gamma_turret_fw に 2 つの submodule を追加し、`board.hpp` + 骨組みの `main.cpp` にする | ビルドが通り、LED が点滅し、rabcl の関数がリンクされること |
| 5 | `SerialPort` / `PwmOut` は Jetson UART と ESC を実装する段階で足す | |

## 5. 実装者に任せる範囲

- 各クラスの設定構造体のメンバ名と既定値
- 受信キューの深さの既定値（CanBus は 16 程度）
- examples のディレクトリ構成と CI の YAML の細部
- README の文面（handbook の common-lib.md の形に合わせる）
