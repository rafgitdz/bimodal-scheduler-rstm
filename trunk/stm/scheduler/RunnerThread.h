/*
 * The thread that runs a single transaction at a given time
 * 
 */

#ifndef __STM_RUNNER_THREAD__
#define __STM_RUNNER_THREAD__

#include <string>
#include <pthread.h>
#include "Queue.h"

namespace stm
{
	namespace scheduler
	{
		// The runner thread is a thread that runs on a single cpu and runs transactions given to it in a queue
		class RunnerThread
		{
		private:

			// Holds the core number this thread associates to
			int m_iCoreID;

			// The thread itself
			pthread_t m_thread;

			// Queue
			Queue* m_queue;

			// condition variable
			pthread_mutex_t m_queueLock;
			pthread_cond_t m_condQueueNotEmpty;

			// The job that runs in the thread info, lets to compare against other jobs
			void *m_currJobInfo;

			InnerJob *m_currJob;

			bool m_blnShouldShutdown;
			/*
			 * Sets the cpu/core affinity that current process will use.
			 */
			void setAffinity(int iCpuID);

			/*
			 * Waits for jobs to be done and then processes them
			 */
			void doJobs();

			/* Moves a given job to the current queue (just adds it to the queue) */
			void moveJob(InnerJob *jobMoved);

		public:

			RunnerThread(const int iCpuID);

			// D'tor
			~RunnerThread();

			void run();

			// The method that will be called when a thread starts
			void threadStart();

			// Adds an external job (transaction) that the thread needs to perform
			void *addJob(void *(*pFunc)(void*), void *pArgs, void *pJobInfo, ThreadData* pThreadData);

			void *getJobInfo();

			// Moves the job that currently runs to the given core
			void moveJob(RunnerThread *otherThread);
			
			// Moves the job that currently runs to the scheduler ro Queue
			void moveJobToROQueue();
			
			// Shutdown transactions running on this thread
			void shutdown();
			
			long getCurrentEpoch() { return m_currJob->getEpochNum(); }

		};
	}
}



#endif //__STM_RUNNER_THREAD__
