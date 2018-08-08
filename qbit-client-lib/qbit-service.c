//
//  qbit-service.c
//  QBit WebSocket Client
//
//  Created by Bandoian, Terence (CONT) on 7/7/15.
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
#include <pthread.h>
#include <errno.h>

#include <lws_config.h>
#include <libwebsockets.h>

#include "qbit-api.h"
#include "qbit-common.h"
#include "qbit-queue.h"
#include "qbit-protocol.h"
#include "qbit-thread.h"

#include "qbit-service.h"
#include "qbit-service.hp"


/* startQbitService */

int startQbitService( QBIT_SERVICE_CALLBACK * callback ) {

    int status = 0;


    if ( qbitServiceThread.id || webSocketContext ) {
        lwsl_warn( "QBit service already started.\n" );
        return 1;
    }


    qbitServiceThread.status.isStopPending = FALSE;
    qbitServiceThread.status.isStopping = FALSE;

    qbitServiceCallback = callback;


    lws_set_log_level( LLL_ERR | LLL_WARN, NULL );

    getQbitProtocolDefinition( webSocketProtocols );
    initQbitProtocol( webSocketProtocols );

    webSocketContext = libwebsocket_create_context( &webSocketContextInfo );

    if ( webSocketContext == NULL ) {
        lwsl_err( "Unable to create QBit web socket context.\n" );
        status = -1;
    }


    if ( initQbitQueue( &qbitServiceQueue ) != 0 ) {
        lwsl_err( "Unable to initialize QBit service command queue: %d: %s.\n",
            errno, strerror( errno ) );

        if ( status == 0 ) {
            status = -1;
        }
    }


    if ( status == 0 ) {
        if ( pthread_create(
                &qbitServiceThread.id,
                NULL,
                &serviceQbitWebSocketEvents,
                webSocketContext ) != 0 )
        {
            lwsl_err( "Unable to create QBit service thread: %d: %s.\n",
                errno, strerror( errno ) );

            qbitServiceThread.id = 0;
            status = -1;
        }
    }


    if ( status != 0 ) {
        stopQbitService();
    }


    return status;
}


/* stopQbitService */

void stopQbitService() {

    if ( qbitServiceThread.id != 0 ) {
        struct timespec delay;

        delay.tv_sec = 0;
        delay.tv_nsec = 100;

        qbitServiceThread.status.isStopPending = TRUE;

        do {
            libwebsocket_cancel_service( webSocketContext );
        }
        while ( !qbitServiceThread.status.isStopping
            && ( nanosleep( &delay, NULL ) <= 0 ));

        pthread_join( qbitServiceThread.id, NULL );
        qbitServiceThread.id = 0;
    }

    termQbitQueue( &qbitServiceQueue );

    if ( webSocketContext != NULL ) {
        struct libwebsocket_context * context = webSocketContext;
        webSocketContext = NULL;
        libwebsocket_context_destroy( context );
    }

    termQbitProtocol();
}


/* openQbitConnection */

int openQbitConnection(
    QBIT_CONNECTION_CALLBACK * connectionCallback,
    char * host,
    int port,
    char * path,
    int isSsl )
{
    int connectionId;
    QBIT_QUEUE_ITEM * item;
    QBIT_SERVICE_QUEUE_ITEM_DATA * itemData;
    QBIT_OPEN_CONNECTION_DATA * openConnectionData;

    if ( qbitServiceThread.id == 0 ) {
        lwsl_err( "Unable to open connection - QBit service stopped.\n" );
        return -1;
    }

    item = allocQbitServiceCommand();

    if ( item == NULL ) {
        return -1;
    }

    connectionId = allocQbitProtocolConnection();

    if ( connectionId <= 0 ) {
        free( item );
        return -1;
    }

    itemData = (QBIT_SERVICE_QUEUE_ITEM_DATA *)item->data;
    itemData->command = QSC_OPEN_CONNECTION;

    openConnectionData = &itemData->openConnectionData;
    openConnectionData->connectionId = connectionId;
    openConnectionData->connectionCallback = connectionCallback;
    openConnectionData->host = host;
    openConnectionData->port = port;
    openConnectionData->path = path;
    openConnectionData->isSsl = ( isSsl != FALSE );

    submitQbitServiceCommand( item );

    return connectionId;
}


/* closeQbitConnection */

