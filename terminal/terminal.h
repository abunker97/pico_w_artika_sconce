#ifndef TERMINAL_H
#define TERMINAL_H

#include <stdio.h>
#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "task.h"
#include "queue.h"

#define COMMAND_MESSAGE_LENGTH     20
#define COMMAND_DESCRIPTION_LENGTH 100

#define DELAY_QUEUE_LEN 5

extern TaskHandle_t TerminalTaskHandle;

typedef struct Command
{
   char command[ COMMAND_MESSAGE_LENGTH ];
   uint8_t length;
   void* ( *callback )( char* );
   char description[ COMMAND_DESCRIPTION_LENGTH ];
} Command;

extern char cmdBuff[ COMMAND_MESSAGE_LENGTH ];

uint32_t TerminalTaskSetup();

void TerminalTask( void* );

#endif
