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
#include <fcntl.h>

#include <linux/dvb/version.h>
#include <linux/dvb/frontend.h>
#include <linux/dvb/dmx.h>

#include "satip_config.h"
#include "satip_vtuner.h"
#include "log.h"

/* from vtunerc_priv.h */
#define MAX_PIDTAB_LEN 30
#define PID_UNKNOWN 0x0FFFF

/* fixme: align with driver */
typedef unsigned int   u32;
typedef signed int     s32;
typedef unsigned short u16;
typedef unsigned char  u8;

/* driver interface */
#if defined(VTUNER_TYPE_VUPLUS)
#include "vtuner_vuplus.h"
#elif defined(VTUNER_TYPE_BLACKBOX)
#include "vtuner_blackbox7405.h"
#else
#include "vtuner.h"
#endif

typedef struct satip_vtuner
{
    int fd;
    int tone;
    char tone_set;
    t_satip_config* satip_cfg;
} t_satip_vtuner;

static t_mod_sys ms_map[] =
{
    // defined in linux/dvb/frontend.h
    SATIPCFG_MS_NONE,   // SYS_UNDEFINED
    SATIPCFG_MS_DVB_C,  // SYS_DVBC_ANNEX_A
    SATIPCFG_MS_DVB_C,  // SYS_DVBC_ANNEX_B
    SATIPCFG_MS_DVB_T,  // SYS_DVBT
    SATIPCFG_MS_NONE,   // SYS_DSS
    SATIPCFG_MS_DVB_S,  // SYS_DVBS
    SATIPCFG_MS_DVB_S2, // SYS_DVBS2
    SATIPCFG_MS_NONE,   // SYS_DVBH
    SATIPCFG_MS_NONE,   // SYS_ISDBT
    SATIPCFG_MS_NONE,   // SYS_ISDBS
    SATIPCFG_MS_NONE,   // SYS_ISDBC
    SATIPCFG_MS_NONE,   // SYS_ATSC
    SATIPCFG_MS_NONE,   // SYS_ATSCMH
    SATIPCFG_MS_NONE,   // SYS_DTMB
    SATIPCFG_MS_NONE,   // SYS_CMMB
    SATIPCFG_MS_NONE,   // SYS_DAB
    SATIPCFG_MS_DVB_T2, // SYS_DVBT2
    SATIPCFG_MS_NONE,   // SYS_TURBO
    SATIPCFG_MS_DVB_C   // SYS_DVBC_ANNEX_C
};

static t_mod_type mt_map[] =
{
    // defined in linux/dvb/frontend.h
    SATIPCFG_MT_QPSK,  // QPSK
    SATIPCFG_MT_16QAM, // QAM_16
    SATIPCFG_MT_32QAM, // QAM_32
    SATIPCFG_MT_64QAM, // QAM_64
    SATIPCFG_MT_128QAM,// QAM_128
    SATIPCFG_MT_256QAM,// QAM_256
    SATIPCFG_MT_NONE,  // QAM_AUTO
    SATIPCFG_MT_NONE,  // VSB_8
    SATIPCFG_MT_NONE,  // VSB_16
    SATIPCFG_MT_8PSK,  // PSK_8
    SATIPCFG_MT_NONE,  // APSK_16
    SATIPCFG_MT_NONE,  // APSK_32
    SATIPCFG_MT_NONE,  // DQPSK
    SATIPCFG_MT_NONE,  // QAM_4_NR
};

static t_fec_inner fec_map[] =
{
    // defined in linux/dvb/frontend.h
    SATIPCFG_F_NONE,   // FEC_NONE = 0,
    SATIPCFG_F_12,     // FEC_1_2
    SATIPCFG_F_23,     // FEC_2_3
    SATIPCFG_F_34,     // FEC_3_4
    SATIPCFG_F_45,     // FEC_4_5
    SATIPCFG_F_56,     // FEC_5_6
    SATIPCFG_F_NONE,   // FEC_6_7
    SATIPCFG_F_78,     // FEC_7_8
    SATIPCFG_F_89,     // FEC_8_9
    SATIPCFG_F_NONE,   // FEC_AUTO
    SATIPCFG_F_35,     // FEC_3_5
    SATIPCFG_F_910,    // FEC_9_10
    SATIPCFG_F_NONE    // FEC_2_5
};

