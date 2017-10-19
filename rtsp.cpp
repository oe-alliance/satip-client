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

#include <sstream> // std::ostringstream

#include "config.h"
#include "rtsp.h"
#include "timer.h"
#include "log.h"

satipRTSP::satipRTSP(satipConfig* satip_config,
			     const char* host, 
			     const char* rtsp_port,
			     int rtp_port):
			     m_host(NULL),
			     m_port(NULL),
			     m_rtp_port(rtp_port),
			     m_satip_config(satip_config),
			     m_timer_reset_connect(NULL),
			     m_timer_keep_alive(NULL),
			     m_fd(-1),
			     m_rx_data_pos(0),
			     m_rtsp_status(RTSP_STATUS_CONFIG_WAITING),
			     m_rtsp_request(RTSP_REQUEST_NONE),
			     m_wait_response(false)
{
	DEBUG(MSG_MAIN,"Create RTSP. (host : %s, port : %s, rtp_port : %d)\n", host, rtsp_port, rtp_port);
	m_host=strdup(host);
	m_port=strdup(rtsp_port);

	m_timer_reset_connect = m_satip_timer.create(timeoutConnect, (void *)this, "reset connect");
	m_timer_keep_alive = m_satip_timer.create(timeoutKeepAlive, (void *)this, "keep alive message");
	
	resetConnect();
}

satipRTSP::~satipRTSP()
{
	DEBUG(MSG_MAIN,"Destruct RTSP.\n");
	free(m_host);
	free(m_port);
}

void satipRTSP::resetConnect()
{
	DEBUG(MSG_MAIN, "resetConnect\n");
	m_rtsp_status = RTSP_STATUS_CONFIG_WAITING;
	m_rtsp_request = RTSP_REQUEST_NONE;
	m_rtsp_cseq = 1;
	m_rtsp_session_id.clear();
	m_rtsp_stream_id = -1;
    m_rtsp_timeout = 60;

	m_rx_data_pos = 0;

	m_wait_response = false;

	if (m_fd != -1)
	{
		close(m_fd);
		m_fd = -1;
	}

	stopTimerResetConnect();
	stopTimerKeepAliveMessage();
}

void satipRTSP::timeoutConnect(void *ptr)
{
	DEBUG(MSG_MAIN, "timeoutConnect\n");
	satipRTSP* _this = (satipRTSP*)ptr;
	_this->resetConnect();
}

void satipRTSP::timeoutKeepAlive(void *ptr)
{
	DEBUG(MSG_MAIN, "timeoutKeepAlive\n");
	satipRTSP* _this = (satipRTSP*)ptr;
	_this->sendRequest(RTSP_REQUEST_OPTION);
}

void satipRTSP::timeoutStreamInfo(void *ptr)
{
	DEBUG(MSG_MAIN, "timeoutStreamInfo\n");
	satipRTSP* _this = (satipRTSP*)ptr;
	_this->sendRequest(RTSP_REQUEST_DESCRIBE);
}

int satipRTSP::connectToServer()
{
	int fd;
	int flags;
	int error;
	struct addrinfo hints;
	struct addrinfo *result;
	struct addrinfo *rp;

	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;    /* IPv4 or IPv6 */
	hints.ai_socktype = SOCK_STREAM; 
	hints.ai_flags = 0;
	hints.ai_protocol = 0;

	error = getaddrinfo(m_host, m_port, &hints, &result);
	if (error) 
	{
		if (error == EAI_SYSTEM)
		{
			ERROR(MSG_NET, "getaddrinfo: %s\n", strerror(error));
		}
		else
		{
			ERROR(MSG_NET, "getaddrinfo: %s\n", gai_strerror(error));
		}
		return RTSP_ERROR;
	}

	for (rp = result; rp != NULL; rp = rp->ai_next) 
	{
		fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		if (fd == -1)
			continue; /* error, try next..*/

		flags=fcntl(fd,F_GETFL,0);
		if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1 ) /* set non blocking mode */
		{
			close(fd);
			continue;
		}

		if (connect(fd, rp->ai_addr, rp->ai_addrlen) != -1)
		{
			break; /* connect ok */
		}
		else if(errno == EINPROGRESS)
		{
			break; /* connecting */
		}

		close(fd); /* connect fail */
	}

	if (rp == NULL) {           
		DEBUG(MSG_NET, "Could not connect\n");
		freeaddrinfo(result);   
		return RTSP_ERROR;
	}

	freeaddrinfo(result);

	m_fd = fd;

	return RTSP_OK;
}

