#include "terminal.h"
#include <stdlib.h>
#include "heartbeat.h"
#include "pico/stdlib.h"
#include <string.h>

static void delayCommand( char* );
static void printStats();
static void displayHelp();

TaskHandle_t TerminalTaskHandle = NULL;

char cmdBuff[ COMMAND_MESSAGE_LENGTH ];

const Command availCommands[] = {
    { .command = "delay",
      .length = 5,
      .callback = (void*)delayCommand,
      .description =
          "Changes led blink rate to a given value in ms. EX: delay 100" },
    { .command = "stat",
      .length = 4,
      .callback = (void*)printStats,
      .description = "Allows the printing of statics about what is running." },
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

   if( delay_queue == NULL )
   {
      printf( "ERROR: 'delay_queue' is not initialized.\r\n" );
      return;
   }

   if( xQueueSend( delay_queue, (void*)&val, 10 ) != pdTRUE )
   {
      printf( "ERROR: Could not put item in delay queue.\r\n" );
      return;
   }

   if( HeartbeatSemaphore == NULL )
   {
      printf( "ERROR: 'HeartbeatSemaphore' is not initialize.\r\n" );
      return;
   }

   xSemaphoreGive( HeartbeatSemaphore );
}
// helper functions for printStats
void getStateSting( eTaskState state, char* stateString )
{
   switch( state )
   {
   case( eRunning ):
      strcpy( stateString, "Running" );
      break;
   case( eReady ):
      strcpy( stateString, "Ready" );
      break;
   case( eBlocked ):
      strcpy( stateString, "Blocked" );
      break;
   case( eSuspended ):
      strcpy( stateString, "Suspened" );
      break;
   case( eDeleted ):
      strcpy( stateString, "Deleted" );
      break;
   case( eInvalid ):
      strcpy( stateString, "Invalid" );
      break;
   default:
      strcpy( stateString, "Unknown" );
   }
}

static inline uint32_t getUsagePercent( uint32_t time, uint32_t totalTime )
{
   return ( time * 100 ) / totalTime;
}

//prints stats for freertos
void printStats()
{
   UBaseType_t numTasks = uxTaskGetNumberOfTasks();
   TaskStatus_t* pTaskStatusArray =
       (TaskStatus_t*)pvPortMalloc( numTasks * sizeof( TaskStatus_t ) );
   uxTaskGetSystemState( pTaskStatusArray, numTasks, NULL );

   char stateString[ 15 ];

   uint32_t totalTime = 0;

   for( uint32_t i = 0; i < numTasks; i++ )
   {
      totalTime += pTaskStatusArray[ i ].ulRunTimeCounter;
   }

   printf( "\r\n\r\nNumber of Tasks: %d\r\n", numTasks );

   printf( "Total Runtime: %u\r\n", totalTime );

   printf( "NAME\t\t\tSTATE\t\t\tRUNTIME\t\t\t%%USAGE\t\t\tWATERMARK\r\n----\t\t\t-----\t\t\t-"
           "------\t\t\t------\t\t\t---------\r\n" );

   for( uint32_t i = 0; i < numTasks; i++ )
   {
      getStateSting( pTaskStatusArray[ i ].eCurrentState, stateString );

      printf( "%-15s\t\t%-15s\t\t%-15u\t\t%-15u\t\t%u\r\n",
              pTaskStatusArray[ i ].pcTaskName, stateString,
              pTaskStatusArray[ i ].ulRunTimeCounter,
              getUsagePercent( pTaskStatusArray[ i ].ulRunTimeCounter,
                               totalTime ), pTaskStatusArray[i].usStackHighWaterMark );
   }

   printf( "\r\n\r\n" );
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

// setups and creates the terminal task
uint32_t TerminalTaskSetup()
{
   // creates queue for passing values around
   delay_queue = xQueueCreate( DELAY_QUEUE_LEN, sizeof( uint32_t ) );

   return xTaskCreate( TerminalTask, "Terminal Task", configMINIMAL_STACK_SIZE,
                       NULL, 1, &TerminalTaskHandle );
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
