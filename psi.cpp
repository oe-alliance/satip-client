/*
 * satip: PSI (PAT/CAT/PMT) parsing to derive ECM/EMM pids
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
#include <stdint.h>

#include "psi.h"
#include "config.h"
#include "log.h"

static uint32_t crc32_table[256];
static bool crc32_table_ready = false;

static void crc32_init()
{
	if (crc32_table_ready)
		return;

	for (unsigned int i = 0; i < 256; i++)
	{
		uint32_t k = i << 24;
		for (int j = 0; j < 8; j++)
			k = (k & 0x80000000) ? ((k << 1) ^ 0x04c11db7) : (k << 1);

		crc32_table[i] = k;
	}

	crc32_table_ready = true;
}

/* MPEG-2 systems CRC32, a complete section (crc included) results in 0 */
static uint32_t crc32_calc(const unsigned char* data, int len)
{
	uint32_t crc = 0xffffffff;
	for (int i = 0; i < len; i++)
		crc = (crc << 8) ^ crc32_table[((crc >> 24) ^ data[i]) & 0xff];

	return crc;
}

template <class M> static void eraseUnlisted(M& container, const std::set<int>& keep)
{
	typename M::iterator it = container.begin();
	while (it != container.end())
	{
		if (keep.find(it->first) == keep.end())
			container.erase(it++);
		else
			++it;
	}
}

satipPSI::satipPSI(satipConfig* cfg, bool emm_enabled, const std::set<int>& caid_filter, int tuner_id):
	m_cfg(cfg),
	m_emm_enabled(emm_enabled),
	m_tuner_id(tuner_id),
	m_caid_filter(caid_filter),
	m_dirty(false),
	m_pat_version(-1),
	m_cat_version(-1),
	m_ts_packets(0),
	m_sections(0),
	m_crc_errors(0),
	m_last_stats(0),
	m_saw_data(false)
{
	crc32_init();
	pthread_mutex_init(&m_lock, NULL);

	INFO(MSG_CA, "[ca%d] ecm/emm pid detection enabled (emm : %s, caid filter : %s)\n",
		m_tuner_id,
		m_emm_enabled ? "on" : "off",
		m_caid_filter.empty() ? "none, all CA systems" : joinNumbers(m_caid_filter, true).c_str());

	reset();
}

satipPSI::~satipPSI()
{
	DEBUG(MSG_MAIN, "Destruct PSI.\n");
	pthread_mutex_destroy(&m_lock);
}

void satipPSI::clearState()
{
	m_watched_pids.clear();
	m_pat_version = -1;
	m_cat_version = -1;
	m_pat_sections.clear();
	m_cat_sections.clear();
	m_pat_pmt_pids.clear();
	m_emm_pids.clear();
	m_pmt.clear();
	m_assembler.clear();
	m_dirty = false;
}

void satipPSI::reset()
{
	pthread_mutex_lock(&m_lock);
	INFO(MSG_CA, "[ca%d] reset (retune), forgetting all tables.\n", m_tuner_id);
	clearState();
	m_joined_pids.clear();
	recompute();
	pthread_mutex_unlock(&m_lock);
}

void satipPSI::setJoinedPids(const std::set<int>& pids)
{
	pthread_mutex_lock(&m_lock);
	if (pids != m_joined_pids)
	{
		m_joined_pids = pids;
		DEBUG(MSG_CA, "[ca%d] kernel pid list : %s\n", m_tuner_id, joinNumbers(pids).c_str());
		recompute();
	}
	pthread_mutex_unlock(&m_lock);
}

bool satipPSI::caidAllowed(int caid)
{
	if (m_caid_filter.empty())
		return true;

	return m_caid_filter.find(caid) != m_caid_filter.end();
}