int satipRTSP::handleResponse()
{
	int res = RTSP_ERROR;
	int res_code = 0;
	ssize_t read_data = recv(m_fd, &(m_rx_data[m_rx_data_pos]), sizeof(m_rx_data) - m_rx_data_pos, 0);

	if (read_data == 0)
	{
		return RTSP_ERROR;
	}

	m_rx_data_pos += read_data;
	m_rx_data[m_rx_data_pos] = 0;

	DEBUG(MSG_NET,"RTSP rx data : \n%s\n", m_rx_data);

	if (strstr(m_rx_data, "\r\n\r\n") == NULL)
	{
		return RTSP_OK;
	}

	/* receive data complete */
	m_rx_data_pos = 0;

	/* set response wait to false */
	m_wait_response = false;

	/* exceptional handle for describe */
	if (m_rtsp_request == RTSP_REQUEST_DESCRIBE)
	{
		stopTimerResetConnect();
		m_rtsp_request = RTSP_REQUEST_NONE;
		return handleResponseDescribe();
	}

	sscanf(m_rx_data,"RTSP/%*s %d",&res_code);
	if (res_code!=200)
		return RTSP_ERROR;

	switch(m_rtsp_request)
	{
		case RTSP_REQUEST_NONE:
			DEBUG(MSG_NET,"response skip..\n");
			break;

		case RTSP_REQUEST_OPTION:
			res = handleResponseOption();
			break;

		case RTSP_REQUEST_SETUP:
			res = handleResponseSetup();
			break;

		case RTSP_REQUEST_PLAY:
			res = handleResponsePlay();
			break;

		case RTSP_REQUEST_TEARDOWN:
			res = handleResponseTeardown();
			break;
		default:
			break;
	}

	if(res == RTSP_ERROR)
	{
		switch(m_rtsp_request)
		{
			case RTSP_REQUEST_NONE:
				DEBUG(MSG_MAIN, "RTSP RESPONSE NONE is failed! try reconnect.\n");
				break;
			case RTSP_REQUEST_OPTION:
				DEBUG(MSG_MAIN, "RTSP RESPONSE OPTION is failed! try reconnect.\n");
				break;

			case RTSP_REQUEST_SETUP:
				DEBUG(MSG_MAIN, "RTSP RESPONSE SETUP is failed! try reconnect.\n");
				break;

			case RTSP_REQUEST_PLAY:
				DEBUG(MSG_MAIN, "RTSP RESPONSE PLAY is failed! try reconnect.\n");
				break;

			case RTSP_REQUEST_TEARDOWN:
				DEBUG(MSG_MAIN, "RTSP RESPONSE TEARDOWN is failed! try reconnect.\n");
				break;
		}
		resetConnect();
	}

	else if (res == RTSP_RESPONSE_COMPLETE)
	{
		stopTimerResetConnect();
	}

	m_rtsp_request = RTSP_REQUEST_NONE;

	return res;
}

