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

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <poll.h>

#include "satip_config.h"
#include "satip_rtsp.h"
#include "polltimer.h"
#include "log.h"

extern int enabled_workarounds;

typedef enum
{
    RTSP_NOCONFIG = 0,
    RTSP_CONNECTING,
    RTSP_ESTABLISHING,  /* try to get stream ID */
    RTSP_READY,         /* connected with stream ID and no request pending */
    RTSP_WAITING,       /* waiting for response */
} t_rtsp_state;


typedef enum
{
    RTSP_REQ_OPTIONS = 0,
    RTSP_REQ_SETUP,
    RTSP_REQ_PLAY,
    RTSP_REQ_TEARDOWN,
    RTSP_REQ_NONE
} t_rtsp_request;


#define MAX_BUF 1024
#define MAX_SESSION 50

typedef struct satip_rtsp
{
    t_rtsp_state status;

    int sockfd;
    char* host;
    char* port;
    int rtp_port;
    struct addrinfo* addrinfo;

    t_satip_config* satip_config;

    struct polltimer** timer_queue;
    struct polltimer* timer;

    t_rtsp_request request;
    int cseq;
    int streamid;
    char session[MAX_SESSION];
    int timeout;

    char txbuf[MAX_BUF];

    char rxbuf[MAX_BUF];
    int rxbuf_pos;

} t_satip_rtsp;


static int handle_response_options(t_satip_rtsp* rtsp);
static int handle_response_setup(t_satip_rtsp* rtsp);
static int handle_response_play(t_satip_rtsp* rtsp);
static int handle_response_teardown(t_satip_rtsp* rtsp);

int(*handle_response[])(t_satip_rtsp*)=
{
    handle_response_options,
    handle_response_setup,
    handle_response_play,
    handle_response_teardown
};




static void restart_connection(t_satip_rtsp* rtsp,int now);





static void reset_connection(t_satip_rtsp* rtsp)
{
    rtsp->status = RTSP_NOCONFIG;

    rtsp->cseq = 1;
    rtsp->request = RTSP_REQ_NONE;
    rtsp->timeout = 30;
    rtsp->session[0] = 0;

    rtsp->txbuf[0]=0;

    rtsp->rxbuf_pos=0;
    rtsp->rxbuf[0]=0;


    if (rtsp->timer != NULL)
    {
        polltimer_cancel(rtsp->timer_queue,rtsp->timer);
        rtsp->timer=NULL;
    }

    if (rtsp->sockfd>=0)
    {
        DEBUG(MSG_NET,"closing socket\n");
        close(rtsp->sockfd);
        rtsp->sockfd=-1;
    }
}


static void timeout_reconnect(void* param)
{
    t_satip_rtsp* rtsp=(t_satip_rtsp*)param;

    DEBUG(MSG_NET,"timeout\n");

    /* timer expired, clear it */
    rtsp->timer = NULL;

    restart_connection(rtsp,1);
}





t_satip_rtsp* satip_rtsp_new(t_satip_config* satip_config,
                             struct polltimer** timer_queue,
                             const char* host,
                             const char* port,
                             int rtp_port)
{
    t_satip_rtsp* rtsp;

    rtsp=(t_satip_rtsp*)malloc(sizeof(t_satip_rtsp));

    rtsp->host=strdup(host);
    rtsp->port=strdup(port);
    rtsp->rtp_port=rtp_port;

    rtsp->satip_config= satip_config;
    rtsp->timer_queue = timer_queue;

    rtsp->timer = NULL;
    rtsp->sockfd = -1;

    /* reset dynamic parts*/
    reset_connection(rtsp);

    return rtsp;
}


static void restart_connection(t_satip_rtsp* rtsp,int now)
{
    reset_connection(rtsp);

    if ( now )
    {
        satip_rtsp_check_update(rtsp);
    }
    else
    {
        /* attempt again after some time*/
        rtsp->timer = polltimer_start( rtsp->timer_queue,
                                       timeout_reconnect,
                                       2000,(void*)rtsp);
    }
}