static t_roll_off ro_map[] =
{
    // defined in linux/dvb/frontend.h
    SATIPCFG_R_0_35,   // ROLLOFF_35 /* Implied value in DVB-S, default for DVB-S2 */
    SATIPCFG_R_0_20,   // ROLLOFF_20
    SATIPCFG_R_0_25,   // ROLLOFF_25
    SATIPCFG_R_0_35	   // ROLLOFF_AUTO
};

static t_inversion i_map[] =
{
    // defined in linux/dvb/frontend.h
    SATIPCFG_I_OFF,    // INVERSION_OFF
    SATIPCFG_I_ON,     // INVERSION_ON
    SATIPCFG_I_NONE    // INVERSION_AUTO
};

static t_pilots pilot_map[] =
{
    // defined in linux/dvb/frontend.h
    SATIPCFG_P_ON,     // PILOT_ON
    SATIPCFG_P_OFF,    // PILOT_OFF
    SATIPCFG_P_ON	   // PILOT_AUTO

};

static t_trans_mode tm_map[] =
{
    // defined in linux/dvb/frontend.h
    SATIPCFG_TM_2K,    // TRANSMISSION_MODE_2K
    SATIPCFG_TM_8K,    // TRANSMISSION_MODE_8K
    SATIPCFG_TM_NONE,  // TRANSMISSION_MODE_AUTO
    SATIPCFG_TM_4K,    // TRANSMISSION_MODE_4K
    SATIPCFG_TM_1K,    // TRANSMISSION_MODE_1K
    SATIPCFG_TM_16K,   // TRANSMISSION_MODE_16K
    SATIPCFG_TM_32K,   // TRANSMISSION_MODE_32K
    SATIPCFG_TM_NONE,  // TRANSMISSION_MODE_C1
    SATIPCFG_TM_NONE   // TRANSMISSION_MODE_C3780
};

static t_bandwidth bw_map[] =
{
    // defined in linux/dvb/frontend.h
    SATIPCFG_BW_8MHZ,  // BANDWIDTH_8_MHZ
    SATIPCFG_BW_7MHZ,  // BANDWIDTH_7_MHZ
    SATIPCFG_BW_6MHZ,  // BANDWIDTH_6_MHZ
    SATIPCFG_BW_NONE,  // BANDWIDTH_AUTO
    SATIPCFG_BW_5MHZ,  // BANDWIDTH_5_MHZ
    SATIPCFG_BW_10MHZ, // BANDWIDTH_10_MHZ
    SATIPCFG_BW_1_712MHZ  // BANDWIDTH_1_712_MHZ
};

static t_guard_interval gi_map[] =
{
    // defined in linux/dvb/frontend.h
    SATIPCFG_GI_1_32,  // GUARD_INTERVAL_1_32
    SATIPCFG_GI_1_16,  // GUARD_INTERVAL_1_16
    SATIPCFG_GI_1_8,   // GUARD_INTERVAL_1_8
    SATIPCFG_GI_1_4,   // GUARD_INTERVAL_1_4
    SATIPCFG_GI_NONE,  // GUARD_INTERVAL_AUTO
    SATIPCFG_GI_1_128, // GUARD_INTERVAL_1_128
    SATIPCFG_GI_19_128,// GUARD_INTERVAL_19_128
    SATIPCFG_GI_19_256,// GUARD_INTERVAL_19_256
    SATIPCFG_GI_NONE,  // GUARD_INTERVAL_PN420
    SATIPCFG_GI_NONE,  // GUARD_INTERVAL_PN595
    SATIPCFG_GI_NONE,  // GUARD_INTERVAL_PN945
};

