#ifndef WS2812_H
#define WS2812_H

#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "abtypes.h"
#include "pico/stdlib.h"
#include "queue.h"
#include "semphr.h"
#include "task.h"

#define WS281X_NUM_LEDS 47

#define WS281X_LIGHT_STATE_NUM 2

#define WS281X_QUEUE_LEN 10

typedef enum {
  WS281X_LIGHT_OFF = 0,
  WS281X_LIGHT_ON,
} WS281X_Light_State_Enum;

static char *const WS281X_Light_State_Strings[WS281X_LIGHT_STATE_NUM] = {
    [WS281X_LIGHT_OFF] = "OFF",
    [WS281X_LIGHT_ON] = "ON",
};

#define WS281X_LIGHT_EFFECT_NUM 5
typedef enum {
  WS281X_LIGHT_EFFECT_0 = 0,
  WS281X_LIGHT_EFFECT_1,
  WS281X_LIGHT_EFFECT_2,
  WS281X_LIGHT_EFFECT_3,
  WS281X_LIGHT_EFFECT_4,
} WS2812_Light_Effect_Enum;

typedef struct lightEffect {
  char *string;
  void *(*function)();
} LightEffect;

extern const LightEffect WS281X_Light_Effects[WS281X_LIGHT_EFFECT_NUM];

typedef struct LightStateStruct {
  WS281X_Light_State_Enum lightState;
  WS2812_Light_Effect_Enum effect;
  uint8_t brightness;
} LightState;

extern TaskHandle_t WS281X_taskHandle;

extern QueueHandle_t WS281X_stateQueue;

u32 WS281X_taskSetup();
#endif
