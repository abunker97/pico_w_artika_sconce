#include "heartbeat.h"

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"

SemaphoreHandle_t HeartbeatSemaphore = NULL;
SemaphoreHandle_t HeartbeatSetupSemaphore = NULL;

QueueHandle_t delay_queue = NULL;

TaskHandle_t HeartbeatTaskHandle = NULL;

// setup Heartbeat Task
uint32_t HeartbeatTaskSetup()
{
   HeartbeatSemaphore = xSemaphoreCreateBinary();
   HeartbeatSetupSemaphore = xSemaphoreCreateBinary();

   return xTaskCreate( HeartbeatTask, "Heartbeat Task", configMINIMAL_STACK_SIZE,
                       NULL, tskIDLE_PRIORITY, &HeartbeatTaskHandle );
}

// task for blinking the led
void HeartbeatTask( void* param )
{
   uint32_t delayTime = 1000;

   xSemaphoreTake( HeartbeatSetupSemaphore, portMAX_DELAY );

   while( 1 )
   {
      // receive val from queue with non-blocking
      if( xQueueReceive( delay_queue, (void*)&delayTime, 0 ) == pdTRUE )
      {
         printf( "Updating delay time to %d\r\n", delayTime );
      }

      // toggle led pin
      cyw43_arch_gpio_put( CYW43_WL_GPIO_LED_PIN,
                           !cyw43_arch_gpio_get( CYW43_WL_GPIO_LED_PIN ) );

      // Use a semaphore here so terminal task can set this task as ready
      xSemaphoreTake( HeartbeatSemaphore, delayTime / portTICK_PERIOD_MS );
   }
}
