#include "BiModalCM.h" 

#include <sched.h>

using namespace stm;

cm::BiModalCM::BiModalCM() : m_iCore(sched_getcpu()), m_isRO(true) {
}

void cm::BiModalCM::OnBeginTransaction() {
	struct timeval t;

	gettimeofday(&t, NULL);
	m_timestamp = t.tv_sec;
}
