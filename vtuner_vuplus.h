/*
 * satip: driver interface for vtuner
 *
 * Copyright (C) 2014 mc.fishdish@gmail.com
 * [copy of vtuner by Honza Petrous]
 * Copyright (C) 2010-11 Honza Petrous <jpetrous@smartimp.cz>
 * [based on dreamtuner userland code by Roland Mieslinger]
 * [based on usbtunerhelper code]
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation version 2.
 *
 * This program is distributed WITHOUT ANY WARRANTY of any
 * kind, whether express or implied; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _VTUNER_H_
#define _VTUNER_H_

#include <linux/dvb/version.h>
#include <linux/dvb/frontend.h>
#include <linux/dvb/dmx.h>

#define VT_NULL 0x00
#define VT_S   0x01
#define VT_C   0x02
#define VT_T   0x04
#define VT_S2  0x08

#define MSG_SET_FRONTEND		1
#define MSG_GET_FRONTEND		2
#define MSG_READ_STATUS			3
#define MSG_READ_BER			4
#define MSG_READ_SIGNAL_STRENGTH	5
#define MSG_READ_SNR			6
#define MSG_READ_UCBLOCKS		7
#define MSG_SET_TONE			8
#define MSG_SET_VOLTAGE			9
#define MSG_ENABLE_HIGH_VOLTAGE		10
#define MSG_SEND_DISEQC_MSG		11
#define MSG_SEND_DISEQC_BURST		13
#define MSG_PIDLIST			14
#define MSG_TYPE_CHANGED		15
#define MSG_SET_PROPERTY		16
#define MSG_GET_PROPERTY		17

#define MSG_NULL			1024
#define MSG_DISCOVER			1025
#define MSG_UPDATE       		1026

struct diseqc_master_cmd
{
    u8 msg[6];
    u8 msg_len;
};

struct vtuner_message
{
    s32 type;
    union
    {
        struct dvb_frontend_parameters fe_params;
#if DVB_API_VERSION >= 5
        struct dtv_property prop;
#endif
        u32 status;
        u32 ber;
        u16 ss;
        u16 snr;
        u32 ucb;
        u8 tone;
        u8 voltage;
        struct diseqc_master_cmd diseqc_master_cmd;
        u8 burst;
        u16 pidlist[30];
        u8  pad[72];
        u32 type_changed;
    } body;
};

#define VTUNER_GET_MESSAGE  1
#define VTUNER_SET_RESPONSE 2
#define VTUNER_SET_NAME     3
#define VTUNER_SET_TYPE     4
#define VTUNER_SET_HAS_OUTPUTS 5
#define VTUNER_SET_FE_INFO  6
#define VTUNER_SET_DELSYS   7


#endif

