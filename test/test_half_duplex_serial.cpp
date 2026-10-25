#include <gtest/gtest.h>

#include <cstring>
#include <vector>

#include "fake_state.hpp"
#include "rabcl_esp32/half_duplex_serial.hpp"

namespace rabcl_esp32
{

class HalfDuplexSerialTest : public ::testing::Test
{
protected:
  void SetUp() override {fake_state::Reset();}

  static HalfDuplexSerialConfig TestConfig()
  {
    HalfDuplexSerialConfig cfg;
    cfg.port = UART_NUM_1;
    cfg.tx = GPIO_NUM_8;
    cfg.rx = GPIO_NUM_10;
    cfg.baud = 1000000;
    cfg.rx_buffer = 256;
    return cfg;
  }

  static fake_state::UartRecord & Uart() {return fake_state::uart()[UART_NUM_1];}

  static void Pending(std::initializer_list<uint8_t> bytes)  // Transfer の前から溜まっているもの
  {
    for (uint8_t b : bytes) {
      Uart().rx_pending.push_back(b);
    }
  }

  static void AfterWrite(std::initializer_list<uint8_t> bytes)  // 送信後に届くもの（エコー + 応答）
  {
    for (uint8_t b : bytes) {
      Uart().rx_after_write.push_back(b);
    }
  }

  // ST3215 の PING（6 B）とその ACK（6 B）
  const std::vector<uint8_t> ping_ = {0xFF, 0xFF, 0x01, 0x02, 0x01, 0xFB};
  const std::vector<uint8_t> ack_ = {0xFF, 0xFF, 0x01, 0x02, 0x00, 0xFC};
};

TEST_F(HalfDuplexSerialTest, StartConfiguresUart)
{
  HalfDuplexSerial port(TestConfig());
  ASSERT_EQ(ESP_OK, port.Start());
  EXPECT_TRUE(Uart().installed);
  EXPECT_EQ(Uart().rx_buffer_size, 256);
  EXPECT_EQ(Uart().tx_buffer_size, 0);
  EXPECT_EQ(Uart().config.baud_rate, 1000000);
  EXPECT_EQ(Uart().config.data_bits, UART_DATA_8_BITS);
  EXPECT_EQ(Uart().config.parity, UART_PARITY_DISABLE);
  EXPECT_EQ(Uart().config.stop_bits, UART_STOP_BITS_1);
  EXPECT_EQ(Uart().config.flow_ctrl, UART_HW_FLOWCTRL_DISABLE);
  EXPECT_EQ(Uart().tx_pin, 8);
  EXPECT_EQ(Uart().rx_pin, 10);
}

TEST_F(HalfDuplexSerialTest, TransferWritesThenSkipsEchoThenReads)
{
  HalfDuplexSerial port(TestConfig());
  ASSERT_EQ(ESP_OK, port.Start());
  AfterWrite({0xFF, 0xFF, 0x01, 0x02, 0x01, 0xFB, 0xFF, 0xFF, 0x01, 0x02, 0x00, 0xFC});  // エコー + 応答
  uint8_t rx[8] = {};
  EXPECT_EQ(6, port.Transfer(ping_.data(), ping_.size(), rx, 6, 10));
  EXPECT_EQ(0, std::memcmp(rx, ack_.data(), 6));
  EXPECT_EQ(ping_, Uart().written);
  EXPECT_EQ(1, Uart().wait_tx_done_calls);
}

TEST_F(HalfDuplexSerialTest, TransferFlushesStaleInput)
{
  HalfDuplexSerial port(TestConfig());
  ASSERT_EQ(ESP_OK, port.Start());
  Pending({0x11, 0x22, 0x33});  // 前回の残り（遅れて来た応答など）
  AfterWrite({0xFF, 0xFF, 0x01, 0x02, 0x01, 0xFB, 0xFF, 0xFF, 0x01, 0x02, 0x00, 0xFC});
  uint8_t rx[8] = {};
  EXPECT_EQ(6, port.Transfer(ping_.data(), ping_.size(), rx, 6, 10));
  EXPECT_EQ(1, Uart().flush_calls);
  EXPECT_EQ(0, std::memcmp(rx, ack_.data(), 6));  // 残りに引きずられず応答だけを受ける
}

TEST_F(HalfDuplexSerialTest, TransferReturnsMinusOneWhenEchoMissing)
{
  HalfDuplexSerial port(TestConfig());
  ASSERT_EQ(ESP_OK, port.Start());
  AfterWrite({0xFF, 0xFF, 0x01});  // エコーが途中まで
  uint8_t rx[8] = {};
  EXPECT_EQ(-1, port.Transfer(ping_.data(), ping_.size(), rx, 6, 10));
}

TEST_F(HalfDuplexSerialTest, TransferReturnsShortReadOnTimeout)
{
  HalfDuplexSerial port(TestConfig());
  ASSERT_EQ(ESP_OK, port.Start());
  AfterWrite({0xFF, 0xFF, 0x01, 0x02, 0x01, 0xFB, 0xFF, 0xFF, 0x01, 0x02});  // エコー + 応答 4 B だけ
  uint8_t rx[8] = {};
  EXPECT_EQ(4, port.Transfer(ping_.data(), ping_.size(), rx, 6, 10));
  EXPECT_EQ(pdMS_TO_TICKS(10), Uart().last_read_timeout_ticks);
}

TEST_F(HalfDuplexSerialTest, TransferBeforeStartIsInvalidState)
{
  HalfDuplexSerial port(TestConfig());
  uint8_t rx[8] = {};
  EXPECT_EQ(-ESP_ERR_INVALID_STATE, port.Transfer(ping_.data(), ping_.size(), rx, 6, 10));
}

}  // namespace rabcl_esp32
