/*
 * satip: tuning and pid config
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
#include <sstream> // std::ostringstream

#include "config.h"
#include "log.h"
#include "option.h"

satipConfig::satipConfig(int fe_type, vtunerOpt* settings):
	m_fe_type(fe_type),
	m_settings(settings),
	m_signal_source(1),
	m_pol(CONFIG_POL_HORIZONTAL),
	m_status(CONFIG_STATUS_CHANNEL_INVALID),
	m_pid_status(CONFIG_STATUS_PID_STATIONARY),
	m_lnb_voltage_onoff(CONFIG_LNB_OFF)
{
	for (int i = 0; i < MAX_PIDS; i++)
	{
		m_pid_list[i].status = PID_INVALID;
		m_pid_list[i].pid = -1;
	}

	clearProperty();
}

satipConfig::~satipConfig()
{
	DEBUG(MSG_MAIN,"Destruct satipConfig.\n");
}

void satipConfig::clearProperty()
{
	/* clear frontend params */
	m_frequency = 0;
	m_symrate = 0;

	m_msys = SYS_UNDEFINED; /* USE DEFAULT */
	m_mtype = QPSK;
	m_fec = FEC_AUTO;
	m_rolloff = ROLLOFF_AUTO;
	m_pilot = PILOT_AUTO;

	m_transmode = TRANSMISSION_MODE_AUTO;
	m_guard_interval = GUARD_INTERVAL_AUTO;
	m_bandwidth = 0; /* AUTO */
	m_plpid = 0;

	clearPidList();
}

void satipConfig::clearPidList()
{
	for (int i = 0; i < MAX_PIDS; i++)
	{
		m_pid_list[i].status = PID_INVALID;
	}
}

void satipConfig::updatePidList(u16* new_pid_list)
{
	DEBUG(MSG_MAIN, "====================== updatePidList ======================\n");
	int new_pid = -1;
	int cur_pid = -1;
	int found = 0;

	for (int new_index = 0; new_index < MAX_PIDS; new_index++)
	{
		if (new_pid_list[new_index] >= 0x2000)
			continue;

		new_pid = new_pid_list[new_index];

		DEBUG(MSG_MAIN, "receive PID : %d\n", (int)new_pid);
	}

	/* add pid */
	for (int new_index = 0; new_index < MAX_PIDS; new_index++)
	{
		new_pid = (int)new_pid_list[new_index];

		if (new_pid >= 0x2000)
			continue;

		found = 0;
		for (int cur_index = 0; cur_index < MAX_PIDS; cur_index++)
		{
			if(m_pid_list[cur_index].pid == new_pid)
			{
				found = 1;
				switch(m_pid_list[cur_index].status)
				{
					case PID_ADD:
						DEBUG(MSG_MAIN, "ADD PID(already added) : %d\n", new_pid);
						break;

					case PID_DELETE:
						DEBUG(MSG_MAIN, "ADD PID : %d\n", new_pid);
						m_pid_list[cur_index].status = PID_VAILD;

					case PID_VAILD:
						DEBUG(MSG_MAIN, "ADD PID(already added) : %d\n", new_pid);
						break;

					case PID_INVALID:
						DEBUG(MSG_MAIN, "ADD PID : %d\n", new_pid);
						m_pid_list[cur_index].status = PID_ADD;
						break;

					default:
						break;
				}
				
			}
		}

		if (!found)
		{
			int cur_index = 0;
			for (; cur_index < MAX_PIDS; cur_index++)
			{
				if(m_pid_list[cur_index].status == PID_INVALID)
				{
					DEBUG(MSG_MAIN, "ADD PID : %d\n", new_pid);
					m_pid_list[cur_index].pid = new_pid;
					m_pid_list[cur_index].status = PID_ADD;
					break;
				}
			}

			if (cur_index == MAX_PIDS)
			{
				ERROR(MSG_MAIN, "NEW PID ADD FAILED, m_pid_list list is fullfilled..");
			}
		}
	}

	/* del pid */
	for (int cur_index = 0; cur_index < MAX_PIDS; cur_index++)
	{
		cur_pid = m_pid_list[cur_index].pid;

		if (cur_pid >= 0x2000)
			continue;
	
		found = 0;
		for (int new_index = 0; new_index < MAX_PIDS; new_index++)
		{
			new_pid = (int)new_pid_list[new_index];

			if (new_pid >= 0x2000)
				continue;

			if (cur_pid == new_pid)
			{
				found = 1;
				break;
			}
		}

		if (!found)
		{
			switch(m_pid_list[cur_index].status)
			{
				case PID_ADD:
					DEBUG(MSG_MAIN, "DELETE PID : %d\n", cur_pid);
					m_pid_list[cur_index].status = PID_INVALID;
					break;

				case PID_DELETE:
					break;

				case PID_VAILD:
					DEBUG(MSG_MAIN, "DELETE PID : %d\n", cur_pid);
					m_pid_list[cur_index].status = PID_DELETE;
					break;

				case PID_INVALID:
					break;

				default:
					break;
			}
		}
	}

	updatePidStatus();

	DEBUG(MSG_MAIN, "====================== updatePidList END======================\n");

	return;
}

