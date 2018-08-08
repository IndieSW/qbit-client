//
//  qbit-protocol-transmit.c
//  QBit WebSocket Client
//
//  Created by Bandoian, Terence (CONT) on 6/30/15.
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
#include <ctype.h>
#include <sys/time.h>

#include <pthread.h>
#include <errno.h>

#include <lws_config.h>
#include <libwebsockets.h>

#include "qbit-api.h"
#include "qbit-common.h"
#include "qbit-callback-map.h"
#include "qbit-queue.h"

#include "qbit-protocol-transmit.h"
#include "qbit-protocol-transmit.hp"


/* initQbitTransmit */

int initQbitTransmit(
    QBIT_TRANSMIT_CONTEXT * qbitTransmitContext )
{
    qbitTransmitContext->webSocketContextProtocol = NULL;
    qbitTransmitContext->webSocketContext = NULL;
    qbitTransmitContext->webSocket = NULL;

    initQbitTransmitBuf( &qbitTransmitContext->buf );

    if ( initQbitTransmitQueue( &qbitTransmitContext->queue ) != 0 ) {
        return -1;
    }

    return 0;
}


/* termQbitTransmit */

void termQbitTransmit(
    QBIT_TRANSMIT_CONTEXT * qbitTransmitContext )
{
    freeQbitTransmitBuf( &qbitTransmitContext->buf );
    termQbitTransmitQueue( &qbitTransmitContext->queue );
}


/* startQbitTransmit */

void startQbitTransmit(
    QBIT_TRANSMIT_CONTEXT * qbitTransmitContext,
    struct libwebsocket_protocols * webSocketContextProtocol,
    struct libwebsocket_context * webSocketContext,
    struct libwebsocket * webSocket )
{
    qbitTransmitContext->webSocketContextProtocol = webSocketContextProtocol;
    qbitTransmitContext->webSocketContext = webSocketContext;
    qbitTransmitContext->webSocket = webSocket;
}


/* stopQbitTransmit */

void stopQbitTransmit(
    QBIT_TRANSMIT_CONTEXT * qbitTransmitContext )
{
    freeQbitTransmitBuf( &qbitTransmitContext->buf );
    freeQbitQueue( &qbitTransmitContext->queue );
}


/* enqueueQbitMethodCallMessage */

int enqueueQbitMethodCallMessage(
    QBIT_TRANSMIT_CONTEXT * qbitTransmitContext,
    QBIT_MESSAGE_CALLBACK * callback,
    void * context,
    char * buf,
    int len )
{
    QBIT_QUEUE * queue;
    QBIT_QUEUE_ITEM * item;
    QBIT_TRANSMIT_QUEUE_ITEM_DATA * itemData;
    int messageLen;
    int payloadBufLen;
    int messageId;

    queue = &qbitTransmitContext->queue;

    pthread_mutex_lock( &queue->mutex );

    item = queue->tail;
    itemData = (QBIT_TRANSMIT_QUEUE_ITEM_DATA *)item->data;

    messageLen = METHOD_CALL_MESSAGE_HEADER_LEN + len;

    if (( item == NULL )
        || ( itemData->payloadLen + messageLen >= itemData->payloadBufLen )
        || ( itemData->messageCount >= QBIT_TRANSMIT_BATCH_SIZE ))
    {
        payloadBufLen = MAX( QBIT_TRANSMIT_PAYLOAD_SIZE, messageLen + 3 );

        item = malloc(
            sizeof( QBIT_QUEUE_ITEM )
            + sizeof( QBIT_TRANSMIT_QUEUE_ITEM_DATA )
            + LWS_SEND_BUFFER_PRE_PADDING
            + payloadBufLen
            + LWS_SEND_BUFFER_POST_PADDING );

        if ( item == NULL ) {
            pthread_mutex_unlock( &queue->mutex );

            lwsl_err( "Unable to allocate transmit buffer.\n" );
            return -1;
        }

        itemData = (QBIT_TRANSMIT_QUEUE_ITEM_DATA *)item->data;

        itemData->payload = (char *)item
            + sizeof( QBIT_QUEUE_ITEM )
            + sizeof( QBIT_TRANSMIT_QUEUE_ITEM_DATA )
            + LWS_SEND_BUFFER_PRE_PADDING;

        memcpy( itemData->payload, "\x1cg", 2 );

        itemData->payloadLen = 2;
        itemData->payloadBufLen = payloadBufLen;
        itemData->messageCount = 0;

        addToQbitQueue( queue, item );
    }

    if ( addToQbitCallbackMap( &messageId, callback, context ) != 0 ) {
        pthread_mutex_unlock( &queue->mutex );
        return -1;
    }

    sprintf( itemData->payload + itemData->payloadLen,
        METHOD_CALL_MESSAGE_HEADER, messageId );

    itemData->payloadLen += METHOD_CALL_MESSAGE_HEADER_LEN;

    memcpy( itemData->payload + itemData->payloadLen, buf, len );

    itemData->payloadLen += len;
    itemData->payload[ itemData->payloadLen++ ] = '\x1f';
    itemData->messageCount++;

    pthread_mutex_unlock( &queue->mutex );

    if ( qbitTransmitContext->webSocketContextProtocol ) {
        libwebsocket_callback_on_writable_all_protocol(
            qbitTransmitContext->webSocketContextProtocol );
    }

    return 0;
}


