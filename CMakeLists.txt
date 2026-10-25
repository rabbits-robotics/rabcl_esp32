idf_component_register(
  SRCS
    src/can_bus.cpp
    src/debug_led.cpp
    src/serial_port.cpp
    src/half_duplex_serial.cpp
  INCLUDE_DIRS include
  REQUIRES esp_driver_twai esp_driver_gpio esp_driver_uart)
