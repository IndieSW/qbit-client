//
//  qbit-protocol-receive.c
//  QBit WebSocket Client
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
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <sys/time.h>

#include <pthread.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <semaphore.h>
#include <errno.h>

#include <lws_config.h>
#include <libwebsockets.h>

#include "qbit-api.h"
#include "qbit-common.h"
#include "qbit-callback-map.h"
#include "qbit-queue.h"
#include "qbit-thread.h"

#include "qbit-protocol-receive.h"
#include "qbit-protocol-receive.hp"


/* initQbitReceive */

int initQbitReceive(
    QBIT_RECEIVE_CONTEXT * qbitReceiveContext )
{
    int status = 0;

    qbitReceiveContext->webSocketContext = NULL;
    qbitReceiveContext->webSocket = NULL;

    initQbitReceiveBuf( &qbitReceiveContext->receiveBuf );

    if ( initQbitReceiveQueue( &qbitReceiveContext->queue ) != 0 ) {
        status = -1;
    }

    if ( initQbitReceiveThread( qbitReceiveContext ) != 0 ) {
        status = -1;
    }

    return status;
}


/* termQbitReceive */

void termQbitReceive(
    QBIT_RECEIVE_CONTEXT * qbitReceiveContext )
{
    termQbitReceiveThread( &qbitReceiveContext->thread );
    termQbitReceiveQueue( &qbitReceiveContext->queue );
    freeQbitReceiveBuf( &qbitReceiveContext->receiveBuf );
}


/* startQbitReceive */

void startQbitReceive(
    QBIT_RECEIVE_CONTEXT * qbitReceiveContext,
    struct libwebsocket_context * webSocketContext,
    struct libwebsocket * webSocket )
{
    qbitReceiveContext->webSocketContext = webSocketContext;
    qbitReceiveContext->webSocket = webSocket;
}


/* stopQbitReceive */

void stopQbitReceive(
    QBIT_RECEIVE_CONTEXT * qbitReceiveContext )
{
    freeQbitReceiveBuf( &qbitReceiveContext->receiveBuf );
    freeQbitQueue( &qbitReceiveContext->queue );
}


/* receiveQbitMessage */

int receiveQbitMessage(
    QBIT_RECEIVE_CONTEXT * qbitReceiveContext, void * in, size_t len )
{
    QBIT_QUEUE_ITEM * receiveBuf;
    QBIT_QUEUE_ITEM * item;
    QBIT_RECEIVE_QUEUE_ITEM_DATA * itemData;

    receiveBuf = qbitReceiveContext->receiveBuf;

    if ( receiveBuf == NULL ) {

        item = malloc(
            sizeof( QBIT_QUEUE_ITEM )
            + sizeof( QBIT_RECEIVE_QUEUE_ITEM_DATA )
            + len );

        if ( item == NULL ) {
            lwsl_err( "Unable to allocate receive buffer.\n" );
            return -1;
        }

        itemData = (QBIT_RECEIVE_QUEUE_ITEM_DATA *)item->data;

        itemData->buf = (char *)item
            + sizeof( QBIT_QUEUE_ITEM )
            + sizeof( QBIT_RECEIVE_QUEUE_ITEM_DATA );

        memcpy( itemData->buf, in, len );
        itemData->len = (int)len;

        qbitReceiveContext->receiveBuf = item;
    }
    else {

        itemData = (QBIT_RECEIVE_QUEUE_ITEM_DATA *)receiveBuf->data;

        item = realloc( receiveBuf,
            sizeof( QBIT_QUEUE_ITEM )
            + sizeof( QBIT_RECEIVE_QUEUE_ITEM_DATA )
            + itemData->len
            + len );

        if ( item == NULL ) {
            lwsl_err( "Unable to allocate receive buffer.\n" );
            freeQbitReceiveBuf( &qbitReceiveContext->receiveBuf );
            return -1;
        }

        itemData = (QBIT_RECEIVE_QUEUE_ITEM_DATA *)item->data;

        itemData->buf = (char *)item
            + sizeof( QBIT_QUEUE_ITEM )
            + sizeof( QBIT_RECEIVE_QUEUE_ITEM_DATA );

        memcpy( itemData->buf + itemData->len, in, len );
        itemData->len += (int)len;

        qbitReceiveContext->receiveBuf = item;
    }

    if ( libwebsockets_remaining_packet_payload(
            qbitReceiveContext->webSocket ) != 0 ) {
        return 0;
    }

    enqueueQbitQueueItem(
        &qbitReceiveContext->queue,
        qbitReceiveContext->receiveBuf );

    qbitReceiveContext->receiveBuf = NULL;

    sem_post( qbitReceiveContext->thread.sem );

    return 1;
}


/* initQbitReceiveBuf */

static void initQbitReceiveBuf( QBIT_QUEUE_ITEM ** receiveBuf ) {
    *receiveBuf = NULL;
}


/* freeQbitReceiveBuf */

static void freeQbitReceiveBuf( QBIT_QUEUE_ITEM ** receiveBuf ) {
    if ( *receiveBuf ) {
        free( *receiveBuf );
    }

    *receiveBuf = NULL;
}


/* initQbitReceiveQueue */

static int initQbitReceiveQueue( QBIT_QUEUE * queue ) {

    if ( initQbitQueue( queue ) != 0 ) {

        lwsl_err( "Unable to initialize receive queue: %d: %s.\n",
            errno, strerror( errno ) );

        return -1;
    }

    return 0;
}


/* termQbitReceiveQueue */

static void termQbitReceiveQueue( QBIT_QUEUE * queue ) {
    termQbitQueue( queue );
}


/* initQbitReceiveThread */

