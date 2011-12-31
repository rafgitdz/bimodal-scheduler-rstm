#ifndef __BIMODAL_CM_H__
#define __BIMODAL_CM_H__

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
			
				time_t getTimestamp() { return m_timestamp; }
				bool isReadOnly() { return m_isRO; }

				virtual bool ShouldAbort(ContentionManager* enemy)
				{
					std::cout << "Conflict \n";
					BiModalCM *b = dynamic_cast<BiModalCM*>(enemy);
					
					if (!b->isReadOnly() && !m_isRO)
						return (b->getTimestamp() < m_timestamp);
					
					if (m_isRO) {
						std::cout << "i am  RO, aborting the enemy \n";
						return false;
					}
					
					std::cout << "i am aborted by my RO enemy \n";
					return true;
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