int satipRTSP::handleResponseSetup()
{
	/*
	RTSP/1.0 200 OK
	CSeq: 1
	Session: 12345678;timeout=60
	Transport: RTP/AVP;unicast;client_port=1400-1401;source=192.168.128.5;server_port=1528-1529
	com.ses.streamID: 1
	<CRLF>
	*/

	/*
	RTSP/1.0 200 OK
	CSeq: 1
	Session: 0521595368;timeout=60
	Transport: RTP/AVP;unicast;client_port=46938-46939;server_port=8000-8001
	com.ses.streamID: 1
	*/

	char *ptr;
	char session_id[1024];

	/* get session id */
	ptr = strstr(m_rx_data, "Session");
	if (!ptr)
		return RTSP_ERROR;

	ptr = strchr(ptr, ':');
	if (!ptr)
		return RTSP_ERROR;

	do {
		ptr++;
	} while (isspace(*ptr));

	strcpy(session_id, ptr);

	ptr = session_id + strcspn(session_id, ";\n");
	do {
		ptr--;
	} while (ptr >= session_id && isspace(*ptr));

	*(ptr + 1) = '\0';

	m_rtsp_session_id = session_id;

	/* get timeout */
	ptr = strstr(m_rx_data, "timeout");
	if (ptr)
	{
		ptr = strchr(ptr, '=');
		if (ptr)
		{
			m_rtsp_timeout = strtol(ptr + 1, 0, 0);

			DEBUG(MSG_MAIN, "Timeout : %d\n", m_rtsp_timeout);
		}
	}

	/* get stream id */
	ptr = strstr(m_rx_data, "com.ses.streamID");
	if (!ptr)
		return RTSP_ERROR;

	ptr = strchr(ptr, ':');
	if (!ptr)
		return RTSP_ERROR;

	m_rtsp_stream_id = strtol(ptr + 1, 0, 0);

	DEBUG(MSG_MAIN, "Session ID : %s\n", m_rtsp_session_id.c_str());
	DEBUG(MSG_MAIN, "Stream ID : %d\n", m_rtsp_stream_id);

	return RTSP_RESPONSE_COMPLETE;
}

int satipRTSP::handleResponsePlay()
{
	return RTSP_RESPONSE_COMPLETE;
}

int satipRTSP::handleResponseOption()
{
	return RTSP_RESPONSE_COMPLETE;
}

int satipRTSP::handleResponseTeardown()
{
	return RTSP_RESPONSE_COMPLETE;
}

int satipRTSP::handleResponseDescribe()
{
	return RTSP_RESPONSE_COMPLETE;
}

int satipRTSP::sendRequest(int request)
{
	if (m_wait_response)
	{
		DEBUG(MSG_MAIN, "Now waitng response, skip sendRequest(%d)\n", request);
		return RTSP_FAILED;
	}

	stopTimerKeepAliveMessage(); // before send request, stop keep alive message timer.

	int res = RTSP_ERROR;
	switch(request)
	{
		case RTSP_REQUEST_OPTION:
			res = sendOption();
			break;

		case RTSP_REQUEST_SETUP:
			res = sendSetup();
			break;

		case RTSP_REQUEST_PLAY:
			res = sendPlay();
			break;

		case RTSP_REQUEST_TEARDOWN:
			res = sendTearDown();
			break;

		case RTSP_REQUEST_DESCRIBE:
			res = sendDescribe();
			break;

		default:
			DEBUG(MSG_MAIN, "Unknown request!\n");
			break;
	}

	if (res == RTSP_OK)
	{
		m_wait_response = true;
		m_rtsp_request = request;
		startTimerResetConnect(2000); // server connect timer start
	}
	else
	{
		switch(request)
		{
			case RTSP_REQUEST_OPTION:
				DEBUG(MSG_MAIN, "RTSP SEND OPTION is failed! try reconnect.\n");
				break;

			case RTSP_REQUEST_SETUP:
				DEBUG(MSG_MAIN, "RTSP SEND SETUP is failed! try reconnect.\n");
				break;

			case RTSP_REQUEST_PLAY:
				DEBUG(MSG_MAIN, "RTSP SEND PLAY is failed! try reconnect.\n");
				break;

			case RTSP_REQUEST_TEARDOWN:
				DEBUG(MSG_MAIN, "RTSP SEND TEARDOWN is failed! try reconnect.\n");
				break;

			case RTSP_REQUEST_DESCRIBE:
				DEBUG(MSG_MAIN, "RTSP SEND DESCRIBE is failed! try reconnect.\n");
				break;
			default:
				break;
		}
		resetConnect();
	}

	return res;
}

