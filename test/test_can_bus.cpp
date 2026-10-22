#include <gtest/gtest.h>

#include "fake_state.hpp"
#include "rabcl_esp32/can_bus.hpp"

using rabcl_esp32::CanBus;
using rabcl_esp32::CanBusConfig;
using rabcl_esp32::CanFrame;

namespace
{

CanBusConfig TestConfig()
{
  CanBusConfig cfg;
  cfg.tx = GPIO_NUM_23;
  cfg.rx = GPIO_NUM_21;
  cfg.bitrate = 1000000;
  return cfg;
}

CanFrame TestFrame(uint32_t id = 0x101, uint8_t dlc = 8)
{
  CanFrame f;
  f.id = id;
  f.dlc = dlc;
  for (uint8_t i = 0; i < 8; ++i) {
    f.data[i] = static_cast<uint8_t>(0xA0 + i);
  }
  return f;
}

twai_node_t::RxFrame RxFrame(uint32_t id, uint8_t marker)
{
  twai_node_t::RxFrame f;
  f.id = id;
  f.dlc = 8;
  f.ide = false;
  f.rtr = false;
  f.timestamp = 1234;
  f.data = {marker, 1, 2, 3, 4, 5, 6, 7};
  return f;
}

class CanBusTest : public ::testing::Test
{
protected:
  void SetUp() override { fake_state::Reset(); }
  void TearDown() override { fake_state::Reset(); }

  twai_node_t * Node() { return fake_state::twai_nodes().at(0); }
};

}  // namespace

TEST_F(CanBusTest, StartCreatesEnabledNodeWithConfiguredPinsAndBitrate)
{
  CanBus can(TestConfig());
  ASSERT_EQ(ESP_OK, can.Start());
  ASSERT_EQ(1u, fake_state::twai_nodes().size());
  EXPECT_TRUE(Node()->enabled);
  EXPECT_EQ(GPIO_NUM_23, Node()->config.io_cfg.tx);
  EXPECT_EQ(GPIO_NUM_21, Node()->config.io_cfg.rx);
  EXPECT_EQ(1000000u, Node()->config.bit_timing.bitrate);
  EXPECT_EQ(GPIO_NUM_NC, Node()->config.io_cfg.quanta_clk_out);
  EXPECT_EQ(GPIO_NUM_NC, Node()->config.io_cfg.bus_off_indicator);
}

TEST_F(CanBusTest, StartTwiceReturnsInvalidStateAndKeepsOneNode)
{
  CanBus can(TestConfig());
  ASSERT_EQ(ESP_OK, can.Start());
  EXPECT_EQ(ESP_ERR_INVALID_STATE, can.Start());
  EXPECT_EQ(1u, fake_state::twai_nodes().size());
}

TEST_F(CanBusTest, StartPropagatesDriverFailureAndCanRetry)
{
  fake_state::twai_new_node_result() = ESP_ERR_NO_MEM;
  CanBus can(TestConfig());
  EXPECT_EQ(ESP_ERR_NO_MEM, can.Start());
  fake_state::twai_new_node_result() = ESP_OK;
  EXPECT_EQ(ESP_OK, can.Start());  // 失敗後にやり直せる
}

TEST_F(CanBusTest, SendBeforeStartReturnsInvalidState)
{
  CanBus can(TestConfig());
  EXPECT_EQ(ESP_ERR_INVALID_STATE, can.Send(TestFrame(), 10));
}

TEST_F(CanBusTest, SendRejectsDlcOver8WithoutTouchingDriver)
{
  CanBus can(TestConfig());
  ASSERT_EQ(ESP_OK, can.Start());
  EXPECT_EQ(ESP_ERR_INVALID_ARG, can.Send(TestFrame(0x101, 9), 10));
  EXPECT_TRUE(Node()->tx.empty());
}

