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

#ifndef __SESSION_H__
#define __SESSION_H__

#include "config.h"
#include "vtuner.h"
#include "rtsp.h"
#include "rtp.h"
#include "session.h"

#include <string>
#include <pthread.h>

class Session
{
public:
		virtual void start() = 0;
		virtual void run() = 0;
		virtual void stop() = 0;
		virtual void join() = 0;
};

class satipSession:public Session
{
	satipConfig* m_satip_config;
	satipVtuner* m_satip_vtuner;
	satipRTP* m_satip_rtp;
	satipRTSP* m_satip_rtsp;
	pthread_t m_session_thread;
	bool m_running;

	void *satipMainLoop();
	static void *thread_wrapper(void *ptr);

public:
	satipSession(const char* host,
							const char* rtsp_port,
							int fe_type,
							int& initok);

	virtual ~satipSession();
	void start();
	void run();
	void stop();
	void join();
};

#endif // __SESSION_H__