void satipConfig::updatePidStatus()
{
	/* update PID status */
	int pid_changed = 0;
	for (int cur_index = 0; cur_index < MAX_PIDS; cur_index++)
	{
		if ((m_pid_list[cur_index].status == PID_ADD) || (m_pid_list[cur_index].status == PID_DELETE))
		{
			pid_changed = 1;
			break;
		}
	}

	if (pid_changed)
		m_pid_status = CONFIG_STATUS_PID_CHANGED;
	else
		m_pid_status = CONFIG_STATUS_PID_STATIONARY;
}

void satipConfig::setVoltage(int voltage)
{
	m_lnb_voltage_onoff = CONFIG_LNB_ON;
	switch (voltage)
	{
		case SEC_VOLTAGE_13:
			m_pol = CONFIG_POL_VERTICAL;
			break;

		case SEC_VOLTAGE_18:
			m_pol = CONFIG_POL_HORIZONTAL;
			break;

		default: /*  SEC_VOLTAGE_OFF */
			m_lnb_voltage_onoff = CONFIG_LNB_OFF;
			m_status = CONFIG_STATUS_CHANNEL_INVALID;
			break;
	}
}

t_channel_status satipConfig::getChannelStatus()
{
	return m_status;
}

void satipConfig::setChannelChanged()
{
	m_status = CONFIG_STATUS_CHANNEL_CHANGED;
}

t_pid_status satipConfig::getPidStatus()
{
	return m_pid_status;
}

