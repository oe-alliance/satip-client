/*
 * satip: timer functions
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

#include "timer.h"
#include "log.h"

satipTimer::satipTimer()
{
}

satipTimer::~satipTimer()
{
	for(std::list<timer_elem*>::iterator it = m_timer_list.begin(); it != m_timer_list.end(); ++it)
	{
		delete (*it);
	}
	m_timer_list.clear();
}

void satipTimer::dump()
{
	struct timespec cur_ts;
	clock_gettime(CLOCK_REALTIME,&cur_ts);

	std::list<timer_elem*>::iterator it = m_timer_list.begin();
	for(;it != m_timer_list.end(); ++it)
	{
		int msec = ((*it)->getTimeSpecSec() - cur_ts.tv_sec) * 1000 + ((*it)->getTimeSpecNsec() - cur_ts.tv_nsec) / (1000 * 1000) + 1;
		DEBUG(MSG_MAIN, "TIMER DUMP : %d (%s)\n", msec, (*it)->isActive() ? "active" : "inactive");
	}
}

timer_elem* satipTimer::create(void (*handler)(void*), void* params, const char *description)
{
	DEBUG(MSG_MAIN, "satipTimer::create timer %s \n", description);
	dump();	

	timer_elem *timer = new timer_elem(handler, params, description);
	m_timer_list.push_back(timer);
	dump();
	DEBUG(MSG_MAIN, "satipTimer::create end.\n");
	return timer;
}

void satipTimer::remove(timer_elem* timer)
{
	DEBUG(MSG_MAIN, "satipTimer::remove %s\n", timer->getDescription());
	dump();
	m_timer_list.remove(timer);
	delete timer;
	dump();
	DEBUG(MSG_MAIN, "satipTimer::remove end.\n");
}

void satipTimer::callNextTimer()
{
	struct timespec cur_ts;
	clock_gettime(CLOCK_REALTIME,&cur_ts);

	for(std::list<timer_elem*>::iterator it = m_timer_list.begin() ;it != m_timer_list.end(); ++it)
	{
		if ((*it)->isActive())
		{
			if ((cur_ts.tv_sec > (*it)->getTimeSpecSec()) || ((cur_ts.tv_sec == (*it)->getTimeSpecSec()) && cur_ts.tv_nsec > (*it)->getTimeSpecNsec()))
			{
				DEBUG(MSG_MAIN, "satipTimer::callNextTimer run %s\n", (*it)->getDescription());
				(*it)->call();
			}
		}
	}
}

int satipTimer::getNextTimerBegin()
{
	int msec = 1000;
//	int msec = -1;

	if (!m_timer_list.empty())
	{
		timer_elem* min_timer_elem = NULL;
		std::list<timer_elem*>::iterator it = m_timer_list.begin();

		for(;it != m_timer_list.end(); ++it)
		{
			if ((*it)->isActive())
			{
				if ( (min_timer_elem == NULL) || 
					((min_timer_elem->getTimeSpecSec() > (*it)->getTimeSpecSec()) || 
					((min_timer_elem->getTimeSpecSec() == (*it)->getTimeSpecSec()) &&
					(min_timer_elem->getTimeSpecNsec() > (*it)->getTimeSpecNsec()))))
				{
					min_timer_elem = (*it);
				}
			}	
		}

		if (min_timer_elem != NULL)
		{
			struct timespec cur_ts;
			clock_gettime(CLOCK_REALTIME,&cur_ts);

			msec = (min_timer_elem->getTimeSpecSec() - cur_ts.tv_sec) * 1000 + (min_timer_elem->getTimeSpecNsec() - cur_ts.tv_nsec) / (1000000) + 1;

			if (msec < 0)
				msec = 0;

//			DEBUG(MSG_MAIN, "satipTimer::getNextTimerBegin msec : %d (%s)\n", msec, min_timer_elem->getDescription());
		}
	}

//	if (msec == -1)
//		DEBUG(MSG_MAIN, "satipTimer::getNextTimerBegin msec : %d\n", msec);

	return msec;
}

