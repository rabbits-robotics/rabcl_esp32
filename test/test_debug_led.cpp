#include <gtest/gtest.h>

#include "fake_state.hpp"
#include "rabcl_esp32/debug_led.hpp"

using rabcl_esp32::DebugLed;
using rabcl_esp32::DebugLedConfig;

namespace
{

class DebugLedTest : public ::testing::Test
{
protected:
  void SetUp() override { fake_state::Reset(); }
  void TearDown() override { fake_state::Reset(); }

  const std::vector<uint32_t> & Levels(gpio_num_t pin) { return fake_state::gpio()[static_cast<int>(pin)].levels; }
};

}  // namespace

TEST_F(DebugLedTest, StartConfiguresOutputWithoutPullsAndTurnsOff)
{
  DebugLed led(DebugLedConfig{GPIO_NUM_38, true});
  EXPECT_EQ(ESP_OK, led.Start());
  const auto & cfg = fake_state::gpio()[38].config;
  EXPECT_EQ(1ULL << 38, cfg.pin_bit_mask);
  EXPECT_EQ(GPIO_MODE_OUTPUT, cfg.mode);
  EXPECT_EQ(GPIO_PULLUP_DISABLE, cfg.pull_up_en);
  EXPECT_EQ(GPIO_PULLDOWN_DISABLE, cfg.pull_down_en);
  EXPECT_EQ(GPIO_INTR_DISABLE, cfg.intr_type);
  EXPECT_EQ((std::vector<uint32_t>{1}), Levels(GPIO_NUM_38));  // 吸い込みの消灯 = High
}

TEST_F(DebugLedTest, OnDrivesLowAndOffDrivesHighWhenActiveLow)
{
  DebugLed led(DebugLedConfig{GPIO_NUM_38, true});
  ASSERT_EQ(ESP_OK, led.Start());
  led.On();
  led.Off();
  EXPECT_EQ((std::vector<uint32_t>{1, 0, 1}), Levels(GPIO_NUM_38));
}

TEST_F(DebugLedTest, OnDrivesHighAndOffDrivesLowWhenActiveHigh)
{
  DebugLed led(DebugLedConfig{GPIO_NUM_5, false});
  ASSERT_EQ(ESP_OK, led.Start());
  led.On();
  led.Off();
  EXPECT_EQ((std::vector<uint32_t>{0, 1, 0}), Levels(GPIO_NUM_5));
}

TEST_F(DebugLedTest, ToggleAlternatesStartingFromOff)
{
  DebugLed led(DebugLedConfig{GPIO_NUM_38, true});
  ASSERT_EQ(ESP_OK, led.Start());
  led.Toggle();
  led.Toggle();
  led.Toggle();
  EXPECT_EQ((std::vector<uint32_t>{1, 0, 1, 0}), Levels(GPIO_NUM_38));
}
