#include "asMqtt.h"
#include "heartbeat.h"
#include "WS281X.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "picowota/reboot.h"

#include "pico/cyw43_arch.h"
#include "lwip/ip4_addr.h"
#include "lwip/dns.h"
#include "lwip/apps/mqtt.h"
#include "ping.h"

#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

typedef enum
{
   WIFI_CONNECTED = 0,
   WIFI_DISCONNECTED,
} wifi_state;

typedef enum
{
   COMMAND_TOPIC,
   EFFECT_COMMAND_TOPIC,
   BRIGHTNESS_COMMAND_TOPIC,
   PICOWOTA_REBOOT_TOPIC,
} asMqtt_subTopics_enum;

static const char* subTopics[ NUMBER_OF_SUB_TOPICS ] = {
    [COMMAND_TOPIC] = COMMAND_TOPIC_STRING,
    [EFFECT_COMMAND_TOPIC] = EFFECT_COMMAND_TOPIC_STRING,
    [BRIGHTNESS_COMMAND_TOPIC] = BRIGHTNESS_COMMAND_TOPIC_STRING,
    [PICOWOTA_REBOOT_TOPIC] = PICOWOTA_REBOOT_TOPIC_STRING,
};

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

static TaskHandle_t MqttTaskHandle = NULL;

static mqtt_client_t* mqtt_client;

static ip4_addr_t brokerIP;

static char mqttData[ MAX_MQTT_MESSAGE_DATA ];

static char num_to_string_buf[ 10 ];

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
    .lightState = WS281X_LIGHT_OFF,
    .effect = WS281X_LIGHT_EFFECT_0,
    .brightness = 255,
};

asMqtt_subTopics_enum currentTopic;

static void mqtt_incoming_data_cb( void*, const uint8_t*, uint16_t, uint8_t );
static void mqtt_incoming_publish_cb( void*, const char*, uint32_t );
static void mqtt_request_cb( void*, err_t );
static void mqtt_connection_cb( mqtt_client_t*, void*,
                                mqtt_connection_status_t );
wifi_state connect_wifi();
void mqtt_client_connect_broker();

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
      for( uint32_t i = 0; i < WS281X_LIGHT_STATE_NUM; i++ )
      {
         if( strcmp( mqttData, WS281X_Light_State_Strings[ i ] ) == 0 )
         {
            currentLightState.lightState = i;
            break;
         }
      }

      mqtt_publish(
          mqtt_client, pubTopics[ STATE_TOPIC ],
          WS281X_Light_State_Strings[ currentLightState.lightState ],
          strlen( WS281X_Light_State_Strings[ currentLightState.lightState ] ),
          MQTT_QOS, true, mqtt_request_cb,
          LWIP_CONST_CAST( void*, &mqtt_client_info ) );

      break;
   case EFFECT_COMMAND_TOPIC:
      for( uint32_t i = 0; i < WS281X_LIGHT_EFFECT_NUM; i++ )
      {
         if( strcmp( mqttData, WS281X_Light_Effects[ i ].string ) == 0 )
         {
            currentLightState.effect = i;
         }
      }

      mqtt_publish(
          mqtt_client, pubTopics[ EFFECT_STATE_TOPIC ],
          WS281X_Light_Effects[ currentLightState.effect ].string,
          strlen( WS281X_Light_Effects[ currentLightState.effect ].string ),
          MQTT_QOS, true, mqtt_request_cb,
          LWIP_CONST_CAST( void*, &mqtt_client_info ) );

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
   case PICOWOTA_REBOOT_TOPIC:
      printf( "Rebooting into bootloader...\r\n" );
      vTaskDelay( 100 / portTICK_PERIOD_MS );
      if( strcmp( mqttData, "reboot" ) == 0 )
      {
         picowota_reboot( true );
      }
      break;
   default:
      break;
   }

   if( xQueueSend( WS281X_stateQueue, (void*)&currentLightState, 10 ) !=
       pdTRUE )
   {
      printf( "ERROR: could not put item in state queue\r\n" );
   }

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
   if( err != 0 )
   {
      const struct mqtt_connect_client_info_t* client_info =
          (const struct mqtt_connect_client_info_t*)arg;

      printf( "MQTT client \"%s\" request cb: err %d\r\n",
              client_info->client_id, (int)err );
   }
}

