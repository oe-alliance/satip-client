/*
 * satip: option
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

#ifndef __OPTION_H__
#define __OPTION_H__

#include <string>
#include <map>
#include <vector>

bool modelCheck();

class vtunerOpt
{
public:
	std::string m_vtuner_type;
	std::string m_ipaddr;
	int m_fe_type;

	vtunerOpt():m_fe_type(-1)
	{
	}

	bool isAvailable()
	{
		if (m_vtuner_type != "satip_client" || m_fe_type == -1 || m_ipaddr.empty())
			return false;

		return true;
	}
};

class optParser
{
	std::map<int, vtunerOpt> m_settings;
	void dump();
	std::vector<std::string> split(const std::string &line, char delim);
	void load();

public:
	optParser();
	virtual ~optParser();
	bool isEmpty();
	std::map<int, vtunerOpt>* getData() { return &m_settings; }
	
};

#endif //__OPTION_H__

