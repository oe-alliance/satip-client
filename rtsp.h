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

#ifndef __RTSP_H__
#define __RTSP_H__

#include "timer.h"
#include "config.h"
#include <string>

enum 
{
	RTSP_STATUS_CONFIG_WAITING = 0, // need to check if tuner config is completed.
	RTSP_STATUS_SERVER_CONNECTING, // try to server connecting..
	RTSP_STATUS_SESSION_ESTABLISHING, // connected, send to setup..and receive response to get streamID, sessionID, timeout..
	RTSP_STATUS_SESSION_PLAYING, // session established, send to play and wait until receive play ok.
	RTSP_STATUS_SESSION_TRANSMITTING,// play ok, data transmitting..check channel or pid changed. if tuner config invalid, status move to teardown.
	RTSP_STATUS_SESSION_TEARDOWNING, // send teardown and if receive, go to waiting.
};

enum
{
	RTSP_REQUEST_NONE = 0,
	RTSP_REQUEST_OPTION, // use to keep alive message..
	RTSP_REQUEST_SETUP, // send tuning params.. (and pids)
	RTSP_REQUEST_PLAY, // send play commend.. and tuning params or addpid, delpid (channel change)
	RTSP_REQUEST_TEARDOWN, // request server disconnect.
	RTSP_REQUEST_DESCRIBE, // request stream infomation
};

class satipRTSP
{
public:
	satipRTSP(satipConfig* satip_config,
			     const char* host, 
			     const char* rtsp_port,
			     int rtp_port);
	~satipRTSP();
	void handleRTSPStatus();
	int getRtspSocketFd();
	short getPollEvent();
	int getPollTimeout();
	void handleNextTimer();
	void handlePollEvents(short events);

	static void timeoutConnect(void *ptr);
	static void timeoutKeepAlive(void *ptr);
	static void timeoutStreamInfo(void *ptr);

private:
	char *m_host;
	char* m_port;
	int m_rtp_port;
	satipConfig *m_satip_config;
	satipTimer m_satip_timer;
	timer_elem *m_timer_reset_connect;
	timer_elem *m_timer_keep_alive;
	int m_fd;

	char m_txbuf[1024];
	char m_rx_data[1024];
	int m_rx_data_pos;

	int m_rtsp_status;
	int m_rtsp_request;

	enum { RTSP_FAILED = -1, RTSP_OK = 0, RTSP_ERROR = 1, RTSP_RESPONSE_COMPLETE = 2 };

	std::string m_rtsp_session_id;
	int m_rtsp_stream_id;
	int m_rtsp_timeout;
	int m_rtsp_cseq;

	bool m_wait_response;
	
	void resetConnect();
	int connectToServer();

	int handleResponse();
	int handleResponseSetup();
	int handleResponsePlay();
	int handleResponseOption();
	int handleResponseTeardown();
	int handleResponseDescribe();

	int sendRequest(int request);
	int sendSetup();
	int sendPlay();
	int sendOption();
	int sendTearDown();
	int sendDescribe();
	int setTuneParams();
	int getPidList(int get_changed = 0);

	void startTimerResetConnect(long timeout);
	void stopTimerResetConnect();
	void startTimerKeepAliveMessage();
	void stopTimerKeepAliveMessage();
};
#endif // __RTSP_H__

