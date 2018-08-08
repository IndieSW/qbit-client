//
//  qbit-api.h
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


/* qbit service callback */

typedef enum {
    QSE_UNRECOVERABLE_ERROR,
    QSE_WEB_SOCKET_ERROR,
    QSE_INTERRUPT,
}
QBIT_SERVICE_EVENT;

typedef void (QBIT_SERVICE_CALLBACK)( QBIT_SERVICE_EVENT event );


/* qbit connection callback */

typedef enum {
    QCE_ERROR,              /* data = error code */
    QCE_ESTABLISHED,        /* data = NULL */
    QCE_CLOSED,             /* data = NULL */
}
QBIT_CONNECTION_EVENT;

typedef void (QBIT_CONNECTION_CALLBACK)(
    int connectionId, QBIT_CONNECTION_EVENT event, void * data );


/* qbit message callback - message must be consumed immediately */

typedef enum {
    QMS_SUCCESSFUL,         /* data = message buffer, len = message length */
    QMS_CONNECTION_CLOSED,  /* data = NULL */
    QMS_TRANSMIT_ERROR,     /* data = NULL */
    QMS_TIMEOUT,            /* data = NULL */
}
QBIT_MESSAGE_STATUS;

typedef void (QBIT_MESSAGE_CALLBACK)(
    void * context, QBIT_MESSAGE_STATUS status, void * data, int len );


/* functions */

extern int startQbitService( QBIT_SERVICE_CALLBACK * callback );
extern void stopQbitService();

extern int openQbitConnection(
    QBIT_CONNECTION_CALLBACK * connectionCallback,
    char * host,
    int port,
    char * path,
    int isSsl );

extern int closeQbitConnection( int connectionId );

extern int sendQbitMethodCallMessage(
    int connectionId,
    QBIT_MESSAGE_CALLBACK * callback,
    void * context,
    char * buf,
    int len );





