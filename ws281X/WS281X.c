#include "WS281X.h"
#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "pinout.h"
#include "abtypes.h"
#include "ws2812.pio.h"
#include "hardware/dma.h"

static void WS281X_task();
static void showStrip();
static inline u32 getCurrTime();
static inline u32 urgbU32( u8, u8, u8 );
static inline u32 randomNumber( u32 min, u32 max );
static void switchCurrStrip();
static void powerOffStrip();
static void powerOnStrip();
static void clearStrip();
static void keepStripOff();

static void solidBlue();
static void chasingGreen();
static void ocean();
static void torch();
static void wreath();

static u8 coolDown( u8, u8, u32 );
static inline u8 scale8_video( u8, u8 );
static u32 waterColor( u8 );
static u32 heatColor( u8 );

static PIO pio = pio0;
static u8 sm = 0;
static u32 strip0[ WS281X_NUM_LEDS ];
static u32 strip1[ WS281X_NUM_LEDS ];
static u32* currentStrip = strip0;
static u32* previousStrip = strip1;
static u8 dma_chan;
static WS281X_Light_State_Enum previous_state = WS281X_LIGHT_OFF;
static LightState current_state = { .lightState = WS281X_LIGHT_OFF,
                                    .effect = WS281X_LIGHT_EFFECT_1,
                                    .brightness = 255 };
static LightState recieved_state;
static u32 ms_since_last_update;

QueueHandle_t WS281X_stateQueue = NULL;

TaskHandle_t WS281X_taskHandle = NULL;

const LightEffect WS281X_Light_Effects[ WS281X_LIGHT_EFFECT_NUM ] = {
    [WS281X_LIGHT_EFFECT_0] = { .string = "Solid Blue",
                                .function = (void*)solidBlue },
    [WS281X_LIGHT_EFFECT_1] = { .string = "Chasing Green",
                                .function = (void*)chasingGreen },
    [WS281X_LIGHT_EFFECT_2] = { .string = "Ocean", .function = (void*)ocean },
    [WS281X_LIGHT_EFFECT_3] = { .string = "Torch", .function = (void*)torch },
    [WS281X_LIGHT_EFFECT_4] = { .string = "Wreath", .function = (void*)wreath },
};

u32 WS281X_taskSetup()
{
   // enable level shift pin
   gpio_init( LEVEL_SHIFT_ENABLE_PIN );
   gpio_set_dir( LEVEL_SHIFT_ENABLE_PIN, GPIO_OUT );
   gpio_put( LEVEL_SHIFT_ENABLE_PIN, true );

   // power for WS281x
   gpio_init( WS281X_PWR_PIN );
   gpio_set_dir( WS281X_PWR_PIN, GPIO_OUT );
   gpio_put( WS281X_PWR_PIN, true );

   u32 offset = pio_add_program( pio, &ws2812_program );

   ws2812_program_init( pio, sm, offset, WS281X_PIN, 800000, false );

   dma_chan = dma_claim_unused_channel( true );
   dma_channel_config c = dma_channel_get_default_config( dma_chan );
   channel_config_set_transfer_data_size( &c, DMA_SIZE_32 );
   channel_config_set_read_increment( &c, true );
   channel_config_set_write_increment( &c, false );
   channel_config_set_dreq( &c, pio_get_dreq( pio, sm, true ) );

   dma_channel_configure(
       dma_chan, &c,
       &pio->txf[ sm ], // Write address (only need to set this once)
       NULL,            // Don't provide a read address yet
       WS281X_NUM_LEDS, // Write the same value many times, then halt and
                        // u32errupt
       false            // Don't start yet
   );

   WS281X_stateQueue = xQueueCreate( WS281X_QUEUE_LEN, sizeof( LightState ) );

   return xTaskCreate( WS281X_task, "WS281X Task", configMINIMAL_STACK_SIZE,
                       NULL, 3, &WS281X_taskHandle );
}

