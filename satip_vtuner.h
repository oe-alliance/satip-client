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

#ifndef _SATIP_VTUNER_H
#define _SATIP_VTUNER_H


struct satip_vtuner;

struct satip_vtuner* satip_vtuner_new(char* devname,struct satip_config* satip_cfg);
int satip_vtuner_fd(struct satip_vtuner* vt);

void satip_vtuner_event(struct satip_vtuner* vt);


#endif
