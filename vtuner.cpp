/*
 * satip: vtuner to satip mapping
 *
 * Copyright (C) 2014  mc.fishdish@gmail.com
 * [fragments from vtuner by Honza Petrous]
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#include "config.h"
#include "vtuner.h"
#include "log.h"

#include "rtp.h"

const char* vtuner_path = "/dev/misc/vtuner";

satipVtuner::satipVtuner(satipConfig* satip_cfg)
	:m_fd(-1), m_openok(false), m_tone(SEC_TONE_OFF)
{
	DEBUG(MSG_MAIN,"Create SATIP VTUNER.\n");
	m_satip_cfg = satip_cfg;
	m_satip_rtp = NULL;
	m_openok = !openVtuner();
	if (!m_openok)
		ERROR(MSG_MAIN,"vtuner control failed\n");
}

satipVtuner::~satipVtuner()
{
	if (m_fd >= 0)
		close(m_fd);

	DEBUG(MSG_MAIN,"Destruct SATIP VTUNER.\n");
}

int satipVtuner::AllocateVtuner()
{
	int vtuner_fd = -1;
	int vtuner_index = 0;
	char filename[128];

	while( vtuner_fd < 0)
	{
		sprintf(filename, "%s%d", vtuner_path, vtuner_index);
		if (access (filename, 0) != 0)	break;

		vtuner_fd = open(filename, O_RDWR);
		if (vtuner_fd < 0)	vtuner_index++;
	}

	if (vtuner_fd < 0)
		return -1;

	DEBUG(MSG_MAIN, "Allocate vtuner %s%d\n", vtuner_path, vtuner_index);

	return vtuner_fd;
}

int satipVtuner::openVtuner()
{
	int fd;
	struct dvb_frontend_info fe_info;

	int fe_type = -1;
	char fe_type_str[8];

	fd = AllocateVtuner();

	if (fd == -1)
	{
		DEBUG(MSG_MAIN, "Allocate vtuner failed!\n");
		goto error;
	}

	m_fd = fd;

	fe_type = m_satip_cfg->getFeType();

	switch (fe_type)
	{
		case FE_TYPE_SAT:
			strncpy(fe_info.name, "SAT>IP DVB-S2 VTUNER", sizeof(fe_info.name));
			strcpy(fe_type_str, "DVB-S2");
			fe_info.type = FE_QPSK;
			fe_info.frequency_min=925000;
			fe_info.frequency_max=2175000;
			fe_info.frequency_stepsize=125000;
			fe_info.frequency_tolerance=0;
			fe_info.symbol_rate_min=1000000;
			fe_info.symbol_rate_max=45000000;
			fe_info.symbol_rate_tolerance=0;
			fe_info.notifier_delay=0;
			fe_info.caps=(fe_caps_t)(FE_CAN_INVERSION_AUTO |
						FE_CAN_FEC_1_2 | FE_CAN_FEC_2_3 | FE_CAN_FEC_3_4 | FE_CAN_FEC_4_5 |
						FE_CAN_FEC_5_6 | FE_CAN_FEC_7_8 | FE_CAN_FEC_8_9 |
						FE_CAN_QPSK | FE_CAN_RECOVER | FE_CAN_2G_MODULATION);
			break;

		case FE_TYPE_CABLE:
			strncpy(fe_info.name, "SAT>IP DVB-C VTUNER", sizeof(fe_info.name));
			strcpy(fe_type_str,"DVB-C");
			fe_info.type = FE_QAM;
			fe_info.frequency_min=51000000;
			fe_info.frequency_max=858000000;
			fe_info.frequency_stepsize=62500;
			fe_info.frequency_tolerance=0;
			fe_info.symbol_rate_min=(57840000/2)/64;	/* SACLK/64 == (XIN/2)/64 */
			fe_info.symbol_rate_max=(57840000/2)/4;		/* SACLK/4 */
			fe_info.symbol_rate_tolerance=0;
			fe_info.notifier_delay=0;
			fe_info.caps=(fe_caps_t)(FE_CAN_INVERSION_AUTO |
						FE_CAN_QAM_16 | FE_CAN_QAM_32 | FE_CAN_QAM_64 | FE_CAN_QAM_128 |
						FE_CAN_QAM_256 | FE_CAN_RECOVER | FE_CAN_FEC_AUTO);
			break;

		case FE_TYPE_TERRESTRIAL:
			strncpy(fe_info.name, "SAT>IP DVB-T VTUNER", sizeof(fe_info.name));
			strcpy(fe_type_str,"DVB-T");
			fe_info.type = FE_OFDM;
			fe_info.frequency_min=0;
			fe_info.frequency_max=863250000;
			fe_info.frequency_stepsize=62500;
			fe_info.frequency_tolerance=0;
			fe_info.notifier_delay=0;
			fe_info.caps=(fe_caps_t)(FE_CAN_INVERSION_AUTO |
						FE_CAN_FEC_1_2 | FE_CAN_FEC_2_3 | FE_CAN_FEC_3_4 |
						FE_CAN_FEC_5_6 | FE_CAN_FEC_7_8 | FE_CAN_FEC_AUTO |
						FE_CAN_QAM_16 | FE_CAN_QAM_64 | FE_CAN_QPSK |
						FE_CAN_TRANSMISSION_MODE_AUTO | FE_CAN_GUARD_INTERVAL_AUTO |
						FE_CAN_HIERARCHY_AUTO | FE_CAN_RECOVER);
			break;

		default:
			ERROR(MSG_MAIN,"Unknown frontend type: %d\n", fe_type);
			goto error;
			break;
	}

	ioctl(fd, VTUNER_SET_NAME, fe_info.name);
	ioctl(fd, VTUNER_SET_TYPE, fe_type_str);
	ioctl(fd, VTUNER_SET_FE_INFO, &fe_info);

