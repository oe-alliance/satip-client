/*
 * satip: session manager
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

#ifndef __MAMAGER_H__
#define __MAMAGER_H__

#include <stdlib.h>
#include <pthread.h>

#include <map>
#include <list>

#include "option.h"
#include "session.h"
#include "manager.h"
#include "log.h"

const int max_adapters = 8;

class sessionManager
{
	std::list<Session*> m_sessions;
	optParser m_satip_opt;

	int satipSessionCreate(const char* ipaddr, int fe_type, const char *port, vtunerOpt* settings);
	void addSession(Session* session) { m_sessions.push_back(session); }

public:
	sessionManager();
	virtual ~sessionManager();

	void satipStart();
	void sessionStart();
	void sessionJoin();
	void sessionStop();

	static sessionManager* getInstance() 
	{
		static sessionManager instance;
		return &instance;
	}
};

#endif //__MAMAGER_H__

