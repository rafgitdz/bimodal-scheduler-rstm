///////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2005, 2006
// University of Rochester
// Department of Computer Science
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//    * Redistributions of source code must retain the above copyright notice,
//      this list of conditions and the following disclaimer.
//
//    * Redistributions in binary form must reproduce the above copyright
//      notice, this list of conditions and the following disclaimer in the
//      documentation and/or other materials provided with the distribution.
//
//    * Neither the name of the University of Rochester nor the names of its
//      contributors may be used to endorse or promote products derived from
//      this software without specific prior written permission.
//
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.

#ifndef __STM_IDMANAGER_H__
#define __STM_IDMANAGER_H__

#if defined(TLS_PTHREAD)
#include <pthread.h>
#endif

#include <cassert>
#include "atomic_ops.h"

namespace stm
{
    /**
     *  Interface to thread-local storage to keep a 0-based ID for each
     *  transactional thread.
     */
    class IdManager
    {
      private:
        /**
         *  number of threads registered with the system
         */
        volatile unsigned long activeThreads;

#if defined(TLS_GCC_IMPLICIT)
        /**
         *  if we're using gcc thread local storage specifiers, then here is
         *  where we'll put the thread id.
         */
        static __thread unsigned long s_tid;
#elif defined(TLS_PTHREAD)
        /**
         *  If we're using pthread calls for thread local storage, then here is
         *  the pthread_key for the thread id.
         */
        pthread_key_t tlsThreadIDKey;
#endif

      public:
        /**
         *  Configure the IdManager.  This assumes that there are no threads,
         *  and zeroes the thread count.
         */
        IdManager() : activeThreads(0)
        {
#if defined(TLS_PTHREAD)
            // if we're using pthreads, then we need to create the key
            int result __attribute__((unused)) =
                pthread_key_create(&tlsThreadIDKey, NULL);
            assert(result == 0);
#endif
        }

        /**
         *  Basic getter method to announce how many active transactional
         *  threads have been registered with the IdManager.
         */
        unsigned long getThreadCount() const { return activeThreads; }

        /**
         *  Get a thread's transactional id via whatever method is configured
         *  as TLS
         */
        unsigned long getThreadId() const
        {
#if defined(TLS_GCC_IMPLICIT)
            // easy:  just return the thread-local gcc static variable
            return s_tid;

#elif defined(TLS_SPARC_UNSAFE)
            // use asm to get register g5, which will store the thread-local
            // gcc static variable
            unsigned long out;
            asm volatile("mov   %%g5, %0;" : "=&r"(out));
            return out;

#elif defined(TLS_PTHREAD)
            // use pthreads
            return reinterpret_cast<unsigned long>
                (pthread_getspecific(tlsThreadIDKey));

#else
            // something is wrong
            abort();
#endif
        }

        /**
         *  Register a thread with the IdManager.  In other words, get an id if
         *  you have never gotten one before, and store it in whatever method
         *  of TLS we're using.
         */
        unsigned long registerThread()
        {
            // get an id for this thread
            unsigned long id = fai(&activeThreads);

#if defined(TLS_GCC_IMPLICIT)
            // store the id in the gcc thread-local static var
            s_tid = id;

#elif defined(TLS_SPARC_UNSAFE)
            // store the id in register g5
            asm volatile("mov   %0,   %%g5;"
                         :: "r"(id) : "g5");

#elif defined(TLS_PTHREAD)
            // store the id in the pthread key
            pthread_setspecific(tlsThreadIDKey, reinterpret_cast<void*>(id));

#else
            // something is wrong
            abort();
#endif
            // return the ID
            return id;
        }
    } __attribute__ ((aligned(64)));


    /**
     *  One IdManager will be created, and everyone can see it.
     */
    extern IdManager idManager;
}

#endif // __STM_IDMANAGER_H__