#if DVB_API_VERSION > 5 || DVB_API_VERSION == 5 && DVB_API_VERSION_MINOR >= 5
	/* set delsys */
	{
		u32 ncaps = 0;
		struct dtv_property p[1];
		p[0].cmd = DTV_ENUM_DELSYS;
		memset(p[0].u.buffer.data, 0, sizeof(p[0].u.buffer.data));

		switch(fe_type)
		{
			case FE_TYPE_SAT:
				p[0].u.buffer.data[ncaps++] = SYS_DVBS;
				if (fe_info.caps & FE_CAN_2G_MODULATION)
					p[0].u.buffer.data[ncaps++] = SYS_DVBS2;
				break;

			case FE_TYPE_CABLE:
				p[0].u.buffer.data[ncaps++] = SYS_DVBC_ANNEX_A;
				break;

			case FE_TYPE_TERRESTRIAL:
				p[0].u.buffer.data[ncaps++] = SYS_DVBT;
				if (fe_info.caps & FE_CAN_2G_MODULATION)
					p[0].u.buffer.data[ncaps++] = SYS_DVBT2;
				break;
		}
	//		p[0].u.buffer.len = ncaps;
		ioctl(fd, VTUNER_SET_DELSYS, p[0].u.buffer.data);
	}
#endif

	/* has_outputs */
	if (ioctl(fd, VTUNER_SET_HAS_OUTPUTS, "no"))
	{
		ERROR(MSG_MAIN,"VTUNER_SET_HAS_OUTPUTS error\n");
	}

	DEBUG(MSG_MAIN, "Vtuner initialize OK!\n");
	return 0;

error:
	if (m_fd >= 0)
	{
		close(m_fd);
		m_fd = -1;
	}
	return -1;
}