t_satip_vtuner* satip_vtuner_new(char* devname,t_satip_config* satip_cfg)
{
    int fd;
    t_satip_vtuner* vt;
    char typestr[8];
    struct dvb_frontend_info *fe_info;

    fd  = open(devname, O_RDWR);
    if ( fd < 0)
    {
        ERROR(MSG_MAIN,"Couldn't open %s\n",devname);
        return NULL;
    }

    fe_info = malloc(sizeof(struct dvb_frontend_info));
    if (!fe_info)
    {
        close(fd);
        return NULL;
    }

    switch(satip_cfg->fe_type)
    {
    case SATIP_FE_TYPE_SAT:
        strncpy(fe_info->name, "SAT>IP DVB-S2 NIM", sizeof(fe_info->name));
        strncpy((char*)&typestr, "DVB-S2", sizeof(typestr));
        fe_info->type = FE_QPSK;
        fe_info->frequency_min=925000;
        fe_info->frequency_max=2175000;
        fe_info->frequency_stepsize=125000;
        fe_info->frequency_tolerance=0;
        fe_info->symbol_rate_min=1000000;
        fe_info->symbol_rate_max=45000000;
        fe_info->symbol_rate_tolerance=0;
        fe_info->notifier_delay=0;
        fe_info->caps=FE_CAN_INVERSION_AUTO |
                      FE_CAN_FEC_1_2 | FE_CAN_FEC_2_3 | FE_CAN_FEC_3_4 | FE_CAN_FEC_4_5 |
                      FE_CAN_FEC_5_6 | FE_CAN_FEC_7_8 | FE_CAN_FEC_8_9 |
                      FE_CAN_QPSK | FE_CAN_RECOVER | FE_CAN_2G_MODULATION | FE_HAS_EXTENDED_CAPS;
        break;
    case SATIP_FE_TYPE_CAB:
        strncpy(fe_info->name, "SAT>IP DVB-C NIM", sizeof(fe_info->name));
        strncpy(typestr, "DVB-C", sizeof(typestr));
        fe_info->type = FE_QAM;
        fe_info->frequency_min=51000000;
        fe_info->frequency_max=858000000;
        fe_info->frequency_stepsize=62500;
        fe_info->frequency_tolerance=0;
        fe_info->symbol_rate_min=(57840000/2)/64;     /* SACLK/64 == (XIN/2)/64 */
        fe_info->symbol_rate_max=(57840000/2)/4;      /* SACLK/4 */
        fe_info->symbol_rate_tolerance=0;
        fe_info->notifier_delay=0;
        fe_info->caps=FE_CAN_INVERSION_AUTO |
                      FE_CAN_QAM_16 | FE_CAN_QAM_32 | FE_CAN_QAM_64 | FE_CAN_QAM_128 |
                      FE_CAN_QAM_256 | FE_CAN_RECOVER | FE_CAN_FEC_AUTO;
        break;
    case SATIP_FE_TYPE_TER:
        strncpy(fe_info->name, "SAT>IP DVB-T NIM", sizeof(fe_info->name));
        strncpy(typestr, "DVB-T", sizeof(typestr));
        fe_info->type = FE_OFDM;
        fe_info->frequency_min=0;
        fe_info->frequency_max=863250000;
        fe_info->frequency_stepsize=62500;
        fe_info->frequency_tolerance=0;
        fe_info->notifier_delay=0;
        fe_info->caps=FE_CAN_INVERSION_AUTO |
                      FE_CAN_FEC_1_2 | FE_CAN_FEC_2_3 | FE_CAN_FEC_3_4 |
                      FE_CAN_FEC_5_6 | FE_CAN_FEC_7_8 | FE_CAN_FEC_AUTO |
                      FE_CAN_QAM_16 | FE_CAN_QAM_64 | FE_CAN_QPSK |
                      FE_CAN_TRANSMISSION_MODE_AUTO | FE_CAN_GUARD_INTERVAL_AUTO |
                      FE_CAN_HIERARCHY_AUTO | FE_CAN_RECOVER;
        break;
    default:
        ERROR(MSG_MAIN,"Unknown frontend type: %d\n",satip_cfg->fe_type);
        close(fd);
        free(fe_info);
        return NULL;
    }

    INFO(MSG_MAIN,"Frontend type: %s, name: %s\n", typestr, &fe_info->name);

    if ( ioctl(fd, VTUNER_SET_NAME, &fe_info->name) )
        perror("ioctl(VTUNER_SET_NAME)");

    if ( ioctl(fd, VTUNER_SET_TYPE, typestr)  )
    {
        perror("ioctl(VTUNER_SET_TYPE)");
    }
    if ( ioctl(fd, VTUNER_SET_FE_INFO, fe_info) )
    {
        perror("ioctl(VTUNER_SET_FE_INFO)");
    }

#ifdef VTUNER_TYPE_VUPLUS
    if ( ioctl(fd, VTUNER_SET_HAS_OUTPUTS, "no") )
    {
        perror("ioctl(VTUNER_SET_HAS_OUTPUTS)");
    }
#if DVB_API_VERSION > 5 || DVB_API_VERSION == 5 && DVB_API_VERSION_MINOR >= 5
    {
        struct dtv_property p[1];
        u32 ncaps = 0;
        p[0].cmd = DTV_ENUM_DELSYS; /* Not used, but kept for reference. */

        switch(satip_cfg->fe_type)
        {
        case SATIP_FE_TYPE_SAT:
            p[0].u.buffer.data[ncaps++] = SYS_DVBS;
            if (fe_info->caps & FE_CAN_2G_MODULATION)
                p[0].u.buffer.data[ncaps++] = SYS_DVBS2;
            break;
        case SATIP_FE_TYPE_CAB:
            p[0].u.buffer.data[ncaps++] = SYS_DVBC_ANNEX_A;
            break;
        case SATIP_FE_TYPE_TER:
            p[0].u.buffer.data[ncaps++] = SYS_DVBT;
            if (fe_info->caps & FE_CAN_2G_MODULATION)
                p[0].u.buffer.data[ncaps++] = SYS_DVBT2;
            break;
        default:
            ERROR(MSG_MAIN,"Unknown delivery system: %d\n",satip_cfg->fe_type);
            break;
        }
        p[0].u.buffer.len = ncaps;
        ioctl(fd, VTUNER_SET_DELSYS, p[0].u.buffer.data);
    }
#endif

#endif

    free(fe_info);

    vt=(t_satip_vtuner*)malloc(sizeof(t_satip_vtuner));

    vt->fd=fd;
    vt->satip_cfg=satip_cfg;

    /* set default position A, if appl. does not configure */
    satip_set_position(satip_cfg,1);

    vt->tone_set=0;

    return vt;
}

