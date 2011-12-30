#include "ThreadLock.h"

using namespace stm::scheduler;

ThreadLock::ThreadLock()
{
	pthread_mutex_init(&m_lock, NULL);
}

ThreadLock::~ThreadLock()
{
	pthread_mutex_destroy(&m_lock);
}

void ThreadLock::Lock()
{
	pthread_mutex_lock(&m_lock);
}

void ThreadLock::Unlock()
{
	pthread_mutex_unlock(&m_lock);
}