TEST_F(CanBusTest, SendPassesIdDlcDataAndWaitsForCompletion)
{
  CanBus can(TestConfig());
  ASSERT_EQ(ESP_OK, can.Start());
  CanFrame f = TestFrame(0x202, 3);
  f.ext = true;
  EXPECT_EQ(ESP_OK, can.Send(f, 100));
  ASSERT_EQ(1u, Node()->tx.size());
  EXPECT_EQ(0x202u, Node()->tx[0].id);
  EXPECT_EQ(3u, Node()->tx[0].dlc);
  EXPECT_TRUE(Node()->tx[0].ide);
  EXPECT_FALSE(Node()->tx[0].rtr);
  EXPECT_EQ((std::vector<uint8_t>{0xA0, 0xA1, 0xA2}), Node()->tx[0].data);
  EXPECT_EQ(100, Node()->tx[0].timeout_ms);
  EXPECT_EQ(1, Node()->wait_all_done_calls);
}

TEST_F(CanBusTest, SendReturnsDriverTransmitError)
{
  CanBus can(TestConfig());
  ASSERT_EQ(ESP_OK, can.Start());
  Node()->transmit_result = ESP_ERR_TIMEOUT;
  EXPECT_EQ(ESP_ERR_TIMEOUT, can.Send(TestFrame(), 10));
  EXPECT_EQ(0, Node()->wait_all_done_calls);
}

TEST_F(CanBusTest, ReceiveReturnsFrameDeliveredByRxCallback)
{
  CanBus can(TestConfig());
  ASSERT_EQ(ESP_OK, can.Start());
  Node()->FireRx(RxFrame(0x141, 0xEE));
  CanFrame f;
  ASSERT_TRUE(can.Receive(f, 0));
  EXPECT_EQ(0x141u, f.id);
  EXPECT_EQ(8u, f.dlc);
  EXPECT_EQ(0xEE, f.data[0]);
  EXPECT_EQ(7, f.data[7]);
  EXPECT_EQ(1234u, f.timestamp_us);
  EXPECT_FALSE(can.Receive(f, 0));  // 2 つ目は無い
}

TEST_F(CanBusTest, ReceiveBeforeStartReturnsFalse)
{
  CanBus can(TestConfig());
  CanFrame f;
  EXPECT_FALSE(can.Receive(f, 0));
}

TEST_F(CanBusTest, RxQueueOverflowCountsDroppedAndKeepsOldest)
{
  CanBusConfig cfg = TestConfig();
  cfg.rx_queue_depth = 16;
  CanBus can(cfg);
  ASSERT_EQ(ESP_OK, can.Start());
  for (uint8_t i = 0; i < 20; ++i) {
    Node()->FireRx(RxFrame(0x101, i));
  }
  EXPECT_EQ(4u, can.GetCounters().rx_dropped);
  CanFrame f;
  int received = 0;
  uint8_t last = 0;
  while (can.Receive(f, 0)) {
    last = f.data[0];
    ++received;
  }
  EXPECT_EQ(16, received);
  EXPECT_EQ(15, last);  // 先に届いた 16 件が残り、後の 4 件が捨てられる
}

TEST_F(CanBusTest, TxDoneFailureCountsTxFail)
{
  CanBus can(TestConfig());
  ASSERT_EQ(ESP_OK, can.Start());
  Node()->FireTxDone(true);
  Node()->FireTxDone(false);
  Node()->FireTxDone(false);
  EXPECT_EQ(2u, can.GetCounters().tx_fail);
}

TEST_F(CanBusTest, ErrorCallbackCountsBusError)
{
  CanBus can(TestConfig());
  ASSERT_EQ(ESP_OK, can.Start());
  Node()->FireError(0x10);
  Node()->FireError(0x01);
  EXPECT_EQ(2u, can.GetCounters().bus_error);
}