// callback for initial publish chain
static void mqtt_init_pub_chan_cb( void* arg, err_t err )
{
   asMqtt_pubTopics_enum prevTopic = (asMqtt_pubTopics_enum)arg;
   if( err == 0 )
   {
      switch( prevTopic )
      {
      case( AVAILABILITY_TOPIC ):
         mqtt_publish(
             mqtt_client, pubTopics[ STATE_TOPIC ],
             WS281X_Light_State_Strings[ currentLightState.lightState ],
             strlen(
                 WS281X_Light_State_Strings[ currentLightState.lightState ] ),
             MQTT_QOS, true, mqtt_init_pub_chan_cb,
             LWIP_CONST_CAST( void*, STATE_TOPIC ) );
         break;
      case( STATE_TOPIC ):
         mqtt_publish(
             mqtt_client, pubTopics[ EFFECT_STATE_TOPIC ],
             WS281X_Light_Effects[ currentLightState.effect ].string,
             strlen( WS281X_Light_Effects[ currentLightState.effect ].string ),
             MQTT_QOS, true, mqtt_init_pub_chan_cb,
             LWIP_CONST_CAST( void*, EFFECT_STATE_TOPIC ) );
         break;
      case( EFFECT_STATE_TOPIC ):
         itoa( currentLightState.brightness, num_to_string_buf,
               sizeof( num_to_string_buf ) / sizeof( char ) );

         mqtt_publish( mqtt_client, pubTopics[ BRIGHTNESS_STATE_TOPIC ],
                       num_to_string_buf, strlen( num_to_string_buf ), MQTT_QOS,
                       true, mqtt_init_pub_chan_cb,
                       LWIP_CONST_CAST( void*, BRIGHTNESS_STATE_TOPIC ) );
         break;

      case( BRIGHTNESS_STATE_TOPIC ):
         printf( "Pub chain complete...\r\n" );
         break;
      default:
         printf( "WARNING: Unknown state topic\r\n" );
      }
   }
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

wifi_state connect_wifi()
{
   if( cyw43_arch_wifi_connect_timeout_ms( WIFI_SSID, WIFI_PASSWORD,
                                           WIFI_SECURITY,
                                           WIFI_CONNECTION_TIMEOUT_MS ) )
   {
      printf( "ERROR: Failed to connet to wifi, SSID: '%s'\r\n", WIFI_SSID );
      return WIFI_DISCONNECTED;
   }

   return WIFI_CONNECTED;
}

void mqtt_client_connect_broker()
{
   mqtt_client_connect( mqtt_client, &brokerIP, MQTT_PORT, mqtt_connection_cb,
                        LWIP_CONST_CAST( void*, &mqtt_client_info ),
                        &mqtt_client_info );

   // wait for connection
   while( !mqtt_client_is_connected( mqtt_client ) )
   {
      vTaskDelay( 100 / portTICK_PERIOD_MS );
   }

   mqtt_publish( mqtt_client, pubTopics[ AVAILABILITY_TOPIC ], "online", 6,
                 MQTT_QOS, true, mqtt_init_pub_chan_cb,
                 LWIP_CONST_CAST( void*, AVAILABILITY_TOPIC ) );
}

void MqttTask( void* param )
{
   // check to make sure queue is initialized
   if( WS281X_stateQueue == NULL )
   {
      printf( "ERROR: WS281X_stateQueue not initialized\r\n" );
      vTaskDelete( NULL );
   }

   if( cyw43_arch_init_with_country( WIFI_COUNTRY_CODE ) )
   {
      printf( "Failed to initialize cyw43\r\n" );
      vTaskDelete( NULL );
   }

   // allow the led task to start now that cyw43 is initialized
   if( HeartbeatSetupSemaphore != NULL )
   {
      xSemaphoreGive( HeartbeatSetupSemaphore );
   }

   cyw43_arch_enable_sta_mode();

   printf( "Connection to wifi..\r\n" );

   while( connect_wifi() != WIFI_CONNECTED )
   {
      vTaskDelay( WIFI_CONNECTION_RETRY_TIME_MS / portTICK_PERIOD_MS );
   }

   printf( "WIFI CONNECTED\r\n" );

   // change led blink rate to slower when wifi connected
   uint32_t delayVal = 1000;
   xQueueSend( delay_queue, (void*)&delayVal, 10 );

   // TODO: add dns probably needs to use mdns
   ipaddr_aton( MQTT_SERVER_ADDR, &brokerIP );

   mqtt_client = mqtt_client_new();

   mqtt_set_inpub_callback( mqtt_client, mqtt_incoming_publish_cb,
                            mqtt_incoming_data_cb,
                            LWIP_CONST_CAST( void*, &mqtt_client_info ) );

   mqtt_client_connect_broker();

   while( true )
   {
      while( connect_wifi() != WIFI_CONNECTED )
      {
         vTaskDelay( WIFI_CONNECTION_RETRY_TIME_MS / portTICK_PERIOD_MS );
      }

      if( !mqtt_client_is_connected( mqtt_client ) )
      {
         mqtt_client_connect_broker();
      }

      vTaskDelay( CHECKUP_TIME_MS / portTICK_PERIOD_MS );
   }
}

uint32_t asMqtt_TaskSetup()
{
   return xTaskCreate( MqttTask, "Wifi Task", configMINIMAL_STACK_SIZE, NULL, 2,
                       &MqttTaskHandle );
}
