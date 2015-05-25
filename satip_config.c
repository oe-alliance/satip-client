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
#include "satip_config.h"
#include "log.h"

/* configuration items present */
#define  PC_FREQ      0x00000001
#define  PC_POL       0x00000002
#define  PC_ROLLOFF   0x00000004
#define  PC_MODSYS    0x00000008
#define  PC_MODTYPE   0x00000010
#define  PC_PILOTS    0x00000020
#define  PC_SYMRATE   0x00000040
#define  PC_FECINNER  0x00000080
#define  PC_POSITION  0x00000100
#define  PC_GUARDINT  0x00000200
#define  PC_TRANSMODE 0x00000400
#define  PC_BANDWIDTH 0x00000800
#define  PC_INVERSION 0x00001000
#define  PC_PLP       0x00002000
#define  PC_T2ID      0x00004000
#define  PC_SM        0x00008000

#define  PC_COMPLETE_DVBS   \
  ( PC_FREQ | PC_POL | PC_MODSYS | PC_SYMRATE | \
    PC_FECINNER | PC_POSITION )

#define  PC_COMPLETE_DVBS2  \
  ( PC_FREQ | PC_POL | PC_ROLLOFF | PC_MODSYS | \
    PC_MODTYPE | PC_PILOTS | PC_SYMRATE | \
    PC_FECINNER | PC_POSITION )

#define  PC_COMPLETE_DVBT   \
   ( PC_FREQ | PC_MODSYS | PC_MODTYPE | PC_FECINNER | \
    PC_GUARDINT | PC_TRANSMODE | PC_BANDWIDTH )

#define  PC_COMPLETE_DVBT2   \
   ( PC_FREQ | PC_MODSYS | PC_MODTYPE | PC_FECINNER | \
    PC_GUARDINT | PC_TRANSMODE | PC_BANDWIDTH | PC_PLP )

#define  PC_COMPLETE_DVBC   \
   ( PC_FREQ | PC_MODSYS | PC_MODTYPE | PC_SYMRATE )

/* PID handling */
#define PID_VALID  0
#define PID_IGNORE 1
#define PID_ADD    2
#define PID_DELETE 3

extern int enabled_workarounds;

/* strings for query strings */
char  const strmap_polarization[] = { 'h', 'v', 'l', 'r' };
char* const strmap_fecinner[] = { "12","23","34","56","78","89","35","45","910" };
char* const strmap_rolloff[] = { "0.35","0.25","0.20" };
char* const strmap_modtype[] = { "qpsk", "8psk", "16qam", "32qam", "64qam", "128qam", "256qam"};
char* const strmap_guartinterval[] = {"14", "19256", "18", "19128", "116", "132", "1128"};
char* const strmap_transmode[] = {"1k", "2k", "4k", "8k", "16k", "32k"};
char* const strmap_bandwidth[] = {"5", "6", "7", "8", "10", "1.712"};

t_satip_config* satip_new_config(int frontend, int type)
{
    t_satip_config* cfg;

    cfg=(t_satip_config*) malloc(sizeof(t_satip_config));

    cfg->frontend = frontend;
    cfg->fe_type = type;

    satip_clear_config(cfg);

    return cfg;
}

/*
 * PIDs need extra handling to cover "addpids" and "delpids" use cases
 */

static void pidupdate_status(t_satip_config* cfg)
{
    int i;
    int mod_found=0;

    for (i=0; i<SATIPCFG_MAX_PIDS; i++)
        if ( cfg->mod_pid[i] == PID_ADD ||
                cfg->mod_pid[i] == PID_DELETE )
        {
            mod_found=1;
            break;
        }

    switch (cfg->status)
    {
    case SATIPCFG_SETTLED:
        if (mod_found)
            cfg->status = SATIPCFG_PID_CHANGED;
        break;

    case SATIPCFG_PID_CHANGED:
        if (!mod_found)
            cfg->status = SATIPCFG_SETTLED;
        break;

    case SATIPCFG_CHANGED:
    case SATIPCFG_INCOMPLETE:
        break;
    }
}

void satip_del_allpid(t_satip_config* cfg)
{
    int i;

    for ( i=0; i<SATIPCFG_MAX_PIDS; i++ )
        satip_del_pid(cfg, cfg->pid[i]);
}


