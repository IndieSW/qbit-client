//
//  qbit-queue.c
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
#include <pthread.h>

#include "qbit-queue.h"


/* initQbitQueue */

int initQbitQueue( QBIT_QUEUE * queue ) {

    queue->head = NULL;
    queue->tail = NULL;

    if ( pthread_mutex_init( &queue->mutex, NULL ) != 0 ) {
        return -1;
    }

    return 0;
}


/* termQbitQueue */

void termQbitQueue( QBIT_QUEUE * queue ) {
    freeQbitQueue( queue );
    pthread_mutex_destroy( &queue->mutex );
}


/* enqueueQbitQueueItem */

void enqueueQbitQueueItem( QBIT_QUEUE * queue, QBIT_QUEUE_ITEM * item ) {
    pthread_mutex_lock( &queue->mutex );
    addToQbitQueue( queue, item );
    pthread_mutex_unlock( &queue->mutex );
}


/* dequeueQbitQueueItem */

QBIT_QUEUE_ITEM * dequeueQbitQueueItem( QBIT_QUEUE * queue ) {

    QBIT_QUEUE_ITEM * item;

    pthread_mutex_lock( &queue->mutex );
    item = removeFromQbitQueue( queue );
    pthread_mutex_unlock( &queue->mutex );

    return item;
}


/* getQbitQueueHead */

QBIT_QUEUE_ITEM * getQbitQueueHead( QBIT_QUEUE * queue ) {

    QBIT_QUEUE_ITEM * item;

    pthread_mutex_lock( &queue->mutex );
    item = queue->head;
    pthread_mutex_unlock( &queue->mutex );

    return item;
}


/* freeQbitQueueHead */

void freeQbitQueueHead( QBIT_QUEUE * queue ) {

    QBIT_QUEUE_ITEM * item;

    pthread_mutex_lock( &queue->mutex );
    item = removeFromQbitQueue( queue );
    pthread_mutex_unlock( &queue->mutex );

    freeQbitQueueItem( item );
}


/* getQbitQueueTail */

QBIT_QUEUE_ITEM * getQbitQueueTail( QBIT_QUEUE * queue ) {

    QBIT_QUEUE_ITEM * item;

    pthread_mutex_lock( &queue->mutex );
    item = queue->tail;
    pthread_mutex_unlock( &queue->mutex );

    return item;
}


/* freeQbitQueue */

void freeQbitQueue( QBIT_QUEUE * queue ) {

    QBIT_QUEUE_ITEM * item;

    pthread_mutex_lock( &queue->mutex );

    while (( item = removeFromQbitQueue( queue ) )) {
        freeQbitQueueItem( item );
    }

    pthread_mutex_unlock( &queue->mutex );
}


/* freeQbitQueueItem */

void freeQbitQueueItem( QBIT_QUEUE_ITEM * item ) {

    if ( item ) {
        free( item );
    }
}


/* addToQbitQueue - must be synchronized externally */

void addToQbitQueue( QBIT_QUEUE * queue, QBIT_QUEUE_ITEM * item ) {

    item->next = NULL;

    if ( queue->head == NULL ) {
        queue->head = item;
    }
    else {
        queue->tail->next = item;
    }

    queue->tail = item;
}


/* removeFromQbitQueue - must be synchronized externally */

QBIT_QUEUE_ITEM * removeFromQbitQueue( QBIT_QUEUE * queue ) {

    QBIT_QUEUE_ITEM * item;

    item = queue->head;

    if ( item ) {
        queue->head = item->next;

        if ( queue->head == NULL ) {
            queue->tail = NULL;
        }
    }

    return item;
}