int satip_vtuner_fd(struct satip_vtuner* vt)
{
    return vt->fd;
}


static void set_frontend(struct satip_vtuner* vt, struct vtuner_message* msg)
{
    if (vt->satip_cfg->fe_type == SATIP_FE_TYPE_SAT)
    {
        int fec_inner=msg->body.fe_params.u.qpsk.fec_inner;
        int frequency = msg->body.fe_params.frequency/100;

        if ( !vt->tone_set )
        {
            DEBUG(MSG_MAIN,"cannot tune: no band selected\n");
            return;
        }

        /* revert frequency shift */
        if ( vt->tone == SEC_TONE_ON ) /* high band */
            frequency += 106000;
        else /* low band */
            if ( frequency-97500 < 0 )
                frequency+=97500;
            else
                frequency-=97500;

        satip_set_freq(vt->satip_cfg, frequency);
        satip_set_symbol_rate(vt->satip_cfg, msg->body.fe_params.u.qpsk.symbol_rate/1000 );

        if ( ( fec_inner & 31 ) < SATIPCFG_F_NONE )
            satip_set_fecinner(vt->satip_cfg, fec_map[fec_inner & 31]);
        else
            ERROR(MSG_MAIN,"invalid FEC configured\n");

#ifdef VTUNER_TYPE_ORIGINAL
        /* modulation system and modulation piggybacked in FEC DVB-S(2) for old DVB API version 3*/
        if ( fec_inner & 32 )
        {
            int inversion=msg->body.fe_params.inversion;

            satip_set_modsys(vt->satip_cfg, SATIPCFG_MS_DVB_S2);

            /* DVB-S2: modulation */
            if ( fec_inner & 64 )
                satip_set_modtype(vt->satip_cfg, SATIPCFG_MT_8PSK);
            else
                satip_set_modtype(vt->satip_cfg, SATIPCFG_MT_QPSK);

            /* DVB-S2: rolloff and pilots encoded in inversion */
            if ( inversion & 0x04 )
                satip_set_rolloff(vt->satip_cfg,SATIPCFG_R_0_25);
            else if (inversion & 0x08)
                satip_set_rolloff(vt->satip_cfg,SATIPCFG_R_0_20);
            else
                satip_set_rolloff(vt->satip_cfg,SATIPCFG_R_0_35);

            if ( inversion & 0x10 )
                satip_set_pilots(vt->satip_cfg,SATIPCFG_P_ON);
            else if ( inversion & 0x20 )
                satip_set_pilots(vt->satip_cfg,SATIPCFG_P_ON);
            else
                satip_set_pilots(vt->satip_cfg,SATIPCFG_P_OFF);
        }
        else
            satip_set_modsys(vt->satip_cfg, SATIPCFG_MS_DVB_S);
#else
        /*VU+
          Do not set modulation system (aka delivery system)!
          This is done using FE_SET_PROPERTY(DTV_DELIVERY_SYSTEM)!
        */
#endif

        DEBUG(MSG_MAIN,"MSG_SET_FRONTEND freq: %d symrate: %d \n",
              frequency,
              msg->body.fe_params.u.qpsk.symbol_rate );
    }
    else if (vt->satip_cfg->fe_type == SATIP_FE_TYPE_CAB)
    {
        int fec_inner = msg->body.fe_params.u.qam.fec_inner;
        int inversion=msg->body.fe_params.inversion;
        int frequency = msg->body.fe_params.frequency;

        satip_set_freq(vt->satip_cfg, frequency);
        satip_set_inversion(vt->satip_cfg, i_map[inversion]);
        satip_set_symbol_rate(vt->satip_cfg, msg->body.fe_params.u.qam.symbol_rate/1000 );

        if ( ( fec_inner & 31 ) < SATIPCFG_F_NONE )
            satip_set_fecinner(vt->satip_cfg, fec_map[fec_inner & 31]);
        else
            ERROR(MSG_MAIN,"invalid FEC configured\n");

#ifdef VTUNER_TYPE_ORIGINAL
        int modulation = msg->body.fe_params.u.qam.modulation;

        satip_set_modtype(vt->satip_cfg, mt_map[modulation]);
        satip_set_modsys(vt->satip_cfg, SATIPCFG_MS_DVB_C);
#else
        /*VU+
          Do not set modulation system (aka delivery system)!
          This is done using FE_SET_PROPERTY(DTV_DELIVERY_SYSTEM)!
        */
#endif

        DEBUG(MSG_MAIN,"MSG_SET_FRONTEND freq: %d symrate: %d \n",
              frequency,
              msg->body.fe_params.u.qam.symbol_rate );
    }
    else if (vt->satip_cfg->fe_type == SATIP_FE_TYPE_TER)
    {
        int coderate_hp = msg->body.fe_params.u.ofdm.code_rate_HP;
        int coderate_lp = msg->body.fe_params.u.ofdm.code_rate_LP;
        int bandwidth = msg->body.fe_params.u.ofdm.bandwidth;
        int transmode = msg->body.fe_params.u.ofdm.transmission_mode;
        int guardint = msg->body.fe_params.u.ofdm.guard_interval;
        int frequency = msg->body.fe_params.frequency;

        /*
          TODO: do not know how to handle this as the SAT>IP spec does not
                differentiate between coderate_hp and coderate_lp!
                Use coderate_hp for now.
        */
        if ( coderate_hp != coderate_lp)
            WARN(MSG_MAIN,"coderate_hp and coderate_lp are not equal! Using coderate_hp for FEC!\n");

        int fec_inner = coderate_hp;

        if ( ( fec_inner & 31 ) < SATIPCFG_F_NONE )
            satip_set_fecinner(vt->satip_cfg, fec_map[fec_inner & 31]);
        else
            ERROR(MSG_MAIN,"invalid FEC configured\n");

        satip_set_freq(vt->satip_cfg, frequency);
        satip_set_bandwidth(vt->satip_cfg, bw_map[bandwidth]);
        satip_set_transmode(vt->satip_cfg, tm_map[transmode]);
        satip_set_guardinterval(vt->satip_cfg, gi_map[guardint]);

#ifdef VTUNER_TYPE_ORIGINAL
        int constellation = msg->body.fe_params.u.ofdm.constellation;

        satip_set_modtype(vt->satip_cfg, mt_map[constellation]);
        satip_set_modsys(vt->satip_cfg, SATIPCFG_MS_DVB_T);
#else
        /*VU+
          Do not set modulation system (aka delivery system)!
          This is done using FE_SET_PROPERTY(DTV_DELIVERY_SYSTEM)!
        */
#endif

        DEBUG(MSG_MAIN,"MSG_SET_FRONTEND freq: %d \n",
              frequency);
    }

}

