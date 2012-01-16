#include "RunnerThread.h"

#include <cstdlib>
#include <algorithm>
#include <unistd.h>
#include <iostream>

#include "scheduler_common.h"
#include "BiModalScheduler.h"
#include "rstm.h" /* for stm::init - initializing stm threads */
#include "atomic_ops.h"

using namespace std;
using namespace stm::scheduler;

RunnerThread::RunnerThread(const int iCpuID) 
	: m_iCoreID(iCpuID), m_blnShouldShutdown(false)
{
	// Initialize the thread queue
	m_queue = new Queue();
	m_currJob = NULL;

	// Initialize the lock and condition var
	pthread_mutex_init(&m_queueLock, NULL);
}

RunnerThread::~RunnerThread()
{
	pthread_mutex_destroy(&m_queueLock);
	delete m_queue;
}

// The thread function declaration
void *threadFunc(void *pArgs);

void RunnerThread::run()
{
	// start the thread
	if (pthread_create(&m_thread, NULL, &threadFunc, (void *)this) != 0)
	{
		//throw 
		exit(1);
	}
}

void thread_cleanup(void *pArgs)
{
	stm::shutdown();
}

// The thread function
void *threadFunc(void *pArgs)
{

	pthread_cleanup_push(thread_cleanup, pArgs);

	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS,NULL);
	RunnerThread* pNewThread = (RunnerThread*)pArgs;

	pNewThread->threadStart();

	pthread_cleanup_pop(0);

	pthread_exit(NULL);
}

void RunnerThread::threadStart()
{

	// Set thread affinity
	setAffinity(m_iCoreID);

	// Introduce the thread to the stm
	stm::init("Bimodal", "vis-eager", false);

	doJobs();
}

void RunnerThread::setAffinity(int iCpuID)
{
	cpu_set_t cpuMask;
	unsigned int lLen = 0;
	
	CPU_ZERO(&cpuMask);
	CPU_SET(iCpuID, &cpuMask);
	lLen = sizeof(cpuMask);
	sched_setaffinity(0, lLen, &cpuMask); // Set the affinity of the current process
	cout << "Affinity was set for cpu " << iCpuID << endl;
}

void RunnerThread::doJobs()
{
	while (1)
	{
		// Waiting for a job
		while (!m_currJob) {
			long epoch = *BiModalScheduler::instance()->m_epoch;
			if (IS_READING(epoch)) {
				/*
				 * If we are in a reading epoch, we have to take a job in the ro queue
				 */
				int count = *BiModalScheduler::instance()->m_roQueueCount;
				if (count > 1) {
					if (bool_cas((volatile long unsigned int*)BiModalScheduler::instance()->m_roQueueCount,count, count - 1)) {
						m_currJob = BiModalScheduler::instance()->roQueueDeque();
						m_currJob->setEpoch(epoch);
					} else continue;
				}
				else
					if (bool_cas((volatile long unsigned int*)BiModalScheduler::instance()->m_roQueueCount,1,0)) {
						m_currJob = BiModalScheduler::instance()->roQueueDeque();
						m_currJob->setEpoch(epoch);
						// If this is the last job to take in the ro queue, we change the epoch
						bool_cas((volatile long unsigned int*)BiModalScheduler::instance()->m_epoch, epoch, epoch +1);
					} else continue;
			} else {
				/*
				 * If we are in a writing epoch we first check if we have to go to a reading epoch
				 */
				if (BiModalScheduler::instance()->m_roQueue->size() >= BiModalScheduler::instance()->getCoresNum()
					|| BiModalScheduler::instance()->allQueuesEmpty()) {
						
					if (BiModalScheduler::instance()->m_roQueue->size() != 0)
						if (bool_cas((volatile long unsigned int*)BiModalScheduler::instance()->m_epoch, epoch, epoch +1))
						// we set the number of transactions to take from the ro queue
							*BiModalScheduler::instance()->m_roQueueCount = 
								min(BiModalScheduler::instance()->getCoresNum(), (long)BiModalScheduler::instance()->m_roQueue->size());
							
							
					continue;
				} else {
					// if we are in a writing epoch and have a job, we execute it
					if (!m_queue->empty()) {
						pthread_mutex_lock(&m_queueLock);
						m_currJob = m_queue->front();
						m_currJob->setEpoch(epoch);
			
						m_queue->pop(); // Remove the job from the queue
						pthread_mutex_unlock(&m_queueLock);
					}
				}
			}
				
		}
		try
		{
			// Execute the job
			//cout << "executing job" << endl;
			m_currJob->execute();
		}
		catch (RescheduleException) // If a rescheduling has happened just move on to the next job
		{
		}
		m_currJob = NULL;
	}
}

void *RunnerThread::addJob(void *(*pFunc)(void*), void *pArgs, ThreadData* pThreadData)
{
	InnerJob* newJob = new InnerJob(pFunc, pArgs, pThreadData);

	// Add the job to the queue
	pthread_mutex_lock(&m_queueLock);
	m_queue->push(newJob);
	pthread_mutex_unlock(&m_queueLock);

	// wait for the job to end
	void* result = newJob->waitForFinish();
	delete newJob;
	return result;
}

void RunnerThread::moveJob(InnerJob *jobMoved)
{
	// Just add the job to the current queue (there is already a thread that waits for it's end)
	pthread_mutex_lock(&m_queueLock);
	m_queue->pushFront(jobMoved);
	pthread_mutex_unlock(&m_queueLock);
}

//void RunnerThread::moveJob(RunnerThread::RunnerThread *otherThread)
void RunnerThread::moveJob(RunnerThread *otherThread)
{
	otherThread->moveJob(m_currJob);
}

void RunnerThread::shutdown()
{
	pthread_cancel(m_thread);
}

void RunnerThread::moveJobToROQueue() {
	BiModalScheduler::instance()->moveJobToROQueue(m_currJob);
}