void satipPSI::processTS(const unsigned char* data, int len)
{
	pthread_mutex_lock(&m_lock);

	if (!m_saw_data)
	{
		m_saw_data = true;
		m_last_stats = time(NULL);
		INFO(MSG_CA, "[ca%d] ts tap active, watching pids %s\n",
			m_tuner_id, joinNumbers(m_watched_pids).c_str());
	}

	for (int off = 0; off + TS_PACKET_SIZE <= len; off += TS_PACKET_SIZE)
	{
		const unsigned char* p = data + off;

		if (p[0] != 0x47) /* not aligned, the rest of the buffer is useless */
			break;

		m_ts_packets++;

		int pid = ((p[1] & 0x1f) << 8) | p[2];

		if (m_watched_pids.find(pid) == m_watched_pids.end())
			continue;

		if (p[3] & 0xc0) /* scrambled, never a PSI section */
			continue;

		int afc = (p[3] >> 4) & 0x03;
		if (afc == 0 || afc == 2) /* no payload */
			continue;

		int pos = 4;
		if (afc == 3) /* skip the adaptation field */
		{
			pos += 1 + p[4];
			if (pos >= TS_PACKET_SIZE)
				continue;
		}

		if (p[1] & 0x40) /* payload_unit_start_indicator */
		{
			int ptr = p[pos];
			pos++;

			if (pos + ptr > TS_PACKET_SIZE)
				continue;

			if (ptr > 0) /* tail of the previous section */
				feed(pid, p + pos, ptr, false);

			pos += ptr;
			feed(pid, p + pos, TS_PACKET_SIZE - pos, true);
		}
		else
		{
			feed(pid, p + pos, TS_PACKET_SIZE - pos, false);
		}

		/* a PAT here can make a pmt pid watchable that this buffer still carries */
		if (m_dirty)
		{
			m_dirty = false;
			recompute();
		}
	}

	time_t now = time(NULL);
	if (now - m_last_stats >= 60)
	{
		m_last_stats = now;
		logHeartbeat();
	}

	pthread_mutex_unlock(&m_lock);
}

void satipPSI::feed(int pid, const unsigned char* buf, int len, bool allow_start)
{
	section_assembler& sa = m_assembler[pid];
	int pos = 0;

	while (pos < len)
	{
		if (sa.expected == 0) /* collecting the 3 byte section header */
		{
			if (sa.len == 0)
			{
				if (!allow_start) /* without a pointer field we cannot find the start */
					return;

				if (buf[pos] == 0xff) /* stuffing */
					return;
			}

			sa.data[sa.len++] = buf[pos++];

			if (sa.len < 3)
				continue;

			int section_length = ((sa.data[1] & 0x0f) << 8) | sa.data[2];

			/* 5 bytes of table header + 4 bytes crc */
			if (section_length < 9 || section_length + 3 > PSI_MAX_SECTION_LEN)
			{
				DEBUG(MSG_CA, "[ca%d] pid %d, bogus section_length %d, drop.\n",
					m_tuner_id, pid, section_length);
				sa.len = 0;
				return;
			}

			sa.expected = section_length + 3;
			continue;
		}

		int need = sa.expected - sa.len;
		int take = (len - pos < need) ? (len - pos) : need;

		memcpy(sa.data + sa.len, buf + pos, take);
		sa.len += take;
		pos += take;

		if (sa.len == sa.expected)
		{
			handleSection(pid, sa.data, sa.len);
			sa.len = 0;
			sa.expected = 0;
		}
	}
}

void satipPSI::handleSection(int pid, const unsigned char* sec, int len)
{
	if (!(sec[1] & 0x80)) /* section_syntax_indicator */
		return;

	if (crc32_calc(sec, len) != 0)
	{
		m_crc_errors++;
		DEBUG(MSG_CA, "[ca%d] pid %d, crc error, drop section (%llu so far).\n",
			m_tuner_id, pid, m_crc_errors);
		return;
	}

	m_sections++;

	if (!(sec[5] & 0x01)) /* current_next_indicator, not applicable yet */
		return;

	int table_id = sec[0];

	if (pid == PSI_PID_PAT)
	{
		if (table_id == 0x00)
			parsePAT(sec, len);
	}
	else if (pid == PSI_PID_CAT)
	{
		if (table_id == 0x01)
			parseCAT(sec, len);
	}
	else if (table_id == 0x02)
	{
		parsePMT(pid, sec, len);
	}
}

