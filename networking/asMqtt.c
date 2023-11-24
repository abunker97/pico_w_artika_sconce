#include "asMqtt.h"
#include "heartbeat.h"
#include "WS2812.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pico/cyw43_arch.h"
#include "lwip/ip4_addr.h"
#include "lwip/apps/mqtt.h"
#include "ping.h"

#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

static TaskHandle_t MqttTaskHandle = NULL;

static mqtt_client_t* mqtt_client;

static ip4_addr_t brokerIP;

static char mqttData[ MAX_MQTT_MESSAGE_DATA ];

static char num_to_string_buf[ 10 ];

#define NUMBER_OF_SUB_TOPICS 3

typedef enum
{
   COMMAND_TOPIC,
   EFFECT_COMMAND_TOPIC,
   BRIGHTNESS_COMMAND_TOPIC,
} asMqtt_subTopics_enum;

static const char* subTopics[ NUMBER_OF_SUB_TOPICS ] = {
    [COMMAND_TOPIC] = COMMAND_TOPIC_STRING,
    [EFFECT_COMMAND_TOPIC] = EFFECT_COMMAND_TOPIC_STRING,
    [BRIGHTNESS_COMMAND_TOPIC] = BRIGHTNESS_COMMAND_TOPIC_STRING,
};

#define NUMBER_OF_PUB_TOPICS 4

typedef enum
{
   AVAILABILITY_TOPIC,
   STATE_TOPIC,
   EFFECT_STATE_TOPIC,
   BRIGHTNESS_STATE_TOPIC,
} asMqtt_pubTopics_enum;

static const char* pubTopics[ NUMBER_OF_PUB_TOPICS ] = {
    [AVAILABILITY_TOPIC] = AVAILABILITY_TOPIC_STRING,
    [STATE_TOPIC] = STATE_TOPIC_STRING,
    [EFFECT_STATE_TOPIC] = EFFECT_STATE_TOPIC_STRING,
    [BRIGHTNESS_STATE_TOPIC] = BRIGHTNESS_STATE_TOPIC_STRING,
};

static const struct mqtt_connect_client_info_t mqtt_client_info = {
    MQTT_CLIENT_NAME,
    MQTT_USERNAME,
    MQTT_PASSWORD,
    MQTT_KEEPALIVE_S,
    AVAILABILITY_TOPIC_STRING,
    "offline",
    1,
    1,
};

static LightState currentLightState = {
    .lightState = WS2812_LIGHT_OFF,
    .effect = WS2812_LIGHT_EFFECT_1,
    .brightness = 255,
};

asMqtt_subTopics_enum currentTopic;

static void mqtt_incoming_data_cb( void*, const uint8_t*, uint16_t, uint8_t );
static void mqtt_incoming_publish_cb( void*, const char*, uint32_t );
static void mqtt_request_cb( void*, err_t );
static void mqtt_connection_cb( mqtt_client_t*, void*,
                                mqtt_connection_status_t );

static void mqtt_incoming_data_cb( void* arg, const uint8_t* data, uint16_t len,
                                   uint8_t flags )
{
   const struct mqtt_connect_client_info_t* client_info =
       (const struct mqtt_connect_client_info_t*)arg;

   // need to move data because data not reset
   if( len < MAX_MQTT_MESSAGE_DATA - 1 )
   {
      memcpy( mqttData, data, len );
      mqttData[ len ] = '\0';
   }
   else
   {
      strcpy( mqttData, "ERROR: TOO LARGE" );
   }

   switch( currentTopic )
   {
   case COMMAND_TOPIC:
      for( uint32_t i = 0; i < WS2812_LIGHT_STATE_NUM; i++ )
      {
         if( strcmp( mqttData, WS2812_Light_State_Strings[ i ] ) == 0 )
         {
            currentLightState.lightState = i;
            break;
         }
      }

      mqtt_publish(
          mqtt_client, pubTopics[ STATE_TOPIC ],
          WS2812_Light_State_Strings[ currentLightState.lightState ],
          strlen( WS2812_Light_State_Strings[ currentLightState.lightState ] ),
          MQTT_QOS, true, mqtt_request_cb,
          LWIP_CONST_CAST( void*, &mqtt_client_info ) );

      break;
   case EFFECT_COMMAND_TOPIC:
      for( uint32_t i = 0; i < WS2812_LIGHT_EFFECT_NUM; i++ )
      {
         if( strcmp( mqttData, WS2812_Light_Effect_Strings[ i ] ) == 0 )
         {
            currentLightState.effect = i;
         }

         mqtt_publish(
             mqtt_client, pubTopics[ EFFECT_STATE_TOPIC ],
             WS2812_Light_Effect_Strings[ currentLightState.effect ],
             strlen( WS2812_Light_Effect_Strings[ currentLightState.effect ] ),
             MQTT_QOS, true, mqtt_request_cb,
             LWIP_CONST_CAST( void*, &mqtt_client_info ) );
      }
      break;
   case BRIGHTNESS_COMMAND_TOPIC:

      currentLightState.brightness = atoi( mqttData );

      itoa( currentLightState.brightness, num_to_string_buf,
            sizeof( num_to_string_buf ) / sizeof( char ) );

      mqtt_publish( mqtt_client, pubTopics[ BRIGHTNESS_STATE_TOPIC ],
                    num_to_string_buf, strlen( num_to_string_buf ), MQTT_QOS,
                    true, mqtt_request_cb,
                    LWIP_CONST_CAST( void*, &mqtt_client_info ) );
      break;
   default:
      break;
   }

   // TODO: send new state over to light control task

   printf( "MQTT client \"%s\" data cb: data '%s' len %d, flags %d\r\n",
           client_info->client_id, mqttData, (int)len, (int)flags );
}