void WS281X_task()
{

   while( 1 )
   {
      if( xQueueReceive( WS281X_stateQueue, (void*)&recieved_state, 0 ) ==
          pdTRUE )
      {
         current_state.lightState = recieved_state.lightState;
         current_state.effect = recieved_state.effect;
         current_state.brightness = recieved_state.brightness;
      }

      if( current_state.lightState != previous_state )
      {
         if( current_state.lightState == WS281X_LIGHT_OFF )
         {
            printf( "Turning strip off\r\n" );
            powerOffStrip();
         }
         else
         {
            printf( "Turning strip on\r\n" );
            powerOnStrip();
            ms_since_last_update = 0;
         }

         previous_state = current_state.lightState;
      }

      if( current_state.lightState == WS281X_LIGHT_ON )
      {

         WS281X_Light_Effects[ current_state.effect ].function();
      }

      else
      {
         // dirty workaround because turning power off to the strip messes with wifi
         keepStripOff();
      }

      vTaskDelay( 10 / portTICK_PERIOD_MS );
   }
}

void showStrip()
{
   // check if dma is busy and then wait for it to finish.
   // Also need to delay otherwise leds get hung.
   // NOTE: there is a certain window of time that if the dma finishes right
   //       before reaching this point the leds will also hang
   if( dma_channel_is_busy( dma_chan ) )
   {
      dma_channel_wait_for_finish_blocking( dma_chan );
      vTaskDelay( 2 / portTICK_PERIOD_MS );
   }

   dma_channel_set_read_addr( dma_chan, currentStrip, true );
   switchCurrStrip();
}

static inline u32 getCurrTime()
{
   return to_ms_since_boot( get_absolute_time() );
}

static inline u32 urgbU32( u8 r, u8 g, u8 b )
{
   return ( (u32)( ( r * current_state.brightness ) / 255 ) << 16 ) |
          ( (u32)( ( g * current_state.brightness ) / 255 ) << 24 ) |
          ( (u32)( ( b * current_state.brightness ) / 255 ) << 8 );
}

static inline u32 randomNumber( u32 min, u32 max )
{
   return rand() % ( max - min + 1 );
}

void switchCurrStrip()
{
   if( currentStrip == strip0 )
   {
      currentStrip = strip1;
      previousStrip = strip0;
   }
   else
   {
      currentStrip = strip0;
      previousStrip = strip1;
   }
}

void powerOffStrip()
{
   clearStrip();
   //gpio_put( WS281X_PWR_PIN, false );
}

void powerOnStrip()
{
   gpio_put( WS281X_PWR_PIN, true );
   gpio_put( WS281X_PWR_PIN, true );
}

void clearStrip()
{
   for( u32 i = 0; i < WS281X_NUM_LEDS; i++ )
   {
      currentStrip[ i ] = urgbU32( 0, 0, 0 );
   }

   showStrip();

   dma_channel_wait_for_finish_blocking( dma_chan );
}

void keepStripOff()
{
   if( getCurrTime() - ms_since_last_update > 30000 )
   {
      clearStrip();
      ms_since_last_update = getCurrTime();
   }
}

void solidBlue()
{
   static bool newFrameNeeded = true;

   if( newFrameNeeded )
   {
      for( u32 i = 0; i < WS281X_NUM_LEDS; i++ )
      {
         currentStrip[ i ] = urgbU32( 0, 0, 255 );
      }
      newFrameNeeded = false;
   }

   if( getCurrTime() - ms_since_last_update > 100 )
   {
      showStrip();
      ms_since_last_update = getCurrTime();
      newFrameNeeded = true;
   }
}

void chasingGreen()
{
   static u32 headLoc = 0;
   static bool newFrameNeeded = true;
   static const u32 tailLength = 5;

   if( newFrameNeeded )
   {
      for( u32 i = 0; i < WS281X_NUM_LEDS; i++ )
      {
         if( ( i <= headLoc && i >= headLoc - tailLength ) ||
             ( headLoc < tailLength &&
               i > WS281X_NUM_LEDS + headLoc - tailLength ) ||
             ( headLoc < tailLength && i < headLoc ) )
         {
            currentStrip[ i ] = urgbU32( 0, 255, 0 );
         }
         else
         {
            currentStrip[ i ] = urgbU32( 0, 0, 255 );
         }
      }

      if( headLoc >= WS281X_NUM_LEDS )
      {
         headLoc = 0;
      }
      else
      {
         headLoc++;
      }

      newFrameNeeded = false;
   }

   if( getCurrTime() - ms_since_last_update > 50 )
   {
      newFrameNeeded = true;
      showStrip();
      ms_since_last_update = getCurrTime();
   }
}

