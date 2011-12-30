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

#ifndef __POLICIES_H__
#define __POLICIES_H__

#include "ContentionManager.h"
#include "Reclaimer.h"
#include "MiniVector.h"
#include "Epoch.h"

namespace stm
{
    namespace internal
    {
        /**
         *  Wrap all interaction with Epochs and with the Heap, so that we can
         *  ensure that memory is reclaimed properly on abort or commit
         */
        class DeferredReclamationMMPolicy
        {
            /**
             *  Cache of the tx id
             */
            unsigned long id;

            /**
             *  Interface to memory management.  This might point to a
             *  thread-private heap, or it might just be a thin wrapper on a
             *  malloc() implementation
             */
            mm::TxHeap* heap;

            /**
             *  use this flag to log allocations.  If we're in a transaction,
             *  then anything we allocate should be deleted on abort.  The flag
             *  ensures that we don't have logging overhead for allocations
             *  outside of a transaction.
             */
            bool should_log;

          public:
            /**
             *  List of objects to delete if the current transaction commits.
             *  We use this to schedule objects for deletion from within a
             *  transaction.
             */
            MiniVector<mm::CustomAllocedBase*> deleteOnCommit;

            /**
             *  List of objects to delete if the current transaction aborts.
             *  These are the things we allocated in a transaction that failed.
             */
            MiniVector<mm::CustomAllocedBase*> deleteOnAbort;

            /**
             *  The reclaimer is used to let objects 'coalesce' - we don't
             *  actually delete until we know no tx is looking at the object.
             */
            Reclaimer reclaimer;

          public:

            /**
             *  Constructing the DeferredReclamationMMPolicy is very easy
             */
            DeferredReclamationMMPolicy(unsigned long _id)
                : id(_id), heap(::new mm::TxHeap(id)), should_log(false),
                  deleteOnCommit(heap, 128), deleteOnAbort(heap, 128),
                  reclaimer(heap)
            {
                // set up global GC info
                globalEpoch.zero_transaction(id);
            }

            /**
             *  Wrapper to thread-specific allocator for allocating memory
             */
            void* txAlloc(size_t const &size)
            {
                void* ptr = heap->tx_alloc(size);
                if (should_log)
                    deleteOnAbort.insert(static_cast<mm::CustomAllocedBase*>
                                         (ptr));
                return ptr;
            }

            /**
             *  Wrapper to thread-specific allocator for freeing memory
             */
            void txFree(void* ptr)
            {
                if (ptr == NULL)
                    return;
                heap->tx_free(ptr);
            }

          private:
            /**
             *  Use the state of the transaction to pick one of the mm logs and
             *  delete everything on it.
             */
            void logFlush(unsigned long state)
            {
                if (state == COMMITTED) {
                    if (deleteOnCommit.is_empty())
                        return;
                    for (unsigned long i = 0;
                         i < deleteOnCommit.element_count; i++)
                    {
                        reclaimer.add(deleteOnCommit.elements[i]);
                    }
                }
                else {
                    if (deleteOnAbort.is_empty())
                        return;
                    for (unsigned long i = 0;
                         i < deleteOnAbort.element_count; i++)
                    {
                        reclaimer.add(deleteOnAbort.elements[i]);
                    }
                }
            }

          public:
            /**
             *  Event method on beginning of a transaction
             */
            void onTxBegin()
            {
                // update the gc epoch
                globalEpoch.enter_transaction(id);

                // set up alloc logging
                should_log = true;
            }

            /**
             *  Event method on any tx validation call
             */
            void onValidate()
            {
                // update our epoch.  Either we are going to abort, or else we
                // are not looking at anything that other active threads have
                // deleted

                // NB: if we want privatization with a heavy fence, this cannot
                // be turned on.
#ifndef PRIVATIZATION_TFENCE
                globalEpoch.on_validation(id);
#endif
            }

            /**
             *  Event method on end of a transaction
             */
            void onTxEnd(unsigned long state)
            {
                logFlush(state);
                should_log = false;
                deleteOnCommit.reset();
                deleteOnAbort.reset();

                // update state in GC
                globalEpoch.exit_transaction(id);
            }

            /**
             *  accessor to return the heap object
             */
            mm::TxHeap* getHeap() const { return heap; }
        };

        /**
         *  Policy that prefers to use a static CM with all inlined calls and
         *  no vtable overheads, but that lets you specify a different CM at
         *  run time if that's what you really want.
         */
        class HybridCMPolicy
        {
            /**
             *  Flag for deciding which CM to use.
             */
            const bool static_flag;

            /**
             *  Static CM.  All calls get inlined, but you can't change your CM
             *  at run time.  Used if use_static_cm is true.
             */
            cm::DEFAULT_CM staticCM;

            /**
             *  Dynamic contention manager (used only if use_static_cm is
             *  false).  Has big vtable overhead, but you can pick you CM at
             *  run time.
             */
            cm::ContentionManager* dynamicCM;

          public:

            /**
             *  Set up the hybrid policy: if the bool flag is false, then get a
             *  CM from the factory.
             */
            HybridCMPolicy(bool static_cm, std::string dynamic_cm)
                : static_flag(static_cm), staticCM()
            {
                if (!static_flag)
                    dynamicCM = cm::Factory(dynamic_cm);
            }

            /**
             *  return a contention manager (used by a CM to get another
             *  descriptor's CM)
             */
            cm::ContentionManager* getCM()
            {
                if (static_flag) return &staticCM;
                else             return dynamicCM;
            }

            ///  Wrapper around OnBeginTransaction
            void onBeginTx()
            {
                if (static_flag) staticCM.OnBeginTransaction();
                else             dynamicCM->OnBeginTransaction();
            }