static int initQbitReceiveThread( QBIT_RECEIVE_CONTEXT * context ) {

    QBIT_RECEIVE_THREAD * thread = &context->thread;

    memset( thread->semName, 0, QBIT_RECEIVE_THREAD_SEM_NAME_LEN );

    strncpy( thread->semName,
        QBIT_RECEIVE_THREAD_SEM_NAME_PREFIX,
        QBIT_RECEIVE_THREAD_SEM_NAME_MAX_PREFIX_LEN - 1 );

    thread->sem = NULL;
    thread->id = 0;

    sem_unlink( thread->semName );

    thread->sem = sem_open(
        thread->semName,
        O_CREAT | O_EXCL,
        S_IRUSR | S_IWUSR,
        0 );

    if ( thread->sem == SEM_FAILED ) {

        lwsl_err( "Unable to open receive thread semaphore: %d: %s.\n",
            errno, strerror( errno ) );

        thread->sem = NULL;
        return -1;
    }

    thread->status.isStopPending = FALSE;
    thread->status.isStopping = FALSE;

    if ( pthread_create(
            &thread->id,
            NULL,
            &processQbitReceiveQueue,
            context ) != 0 )
    {
        lwsl_err( "Unable to create receive thread: %d: %s.\n",
            errno, strerror( errno ) );

        thread->id = 0;
        return -1;
    }

    return 0;
}


/* termQbitReceiveThread */

static void termQbitReceiveThread( QBIT_RECEIVE_THREAD * thread ) {

    if ( thread->id != 0 ) {
        struct timespec delay;

        delay.tv_sec = 0;
        delay.tv_nsec = 100;

        thread->status.isStopPending = TRUE;

        do {
            sem_post( thread->sem );
        }
        while ( !thread->status.isStopping
            && ( nanosleep( &delay, NULL ) <= 0 ));

        pthread_join( thread->id, NULL );
    }

    if ( thread->sem != NULL ) {
        sem_close( thread->sem );
    }

    sem_unlink( thread->semName );

    thread->id = 0;
    thread->sem = NULL;
}


/* isReceiveThreadStopping */

static int isReceiveThreadStopping( QBIT_RECEIVE_THREAD * thread ) {
    if ( thread->status.isStopPending ) {
        thread->status.isStopping = TRUE;
    }
    return thread->status.isStopping;
}


/* processQbitReceiveQueue - receive thread */

static void * processQbitReceiveQueue( void * qbitReceiveContext ) {

    QBIT_RECEIVE_CONTEXT * context;
    QBIT_QUEUE * queue;
    QBIT_RECEIVE_THREAD * thread;
    QBIT_QUEUE_ITEM * item;
    QBIT_RECEIVE_QUEUE_ITEM_DATA * itemData;

    context = (QBIT_RECEIVE_CONTEXT *)qbitReceiveContext;
    queue = &context->queue;
    thread = &context->thread;

    while( TRUE ) {

        sem_wait( thread->sem );

        if ( isReceiveThreadStopping( thread ) ) {
            break;
        }

        while (( item = getQbitQueueHead( queue ) )) {

            if ( isReceiveThreadStopping( thread ) ) {
                break;
            }

            itemData = (QBIT_RECEIVE_QUEUE_ITEM_DATA *)item->data;

            processQbitMessage( itemData->buf, itemData->len );

            freeQbitQueueHead( queue );
        }
    }

    return NULL;
}


/* processQbitMessage */

static int processQbitMessage( char * buf, int len )
{
    char * ptr;
    char * endPtr;
    char * nextPtr;
    int remLen;
    int count;

    if ( len < 2 ) {
        count = -1;
    }

    else if ( memcmp( buf, "\x1cg", 2 ) != 0 ) {

        if ( memcmp( buf, "\x1cr", 2 ) == 0 ) {
            processQbitResponseMessage( buf, len );
            count = 1;
        }
        else {
            count = -1;
        }
    }

    else {
        ptr = buf + 2;
        endPtr = buf + len;
        remLen = len - 2;

        count = 0;

        while ( ptr < endPtr ) {

            if (( remLen <= 3 )
                    || ( memcmp( ptr, "\x1cr\x1d", 3 ) != 0 )) {
                count = -1;
                break;
            }

            nextPtr = memchr( ptr + 3, '\x1f', remLen - 3 );

            if ( nextPtr == NULL ) {
                count = -1;
                break;
            }

            processQbitResponseMessage( ptr, (int)( nextPtr - ptr ) );

            remLen -= ( nextPtr - ptr ) + 1;
            ptr = nextPtr + 1;

            count++;
        }
    }

    if ( count <= 0 ) {
        lwsl_err( "Discarding invalid message: %d %.*s\n",
            len, len > 0 ? len : 0, buf );
    }

    return count;
}


/* processQbitResponseMessage */

static void processQbitResponseMessage( char * buf, int len ) {
    char lastChar;
    long messageId;
    QBIT_MESSAGE_CALLBACK * callback;
    void * context;
    

    lastChar = buf[ len ];
    buf[ len ] = 0;

    messageId = strtol( buf + 3, NULL, 10 );

    buf[ len ] = lastChar;

    if (( messageId == 0 )
        || ( messageId == LONG_MIN )
        || ( messageId == LONG_MAX )) {

        lwsl_err( "Invalid message ID: %d %.*s\n",
            len, len > 0 ? len : 0, buf );

        return;
    }

    if ( removeFromQbitCallbackMap(
            (int)messageId, &callback, &context ) != 0 ) {

        lwsl_err( "Message callback not found: %d %.*s\n",
            len, len > 0 ? len : 0, buf );

        return;
    }

    callback( context, QMS_SUCCESSFUL, buf, len );
}