static void set_tone(struct satip_vtuner* vt, struct vtuner_message* msg)
{
    vt->tone = msg->body.tone;
    vt->tone_set = 1;

    DEBUG(MSG_MAIN,"MSG_SET_TONE:  %s\n",vt->tone == SEC_TONE_ON  ? "ON = high band" : "OFF = low band");
}


static void set_voltage(struct satip_vtuner* vt, struct vtuner_message* msg)
{
    if ( msg->body.voltage == SEC_VOLTAGE_13 )
        satip_set_polarization(vt->satip_cfg, SATIPCFG_P_VERTICAL);
    else if (msg->body.voltage == SEC_VOLTAGE_18)
        satip_set_polarization(vt->satip_cfg, SATIPCFG_P_HORIZONTAL);
    else  /*  SEC_VOLTAGE_OFF */
        satip_lnb_off(vt->satip_cfg);

    DEBUG(MSG_MAIN,"MSG_SET_VOLTAGE:  %d\n",msg->body.voltage);
}


static void diseqc_msg(struct satip_vtuner* vt, struct vtuner_message* msg)
{
    char dbg[50];
    struct diseqc_master_cmd* cmd=&msg->body.diseqc_master_cmd;

    if ( cmd->msg[0] == 0xe0 &&
            cmd->msg[1] == 0x10 &&
            cmd->msg[2] == 0x38 &&
            cmd->msg_len == 4 )
    {
        /* committed switch */
        u8 data=cmd->msg[3];

        if ( (data & 0x01) == 0x01 )
        {
            vt->tone = SEC_TONE_ON; /* high band */
            vt->tone_set=1;
        }
        else if ( (data & 0x11) == 0x10 )
        {
            vt->tone = SEC_TONE_OFF; /* low band */
            vt->tone_set=1;
        }

        if ( (data & 0x02) == 0x02 )
            satip_set_polarization(vt->satip_cfg, SATIPCFG_P_HORIZONTAL);
        else if ( (data & 0x22) == 0x20 )
            satip_set_polarization(vt->satip_cfg, SATIPCFG_P_VERTICAL);

        /* some invalid combinations ? */
        satip_set_position(vt->satip_cfg, ( (data & 0x0c) >> 2) + 1 );
    }

    sprintf(dbg,"%02x %02x %02x   msg %02x %02x %02x len %d",
            msg->body.diseqc_master_cmd.msg[0],
            msg->body.diseqc_master_cmd.msg[1],
            msg->body.diseqc_master_cmd.msg[2],
            msg->body.diseqc_master_cmd.msg[3],
            msg->body.diseqc_master_cmd.msg[4],
            msg->body.diseqc_master_cmd.msg[5],
            msg->body.diseqc_master_cmd.msg_len);
    DEBUG(MSG_MAIN,"MSG_SEND_DISEQC_MSG:  %s\n",dbg);
}



