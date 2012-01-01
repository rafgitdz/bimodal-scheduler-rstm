#ifndef __BIMODAL_CM_HPP__
#define __BIMODAL_CM_HPP__

#include <sched.h>

#include "ContentionManager.h"
#include "scheduler/BiModalScheduler.h"


namespace stm {
	namespace cm {
		// The BiModal contention manager
		class BiModalCM : public ContentionManager
		{
			private:
				time_t m_timestamp;
				bool m_isRO;
				int m_iCore;
				
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
					
				}
				
				bool ShouldAbort(ContentionManager *enemy) 
				{
					BiModalCM* b = dynamic_cast<BiModalCM*>(enemy);
					
					return b->getTimestamp() < m_timestamp;
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
