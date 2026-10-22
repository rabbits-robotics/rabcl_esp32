// fake: freertos/FreeRTOS.h（ユニットテスト用。本物と同じく 1 tick = 10 ms（configTICK_RATE_HZ = 100）にして単位変換の誤りを検出する）
#ifndef FAKES__FREERTOS__FREERTOS_H_
#define FAKES__FREERTOS__FREERTOS_H_

#include <cstdint>

typedef uint32_t TickType_t;
typedef int BaseType_t;
typedef unsigned int UBaseType_t;

#define configTICK_RATE_HZ 100
#define pdTRUE 1
#define pdFALSE 0
#define pdPASS 1
#define pdFAIL 0
#define portMAX_DELAY 0xFFFFFFFFu
#define pdMS_TO_TICKS(ms) ((TickType_t)(((uint64_t)(ms) * configTICK_RATE_HZ) / 1000))
#define pdTICKS_TO_MS(ticks) ((TickType_t)(((uint64_t)(ticks) * 1000) / configTICK_RATE_HZ))

#endif  // FAKES__FREERTOS__FREERTOS_H_
