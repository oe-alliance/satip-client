/*
 * satip: PSI (PAT/CAT/PMT) parsing to derive ECM/EMM pids
 *
 * A CI(+) CAM never opens demux filters of its own, so over SAT>IP the ECM/EMM
 * pids have to be requested explicitly or the CAM cannot descramble. This
 * parser taps the incoming TS and reports the ca pids of the active services
 * back to satipConfig. See README.md for the details.
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

#ifndef __PSI_H__
#define __PSI_H__

#include <map>
#include <set>
#include <time.h>
#include <pthread.h>

class satipConfig;

#define TS_PACKET_SIZE			188
#define PSI_MAX_SECTION_LEN		4096
#define PSI_MAX_DERIVED_PIDS	32

#define PSI_PID_PAT				0x0000
#define PSI_PID_CAT				0x0001
#define PSI_PID_NULL			0x1fff

class satipPSI
{
public:
	satipPSI(satipConfig* cfg, bool emm_enabled, const std::set<int>& caid_filter, int tuner_id);
	virtual ~satipPSI();

	/* retune: forget everything we learned */
	void reset();

	/* kernel (vtuner) pid list changed - called from the session thread */
	void setJoinedPids(const std::set<int>& pids);

	/* TS data from the SAT>IP server - called from the RTP thread */
	void processTS(const unsigned char* data, int len);

private:
	struct section_assembler
	{
		unsigned char data[PSI_MAX_SECTION_LEN];
		int len;		/* bytes collected so far */
		int expected;	/* total section size (3 + section_length), 0 = idle */

		section_assembler(): len(0), expected(0) {}
	};

	struct pmt_info
	{
		int version;
		std::set<int> ecm_pids;
		std::set<int> es_pids;

		pmt_info(): version(-1) {}
	};

	satipConfig* m_cfg;
	bool m_emm_enabled;
	int m_tuner_id;
	std::set<int> m_caid_filter;

	pthread_mutex_t m_lock;
	bool m_dirty;

	std::set<int> m_joined_pids;	/* pids requested by the kernel/enigma2 */
	std::set<int> m_watched_pids;	/* pids we run the section parser on */
	std::set<int> m_derived_pids;	/* what we last reported to satipConfig */

	int m_pat_version;
	int m_cat_version;
	std::map<int, std::set<int> > m_pat_sections;	/* section_number -> pmt pids */
	std::map<int, std::set<int> > m_cat_sections;	/* section_number -> emm pids */
	std::set<int> m_pat_pmt_pids;
	std::set<int> m_emm_pids;
	std::map<int, pmt_info> m_pmt;					/* pmt pid -> ecm pids */
	std::map<int, section_assembler> m_assembler;

	unsigned long long m_ts_packets;
	unsigned long long m_sections;
	unsigned long long m_crc_errors;
	time_t m_last_stats;
	bool m_saw_data;

	/* all of these expect m_lock to be held */
	void clearState();
	void feed(int pid, const unsigned char* buf, int len, bool allow_start);
	void handleSection(int pid, const unsigned char* sec, int len);
	void parsePAT(const unsigned char* sec, int len);
	void parseCAT(const unsigned char* sec, int len);
	void parsePMT(int pid, const unsigned char* sec, int len);
	void collectCaDescriptors(const unsigned char* p, int len, std::set<int>& out, const char* what);
	bool caidAllowed(int caid);
	void recompute();
	void logHeartbeat();
};

#endif // __PSI_H__
