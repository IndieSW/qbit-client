//
//  qbit-test-client.c
//  QBit WebSocket Client
//
//  Created by Bandoian, Terence (CONT) on 6/16/15.
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
#include <string.h>
#include <errno.h>
#include <sys/time.h>
#include <fcntl.h>
#include <sched.h>
#include <sys/stat.h>
#include <semaphore.h>

#include "../qbit-client-lib/qbit-api.h"

#include "interrupt.h"
#include "qbit-test-client.hp"


/* main */

int main( int argc, const char * argv[] ) {

    int isSsl = FALSE;
    int status = 0;

    /* check for ssl */

    if ( argc > 1 ) {
        isSsl = ( strcasecmp( argv[ 1 ], "--ssl" ) == 0 );
    }

    /* init app */

    if ( status == 0 ) {
        status = initQbitApp();
    }

    /* start qbit service */

    if ( startQbitService( &qbitServiceCallback ) != 0 ) {
        status = -1;
    }

    /* open qbit connection */

    if ( status == 0 ) {
        qbitConnection.id = openQbitConnection(
            &qbitConnectionCallback,
            "127.0.0.1",
            6060,
            "/",
            isSsl );

        if ( qbitConnection.id <= 0 ) {
            status = -1;
        }
    }

    /* handle ctrl-c */

    enableInterrupt();

    /* send requests */

    if ( status == 0 ) {
        status = sendQbitMessages( qbitConnection.id );
    }

    /* wait for responses */

    if ( status == 0 ) {
        status = waitForQbitResponses();
    }

    /* shut down */

    if ( qbitConnection.id != 0 ) {
        qbitConnection.isClosed = FALSE;

        if ( closeQbitConnection( qbitConnection.id ) == 0 ) {
            while ( !qbitConnection.isClosed && !hasInterruptOccurred() ) {
                sched_yield();
            }
        }
    }

    stopQbitService();
    termQbitApp();

    /* display results */

    printQbitPerformanceData();

    exit( status == 0 ? EXIT_SUCCESS : EXIT_FAILURE );
}


/* initQbitApp */

static int initQbitApp() {

    qbitConnection.id = 0;
    qbitConnection.isClosed = FALSE;

    sem_unlink( QBIT_APP_SEM_NAME );

    qbitAppSem = sem_open(
        QBIT_APP_SEM_NAME,
        O_CREAT | O_EXCL,
        S_IRUSR | S_IWUSR,
        0 );

    if ( qbitAppSem == SEM_FAILED ) {

        printf( "Unable to open app semaphore: %d: %s.\n",
            errno, strerror( errno ) );

        qbitAppSem = NULL;
        return -1;
    }

    transmitIndex = 0;
    receiveIndex = 0;

    return 0;
}


/* termQbitApp */

static void termQbitApp() {

    if ( qbitAppSem != NULL ) {
        sem_close( qbitAppSem );
    }

    sem_unlink( QBIT_APP_SEM_NAME );

    qbitAppSem = NULL;
}


/* sendQbitMessages */

static int sendQbitMessages( int connectionId ) {

    long timestamp;
    char buf[ QBIT_MAX_MESSAGE_SIZE ];
    int len;

    for ( transmitIndex = 0;
        transmitIndex < MAX_XR_COUNT;
        transmitIndex++ )
    {
        if ( hasInterruptOccurred() ) {
            printf( "Terminating on user interrupt.\n" );
            return -1;
        }

        gettimeofday( transmitTime + transmitIndex, NULL );

        timestamp = transmitTime[ transmitIndex ].tv_sec * 1000
            + transmitTime[ transmitIndex ].tv_usec / 1000;

        len = sprintf( buf, QBIT_MESSAGE_BODY, timestamp );

        if ( sendQbitMethodCallMessage(
                connectionId,
                &qbitMessageCallback,
                NULL,
                buf,
                len ) != 0 )
        {
            printf( "Error sending message.\n" );
            return -1;
        }

        if (( transmitIndex == 0 )
            || ( transmitIndex % 1000 == 999 ))
        {
            printf( "xmt: %8d  %10ld.%06d\n",
                transmitIndex + 1,
                transmitTime[ transmitIndex ].tv_sec,
                transmitTime[ transmitIndex ].tv_usec );
        }
    }

    return 0;
}


/* waitForQbitResponses */

static int waitForQbitResponses() {

    int status;

    status = sem_wait( qbitAppSem );

    if ( status != 0 ) {
        printf( "Error waiting for app signal: (%d) %s.\n",
            errno, strerror( errno ) );
    }

    return 0;
}