// Based on: https://www.youtube.com/watch?v=MauVVTJb2tk
void ocean()
{
   static bool newFrameNeeded = true;

   static const u8 blendSelf = 2;
   static const u8 blendNeighbor1 = 3;
   static const u8 blendNeighbor2 = 2;
   static const u8 blendNeighbor3 = 1;
   static const u8 blendTotal =
       ( blendSelf + blendNeighbor1 + blendNeighbor2 + blendNeighbor3 );

   static u32 cooling = 15;
   static u8 sparking = 150;
   static u8 sparks = 5;
   static u8 sparkHeight = 2;
   static bool reversed = true;
   static bool mirrored = true;
   static u32 size = WS281X_NUM_LEDS / 2 +
                     1; // Add one because there is an odd number of lights

   static u8 heatValues[ WS281X_NUM_LEDS / 2 + 1 ] = { 0 }; // set to size

   if( newFrameNeeded )
   {
      // First cool each cell by a little bit
      for( u32 i = 0; i < size; i++ )
      {
         heatValues[ i ] = coolDown( heatValues[ i ], cooling, size );
      }

      // Next drift heat up and diffuse it a little but
      for( u32 i = 0; i < size; i++ )
         heatValues[ i ] = ( heatValues[ i ] * blendSelf +
                             heatValues[ ( i + 1 ) % size ] * blendNeighbor1 +
                             heatValues[ ( i + 2 ) % size ] * blendNeighbor2 +
                             heatValues[ ( i + 3 ) % size ] * blendNeighbor3 ) /
                           blendTotal;

      // Randomly ignite new sparks down in the flame kernel
      for( u32 i = 0; i < sparks; i++ )
      {
         if( randomNumber( 0, 255 ) < sparking )
         {
            u32 y = size - 1 - randomNumber( 0, sparkHeight );
            heatValues[ y ] =
                heatValues[ y ] +
                randomNumber( 160,
                              255 ); // This randomly rolls over sometimes of
            // course, and that's essential to the effect
         }

         // Finally convert heat to a color
         for( u32 i = 0; i < size; i++ )
         {
            u32 color = waterColor( heatValues[ i ] );
            u32 j = reversed ? ( size - 1 - i ) : i;
            currentStrip[ j ] = color;
            if( mirrored )
            {
               u32 j2 = !reversed ? ( 2 * size - 1 - i ) : size + i;
               currentStrip[ j2 ] = color;
            }
         }
      }

      newFrameNeeded = false;
   }

   if( getCurrTime() - ms_since_last_update > 90 )
   {
      newFrameNeeded = true;
      showStrip();
      ms_since_last_update = getCurrTime();
   }
}

// Based on: https://www.youtube.com/watch?v=MauVVTJb2tk
void torch()
{
   static bool newFrameNeeded = true;

   static const u8 blendSelf = 2;
   static const u8 blendNeighbor1 = 3;
   static const u8 blendNeighbor2 = 2;
   static const u8 blendNeighbor3 = 1;
   static const u8 blendTotal =
       ( blendSelf + blendNeighbor1 + blendNeighbor2 + blendNeighbor3 );

   static u32 cooling = 15;
   static u8 sparking = 150;
   static u8 sparks = 5;
   static u8 sparkHeight = 2;
   static bool reversed = true;
   static bool mirrored = true;
   static u32 size = WS281X_NUM_LEDS / 2 +
                     1; // Add one because there is an odd number of lights

   static u8 heatValues[ WS281X_NUM_LEDS / 2 + 1 ] = { 0 }; // set to size

   if( newFrameNeeded )
   {
      // First cool each cell by a little bit
      for( u32 i = 0; i < size; i++ )
      {
         heatValues[ i ] = coolDown( heatValues[ i ], cooling, size );
      }

      // Next drift heat up and diffuse it a little but
      for( u32 i = 0; i < size; i++ )
         heatValues[ i ] = ( heatValues[ i ] * blendSelf +
                             heatValues[ ( i + 1 ) % size ] * blendNeighbor1 +
                             heatValues[ ( i + 2 ) % size ] * blendNeighbor2 +
                             heatValues[ ( i + 3 ) % size ] * blendNeighbor3 ) /
                           blendTotal;

      // Randomly ignite new sparks down in the flame kernel
      for( u32 i = 0; i < sparks; i++ )
      {
         if( randomNumber( 0, 255 ) < sparking )
         {
            u32 y = size - 1 - randomNumber( 0, sparkHeight );
            heatValues[ y ] =
                heatValues[ y ] +
                randomNumber( 160,
                              255 ); // This randomly rolls over sometimes of
            // course, and that's essential to the effect
         }

         // Finally convert heat to a color
         for( u32 i = 0; i < size; i++ )
         {
            u32 color = heatColor( heatValues[ i ] );
            u32 j = reversed ? ( size - 1 - i ) : i;
            currentStrip[ j ] = color;
            if( mirrored )
            {
               u32 j2 = !reversed ? ( 2 * size - 1 - i ) : size + i;
               currentStrip[ j2 ] = color;
            }
         }
      }

      newFrameNeeded = false;
   }

   if( getCurrTime() - ms_since_last_update > 30 )
   {
      newFrameNeeded = true;
      showStrip();
      ms_since_last_update = getCurrTime();
   }
}