static void set_pidlist(struct satip_vtuner* vt, struct vtuner_message* msg)
{
    int i;
    u16* pidlist=msg->body.pidlist;

    DEBUG(MSG_MAIN,"MSG_SET_PIDLIST:\n");

    satip_del_allpid(vt->satip_cfg);

    for (i=0; i<MAX_PIDTAB_LEN; i++)
        if (pidlist[i] < 8192  )
        {
            satip_add_pid(vt->satip_cfg,pidlist[i]);
            DEBUG(MSG_MAIN,"%d\n",pidlist[i]);
        }
}

static void set_property(struct satip_vtuner* vt, struct vtuner_message* msg)
{
#if DVB_API_VERSION >= 5
    struct dtv_property *prop = &msg->body.prop;

    switch (prop->cmd)
    {
    case DTV_TUNE:
        DEBUG(MSG_MAIN,"set_property: DTV_TUNE: Ignoring. Waiting for final FE_SET_FRONTEND.\n");
        break;
    case DTV_CLEAR:
        DEBUG(MSG_MAIN,"set_property: DTV_CLEAR: Ignoring.\n");
        break;
    case DTV_DELIVERY_SYSTEM:
        DEBUG(MSG_MAIN,"set_property: DTV_DELIVERY_SYSTEM: Setting delivery system: %d\n", prop->u.data);
        satip_set_modsys(vt->satip_cfg, ms_map[prop->u.data]);
        break;
    case DTV_FREQUENCY:
        DEBUG(MSG_MAIN,"set_property: DTV_FREQUENCY: Ignoring.\n");
        break;
    case DTV_MODULATION:
        DEBUG(MSG_MAIN,"set_property: DTV_MODULATION: Setting modulation type: %d\n", prop->u.data);
        satip_set_modtype(vt->satip_cfg, mt_map[prop->u.data]);
        break;
    case DTV_SYMBOL_RATE:
        DEBUG(MSG_MAIN,"set_property: DTV_SYMBOL_RATE: Ignoring.\n");
        break;
    case DTV_INNER_FEC:
        DEBUG(MSG_MAIN,"set_property: DTV_INNER_FEC: Ignoring.\n");
        break;
    case DTV_INVERSION:
        DEBUG(MSG_MAIN,"set_property: DTV_INVERSION: Ignoring.\n");
        break;
    case DTV_ROLLOFF:
        DEBUG(MSG_MAIN,"set_property: DTV_ROLLOFF: Setting roll-off: %d\n", prop->u.data);
        satip_set_rolloff(vt->satip_cfg, ro_map[prop->u.data]);
        break;
    case DTV_PILOT:
        DEBUG(MSG_MAIN,"set_property: DTV_PILOT: Setting pilot: %d\n", prop->u.data);
        satip_set_pilots(vt->satip_cfg, pilot_map[prop->u.data]);
        break;
    case DTV_CODE_RATE_LP:
        DEBUG(MSG_MAIN,"set_property: DTV_CODE_RATE_LP: Ignoring.\n");
        break;
    case DTV_CODE_RATE_HP:
        DEBUG(MSG_MAIN,"set_property: DTV_CODE_RATE_HP: Ignoring.\n");
        break;
    case DTV_TRANSMISSION_MODE:
        DEBUG(MSG_MAIN,"set_property: DTV_TRANSMISSION_MODE: Ignoring.\n");
        break;
    case DTV_GUARD_INTERVAL:
        DEBUG(MSG_MAIN,"set_property: DTV_GUARD_INTERVAL: Ignoring.\n");
        break;
    case DTV_HIERARCHY:
        DEBUG(MSG_MAIN,"set_property: DTV_HIERARCHY: Ignoring.\n");
        break;
    case DTV_BANDWIDTH_HZ:
        DEBUG(MSG_MAIN,"set_property: DTV_BANDWIDTH_HZ: Ignoring.\n");
        break;
#if DVB_API_VERSION > 5 || DVB_API_VERSION == 5 && DVB_API_VERSION_MINOR >= 9
    case DTV_STREAM_ID:
        DEBUG(MSG_MAIN,"set_property: DTV_STREAM_ID: Setting plp stream id: %d \n", prop->u.data);
        satip_set_plp(vt->satip_cfg, prop->u.data);
        break;
#elif DVB_API_VERSION > 5 || DVB_API_VERSION == 5 && DVB_API_VERSION_MINOR >= 3
    case DTV_DVBT2_PLP_ID:
        DEBUG(MSG_MAIN,"set_property: DTV_DVBT2_PLP_ID: Setting plp stream id: %d \n");
        satip_set_plp(vt->satip_cfg, prop->u.data);
        break;
#endif

    default:
        break;
    }

#else
    DEBUG(MSG_MAIN,"set_property: Not supported by DVB-API version: %d.%d\n", DVB_API_VERSION, DVB_API_VERSION_MINOR);
#endif
}