/* qbitServiceCallback */

static void qbitServiceCallback( QBIT_SERVICE_EVENT event ) {

    switch ( event ) {
    case QSE_UNRECOVERABLE_ERROR:
        printf( "Unrecoverable error.\n" );
        sem_post( qbitAppSem );
        break;

    case QSE_WEB_SOCKET_ERROR:
        printf( "Web socket error.\n" );
        sem_post( qbitAppSem );
        break;

    case QSE_INTERRUPT:
        printf( "Service interrupted.\n" );
        sem_post( qbitAppSem );
        break;

    default:
        break;
    }
}


/* qbitConnectionCallback */

static void qbitConnectionCallback(
    int connectionId, QBIT_CONNECTION_EVENT event, void * data )
{
    switch ( event ) {
    case QCE_ERROR:
        printf( "Unable to open QBit connection.\n" );
        sem_post( qbitAppSem );
        break;

    case QCE_ESTABLISHED:
        break;

    case QCE_CLOSED:
        qbitConnection.isClosed = TRUE;
        sem_post( qbitAppSem );
        break;

    default:
        break;
    }
}


/* qbitMessageCallback */

static void qbitMessageCallback(
    void * context, QBIT_MESSAGE_STATUS status, void * data, int len )
{
    if (( context != NULL )
        || ( status != QMS_SUCCESSFUL )
        || ( memmem( data, len,
                QBIT_SUCCESS_ARGUMENT,
                QBIT_SUCCESS_ARGUMENT_LEN ) == NULL ))
    {
        printf( "Invalid response received: %lx %d %d %.*s\n",
            (unsigned long)context, status, len, len, data );

        return;
    }

    if ( receiveIndex < MAX_XR_COUNT ) {
        gettimeofday( receiveTime + receiveIndex, NULL );

        if (( receiveIndex == 0 )
            || ( receiveIndex % 1000 == 999 ))
        {
            printf( "rcv: %8d  %10ld.%06d\n",
                receiveIndex + 1,
                receiveTime[ receiveIndex ].tv_sec,
                receiveTime[ receiveIndex ].tv_usec );
        }

        receiveIndex++;
    }
    else {
        printf( "Extraneous message received.\n" );
    }

    if ( receiveIndex >= MAX_XR_COUNT ) {
        sem_post( qbitAppSem );
    }
}


/* printQbitPerformanceData */

static void printQbitPerformanceData() {

    int transmitCount = getQbitTransmitCount();
    int receiveCount = getQbitReceiveCount();

    if (( transmitCount > 0 ) && ( receiveCount > 0 )) {
        struct timeval transmitTime;
        struct timeval receiveTime;
        long seconds;
        int microSeconds;

        getQbitTransmitTime( &transmitTime, 0 );
        getQbitReceiveTime( &receiveTime, receiveCount - 1 );

        seconds = receiveTime.tv_sec - transmitTime.tv_sec;
        microSeconds = receiveTime.tv_usec - transmitTime.tv_usec;

        if ( microSeconds < 0 ) {
            microSeconds += 1000000;
            seconds -= 1;
        }

        printf(
            "\n"
            "first transmit to last receive = ~%ld.%06d seconds\n"
            "\n"
            "  rcv %7d %ld.%06d\n"
            "  xmt %7d %ld.%06d\n"
            "\n"
            ,
            seconds, microSeconds,
            receiveCount, receiveTime.tv_sec, receiveTime.tv_usec,
            1, transmitTime.tv_sec, transmitTime.tv_usec );
    }
}


/* getQbitTransmitCount */

static int getQbitTransmitCount() {
    return transmitIndex;
}


/* getQbitTransmitTime */

static int getQbitTransmitTime( struct timeval * time, int index ) {

    if (( index >= 0 )
        && ( index < transmitIndex )
        && ( index < MAX_XR_COUNT ))
    {
        memcpy( time, &transmitTime[ index ], sizeof( *time ) );
        return 0;
    }

    memset( time, 0, sizeof( *time ) );
    return -1;
}


/* getQbitReceiveCount */

static int getQbitReceiveCount() {
    return receiveIndex;
}


/* getQbitReceiveTime */

static int getQbitReceiveTime( struct timeval * time, int index ) {

    if (( index >= 0 )
        && ( index < receiveIndex )
        && ( index < MAX_XR_COUNT ))
    {
        memcpy( time, &receiveTime[ index ], sizeof( *time ) );
        return 0;
    }

    memset( time, 0, sizeof( *time ) );
    return -1;
}






