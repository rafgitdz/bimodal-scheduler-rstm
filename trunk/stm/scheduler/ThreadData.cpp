#include "ThreadData.h"

using namespace stm::scheduler;

// Create a unique thread data manager
namespace stm
{
	namespace scheduler
	{
		ThreadDataManager threadDataManager = ThreadDataManager();
	}
}

ThreadData::ThreadData()
{
	pthread_mutex_init(&m_lock, NULL);
	pthread_cond_init(&m_condVar, NULL);
}

ThreadData::~ThreadData()
{
	pthread_cond_destroy(&m_condVar);
	pthread_mutex_destroy(&m_lock);
}

pthread_mutex_t* ThreadData::getLock()
{
	return &m_lock;
}

pthread_cond_t* ThreadData::getCondVar()
{
	return &m_condVar;
}

