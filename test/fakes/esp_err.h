// fake: esp_err.h（ユニットテスト用。本物の値に合わせる）
#ifndef FAKES__ESP_ERR_H_
#define FAKES__ESP_ERR_H_

#include <cstdint>

typedef int esp_err_t;

#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_NO_MEM 0x101
#define ESP_ERR_INVALID_ARG 0x102
#define ESP_ERR_INVALID_STATE 0x103
#define ESP_ERR_TIMEOUT 0x107

#endif  // FAKES__ESP_ERR_H_
