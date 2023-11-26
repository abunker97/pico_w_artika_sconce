#ifndef ASMQTT_H
#define ASMQTT_H

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "cyw43_country.h"

// allows for wifi info to be changed at compile time

#define WIFI_COUNTRY_CODE CYW43_COUNTRY_USA

#define WIFI_SSID "ATB"

#define WIFI_PASSWORD "Aust1nBunk3r6545"

#define WIFI_SECURITY CYW43_AUTH_WPA2_AES_PSK

#define WIFI_CONNECTION_TIMEOUT_MS 30000

#define WIFI_CONNECTION_RETRY_TIME_MS 5000

#define MQTT_SERVER_ADDR "192.168.1.75"

#define MQTT_CLIENT_NAME "artika_sconce"

#define MQTT_USERNAME "austin"

#define MQTT_PASSWORD "Peyton@1997"

#define MQTT_KEEPALIVE_S 100

#define MQTT_QOS 1

#define MAX_MQTT_MESSAGE_DATA 50

#define CHECKUP_TIME_MS 30000

#define PICOWOTA_REBOOT_COMMAND "reboot"

//
// SUB TOPICS
// these also need to be put into the .c file
//

#define NUMBER_OF_SUB_TOPICS 4
#define COMMAND_TOPIC_STRING "electronics_room_sconce/command_topic"
#define EFFECT_COMMAND_TOPIC_STRING "electronics_room_sconce/effect_command_topic"
#define BRIGHTNESS_COMMAND_TOPIC_STRING "electronics_room_sconce/brightness_command_topic"
#define PICOWOTA_REBOOT_TOPIC_STRING "electronics_room_sconce/picowota_reboot_topic"

//
// PUB TOPICS
// these also need to added to the .c file
//

#define NUMBER_OF_PUB_TOPICS 4
#define AVAILABILITY_TOPIC_STRING "electronics_room_sconce/availability_topic"
#define STATE_TOPIC_STRING "electronics_room_sconce/state_topic"
#define EFFECT_STATE_TOPIC_STRING "electronics_room_sconce/effect_state_topic"
#define BRIGHTNESS_STATE_TOPIC_STRING "electronics_room_sconce/brightness_state_topic"

uint32_t asMqtt_TaskSetup();

#endif
