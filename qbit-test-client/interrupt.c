//
//  interrupt.c
//  utility for qbit websocket client
//
//  Created by Bandoian, Terence (CONT) on 6/28/15.
//
//  Copyright (c) 2015 PushPoint Mobile.
//
//  Licensed under the Apache License, Version 2.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.
//


#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>

#include "interrupt.h"


/* utility */

#define TRUE 1
#define FALSE 0


/* interrupt status */

static int wasInterrupted = FALSE;


/* functions */

static void interruptHandler( int signum );


/* enableInterrupt */

void enableInterrupt() {
    struct sigaction action;
    struct sigaction ignorePreviousAction;

    memset( &action, 0, sizeof( action ) );
    action.sa_handler = interruptHandler;

    sigaction( SIGINT, &action, &ignorePreviousAction );
}


/* hasInterruptOccurred */

int hasInterruptOccurred() {
    return wasInterrupted;
}


/* interruptHandler */

static void interruptHandler( int signum ) {
    wasInterrupted = TRUE;
    write( STDOUT_FILENO, "\n", 1 );
}
