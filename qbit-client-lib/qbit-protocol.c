//
//  qbit-protocol.c
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
#include <signal.h>
#include <string.h>
#include <ctype.h>
#include <sys/time.h>

#include <pthread.h>
#include <semaphore.h>

#include <lws_config.h>
#include <libwebsockets.h>

#include "qbit-api.h"
#include "qbit-common.h"
#include "qbit-callback-map.h"
#include "qbit-queue.h"
#include "qbit-thread.h"

#include "qbit-protocol-transmit.h"
#include "qbit-protocol-receive.h"

#include "qbit-protocol.h"
#include "qbit-protocol.hp"


/* getQbitProtocolDefinition */

void getQbitProtocolDefinition(
    struct libwebsocket_protocols * protocol )
{
    memcpy( protocol, &QBIT_PROTOCOL, sizeof( struct libwebsocket_protocols ) );
    protocol->callback = &qbitCallback;
}


/* initQbitProtocol */

void initQbitProtocol(
    struct libwebsocket_protocols * contextProtocol )
{
    pthread_mutex_lock( &qbitConnectionMutex );

    webSocketContextProtocol = contextProtocol;
    initQbitProtocolConnectionContext( &qbitConnectionContext );
    initQbitCallbackMap();

    pthread_mutex_unlock( &qbitConnectionMutex );
}


/* termQbitProtocol */

void termQbitProtocol() {
    pthread_mutex_lock( &qbitConnectionMutex );

    webSocketContextProtocol = NULL;
    initQbitProtocolConnectionContext( &qbitConnectionContext );
    termQbitCallbackMap();

    pthread_mutex_unlock( &qbitConnectionMutex );
}


/* initQbitProtocolConnectionContext */

static void initQbitProtocolConnectionContext(
    QBIT_CONNECTION_CONTEXT * connectionContext )
{
    memset( connectionContext, 0, sizeof( QBIT_CONNECTION_CONTEXT ) );
}


/* allocQbitProtocolConnection */

int allocQbitProtocolConnection()
{
    QBIT_CONNECTION_CONTEXT * connectionContext;
    int connectionId;

    pthread_mutex_lock( &qbitConnectionMutex );

    connectionContext = &qbitConnectionContext;

    if ( connectionContext->connectionId == 0 ) {

        connectionId = 1;

        connectionContext->connectionId = connectionId;
        connectionContext->connectionCallback = NULL;
        connectionContext->isClosePending = FALSE;

        connectionContext->webSocketContext = NULL;
        connectionContext->webSocket = NULL;

        if ( initQbitTransmit( &connectionContext->transmitContext ) != 0 ) {
            connectionId = -1;
        }

        if ( initQbitReceive( &connectionContext->receiveContext ) != 0 ) {
            connectionId = -1;
        }

        if ( connectionId != connectionContext->connectionId ) {
            freeQbitProtocolConnection( connectionContext );
        }
    }
    else {
        lwsl_err( "Unable to allocate QBit connection.\n" );
        connectionId = -1;
    }

    pthread_mutex_unlock( &qbitConnectionMutex );

    return connectionId;
}


/* freeQbitProtocolConnection */

static void freeQbitProtocolConnection(
    QBIT_CONNECTION_CONTEXT * connectionContext )
{
    stopQbitTransmit( &connectionContext->transmitContext );
    stopQbitReceive( &connectionContext->receiveContext );

    termQbitTransmit( &connectionContext->transmitContext );
    termQbitReceive( &connectionContext->receiveContext );

    initQbitProtocolConnectionContext( connectionContext );
}


/* openQbitProtocolConnection */

void openQbitProtocolConnection(
    int connectionId,
    QBIT_CONNECTION_CALLBACK * connectionCallback,
    struct libwebsocket_context * webSocketContext,
    char * host,
    int port,
    char * path,
    int isSsl )
{
    struct libwebsocket * webSocket = NULL;

    pthread_mutex_lock( &qbitConnectionMutex );

    if ( connectionId == qbitConnectionContext.connectionId ) {

        qbitConnectionContext.connectionCallback = connectionCallback;
        qbitConnectionContext.webSocketContext = webSocketContext;

        webSocket = libwebsocket_client_connect_extended(
            webSocketContext,           /* web socket context */
            host,                       /* address */
            port,                       /* port */
            isSsl ? 2 : 0,              /* ssl_connection */
            path,                       /* path */
            host,                       /* host */
            host,                       /* origin */
            QBIT_PROTOCOL_NAME,         /* protocol */
            -1,                         /* ietf version or -1 */
            &qbitConnectionContext
        );
    }

    pthread_mutex_unlock( &qbitConnectionMutex );

    if ( webSocket == NULL ) {
        connectionCallback( connectionId, QCE_ERROR, (void *)-1 );
    }
}


/* closeQbitProtocolConnection */

