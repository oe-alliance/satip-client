/*
 * satip: startup and main loop
 *
 * Copyright (C) 2014  mc.fishdish@gmail.com
 * DVB-C and DVB-T(2) support added by ferranti@topmail.net
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

#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <errno.h>
#include <poll.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sched.h>

#include "satip_config.h"
#include "satip_vtuner.h"
#include "satip_rtsp.h"
#include "satip_rtp.h"
#include "log.h"
#include "polltimer.h"



int dbg_level = MSG_ERROR;
unsigned int dbg_mask = MSG_MAIN | MSG_NET | MSG_HW | MSG_SRV; // MSG_DATA
int use_syslog = 0;
int enabled_workarounds = 0;

//#define TEST_SEQUENCER

#ifdef TEST_SEQUENCER

static int cseq=0;

static void test_sequencer(void* param)
{
    t_satip_config* sc=(t_satip_config*)param;

    cseq++;

    switch (cseq)
    {
    case 1:
        if ( (sc->fe_type == SATIP_FE_TYPE_SAT) )
        {
            satip_set_freq(sc,118360);
            satip_set_modsys(sc, SATIPCFG_MS_DVB_S);
            satip_set_pilots(sc, SATIPCFG_P_OFF);
            satip_set_fecinner(sc, SATIPCFG_F_34);
            satip_set_polarization(sc, SATIPCFG_P_HORIZONTAL);
            satip_set_rolloff(sc, SATIPCFG_R_0_35);
            satip_set_symbol_rate(sc,27500);
            satip_set_modtype(sc, SATIPCFG_MT_QPSK);
            satip_set_position(sc, 1);
            satip_add_pid(sc, 18);
        }
        else if ( (sc->fe_type == SATIP_FE_TYPE_CAB) )
        {
            satip_set_freq(sc,4180);
            satip_set_modsys(sc, SATIPCFG_MS_DVB_C);
            satip_set_symbol_rate(sc,6900);
            satip_set_modtype(sc, SATIPCFG_MT_256QAM);
            satip_add_pid(sc, 6000);
        }
        break;

    case 200:
        //satip_add_pid(sc, 19);
        break;

    case 400:
        break;

    default:
        break;

    }
}

#endif


static void enable_rt_scheduling()
{
    struct sched_param schedp;

    if ( mlockall(MCL_CURRENT|MCL_FUTURE) )
        DEBUG(MSG_MAIN,"Pages not locked\n");
    else
        DEBUG(MSG_MAIN,"Pages locked\n");

    schedp.sched_priority = sched_get_priority_min(SCHED_FIFO);

    if ( sched_setscheduler(0, SCHED_FIFO, &schedp) )
        DEBUG(MSG_MAIN,"No realtime scheduling\n");
    else
        DEBUG(MSG_MAIN,"Realtime scheduling enabled at prio %d\n",schedp.sched_priority);

}

void print_usage(void)
{
    printf("Usage: satip-client <options>\n"
           "       -s <ip address>      IP address of SAT>IP server (MANDATORY)\n"
           "       -t <frontend_type>   Frontend type: (MANDATORY)\n"
           "                               0: DVB-S2\n"
           "                               1: DVB-C\n"
           "                               2: DVB-T\n"
           "       -p <port>            Port number of SAT>IP server (default: 554)\n"
           "       -d <vtuner_device>   Vtuner device (default: /dev/misc/vtuner0)\n"
           "       -f <num>             Force usage of frontend <num> on SAT>IP server\n"
           "       -m <debug_mask>      Used for debugging, see code\n"
           "       -l <log_level>       Log level:\n"
           "                               0: None\n"
           "                               1: Error (default)\n"
           "                               2: Warning\n"
           "                               3: Info\n"
           "                               4: Debug\n"
           "       -y                   Use syslog instead of STDERR for logging\n"
           "       -w <workaround_mask> Enable workarounds for known quirks for some device\n"
           "                               0: Disable all workarounds (default)\n"
           "                               1: Enable workaround ADD-DUMMY-PID  (AVM Fritz!WLAN Repeater DVB-C)\n"
           "                               2: Enable workaround FORCE-FULL-PID (AVM Fritz!WLAN Repeater DVB-C)\n"
           "                               4: Enable workaround FORCE-FE-LOCK  (AVM Fritz!WLAN Repeater DVB-C) CURRENTLY ALWAYS ACTIVE!\n"
           "                               Example: -w3 means enable first two workarounds"
           "       -h                   Print help\n"
          );
}

int main(int argc, char** argv)
{
    char* host = NULL;
    char* port = "554";
    char* device = "/dev/misc/vtuner0";
    int frontend = -1;
    int type = -1;

    t_satip_config* satconf;
    struct satip_rtsp* srtsp;
    struct satip_rtp* srtp;
    struct satip_vtuner* satvt;

    struct pollfd pollfds[2];
    int poll_idx;
    struct polltimer* timerq=NULL;

    int opt;

    while((opt = getopt(argc, argv, "s:p:d:f:m:l:t:yw:h")) != -1 )
    {
        switch(opt)
        {
        case 's':
            host = optarg;
            break;

        case 'p':
            port = optarg;
            break;

        case 'd':
            device = optarg;
            break;

        case 'f':
            frontend = atoi(optarg);
            break;

        case 'm':
            dbg_mask = atoi(optarg);
            break;

        case 'l':
            dbg_level = atoi(optarg);
            break;

        case 't':
            type = atoi(optarg);
            break;

        case 'w':
            enabled_workarounds=atoi(optarg);
            break;

        case 'y':
            use_syslog=1;
            break;

        case 'h':
        /* fall through ... */
        default:
            ERROR(MSG_MAIN,"unknown option %c\n",opt);
            print_usage();
            exit(1);
        }
    }

    if ( host==NULL )
    {
        ERROR(MSG_MAIN,"No server IP argument. Please specify SAT>IP server address.\n");
        print_usage();
        exit(1);
    }

    if ( type==-1 )
    {
        ERROR(MSG_MAIN,"No type argument. Please specify frontend type.\n");
        print_usage();
        exit(1);
    }

    enable_rt_scheduling();

    satconf = satip_new_config(frontend, type);

