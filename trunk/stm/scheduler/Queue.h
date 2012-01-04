/*
 * A simple queue implementation
 * 
 */

#ifndef __STM_QUEUE__
#define __STM_QUEUE__

#include <pthread.h>
#include "ThreadData.h"

#include <iostream>

namespace stm
{
	namespace scheduler
	{
		// An inner class that represents a job that needs to be done
		class InnerJob
		{
		private:
			static int m_iAllJobsIDs;
			void *(*m_pFunc)(void*);
			void *m_pArgs;
			void *m_pJobInfo;
			bool m_blnFinished;
			void *m_result;
			long m_epochNum;

			// condition variable 
			pthread_mutex_t* m_jobLock;
			pthread_cond_t* m_condJobFinished;

			int m_iJobID;

		public:
			InnerJob(void *(*pFunc)(void*), void *pArgs, void *pJobInfo, ThreadData* pThreadData) 
				: m_pFunc(pFunc), m_pArgs(pArgs), m_pJobInfo(pJobInfo), m_blnFinished(false), m_result(0), m_epochNum(-1),
					m_jobLock(pThreadData->getLock()), m_condJobFinished(pThreadData->getCondVar()), m_iJobID(++m_iAllJobsIDs)
			{
			}

			void execute()
			{
				m_result = (*m_pFunc)(m_pArgs);
				pthread_mutex_lock(m_jobLock);
				m_blnFinished = true;
				pthread_cond_signal(m_condJobFinished);
				pthread_mutex_unlock(m_jobLock);
			}
			
			void *waitForFinish()
			{
				pthread_mutex_lock(m_jobLock);
				while (!m_blnFinished)
				{
					pthread_cond_wait(m_condJobFinished, m_jobLock);
				}
				pthread_mutex_unlock(m_jobLock);
				return m_result;
			}

			int getJobID()
			{
				return m_iJobID;
			}

			void *getJobInfo()
			{
				return m_pJobInfo;
			}
			
			void setEpochNum(long epochNum)
			{
				m_epochNum = epochNum;
			}
			
			long getEpochNum() 
			{
				return m_epochNum;
			}
		};

		class Queue
		{
		public:

			Queue() : first(0), last(0), mySize(0)
			{ }

			Queue(const Queue &original);
			~Queue();

			bool empty() const
			{ return (first == 0); }

			void push(InnerJob *const value);

			void pushFront(InnerJob *const value);

			InnerJob* front() const
			{ return (first->data); }

			void pop();


		private:
			class Node
			{
			public: // this public is public inside Queue but still private outside Queue
				InnerJob* data;
				Node *next;
			};
		  Node* first;
		  Node* last;   // added - comparing with linked list based stack
		  int mySize;
		};
	}
}

#endif //__STM_QUEUE__
