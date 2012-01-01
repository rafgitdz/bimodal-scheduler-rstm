#include "BiModalScheduler.h" 

#include "RescheduleException.h"
#include <cstdlib>

using namespace std;
using namespace stm::scheduler;

// static members declarations
long stm::scheduler::BiModalScheduler::m_lngCoresNum;
stm::scheduler::BiModalScheduler* stm::scheduler::BiModalScheduler::m_Instance;

ThreadLock* stm::scheduler::BiModalScheduler::m_threadLock = new ThreadLock();

stm::scheduler::BiModalScheduler::BiModalScheduler() 
{
	m_lngCoresNum = getCoresNum();
	initExecutingThreads();
}

stm::scheduler::BiModalScheduler::~BiModalScheduler()
{
	if (m_Instance)
	{
		delete m_threadLock;
		delete m_Instance;
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