static int read_response(t_satip_rtsp* rtsp)
{
    int rec;

    rec=recv(rtsp->sockfd,
             &(rtsp->rxbuf[rtsp->rxbuf_pos]),
             MAX_BUF-rtsp->rxbuf_pos, 0);

    if ( rec==0 )
        return SATIP_RTSP_ERROR;

    rtsp->rxbuf_pos += rec;
    rtsp->rxbuf[rtsp->rxbuf_pos]=0;

    DEBUG(MSG_NET,"rxbuf:\n%s\n<<\n",rtsp->rxbuf);


    if (strstr(rtsp->rxbuf,"\r\n\r\n") == NULL )
    {
        return SATIP_RTSP_OK;
    }
    else
    {
        /* reset buffer index for next response */
        rtsp->rxbuf_pos=0;

        /* check basic status code */
        int ret=0;
        sscanf(rtsp->rxbuf,"RTSP/%*s %d",&ret);
        if (ret!=200)
            return SATIP_RTSP_ERROR;

        /* request specific evaluation of response */
        return (*handle_response[rtsp->request])(rtsp);
    }
}

static int handle_response_options(t_satip_rtsp* rtsp)
{
    return SATIP_RTSP_COMPLETE;
}


static int handle_response_setup(t_satip_rtsp* rtsp)
{
    char* str;

    str=strstr(rtsp->rxbuf,"\ncom.ses.streamID:");
    if ( str==NULL  || sscanf(str,"\ncom.ses.streamID: %d",&rtsp->streamid) != 1 )
        return SATIP_RTSP_ERROR;

    DEBUG(MSG_NET,"streamid %d\n",rtsp->streamid);

    str=strstr(rtsp->rxbuf,"\nSession:");
    if ( str==NULL || sscanf(str,"\nSession: %s",rtsp->session) !=1 )
        return SATIP_RTSP_ERROR;

    str=strstr(rtsp->session,";timeout=");
    if ( str!=NULL )
    {
        if ( sscanf(str,";timeout= %d",&rtsp->timeout) != 1 )
            return SATIP_RTSP_ERROR;

        /* terminate session at ";" */
        *str=0;
        DEBUG(MSG_NET,"timeout: %d\n",rtsp->timeout);
    }

    DEBUG(MSG_NET,"Session: %s\n",rtsp->session);

    return SATIP_RTSP_COMPLETE;
}


static int handle_response_play(t_satip_rtsp* rtsp)
{
    return SATIP_RTSP_COMPLETE;
}

static int handle_response_teardown(t_satip_rtsp* rtsp)
{
    reset_connection(rtsp);

    return SATIP_RTSP_OK;
}


static int send_options(t_satip_rtsp* rtsp)
{
    int printed;

    printed =
        snprintf(rtsp->txbuf,MAX_BUF,"OPTIONS rtsp://%s:%s/ RTSP/1.0\r\n"
                 "CSeq: %d\r\n"
                 "%s%s%s",
                 rtsp->host, rtsp->port,
                 rtsp->cseq++,
                 rtsp->session[0] ? "Session: " : "",
                 rtsp->session ,
                 rtsp->session[0] ? "\r\n\r\n" : "\r\n"
                );

    if ( printed >= MAX_BUF )
        return SATIP_RTSP_ERROR;

    DEBUG(MSG_NET,">>txbuf:\n%s\n<<\n",rtsp->txbuf);

    if ( send(rtsp->sockfd,rtsp->txbuf,printed,0) != printed )
        return SATIP_RTSP_ERROR;

    return SATIP_RTSP_OK;
}


