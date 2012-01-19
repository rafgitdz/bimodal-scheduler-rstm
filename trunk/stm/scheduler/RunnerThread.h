/*
 * The thread that runs a single transaction at a given time
 * 
 */

#ifndef __STM_RUNNER_THREAD__
#define __STM_RUNNER_THREAD__

#include <string>
#include <pthread.h>
#include "Queue.h"
#include <iostream>

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

			// queue lock
			pthread_mutex_t m_queueLock;

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
			void *addJob(void *(*pFunc)(void*), void *pArgs, ThreadData* pThreadData);

			// Moves the job that currently runs to the given core
			void moveJob(RunnerThread *otherThread);
			
			// Moves the job that currently runs to the scheduler ro Queue
			void moveJobToROQueue();
			
			// Shutdown transactions running on this thread
			void shutdown();
			
			inline long getCurrentEpoch() {return m_currJob->getEpoch(); }
			inline bool isTxRO() { return m_currJob->isTxRO();}
			inline void setTxRO(bool value) { m_currJob->setTxRO(value); }
			inline time_t getTxTimestamp() {return m_currJob->getTxTimestamp();}
			inline void setTxTimestamp(time_t stamp) {return m_currJob->setTxTimestamp(stamp);}
			
			
			const int& getJobsNum() { return m_queue->size(); }

		};
	}
}



#endif //__STM_RUNNER_THREAD__