            ///  Wrapper for OnTryCommitTransaction
            void onTryCommitTx()
            {
                if (static_flag) staticCM.OnTryCommitTransaction();
                else             dynamicCM->OnTryCommitTransaction();
            }

            ///  Wrapper for OnTransactionCommitted
            void onTxCommitted()
            {
                if (static_flag) staticCM.OnTransactionCommitted();
                else             dynamicCM->OnTransactionCommitted();
            }

            ///  Wrapper for OnTransactionAborted
            void onTxAborted()
            {
                if (static_flag) staticCM.OnTransactionAborted();
                else             dynamicCM->OnTransactionAborted();
            }

            ///  Wrapper for onContention
            void onContention()
            {
                if (static_flag) staticCM.onContention();
                else             dynamicCM->onContention();
            }

            ///  Wrapper for OnOpenRead
            void onOpenRead()
            {
                if (static_flag) staticCM.OnOpenRead();
                else             dynamicCM->OnOpenRead();
            }

            ///  Wrapper for OnOpenWrite
            void onOpenWrite()
            {
                if (static_flag) staticCM.OnOpenWrite();
                else             dynamicCM->OnOpenWrite();
            }

            ///  Wrapper for OnReOpen
            void onReOpen()
            {
                if (static_flag) staticCM.OnReOpen();
                else             dynamicCM->OnReOpen();
            }

            ///  Wrapper for ShouldAbort
            bool shouldAbort(cm::ContentionManager* enemy)
            {
                if (static_flag) return staticCM.ShouldAbort(enemy);
                else             return dynamicCM->ShouldAbort(enemy);
            }

            ///  Wrapper for ShouldAbortAll
            bool shouldAbortAll(unsigned long bitmap)
            {
                if (static_flag) return staticCM.ShouldAbortAll(bitmap);
                else             return dynamicCM->ShouldAbortAll(bitmap);
            }
        };

        /**
         *  Policy that ignores user input and always just uses a static CM
         */
        class PureStaticCMPolicy
        {
            ///  Static CM to use at all times
            cm::DEFAULT_CM staticCM;

          public:

            ///  Set up the PureStatic policy by constructing staticCM
            PureStaticCMPolicy(bool b, std::string s) : staticCM() { }

            /**
             *  return a contention manager (used by a CM to get another
             *  descriptor's CM)
             */
            cm::ContentionManager* getCM() { return &staticCM; }

            ///  Wrapper around OnBeginTransaction
            void onBeginTx() { staticCM.OnBeginTransaction(); }

            ///  Wrapper for OnTryCommitTransaction
            void onTryCommitTx() { staticCM.OnTryCommitTransaction(); }

            ///  Wrapper for OnTransactionCommitted
            void onTxCommitted() { staticCM.OnTransactionCommitted(); }

            ///  Wrapper for OnTransactionAborted
            void onTxAborted() { staticCM.OnTransactionAborted(); }

            ///  Wrapper for onContention
            void onContention() { staticCM.onContention(); }

            ///  Wrapper for OnOpenRead
            void onOpenRead() { staticCM.OnOpenRead(); }

            ///  Wrapper for OnOpenWrite
            void onOpenWrite() { staticCM.OnOpenWrite(); }

            ///  Wrapper for OnReOpen
            void onReOpen() { staticCM.OnReOpen(); }

            ///  Wrapper for ShouldAbort
            bool shouldAbort(cm::ContentionManager* enemy)
            {
                return staticCM.ShouldAbort(enemy);
            }

            ///  Wrapper for ShouldAbortAll
            bool shouldAbortAll(unsigned long bitmap)
            {
                return staticCM.ShouldAbortAll(bitmap);
            }
        };

        /**
         *  Policy that always uses a dynamically allocated CM and pays the
         *  vtable overhead.
         */
        class PureDynamicCMPolicy
        {
            /**
             *  Contention manager to use for all calls.  Has big vtable
             *  overhead, but you can pick you CM at run time.
             */
            cm::ContentionManager* dynamicCM;

          public:

            /**
             *  Set up the PureDynamic policy by calling the factory
             */
            PureDynamicCMPolicy(bool b, std::string cm_name)
            {
                dynamicCM = cm::Factory(cm_name);
            }

            /**
             *  return a contention manager (used by a CM to get another
             *  descriptor's CM)
             */
            cm::ContentionManager* getCM() { return dynamicCM; }

            ///  Wrapper around OnBeginTransaction
            void onBeginTx() { dynamicCM->OnBeginTransaction(); }

            ///  Wrapper for OnTryCommitTransaction
            void onTryCommitTx() { dynamicCM->OnTryCommitTransaction(); }

            ///  Wrapper for OnTransactionCommitted
            void onTxCommitted() { dynamicCM->OnTransactionCommitted(); }

            ///  Wrapper for OnTransactionAborted
            void onTxAborted() { dynamicCM->OnTransactionAborted(); }

            ///  Wrapper for onContention
            void onContention() { dynamicCM->onContention(); }

            ///  Wrapper for OnOpenRead
            void onOpenRead() { dynamicCM->OnOpenRead(); }

            ///  Wrapper for OnOpenWrite
            void onOpenWrite() { dynamicCM->OnOpenWrite(); }

            ///  Wrapper for OnReOpen
            void onReOpen() { dynamicCM->OnReOpen(); }

            ///  Wrapper for ShouldAbort
            bool shouldAbort(cm::ContentionManager* enemy)
            {
                return dynamicCM->ShouldAbort(enemy);
            }

            ///  Wrapper for ShouldAbortAll
            bool shouldAbortAll(unsigned long bitmap)
            {
                return dynamicCM->ShouldAbortAll(bitmap);
            }
        };
    } // namespace stm::internal
} // namespace stm
#endif // __POLICIES_H__