/* processQbitTransmitQueue - service thread */

int processQbitTransmitQueue(
    QBIT_TRANSMIT_CONTEXT * qbitTransmitContext )
{
    QBIT_QUEUE * queue;
    QBIT_TRANSMIT_BUF * buf;
    struct libwebsocket_context * webSocketContext;
    struct libwebsocket * webSocket;
    int count;

    queue = &qbitTransmitContext->queue;
    buf = &qbitTransmitContext->buf;

    if ( buf->bufSent >= buf->bufLen ) {
        freeQbitTransmitBuf( buf );
        fillQbitTransmitBuf( queue, buf );
    }

    count = 0;

    if ( buf->bufLen > 0 ) {
        webSocketContext = qbitTransmitContext->webSocketContext;
        webSocket = qbitTransmitContext->webSocket;

        count = transmitQbitMessage( webSocket, buf );

        libwebsocket_callback_on_writable( webSocketContext, webSocket );
    }

    return ( count < 0 ? -1 : 0 );
}


/* transmitQbitMessage */

static int transmitQbitMessage(
    struct libwebsocket * webSocket,
    QBIT_TRANSMIT_BUF * buf )
{
    int count;

    count = libwebsocket_write(
        webSocket,
        (unsigned char *)( buf->buf + buf->bufSent ),
        buf->bufLen - buf->bufSent,
        LWS_WRITE_TEXT );

    if ( count == -1 ) {
        lwsl_err( "Error writing to web socket.\n" );
        return -1;
    }

    buf->bufSent += count;

    return count;
}


/* initQbitTransmitBuf */

static void initQbitTransmitBuf( QBIT_TRANSMIT_BUF * buf ) {
    buf->item = NULL;
    buf->buf = NULL;
    buf->bufLen = 0;
    buf->bufSent = 0;
    buf->bufCount = 0;
}


/* fillQbitTransmitBuf */

static int fillQbitTransmitBuf(
    QBIT_QUEUE * queue,
    QBIT_TRANSMIT_BUF * buf )
{
    QBIT_QUEUE_ITEM * item;
    QBIT_TRANSMIT_QUEUE_ITEM_DATA * itemData;

    item = dequeueQbitQueueItem( queue );

    if ( item == NULL ) {
        return 0;
    }

    itemData = (QBIT_TRANSMIT_QUEUE_ITEM_DATA *)item->data;

    if ( itemData->messageCount <= 0 ) {
        freeQbitQueueItem( item );
        return 0;
    }

    buf->item = item;
    buf->buf = itemData->payload;
    buf->bufLen = itemData->payloadLen;
    buf->bufSent = 0;
    buf->bufCount = itemData->messageCount;

    return itemData->messageCount;
}


/* freeQbitTransmitBuf */

static void freeQbitTransmitBuf( QBIT_TRANSMIT_BUF * buf ) {

    if ( buf->item ) {
        freeQbitQueueItem( buf->item );
    }

    initQbitTransmitBuf( buf );
}


/* initQbitTransmitQueue */

static int initQbitTransmitQueue( QBIT_QUEUE * queue ) {

    if ( initQbitQueue( queue ) != 0 ) {

        lwsl_err( "Unable to initialize transmit queue: %d: %s.\n",
            errno, strerror( errno ) );

        return -1;
    }

    return 0;
}


/* termQbitTransmitQueue */

static void termQbitTransmitQueue( QBIT_QUEUE * queue ) {
    termQbitQueue( queue );
}
