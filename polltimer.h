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

#ifndef _POLLTIMER_H
#define _POLLTIMER_H

struct polltimer;

struct polltimer* polltimer_start(struct polltimer** queue_head,
                                  void (*timer_handler)(void *),
                                  int msec,void* param);

void polltimer_cancel(struct polltimer** queue_head,
                      struct polltimer* canceltimer);



struct polltimer_periodic;

void  polltimer_periodic_start(struct polltimer** queue_head,
                               struct polltimer_periodic** periodic,
                               void (*timer_handler)(void *),
                               int msec,void* param);


void polltimer_call_next(struct polltimer** queue_head);


int polltimer_next_ms(struct polltimer* queue_head);

#endif
