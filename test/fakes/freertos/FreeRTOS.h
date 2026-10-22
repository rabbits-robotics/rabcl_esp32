// fake: freertos/FreeRTOS.h（ユニットテスト用。ティック = ミリ秒として扱う）
#ifndef FAKES__FREERTOS__FREERTOS_H_
#define FAKES__FREERTOS__FREERTOS_H_

#include <cstdint>

typedef uint32_t TickType_t;
typedef int BaseType_t;
typedef unsigned int UBaseType_t;

#define pdTRUE 1
#define pdFALSE 0
#define pdPASS 1
#define pdFAIL 0
#define portMAX_DELAY 0xFFFFFFFFu
#define pdMS_TO_TICKS(ms) ((TickType_t)(ms))
#define pdTICKS_TO_MS(ticks) ((uint32_t)(ticks))

#endif  // FAKES__FREERTOS__FREERTOS_H_
