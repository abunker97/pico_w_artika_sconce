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

TaskHandle_t MqttTaskHandle = NULL;

static mqtt_client_t* mqtt_client;

static ip4_addr_t brokerIP;

#define MAX_MQTT_DATA 50
static char mqttData[ MAX_MQTT_DATA ];

static const struct mqtt_connect_client_info_t mqtt_client_info = {
    "test", "austin", "Peyton@1997", 100, NULL, NULL, 0, 0,
};

static void mqtt_incoming_data_cb( void* arg, const uint8_t* data, uint16_t len,
                                   uint8_t flags )
{
   const struct mqtt_connect_client_info_t* client_info =
       (const struct mqtt_connect_client_info_t*)arg;

   if( len < MAX_MQTT_DATA - 1 )
   {
      memcpy( mqttData, data, len );
      mqttData[ len ] = '\0';
   }
   else
   {
      strcpy( mqttData, "TOO LARGE" );
   }

   printf( "MQTT client \"%s\" data cb: data '%s' len %d, flags %d\r\n",
           client_info->client_id, mqttData, (int)len, (int)flags );
}

static void mqtt_incoming_publish_cb( void* arg, const char* topic,
                                      uint32_t tot_len )
{
   const struct mqtt_connect_client_info_t* client_info =
       (const struct mqtt_connect_client_info_t*)arg;

   printf( "MQTT client \"%s\" publish cb: topic '%s', len %d\r\n",
           client_info->client_id, topic, (int)tot_len );
}

static void mqtt_request_cb( void* arg, err_t err )
{
   const struct mqtt_connect_client_info_t* client_info =
       (const struct mqtt_connect_client_info_t*)arg;

   printf( "MQTT client \"%s\" request cb: err %d\r\n", client_info->client_id,
           (int)err );
}

static void mqtt_connection_cb( mqtt_client_t* client, void* arg,
                                mqtt_connection_status_t status )
{
   const struct mqtt_connect_client_info_t* client_info =
       (const struct mqtt_connect_client_info_t*)arg;
   LWIP_UNUSED_ARG( client );

   printf( "MQTT client \"%s\" connection cb: status %d\\n",
           client_info->client_id, (int)status );

   if( status == MQTT_CONNECT_ACCEPTED )
   {
      mqtt_sub_unsub( client, "topic_qos1", 1, mqtt_request_cb,
                      LWIP_CONST_CAST( void*, client_info ), 1 );
      mqtt_sub_unsub( client, "topic_qos0", 0, mqtt_request_cb,
                      LWIP_CONST_CAST( void*, client_info ), 1 );
   }
}

void MqttTask( void* param )
{
   if( cyw43_arch_init() )
   {
      printf( "Failed to initialize cyw43\r\n" );
      return;
   }

   // allow the led task to start now that cyw43 is initialized
   if( HeartbeatSetupSemaphore != NULL )
   {
      xSemaphoreGive( HeartbeatSetupSemaphore );
   }

   cyw43_arch_enable_sta_mode();

   printf( "Connection to wifi..\r\n" );

   if( cyw43_arch_wifi_connect_timeout_ms( "ATB", "Aust1nBunk3r6545",
                                           CYW43_AUTH_WPA2_AES_PSK, 3000 ) )
   {
      printf( "ERROR: Failed to connect to wifi.\r\n" );
      vTaskDelete( NULL );
   }

   printf( "WIFI CONNECTED\r\n" );

   IP4_ADDR( &brokerIP, 192, 168, 1, 75 );

   mqtt_client = mqtt_client_new();

   mqtt_set_inpub_callback( mqtt_client, mqtt_incoming_publish_cb,
                            mqtt_incoming_data_cb,
                            LWIP_CONST_CAST( void*, &mqtt_client_info ) );

   mqtt_client_connect( mqtt_client, &brokerIP, MQTT_PORT, mqtt_connection_cb,
                        LWIP_CONST_CAST( void*, &mqtt_client_info ),
                        &mqtt_client_info );

   while( true )
   {
      vTaskDelay( 100 );
   }

   vTaskDelete( NULL );
}

int main()
{
   stdio_init_all();

   printf( "\r\n\r\nPICO_W ARTIKA SCONCE\r\n\r\n" );

   uint32_t status;

   // creates all tasks
   status = HeartbeatTaskSetup();
   status = TerminalTaskSetup();

   status = xTaskCreate( MqttTask, "Wifi Task", configMINIMAL_STACK_SIZE, NULL,
                         2, &MqttTaskHandle );

   vTaskStartScheduler();

   // Should never get here
   return 0;
}
