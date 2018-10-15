/*
 * satip: RTP processing
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
#include <time.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <poll.h>
#include <errno.h>

#include "rtp.h"
#include "log.h"

//#define BUFFER_SIZE ((188 / 4) * 4096) /* multiple of ts packet and page size */
#define BUFFER_SIZE 1328 // 12byte +188*7
#define PORT_BASE 45000
#define PORT_RANGE 2000

satipRTP::satipRTP(int vtuner_fd, int tcp_data)
						:m_rtp_port(-1),
						m_rtp_socket(-1),
						m_rtcp_port(-1),
						m_rtcp_socket(-1),
						m_thread(0),
						m_running(false),
						m_hasLock(false),
						m_signalStrength(0),
						m_signalQuality(0),
						m_openok(false)
{
	DEBUG(MSG_MAIN,"Create RTP.\n");
	m_vtuner_fd = vtuner_fd;
	m_tcp_data = tcp_data;
	if (tcp_data) {
		m_openok = 1;
	} else {
		m_openok = !openRTP();
		if (!m_openok)
			DEBUG(MSG_MAIN,"Create RTP failed.\n");
	}
}

satipRTP::~satipRTP()
{
	DEBUG(MSG_MAIN,"Destruct RTP.\n");
	stop();

	if (m_rtcp_socket)
		close(m_rtcp_socket);

	if (m_rtp_socket)
		close(m_rtp_socket);
}

void satipRTP::unset()
{
	m_signalStrength = 0;
	m_hasLock = false;
	m_signalQuality = 0;
}

int satipRTP::openRTP()
{
	int rtp_sock;
	int rtp_port;
	int rtcp_sock;
	int rtcp_port;

	struct timespec ts;
	int attempts = PORT_RANGE/2;
	clock_gettime(CLOCK_REALTIME, &ts);
	srandom(ts.tv_nsec);

	while ( --attempts > 0 )
	{
		struct sockaddr_in inaddr;

		rtp_port = PORT_BASE + ( random() % (PORT_RANGE-1) );
		rtcp_port = rtp_port+1;

		rtp_sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
		rtcp_sock= socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);

		memset(&inaddr, 0, sizeof(inaddr));
		inaddr.sin_family = AF_INET;
		inaddr.sin_addr.s_addr = htonl(INADDR_ANY);
		inaddr.sin_port = htons(rtp_port);

		/* rtp bind */
		if (bind(rtp_sock, (struct sockaddr *) &inaddr, sizeof(inaddr)) < 0)
		{
			close(rtp_sock);
			close(rtcp_sock);
			continue;
		}

		int len = 4 * 1024 * 1024;
		if (setsockopt(rtp_sock, SOL_SOCKET, SO_RCVBUFFORCE, &len, sizeof(len)))
			WARN(MSG_MAIN, "unable to set UDP buffer (force) size to %d\n", len);

		if (setsockopt(rtp_sock, SOL_SOCKET, SO_RCVBUF, &len, sizeof(len)))
			WARN(MSG_MAIN, "unable to set UDP buffer size to %d\n", len);

		socklen_t sl = sizeof(int);
		if (!getsockopt(rtp_sock, SOL_SOCKET, SO_RCVBUF, &len, &sl))
			DEBUG(MSG_DATA, "UDP buffer size is %d bytes\n", len);

		memset(&inaddr, 0, sizeof(inaddr));
		inaddr.sin_family = AF_INET;
		inaddr.sin_addr.s_addr = htonl(INADDR_ANY);
		inaddr.sin_port = htons(rtcp_port);

		/* rtcp bind */
		if (bind(rtcp_sock, (struct sockaddr *) &inaddr, sizeof(inaddr)) < 0)
		{
			close(rtp_sock);
			close(rtcp_sock);
			continue;
		}

		INFO(MSG_NET, "RTP PORT : %d, RTCP PORT : %d\n", rtp_port, rtcp_port);
		break;
	}

	if (attempts <= 0)
		return -1;

	m_rtp_port    = rtp_port;
	m_rtp_socket  = rtp_sock;
	m_rtcp_port   = rtcp_port;
	m_rtcp_socket = rtcp_sock;
	return 0;
}

void satipRTP::parseRtcpAppPayload(char *buffer)
{
	/*
		APP Packet String Payload Format:
		ver=<major>.<minor>;src=<srcID>;tuner=<feID>,<level>,<lock>,<quality>,<frequency>,<polarisation>, 
			<system>,<type>,<pilots>,<roll_off>,<symbol_rate>,<fec_inner>;pids=<pid0>,...,<pidn>

		level (Signal level) : Numerical value between 0 and 255
		lock (Frontend lock) : Set to one of the following values:
			'0' : the frontend is not locked
			'1' : the frontend if locked
		quality (Signal quality) : Numerical value between 0 and 15
	*/

	unset();

	char *strp = strstr(buffer, ";tuner=");
	if (strp)
	{
		int level, lock, quality;
		strp = strstr(strp, ",");
		sscanf(strp, ",%d,%d,%d,%*s", &level, &lock, &quality);

		m_signalStrength = (level >= 0) ? (level * 65535 / 255) : 0;
		m_hasLock = !!lock;
		m_signalQuality = (m_hasLock && (quality >= 0)) ? (quality * 65535 / 15) : 0;

//		DEBUG(MSG_DATA, "RTCP: signalStrength : %d, hasLock : %d, signalQuality : %d\n", m_signalStrength, m_hasLock, m_signalQuality);
	}
}

