/*
 * satip: startup and main loop
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

#include <stdio.h>
#include <unistd.h>
#include <signal.h>

#include <sched.h>
#include <sys/mman.h>

#include <sys/types.h>
#include <sys/stat.h>

#include "session.h"
#include "log.h"
#include "manager.h"

int dbg_level = MSG_ERROR;

unsigned int dbg_mask = MSG_MAIN | MSG_NET | MSG_HW | MSG_SRV;
int use_syslog = 0;

bool main_running = true;

void sigint_handler(int signo)
{
	DEBUG(MSG_MAIN, "sigint_handler called\n");
	signal(SIGINT, SIG_DFL);
	sessionManager* vtmng = sessionManager::getInstance();
	vtmng->sessionStop();
	vtmng->sessionJoin();
}

void print_usage(void)
{
    printf("Usage: satip-client <options>\n"
           "       -m <debug_mask>      Used for debugging, see code\n"
           "       -l <log_level>       Log level: (default: 1)\n"
           "                               0: None\n"
           "                               1: Error\n"
           "                               2: Warning\n"
           "                               3: Info\n"
           "                               4: Debug\n"
           "       -y                   Use syslog instead of STDERR for logging\n"
           "       -h                   Print help\n"
                                             );
}

#ifdef RT_SCHEDULING
static void enable_rt_scheduling()
{
  struct sched_param schedp;

  if ( mlockall(MCL_CURRENT|MCL_FUTURE) )
    DEBUG(MSG_MAIN, "Pages not locked\n");
  else
    DEBUG(MSG_MAIN, "Pages locked\n");

  schedp.sched_priority = sched_get_priority_min(SCHED_FIFO);

  if ( sched_setscheduler(0, SCHED_FIFO, &schedp) )
    DEBUG(MSG_MAIN, "No realtime scheduling\n");
  else
    DEBUG(MSG_MAIN, "Realtime scheduling enabled at prio %d\n",schedp.sched_priority);

}
#endif // RT_SCHEDULING

int main(int argc, char** argv)
{
	int opt;

	while( (opt = getopt(argc, argv, "m:l:yh") ) != -1 )
	{
		switch(opt)
		{
			case 'm':
				dbg_mask = atoi(optarg);
				break;

			case 'l':
				dbg_level = atoi(optarg);
				break;

			case 'y':
				use_syslog = 1;
				break;

			case 'h':
			default:
				print_usage();
				exit(1);
		}
	}

#ifdef RT_SCHEDULING
	enable_rt_scheduling();
#endif

	signal(SIGINT, sigint_handler);
	signal(SIGTERM, sigint_handler);

	sessionManager* vtmng = sessionManager::getInstance();
	vtmng->satipStart();
	pause();

	DEBUG(MSG_MAIN,"End MAIN\n");

	return 0;
}

