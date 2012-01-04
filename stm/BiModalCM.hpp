#ifndef __BIMODAL_CM_HPP__
#define __BIMODAL_CM_HPP__

#include <sched.h>

#include "ContentionManager.h"
#include "BiModalScheduler.h"


namespace stm {
	namespace cm {
		// The BiModal contention manager
		class BiModalCM : public ContentionManager
		{
			private:
				time_t m_timestamp;
				bool m_isRO;
				int m_iCore;
				long m_epochNum;
				
			public:
				
				BiModalCM() : m_isRO(true), m_iCore(sched_getcpu()) {};
				
				~BiModalCM(){};
				
				time_t getTimestamp() { return m_timestamp; }
				bool isReadOnly() { return m_isRO; }
				
				virtual void OnBeginTransaction() 
				{
					struct timeval t;

					gettimeofday(&t, NULL);
					m_timestamp = t.tv_sec;
					m_epochNum = stm::scheduler::BiModalScheduler::instance()->getCurrentEpoch(m_iCore);
				}
				
				bool ShouldAbort(ContentionManager *enemy) 
				{
					BiModalCM* b = dynamic_cast<BiModalCM*>(enemy);
					
					if (m_epochNum != b->m_epochNum)
						return m_epochNum > b->m_epochNum;
					
					if (!m_isRO && !b->isReadOnly()) {
						if (b->getTimestamp() < m_timestamp) {
							stm::scheduler::BiModalScheduler::instance()->reschedule(m_iCore, b->m_iCore);
							return true;
						}
						else
							return false;
					}
					
					if (m_isRO)
						if (m_epochNum % 2 == 0) {
							stm::scheduler::BiModalScheduler::instance()->moveJobToROQueue(m_iCore);
							return true;
						}
						else
							return false;
					else
						// my enemy is read-only
						if (m_epochNum % 2 == 0)
							return false;
						else {
							stm::scheduler::BiModalScheduler::instance()->reschedule(m_iCore, b->m_iCore);
							return true;
						}
					
				}
				
				virtual void OnOpenWrite()
				{
					if (m_isRO)
						m_isRO = false;
				}
		};
		
	}
}


#endif
