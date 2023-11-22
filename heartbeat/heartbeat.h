#ifndef HEARTBEAT_H
#define HEARTBEAT_H

#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

extern SemaphoreHandle_t HeartbeatSemaphore;
extern SemaphoreHandle_t HeartbeatSetupSemaphore;

extern QueueHandle_t delay_queue;

extern TaskHandle_t HeartbeatTaskHandle;

uint32_t HeartbeatTaskSetup();

void HeartbeatTask( void* );

#endif