int satip_del_pid(t_satip_config* cfg,unsigned short pid)
{
    int i;

    for (i=0; i<SATIPCFG_MAX_PIDS; i++)
    {
        if ( cfg->pid[i] == pid )
            switch (cfg->mod_pid[i])
            {
            case PID_VALID: /* mark it for deletion */
                cfg->mod_pid[i] = PID_DELETE;
                pidupdate_status(cfg);
                return SATIPCFG_OK;

            case PID_ADD:   /* pid shall be added, ignore it */
                cfg->mod_pid[i] = PID_IGNORE;
                pidupdate_status(cfg);
                return SATIPCFG_OK;

            case PID_IGNORE:
                break;

            case PID_DELETE: /* pid shall already be deleted*/
                return SATIPCFG_OK;
            }
    }

    /* pid was not found, ignore request */
    return SATIPCFG_OK;
}



int satip_add_pid(t_satip_config* cfg,unsigned short pid)
{
    int i;

    /* check if pid is present and valid, to be added, to be deleted */
    for (i=0; i<SATIPCFG_MAX_PIDS; i++)
    {
        if ( cfg->pid[i] == pid )
            switch (cfg->mod_pid[i])
            {
            case PID_VALID: /* already present */
            case PID_ADD:   /* pid shall be already added */
                /* just return current status, no update required */
                return SATIPCFG_OK;

            case PID_IGNORE:
                break;

            case PID_DELETE:
                /* pid shall be deleted, make it valid again */
                cfg->mod_pid[i] = PID_VALID;
                pidupdate_status(cfg);
                return SATIPCFG_OK;
            }
    }

    /* pid was not found, add it */
    for ( i=0; i<SATIPCFG_MAX_PIDS; i++)
    {
        if (cfg->mod_pid[i] == PID_IGNORE )
        {
            cfg->mod_pid[i] = PID_ADD;
            cfg->pid[i] = pid;
            pidupdate_status(cfg);
            return SATIPCFG_OK;
        }
    }

    /* could not add it */
    return SATIPCFG_ERROR;
}

static void  param_update_status(t_satip_config* cfg)
{
    if ( cfg->fe_type == SATIP_FE_TYPE_SAT )
    {
        if (
            ((cfg->mod_sys == SATIPCFG_MS_DVB_S)  && ((cfg->param_cfg & PC_COMPLETE_DVBS ) == PC_COMPLETE_DVBS  )) ||
            ((cfg->mod_sys == SATIPCFG_MS_DVB_S2) && ((cfg->param_cfg & PC_COMPLETE_DVBS2) == PC_COMPLETE_DVBS2 ))
        )
            cfg->status = SATIPCFG_CHANGED;
        else
            cfg->status = SATIPCFG_INCOMPLETE;
    }
    else if ( cfg->fe_type == SATIP_FE_TYPE_TER )
    {
        if (
            ((cfg->mod_sys == SATIPCFG_MS_DVB_T)  && ((cfg->param_cfg & PC_COMPLETE_DVBT ) == PC_COMPLETE_DVBT  )) ||
            ((cfg->mod_sys == SATIPCFG_MS_DVB_T2) && ((cfg->param_cfg & PC_COMPLETE_DVBT2) == PC_COMPLETE_DVBT2 ))
        )
            cfg->status = SATIPCFG_CHANGED;
        else
            cfg->status = SATIPCFG_INCOMPLETE;
    }
    else if ( cfg->fe_type == SATIP_FE_TYPE_CAB )
    {
        if (
            (cfg->mod_sys == SATIPCFG_MS_DVB_C) && ((cfg->param_cfg & PC_COMPLETE_DVBC) == PC_COMPLETE_DVBC )
        )
            cfg->status = SATIPCFG_CHANGED;
        else
            cfg->status = SATIPCFG_INCOMPLETE;
    }
    else
    {
        cfg->status = SATIPCFG_INCOMPLETE;
    }

    DEBUG(MSG_MAIN,"Tuning parameter status: %08lx -> config status: %d\n",cfg->param_cfg, cfg->status);
}

int satip_set_freq(t_satip_config* cfg,unsigned int freq)
{
    if ( (cfg->param_cfg & PC_FREQ) &&  cfg->frequency == freq )
        return SATIPCFG_OK;

    cfg->param_cfg |= PC_FREQ;
    cfg->frequency = freq;

    param_update_status(cfg);
    return SATIPCFG_OK;
}

