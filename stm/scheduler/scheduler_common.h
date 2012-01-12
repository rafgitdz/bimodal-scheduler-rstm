#ifndef __SCHEDULER_COMMON_H__
#define __SCHEDULER_COMMON_H__ 


#define IS_READING(epoch) epoch%2
#define IS_WRITING(epoch) !(epoch%2)

namespace stm {
	namespace scheduler {
		/*
		 * A class that will be thrown when a rescheduling needs to occur
		 */
		class RescheduleException {
		};
	}
}

#endif //__SCHEDULER_COMMON_H__ 
 
