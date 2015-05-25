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

#ifndef _SATIP_CONFIG_H
#define _SATIP_CONFIG_H

/* DVB-S, DVB-S2 only */
typedef enum
{
    SATIPCFG_P_HORIZONTAL = 0,
    SATIPCFG_P_VERTICAL,
    SATIPCFG_P_CIRC_LEFT,
    SATIPCFG_P_CIRC_RIGHT
} t_polarization;

/* DVB-S2 only */
typedef enum
{
    SATIPCFG_R_0_35 = 0,
    SATIPCFG_R_0_25,
    SATIPCFG_R_0_20,
    SATIPCFG_R_NONE
} t_roll_off;

/* DVB-C */
typedef enum
{
    SATIPCFG_I_OFF = 0,
    SATIPCFG_I_ON,
    SATIPCFG_I_NONE
} t_inversion;

/* DVB-* */
typedef enum
{
    SATIPCFG_MS_DVB_S = 0,
    SATIPCFG_MS_DVB_S2,
    SATIPCFG_MS_DVB_T,
    SATIPCFG_MS_DVB_T2,
    SATIPCFG_MS_DVB_C,
    SATIPCFG_MS_NONE
} t_mod_sys;

/* DVB-* */
typedef enum
{
    SATIPCFG_MT_QPSK = 0,
    SATIPCFG_MT_8PSK,
    SATIPCFG_MT_16QAM,
    SATIPCFG_MT_32QAM,
    SATIPCFG_MT_64QAM,
    SATIPCFG_MT_128QAM,
    SATIPCFG_MT_256QAM,
    SATIPCFG_MT_NONE
} t_mod_type;

/* DVB-T, DVB-T2 only */
typedef enum
{
    SATIPCFG_GI_1_4 = 0,
    SATIPCFG_GI_19_256,
    SATIPCFG_GI_1_8,
    SATIPCFG_GI_19_128,
    SATIPCFG_GI_1_16,
    SATIPCFG_GI_1_32,
    SATIPCFG_GI_1_128,
    SATIPCFG_GI_NONE
} t_guard_interval;

/* DVB-T, DVB-T2 only */
typedef enum
{
    SATIPCFG_TM_1K = 0,
    SATIPCFG_TM_2K,
    SATIPCFG_TM_4K,
    SATIPCFG_TM_8K,
    SATIPCFG_TM_16K,
    SATIPCFG_TM_32K,
    SATIPCFG_TM_NONE
} t_trans_mode;

/* DVB-T, DVB-T2 only */
typedef enum
{
    SATIPCFG_BW_5MHZ = 0,
    SATIPCFG_BW_6MHZ,
    SATIPCFG_BW_7MHZ,
    SATIPCFG_BW_8MHZ,
    SATIPCFG_BW_10MHZ,
    SATIPCFG_BW_1_712MHZ,
    SATIPCFG_BW_NONE
} t_bandwidth;

typedef enum
{
    SATIPCFG_SM_SISO = 0,
    SATIPCFG_SM_MISO = 1,
} t_siso_miso;

typedef enum
{
    SATIPCFG_P_OFF = 0,
    SATIPCFG_P_ON,
    SATIPCFG_P_NONE
} t_pilots;

/* DVB-S, DVB-S2, DVB-T, DVB-T2 */
typedef enum
{
    SATIPCFG_F_12 = 0,
    SATIPCFG_F_23,
    SATIPCFG_F_34,
    SATIPCFG_F_56,
    SATIPCFG_F_78,
    SATIPCFG_F_89,
    SATIPCFG_F_35,
    SATIPCFG_F_45,
    SATIPCFG_F_910,
    SATIPCFG_F_NONE
} t_fec_inner;


typedef enum
{
    SATIPCFG_INCOMPLETE = 0, /* parameters are missing */
    SATIPCFG_PID_CHANGED,    /* only PIDs were touched, allows for "PLAY" with addpid/delpids */
    SATIPCFG_CHANGED,        /* requires new tuning */
    SATIPCFG_SETTLED         /* configuration did not change since last access */
} t_satip_config_status;

typedef enum
{
    SATIP_FE_TYPE_SAT = 0,
    SATIP_FE_TYPE_CAB = 1,
    SATIP_FE_TYPE_TER = 2,
} t_fe_type;

#define SATIPCFG_MAX_PIDS 64

typedef struct satip_config
{
    t_fe_type         fe_type;

    /* status info */
    t_satip_config_status status;
    unsigned long     param_cfg;

    /* current/new configuration */
    unsigned int      frequency;
    t_polarization    polarization;
    int               lnb_off;
    t_roll_off        roll_off;
    t_mod_sys         mod_sys;
    t_mod_type        mod_type;
    t_inversion       inversion;

    /* DVB-T, DVB-T2 */
    t_guard_interval  guard_interval;
    t_trans_mode      trans_mode;
    t_bandwidth       bandwidth;
    unsigned int      plp;
    unsigned short    t2id;
    t_siso_miso       sisomiso_mode;

    t_pilots          pilots;
    unsigned int      symbol_rate;
    t_fec_inner       fec_inner;
    int               position;
    int               frontend;
    unsigned short    pid[SATIPCFG_MAX_PIDS];

    /* delta info for addpids/delpids cmd */
    unsigned short    mod_pid[SATIPCFG_MAX_PIDS];

} t_satip_config;





#define SATIPCFG_OK       0
#define SATIPCFG_ERROR    1


t_satip_config* satip_new_config(int frontend, int type);

int satip_del_pid(t_satip_config* cfg,unsigned short pid);
int satip_add_pid(t_satip_config* cfg,unsigned short pid);
void satip_del_allpid(t_satip_config* cfg);

int satip_set_freq(t_satip_config* cfg,unsigned int freq);
int satip_set_polarization(t_satip_config* cfg,t_polarization pol);
int satip_lnb_off(t_satip_config* cfg);
int satip_set_rolloff(t_satip_config* cfg,t_roll_off rolloff);
int satip_set_inversion(t_satip_config* cfg,t_inversion inversion);
int satip_set_modsys(t_satip_config* cfg,t_mod_sys modsys);
int satip_set_modtype(t_satip_config* cfg,t_mod_type modtype);
int satip_set_guardinterval(t_satip_config* cfg,t_guard_interval guard_interval);
int satip_set_transmode(t_satip_config* cfg,t_trans_mode trans_mode);
int satip_set_bandwidth(t_satip_config* cfg,t_bandwidth bandwidth);
int satip_set_plp(t_satip_config* cfg,unsigned int plp);
int satip_set_t2id(t_satip_config* cfg,unsigned short t2id);
int satip_set_sisomosi_mode(t_satip_config* cfg,t_siso_miso sm);
int satip_set_pilots(t_satip_config* cfg,t_pilots pilots);
int satip_set_symbol_rate(t_satip_config* cfg,unsigned int symrate);
int satip_set_fecinner(t_satip_config* cfg, t_fec_inner fecinner);
int satip_set_position(t_satip_config* cfg, int position);


int satip_valid_config(t_satip_config* cfg);
int satip_tuning_required(t_satip_config* cfg);
int satip_pid_update_required(t_satip_config* cfg);

int satip_prepare_tuning(t_satip_config* cfg, char* str, int maxlen);
int satip_prepare_pids(t_satip_config* cfg, char* str, int maxlen,int modpid);

int satip_settle_config(t_satip_config* cfg);
void satip_clear_config(t_satip_config* cfg);

#endif

