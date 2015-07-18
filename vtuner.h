/*
 * satip: vtuner to satip mapping
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

#ifndef __VTUNER_H__
#define __VTUNER_H__

#include "config.h"
#include "rtp.h"

#define VTUNER_PIDLIST_LEN 30 // from usbtunerhelper

#include <linux/dvb/version.h>
#include <linux/dvb/frontend.h>
#include <linux/dvb/dmx.h>

#define MSG_SET_FRONTEND			1
#define MSG_GET_FRONTEND			2
#define MSG_READ_STATUS				3
#define MSG_READ_BER				4
#define MSG_READ_SIGNAL_STRENGTH	5
#define MSG_READ_SNR				6
#define MSG_READ_UCBLOCKS			7
#define MSG_SET_TONE				8
#define MSG_SET_VOLTAGE				9
#define MSG_ENABLE_HIGH_VOLTAGE		10
#define MSG_SEND_DISEQC_MSG			11
#define MSG_SEND_DISEQC_BURST		13
#define MSG_PIDLIST					14
#define MSG_TYPE_CHANGED			15
#define MSG_SET_PROPERTY			16
#define MSG_GET_PROPERTY			17

#define MSG_NULL				1024
#define MSG_DISCOVER			1025
#define MSG_UPDATE       		1026

typedef unsigned int   u32;
typedef unsigned short u16;
typedef unsigned char  u8;

struct vtuner_message {
	__u32 type;
	union 
	{
        struct dvb_frontend_parameters fe_params;

#if DVB_API_VERSION >= 5
		struct dtv_property prop;
#endif
		u32 status;
		__u32 ber;
		__u16 ss;
		__u16 snr;
		__u32 ucb;
		fe_sec_tone_mode_t tone;
		fe_sec_voltage_t voltage;
		struct dvb_diseqc_master_cmd diseqc_master_cmd;
		fe_sec_mini_cmd_t burst;
		__u16 pidlist[VTUNER_PIDLIST_LEN];
		unsigned char  pad[72];
		__u32 type_changed;
	} body;
};

#define VTUNER_GET_MESSAGE  1
#define VTUNER_SET_RESPONSE 2
#define VTUNER_SET_NAME     3
#define VTUNER_SET_TYPE     4
#define VTUNER_SET_HAS_OUTPUTS 5
#define VTUNER_SET_FE_INFO  6
#define VTUNER_SET_DELSYS   7


class satipVtuner
{
	int m_fd;
	bool m_openok;
	fe_sec_tone_mode_t m_tone;
	satipConfig* m_satip_cfg;
	satipRTP* m_satip_rtp;

	int AllocateVtuner();
	int openVtuner();

	void setProperty(struct vtuner_message* msg);
	void setDiseqc(struct vtuner_message* msg);
	void setPidList(struct vtuner_message* msg);

public:
	satipVtuner(satipConfig* satip_cfg);
	virtual ~satipVtuner();
	int getVtunerFd() { return m_fd; }
	void vtunerEvent();
	void setSatipRTP(satipRTP* satip_rtp) { m_satip_rtp = satip_rtp; }
	bool isOpened() { return m_openok; }
};

#endif // __VTUNER_H__