/*
 * satip: session
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
#include <string>
#include <poll.h>
#include <errno.h>

#include "session.h"
#include "config.h"
#include "vtuner.h"
#include "rtp.h"
#include "rtsp.h"
#include "log.h"
#include "option.h"

satipSession::satipSession(const char* host,
							const char* rtsp_port,
							int fe_type,
							vtunerOpt* settings,
							int& initok):
							m_satip_config(NULL),
							m_satip_vtuner(NULL),
							m_satip_rtp(NULL),
							m_satip_rtsp(NULL),
							m_session_thread(0),
							m_running(false)
{
	int rtp_port;

	DEBUG(MSG_MAIN,"Create SESSION.(host : %s, rtsp_port : %s, fe_type : %d\n",
		host, rtsp_port, fe_type);
	m_satip_config = new satipConfig(fe_type, settings);
	m_satip_vtuner = new satipVtuner(m_satip_config);
	m_satip_rtp  = new satipRTP(m_satip_vtuner->getVtunerFd(), settings->m_tcpdata);

	m_satip_vtuner->setSatipRTP(m_satip_rtp); // for receive RTCP data

	m_satip_rtsp = new satipRTSP(m_satip_config, host, rtsp_port, m_satip_rtp);

	if (m_satip_vtuner->isOpened() && m_satip_rtp->isOpened())
		initok = 1;
}

satipSession::~satipSession()
{
	DEBUG(MSG_MAIN,"Destruct SESSION.\n");
	stop();

	if (m_satip_rtsp)
		delete m_satip_rtsp;

	if (m_satip_rtp)
		delete m_satip_rtp;	

	if (m_satip_vtuner)
		delete m_satip_vtuner;

	if (m_satip_config)
		delete m_satip_config;
}

void *satipSession::thread_wrapper(void *ptr)
{
	return ((satipSession*)ptr)->satipMainLoop();
}

void *satipSession::satipMainLoop()
{
	struct pollfd poll_fds[2];
	int poll_nfds;
	int poll_ret;
	int poll_timeout = 1000;

	poll_fds[0].fd = m_satip_vtuner->getVtunerFd();
	poll_fds[0].events = POLLPRI;
	poll_nfds=1;

	while (m_running)
	{
		/* loop */
		m_satip_rtsp->handleRTSPStatus();

		poll_nfds = 1;
		poll_fds[1].fd = m_satip_rtsp->getRtspSocketFd();
		poll_fds[1].events = m_satip_rtsp->getPollEvent();
		poll_fds[1].revents = 0;
		if (poll_fds[1].events != 0)
		{
			poll_nfds++;
		}

		poll_timeout = m_satip_rtsp->getPollTimeout();

//		DEBUG(MSG_MAIN, "poll_timeout : %d\n", poll_timeout);

		poll_ret = poll(poll_fds, poll_nfds, poll_timeout);

//		DEBUG(MSG_MAIN, "poll_ret : %d\n", poll_ret);

		if (poll_ret == -1 && errno == EINTR)
			DEBUG(MSG_MAIN, "poll EINTR.\n");
		
		if (poll_ret == -1 && errno != EINTR)
		{
			perror ("poll error : ");
			break;
		}

		m_satip_rtsp->handleNextTimer();

		if (poll_fds[0].revents != 0)
			m_satip_vtuner->vtunerEvent();

		if (poll_fds[1].revents != 0)
			m_satip_rtsp->handlePollEvents(poll_fds[1].revents);

	}
	return 0;
}

void satipSession::start()
{
	m_satip_rtp->run();
	run();
}

void satipSession::run()
{
	m_running = true;
	pthread_create( &m_session_thread, NULL, thread_wrapper, this);
}

void satipSession::stop()
{
	m_satip_rtp->stop();
	m_running = false;
}

void satipSession::join()
{
	if (m_session_thread) 
	{
		int status;
		pthread_join(m_session_thread, (void **)&status);
		DEBUG(MSG_MAIN,"SATIP session thread END : %d.\n", status);
		m_session_thread = 0;
	}
}