int satip_set_polarization(t_satip_config* cfg,t_polarization pol)
{
    if ( (cfg->param_cfg & PC_POL) &&  cfg->polarization == pol )
        return SATIPCFG_OK;

    cfg->param_cfg |= PC_POL;
    cfg->polarization = pol;

    /*  polarization shall only trigger tuning if it was powered down explicitly */
    if ( cfg->lnb_off==1 )
    {
        cfg->lnb_off = 0;
        param_update_status(cfg);
    }

    return SATIPCFG_OK;
}


int satip_lnb_off(t_satip_config* cfg)
{
    cfg->lnb_off = 1;
    cfg->param_cfg &= ~PC_POL;

    cfg->status = SATIPCFG_INCOMPLETE;

    return SATIPCFG_OK;
}


int satip_set_rolloff(t_satip_config* cfg,t_roll_off rolloff)
{
    if ( (cfg->param_cfg & PC_ROLLOFF) &&  cfg->roll_off == rolloff )
        return SATIPCFG_OK;

    cfg->param_cfg |= PC_ROLLOFF;
    cfg->roll_off = rolloff;

    param_update_status(cfg);
    return SATIPCFG_OK;
}

int satip_set_inversion(t_satip_config* cfg,t_inversion inversion)
{
    if ( (cfg->param_cfg & PC_INVERSION) &&  cfg->inversion == inversion )
        return SATIPCFG_OK;

    cfg->param_cfg |= PC_INVERSION;
    cfg->inversion = inversion;

    param_update_status(cfg);
    return SATIPCFG_OK;
}

int satip_set_modsys(t_satip_config* cfg,t_mod_sys modsys)
{
    if ( (cfg->param_cfg & PC_MODSYS) &&  cfg->mod_sys == modsys )
        return SATIPCFG_OK;

    cfg->param_cfg |= PC_MODSYS;
    cfg->mod_sys = modsys;

    param_update_status(cfg);
    return SATIPCFG_OK;
}

int satip_set_modtype(t_satip_config* cfg,t_mod_type modtype)
{
    if ( (cfg->param_cfg & PC_MODTYPE) &&  cfg->mod_type == modtype )
        return SATIPCFG_OK;

    cfg->param_cfg |= PC_MODTYPE;
    cfg->mod_type = modtype;

    param_update_status(cfg);
    return SATIPCFG_OK;
}

int satip_set_guardinterval(t_satip_config* cfg,t_guard_interval guard_interval)
{
    if ( (cfg->param_cfg & PC_GUARDINT) &&  cfg->guard_interval == guard_interval )
        return SATIPCFG_OK;

    cfg->param_cfg |= PC_GUARDINT;
    cfg->guard_interval = guard_interval;

    param_update_status(cfg);
    return SATIPCFG_OK;
}

int satip_set_transmode(t_satip_config* cfg,t_trans_mode trans_mode)
{
    if ( (cfg->param_cfg & PC_TRANSMODE) &&  cfg->trans_mode == trans_mode )
        return SATIPCFG_OK;

    cfg->param_cfg |= PC_TRANSMODE;
    cfg->trans_mode = trans_mode;

    param_update_status(cfg);
    return SATIPCFG_OK;
}

int satip_set_bandwidth(t_satip_config* cfg,t_bandwidth bandwidth)
{
    if ( (cfg->param_cfg & PC_BANDWIDTH) &&  cfg->bandwidth == bandwidth )
        return SATIPCFG_OK;

    cfg->param_cfg |= PC_BANDWIDTH;
    cfg->bandwidth = bandwidth;

    param_update_status(cfg);
    return SATIPCFG_OK;
}

int satip_set_plp(t_satip_config* cfg,unsigned int plp)
{
    if ( (cfg->param_cfg & PC_PLP) &&  cfg->plp == plp )
        return SATIPCFG_OK;

    cfg->param_cfg |= PC_PLP;
    cfg->plp = plp;

    param_update_status(cfg);
    return SATIPCFG_OK;
}

/* unused */
int satip_set_t2id(t_satip_config* cfg,unsigned short t2id)
{
    if ( (cfg->param_cfg & PC_T2ID) &&  cfg->t2id == t2id )
        return SATIPCFG_OK;

    cfg->param_cfg |= PC_T2ID;
    cfg->t2id = t2id;

    param_update_status(cfg);
    return SATIPCFG_OK;
}

/* unused */
int satip_set_sisomosi_mode(t_satip_config* cfg,t_siso_miso sm)
{
    if ( (cfg->param_cfg & PC_SM) &&  cfg->sisomiso_mode == sm )
        return SATIPCFG_OK;

    cfg->param_cfg |= PC_SM;
    cfg->sisomiso_mode = sm;

    param_update_status(cfg);
    return SATIPCFG_OK;
}