std::string satipConfig::getTuningData()
{
	std::string data;
	std::ostringstream oss_data;

	if (m_fe_type == FE_TYPE_SAT)
	{
		oss_data << "&src=" << m_signal_source;

		/* frequency */
		if (m_frequency%10)
			oss_data << "&freq=" << m_frequency/10 << '.' << m_frequency%10;

		else
			oss_data << "&freq=" << m_frequency/10;

		/* polarisation */
		switch (m_pol)
		{
			case CONFIG_POL_HORIZONTAL: oss_data << "&pol=h"; break;
			case CONFIG_POL_VERTICAL: 	oss_data << "&pol=v"; break;
			default: 					oss_data << "&pol=h"; break;
		}

		/* modulation system */
		switch (m_msys)
		{
			case SYS_DVBS: 	oss_data << "&msys=dvbs"; break;
			case SYS_DVBS2: oss_data << "&msys=dvbs2"; break;
			default: 		oss_data << "&msys=dvbs"; break;
		}

		/* symbol rate */
		oss_data << "&sr=" << m_symrate;

		/* fec inner */
		switch (m_fec)
		{
			case FEC_1_2:	oss_data << "&fec=12"; break;
			case FEC_2_3:	oss_data << "&fec=23"; break;
			case FEC_3_4:	oss_data << "&fec=34"; break;
			case FEC_5_6:	oss_data << "&fec=56"; break;
			case FEC_7_8:	oss_data << "&fec=78"; break;
			case FEC_8_9:	oss_data << "&fec=89"; break;
			case FEC_3_5:	oss_data << "&fec=35"; break;
			case FEC_4_5:	oss_data << "&fec=45"; break;
			case FEC_9_10:	oss_data << "&fec=910"; break;
			default:	break;
		}

        /* rolloff */
        switch (m_rolloff)
        {
            case ROLLOFF_35:	oss_data << "&ro=0.35"; break;
            case ROLLOFF_20:	oss_data << "&ro=0.20"; break;
            case ROLLOFF_25:	oss_data << "&ro=0.25"; break;
            default:            oss_data << "&ro=0.35"; break;
        }

        /* modulation type */
        switch (m_mtype)
        {
            case QPSK:	oss_data << "&mtype=qpsk"; break;
            case PSK_8:	oss_data << "&mtype=8psk"; break;
            default:	oss_data << "&mtype=qpsk"; break;
        }

        /* pilots */
        if(m_settings->m_force_plts)
            m_pilot = PILOT_ON;
        else
            m_pilot = PILOT_OFF;

        switch (m_pilot)
        {
            case PILOT_ON: 	oss_data << "&plts=on"; break;
            case PILOT_OFF: oss_data << "&plts=off"; break;
            default: break;
        }
		
	}
	else if (m_fe_type == FE_TYPE_CABLE)
	{
		/* frequency */
		if (m_frequency%10)
			oss_data << "&freq=" << m_frequency/10 << '.' << m_frequency%10;

		else
			oss_data << "&freq=" << m_frequency/10;

		/* modulation system */
		oss_data << "&msys=dvbc";

		/* modulation type */
		switch (m_mtype)
		{
			case QAM_16:	oss_data << "&mtype=16qam"; break;
			case QAM_32:	oss_data << "&mtype=32qam"; break;
			case QAM_64:	oss_data << "&mtype=64qam"; break;
			case QAM_128:	oss_data << "&mtype=128qam"; break;
			case QAM_256:	oss_data << "&mtype=256qam"; break;
			default:		break;
		}

		/* symbol rate */
		oss_data << "&sr=" << m_symrate;
	}
	else // (m_fe_type == FE_TYPE_TERRESTRIAL)
	{
		/* frequency */
		if (m_frequency%10)
			oss_data << "&freq=" << m_frequency/10 << '.' << m_frequency%10;

		else
			oss_data << "&freq=" << m_frequency/10;

		/* bandwidth */
		switch (m_bandwidth)
		{
			case 5000000:	oss_data << "&bw=5"; break;
			case 6000000:	oss_data << "&bw=6"; break;
			case 7000000:	oss_data << "&bw=7"; break;
			case 8000000:	oss_data << "&bw=8"; break;
			case 10000000:	oss_data << "&bw=10"; break;
			case 1712000:	oss_data << "&bw=1.712"; break;
			default:	break;
		}

		/* modulation system */
		switch (m_msys)
		{
			case SYS_DVBT: 	oss_data << "&msys=dvbt"; break;
			case SYS_DVBT2: oss_data << "&msys=dvbt2"; break;
			default: 		oss_data << "&msys=dvbt"; break;
		}

		/* transmission mode */
		switch (m_transmode)
		{
			case TRANSMISSION_MODE_2K:	oss_data << "&tmode=2k"; break;
			case TRANSMISSION_MODE_8K:	oss_data << "&tmode=4k"; break;
			case TRANSMISSION_MODE_4K:	oss_data << "&tmode=8k"; break;
			case TRANSMISSION_MODE_1K:	oss_data << "&tmode=1k"; break;
			case TRANSMISSION_MODE_16K:	oss_data << "&tmode=16k"; break;
			case TRANSMISSION_MODE_32K:	oss_data << "&tmode=32k"; break;
			default:	break;
		}

		/* modulation type */
		switch (m_mtype)
		{
			case QPSK:		oss_data << "&mtype=qpsk"; break;
			case QAM_16:	oss_data << "&mtype=16qam"; break;
			case QAM_64:	oss_data << "&mtype=64qam"; break;
			case QAM_256:	oss_data << "&mtype=256qam"; break;
			default:		break;
		}

		/* guard interval */
		switch (m_guard_interval)
		{
			case GUARD_INTERVAL_1_4:	oss_data << "&gi=14"; break;
			case GUARD_INTERVAL_1_8:	oss_data << "&gi=18"; break;
			case GUARD_INTERVAL_1_16:	oss_data << "&gi=116"; break;
			case GUARD_INTERVAL_1_32:	oss_data << "&gi=132"; break;
			case GUARD_INTERVAL_1_128:	oss_data << "&gi=1128"; break;
			case GUARD_INTERVAL_19_128:	oss_data << "&gi=19128"; break;
			case GUARD_INTERVAL_19_256:	oss_data << "&gi=19256"; break;
			default:	break;
		}

		/* fec inner */
		switch (m_fec)
		{
			case FEC_1_2:	oss_data << "&fec=12"; break;
			case FEC_3_5:	oss_data << "&fec=35"; break;
			case FEC_2_3:	oss_data << "&fec=23"; break;
			case FEC_3_4:	oss_data << "&fec=34"; break;
			case FEC_4_5:	oss_data << "&fec=45"; break;
			case FEC_5_6:	oss_data << "&fec=56"; break;
			case FEC_7_8:	oss_data << "&fec=78"; break;
			default:	break;
		}

		if (m_msys == SYS_DVBT2)
		{
			/* plp id */
			oss_data << "&plp=" << m_plpid;

			/* t2 system id */
			/* siso / miso */
		}
	}

	/* 
	src=1&freq=10202&pol=v&msys=dvbs&sr=27500&fec=34&pids=0,16,25,104
	src=1&freq=11538&pol=v&ro=0.35&msys=dvbs&mtype=qpsk&plts=off&sr=22000&fec=56&pids=0,611,621,631
	*/

	DEBUG(MSG_MAIN, "TUNE DATA : \n%s\n", oss_data.str().c_str());

	return oss_data.str();
}

