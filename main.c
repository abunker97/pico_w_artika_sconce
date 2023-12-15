#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hardware/gpio.h"
#include "lwip/apps/mqtt.h"
#include "lwip/ip4_addr.h"
#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"
#include "ping.h"

#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "queue.h"
#include "semphr.h"
#include "task.h"

#include "WS281X.h"
#include "asMqtt.h"
#include "heartbeat.h"
#include "terminal.h"

int main() {
  stdio_init_all();

  printf("\r\n\r\nPICO_W ARTIKA SCONCE\r\n\r\n");

  uint32_t status;

  // creates all tasks
  // status = HeartbeatTaskSetup();
  status = TerminalTaskSetup();

  status = WS281X_taskSetup();

  status = asMqtt_TaskSetup();

  vTaskStartScheduler();

  // Should never get here
  return 0;
}