static int send_teardown(t_satip_rtsp* rtsp)
{
    int printed;

    printed =
        snprintf(rtsp->txbuf,MAX_BUF,"TEARDOWN rtsp://%s/stream=%d RTSP/1.0\r\n"
                 "CSeq: %d\r\n"
                 "Session: %s\r\n\r\n",
                 rtsp->host,
                 rtsp->streamid,
                 rtsp->cseq++,
                 rtsp->session);

    if ( printed >= MAX_BUF )
        return SATIP_RTSP_ERROR;

    DEBUG(MSG_NET,">>txbuf:\n%s\n<<\n",rtsp->txbuf);

    if ( send(rtsp->sockfd,rtsp->txbuf,printed,0) != printed )
        return SATIP_RTSP_ERROR;

    return SATIP_RTSP_OK;
}



static int send_setup(t_satip_rtsp* rtsp)
{
    char* buf=rtsp->txbuf;
    int printed;
    int remain=MAX_BUF;

    printed = snprintf(buf,remain,"SETUP rtsp://%s/?", rtsp->host);
    if ( printed >= remain )
        return SATIP_RTSP_ERROR;


    printed += satip_prepare_tuning(rtsp->satip_config,buf+printed,remain-printed);
    if ( printed >= remain )
        return SATIP_RTSP_ERROR;

    printed += snprintf(buf+printed,remain-printed,"&");

    if ( printed >= remain )
        return SATIP_RTSP_ERROR;

#if 1
    printed += satip_prepare_pids(rtsp->satip_config,buf+printed,remain-printed,0);
    if ( printed >= remain )
        return SATIP_RTSP_ERROR;


    satip_settle_config(rtsp->satip_config);

#endif

#if 1
    printed += snprintf(buf+printed,remain-printed," RTSP/1.0\r\n"
                        "CSeq: %d\r\n"
                        "Transport: RTP/AVP;unicast;client_port=%d-%d\r\n\r\n",
                        rtsp->cseq++,rtsp->rtp_port,rtsp->rtp_port+1);

#else
    printed += snprintf(buf+printed,remain-printed," RTSP/1.0\r\n"
                        "CSeq: %d\r\n"
                        "Transport: RTP/AVP;multicast;destination=224.16.16.1;port=%d-%d\r\n\r\n",
                        rtsp->cseq++,rtsp->rtp_port,rtsp->rtp_port+1);
#endif


    if ( printed >= remain )
        return SATIP_RTSP_ERROR;

    DEBUG(MSG_NET,">>txbuf:\n%s\n<<\n",rtsp->txbuf);

    if ( send(rtsp->sockfd,rtsp->txbuf,printed,0) != printed )
        return  SATIP_RTSP_ERROR ;

    return SATIP_RTSP_OK;
}


static int send_play(t_satip_rtsp* rtsp)
{
    char* buf=rtsp->txbuf;
    int printed;
    int remain=MAX_BUF;
    int tuning,pid_update;

    tuning = satip_tuning_required(rtsp->satip_config);
    pid_update = satip_pid_update_required(rtsp->satip_config);

    printed = snprintf(buf,remain,"PLAY rtsp://%s/stream=%d%s",
                       rtsp->host,rtsp->streamid,
                       (tuning || pid_update) ? "?" : "");
    if ( printed>=remain )
        return SATIP_RTSP_ERROR;

#if 1

    if ( tuning )
    {
        printed += satip_prepare_tuning(rtsp->satip_config, buf+printed, remain-printed);
        if ( printed>=remain )
            return SATIP_RTSP_ERROR;

        printed += snprintf(buf+printed,remain-printed,"&");

        if ( printed >= remain )
            return SATIP_RTSP_ERROR;

        printed += satip_prepare_pids(rtsp->satip_config, buf+printed, remain-printed,0);
        if ( printed>=remain )
            return SATIP_RTSP_ERROR;

    }
    else if (  pid_update )
    {
        // Add workaround for Fritzdvbc quirk: use full pid updates
        if (enabled_workarounds & 0x02)
            printed += satip_prepare_pids(rtsp->satip_config, buf+printed, remain-printed,0);
        else
            printed += satip_prepare_pids(rtsp->satip_config, buf+printed, remain-printed,1);
        if ( printed>=remain )
            return SATIP_RTSP_ERROR;
    }
#else


    printed += snprintf(buf+printed,remain-printed,"pids=all");
    if ( printed>=remain )
        return SATIP_RTSP_ERROR;
#endif

    satip_settle_config(rtsp->satip_config);

    printed += snprintf(buf+printed,remain-printed," RTSP/1.0\r\n"
                        "CSeq: %d\r\n"
                        "%s%s%s",
                        rtsp->cseq++,
                        rtsp->session[0] ? "Session: " : "",
                        rtsp->session,
                        rtsp->session[0] ? "\r\n\r\n" : "\r\n"
                       );

    if ( printed >= remain )
        return SATIP_RTSP_ERROR;

    DEBUG(MSG_NET,">>play:\n%s\n<<\n",rtsp->txbuf);

    if ( send(rtsp->sockfd,rtsp->txbuf,printed,0) != printed )
        return  SATIP_RTSP_ERROR ;

    return SATIP_RTSP_OK;
}



