/*
 * The BiModal Scheduler
 * this class is a singleton , his purpose is to reschedule aborted 
 * transactions, whether in the RO Queue, or in the queue of the core 
 * where the winning transaction is excecuted. It also stores the current
 * epoch number and is responsible for the epoch changing
 * 
 */ 

#include "RunnerThread.h"
#include "ThreadLock.h"
#include "Queue.h"

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
			friend class RunnerThread;
			// Holds the number of cores in the system
			static long m_lngCoresNum;
			// An array of threads that are used, each thread for a core
			RunnerThread **m_arThreads;
			


			// Initializes the threads that are responsible to do activate the transaction-function
			void initExecutingThreads();
			
			// The number of the current epoch
			long* m_epoch;
			int* m_roQueueCount;
			
			// The Queue where the read-only transactions will be stored
			Queue* m_roQueue;
			
		public:
			// Returns the number of cores that are on the machine
			long getCoresNum();
		
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
			
			/*
			 * Schedules the job that currently runs on the iFromCore in the RO queue
			 */ 
			void moveJobToROQueue(InnerJob *job);
			
			void moveJobToROQueue(int iFromCore) { m_arThreads[iFromCore]->moveJobToROQueue();}
			

			InnerJob* roQueueDeque();
			
			long getCurrentEpoch(int iCore);
			
			bool allQueuesEmpty();
	};
		
	}
}
