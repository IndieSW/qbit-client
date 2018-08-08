//
//  qbit-protocol-receive.h
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


/* receive frame */

#define QBIT_RECEIVE_MESSAGE_SIZE   128
#define QBIT_RECEIVE_BATCH_SIZE     500

#define QBIT_RECEIVE_FRAME_SIZE  \
            ( QBIT_RECEIVE_MESSAGE_SIZE * QBIT_RECEIVE_BATCH_SIZE )


/* receive queue */

typedef struct {
    char * buf;
    int len;
}
QBIT_RECEIVE_QUEUE_ITEM_DATA;


/* receive thread */

#define QBIT_RECEIVE_THREAD_SEM_NAME_MAX_PREFIX_LEN  32
#define QBIT_RECEIVE_THREAD_SEM_NAME_MAX_SUFFIX_LEN  32

#define QBIT_RECEIVE_THREAD_SEM_NAME_LEN ( \
            QBIT_RECEIVE_THREAD_SEM_NAME_MAX_PREFIX_LEN \
            + QBIT_RECEIVE_THREAD_SEM_NAME_MAX_SUFFIX_LEN \
            + 1 )

typedef struct {
    pthread_t id;
    QBIT_THREAD_STATUS status;
    sem_t * sem;
    char semName[ QBIT_RECEIVE_THREAD_SEM_NAME_LEN ];
}
QBIT_RECEIVE_THREAD;


/* receive context */

typedef struct {
    struct libwebsocket_context * webSocketContext;
    struct libwebsocket * webSocket;
    QBIT_QUEUE_ITEM * receiveBuf;
    QBIT_QUEUE queue;
    QBIT_RECEIVE_THREAD thread;
}
QBIT_RECEIVE_CONTEXT;


/* functions */

extern int initQbitReceive(
    QBIT_RECEIVE_CONTEXT * qbitReceiveContext );

extern void termQbitReceive(
    QBIT_RECEIVE_CONTEXT * qbitReceiveContext );

extern void startQbitReceive(
    QBIT_RECEIVE_CONTEXT * qbitReceiveContext,
    struct libwebsocket_context * webSocketContext,
    struct libwebsocket * webSocket );

extern void stopQbitReceive(
    QBIT_RECEIVE_CONTEXT * qbitReceiveContext );

extern int receiveQbitMessage(
    QBIT_RECEIVE_CONTEXT * qbitReceiveContext, void * in, size_t len );
