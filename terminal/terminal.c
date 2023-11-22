#include "terminal.h"
#include <stdlib.h>
#include "heartbeat.h"
#include "pico/stdlib.h"
#include <string.h>

static void delayCommand( char* );
static void displayHelp();

TaskHandle_t TerminalTaskHandle = NULL;

char cmdBuff[ COMMAND_MESSAGE_LENGTH ];

const Command availCommands[] = {
    { .command = "delay",
      .length = 5,
      .callback = (void*)delayCommand,
      .description =
          "Changes led blink rate to a given value in ms. EX: delay 100" },
    { .command = "help",
      .length = 4,
      .callback = (void*)displayHelp,
      .description = "This help menu." } };


// delay command callback
void delayCommand( char* cmd )
{
   uint32_t val = atoi( cmd + 6 );

   if( val <= 0 )
   {
      printf( "Please enter a value greater than 0.\r\n" );
      return;
   }

   if( xQueueSend( delay_queue, (void*)&val, 10 ) != pdTRUE )
   {
      printf( "ERROR: Could not put item in delay queue.\r\n" );
      return;
   }

   xSemaphoreGive( HeartbeatSemaphore );
}

// display the help command
void displayHelp()
{
   printf( "\r\n\r\nPOSSIBLE COMMANDS:\r\n" );
   for( int i = 0; i < sizeof( availCommands ) / sizeof( Command ); i++ )
   {
      printf( "%s\r\n\t%s\r\n", availCommands[ i ].command,
              availCommands[ i ].description );
   }
}

// task that handles reading user input from the terminal
void TerminalTask( void* param )
{
   char currentChar = 0;
   uint8_t pos = 0;

   printf( "Type 'help' for a list of possible commands\r\n" );

   while( 1 )
   {
      if( uart_is_readable( uart0 ) )
      {
         currentChar = uart_getc( uart0 );

         // handle backspace
         if( currentChar == 8 )
         {
            cmdBuff[ pos ] = '\0';
            if( pos > 0 )
            {
               pos--;
            }
         }
         else if( currentChar != '\r' && ( pos < COMMAND_MESSAGE_LENGTH - 1 ) )
         {
            cmdBuff[ pos ] = currentChar;
            pos++;
         }
         else
         {
            // echo back command
            printf( "CMD: %s \r\n", cmdBuff );

            bool commandFound = false;

            // check for available commands
            for( uint8_t i = 0; i < sizeof( availCommands ) / sizeof( Command );
                 i++ )
            {
               if( memcmp( cmdBuff, availCommands[ i ].command,
                           availCommands[ i ].length ) == 0 )
               {
                  commandFound = true;
                  availCommands[ i ].callback( cmdBuff );
               }
            }

            if( !commandFound )
            {
               printf( "Unknown Command. Type 'help' to see a list of "
                       "available commands.\r\n" );
            }

            // clear and reset buffer pos
            memset( cmdBuff, '\0', COMMAND_MESSAGE_LENGTH );
            pos = 0;
         }
      }
      else
      {
         // wait a second if no data in uart
         vTaskDelay( 1000 / portTICK_PERIOD_MS );
      }
   }
}