static void mqtt_incoming_publish_cb( void* arg, const char* topic,
                                      uint32_t tot_len )
{
   const struct mqtt_connect_client_info_t* client_info =
       (const struct mqtt_connect_client_info_t*)arg;

   for( uint32_t i = 0; i < NUMBER_OF_SUB_TOPICS; i++ )
   {
      if( strcmp( topic, subTopics[ i ] ) == 0 )
      {
         currentTopic = i;
         break;
      }
   }

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

   printf( "MQTT client \"%s\" connection cb: status %d\r\n",
           client_info->client_id, (int)status );

   if( status == MQTT_CONNECT_ACCEPTED )
   {
      for( uint32_t i = 0; i < NUMBER_OF_SUB_TOPICS; i++ )
      {
         mqtt_subscribe( client, subTopics[ i ], MQTT_QOS, mqtt_request_cb,
                         LWIP_CONST_CAST( void*, &mqtt_client_info ) );
      }
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

   if( cyw43_arch_wifi_connect_timeout_ms( WIFI_SSID, WIFI_PASSWORD,
                                           CYW43_AUTH_WPA2_AES_PSK, 3000 ) )
   {
      printf( "ERROR: Failed to connect to wifi.\r\n" );

      // change led blink rate to almost solid if connection fails
      uint32_t delayVal = 50;
      xQueueSend( delay_queue, (void*)&delayVal, 10 );

      vTaskDelete( NULL );
   }

   printf( "WIFI CONNECTED\r\n" );

   // change led blink rate to slower when wifi connected
   uint32_t delayVal = 1000;
   xQueueSend( delay_queue, (void*)&delayVal, 10 );

   // TODO: add dns
   IP4_ADDR( &brokerIP, 192, 168, 1, 75 );

   mqtt_client = mqtt_client_new();

   mqtt_set_inpub_callback( mqtt_client, mqtt_incoming_publish_cb,
                            mqtt_incoming_data_cb,
                            LWIP_CONST_CAST( void*, &mqtt_client_info ) );

   mqtt_client_connect( mqtt_client, &brokerIP, MQTT_PORT, mqtt_connection_cb,
                        LWIP_CONST_CAST( void*, &mqtt_client_info ),
                        &mqtt_client_info );

   // wait for connection
   while( !mqtt_client_is_connected( mqtt_client ) )
   {
      vTaskDelay( 100 / portTICK_PERIOD_MS );
   }

   mqtt_publish( mqtt_client, pubTopics[ AVAILABILITY_TOPIC ], "online", 6,
                 MQTT_QOS, true, mqtt_request_cb,
                 LWIP_CONST_CAST( void*, &mqtt_client_info ) );

   mqtt_publish(
       mqtt_client, pubTopics[ STATE_TOPIC ],
       WS2812_Light_State_Strings[ currentLightState.lightState ],
       strlen( WS2812_Light_State_Strings[ currentLightState.lightState ] ),
       MQTT_QOS, true, mqtt_request_cb,
       LWIP_CONST_CAST( void*, &mqtt_client_info ) );

   mqtt_publish(
       mqtt_client, pubTopics[ EFFECT_STATE_TOPIC ],
       WS2812_Light_Effect_Strings[ currentLightState.effect ],
       strlen( WS2812_Light_Effect_Strings[ currentLightState.effect ] ),
       MQTT_QOS, true, mqtt_request_cb,
       LWIP_CONST_CAST( void*, &mqtt_client_info ) );

   itoa( currentLightState.brightness, num_to_string_buf,
         sizeof( num_to_string_buf ) / sizeof( char ) );

   mqtt_publish( mqtt_client, pubTopics[ BRIGHTNESS_STATE_TOPIC ],
                 num_to_string_buf, strlen( num_to_string_buf ), MQTT_QOS, true,
                 mqtt_request_cb, LWIP_CONST_CAST( void*, &mqtt_client_info ) );

   while( true )
   {
      vTaskDelay( 1000 / portTICK_PERIOD_MS );
   }
}

uint32_t asMqtt_TaskSetup()
{
   return xTaskCreate( MqttTask, "Wifi Task", configMINIMAL_STACK_SIZE, NULL, 2,
                       &MqttTaskHandle );
}