int closeQbitConnection( int connectionId ) {

    QBIT_QUEUE_ITEM * item;
    QBIT_SERVICE_QUEUE_ITEM_DATA * itemData;
    QBIT_CLOSE_CONNECTION_DATA * closeConnectionData;

    if ( qbitServiceThread.id == 0 ) {
        lwsl_err( "Unable to close connection - QBit service stopped.\n" );
        return -1;
    }

    item = allocQbitServiceCommand();

    if ( item == NULL ) {
        return -1;
    }

    itemData = (QBIT_SERVICE_QUEUE_ITEM_DATA *)item->data;
    itemData->command = QSC_CLOSE_CONNECTION;

    closeConnectionData = &itemData->closeConnectionData;
    closeConnectionData->connectionId = connectionId;

    submitQbitServiceCommand( item );

    return 0;
}


/* sendQbitMethodCallMessage */

int sendQbitMethodCallMessage(
    int connectionId,
    QBIT_MESSAGE_CALLBACK * callback,
    void * context,
    char * buf,
    int len )
{
    if ( qbitServiceThread.id == 0 ) {
        lwsl_err( "Unable to send message - QBit service stopped.\n" );
        return -1;
    }

    return sendQbitProtocolMethodCallMessage(
        connectionId, callback, context, buf, len );
}


/* allocQbitServiceCommand */

static QBIT_QUEUE_ITEM * allocQbitServiceCommand() {

    QBIT_QUEUE_ITEM * item;

    item = malloc(
        sizeof( QBIT_QUEUE_ITEM )
        + sizeof( QBIT_SERVICE_QUEUE_ITEM_DATA ) );

    if ( item == NULL ) {
        lwsl_err( "Unable to allocate service buffer.\n" );
    }

    return item;
}


/* submitQbitServiceCommand */

static void submitQbitServiceCommand( QBIT_QUEUE_ITEM * item ) {

    struct timespec delay;

    enqueueQbitQueueItem( &qbitServiceQueue, item );

    delay.tv_sec = 0;
    delay.tv_nsec = 100;

    do {
        libwebsocket_cancel_service( webSocketContext );
    }
    while (( getQbitQueueTail( &qbitServiceQueue ) == item )
        && ( nanosleep( &delay, NULL ) <= 0 ));
}


/* serviceQbitWebSocketEvents - service thread */

static void * serviceQbitWebSocketEvents( void * webSocketContext ) {

    struct libwebsocket_context * context;
    int status;

    context = (struct libwebsocket_context *)webSocketContext;

    while ( TRUE ) {

#ifdef IS_IOS
        libwebsocket_callback_on_writable_all_protocol( webSocketProtocols );
#endif

        status = libwebsocket_service( context, 1000 );

        if ( status != 0 ) {
            if ( status > 0 ) {
                lwsl_err( "Invalid QBit web socket context.\n" );
                qbitServiceCallback( QSE_UNRECOVERABLE_ERROR );
                break;
            }

            lwsl_err( "QBit web socket error: (%d) %s.\n",
                errno, strerror( errno ) );

            qbitServiceCallback( QSE_WEB_SOCKET_ERROR );
        }

        if ( qbitServiceThread.status.isStopPending ) {
            qbitServiceThread.status.isStopping = TRUE;
            break;
        }

        processQbitServiceQueue( context );
    }

    return NULL;
}


/* processQbitServiceQueue */

static void processQbitServiceQueue(
    struct libwebsocket_context * webSocketContext )
{
    QBIT_QUEUE_ITEM * item;
    QBIT_SERVICE_QUEUE_ITEM_DATA itemData;

    item = dequeueQbitQueueItem( &qbitServiceQueue );

    if ( item == NULL ) {
        return;
    }

    memcpy( &itemData, item->data, sizeof( itemData ) );

    freeQbitQueueItem( item );

    switch ( itemData.command ) {

    case QSC_OPEN_CONNECTION:
        processQbitOpenConnectionCommand(
            webSocketContext, &itemData.openConnectionData );
        break;

    case QSC_CLOSE_CONNECTION:
        processQbitCloseConnectionCommand( &itemData.closeConnectionData );
        break;

    default:
        break;
    }
}


/* processQbitOpenConnectionCommand */

static void processQbitOpenConnectionCommand(
    struct libwebsocket_context * webSocketContext,
    QBIT_OPEN_CONNECTION_DATA * openConnectionData )
{
    openQbitProtocolConnection(
        openConnectionData->connectionId,
        openConnectionData->connectionCallback,
        webSocketContext,
        openConnectionData->host,
        openConnectionData->port,
        openConnectionData->path,
        openConnectionData->isSsl );
}


/* processQbitCloseConnectionCommand */

static void processQbitCloseConnectionCommand(
    QBIT_CLOSE_CONNECTION_DATA * closeConnectionData )
{
    closeQbitProtocolConnection( closeConnectionData->connectionId );
}




