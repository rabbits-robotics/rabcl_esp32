#include <gtest/gtest.h>

#include <cstring>

#include "fake_state.hpp"
#include "rabcl_esp32/serial_port.hpp"

namespace rabcl_esp32
{

class SerialPortTest : public ::testing::Test
{
protected:
  void SetUp() override {fake_state::Reset();}

  static SerialPortConfig TestConfig()
  {
    SerialPortConfig cfg;
    cfg.port = UART_NUM_2;
    cfg.tx = GPIO_NUM_26;
    cfg.rx = GPIO_NUM_27;
    cfg.baud = 921600;
    cfg.rx_buffer = 1024;
    cfg.tx_buffer = 512;
    return cfg;
  }

  static fake_state::UartRecord & Uart() {return fake_state::uart()[UART_NUM_2];}
};

TEST_F(SerialPortTest, StartConfiguresPortPinsAndBaud)
{
  SerialPort port(TestConfig());
  ASSERT_EQ(ESP_OK, port.Start());
  EXPECT_TRUE(Uart().installed);
  EXPECT_EQ(Uart().rx_buffer_size, 1024);
  EXPECT_EQ(Uart().tx_buffer_size, 512);
  EXPECT_EQ(Uart().config.baud_rate, 921600);
  EXPECT_EQ(Uart().config.data_bits, UART_DATA_8_BITS);
  EXPECT_EQ(Uart().config.parity, UART_PARITY_DISABLE);
  EXPECT_EQ(Uart().config.stop_bits, UART_STOP_BITS_1);
  EXPECT_EQ(Uart().config.flow_ctrl, UART_HW_FLOWCTRL_DISABLE);
  EXPECT_EQ(Uart().tx_pin, 26);
  EXPECT_EQ(Uart().rx_pin, 27);
  EXPECT_EQ(Uart().rts_pin, UART_PIN_NO_CHANGE);
  EXPECT_EQ(Uart().cts_pin, UART_PIN_NO_CHANGE);
}

TEST_F(SerialPortTest, StartTwiceIsInvalidState)
{
  SerialPort port(TestConfig());
  ASSERT_EQ(ESP_OK, port.Start());
  EXPECT_EQ(ESP_ERR_INVALID_STATE, port.Start());
}

TEST_F(SerialPortTest, WriteBeforeStartIsInvalidState)
{
  SerialPort port(TestConfig());
  const uint8_t data[3] = {1, 2, 3};
  EXPECT_EQ(-ESP_ERR_INVALID_STATE, port.Write(data, sizeof(data)));
  uint8_t buf[4];
  EXPECT_EQ(-ESP_ERR_INVALID_STATE, port.Read(buf, sizeof(buf), 0));
}

TEST_F(SerialPortTest, WritePassesBytes)
{
  SerialPort port(TestConfig());
  ASSERT_EQ(ESP_OK, port.Start());
  const uint8_t data[4] = {0xA5, 0x5A, 0x01, 0x02};
  EXPECT_EQ(4, port.Write(data, sizeof(data)));
  ASSERT_EQ(Uart().written.size(), 4u);
  EXPECT_EQ(std::memcmp(Uart().written.data(), data, 4), 0);
}

TEST_F(SerialPortTest, ReadReturnsQueuedBytes)
{
  SerialPort port(TestConfig());
  ASSERT_EQ(ESP_OK, port.Start());
  Uart().rx_pending = {10, 20, 30, 40, 50};
  uint8_t buf[8] = {};
  EXPECT_EQ(5, port.Read(buf, sizeof(buf), pdMS_TO_TICKS(10)));
  EXPECT_EQ(buf[0], 10);
  EXPECT_EQ(buf[4], 50);
  EXPECT_EQ(Uart().last_read_timeout_ticks, pdMS_TO_TICKS(10));
  EXPECT_EQ(0, port.Read(buf, sizeof(buf), 0));  // 何も無ければ 0
}

TEST_F(SerialPortTest, ReadIsLimitedToBufferLength)
{
  SerialPort port(TestConfig());
  ASSERT_EQ(ESP_OK, port.Start());
  Uart().rx_pending = {1, 2, 3, 4, 5};
  uint8_t buf[2] = {};
  EXPECT_EQ(2, port.Read(buf, sizeof(buf), 0));
  EXPECT_EQ(Uart().rx_pending.size(), 3u);
}

TEST_F(SerialPortTest, StopDeletesDriverAndAllowsRestart)
{
  SerialPort port(TestConfig());
  ASSERT_EQ(ESP_OK, port.Start());
  EXPECT_EQ(ESP_OK, port.Stop());
  EXPECT_FALSE(Uart().installed);
  EXPECT_EQ(Uart().delete_calls, 1);
  EXPECT_EQ(ESP_ERR_INVALID_STATE, port.Stop());
  EXPECT_EQ(ESP_OK, port.Start());
  EXPECT_TRUE(Uart().installed);
}

TEST_F(SerialPortTest, DestructorStopsStartedPort)
{
  {
    SerialPort port(TestConfig());
    ASSERT_EQ(ESP_OK, port.Start());
  }
  EXPECT_FALSE(Uart().installed);
}

TEST_F(SerialPortTest, StartPropagatesDriverInstallFailure)
{
  fake_state::uart_install_result() = ESP_ERR_NO_MEM;
  SerialPort port(TestConfig());
  EXPECT_EQ(ESP_ERR_NO_MEM, port.Start());
  EXPECT_FALSE(Uart().installed);
  fake_state::uart_install_result() = ESP_OK;
  EXPECT_EQ(ESP_OK, port.Start());
}

}  // namespace rabcl_esp32
