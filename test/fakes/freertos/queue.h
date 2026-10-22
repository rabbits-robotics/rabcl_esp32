// fake: freertos/queue.h（ユニットテスト用。固定長キュー。待ちはしない）
#ifndef FAKES__FREERTOS__QUEUE_H_
#define FAKES__FREERTOS__QUEUE_H_

#include "freertos/FreeRTOS.h"

typedef void * QueueHandle_t;

QueueHandle_t xQueueCreate(UBaseType_t length, UBaseType_t item_size);
void vQueueDelete(QueueHandle_t queue);
BaseType_t xQueueSendFromISR(QueueHandle_t queue, const void * item, BaseType_t * higher_priority_task_woken);
BaseType_t xQueueReceive(QueueHandle_t queue, void * buffer, TickType_t ticks_to_wait);

#endif  // FAKES__FREERTOS__QUEUE_H_
