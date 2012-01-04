#include "BiModalScheduler.h" 

#include "RescheduleException.h"
#include <cstdlib>

using namespace std;
using namespace stm::scheduler;

// static members declarations
long stm::scheduler::BiModalScheduler::m_lngCoresNum;
stm::scheduler::BiModalScheduler* stm::scheduler::BiModalScheduler::m_Instance;
long stm::scheduler::BiModalScheduler::m_epochNum;
Queue* BiModalScheduler::m_roQueue;
pthread_mutex_t BiModalScheduler::m_queueLock;
long BiModalScheduler::m_pushJobCntr;
long BiModalScheduler::m_popJobCntr;


ThreadLock* stm::scheduler::BiModalScheduler::m_threadLock = new ThreadLock();

stm::scheduler::BiModalScheduler::BiModalScheduler() 
{
	m_lngCoresNum = getCoresNum();
	initExecutingThreads();
	m_roQueue = new Queue();
	m_epochNum = 0;
	m_popJobCntr = 0;
	m_pushJobCntr = 0;
	
	// Initialize the lock and condition var
	pthread_mutex_init(&m_queueLock, NULL);
}

stm::scheduler::BiModalScheduler::~BiModalScheduler()
{
	if (m_Instance)
	{
		delete m_threadLock;
		delete m_Instance;
		pthread_mutex_destroy(&m_queueLock);
		delete m_roQueue;
	}
}

void stm::scheduler::BiModalScheduler::init()
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
}

stm::scheduler::BiModalScheduler* stm::scheduler::BiModalScheduler::instance()
{
	if (!m_Instance)
	{
		cout << "Fault in StmScheduler. Exiting!" << endl;
		// Throw an exception
		exit(1);
	}
	return m_Instance;
}

void stm::scheduler::BiModalScheduler::shutdown()
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

void *stm::scheduler::BiModalScheduler::schedule(void *(*pFunc)(void*), void *pArgs, void *pJobInfo)
{
	void* result = NULL;
	int iCore = 0;
	iCore = sched_getcpu();
	result = m_arThreads[iCore]->addJob(pFunc, pArgs, pJobInfo, threadDataManager.getThreadData());

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
	//cout << "Rescheduling from: " << iFromCore << " to: " << iToCore << endl;
	m_arThreads[iFromCore]->moveJob(m_arThreads[iToCore]);
	throw RescheduleException();
}

long BiModalScheduler::getCurrentEpoch() {
	return m_epochNum;
}

long BiModalScheduler::getCurrentEpoch(int iCoreNum) {
	return m_arThreads[iCoreNum]->getCurrentEpoch();
}

void BiModalScheduler::moveJobToROQueue(InnerJob *job) {
	
	pthread_mutex_lock(&m_queueLock);
	m_roQueue->push(job);
	m_pushJobCntr++;
	if (m_pushJobCntr == getCoresNum()) {
		m_epochNum++;
		m_popJobCntr = 0;
		m_pushJobCntr = 0;
	}
	pthread_mutex_unlock(&m_queueLock);
	
	throw RescheduleException();
}

InnerJob* BiModalScheduler::getJobFromROQueue() {
	pthread_mutex_lock(&m_queueLock);
	InnerJob *job = m_roQueue->front();
	job->setEpochNum(m_epochNum);
	m_roQueue->pop();
	m_popJobCntr++;
	if (m_popJobCntr == getCoresNum()) {
		m_epochNum++;
	}
	
	pthread_mutex_unlock(&m_queueLock);
	
	return job;
}
