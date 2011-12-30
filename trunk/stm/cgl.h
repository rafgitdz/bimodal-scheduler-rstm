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

#ifndef __STM_H__
#define __STM_H__

#include <string>
#include <assert.h>
#include "atomic_ops.h"
#include "stm_common.h"
#include "Descriptor_cgl.h"
#include "accessors.h"

#ifdef TATAS

// NB: only tatas has timing instrumentation.  also note that this puts a
// pthread hit on every cgl call!
extern tatas_lock_t globalLock;

#ifdef TIMING_BREAKDOWNS
#define BEGIN_TRANSACTION                                           \
    {                                                               \
        stm::internal::Descriptor* locallyCachedTransactionContext  \
          = stm::internal::get_descriptor();                        \
        locallyCachedTransactionContext->timing.UPDATE_TIMING(      \
            TIMING_BOOKKEEPING);                                    \
        tatas_acquire(&globalLock);                                 \
        locallyCachedTransactionContext->timing.UPDATE_TIMING(      \
            TIMING_REALWORK);

#define END_TRANSACTION                                                       \
        tatas_release(&globalLock);                                           \
        locallyCachedTransactionContext->timing.UPDATE_TIMING(TIMING_NON_TX); \
        locallyCachedTransactionContext->timing.TIMING_TRANSFER_COMMITTED();  \
    }
#else

#define BEGIN_TRANSACTION                                                   \
    {                                                                       \
        stm::internal::Descriptor*                                          \
        locallyCachedTransactionContext __attribute__ ((unused)) = NULL;    \
        tatas_acquire(&globalLock);

#define END_TRANSACTION                 \
        tatas_release(&globalLock);     \
    }

#endif
#endif

#ifdef TICKET

extern ticket_lock_t globalLock;
#define BEGIN_TRANSACTION               \
    {                                   \
        stm::internal::Descriptor*                                          \
        locallyCachedTransactionContext __attribute__ ((unused)) = NULL;    \
        ticket_acquire(&globalLock);

#define END_TRANSACTION                 \
        ticket_release(&globalLock);    \
    }

#endif

#ifdef MCS

extern mcs_qnode_t* globalLock;
#define BEGIN_TRANSACTION                                               \
    {                                                                   \
        stm::internal::Descriptor* locallyCachedTransactionContext =    \
            stm::internal::get_descriptor();                            \
        mcs_acquire(&globalLock,                                        \
                    &locallyCachedTransactionContext->mcsnode);

#define END_TRANSACTION                                 \
        mcs_release(&globalLock,                        \
        &locallyCachedTransactionContext->mcsnode);     \
    }

#endif

#ifdef PTHREAD_MUTEX

extern pthread_mutex_t globalLock;
#define BEGIN_TRANSACTION                   \
    {                                       \
        stm::internal::Descriptor*                                          \
        locallyCachedTransactionContext __attribute__ ((unused)) = NULL;    \
        pthread_mutex_lock(&globalLock);

#define END_TRANSACTION                     \
        pthread_mutex_unlock(&globalLock);  \
    }

#endif

namespace stm
{
    // standard API entry points
    inline void init(std::string, std::string, bool)
    {
        // get an ID for the new thread
        unsigned long id = stm::idManager.registerThread();

        // create a Descriptor for the new thread and put it in the global
        // table
        internal::desc_array[id] = new internal::Descriptor(id);
    }

    inline void shutdown() { }

    // get a chunk of garbage-collected, tx-safe memory
    inline void* tx_alloc(size_t size)
    {
        return internal::get_descriptor()->txAlloc(size);
    }

    // return memory acquired through tx_alloc
    inline void tx_free(void* ptr) { stm::mm::txfree(ptr); }

    inline void fence() { }

    namespace internal
    {
        // tell each descriptor to print its timing information
        inline static void report_timing()
        {
#ifdef TIMING_BREAKDOWNS
            int i = 0;
            while (desc_array[i]) {
                desc_array[i]->timing.REPORT_TIMING(i);
                i++;
            }
#endif
        }

        class SharedBase
        {
          public:
            void* operator new(size_t size) { return tx_alloc(size); }
            void operator delete(void* p) { tx_free(p); }
        };

        // Shared is just a type wrapper around a T
        template<class T>
        class Shared : public internal::SharedBase, public T
        {
          public:
            static Shared<T>* CreateShared(T* object)
            {
                return object->shared();
            }

            static void DeleteShared(Descriptor& tx, Shared<T>* shared)
            {
                tx.addDtor(shared);
            }

            static void DeleteSharedUn(Shared<T>* shared)
            {
                get_descriptor()->addDtor(shared);
            }

            bool check_alias(const T* t, Descriptor& tx) const
            {
                return true;
            }
        };

        class ObjectBase
        {
          public:
            void* operator new(size_t size) { return tx_alloc(size); }
            void operator delete(void* p) { tx_free(p); }

            virtual ~ObjectBase() { }
        };

        /**
         * Validator is a dummy object for rstm
         */
        class Validator
        {
          public:
            void validate(const ObjectBase*) const { };
        };
    }

    // Object's methods just return T as well
    template<class T>
    class Object : public internal::ObjectBase
    {
      public:
        // open_RO and open_RW just return /this/
        const T* open_RO(internal::Descriptor* tx, internal::Validator& v)
        {
            return static_cast<const T*>(this);
        }

        T* open_RW(internal::Descriptor* tx, internal::Validator& v) const
        {
            return static_cast<T*>(const_cast<Object<T>*>(this));
        }

        const T* open_RO(internal::Descriptor& d, internal::Validator& v)
        {
            return open_RO(&d, v);
        }

        T* open_RW(internal::Descriptor& d, internal::Validator& v)
        {
            return open_RW(&d, v);
        }

        T* open_un(internal::Validator& v)
        {
            return static_cast<T*>(this);
        }

        void verify_un(const T* t) const {  }


        // the shared() of /this/ is just /this/
        internal::Shared<T>* shared(internal::Descriptor* tx = 0) const
        {
            return
               static_cast<internal::Shared<T>*>(const_cast<Object<T>*>(this));
        }
    };

};

inline void stm::internal::Descriptor::addDtor(internal::SharedBase* ptr)
{
    delete(ptr);
}

#endif // __STM_H__
