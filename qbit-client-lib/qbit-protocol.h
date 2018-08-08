//
//  qbit-protocol.h
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


extern void getQbitProtocolDefinition(
    struct libwebsocket_protocols * protocol );

extern void initQbitProtocol(
    struct libwebsocket_protocols * contextProtocol );

extern void termQbitProtocol();

extern int allocQbitProtocolConnection();

extern void openQbitProtocolConnection(
    int connectionId,
    QBIT_CONNECTION_CALLBACK * connectionCallback,
    struct libwebsocket_context * webSocketContext,
    char * host,
    int port,
    char * path,
    int isSsl );

extern int closeQbitProtocolConnection( int connectionId );

extern int sendQbitProtocolMethodCallMessage(
    int connectionId,
    QBIT_MESSAGE_CALLBACK * callback,
    void * context,
    char * buf,
    int len );





