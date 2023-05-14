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

#include <stdio.h>
#include <string.h>
#include <list>
#include <string>

#include "manager.h"
#include "session.h"
#include "option.h"
#include "log.h"

const char* default_port = "554";

sessionManager::sessionManager()
{
	DEBUG(MSG_MAIN,"Create resource manager.\n");
}

sessionManager::~sessionManager()
{
	DEBUG(MSG_MAIN,"Destruct resource manager.\n");
	if (m_sessions.size())
	{
		for (std::list<Session*>::iterator it = m_sessions.begin(); it != m_sessions.end(); ++it)
		{
			Session* session = *it;
			delete session;
		}
	}
}

int sessionManager::satipStart()
{
	if (m_satip_opt.isEmpty())
		return -1;

	std::map<int, vtunerOpt> *data = m_satip_opt.getData();
	for (std::map<int, vtunerOpt>::iterator it(data->begin()); it!=data->end(); it++)
	{
		if (it->second.isAvailable())
		{
			DEBUG(MSG_MAIN, "try connect : [%d] type : %s, ip : %s, fe_type : %d\n", it->first, it->second.m_vtuner_type.c_str(), it->second.m_ipaddr.c_str(), it->second.m_fe_type);
			satipSessionCreate(it->second.m_ipaddr.c_str(), it->second.m_fe_type, it->second.m_port.c_str(), &(it->second));
		}
	}

	return 0;
}

int sessionManager::satipSessionCreate(const char* ipaddr, int fe_type, const char *port, vtunerOpt* settings)
{
	int ok = 0;

	Session* session;
	session = new satipSession( ipaddr, (port[0] == 0) ? default_port: port , fe_type, settings, ok);
	if (!ok) 
	{
		DEBUG(MSG_MAIN, "Session init failed!\n");
		delete session;
		return -1;
	}

	addSession(session);

	DEBUG(MSG_MAIN, "Create satip session ok (%s , %d)\n", ipaddr, fe_type);

	session->start();

	DEBUG(MSG_MAIN, "Satip session start (%s , %d)\n", ipaddr, fe_type);

	return 0;
}

void sessionManager::sessionStart()
{
	DEBUG(MSG_MAIN, "sessionManager::sessionStart, %d\n", m_sessions.size());

	if (m_sessions.size())
	{
		for (std::list<Session*>::iterator it = m_sessions.begin(); it != m_sessions.end(); ++it)
		{
			Session* session = *it;
			session->start();
		}
	}
}

void sessionManager::sessionJoin()
{
	if (m_sessions.size())
	{
		for (std::list<Session*>::iterator it = m_sessions.begin(); it != m_sessions.end(); ++it)
		{
			Session* session = *it;
			session->join();
		}
	}
}

void sessionManager::sessionStop()
{
	if (m_sessions.size())
	{
		for (std::list<Session*>::iterator it = m_sessions.begin(); it != m_sessions.end(); ++it)
		{
			Session* session = *it;
			session->stop();
		}
	}
}
 