/*
	DTV_FREQUENCY
	DTV_DELIVERY_SYSTEM
	DTV_MODULATION
	DTV_SYMBOL_RATE
	DTV_INNER_FEC
	DTV_ROLLOFF
	DTV_PILOT
	DTV_TUNE
	DTV_CLEAR
	DTV_CODE_RATE_LP
	DTV_CODE_RATE_HP
	DTV_TRANSMISSION_MODE
	DTV_GUARD_INTERVAL
	DTV_HIERARCHY
	DTV_BANDWIDTH_HZ
	DTV_STREAM_ID or DTV_DVBT2_PLP_ID
	DTV_INVERSION	
*/
void satipVtuner::setProperty(struct vtuner_message* msg)
{
	struct dtv_property prop = msg->body.prop;
	__u32 cmd = prop.cmd;
	__u32 data = prop.u.data;

	switch(cmd)
	{
		case DTV_FREQUENCY:
		{
			DEBUG(MSG_MAIN,"DTV_FREQUENCY : %d\n",(unsigned int)data);

			unsigned int freq = 0;
			if ( m_satip_cfg->getFeType() == FE_TYPE_SAT) // Khz to Mhz
			{
				freq = (unsigned int)data/100;

				if ( m_tone == SEC_TONE_ON ) /* high band */
				{
					freq += 106000;
				}

				else /* low band */
				{
					freq+=97500;
				}
			}

			else // DVB-T/C, hz to Mhz
			{
				freq = (unsigned int)data/100000;
			}

			m_satip_cfg->setFrequency(freq);
			break;
		}

		case DTV_DELIVERY_SYSTEM:
		{
			DEBUG(MSG_MAIN,"DTV_DELIVERY_SYSTEM : %d\n",(int)data);
			m_satip_cfg->setModsys(data);
			break;
		}

		case DTV_MODULATION:
		{
			DEBUG(MSG_MAIN,"DTV_MODULATION : %d\n", data);
			m_satip_cfg->setModtype(data);
			break;
		}

		case DTV_SYMBOL_RATE:
		{
			DEBUG(MSG_MAIN,"DTV_SYMBOL_RATE : %d\n",(int)data);
			int symrate = (unsigned int)data/1000;
			m_satip_cfg->setSymrate(symrate);
			break;
		}

		case DTV_INNER_FEC:
		{
			DEBUG(MSG_MAIN,"DTV_INNER_FEC : %d\n",(int)data);

			int fec = data & 31;
			if (fec <= FEC_9_10)
			{
				m_satip_cfg->setFec(fec);
			}
			else
			{
				ERROR(MSG_MAIN,"invalid FEC configured\n");
			}

			break;
		}

		case DTV_ROLLOFF:
		{
			DEBUG(MSG_MAIN,"DTV_ROLLOFF : %d\n",(int)data);
			m_satip_cfg->setRolloff(data);
			break;
		}

		case DTV_PILOT:
		{
			DEBUG(MSG_MAIN,"DTV_PILOT : %d\n",(int)data);
			m_satip_cfg->setPilots(data);
			break;
		}

		case DTV_TUNE:
		{
			DEBUG(MSG_MAIN,"DTV_TUNE \n");
			m_satip_cfg->setChannelChanged();
			break;
		}

		case DTV_CLEAR:
		{
			DEBUG(MSG_MAIN,"DTV_CLEAR \n");
			m_satip_cfg->clearProperty();
			break;
		}

		case DTV_CODE_RATE_LP:
		{
			DEBUG(MSG_MAIN,"DTV_CODE_RATE_LP \n");
			fe_code_rate_t fec_inner = FEC_AUTO;
			m_satip_cfg->setFec(fec_inner);
			break;
		}

		case DTV_CODE_RATE_HP:
		{
			DEBUG(MSG_MAIN,"DTV_CODE_RATE_HP \n");
			fe_code_rate_t fec_inner = FEC_AUTO;
			m_satip_cfg->setFec(fec_inner);
			break;
		}

		case DTV_TRANSMISSION_MODE:
		{
			DEBUG(MSG_MAIN,"DTV_TRANSMISSION_MODE : %d\n",(int)data);
			m_satip_cfg->setTransmode(data);
			break;
		}

		case DTV_GUARD_INTERVAL:
		{
			DEBUG(MSG_MAIN,"DTV_GUARD_INTERVAL : %d\n",(int)data);
			m_satip_cfg->setGuardInterval(data);
			break;
		}

		case DTV_HIERARCHY:
		{
			DEBUG(MSG_MAIN,"DTV_HIERARCHY skip..\n");
			break;
		}

		case DTV_BANDWIDTH_HZ:
		{
			DEBUG(MSG_MAIN,"DTV_BANDWIDTH_HZ : %d\n",(int)data);
			m_satip_cfg->setBandwidth(data);
			break;
		}

#if DVB_API_VERSION > 5 || DVB_API_VERSION == 5 && DVB_API_VERSION_MINOR >= 9
		case DTV_STREAM_ID:
			DEBUG(MSG_MAIN,"DTV_STREAM_ID : %d\n",(int)data);
#elif DVB_API_VERSION == 5 && DVB_API_VERSION_MINOR >= 3
		case DTV_DVBT2_PLP_ID:
			DEBUG(MSG_MAIN,"DTV_DVBT2_PLP_ID : %d\n",(int)data);
#endif
		{
			unsigned int plp = (unsigned int)data;
			m_satip_cfg->setPLP(plp);
			break;
		}

		case DTV_INVERSION:
		{
			DEBUG(MSG_MAIN,"DTV_INVERSION skip..\n");
			break;
		}

		default:
			break;

	}
}

