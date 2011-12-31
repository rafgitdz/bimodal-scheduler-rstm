/*
 * The data that every original worker thread will need to hold
 * 
 */

#ifndef __STM_THREAD_DATA__
#define __STM_THREAD_DATA__

#include <pthread.h>

namespace stm
{
	namespace scheduler
	{
		class ThreadData
		{
		private:
			pthread_mutex_t m_lock;
			pthread_cond_t m_condVar;

		public:
			// A default c'tor that will initialize the lock and the cond var
			ThreadData();

			// The d'tor to free the lock and the cond var
			~ThreadData();

			// Retrieves the lock
			pthread_mutex_t* getLock();

			// Retrieves the cond var
			pthread_cond_t* getCondVar();
		};

		class ThreadDataManager
		{
			private:
				pthread_key_t m_threadKey;

			public:
				ThreadDataManager()
				{
		            pthread_key_create(&m_threadKey, threadDataDestroy);
				}

				ThreadData* getThreadData() const
				{
					return reinterpret_cast<ThreadData*>(pthread_getspecific(m_threadKey));
				}

				void setThreadData(ThreadData* pThreadData)
				{
					pthread_setspecific(m_threadKey, reinterpret_cast<void*>(pThreadData));
				}

				static void threadDataDestroy(void *td)
				{
					ThreadData *threadData = (ThreadData *)td;
					delete threadData;
				}


		};

		// Declare the thread data manager
		extern ThreadDataManager threadDataManager;
	}
}

#endif //__STM_THREAD_DATA__
