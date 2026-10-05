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
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>

#include <sched.h>
#include <sys/mman.h>

#include <sys/types.h>
#include <sys/stat.h>

#include "session.h"
#include "log.h"
#include "manager.h"

int dbg_level = MSG_ERROR;

/* MSG_DATA is left out on purpose, it logs every single rtp packet */
unsigned int dbg_mask = MSG_MAIN | MSG_NET | MSG_HW | MSG_SRV | MSG_CA;
int use_syslog = 0;

bool main_running = true;

/* Set by the signal handler; read by main(). sig_atomic_t assignment is
 * async-signal-safe. All logging (fprintf/syslog) and session teardown
 * (sessionStop/sessionJoin, pthread_join) run in main() after wakeup, never
 * in the handler, where they could deadlock against an interrupted call. */
static volatile sig_atomic_t shutdown_signo = 0;

void sigint_handler(int signo)
{
	if (shutdown_signo) {
		/* Second signal while shutting down: restore the default
		 * disposition and re-raise so the process can still be
		 * killed. signal() and raise() are async-signal-safe. */
		signal(signo, SIG_DFL);
		raise(signo);
		return;
	}
	shutdown_signo = signo;
}

void print_usage(void)
{
    printf("Usage: satip-client <options>\n"
           "       -m <debug_mask>      Bitmask, default 47:\n"
           "                               1: main\n"
           "                               2: net\n"
           "                               4: hw\n"
           "                               8: srv\n"
           "                              16: data (one line per rtp packet!)\n"
           "                              32: ca (ecm/emm pid detection)\n"
           "       -l <log_level>       Log level: (default: 1)\n"
           "                               0: None\n"
           "                               1: Error\n"
           "                               2: Warning\n"
           "                               3: Info\n"
           "                               4: Debug\n"
           "       -y                   Use syslog instead of STDERR for logging\n"
           "       -f <log_file>        Append log messages to file instead of STDERR\n"
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

	while( (opt = getopt(argc, argv, "m:l:yf:h") ) != -1 )
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

			case 'f':
			{
				FILE *f = fopen(optarg, "a");
				if (!f)
				{
					fprintf(stderr, "Cannot open log file '%s': %s\n", optarg, strerror(errno));
					exit(1);
				}
				setvbuf(f, NULL, _IOLBF, 0);
				if (log_file)
					fclose(log_file);
				log_file = f;
				break;
			}

			case 'h':
			default:
				print_usage();
				exit(1);
		}
	}

#ifdef RT_SCHEDULING
	enable_rt_scheduling();
#endif

	/* Block SIGINT/SIGTERM before worker threads are created so they
	 * inherit the blocked mask and the signals are always delivered to
	 * the main thread waiting in sigsuspend() below. */
	sigset_t block_set, prev_mask, suspend_mask;
	sigemptyset(&block_set);
	sigaddset(&block_set, SIGINT);
	sigaddset(&block_set, SIGTERM);
	pthread_sigmask(SIG_BLOCK, &block_set, &prev_mask);

	struct sigaction sa;
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = sigint_handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);
	/* NOTE: SIGKILL cannot be caught, blocked, or handled; there is
	 * intentionally no handler for it. */

	sessionManager* vtmng = sessionManager::getInstance();
	int res = vtmng->satipStart();
	if (!res) {
		/* Atomically unblock and wait; unlike pause() this cannot miss
		 * a signal that arrives between the flag check and the wait. */
		suspend_mask = prev_mask;
		sigdelset(&suspend_mask, SIGINT);
		sigdelset(&suspend_mask, SIGTERM);
		while (!shutdown_signo)
			sigsuspend(&suspend_mask);

		pthread_sigmask(SIG_SETMASK, &prev_mask, NULL);

		DEBUG(MSG_MAIN, "received signal %d, shutting down\n", (int)shutdown_signo);
		vtmng->sessionStop();
		vtmng->sessionJoin();
	} else {
		pthread_sigmask(SIG_SETMASK, &prev_mask, NULL);
	}

	DEBUG(MSG_MAIN,"End MAIN\n");

	/* Do not fclose(log_file) here: static destructors (e.g.
	   ~sessionManager) still log after main() returns. The file
	   stays open until process exit; every message is flushed. */

	return 0;
}