int satip_set_pilots(t_satip_config* cfg,t_pilots pilots)
{
    if ( (cfg->param_cfg & PC_PILOTS) &&  cfg->pilots == pilots )
        return SATIPCFG_OK;

    cfg->param_cfg |= PC_PILOTS;
    cfg->pilots = pilots;

    param_update_status(cfg);
    return SATIPCFG_OK;
}

int satip_set_symbol_rate(t_satip_config* cfg,unsigned int symrate)
{
    if ( (cfg->param_cfg & PC_SYMRATE) &&  cfg->symbol_rate == symrate )
        return SATIPCFG_OK;

    cfg->param_cfg |= PC_SYMRATE;
    cfg->symbol_rate = symrate;

    param_update_status(cfg);
    return SATIPCFG_OK;
}


int satip_set_fecinner(t_satip_config* cfg, t_fec_inner fecinner)
{
    if ( (cfg->param_cfg & PC_FECINNER) &&  cfg->fec_inner == fecinner )
        return SATIPCFG_OK;

    cfg->param_cfg |= PC_FECINNER;
    cfg->fec_inner = fecinner;

    param_update_status(cfg);
    return SATIPCFG_OK;
}

int satip_set_position(t_satip_config* cfg, int position)
{
    if ( (cfg->param_cfg & PC_POSITION) &&  cfg->position == position )
        return SATIPCFG_OK;

    cfg->param_cfg |= PC_POSITION;
    cfg->position = position;

    param_update_status(cfg);
    return SATIPCFG_OK;
}


int satip_valid_config(t_satip_config* cfg)
{
    return ( cfg->status != SATIPCFG_INCOMPLETE );
}


int satip_tuning_required(t_satip_config* cfg)
{
    return ( cfg->status == SATIPCFG_CHANGED );
}

int satip_pid_update_required(t_satip_config* cfg)
{
    return ( cfg->status == SATIPCFG_PID_CHANGED );
}


static int setpidlist(t_satip_config* cfg, char* str,int maxlen,const char* firststr,int modtype1, int modtype2)
{
    int i;
    int printed=0;
    int first=1;

    for ( i=0; i<SATIPCFG_MAX_PIDS; i++ )
        if ( cfg->mod_pid[i] == modtype1 ||
                cfg->mod_pid[i] == modtype2 )
        {
            printed += snprintf(str+printed, maxlen-printed, "%s%d",
                                first ? firststr : ",",
                                cfg->pid[i]);
            first=0;

            if ( printed>=maxlen )
                return printed;
        }

    return printed;
}


