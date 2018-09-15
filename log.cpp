/*
 * satip: logging
 *
 * Copyright (C) 2014 mc.fishdish@gmail.com
 * [copy of vtuner by Honza Petrous]
 * Copyright (C) 2010-11 Honza Petrous <jpetrous@smartimp.cz>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation version 2.
 *
 * This program is distributed WITHOUT ANY WARRANTY of any
 * kind, whether express or implied; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <stdarg.h>
#include <time.h>

#define MAX_MSGSIZE 1024
#include "log.h"

__thread char msg[MAX_MSGSIZE];

void write_message(const unsigned int mtype, const int level, const char* fmt, ... ) {
	if( !(mtype & dbg_mask ) )
		return;

	if( level <= dbg_level ) {

		char tn[MAX_MSGSIZE];
		va_list ap;
		va_start(ap, fmt);
		vsnprintf(tn, sizeof(tn)-1, fmt, ap);
		va_end(ap);
		strncat(msg, tn, sizeof(msg)-1);
		msg[sizeof(tn)-1] = '\0';

		if(use_syslog) {
			int priority;
			switch(level) {
				case 1: priority=LOG_ERR; break;
				case 2: priority=LOG_WARNING; break;
				case 3: priority=LOG_INFO; break;
				default: priority=LOG_DEBUG; break;
			}
			syslog(priority, "%s", msg);
		} else {
			struct timespec tp;
			clock_gettime(CLOCK_MONOTONIC_COARSE, &tp);
			fprintf(stderr, "[%ld.%03ld]%s", (long)tp.tv_sec, (long)tp.tv_nsec / 1000000L, msg);
		}
	}

	strncpy(msg, "", sizeof(msg));
} 

