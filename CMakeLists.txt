idf_component_register(
  SRCS
    src/can_bus.cpp
    src/debug_led.cpp
  INCLUDE_DIRS include
  REQUIRES esp_driver_twai esp_driver_gpio)
