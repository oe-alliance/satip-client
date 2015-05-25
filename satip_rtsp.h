/*
 * satip: RTSP processing
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

#ifndef _SATIP_RTSP_H
#define _SATIP_RTSP_H

#include "polltimer.h"
#include "satip_config.h"


struct satip_rtsp;

#define  SATIP_RTSP_OK         0
#define  SATIP_RTSP_ERROR      1
#define  SATIP_RTSP_COMPLETE   2

struct satip_rtsp* satip_rtsp_new(t_satip_config* satip_config,
                                  struct polltimer** timer_queue,
                                  const char* host,
                                  const char* port,
                                  int rtp_port );

int   satip_rtsp_socket(struct satip_rtsp* rtsp);
void  satip_rtsp_pollevents(struct satip_rtsp* rtsp, short events);
short satip_rtsp_pollflags(struct satip_rtsp* rtsp);
void  satip_rtsp_check_update(struct satip_rtsp*  rtsp);

#endif

