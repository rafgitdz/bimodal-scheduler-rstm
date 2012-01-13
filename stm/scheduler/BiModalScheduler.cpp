#include "BiModalScheduler.h" 

#include "scheduler_common.h"
#include <cstdlib>

#include <iostream>

using namespace std;
using namespace stm::scheduler;

// static members declarations
long BiModalScheduler::m_lngCoresNum;
BiModalScheduler* BiModalScheduler::m_Instance;
long BiModalScheduler::m_epoch;
Queue* BiModalScheduler::m_roQueue;
int BiModalScheduler::m_roQueueCount;


ThreadLock* BiModalScheduler::m_threadLock = new ThreadLock();

BiModalScheduler::BiModalScheduler() 
{
	m_lngCoresNum = getCoresNum();
	initExecutingThreads();
	m_roQueue = new Queue();
	m_epoch = 0;
	m_roQueueCount = 0;
}

stm::scheduler::BiModalScheduler::~BiModalScheduler()
{
	if (m_Instance)
	{
		delete m_threadLock;
		delete m_Instance;
		delete m_roQueue;
	}
}

void BiModalScheduler::init()
{
	// Set the thread's data
	ThreadData *pThreadData = new ThreadData();
	threadDataManager.setThreadData(pThreadData);

	if (!m_Instance)
	{
		m_threadLock->Lock();
		if (!m_Instance)
		{
			m_Instance = new BiModalScheduler();
		}
		m_threadLock->Unlock();
	}
	cout << "Scheduler initialized" << endl;
}

stm::scheduler::BiModalScheduler* BiModalScheduler::instance()
{
	if (!m_Instance)
	{
		cout << "Fault in StmScheduler. Exiting!" << endl;
		// Throw an exception
		exit(1);
	}
	return m_Instance;
}

void BiModalScheduler::shutdown()
{
	BiModalScheduler* scheduler = instance();
	/* Go over all runner threads, and shut down each thread */
	for (int iThread = 0; iThread < m_lngCoresNum; iThread++)
	{
		scheduler->m_arThreads[iThread]->shutdown();
	}

}

long stm::scheduler::BiModalScheduler::getCoresNum()
{
	long lngCoresNum = 0;
	lngCoresNum = sysconf(_SC_NPROCESSORS_ONLN);
	return lngCoresNum;
}

/*
 * When a new transation enters the system, we schedule it on the 
 * core which has less transactions in his queue
 */
void *stm::scheduler::BiModalScheduler::schedule(void *(*pFunc)(void*), void *pArgs, void *pJobInfo)
{
	void* result = NULL;
	int iCore = 0;
	iCore = sched_getcpu();
	bool found = false;
	int iCurQueueSize;
	int iMinJobs = m_arThreads[iCore]->getJobsNum();
	for (int iQueue = 0; (iQueue < m_lngCoresNum) && !found; iQueue++) {
		iCurQueueSize = m_arThreads[iQueue]->getJobsNum();
		if (iCurQueueSize <= iMinJobs) {
			iCore = iQueue;
			iMinJobs = iCurQueueSize;
			if (iCurQueueSize == 0)
				found = true;
		}
	}
	result = m_arThreads[iCore]->addJob(pFunc, pArgs, pJobInfo, threadDataManager.getThreadData());
	//cout << "Job scheduled in core " << iCore << endl;

	return result;
}

/******** Threads related ************/

void stm::scheduler::BiModalScheduler::initExecutingThreads()
{
	int iThread = 0;

	// Create the threads needed
	m_arThreads = new RunnerThread*[m_lngCoresNum];

	// Initialize each thread.
	for (iThread = 0; iThread < m_lngCoresNum; iThread++)
	{
		m_arThreads[iThread] = new RunnerThread(iThread);
	}

	for (iThread = 0; iThread < m_lngCoresNum; iThread++)
	{
		// Run each thread
		m_arThreads[iThread]->run();
	}
}

void BiModalScheduler::reschedule(int iFromCore, int iToCore)
{
	cout << "Rescheduling from: " << iFromCore << " to: " << iToCore << endl;
	m_arThreads[iFromCore]->moveJob(m_arThreads[iToCore]);
	throw RescheduleException();
}

long BiModalScheduler::getCurrentEpoch(int iCoreNum) {
	return m_arThreads[iCoreNum]->getCurrentEpoch();
}

void BiModalScheduler::moveJobToROQueue(InnerJob *job) {
	
    m_threadLock->Lock();
	m_roQueue->push(job);
    m_threadLock->Unlock();
	
	throw RescheduleException();
}

InnerJob* BiModalScheduler::roQueueDeque() {
	
	InnerJob *job = m_roQueue->front();
	m_roQueue->pop();
	
	return job;
}

bool BiModalScheduler::allQueuesEmpty() {
	bool empty = true;
	for (int i = 0; (i < 2 && empty); i++)
		for (int iQueue = 0; (iQueue < m_lngCoresNum && empty); iQueue++) {
			empty = m_arThreads[iQueue]->getJobsNum() == 0;
		}
	return empty;
}