int satipRTSP::sendSetup()
{
	std::string tx_data;
	std::ostringstream oss_tx_data;
	/* 
	str = SETUP rtsp://192.168.100.101/?src=1&freq=10202&pol=v&msys=dvbs&sr=27500&fec=34&pids=0,16,25,104 RTSP/1.0
	CSeq: 1
	Transport: RTP/AVP;unicast;client_port=1400-1401
	<CRLF>
	*/

	oss_tx_data << "SETUP rtsp://" << m_host << ":" << m_port << "/";
	if (m_rtsp_stream_id != -1)
		oss_tx_data << "stream=" << m_rtsp_stream_id;

	oss_tx_data << m_satip_config->getSetupData() << " RTSP/1.0\r\n";
	oss_tx_data << "CSeq: " << m_rtsp_cseq++ << "\r\n";
	if (!m_rtsp_session_id.empty())
		oss_tx_data << "Session: " << m_rtsp_session_id << "\r\n";

	oss_tx_data << "Transport: RTP/AVP;unicast;client_port=" << m_rtp_port << "-" << m_rtp_port +1 << "\r\n";
	oss_tx_data << "\r\n";

	tx_data = oss_tx_data.str();

	DEBUG(MSG_MAIN, "SETUP DATA : \n%s\n", tx_data.c_str());

	if (send(m_fd, tx_data.c_str(), tx_data.size(), 0) < 0)
		return RTSP_ERROR;

	return RTSP_OK;
}

int satipRTSP::sendPlay()
{
	std::string tx_data;
	std::ostringstream oss_tx_data;
	/*
	PLAY rtsp://192.168.128.5/stream=1 RTSP/1.0
	CSeq: 2
	Session: 12345678
	<CRLF>
	*/

	/*
	PLAY rtsp://192.168.178.57:554/stream=5?src=1&freq=11538&pol=v&ro=0.35&msys=dvbs&mtype=qpsk&plts=off&sr=22000
	&fec=56&pids=0,611,621,631 RTSP/1.0
	CSeq:5
	Session:21a15c02c1ee244
	*/

	if (m_rtsp_stream_id == -1 || m_rtsp_session_id.empty())
	{
		ERROR(MSG_MAIN, "PLAY : stream_id and session_id are required..\n");
		return RTSP_ERROR;
	}

	oss_tx_data << "PLAY rtsp://" << m_host << ":" << m_port << "/" << "stream=" << m_rtsp_stream_id;
	oss_tx_data << m_satip_config->getPlayData() << " RTSP/1.0\r\n";
	oss_tx_data << "CSeq: " << m_rtsp_cseq++ << "\r\n";
	oss_tx_data << "Session: " << m_rtsp_session_id << "\r\n";
	oss_tx_data << "\r\n";

	tx_data = oss_tx_data.str();

	DEBUG(MSG_MAIN, "PLAY DATA : \n%s\n", tx_data.c_str());

	if (send(m_fd, tx_data.c_str(), tx_data.size(), 0) < 0)
		return RTSP_ERROR;

	return RTSP_OK;
}

