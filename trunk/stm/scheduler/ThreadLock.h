/*
 * A lock that can be used to synchronize threads
 * 
 */

#ifndef __STM_THREAD_LOCK__
#define __STM_THREAD_LOCK__

#include <pthread.h>

namespace stm
{
	namespace scheduler
	{
		class ThreadLock
		{
		private:
			// The actual lock
			pthread_mutex_t m_lock;

		public:
			// A default c'tor that will initialize the lock
			ThreadLock();

			// The d'tor to free the lock
			~ThreadLock();

			// Activate the lock - after the lock any other thread that will try to lock will be blocked
			void Lock();

			// Unlock
			void Unlock();
		};
	}
}

#endif //__STM_THREAD_LOCK__