#ifdef TEST_SEQUENCER

    struct polltimer_periodic* periodic;

    device = device;
    satvt = NULL;
    polltimer_periodic_start(&timerq,
                             &periodic,
                             test_sequencer,
                             10,
                             (void*)satconf);

    srtp  = satip_rtp_new(0);

    /* no vtuner fd*/
    poll_idx=0;

#else

    satvt = satip_vtuner_new( device, satconf );

    if ( satvt == NULL )
    {
        ERROR(MSG_MAIN,"vtuner control failed\n");
        exit(1);
    }

    srtp  = satip_rtp_new(satip_vtuner_fd(satvt));

    pollfds[0].fd=satip_vtuner_fd(satvt);
    pollfds[0].events = POLLPRI;
    poll_idx=1;

#endif

    srtsp = satip_rtsp_new(satconf,&timerq, host, port,
                           satip_rtp_port(srtp));


    while (1)
    {
        /* apply any updates on rtsp  */
        satip_rtsp_check_update(srtsp);

        /* vt control events */
        pollfds[0].revents = 0;

        /* rtsp socket may be closed */
        pollfds[poll_idx].fd = satip_rtsp_socket(srtsp);
        pollfds[poll_idx].events = satip_rtsp_pollflags(srtsp);
        pollfds[poll_idx].revents = 0;

        /* poll and timeout on next pending timer */
        if ( poll(pollfds, pollfds[poll_idx].events == 0 ? poll_idx : poll_idx+1 ,
                  polltimer_next_ms(timerq) ) ==-1 &&
                errno!=EINTR )
        {
            perror(NULL);
            exit(1);
        }

        /* schedule timer callbacks */
        polltimer_call_next(&timerq);

        /* vt control event handling */
        if ( poll_idx>0 && pollfds[0].revents !=0 )
            satip_vtuner_event(satvt);

        /* rtsp event handling */
        if ( pollfds[poll_idx].revents !=0 )
            satip_rtsp_pollevents(srtsp, pollfds[poll_idx].revents);
    }

    return 0;

}