int satip_prepare_tuning(t_satip_config* cfg, char* str, int maxlen)
{
    int printed;
    char frontend_str[7]="";

    /* optional: specific frontend */
    if ( cfg->frontend > 0 && cfg->frontend<100)
        sprintf(frontend_str, "fe=%d&", cfg->frontend);

    if ((cfg->mod_sys == SATIPCFG_MS_DVB_S) || (cfg->mod_sys == SATIPCFG_MS_DVB_S2))
    {
        /* DVB-S mandatory parameters */
        printed = snprintf(str, maxlen,
                           "src=%d&%sfreq=%d.%d&pol=%c&msys=%s&sr=%d&fec=%s",
                           cfg->position,
                           frontend_str,
                           cfg->frequency/10, cfg->frequency%10,
                           strmap_polarization[cfg->polarization],
                           cfg->mod_sys == SATIPCFG_MS_DVB_S ? "dvbs"  : "dvbs2",
                           cfg->symbol_rate,
                           strmap_fecinner[cfg->fec_inner]);

        if ( printed>=maxlen )
            return printed;
        str += printed;

        /* DVB-S2 additional required parameters */
        if ( cfg->mod_sys == SATIPCFG_MS_DVB_S2 )
        {
            printed += snprintf(str, maxlen-printed, "&ro=%s&mtype=%s&plts=%s",
                                strmap_rolloff[cfg->roll_off],
                                strmap_modtype[cfg->mod_type],
                                cfg->pilots == SATIPCFG_P_OFF ? "off" : "on" );
        }
    }

    else if ((cfg->mod_sys == SATIPCFG_MS_DVB_T) || (cfg->mod_sys == SATIPCFG_MS_DVB_T2))
    {
        /* DVB-T mandatory parameters */
        printed = snprintf(str, maxlen,
                           "%sfreq=%d.%d&bw=%s&msys=%s&tmode=%s&mtype=%s&gi=%s&fec=%s",
                           frontend_str,
                           cfg->frequency/1000000, cfg->frequency%1000000,
                           strmap_bandwidth[cfg->bandwidth],
                           cfg->mod_sys == SATIPCFG_MS_DVB_T ? "dvbt"  : "dvbt2",
                           strmap_transmode[cfg->trans_mode],
                           strmap_modtype[cfg->mod_type],
                           strmap_guartinterval[cfg->guard_interval],
                           strmap_fecinner[cfg->fec_inner]);

        if ( printed>=maxlen )
            return printed;
        str += printed;

        /* DVB-T2 additional required parameters */
        if ( cfg->mod_sys == SATIPCFG_MS_DVB_T2 )
        {
            printed += snprintf(str, maxlen-printed, "&plp=%d",
                                cfg->plp);
/* do not know how to handle these parameters as there is no counterpart in the DVB API.
            printed += snprintf(str, maxlen-printed, "&plp=%d&t2id=%d&sm=%d",
                                cfg->plp,
                                cfg->t2id,
                                cfg->sisomiso_mode);
*/
        }
    }

    else if ((cfg->mod_sys == SATIPCFG_MS_DVB_C))
    {
        /* DVB-C mandatory parameters */
        printed = snprintf(str, maxlen,
                           "%sfreq=%d.%d&msys=%s&mtype=%s&sr=%d",
                           frontend_str,
                           cfg->frequency/1000000, cfg->frequency%1000000,
                           cfg->mod_sys == SATIPCFG_MS_DVB_C ? "dvbc"  : "dvbc2",
                           strmap_modtype[cfg->mod_type],
                           cfg->symbol_rate);

        if ( printed>=maxlen )
            return printed;
        str += printed;

        /* DVB-C additional optional parameters */
        if ( cfg->inversion != SATIPCFG_I_NONE )
        {
            printed += snprintf(str, maxlen-printed, "&specinv=%s",
                                cfg->inversion == SATIPCFG_I_ON ? "1" : "0");
        }
    }

    /* don´t forget to check on caller ! */
    return printed;
}


int satip_prepare_pids(t_satip_config* cfg, char* str, int maxlen,int modpid)
{
    int printed;

    if (modpid)
    {
        printed = setpidlist(cfg,str,maxlen,"addpids=",PID_ADD, PID_ADD);

        if ( printed>=maxlen )
            return printed;

        printed += setpidlist(cfg, str+printed,maxlen-printed,
                              printed>0 ? "&delpids=" : "delpids=",PID_DELETE, PID_DELETE);
    }
    else
    {
        // Add workaround for Fritzdvbc quirk: add dummy pid
        if (enabled_workarounds & 0x01)
            printed = setpidlist(cfg,str,maxlen,"pids=100,",PID_VALID, PID_ADD);
        else
            printed = setpidlist(cfg,str,maxlen,"pids=",PID_VALID, PID_ADD);
    }

    /* nothing was added, use "none" */
    if ( printed == 0 )
    {
        printed = snprintf(str,maxlen,"pids=none");
    }

    /* don´t forget to check on caller */
    return printed;
}

int satip_settle_config(t_satip_config* cfg)
{
    int i;
    int retval=SATIPCFG_OK;


    switch (cfg->status)
    {
    case SATIPCFG_CHANGED:
    case SATIPCFG_PID_CHANGED:
        /* clear up addpids delpids */
        for ( i=0; i<SATIPCFG_MAX_PIDS; i++)
            if ( cfg->mod_pid[i] == PID_ADD )
                cfg->mod_pid[i] = PID_VALID;
            else if (cfg->mod_pid[i] == PID_DELETE )
                cfg->mod_pid[i] = PID_IGNORE;
        /* now settled */
        cfg->status = SATIPCFG_SETTLED;
        break;

    case SATIPCFG_SETTLED:
        break;

    case SATIPCFG_INCOMPLETE: /* cannot settle this.. */
    default:
        retval=SATIPCFG_ERROR;
        break;
    }

    return retval;
}


void satip_clear_config(t_satip_config* cfg)
{
    int i;

    cfg->status    = SATIPCFG_INCOMPLETE;
    cfg->param_cfg = 0;
    cfg->lnb_off = 0;

    for ( i=0; i<SATIPCFG_MAX_PIDS; i++)
        cfg->mod_pid[i]=PID_IGNORE;
}