int satipRTSP::sendOption()
{
	std::string tx_data;
	std::ostringstream oss_tx_data;
	/*
	OPTIONS rtsp://192.168.178.57:554/ RTSP/1.0
	CSeq:5
	Session:2180f601c42957d
	<CRLF>
	*/

	if (m_rtsp_stream_id == -1 || m_rtsp_session_id.empty())
	{
		ERROR(MSG_MAIN, "OPTIONS : stream_id and session_id are required..");
		return RTSP_ERROR;
	}

	oss_tx_data << "OPTIONS rtsp://" << m_host << ":" << m_port << "/";
//	if (m_rtsp_stream_id != -1)
//	oss_tx_data << "stream=" << m_rtsp_stream_id;
	oss_tx_data << " RTSP/1.0\r\n";

	oss_tx_data << "CSeq: " << m_rtsp_cseq++ << "\r\n";
//	if (!m_rtsp_session_id.empty())
	oss_tx_data << "Session: " << m_rtsp_session_id << "\r\n";
	oss_tx_data << "\r\n";

	tx_data = oss_tx_data.str();

	DEBUG(MSG_MAIN, "OPTIONS DATA : \n%s\n", tx_data.c_str());

	if (send(m_fd, tx_data.c_str(), tx_data.size(), 0) < 0)
		return RTSP_ERROR;

	return RTSP_OK;
}

int satipRTSP::sendTearDown()
{
	std::string tx_data;
	std::ostringstream oss_tx_data;

	/*
	TEARDOWN rtsp://192.168.178.57:554/stream=2 RTSP/1.0
	CSeq:5
	Session:2180f601c42957d
	<CRLF>
	*/

	if (m_rtsp_stream_id == -1 || m_rtsp_session_id.empty())
	{
		ERROR(MSG_MAIN, "TEARDOWN : stream_id and session_id are required..");
		return RTSP_ERROR;
	}

	oss_tx_data << "TEARDOWN rtsp://" << m_host << ":" << m_port << "/stream=" << m_rtsp_stream_id << " RTSP/1.0\r\n";
	oss_tx_data << "CSeq: " << m_rtsp_cseq++ << "\r\n";
	oss_tx_data << "Session: " << m_rtsp_session_id << "\r\n";
	oss_tx_data << "\r\n";

	tx_data = oss_tx_data.str();

	if (send(m_fd, tx_data.c_str(), tx_data.size(), 0) < 0)
		return RTSP_ERROR;

	return RTSP_OK;
}

int satipRTSP::sendDescribe()
{
	std::string tx_data;
	std::ostringstream oss_tx_data;

	/*
	DESCRIBE rtsp://192.168.128.5/
	CSeq: 5
	Accept: application/sdp
	<CRLF>
	*/

	oss_tx_data << "DESCRIBE rtsp://" << m_host << ":" << m_port << "/";
	if (m_rtsp_stream_id != -1)
		oss_tx_data << "stream=" << m_rtsp_stream_id;
	oss_tx_data << " RTSP/1.0\r\n";
	oss_tx_data << "CSeq: " << m_rtsp_cseq++ << "\r\n";
	oss_tx_data << "Accept: application/sdp" << "\r\n";
	oss_tx_data << "\r\n";

	tx_data = oss_tx_data.str();

	if (send(m_fd, tx_data.c_str(), tx_data.size(), 0) < 0)
		return RTSP_ERROR;

	return RTSP_OK;
}