void satipPSI::parsePAT(const unsigned char* sec, int len)
{
	int version = (sec[5] >> 1) & 0x1f;
	int section_number = sec[6];

	if (version != m_pat_version)
	{
		INFO(MSG_CA, "[ca%d] PAT version %d -> %d\n", m_tuner_id, m_pat_version, version);
		m_pat_sections.clear();
		m_pat_version = version;
	}

	std::set<int> pmt_pids;
	for (int i = 8; i + 4 <= len - 4; i += 4)
	{
		int program_number = (sec[i] << 8) | sec[i + 1];
		int pmt_pid = ((sec[i + 2] & 0x1f) << 8) | sec[i + 3];

		if (program_number == 0) /* network information table */
			continue;

		if (pmt_pid == 0 || pmt_pid >= PSI_PID_NULL)
			continue;

		DEBUG(MSG_CA, "[ca%d] PAT: program %d -> pmt pid %d\n", m_tuner_id, program_number, pmt_pid);
		pmt_pids.insert(pmt_pid);
	}

	m_pat_sections[section_number] = pmt_pids;

	std::set<int> all;
	for (std::map<int, std::set<int> >::iterator it = m_pat_sections.begin(); it != m_pat_sections.end(); ++it)
		all.insert(it->second.begin(), it->second.end());

	if (all != m_pat_pmt_pids)
	{
		m_pat_pmt_pids = all;
		INFO(MSG_CA, "[ca%d] PAT: %d services, pmt pids %s\n",
			m_tuner_id, (int)all.size(), joinNumbers(all).c_str());

		eraseUnlisted(m_pmt, m_pat_pmt_pids);
		m_dirty = true;
	}
}

void satipPSI::parseCAT(const unsigned char* sec, int len)
{
	int version = (sec[5] >> 1) & 0x1f;
	int section_number = sec[6];

	if (version != m_cat_version)
	{
		INFO(MSG_CA, "[ca%d] CAT version %d -> %d\n", m_tuner_id, m_cat_version, version);
		m_cat_sections.clear();
		m_cat_version = version;
	}

	std::set<int> emm_pids;
	collectCaDescriptors(sec + 8, (len - 4) - 8, emm_pids, "EMM");

	m_cat_sections[section_number] = emm_pids;

	std::set<int> all;
	for (std::map<int, std::set<int> >::iterator it = m_cat_sections.begin(); it != m_cat_sections.end(); ++it)
		all.insert(it->second.begin(), it->second.end());

	if (all != m_emm_pids)
	{
		m_emm_pids = all;
		INFO(MSG_CA, "[ca%d] CAT: emm pids %s\n",
			m_tuner_id, all.empty() ? "none" : joinNumbers(all).c_str());
		m_dirty = true;
	}
}

void satipPSI::parsePMT(int pid, const unsigned char* sec, int len)
{
	int program_number = (sec[3] << 8) | sec[4];
	int version = (sec[5] >> 1) & 0x1f;

	pmt_info& info = m_pmt[pid];
	if (info.version == version)
		return;

	int end = len - 4; /* without crc */
	if (end < 12)
		return;

	int program_info_length = ((sec[10] & 0x0f) << 8) | sec[11];
	int pos = 12;

	if (pos + program_info_length > end)
	{
		DEBUG(MSG_CA, "[ca%d] pmt pid %d, bogus program_info_length %d\n",
			m_tuner_id, pid, program_info_length);
		return;
	}

	DEBUG(MSG_CA, "[ca%d] PMT pid %d: program %d, version %d -> %d\n",
		m_tuner_id, pid, program_number, info.version, version);

	std::set<int> ecm_pids;
	std::set<int> es_pids;

	/* program level */
	collectCaDescriptors(sec + pos, program_info_length, ecm_pids, "ECM");
	pos += program_info_length;

	/* elementary stream level */
	while (pos + 5 <= end)
	{
		int es_pid = ((sec[pos + 1] & 0x1f) << 8) | sec[pos + 2];
		if (es_pid > 0 && es_pid < PSI_PID_NULL)
			es_pids.insert(es_pid);

		int es_info_length = ((sec[pos + 3] & 0x0f) << 8) | sec[pos + 4];
		pos += 5;

		if (pos + es_info_length > end)
			break;

		collectCaDescriptors(sec + pos, es_info_length, ecm_pids, "ECM");
		pos += es_info_length;
	}

	info.version = version;
	info.es_pids = es_pids;

	if (info.ecm_pids != ecm_pids)
	{
		info.ecm_pids = ecm_pids;
		INFO(MSG_CA, "[ca%d] PMT pid %d (program %d): ecm pids %s\n",
			m_tuner_id, pid, program_number,
			ecm_pids.empty() ? "none (free to air, or filtered by ca_caids)" : joinNumbers(ecm_pids).c_str());
		m_dirty = true;
	}
}