void satipVtuner::setDiseqc(struct vtuner_message* msg)
{
	struct dvb_diseqc_master_cmd* cmd=&msg->body.diseqc_master_cmd;

	if ( cmd->msg[0] == 0xe0 && cmd->msg[1] == 0x10 && cmd->msg[2] == 0x38 && cmd->msg_len == 4 )
	{
		/* committed switch */
		u8 data=cmd->msg[3];
		int voltage;

		if ( (data & 0x01) == 0x01 )
		{
			m_tone = SEC_TONE_ON; /* high band */
		}
		else if ( (data & 0x11) == 0x10 )
		{
			m_tone = SEC_TONE_OFF; /* low band */
		}

		if ( (data & 0x02) == 0x02 )
		{
			voltage = SEC_VOLTAGE_18;
			m_satip_cfg->setVoltage(SEC_VOLTAGE_18);
		}
		else if ( (data & 0x22) == 0x20 )
		{
			voltage = SEC_VOLTAGE_13;
			m_satip_cfg->setVoltage(SEC_VOLTAGE_13);
		}

		int position = ( (data & 0x0c) >> 2) + 1;
		m_satip_cfg->setPosition(position);

		DEBUG(MSG_MAIN,"SEC_TONE : %s, SEC_VOLTAGE : %s, pos : %d\n", m_tone == SEC_TONE_ON ? "ON" : "OFF",
			voltage == SEC_VOLTAGE_13 ? "V" : "H", position);
	}
	else if ( cmd->msg[0] == 0xe0 && cmd->msg[1] == 0x10 && cmd->msg[2] == 0x39 && cmd->msg_len == 4 )
	{
		/* uncommitted switch */
		u8 data=cmd->msg[3];
		int voltage;

		int position = (data & 0x0F) + 1;
		m_satip_cfg->setPosition(position);

		DEBUG(MSG_MAIN,"uncommitted switch pos : %d\n", position);
	}
}

void satipVtuner::setPidList(struct vtuner_message* msg)
{
	u16* pid_list = msg->body.pidlist;
	m_satip_cfg->updatePidList(pid_list);
}

