#ifndef ASMQTT_H
#define ASMQTT_H

#include "pico/stdlib.h"

// allows for wifi info to be changed at compile time
#define WIFI_SSID "ATB"

#define WIFI_PASSWORD "Aust1nBunk3r6545"

#define MQTT_CLIENT_NAME "artika_sconce"

#define MQTT_USERNAME "austin"

#define MQTT_PASSWORD "Peyton@1997"

#define MQTT_KEEPALIVE_S 100

#define MQTT_QOS 1

#define MAX_MQTT_MESSAGE_DATA 50

//
// SUB TOPICS
// these also need to be put into the .c file
//

#define COMMAND_TOPIC_STRING "test_light/command_topic"
#define EFFECT_COMMAND_TOPIC_STRING "test_light/effect_command_topic"
#define BRIGHTNESS_COMMAND_TOPIC_STRING "test_light/brightness_command_topic"

//
// PUB TOPICS
// these also need to added to the .c file
//

#define AVAILABILITY_TOPIC_STRING "test_light/availability_topic"
#define STATE_TOPIC_STRING "test_light/state_topic"
#define EFFECT_STATE_TOPIC_STRING "test_light/effect_state_topic"
#define BRIGHTNESS_STATE_TOPIC_STRING "test_light/brightness_state_topic"

uint32_t asMqtt_TaskSetup();

#endif
