//
//  qbit-protocol-transmit.h
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


/* transmit frame */

#define QBIT_TRANSMIT_MESSAGE_SIZE  160
#define QBIT_TRANSMIT_BATCH_SIZE    250

#define QBIT_TRANSMIT_PAYLOAD_SIZE ( \
            QBIT_TRANSMIT_MESSAGE_SIZE * QBIT_TRANSMIT_BATCH_SIZE )

#define QBIT_TRANSMIT_FRAME_SIZE ( \
            LWS_SEND_BUFFER_PRE_PADDING \
            + QBIT_TRANSMIT_PAYLOAD_SIZE \
            + LWS_SEND_BUFFER_POST_PADDING )


/* transmit buffer */

typedef struct {
    void * item;
    char * buf;
    int bufLen;
    int bufSent;
    int bufCount;
}
QBIT_TRANSMIT_BUF;


/* transmit queue */

typedef struct {
    char * payload;
    int payloadLen;
    int payloadBufLen;
    int messageCount;
}
QBIT_TRANSMIT_QUEUE_ITEM_DATA;


/* transmit context */

typedef struct {
    struct libwebsocket_protocols * webSocketContextProtocol;
    struct libwebsocket_context * webSocketContext;
    struct libwebsocket * webSocket;
    QBIT_TRANSMIT_BUF buf;
    QBIT_QUEUE queue;
}
QBIT_TRANSMIT_CONTEXT;


/* functions */

extern int initQbitTransmit(
    QBIT_TRANSMIT_CONTEXT * qbitTransmitContext );

extern void termQbitTransmit(
    QBIT_TRANSMIT_CONTEXT * qbitTransmitContext );

extern void startQbitTransmit(
    QBIT_TRANSMIT_CONTEXT * qbitTransmitContext,
    struct libwebsocket_protocols * webSocketContextProtocol,
    struct libwebsocket_context * webSocketContext,
    struct libwebsocket * webSocket );

extern void stopQbitTransmit(
    QBIT_TRANSMIT_CONTEXT * qbitTransmitContext );

extern int enqueueQbitMethodCallMessage(
    QBIT_TRANSMIT_CONTEXT * qbitTransmitContext,
    QBIT_MESSAGE_CALLBACK * callback,
    void * context,
    char * buf,
    int len );

extern int processQbitTransmitQueue(
    QBIT_TRANSMIT_CONTEXT * qbitTransmitContext );





