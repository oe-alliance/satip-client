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

#ifndef _CONFIG_H_
#define _CONFIG_H_

#include <string>
#include <linux/dvb/frontend.h>

#include "option.h"

#define MAX_PIDS 30 // from usbtunerhelper

typedef unsigned short u16;

typedef enum
{
	SATIPCFG_OK = 0,
	SATIPCFG_ERROR
}t_config_res;

enum
{
	FE_TYPE_SAT = 0,
	FE_TYPE_CABLE,
	FE_TYPE_TERRESTRIAL
};

typedef enum
{
	CONFIG_POL_HORIZONTAL = 0,
	CONFIG_POL_VERTICAL
}t_type_pol;

typedef enum
{
	CONFIG_LNB_OFF = 0,
	CONFIG_LNB_ON
}t_lnb_onoff;

typedef enum
{
	CONFIG_STATUS_CHANNEL_INVALID = 0,
	CONFIG_STATUS_CHANNEL_CHANGED,
	CONFIG_STATUS_CHANNEL_STABLE
}t_channel_status;

typedef enum
{
	CONFIG_STATUS_PID_STATIONARY = 0,
	CONFIG_STATUS_PID_CHANGED
}t_pid_status;

class satipConfig
{
public:
	satipConfig(int fe_type, vtunerOpt* settings);
	virtual ~satipConfig();

	int getFeType() {return m_fe_type;}

	/* vtuner property */
	void clearProperty();
	void setVoltage(int voltage);

	void setPosition(int pos)
	{ 
		m_signal_source = pos;
	}
	
	void setFrequency(unsigned int freq) 
	{ 
		m_frequency = freq;
	}
	void setModsys(int system) 
	{
		m_msys = system;
	}
	void setModtype(int modulation)
	{
		m_mtype = modulation;
	}
	void setSymrate(int symrate) 
	{ 
		m_symrate= symrate;
	}
	void setFec(int fec) 
	{ 
		m_fec = fec;
	}
	void setRolloff(int rolloff) 
	{ 
		m_rolloff = rolloff;
	}
	void setPilots(int pilots) 
	{ 
		m_pilot = pilots;
	}

	/* DVB-T */
	void setTransmode(int transmode) 
	{ 
		m_transmode = transmode;
	}
	void setGuardInterval(int gi) 
	{ 
		m_guard_interval = gi;
	}
	void setBandwidth(int bandwidth) 
	{ 
		m_bandwidth = bandwidth;
	}
	void setPLP(int plpid) 
	{ 
		m_plpid = plpid;
	}
	void setPLScode(int pls_code)
	{
		m_pls_code = pls_code;
	}

	/* channel, pid status */
	t_channel_status getChannelStatus();
	void setChannelChanged();
	t_pid_status getPidStatus();
	void updatePidList(u16* new_pid_list);
	void updatePidStatus();

	/* write RTSP message */
	std::string getSetupData();
	std::string getPlayData();

private:
	int m_fe_type;

	typedef enum
	{
		PID_ADD = 0,
		PID_DELETE,
		PID_VAILD,
		PID_INVALID
	}t_pid_elem_status;
	
	struct pid_elem
	{
		int pid;
		t_pid_elem_status status;
	};

	struct pid_elem m_pid_list[MAX_PIDS];

	void clearPidList();

	/* frontend params */
	int m_signal_source;
	unsigned int m_frequency;
	int m_pol;
	int m_rolloff;
	int m_msys; // dvbs, dvbs2
	int m_mtype; // qpsk, 8psk
	int m_pilot; //plts=off
	int m_symrate; // sr=22000
	int m_fec; // fec=23

	int m_transmode;
	int m_guard_interval;
	int m_bandwidth;
	int m_plpid;
	int m_pls_code; // gold

	t_channel_status m_status;
	t_pid_status m_pid_status;

	t_lnb_onoff m_lnb_voltage_onoff;
	
	vtunerOpt* m_settings;

	std::string getTuningData();
};

#endif /* _CONFIG_H_ */