int closeQbitProtocolConnection( int connectionId ) {

    int status;

    pthread_mutex_lock( &qbitConnectionMutex );

    if (( connectionId == qbitConnectionContext.connectionId )
        && qbitConnectionContext.webSocketContext
        && qbitConnectionContext.webSocket )
    {
        qbitConnectionContext.isClosePending = TRUE;

        libwebsocket_callback_on_writable_all_protocol(
            webSocketContextProtocol );

        status = 0;
    }
    else {
        lwsl_err(
            "Unable to close QBit connection - invalid connection id.\n" );

        status = -1;
    }

    pthread_mutex_unlock( &qbitConnectionMutex );

    return status;
}


/* sendQbitProtocolMethodCallMessage */

int sendQbitProtocolMethodCallMessage(
    int connectionId,
    QBIT_MESSAGE_CALLBACK * callback,
    void * context,
    char * buf,
    int len )
{
    int status;

    pthread_mutex_lock( &qbitConnectionMutex );

    if (( qbitConnectionContext.connectionId == connectionId )
        && !qbitConnectionContext.isClosePending )
    {
        status = enqueueQbitMethodCallMessage(
            &qbitConnectionContext.transmitContext,
            callback,
            context,
            buf,
            len );
    }
    else {
        lwsl_err( "Unable to send QBit message - invalid connection id.\n" );
        status = -1;
    }

    pthread_mutex_unlock( &qbitConnectionMutex );

    return status;
}


/* qbitCallback */

static int qbitCallback(
    struct libwebsocket_context * webSocketContext,
    struct libwebsocket * webSocket,
    enum libwebsocket_callback_reasons reason,
    void * user,
    void * in,
    size_t len )
{
	QBIT_CONNECTION_CONTEXT * connectionContext;
    int status;

    if ( reason == LWS_CALLBACK_GET_THREAD_ID ) {
        return (int)pthread_self();
    }

	connectionContext = (QBIT_CONNECTION_CONTEXT *)user;

    if ( connectionContext == NULL ) {
        return 0;
    }

	switch ( reason ) {

	case LWS_CALLBACK_CLIENT_ESTABLISHED:
        status = qbitClientEstablished(
            connectionContext, webSocketContext, webSocket );
        break;

    case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
        status = qbitClientConnectionError( connectionContext );
        break;

    case LWS_CALLBACK_WSI_DESTROY:
        status = qbitWsiDestroy( connectionContext );
        break;

	case LWS_CALLBACK_CLIENT_WRITEABLE:
        status = qbitClientWritable(
            &connectionContext->transmitContext );
        break;

	case LWS_CALLBACK_CLIENT_RECEIVE:
        status = qbitClientReceive(
            &connectionContext->receiveContext, in, len );
        break;

    default:
        status = 0;
		break;
	}

    if ( webSocketContext
        && webSocket
        && connectionContext->isClosePending )
    {
        return -1;
    }

	return status;
}


/* qbitClientEstablished */

static int qbitClientEstablished(
    QBIT_CONNECTION_CONTEXT * connectionContext,
    struct libwebsocket_context * webSocketContext,
    struct libwebsocket * webSocket )
{
    connectionContext->webSocket = webSocket;

    startQbitTransmit(
        &connectionContext->transmitContext,
        webSocketContextProtocol,
        webSocketContext,
        webSocket );

    startQbitReceive(
        &connectionContext->receiveContext,
        webSocketContext,
        webSocket );

    connectionContext->connectionCallback(
        connectionContext->connectionId, QCE_ESTABLISHED, NULL );

    return 0;
}


/* qbitClientConnectionError */

static int qbitClientConnectionError(
    QBIT_CONNECTION_CONTEXT * connectionContext )
{
    lwsl_err( "QBit web socket connection error.\n" );

    if ( connectionContext->connectionCallback ) {
        connectionContext->connectionCallback(
            connectionContext->connectionId, QCE_ERROR, (void *)-1 );
    }

    return -1;
}


/* qbitWsiDestroy */

static int qbitWsiDestroy( QBIT_CONNECTION_CONTEXT * connectionContext ) {

    int connectionId;
    QBIT_CONNECTION_CALLBACK * connectionCallback;

    pthread_mutex_lock( &qbitConnectionMutex );

    connectionId = connectionContext->connectionId;
    connectionCallback = connectionContext->connectionCallback;

    freeQbitProtocolConnection( connectionContext );

    pthread_mutex_unlock( &qbitConnectionMutex );

    if ( connectionCallback ) {
        connectionCallback( connectionId, QCE_CLOSED, NULL );
    }

    return 0;
}


/* qbitClientWritable */

static int qbitClientWritable( QBIT_TRANSMIT_CONTEXT * transmitContext ) {
    processQbitTransmitQueue( transmitContext );
    return 0;
}


/* qbitClientReceive */

static int qbitClientReceive(
    QBIT_RECEIVE_CONTEXT * receiveContext, void * in, size_t len )
{
    receiveQbitMessage( receiveContext, in, len );
    return 0;
}
