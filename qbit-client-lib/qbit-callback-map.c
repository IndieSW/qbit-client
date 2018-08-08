//
//  qbit-callback-map.c
//  QBit WebSocket Client
//
//  Created by Bandoian, Terence (CONT) on 7/18/15.
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
#include <pthread.h>

#include <lws_config.h>
#include <libwebsockets.h>
#include <uthash.h>

#include "qbit-api.h"
#include "qbit-common.h"

#include "qbit-callback-map.h"
#include "qbit-callback-map.hp"


/* initQbitCallbackMap */

void initQbitCallbackMap() {
    termQbitCallbackMap();
}


/* termQbitCallbackMap */

void termQbitCallbackMap() {
    CALLBACK_MAP_ITEM * item;
    CALLBACK_MAP_ITEM * tmp;

    pthread_mutex_lock( &callbackMapMutex );

#if defined( QBIT_CALLBACK_TEST )
    if ( maxCallbackMapSize != 0 ) {
        unsigned int size = HASH_COUNT( callbackMap );
        printf( "\nFreeing callback map with size %u and max size %u\n",
            size, maxCallbackMapSize );
    }
#endif

    HASH_ITER( hh, callbackMap, item, tmp ) {
        HASH_DEL( callbackMap, item );
        free( item );
    }

#if defined( QBIT_CALLBACK_TEST )
    maxCallbackMapSize = 0;
#endif

    pthread_mutex_unlock( &callbackMapMutex );
}


/* addToQbitCallbackMap */

int addToQbitCallbackMap(
    int * callbackId, QBIT_MESSAGE_CALLBACK * callback, void * context )
{
    CALLBACK_MAP_ITEM * item;
    int status;

    item = NULL;
    status = 0;

    pthread_mutex_lock( &callbackMapMutex );

    if ( status == 0 ) {
        do {
            *callbackId = getQbitCallbackId();
            HASH_FIND_INT( callbackMap, callbackId, item );
        }
        while ( item != NULL );
    }

    if ( status == 0 ) {
        item = (CALLBACK_MAP_ITEM *)malloc( sizeof( CALLBACK_MAP_ITEM ) );

        if ( item == NULL ) {
            lwsl_err( "Unable to allocate callback map item.\n" );
            status = -1;
        }
    }

    if ( status == 0 ) {
        item->callbackId = *callbackId;
        item->callback = callback;
        item->context = context;

        HASH_ADD_INT( callbackMap, callbackId, item );

#if defined( QBIT_CALLBACK_TEST )
    unsigned int size = HASH_COUNT( callbackMap );
    maxCallbackMapSize = MAX( size, maxCallbackMapSize );
#endif
    }

    pthread_mutex_unlock( &callbackMapMutex );

    return status;
}


/* removeFromQbitCallbackMap */

int removeFromQbitCallbackMap(
    int callbackId, QBIT_MESSAGE_CALLBACK ** callback, void ** context )
{
    CALLBACK_MAP_ITEM * item;
    int status;

    item = NULL;
    status = 0;

    pthread_mutex_lock( &callbackMapMutex );

    HASH_FIND_INT( callbackMap, &callbackId, item );

    if ( item == NULL ) {
        lwsl_err( "Unable to find callback %d.\n", callbackId );
        status = -1;
    }
    else {
        HASH_DEL( callbackMap, item );
    }

    pthread_mutex_unlock( &callbackMapMutex );

    if ( status == 0 ) {
        *callback = item->callback;
        *context = item->context;

        free( item );
    }

    return status;
}


/* getQbitCallbackId */

static uint32_t getQbitCallbackId() {
    uint32_t callbackId;

    callbackId = nextCallbackId++;

    if ( nextCallbackId == 0 ) {
        nextCallbackId = 1;
    }

    return callbackId;
}