static void get_property(struct satip_vtuner* vt, struct vtuner_message* msg)
{
#if DVB_API_VERSION >= 5
    struct dtv_property *prop = &msg->body.prop;

    switch (prop->cmd)
    {
    case DTV_DELIVERY_SYSTEM:
        DEBUG(MSG_MAIN,"get_property: DTV_DELIVERY_SYSTEM\n");
        break;
    case DTV_FREQUENCY:
        DEBUG(MSG_MAIN,"get_property: DTV_FREQUENCY\n");
        break;
    case DTV_MODULATION:
        DEBUG(MSG_MAIN,"get_property: DTV_MODULATION\n");
        break;
    case DTV_SYMBOL_RATE:
        DEBUG(MSG_MAIN,"get_property: DTV_SYMBOL_RATE\n");
        break;
    case DTV_INNER_FEC:
        DEBUG(MSG_MAIN,"get_property: DTV_INNER_FEC\n");
        break;
    case DTV_INVERSION:
        DEBUG(MSG_MAIN,"get_property: DTV_INVERSION\n");
        break;
    case DTV_ROLLOFF:
        DEBUG(MSG_MAIN,"get_property: DTV_ROLLOFF\n");
        break;
    case DTV_CODE_RATE_LP:
        DEBUG(MSG_MAIN,"get_property: DTV_CODE_RATE_LP\n");
        break;
    case DTV_CODE_RATE_HP:
        DEBUG(MSG_MAIN,"get_property: DTV_CODE_RATE_HP\n");
        break;
    case DTV_TRANSMISSION_MODE:
        DEBUG(MSG_MAIN,"get_property: DTV_TRANSMISSION_MODE\n");
        break;
    case DTV_GUARD_INTERVAL:
        DEBUG(MSG_MAIN,"get_property: DTV_GUARD_INTERVAL\n");
        break;
    case DTV_HIERARCHY:
        DEBUG(MSG_MAIN,"get_property: DTV_HIERARCHY\n");
        break;
    case DTV_BANDWIDTH_HZ:
        DEBUG(MSG_MAIN,"get_property: DTV_BANDWIDTH_HZ\n");
        break;
#if DVB_API_VERSION > 5 || DVB_API_VERSION == 5 && DVB_API_VERSION_MINOR >= 9
    case DTV_STREAM_ID:
        DEBUG(MSG_MAIN,"get_property: DTV_STREAM_ID\n");
        break;
    case DTV_PILOT:
        DEBUG(MSG_MAIN,"get_property: DTV_PILOT\n");
        break;
#elif DVB_API_VERSION > 5 || DVB_API_VERSION == 5 && DVB_API_VERSION_MINOR >= 3
    case DTV_PILOT:
        DEBUG(MSG_MAIN,"get_property: DTV_DVBT2_PLP_ID\n");
        break;
#endif

    default:
        break;
    }

#else
    DEBUG(MSG_MAIN,"get_property: Not supported by DVB-API version: %d.%d\n", DVB_API_VERSION, DVB_API_VERSION_MINOR);
#endif
}