void satipRTSP::handleRTSPStatus()
{
	switch(m_rtsp_status)
	{
		case RTSP_STATUS_CONFIG_WAITING:
			DEBUG(MSG_MAIN, "RTSP STATUS : RTSP_STATUS_CONFIG_WAITING\n");
			if (m_satip_config->getChannelStatus() == CONFIG_STATUS_CHANNEL_CHANGED)
			{
				if (connectToServer() == RTSP_OK)
				{
					m_rtsp_status = RTSP_STATUS_SERVER_CONNECTING;
					startTimerResetConnect(5000);
				}
				else
				{
					DEBUG(MSG_MAIN, "Connect to server failed!\n");
				}
			}
			break;

		case RTSP_STATUS_SERVER_CONNECTING: // connected to serverm check if server ready to send RTSP requests.
			DEBUG(MSG_MAIN, "RTSP STATUS : RTSP_STATUS_SERVER_CONNECTING\n");
			break;

		case RTSP_STATUS_SESSION_ESTABLISHING: // SETUP request sended, wait POLLIN event to receive SETUP response.
			DEBUG(MSG_MAIN, "RTSP STATUS : RTSP_STATUS_SESSION_ESTABLISHING\n");
			if (sendRequest(RTSP_REQUEST_SETUP) == RTSP_OK) // send ok
			{
				;// rtp thread start;
			}
			break;

		case RTSP_STATUS_SESSION_PLAYING: // PLAY request sended, wait POLLIN event to receive PLAY response.
			DEBUG(MSG_MAIN, "RTSP STATUS : RTSP_STATUS_SESSION_PLAYING\n");
			sendRequest(RTSP_REQUEST_PLAY);
			break;

		case RTSP_STATUS_SESSION_TRANSMITTING:
			DEBUG(MSG_MAIN, "RTSP STATUS : RTSP_STATUS_SESSION_TRANSMITTING\n");
			{
				t_channel_status channel_status = m_satip_config->getChannelStatus();
				t_pid_status pid_status = m_satip_config->getPidStatus();

				if (channel_status == CONFIG_STATUS_CHANNEL_CHANGED)
					DEBUG(MSG_MAIN, "CHANNEL STATUS : CONFIG_STATUS_CHANNEL_CHANGED\n");

				if (pid_status == CONFIG_STATUS_PID_CHANGED)
					DEBUG(MSG_MAIN, "PID STATUS : CONFIG_STATUS_PID_CHANGED\n");

				if ((channel_status == CONFIG_STATUS_CHANNEL_CHANGED) || (pid_status == CONFIG_STATUS_PID_CHANGED))
				{
					if (sendRequest(RTSP_REQUEST_PLAY) == RTSP_OK) // send ok
					{
						m_rtsp_status = RTSP_STATUS_SESSION_PLAYING;
					}			
				}
				else if (channel_status == CONFIG_STATUS_CHANNEL_INVALID)
				{
					if (sendRequest(RTSP_REQUEST_TEARDOWN) == RTSP_OK) // send ok
					{
						m_rtsp_status = RTSP_STATUS_SESSION_TEARDOWNING;
					}
				}
				else // (channel_status == CONFIG_STATUS_CHANNEL_STABLE) || (pid_status == CONFIG_STATUS_PID_STATIONARY)
				{
					if (!m_timer_keep_alive->isActive())
						startTimerKeepAliveMessage();
				}
				break;
			}

		case RTSP_STATUS_SESSION_TEARDOWNING: // TEARDOWN request sended, wait POLLIN event to receive TEARDOWN response.
			DEBUG(MSG_MAIN, "RTSP STATUS : RTSP_STATUS_SESSION_TEARDOWNING\n");
			break;

		default:
			break;
	}
}

short satipRTSP::getPollEvent()
{
	short events = 0;
	switch(m_rtsp_status)
	{
		case RTSP_STATUS_CONFIG_WAITING:
			events = 0; // no poll
			break;

		case RTSP_STATUS_SERVER_CONNECTING: // connected to serverm check if server ready to send RTSP requests.
			events = POLLOUT | POLLHUP;
			break;

		case RTSP_STATUS_SESSION_ESTABLISHING: // SETUP request sended, check read to receive SETUP response.
			events = POLLIN | POLLHUP;
			break;

		case RTSP_STATUS_SESSION_PLAYING: // PLAY request sended, check read to receive PLAY response.
			events = POLLIN | POLLHUP;
			break;

		case RTSP_STATUS_SESSION_TRANSMITTING:
			if ( m_rtsp_request == RTSP_REQUEST_OPTION) // keep alive message
				events = POLLIN | POLLHUP;
			else
				events = 0; // no poll
			break;

		case RTSP_STATUS_SESSION_TEARDOWNING: // TEARDOWN request sended, check read to receive TEARDOWN response.
			events = POLLIN | POLLHUP;
			break;

		default:
			break;
	}

//	DEBUG(MSG_MAIN, "getPollEvent return %d (RTSP STATUS : %d)\n", (int)events, m_rtsp_request);

	return events;
}