void satipVtuner::vtunerEvent()
{
	struct vtuner_message  msg;

	if (ioctl(m_fd, VTUNER_GET_MESSAGE, &msg))
		return;

	switch(msg.type)
	{
		case MSG_SET_FRONTEND:
#if HAVE_NO_MSG16
			DEBUG(MSG_MAIN,"MSG_SET_PROPERTY old Mode\n");
			setProperty(&msg);
#else
			DEBUG(MSG_MAIN,"MSG_SET_FRONTEND skip..\n");
#endif
			break;

		case MSG_GET_FRONTEND:
			DEBUG(MSG_MAIN,"MSG_GET_FRONTEND: Not implemented!\n");
			break;

		case MSG_SET_TONE:
			m_tone = msg.body.tone;
			DEBUG(MSG_MAIN,"MSG_SET_TONE: %s\n", m_tone == SEC_TONE_ON  ? "high" : "low");
			break;

		case MSG_SET_VOLTAGE:
			DEBUG(MSG_MAIN,"MSG_SET_VOLTAGE: %d\n",msg.body.voltage);
			m_satip_cfg->setVoltage(msg.body.voltage);
			break;

		case MSG_PIDLIST:
			setPidList(&msg);
			DEBUG(MSG_MAIN,"MSG_SET_PIDLIST: \n");
			return;
			break;

		case MSG_READ_STATUS:
			//INFO(MSG_MAIN,"MSG_READ_STATUS\n");
//				if (m_satip_rtp)
//				{
//					if (m_satip_rtp->getHasLock())
//						msg.body.status = FE_HAS_LOCK;
//					else
//						msg.body.status = 0;
//				}
			msg.body.status = FE_HAS_LOCK;
			break;
		case MSG_READ_BER:
			//INFO(MSG_MAIN,"MSG_READ_BER\n");
			msg.body.ber = 0;
			break;

		case MSG_READ_SIGNAL_STRENGTH:
			//INFO(MSG_MAIN,"MSG_READ_SIGNAL_STRENGTH %d -> %d\n", msg.body.ss, rtcp_data.signalStrength);
			if (m_satip_rtp)
				msg.body.ss = (u16)m_satip_rtp->getSignalStrenth();
			break;

		case MSG_READ_SNR:
			//INFO(MSG_MAIN,"MSG_READ_SNR %d -> %d\n", msg.body.snr, m_satip_cfg->satip_get_signal_quality());
			if (m_satip_rtp)
				msg.body.snr = (u16)m_satip_rtp->getSignalSNR();
			break;

		case MSG_READ_UCBLOCKS:
			DEBUG(MSG_MAIN,"MSG_READ_UCBLOCKS\n");
			msg.body.ucb = 0;
			break;

		case MSG_SEND_DISEQC_BURST:
			DEBUG(MSG_MAIN,"MSG_SEND_DISEQC_BURST\n");
			if ( msg.body.burst == SEC_MINI_A )
				m_satip_cfg->setPosition(1);
			else if ( msg.body.burst == SEC_MINI_B )
				m_satip_cfg->setPosition(2);
			else
				ERROR(MSG_MAIN,"invalid diseqc burst %d\n",msg.body.burst);
			break;

		case MSG_SEND_DISEQC_MSG:
			DEBUG(MSG_MAIN,"MSG_SEND_DISEQC_MSG: %02x %02x %02x %02x %02x %02x len %d\n",
			msg.body.diseqc_master_cmd.msg[0],
			msg.body.diseqc_master_cmd.msg[1],
			msg.body.diseqc_master_cmd.msg[2],
			msg.body.diseqc_master_cmd.msg[3],
			msg.body.diseqc_master_cmd.msg[4],
			msg.body.diseqc_master_cmd.msg[5],
			msg.body.diseqc_master_cmd.msg_len);
			setDiseqc(&msg);
			break;

		case MSG_SET_PROPERTY:
			DEBUG(MSG_MAIN,"MSG_SET_PROPERTY\n");
			setProperty(&msg);
			break;

		case MSG_GET_PROPERTY:
			DEBUG(MSG_MAIN,"MSG_GET_PROPERTY\n");
			break;

		default:
			DEBUG(MSG_MAIN,"MSG_UNKNOWN: %d\n", msg.type);
			break;
	}

	msg.type=0;

	if (ioctl(m_fd, VTUNER_SET_RESPONSE, &msg))
		return;
}

