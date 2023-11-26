#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "hardware/gpio.h"
#include "lwip/ip4_addr.h"
#include "lwip/apps/mqtt.h"
#include "ping.h"

#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

#include "heartbeat.h"
#include "terminal.h"
#include "asMqtt.h"
#include "WS281X.h"

int main()
{
   stdio_init_all();

   printf( "\r\n\r\nPICO_W ARTIKA SCONCE\r\n\r\n" );

   uint32_t status;

   // creates all tasks
   status = HeartbeatTaskSetup();
   status = TerminalTaskSetup();

   status = WS281X_taskSetup();

   status = asMqtt_TaskSetup();

   vTaskStartScheduler();

   // Should never get here
   return 0;
}