void satipRTSP::handlePollEvents(short events)
{
//	DEBUG(MSG_MAIN, "handlePollEvents.\n");
	if (events & POLLHUP)
	{
		DEBUG(MSG_MAIN, "RTSP socket disconnedted, retry connection.\n");
		resetConnect();
		return;
	}

	switch(m_rtsp_status)
	{
		case RTSP_STATUS_CONFIG_WAITING:
			DEBUG(MSG_MAIN, "RTSP STATUS : RTSP_STATUS_CONFIG_WAITING\n");
			DEBUG(MSG_MAIN, "BUG, this message muse not be shown!!!\n"); // no poll
			break;

		case RTSP_STATUS_SERVER_CONNECTING: // connected to server, check if server ready to send RTSP requests.
			DEBUG(MSG_MAIN, "RTSP STATUS : RTSP_STATUS_SERVER_CONNECTING\n");
			if (events & POLLOUT)
			{
				stopTimerResetConnect();
				m_rtsp_status = RTSP_STATUS_SESSION_ESTABLISHING;
			}
			break;

		case RTSP_STATUS_SESSION_ESTABLISHING: // SETUP request sended, check read to receive SETUP response and send PLAY.
			DEBUG(MSG_MAIN, "RTSP STATUS : RTSP_STATUS_SESSION_ESTABLISHING\n");
			if ((events & POLLIN))
			{
				int res = handleResponse();
				if (res == RTSP_RESPONSE_COMPLETE) // handle response SETUP
				{
					m_rtsp_status = RTSP_STATUS_SESSION_PLAYING;
				}
			}
			break;

		case RTSP_STATUS_SESSION_PLAYING: // PLAY request sended, check read to receive PLAY response.
			DEBUG(MSG_MAIN, "RTSP STATUS : RTSP_STATUS_SESSION_PLAYING\n");
			if ((events & POLLIN))
			{
				int res = handleResponse();
				if (res == RTSP_RESPONSE_COMPLETE) // handle response PLAY
				{
					m_rtsp_status = RTSP_STATUS_SESSION_TRANSMITTING;
				}
			}
			break;

		case RTSP_STATUS_SESSION_TRANSMITTING:
			DEBUG(MSG_MAIN, "RTSP STATUS : RTSP_STATUS_SESSION_TRANSMITTING\n");
			if ((events & POLLIN))
			{
				handleResponse(); // handle response OPTION
			}
			break;

		case RTSP_STATUS_SESSION_TEARDOWNING: // TEARDOWN request sended, check read to receive TEARDOWN response.
			DEBUG(MSG_MAIN, "RTSP STATUS : RTSP_STATUS_SESSION_TEARDOWNING\n");
			if ((events & POLLIN))
			{
				int res = handleResponse(); // handle response TEARDOWN
				if (res == RTSP_RESPONSE_COMPLETE)
				{
					resetConnect();
				}
			}
			break;

		default:
			break;
	}
}

int satipRTSP::getPollTimeout() 
{ 
	return m_satip_timer.getNextTimerBegin(); 
}

void satipRTSP::handleNextTimer()
{
	m_satip_timer.callNextTimer();
}

void satipRTSP::startTimerResetConnect(long timeout)
{
	DEBUG(MSG_MAIN, "startTimerResetConnect\n");
	m_timer_reset_connect->start(timeout, true);
}

void satipRTSP::stopTimerResetConnect()
{
	DEBUG(MSG_MAIN, "stopTimerResetConnect\n");
	m_timer_reset_connect->stop();
}

void satipRTSP::startTimerKeepAliveMessage()
{
	DEBUG(MSG_MAIN, "startTimerKeepAliveMessage\n");
	m_timer_keep_alive->start((m_rtsp_timeout - 5) * 1000, true);
}

void satipRTSP::stopTimerKeepAliveMessage()
{
	DEBUG(MSG_MAIN, "stopTimerKeepAliveMessage\n");
	m_timer_keep_alive->stop();
}

int satipRTSP::getRtspSocketFd()
{
	return m_fd;
}