void satip_vtuner_event(struct satip_vtuner* vt)
{
    struct vtuner_message  msg;

    if (ioctl(vt->fd, VTUNER_GET_MESSAGE, &msg))
        return;

    DEBUG(MSG_MAIN,"msg_type: %d\n", msg.type);

    switch(msg.type)
    {
    case MSG_SET_FRONTEND:
        set_frontend(vt,&msg);
        break;

    case MSG_GET_FRONTEND:
        DEBUG(MSG_MAIN,"MSG_GET_FRONTEND: Not implemented!\n");
        break;

    case MSG_SET_TONE:
        set_tone(vt,&msg);
        break;

    case MSG_SET_VOLTAGE:
        set_voltage(vt,&msg);
        break;

    case MSG_PIDLIST:
        set_pidlist(vt,&msg);
        DEBUG(MSG_MAIN,"set_pidlist: no response required\n");
        return;
        break;

    case MSG_READ_STATUS:
        DEBUG(MSG_MAIN,"MSG_READ_STATUS\n");
        msg.body.status =    // tuning ok!
            // FE_HAS_SIGNAL     |
            // FE_HAS_CARRIER    |
            // FE_HAS_VITERBI    |
            // FE_HAS_SYNC       |
            FE_HAS_LOCK;
        break;

    case MSG_READ_BER:
        DEBUG(MSG_MAIN,"MSG_READ_BER\n");
        msg.body.ber = 0;
        break;

    case MSG_READ_SIGNAL_STRENGTH:
        DEBUG(MSG_MAIN,"MSG_READ_SIGNAL_STRENGTH\n");
        msg.body.ss = 50;
        break;

    case MSG_READ_SNR:
        DEBUG(MSG_MAIN,"MSG_READ_SNR\n");
        msg.body.snr = 900; /* ?*/
        break;

    case MSG_READ_UCBLOCKS:
        DEBUG(MSG_MAIN,"MSG_READ_UCBLOCKS\n");
        msg.body.ucb = 0;
        break;

    case MSG_SEND_DISEQC_BURST:
        DEBUG(MSG_MAIN,"MSG_SEND_DISEQC_BURST\n");
        if ( msg.body.burst == SEC_MINI_A )
            satip_set_position(vt->satip_cfg,1);
        else if ( msg.body.burst == SEC_MINI_B )
            satip_set_position(vt->satip_cfg,2);
        else
            ERROR(MSG_MAIN,"invalid diseqc burst %d\n",msg.body.burst);
        break;

    case MSG_SEND_DISEQC_MSG:
        diseqc_msg(vt, &msg);
        break;

    case MSG_SET_PROPERTY:
        set_property(vt, &msg);
        break;

    case MSG_GET_PROPERTY:
        DEBUG(MSG_MAIN,"MSG_GET_PROPERTY: !!! Not implemented !!!\n");
        get_property(vt, &msg);
        break;

    default:
        DEBUG(MSG_MAIN,"MSG_UNKNOWN: %d\n", msg.type);
        break;
    }

    msg.type=0;

    if (ioctl(vt->fd, VTUNER_SET_RESPONSE, &msg))
        return;

    DEBUG(MSG_MAIN,"ioctl: response ok\n");
}
