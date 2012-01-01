/*
 * The BiModal Scheduler
 * 
 */ 

#include "RunnerThread.h"
#include "ThreadLock.h"

namespace stm {
	namespace scheduler {
	
	class BiModalScheduler {
		// Members and methods to keep this class a singleton
		private:
			static ThreadLock* m_threadLock;
			static BiModalScheduler* m_Instance;
			BiModalScheduler();
			~BiModalScheduler();
		public:
			static void init();
			static BiModalScheduler* instance();
			static void shutdown();
			
			// Members and methods related to the scheduling
		private:

			// Holds the number of cores in the system
			static long m_lngCoresNum;
			// An array of threads that are used, each thread for a core
			RunnerThread **m_arThreads;
			
			// Returns the number of cores that are on the machine
			long getCoresNum();

			// Initializes the threads that are responsible to do activate the transaction-function
			void initExecutingThreads();

		public:
			/*
			 * Schedules the transaction thread that calls it.
			 * Upon return, the thread affinity is set, and the thread
			 * is allowed to run.
			 */
			void *schedule(void *(*pFunc)(void*), void *pArgs, void *pJobInfo);

			/* 
			 * Reschedules the job that currently runs on the iFromCore to the iToCore.
			 */
			void reschedule(int iFromCore, int iToCore);
	};
		
	}
}
