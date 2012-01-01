#include "RunnerThread.h"

#include <cstdlib>
#include <unistd.h>
#include <iostream>

#include "RescheduleException.h"

#include "rstm.h" /* for stm::init - initializing stm threads */

using namespace std;
using namespace stm::scheduler;

RunnerThread::RunnerThread(const int iCpuID) 
	: m_iCoreID(iCpuID), m_currJobInfo(NULL), m_blnShouldShutdown(false)
{
	// Initialize the thread queue
	m_queue = new Queue();

	// Initialize the lock and condition var
	pthread_mutex_init(&m_queueLock, NULL);
	pthread_cond_init(&m_condQueueNotEmpty, NULL);
}

RunnerThread::~RunnerThread()
{
	pthread_mutex_destroy(&m_queueLock);
	pthread_cond_destroy(&m_condQueueNotEmpty);
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
	stm::init("BiModal", "vis-eager", false, m_iCoreID);

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
		// Wait for a job
		pthread_mutex_lock(&m_queueLock);
		while (m_queue->empty())
		{
			pthread_cond_wait(&m_condQueueNotEmpty, &m_queueLock);
		}
		m_currJob = m_queue->front();
		m_currJobInfo = m_currJob->getJobInfo();
//		cout << "processing a job" << jobToExecute->getJobID() << endl;
		m_queue->pop(); // Remove the job from the queue
		pthread_mutex_unlock(&m_queueLock);

		try
		{
			// Execute the job
			m_currJob->execute();
		}
		catch (RescheduleException) // If a rescheduling has happened just move on to the next job
		{
		}
		m_currJob = NULL;
		m_currJobInfo = NULL;
	}
}

void *RunnerThread::addJob(void *(*pFunc)(void*), void *pArgs, void *pJobInfo, ThreadData* pThreadData)
{
	InnerJob* newJob = new InnerJob(pFunc, pArgs, pJobInfo, pThreadData);

	// Add the job to the queue
	pthread_mutex_lock(&m_queueLock);
	m_queue->push(newJob);
	pthread_cond_signal(&m_condQueueNotEmpty);
	pthread_mutex_unlock(&m_queueLock);

	// wait for the job to end
	void* result = newJob->waitForFinish();
	delete newJob;
	return result;
}

void *RunnerThread::getJobInfo()
{
	return m_currJobInfo;
}

void RunnerThread::moveJob(InnerJob *jobMoved)
{
	// Just add the job to the current queue (there is already a thread that waits for it's end)
	pthread_mutex_lock(&m_queueLock);
	m_queue->push(jobMoved);
	pthread_cond_signal(&m_condQueueNotEmpty);
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

