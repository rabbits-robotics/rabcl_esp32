// fake: freertos/timers.h（ユニットテスト用。xTimerPendFunctionCallFromISR だけ。
// 積まれた関数は fake_state::RunPendedFunctions() でテストから実行する）
#ifndef FAKES__FREERTOS__TIMERS_H_
#define FAKES__FREERTOS__TIMERS_H_

#include "freertos/FreeRTOS.h"

typedef void (*PendedFunction_t)(void * arg1, uint32_t arg2);

BaseType_t xTimerPendFunctionCallFromISR(
  PendedFunction_t function, void * arg1, uint32_t arg2, BaseType_t * higher_priority_task_woken);

#endif  // FAKES__FREERTOS__TIMERS_H_