void wreath()
{
   static u8 lightPobability = 50;
   static bool newFrameNeeded = true;
   static bool blockRed = false;

   if( newFrameNeeded )
   {
      for( u32 i = 0; i < WS281X_NUM_LEDS; i++ )
      {
         if( !blockRed && ( randomNumber( 0, 100 ) < lightPobability ) )
         {
            currentStrip[ i ] = urgbU32( 255, 0, 0 );
            blockRed = true;
         }
         else
         {
            currentStrip[ i ] = urgbU32( 0, 255, 0 );
            blockRed = false;
         }
      }

      newFrameNeeded = false;
   }

   if( getCurrTime() - ms_since_last_update > 1000 )
   {
      newFrameNeeded = true;
      showStrip();
      ms_since_last_update = getCurrTime();
   }
}

u8 coolDown( u8 heatValue, u8 coolDownValue, u32 stripSize )
{
   // MAX(0L, heatValues[i] - randomNumber(0, ((cooling * 10) / size) + 2));
   uint32_t coolingByValue =
       randomNumber( 0, ( ( coolDownValue * 10 ) / stripSize ) ) + 2;

   if( coolingByValue > 255 )
   {
      coolingByValue = 255;
   }

   if( heatValue <= coolingByValue )
   {
      return 0;
   }

   return heatValue - coolingByValue;
}

static inline u8 scale8_video( u8 i, u8 scale )
{
   return ( ( (i32)i * (i32)scale ) >> 8 ) + ( ( i && scale ) ? 1 : 0 );
}

u32 waterColor( u8 temperature )
{
   u32 heatColor;

   // Scale 'heat' down from 0-255 to 0-191,
   // which can then be easily divided into three
   // equal 'thirds' of 64 units each.
   u8 t192 = scale8_video( temperature, 191 );

   // calculate a value that ramps up from
   // zero to 255 in each 'third' of the scale.
   u8 heatRamp = t192 & 0x3F; // 0..63
   heatRamp <<= 2;            // scale up to 0..252

   // now figure out which third of the spectrum we're in:
   if( t192 & 0x80 )
   {
      // we're in the hottest third
      heatColor = urgbU32( heatRamp, 255, 255 );
   }
   else if( t192 & 0x40 )
   {
      // we're in the middle third
      heatColor = urgbU32( 0, heatRamp, 255 );
   }
   else
   {
      // we're in the coolest third
      heatColor = urgbU32( 0, 0, heatRamp );
   }

   return heatColor;
}

u32 heatColor( u8 temperature )
{
   u32 heatColor;

   // Scale 'heat' down from 0-255 to 0-191,
   // which can then be easily divided into three
   // equal 'thirds' of 64 units each.
   u8 t192 = scale8_video( temperature, 191 );

   // calculate a value that ramps up from
   // zero to 255 in each 'third' of the scale.
   u8 heatRamp = t192 & 0x3F; // 0..63
   heatRamp <<= 2;            // scale up to 0..252

   // now figure out which third of the spectrum we're in:
   if( t192 & 0x80 )
   {
      // we're in the hottest third
      heatColor = urgbU32( 255, 255, heatRamp );
   }
   else if( t192 & 0x40 )
   {
      // we're in the middle third
      heatColor = urgbU32( 255, heatRamp, 0 );
   }
   else
   {
      // we're in the coolest third
      heatColor = urgbU32( heatRamp, 0, 0 );
   }

   return heatColor;
}
