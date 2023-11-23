#ifndef WS2812_H
#define WS2812_H

#include "pico/stdlib.h"

#define WS2812_LIGHT_STATE_NUM 2

typedef enum
{
   WS2812_LIGHT_OFF = 0,
   WS2812_LIGHT_ON,
} WS2812_Light_State_Enum;

static char* const WS2812_Light_State_Strings[ WS2812_LIGHT_STATE_NUM ] = {
    [WS2812_LIGHT_OFF] = "OFF",
    [WS2812_LIGHT_ON] = "ON",
};

#define WS2812_LIGHT_EFFECT_NUM 1
typedef enum
{
   WS2812_LIGHT_EFFECT_1 = 0,
} WS2812_Light_Effect_Enum;

static char* const WS2812_Light_Effect_Strings[ WS2812_LIGHT_EFFECT_NUM ] = {
    [WS2812_LIGHT_EFFECT_1] = "Solid Blue",
};

typedef struct LightStateStruct
{
   WS2812_Light_State_Enum lightState;
   WS2812_Light_Effect_Enum effect;
   uint8_t brightness;
} LightState;

#endif
