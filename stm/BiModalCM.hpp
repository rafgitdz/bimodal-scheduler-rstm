#ifndef __BIMODAL_CM_HPP__
#define __BIMODAL_CM_HPP__

#include <sched.h>
#include <iostream>
#include "ContentionManager.h"
#include "BiModalScheduler.h"
#include "scheduler_common.h"


namespace stm {
	namespace cm {
		// The BiModal contention manager
		class BiModalCM : public ContentionManager
		{
			private:
				// the id of the core where the transaction is excecuted
				int m_iCore;
				int m_epoch;
				
				// true if we have to reschedule in another queue, false otherwise
				bool m_reschedule;
				
				bool m_newTx;
				
				time_t getTimestamp() { return stm::scheduler::BiModalScheduler::instance()->getTxTimestamp(m_iCore); }
				void setTimestamp(time_t stamp) { stm::scheduler::BiModalScheduler::instance()->setTxTimestamp(m_iCore, stamp); }
				bool isReadOnly() { return stm::scheduler::BiModalScheduler::instance()->isTxRO(m_iCore); }
				void setReadOnly(bool value) { stm::scheduler::BiModalScheduler::instance()->setTxRO(m_iCore, value); }
				long getEpoch() {return m_epoch;}
				
			public:
				
				BiModalCM() : m_iCore(sched_getcpu()), 
							  m_reschedule(false), m_newTx(true) {}
				
				~BiModalCM(){}
				
				
				
				/*
				 * When a transaction begins, we update its core and epoch
				 */
				virtual void OnBeginTransaction() 
				{
					if (m_newTx) {
						m_newTx = false;
						/*
						 * If this is the first time this transaction begins, we 
						 * initialize its timestamp, and set the transaction as read only
						 */
						if (getTimestamp()==NULL) {
							setReadOnly(true);
							struct timeval t;
							gettimeofday(&t, NULL);
							setTimestamp(t.tv_sec);
						}
					}
					m_epoch = stm::scheduler::BiModalScheduler::instance()->getCurrentEpoch(m_iCore);
				}
				
				bool ShouldAbort(ContentionManager *enemy) 
				{
					stm::scheduler::BiModalScheduler::instance()->increaseConflictCounter();
					std::cout << "conflict\n";
					BiModalCM* b = dynamic_cast<BiModalCM*>(enemy);

					/*
					 * If two transactions with different epoch ids have a conflict
					 * the transaction with the bigger epoch number is aborted
					 */
					  
					if (getEpoch() != b->getEpoch()) {
						/*
						 * in this case, the aborted transaction should restart on the same core
						 * where it executed before
						 */
						
						if (getEpoch() < b->getEpoch()) {
							m_reschedule = false;
							return true;
						} else {
							b->m_reschedule = false;
							return false;
						}
					}
					
					m_reschedule = true;
					b->m_reschedule = true;
					if (IS_READING(getEpoch()))
						stm::scheduler::BiModalScheduler::instance()->increaseFalsePositiveCounter();
					/*
					 * If two writing transactions have a conflict, the transaction
					 * with the bigger (i.e. younger) timestamp is aborted
					 */
					if (!isReadOnly() && !b->isReadOnly())
						return (b->getTimestamp() < getTimestamp());
						
					/*
					 * If we are in a Reading epoch, the writing transaction is aborted
					 */
					if (IS_READING(getEpoch()))
						return !isReadOnly();
						
						
					/*
					 * If we are in a Writing epoch, the read-only transaction is aborted
					 */
					 return isReadOnly();
						
					
				}
				
				/*
				 *  When i am on conflict with another transaction (and i lost), i go into the RO queue
				 *  if i am a read-only transaction, and i reschedule myself after 
				 *  the other transaction otherwise
				 */
				virtual void onConflictWith(int iCore) {

					if (m_reschedule) {
						std::cout << "onConflictWith\n";
						if (isReadOnly())
							stm::scheduler::BiModalScheduler::instance()->moveJobToROQueue(m_iCore);
						else
							stm::scheduler::BiModalScheduler::instance()->reschedule(m_iCore,iCore);
					}
				}
				
				virtual void OnOpenWrite()
				{
					if (isReadOnly())
						setReadOnly(false);
				}
				
				virtual void OnTransactionCommitted() {
					m_newTx = true;
				}
				
		};
		
	}
}


#endif
