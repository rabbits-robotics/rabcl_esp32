// fake: driver/uart.h（ユニットテスト用。使う型と関数だけ。値は本物に合わせる）
#ifndef FAKES__DRIVER__UART_H_
#define FAKES__DRIVER__UART_H_

#include <cstddef>
#include <cstdint>

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#define UART_PIN_NO_CHANGE (-1)

typedef enum {
  UART_NUM_0, UART_NUM_1, UART_NUM_2, UART_NUM_3, UART_NUM_4, UART_NUM_MAX
} uart_port_t;

typedef enum { UART_DATA_5_BITS = 0x0, UART_DATA_6_BITS = 0x1, UART_DATA_7_BITS = 0x2, UART_DATA_8_BITS = 0x3 } uart_word_length_t;
typedef enum { UART_STOP_BITS_1 = 0x1, UART_STOP_BITS_1_5 = 0x2, UART_STOP_BITS_2 = 0x3 } uart_stop_bits_t;
typedef enum { UART_PARITY_DISABLE = 0x0, UART_PARITY_EVEN = 0x2, UART_PARITY_ODD = 0x3 } uart_parity_t;
typedef enum { UART_HW_FLOWCTRL_DISABLE = 0x0, UART_HW_FLOWCTRL_RTS = 0x1, UART_HW_FLOWCTRL_CTS = 0x2, UART_HW_FLOWCTRL_CTS_RTS = 0x3 } uart_hw_flowcontrol_t;
typedef enum { UART_SCLK_DEFAULT = 0 } uart_sclk_t;

typedef struct {
  int baud_rate;
  uart_word_length_t data_bits;
  uart_parity_t parity;
  uart_stop_bits_t stop_bits;
  uart_hw_flowcontrol_t flow_ctrl;
  uint8_t rx_flow_ctrl_thresh;
  uart_sclk_t source_clk;
  struct {
    uint32_t allow_pd : 1;
    uint32_t backup_before_sleep : 1;
  } flags;
} uart_config_t;

esp_err_t uart_driver_install(uart_port_t uart_num, int rx_buffer_size, int tx_buffer_size, int queue_size, QueueHandle_t * uart_queue, int intr_alloc_flags);
esp_err_t uart_driver_delete(uart_port_t uart_num);
bool uart_is_driver_installed(uart_port_t uart_num);
esp_err_t uart_param_config(uart_port_t uart_num, const uart_config_t * uart_config);
esp_err_t uart_set_pin(uart_port_t uart_num, int tx_io_num, int rx_io_num, int rts_io_num, int cts_io_num);
int uart_write_bytes(uart_port_t uart_num, const void * src, size_t size);
int uart_read_bytes(uart_port_t uart_num, void * buf, uint32_t length, TickType_t ticks_to_wait);
esp_err_t uart_flush_input(uart_port_t uart_num);
esp_err_t uart_wait_tx_done(uart_port_t uart_num, TickType_t ticks_to_wait);

#endif  // FAKES__DRIVER__UART_H_