void satipRTP::rtcpData(unsigned char* a_buffer, int a_size)
{
	int done = 0;
	uint32_t *buffer = (uint32_t*) a_buffer;
	uint32_t val;

	int pt;
	int length;

	char* payload;

	DEBUG(MSG_DATA,"RTCP DATA : %s\n", buffer);

	while(done < a_size)
	{
		val = ntohl(*buffer);
		pt = ( val & 0x00ff0000 ) >> 16 ;
		length = val & 0x0000ffff;

		switch(pt)
		{
			case 204:
				if (length > 1)
				{
					val = buffer[2];
					DEBUG(MSG_DATA,"RTCP: app defined (204) name: %c%c%c%c\n",
						val & 0x000000ff,
						val >> 8 & 0x000000ff,
						val >> 16 & 0x000000ff,
						val >> 24 & 0x000000ff);

					val = htonl(buffer[3]) & 0x0000ffff; // string_length
					if (val > 0)
					{
						payload = (char*) malloc(val + 1);
						if (payload)
						{
							memcpy(payload, (char*) &buffer[4], val);
							payload[val] = 0;
							DEBUG(MSG_DATA, "RTCP APP string Payload : %s\n", payload);
							parseRtcpAppPayload(payload);
							free(payload);
						}
					}
				}

				break;

			default:
				DEBUG(MSG_DATA,"RTCP: PT : %d.. skip.\n", pt);
				break;
		}

		buffer += length + 1;
		done += (length + 1) * 4;
	}
}

int satipRTP::Write(int fd, unsigned char *buffer, int size)
{
	int count = 0;
	ssize_t write_res;
	while(count < size)
	{
		write_res = write(fd, buffer + count, size - count);
		if (write_res == 0)
			return -1;

		if (write_res == -1)
		{
			if( errno == EINTR )
			{
				DEBUG(MSG_MAIN, "WRITE : raise EINTR..continue.\n");
				continue;
			}

			perror("RTP Read.");
			return write_res;
		}

		count += write_res;
	}
	return count;
}

ssize_t satipRTP::Read(int fd, unsigned char *buffer, int size)
{
	ssize_t recv_res;
	while(1)
	{
		recv_res = recv(fd, buffer, size, 0);
		if (recv_res == -1)
		{
			if( errno == EINTR )
			{
				DEBUG(MSG_MAIN, "READ : raise EINTR..continue.\n");
				continue;
			}

			perror("RTP Read.");
			return recv_res;
		}

		break; /* one read */
	}

	return recv_res;
}


void* satipRTP::rtpDump()
{
	unsigned char rx_data[BUFFER_SIZE];
	struct pollfd pollfds[2];

	int rx_bytes;
	int wr_bytes;

	pollfds[0].fd = m_rtp_socket;
	pollfds[0].events = POLLIN;
	pollfds[0].revents = 0;

	pollfds[1].fd = m_rtcp_socket;
	pollfds[1].events = POLLIN;
	pollfds[1].revents = 0;


	DEBUG(MSG_MAIN, "RTP LOOP START\n");
	while(m_running)
	{
		pollfds[0].revents = 0;
		pollfds[1].revents = 0;

//		poll(pollfds, 2, -1);
		poll(pollfds, 2, 1000);

		if ( pollfds[0].revents & POLLIN )
		{
			rx_bytes = Read(pollfds[0].fd, rx_data, sizeof(rx_data));

			if ( (rx_bytes > 12) && (rx_data[12] == 0x47) ) // remove RTP encapsulation 12bytes.
			{
				wr_bytes= Write(m_vtuner_fd ,&rx_data[12], rx_bytes-12);
				DEBUG(MSG_DATA, "RTP DATA : read %d bytes, write %d bytes\n", rx_bytes, wr_bytes);
			}
		}

		if ( pollfds[1].revents & POLLIN )
		{
			rx_bytes = recv(pollfds[1].fd, rx_data, sizeof(rx_data), 0);
			if (rx_bytes > 0)
			{
				DEBUG(MSG_DATA,"RTCP DATA : read %d bytes\n", rx_bytes);
				rtcpData(rx_data, rx_bytes);
			}
		}

	}
	DEBUG(MSG_MAIN,"RTP LOOP END.\n");
	return 0;
}

int satipRTP::rtpTcpData(unsigned char *data, int size)
{
        if (size <= 4 + 12)
        {
                return 0;
        }

        if (data[1] == 0)
        {
                int wr = Write(m_vtuner_fd, data + 4 + 12, size - 4 - 12);
                DEBUG(MSG_DATA, "RTP TCP DATA : read %d bytes, write %d bytes\n", size - 4, wr);
        }
        else if (data[1] == 1)
        {
                DEBUG(MSG_DATA,"RTCP TCP DATA : read %d bytes\n", size - 4);
                rtcpData(data + 4, size - 4);

        }

        return 0;
}

void *satipRTP::thread_wrapper(void *ptr)
{
	return ((satipRTP*)ptr)->rtpDump();
}

void satipRTP::run()
{
	m_running = true;
	if (!m_tcp_data)
		pthread_create( &m_thread, NULL, thread_wrapper, this);
}

void satipRTP::stop()
{
	if (m_rtcp_socket != -1)
	{
		write(m_rtcp_socket, "end", 4);
	}

	m_running = false;
	if (m_thread)
	{
		int status;
		pthread_join(m_thread, (void **)&status);
		DEBUG(MSG_MAIN,"RTP thread END : %d.\n", status);
		m_thread = 0;
	}
}

