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

#ifndef __TIMER_H__
#define __TIMER_H__

#include <list>
#include <string>
#include <time.h>

class timer_elem
{
private:
	std::string m_description;
	void (*m_handler) (void*);
	void* m_params;
	long m_interval;
	struct timespec m_ts;
	bool m_active;
	bool m_single;
public:
	timer_elem(void (*handler)(void*), void *params, const char* description)
		:m_description(description), m_handler(handler), m_params(params), m_active(false), m_single(false)
	{
	}
	~timer_elem()
	{
		stop();
	}
	void start(long msec, int single = false)
	{
		struct timespec cur_ts;
		clock_gettime(CLOCK_REALTIME,&cur_ts);
		m_interval = msec;
		m_active = true;
		m_single = single;

		m_ts.tv_sec =  cur_ts.tv_sec + m_interval/1000;
		m_ts.tv_nsec = cur_ts.tv_nsec + (m_interval % 1000) * 1000000;
		if (m_ts.tv_nsec > 1000000000)
		{
			m_ts.tv_sec += 1;
			m_ts.tv_nsec -= 1000000000;
		}
	}

	void stop()
	{
		m_active = false;
	}

	void call()
	{
		if (m_active)
		{
			struct timespec ts;
			clock_gettime(CLOCK_REALTIME,&ts);

			m_handler(m_params);

			if (m_single)
			{
				m_active = false;
			}
			else
			{
				m_ts.tv_sec =  ts.tv_sec + m_interval/1000;
				m_ts.tv_nsec = ts.tv_nsec + (m_interval % 1000) * 1000000;
				if (m_ts.tv_nsec > 1000000000)
				{
					m_ts.tv_sec += 1;
					m_ts.tv_nsec -= 1000000000;
				}
			}

		}
	}

	bool isActive() { return m_active; }
	const char* getDescription() { return m_description.c_str(); }
	time_t getTimeSpecSec() { return m_ts.tv_sec; }
	long getTimeSpecNsec() { return m_ts.tv_nsec; }
};

class satipTimer
{
	std::list<timer_elem*> m_timer_list;
public:
	satipTimer();
	~satipTimer();
	void dump();
	timer_elem* create(void (*handler)(void*), void* params, const char *description);
	void remove(timer_elem* timer);
	void callNextTimer();
	int getNextTimerBegin();
};

#endif // __TIMER_H__
