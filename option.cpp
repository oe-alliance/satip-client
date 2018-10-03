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

#include <fstream>
#include <sstream>
#include <stdlib.h>

#include "option.h"
#include "log.h"

const char* conf_name = "/etc/vtuner.conf";

optParser::optParser()
{
	load();
	dump();
	DEBUG(MSG_MAIN, "%d Satip configuration loaded..\n", m_settings.size());
}

optParser::~optParser()
{

}

void optParser::dump()
{
	std::map<int, vtunerOpt>::iterator it;
	for (it = m_settings.begin(); it!=m_settings.end(); it++)
	{
		DEBUG(MSG_MAIN, "[%d] tuner type : %s, ip : %s, fe_type : %d\n", it->first, it->second.m_vtuner_type.c_str(), it->second.m_ipaddr.c_str(), it->second.m_fe_type);
	}
}

std::vector<std::string> optParser::split(const std::string &line, char delim)
{
	std::vector<std::string> elems;
	std::stringstream ss(line);
	std::string item;
	while(std::getline(ss, item, delim))
	{
		elems.push_back(item);
	}
	return elems;
}

void optParser::load()
{
	std::ifstream in_file(conf_name);
	if (!in_file.is_open())
	{
		return;
	}

	std::string line;
	int index = 0;
	std::vector<std::string> data;
	std::map<std::string, int> tuner_type_table;

	tuner_type_table["DVB-S"] = 0;
	tuner_type_table["DVB-C"] = 1;
	tuner_type_table["DVB-T"] = 2;

	while(std::getline(in_file, line))
	{
//		DEBUG(MSG_MAIN, "%s\n", line.c_str());

		data = split(line, '=');
		if (data.size() != 2)
			continue;

		index = atoi(data[0].c_str());

		if (m_settings.find(index) == m_settings.end())
		{
			vtunerOpt vt_set;
			m_settings[index] = vt_set;
		}

//		DEBUG(MSG_MAIN, "index : %d\n", index);

		data = split(data[1], ',');

		for (unsigned int i=0 ; i < data.size() ; i++)
		{
			std::vector<std::string> attr = split(data[i], ':');
			if (attr[0] == "vtuner_type")
				m_settings[index].m_vtuner_type = attr[1];

			else if (attr[0] == "ipaddr")
				m_settings[index].m_ipaddr = attr[1];

			else if (attr[0] == "tuner_type")
				m_settings[index].m_fe_type = tuner_type_table[attr[1]];

                        else if (attr[0] == "force_plts" && attr[1] == "1")
                                m_settings[index].m_force_plts = true;
		}
	}
}

bool optParser::isEmpty()
{
	return !m_settings.size();
}