void satipPSI::collectCaDescriptors(const unsigned char* p, int len, std::set<int>& out, const char* what)
{
	int pos = 0;

	while (pos + 2 <= len)
	{
		int tag = p[pos];
		int dlen = p[pos + 1];

		if (pos + 2 + dlen > len)
			break;

		if (tag == 0x09 && dlen >= 4) /* CA_descriptor */
		{
			int caid = (p[pos + 2] << 8) | p[pos + 3];
			int ca_pid = ((p[pos + 4] & 0x1f) << 8) | p[pos + 5];

			if (ca_pid > 0 && ca_pid < PSI_PID_NULL)
			{
				bool allowed = caidAllowed(caid);
				DEBUG(MSG_CA, "[ca%d] %s pid %d, caid 0x%04x : %s\n", m_tuner_id, what, ca_pid, caid,
					allowed ? "accepted" : "rejected by ca_caids");

				if (allowed)
					out.insert(ca_pid);
			}
		}

		pos += 2 + dlen;
	}
}

void satipPSI::recompute()
{
	std::set<int> derived;
	std::set<int> watched;
	std::set<int> active;

	derived.insert(PSI_PID_PAT); /* needed to find the pmt pids in the first place */
	watched.insert(PSI_PID_PAT);

	if (m_emm_enabled)
	{
		derived.insert(PSI_PID_CAT);
		watched.insert(PSI_PID_CAT);
		derived.insert(m_emm_pids.begin(), m_emm_pids.end());
	}

	/*
	 * A service is active when the kernel joined one of its elementary streams.
	 * The pmt pid alone does not qualify: a service scan opens a section filter
	 * on every pmt of the transponder, which would otherwise pull in the ecm
	 * pids of every service at once.
	 */
	for (std::set<int>::iterator it = m_pat_pmt_pids.begin(); it != m_pat_pmt_pids.end(); ++it)
	{
		int pmt_pid = *it;
		bool is_active = false;

		std::map<int, pmt_info>::iterator pmt = m_pmt.find(pmt_pid);
		if (pmt != m_pmt.end())
		{
			for (std::set<int>::iterator es = pmt->second.es_pids.begin(); es != pmt->second.es_pids.end(); ++es)
			{
				if (m_joined_pids.find(*es) != m_joined_pids.end())
				{
					is_active = true;
					break;
				}
			}
		}

		/* watch every known pmt, its sections are what tells us the es pids */
		watched.insert(pmt_pid);

		if (!is_active)
			continue;

		active.insert(pmt_pid);
		derived.insert(pmt_pid);

		if (pmt != m_pmt.end())
			derived.insert(pmt->second.ecm_pids.begin(), pmt->second.ecm_pids.end());
	}

	if ((int)derived.size() > PSI_MAX_DERIVED_PIDS)
	{
		WARN(MSG_CA, "[ca%d] %d ca pids, truncating to %d. Consider using ca_caids.\n",
			m_tuner_id, (int)derived.size(), PSI_MAX_DERIVED_PIDS);

		std::set<int> truncated;
		int n = 0;
		for (std::set<int>::iterator it = derived.begin(); it != derived.end() && n < PSI_MAX_DERIVED_PIDS; ++it, n++)
			truncated.insert(*it);

		derived.swap(truncated);
	}

	eraseUnlisted(m_assembler, watched);
	m_watched_pids = watched;

	if (derived == m_derived_pids)
		return;

	m_derived_pids = derived;

	INFO(MSG_CA, "[ca%d] active services (pmt pids) : %s\n",
		m_tuner_id, active.empty() ? "none yet" : joinNumbers(active).c_str());
	INFO(MSG_CA, "[ca%d] ---> ca pid set is now : %s\n",
		m_tuner_id, joinNumbers(derived).c_str());

	if (m_cfg)
		m_cfg->setCaPids(derived);
}

/* so that a log without any ca pids can be told apart from a dead tap */
void satipPSI::logHeartbeat()
{
	INFO(MSG_CA, "[ca%d] heartbeat: %llu ts packets, %llu sections, %llu crc errors, "
		"pat %s, %d pmts known, watching %s, ca pids %s\n",
		m_tuner_id, m_ts_packets, m_sections, m_crc_errors,
		m_pat_version < 0 ? "not received" : "ok",
		(int)m_pmt.size(),
		joinNumbers(m_watched_pids).c_str(),
		joinNumbers(m_derived_pids).c_str());
}
