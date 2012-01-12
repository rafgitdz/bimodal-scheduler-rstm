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
				// timestamp to resolve conflicts between two writing transactions
				time_t m_timestamp;
				// true if the transaction only reads shared objects, false otherwise
				bool m_isRO;
				// the id of the core where the transaction is excecuted
				int m_iCore;
				// the number of the epoch when the transaction started
				long m_epochNum;
				
			public:
				
				BiModalCM() : m_isRO(true) {
					struct timeval t;

					gettimeofday(&t, NULL);
					m_timestamp = t.tv_sec;
					};
				
				~BiModalCM(){};
				
				time_t getTimestamp() { return m_timestamp; }
				bool isReadOnly() { return m_isRO; }
				
				
				/*
				 * When a transaction begins, we update its core and epoch
				 */
				virtual void OnBeginTransaction() 
				{
					m_iCore = sched_getcpu();
					m_epochNum = stm::scheduler::BiModalScheduler::instance()->getCurrentEpoch(m_iCore);
				}
				
				bool ShouldAbort(ContentionManager *enemy) 
				{
					BiModalCM* b = dynamic_cast<BiModalCM*>(enemy);
					
					/*
					 * If two transactions with different epoch ids have a conflict
					 * the transaction with the bigger epoch number is aborted
					 */
					if (m_epochNum != b->m_epochNum)
						return m_epochNum < b->m_epochNum;
					
					/*
					 * If two writing transactions have a conflict, the transaction
					 * with the bigger (i.e. younger) timestamp is aborted
					 */
					if (!m_isRO && !b->isReadOnly())
						return (b->getTimestamp() < m_timestamp)
						
					/*
					 * If we are in a Reading epoch, the writing transaction is aborted
					 */
					if (epochNum % 2 == 1)
						return !m_isRo;
						
						
					/*
					 * If we are in a Writing epoch, the read-only transaction is aborted
					 */
					 return m_isRo;
						
					
				}
				
				/*
				 *  When i am on conflict with another transaction (and i lost), i go into the RO queue
				 *  if i am a read-only transaction, and i reschedule myself after 
				 *  the other transaction otherwise
				 */
				virtual void onConflictWith(int iCore) {
					if (m_isRO)
						stm::scheduler::BiModalScheduler::instance()->moveJobToROQueue(m_iCore);
					else
						stm::scheduler::BiModalScheduler::instance()->reschedule(m_iCore,iCore);
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