TEST_F(CanBusTest, BusOffTriggersRecoverOnNextSendWhenAutoRecover)
{
  CanBus can(TestConfig());  // auto_recover は既定で true
  ASSERT_EQ(ESP_OK, can.Start());
  Node()->FireStateChange(TWAI_ERROR_PASSIVE, TWAI_ERROR_BUS_OFF);
  EXPECT_EQ(1u, can.GetCounters().bus_off);
  EXPECT_EQ(0, Node()->recover_calls);  // ISR の中では呼ばない
  can.Send(TestFrame(), 10);
  EXPECT_EQ(1, Node()->recover_calls);
  can.Send(TestFrame(), 10);
  EXPECT_EQ(1, Node()->recover_calls);  // 1 回のバスオフにつき 1 回
}

TEST_F(CanBusTest, BusOffTriggersRecoverOnNextReceiveWhenAutoRecover)
{
  CanBus can(TestConfig());
  ASSERT_EQ(ESP_OK, can.Start());
  Node()->FireStateChange(TWAI_ERROR_PASSIVE, TWAI_ERROR_BUS_OFF);
  CanFrame f;
  can.Receive(f, 0);
  EXPECT_EQ(1, Node()->recover_calls);
}

TEST_F(CanBusTest, BusOffWithoutAutoRecoverDoesNotRecoverUntilAsked)
{
  CanBusConfig cfg = TestConfig();
  cfg.auto_recover = false;
  CanBus can(cfg);
  ASSERT_EQ(ESP_OK, can.Start());
  Node()->FireStateChange(TWAI_ERROR_PASSIVE, TWAI_ERROR_BUS_OFF);
  can.Send(TestFrame(), 10);
  EXPECT_EQ(0, Node()->recover_calls);
  EXPECT_EQ(ESP_OK, can.Recover());
  EXPECT_EQ(1, Node()->recover_calls);
}

TEST_F(CanBusTest, RecoveredCountsWhenReturningFromBusOffToActive)
{
  CanBus can(TestConfig());
  ASSERT_EQ(ESP_OK, can.Start());
  Node()->FireStateChange(TWAI_ERROR_PASSIVE, TWAI_ERROR_BUS_OFF);
  Node()->FireStateChange(TWAI_ERROR_BUS_OFF, TWAI_ERROR_ACTIVE);
  Node()->FireStateChange(TWAI_ERROR_ACTIVE, TWAI_ERROR_WARNING);  // これは回復ではない
  EXPECT_EQ(1u, can.GetCounters().recovered);
}

TEST_F(CanBusTest, GetStatusReportsDriverState)
{
  CanBus can(TestConfig());
  twai_node_status_t st = {};
  EXPECT_EQ(ESP_ERR_INVALID_STATE, can.GetStatus(st));
  ASSERT_EQ(ESP_OK, can.Start());
  Node()->status.state = TWAI_ERROR_PASSIVE;
  Node()->status.tx_error_count = 128;
  EXPECT_EQ(ESP_OK, can.GetStatus(st));
  EXPECT_EQ(TWAI_ERROR_PASSIVE, st.state);
  EXPECT_EQ(128, st.tx_error_count);
}

TEST_F(CanBusTest, StopDisablesAndDeletesNodeAndAllowsRestart)
{
  CanBus can(TestConfig());
  ASSERT_EQ(ESP_OK, can.Start());
  EXPECT_EQ(ESP_OK, can.Stop());
  EXPECT_FALSE(Node()->enabled);
  EXPECT_TRUE(Node()->deleted);
  EXPECT_EQ(ESP_ERR_INVALID_STATE, can.Stop());
  EXPECT_EQ(ESP_OK, can.Start());
  EXPECT_EQ(2u, fake_state::twai_nodes().size());
}

TEST_F(CanBusTest, DestructorDeletesStartedNode)
{
  {
    CanBus can(TestConfig());
    ASSERT_EQ(ESP_OK, can.Start());
  }
  EXPECT_TRUE(fake_state::twai_nodes().at(0)->deleted);
}
