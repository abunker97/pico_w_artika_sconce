#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "hardware/gpio.h"
#include "lwip/ip4_addr.h"
#include "ping.h"

#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

#include "heartbeat.h"
#include "terminal.h"

TaskHandle_t wifiTask = NULL;

void WifiTask( void* param )
{
   if( cyw43_arch_init() )
   {
      printf( "Failed to initialize cyw43\r\n" );
      return;
   }

   // allow the led task to start now that cyw43 is initialized
   xSemaphoreGive( HeartbeatSetupSemaphore );

   cyw43_arch_enable_sta_mode();

   printf( "Connection to wifi..\r\n" );

   if( cyw43_arch_wifi_connect_timeout_ms( "ATB", "Aust1nBunk3r6545",
                                           CYW43_AUTH_WPA2_AES_PSK, 3000 ) )
   {
      printf( "ERROR: Failed to connect to wifi." );
      exit( 1 );
   }

   printf( "WIFI CONNECTED\r\n" );

   ip_addr_t ping_addr;
   ipaddr_aton( "192.168.1.23", &ping_addr );
   ping_init( &ping_addr );

   while( true )
   {
      vTaskDelay( 100 );
   }

   vTaskDelete( NULL );
}

int main()
{
   stdio_init_all();

   printf( "\r\n\r\nSTARTING FREERT0S EXAMPLE\r\n\r\n" );

   // creates queue for passing values around
   delay_queue = xQueueCreate( DELAY_QUEUE_LEN, sizeof( uint32_t ) );
   HeartbeatSemaphore = xSemaphoreCreateBinary();
   HeartbeatSetupSemaphore = xSemaphoreCreateBinary();

   uint32_t status;

   status = xTaskCreate( HeartbeatTask, "LED Task", configMINIMAL_STACK_SIZE,
                         NULL, tskIDLE_PRIORITY, &HeartbeatTaskHandle );

   status = xTaskCreate( WifiTask, "Wifi Task", configMINIMAL_STACK_SIZE, NULL,
                         2, &wifiTask );

   status = xTaskCreate( TerminalTask, "Terminal Task",
                         configMINIMAL_STACK_SIZE, NULL, 1, &TerminalTaskHandle );

   vTaskStartScheduler();

   // Should never get here
   return 0;
}
