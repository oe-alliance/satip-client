/*
 * satip: timer functions
 *
 * Copyright (C) 2014  mc.fishdish@gmail.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "polltimer.h"

static long starttime=0;


typedef struct polltimer
{
    struct polltimer* next;
    struct timespec ts;
    void (*timer_handler) (void*);
    void* param;
} t_polltimer;



typedef struct polltimer_periodic
{
    struct timespec ts;
    t_polltimer** queue_head;
    void (*timer_handler) (void*);
    int msec;
    void* param;
} t_polltimer_periodic;



static void expire_msec(struct timespec* ts,int msec)
{
    ts->tv_nsec +=  (msec%1000)*1000000;
    if ( ts->tv_nsec>1000000000 )
    {
        ts->tv_nsec -= 1000000000;
        ts->tv_sec  += 1;
    }
    ts->tv_sec  +=  msec/1000;
}


static t_polltimer* queue_timer(t_polltimer** queue_head,
                                struct timespec* ts,
                                void (*timer_handler)(void *),
                                void* param)
{
    t_polltimer* newtimer;
    t_polltimer** current = queue_head;

    newtimer=(t_polltimer*)malloc(sizeof(t_polltimer));
    newtimer->next=NULL;
    newtimer->ts=*ts;
    newtimer->param=param;
    newtimer->timer_handler=timer_handler;

    while ( *current != NULL )
        if ( (*current)->ts.tv_sec > ts->tv_sec ||
                ((*current)->ts.tv_sec == ts->tv_sec && (*current)->ts.tv_nsec > ts->tv_nsec) )
        {
            newtimer->next=*current;
            *current = newtimer;
            return newtimer;
        }
        else
            current=&( (*current)->next );

    /* last in queue */
    *current = newtimer;

    return newtimer;
}

static void periodic_handler(void* param)
{
    t_polltimer_periodic* periodic = (t_polltimer_periodic*) param;

    /* call callback function */
    (periodic->timer_handler) ( periodic->param );

    /* set next expiry */
    expire_msec(&periodic->ts,periodic->msec);

    /* queue next */
    queue_timer(periodic->queue_head,
                &periodic->ts,
                periodic_handler,
                (void*)periodic);
}


void  polltimer_periodic_start(t_polltimer** queue_head,
                               struct polltimer_periodic** periodic,
                               void (*timer_handler)(void*),
                               int msec,void* param)
{
    *periodic=(t_polltimer_periodic*)
              malloc(sizeof(t_polltimer_periodic));

    /* base time for this periodic timer */
    clock_gettime(CLOCK_REALTIME,&(*periodic)->ts);

    /* for dumping time stamps */
    if ( starttime == 0 )
        starttime=(*periodic)->ts.tv_sec;

    /* store parameters */
    (*periodic)->queue_head = queue_head;
    (*periodic)->timer_handler = timer_handler;
    (*periodic)->msec = msec;
    (*periodic)->param = param;

    /* set first expiry */
    expire_msec(&(*periodic)->ts,msec);

    queue_timer(queue_head,&(*periodic)->ts,periodic_handler,(void*)(*periodic));
}



t_polltimer* polltimer_start(t_polltimer** queue_head,
                             void (*timer_handler)(void *),
                             int msec,void* param)
{
    struct timespec ts;

    /* calculate abs. time of expiry */
    clock_gettime(CLOCK_REALTIME,&ts);

    /* for dumping time stamps */
    if ( starttime == 0 )
        starttime=ts.tv_sec;

    expire_msec(&ts,msec);

    return queue_timer(queue_head,&ts,timer_handler,param);
}




void polltimer_call_next(struct polltimer** queue_head)
{
    struct timespec ts;
    t_polltimer* next;

    clock_gettime(CLOCK_REALTIME,&ts);

    while (*queue_head != NULL)
    {

        if ( (*queue_head)->ts.tv_sec<ts.tv_sec ||
                ( (*queue_head)->ts.tv_sec==ts.tv_sec &&
                  (*queue_head)->ts.tv_nsec<ts.tv_nsec ) )
        {
            /* call handler function */
            ((*queue_head)->timer_handler) ( (*queue_head)->param );

            next=(*queue_head)->next;
            free(*queue_head);
            *queue_head=next;
        }
        else
            break;
    }
}


int polltimer_next_ms(struct polltimer* queue_head)
{
    struct timespec ts;
    long msec;

    if (queue_head != NULL )
    {
        clock_gettime(CLOCK_REALTIME,&ts);
        msec = (queue_head->ts.tv_sec - ts.tv_sec)*1000 +
               (queue_head->ts.tv_nsec - ts.tv_nsec)/1000000 +1;

        return (msec<0 ? 0 : (int) msec);
    }
    else
        return -1;
}



void polltimer_cancel(struct polltimer** queue_head,
                      struct polltimer* canceltimer)
{
    struct polltimer* current=*queue_head;
    struct polltimer** previousnext=queue_head;

    while ( current != NULL )
    {
        if ( current == canceltimer )
        {
            *previousnext=current->next;
            free(current);
            return;
        }

        previousnext=&(current->next);
        current=current->next;
    }
}