static int connect_server(t_satip_rtsp* rtsp )
{
    int sockfd,s;
    int flags;
    struct addrinfo hints;
    struct addrinfo *result;
    struct addrinfo *rp;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;    /* IPv4 or IPv6 */
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = 0;
    hints.ai_protocol = 0;

    s = getaddrinfo(rtsp->host, rtsp->port, &hints, &result);
    if (s != 0)
    {
        ERROR(MSG_NET, "getaddrinfo: %s\n", gai_strerror(s));
        return(SATIP_RTSP_ERROR);
    }

    for (rp = result; rp != NULL; rp = rp->ai_next)
    {
        sockfd = socket(rp->ai_family, rp->ai_socktype,
                        rp->ai_protocol);
        if (sockfd == -1)
            continue;

        flags=fcntl(sockfd,F_GETFL,0);
        fcntl(sockfd,F_SETFL,flags | O_NONBLOCK);

        if (connect(sockfd, rp->ai_addr, rp->ai_addrlen) == -1 &&
                errno == EINPROGRESS )
            break;

        close(sockfd);
    }

    if (rp == NULL)
    {
        DEBUG(MSG_NET, "Could not connect\n");
        freeaddrinfo(result);
        return(SATIP_RTSP_ERROR);
    }

    rtsp->sockfd = sockfd;
    rtsp->addrinfo = rp;

    return(SATIP_RTSP_OK);
}


int satip_rtsp_socket(t_satip_rtsp* rtsp)
{
    return rtsp->sockfd;
}


static void send_request(t_satip_rtsp* rtsp,
                         t_rtsp_state newstate,
                         t_rtsp_request request,
                         int(*sendfunc)(t_satip_rtsp*))
{
    /* stop supervision timer*/
    polltimer_cancel(rtsp->timer_queue, rtsp->timer);

    rtsp->request = request;
    rtsp->status  = newstate;

    /* send request and start timer */
    if ( (*sendfunc)(rtsp) == SATIP_RTSP_OK )
        rtsp->timer = polltimer_start( rtsp->timer_queue,
                                       timeout_reconnect,
                                       2000,(void*)rtsp);
    else
        restart_connection(rtsp,0);
}

static void timeout_keep_alive(void* param)
{
    t_satip_rtsp* rtsp=(t_satip_rtsp*)param;

    DEBUG(MSG_NET,"keep_alive\n");

    /* timer expired, clear it */
    rtsp->timer = NULL;

    send_request(rtsp, RTSP_READY, RTSP_REQ_OPTIONS, send_options);

}