std::string satipConfig::getSetupData()
{
	std::string data;
	std::ostringstream oss_data;

	if (m_status == CONFIG_STATUS_CHANNEL_CHANGED)
	{
		oss_data << getTuningData();
		m_status = CONFIG_STATUS_CHANNEL_STABLE;
	}

	std::ostringstream oss_addpid;
	for (int cur_index = 0; cur_index < MAX_PIDS; cur_index++)
	{
		if ((m_pid_list[cur_index].status == PID_ADD) || (m_pid_list[cur_index].status == PID_VAILD))
		{
			if (!oss_addpid.str().empty())
				oss_addpid << ',';

			oss_addpid << m_pid_list[cur_index].pid;

			m_pid_list[cur_index].status = PID_VAILD;
		}
	}

	if (!oss_addpid.str().empty()) 
	{
		oss_data << "&pids=" << oss_addpid.str();
	}
	else
	{
		oss_data << "&pids=none";
	}

	updatePidStatus();

	data = oss_data.str();
	if (!data.empty())
	{
		if(data[0] == '&')
			data.erase(data.begin());

		data.insert(data.begin(), '?');
	}

	/* 
	src=1&freq=10202&pol=v&msys=dvbs&sr=27500&fec=34&pids=0,16,25,104
	*/

	return data;
}

std::string satipConfig::getPlayData()
{
	std::string data;
	std::ostringstream oss_data;

	if (m_status == CONFIG_STATUS_CHANNEL_CHANGED)
	{
		oss_data << getTuningData();
		m_status = CONFIG_STATUS_CHANNEL_STABLE;
	}

	if (m_pid_status == CONFIG_STATUS_PID_CHANGED)
	{
		std::string addpid, delpid;
		std::ostringstream oss_addpid, oss_delpid;
		for (int cur_index = 0; cur_index < MAX_PIDS; cur_index++)
		{
			if (m_pid_list[cur_index].status == PID_ADD)
			{
				if (!oss_addpid.str().empty())
					oss_addpid << ',';

				oss_addpid << m_pid_list[cur_index].pid;

				m_pid_list[cur_index].status = PID_VAILD;
			}
			else if (m_pid_list[cur_index].status == PID_DELETE)
			{
				if (!oss_delpid.str().empty())
					oss_delpid << ',';

				oss_delpid << m_pid_list[cur_index].pid;

				m_pid_list[cur_index].status = PID_INVALID;
			}
		}

		if (!oss_addpid.str().empty())
		{
			oss_data << "&addpids=" << oss_addpid.str();
		}

		if (!oss_delpid.str().empty())
		{
			oss_data << "&delpids=" << oss_delpid.str();
		}

		updatePidStatus();
	}

	data = oss_data.str();
	if (!data.empty())
	{
		if(data[0] == '&')
			data.erase(data.begin());

		data.insert(data.begin(), '?');
	}

	/*
	?src=1&freq=11538&pol=v&ro=0.35&msys=dvbs&mtype=qpsk&plts=off&sr=22000&fec=56&pids=0,611,621,631
	*/

	return data;
}