void satip_rtsp_pollevents(t_satip_rtsp* rtsp, short events)
{

    if ( events & POLLHUP )
    {
        /* connection rejected (port closed) */
        DEBUG(MSG_NET,"connection rejected\n");
        restart_connection(rtsp,0);
        return;
    }

    switch ( rtsp->status )
    {
    case RTSP_NOCONFIG:
        break;

    case RTSP_CONNECTING:
        if ( events & POLLOUT )
        {
            DEBUG(MSG_NET,"connected -> establishing\n");
            send_request(rtsp, RTSP_ESTABLISHING, RTSP_REQ_OPTIONS, send_options);
        }
        break;

    case RTSP_ESTABLISHING:
        if ( events & POLLIN )
        {
            int ret=read_response(rtsp);

            if ( ret==SATIP_RTSP_ERROR )
            {
                DEBUG(MSG_NET,"peer closed\n");

                restart_connection(rtsp,0);
            }
            else if ( ret==SATIP_RTSP_COMPLETE )
            {
                DEBUG(MSG_NET,"response complete\n");
                if ( rtsp->request == RTSP_REQ_OPTIONS )
                    send_request(rtsp, RTSP_ESTABLISHING, RTSP_REQ_SETUP, send_setup);
                else if (rtsp->request == RTSP_REQ_SETUP )
                    send_request(rtsp, RTSP_READY, RTSP_REQ_PLAY, send_play);
                else
                {
                    DEBUG(MSG_NET,"bug..\n");
                    restart_connection(rtsp,0);
                }
            }
            /* SATIP_RTSP_OK: response not yet complete, wait for more data.. */

        }
        break;

    case RTSP_READY:
        if ( events & POLLIN )
        {
            int ret=read_response(rtsp);

            if ( ret==SATIP_RTSP_ERROR )
            {
                restart_connection(rtsp,0);
            }
            else if ( ret==SATIP_RTSP_COMPLETE )
            {
                polltimer_cancel(rtsp->timer_queue,rtsp->timer);

                rtsp->request=RTSP_REQ_NONE;

                satip_rtsp_check_update(rtsp);

                if ( rtsp->request == RTSP_REQ_NONE )

                    rtsp->timer = polltimer_start( rtsp->timer_queue,
                                                   timeout_keep_alive,
                                                   (rtsp->timeout-5)*1000,(void*)rtsp);
            }
            /* SATIP_RTSP_OK: response not yet complete, wait for more data.. */

        }
        break;

    default:
        break;
    }


}



short satip_rtsp_pollflags(t_satip_rtsp* rtsp)
{
    short flags=0;

    switch ( rtsp->status )
    {
    case RTSP_NOCONFIG:
        break;

    case RTSP_CONNECTING:
        flags = POLLHUP | POLLOUT;
        break;

    case RTSP_ESTABLISHING:
    case RTSP_READY:
        flags = POLLHUP | POLLIN;
        break;

    default:

        break;
    }

    return flags;

}







void  satip_rtsp_check_update(struct satip_rtsp*  rtsp)
{
    switch ( rtsp->status )
    {
    case RTSP_NOCONFIG:
        if ( satip_valid_config(rtsp->satip_config) &&
                rtsp->timer == NULL )
        {
            DEBUG(MSG_NET,"connecting...\n");
            rtsp->timer = polltimer_start( rtsp->timer_queue,
                                           timeout_reconnect,
                                           5000,(void*)rtsp);

            if ( connect_server(rtsp) == SATIP_RTSP_ERROR )
                /* network down, DNS error,... */
                DEBUG(MSG_NET,"connect failed, waiting...\n");
            else
                rtsp->status = RTSP_CONNECTING;
        }
        break;

    case RTSP_READY:
        if ( rtsp->request == RTSP_REQ_NONE )
        {
            if ( satip_tuning_required(rtsp->satip_config) ||
                    satip_pid_update_required(rtsp->satip_config))
                send_request(rtsp, RTSP_READY, RTSP_REQ_PLAY, send_play);

            else if ( !satip_valid_config(rtsp->satip_config) )
                send_request(rtsp, RTSP_READY, RTSP_REQ_TEARDOWN, send_teardown);
        }
        break;

    case RTSP_CONNECTING:
    case RTSP_ESTABLISHING:
        break;

    default:

        break;
    }

}
