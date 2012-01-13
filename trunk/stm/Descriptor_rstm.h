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

#ifndef __DESCRIPTOR_H__
#define __DESCRIPTOR_H__

// standard requirements for all TMs
#include <string>
#include "stm_common.h"
#include "MiniVector.h"

// global commit counter
#include "ConflictDetector.h"

// mm and interfaces
#include "stm_mm.h"
#include "policies.h"

// instrumentation for counting events and timing
#include "instrumentation.h"

// object metadata for this TM implementation
#include "ObjectBase_rstm.h"
#include "SharedBase_rstm.h"

#ifndef USE_BIMODAL
#define USE_BIMODAL
#endif
#ifdef USE_BIMODAL
#include <sched.h>
#endif

namespace stm
{
    namespace internal
    {
#ifdef PRIVATIZATION_NOFENCE
        /**
         *  if we're using nonblocking privatization, we need a flag to trigger
         *  immediate validation in active transactions whenever a privatizer
         *  commits.
         */
        extern volatile unsigned long pcount;
#endif

        /**
         *  Tuple for storing all the info we need in a lazy write set
         */
        struct lazy_bookkeep_t
        {
            SharedBase* shared;
            ObjectBase* read_version;
            ObjectBase* write_version;
            bool        isAcquired;

            lazy_bookkeep_t(SharedBase* _sh = NULL, ObjectBase* _rd = NULL,
                            ObjectBase* _wr = NULL, bool _acq = false)
                : shared(_sh), read_version(_rd), write_version(_wr),
                  isAcquired(_acq)
            { }
        };

        /**
         *  Tuple for storing all the info we need in an eager write set
         */
        struct eager_bookkeep_t
        {
            SharedBase* shared;
            ObjectBase* read_version;
            ObjectBase* write_version;

            eager_bookkeep_t(SharedBase* _sh = NULL, ObjectBase* _rd = NULL,
                             ObjectBase* _wr = NULL)
                : shared(_sh), read_version(_rd), write_version(_wr)
            { }
        };

        /**
         *  Tuple for storing all the info we need in an invisible read set
         */
        struct invis_bookkeep_t
        {
            SharedBase* shared;
            ObjectBase* read_version;

            invis_bookkeep_t(SharedBase* _sh = NULL, ObjectBase* _rd = NULL)
                : shared(_sh), read_version(_rd)
            { }
        };

        // forward declare the validator, since it and Descriptor are mutually
        // dependent
        class Validator;

        /**
         *  The entire RSTM algorithm, and all of the metadata pertaining to an
         *  active transaction instance, are ensapsulated in the Descriptor.
         *
         *  [todo] - there is a lot of cruft in here, especially since so much
         *  code has moved into the descriptor.  Obviously the private / public
         *  delineations need to change.  However, it's probably also true that
         *  we've got too many levels of function nesting.  The compiler should
         *  squash the many levels, but we don't want to have to maintain them,
         *  so we should clean them up.
         *
         *  [todo] - review the placement of fields.  Some fields are accessed
         *  by other threads, some aren't.  We should segregate fields into
         *  different cache lines accordingly.
         */
        class Descriptor
        {
          public:
            /**
             * state is COMMITTED, ABORTED, or ACTIVE
             */
            volatile unsigned long tx_state; // definitely read by other
                                             // transactions

            /**
             *  thread id
             */
            const unsigned long id;     // definitely read by other
                                        // transactions

#ifdef USE_BIMODAL
			/**
			 * the core where this transaction have to be rescheduled,
			 * when the transaction rollbacks
			 */
			long reschedule_core_num;
			
			/**
			 *  the core where this transaction is executed
			 */
			unsigned long iCore;
#endif
            /**
             *  sw vis reader bitmask
             */
            unsigned long id_mask;

#ifdef PRIVATIZATION_NOFENCE
            /**
             *  If we're using nonblocking privatization, we need a copy of the
             *  value of pcount last time we checked it.
             */
            unsigned long pcount_cache;
#endif

            /**
             *  Policy wrapper around a CM
             */
            HybridCMPolicy cm;  // definitely read by other transactions

            /**
             *  For privatization: check pcount and see if validation is
             *  necessary.
             */
            void check_pcount();

            /**
             *  Constructor is very straightforward
             */
            Descriptor(unsigned long id, std::string dynamic_cm,
                       std::string validation, bool use_static_cm);

            /**
             *  Destructor is currently a nop.
             *
             *  [todo] - Eventually we should flush all memory that the
             *  descriptor is tracking when we destroy the descriptor.
             */
            ~Descriptor() { }

            /**
             *  Heavyweight function to get the current thread's Descriptor via
             *  thread-local storage (TLS)
             */
            inline static Descriptor& CurrentThreadInstance()
            {
                Descriptor* tx = desc_array[idManager.getThreadId()];
                assert(tx);
                return *tx;
            }

          private:
            /**
             *  Check if we are in an active transaction.
             */
            inline static bool nontransactional()
            {
                Descriptor* tx __attribute__((unused));
                return (((tx = desc_array[idManager.getThreadId()]) == 0) ||
                        (tx->tx_state == COMMITTED));
            }

          public:

            /**
             *  Make sure we are not in an active transaction.
             *  No-op unless asserts are turned on, in which case it's
             *  heavyweight.
             */
            inline static void assert_nontransactional()
            {
                assert(nontransactional());
            }

            inline static void assert_transactional()
            {
                assert(!nontransactional());
            }

            /**
             *  If we've turned on profiling instrumentation, this field is
             *  where we'll store intermediate timing information.
             */
            TimeAccounting timing;

            /**
             *  flag indicating if the current transaction is using lazy or
             *  eager acquire.
             */
            bool isLazy;

            /**
             *  flag indicating if the current transaction is using visible or
             *  invisible reads.
             */
            bool isVisible;

            /**
             *  Interface to thread-local allocator that manages reclamation on
             *  abort / commit automatically.
             */
            DeferredReclamationMMPolicy mm;

            /**
             *  Set all metadata up for a new transaction, and move to ACTIVE
             */
            void beginTransaction();

            /**
             *  Attempt to commit a transaction
             */
            void commit();

            /**
             *  abort a transaction.  Usually we want to throw Aborted(), but
             *  use the flag in case we're in a special case where throwing is
             *  a bad idea.
             */
            void abort(bool shouldThrow = true);

            /**
             *  Clean up after a transaction, return true if the tx is
             *  COMMITTED.  This cleans up all the metadata within the
             *  Descriptor, and also cleans up any per-object metadata that the
             *  most recent transaction modified.
             */
            bool cleanup();

          private:
            /**
             * explicit validation of a transaction's state: make sure tx_state
             * isn't ABORTED
             */
            void verifySelf();

            /**
             *  If we're using conflict detection heuristic to avoid
             *  validation, this stores our local heuristic information.
             */
            ValidationPolicy conflicts;

            /**
             *  Make sure that any object we read and are writing lazily hasn't
             *  changed.
             */
            void verifyLazyWrites();

            /**
             *  Make sure that any object we read and aren't writing hasn't
             *  changed.
             */
            void verifyInvisReads();

            /**
             *  Acquire all objects on the lazy write list
             */
            void acquireLazily();

            /**
             *  clean up all objects in the eager write set.  If we committed,
             *  then we should unset the lsb of each object's header.  If we
             *  aborted, we should swap the pointer of each header from the
             *  clone to the original version.
             */
            void cleanupEagerWrites(unsigned long tx_state);

            /**
             *  Lookup an entry in the lazy write set.  If we try to read
             *  something that we've opened lazily, the only way to avoid an
             *  alias error is to use this method.
             */
            ObjectBase* lookupLazyWrite(SharedBase* shared) const;

            /**
             *  clean up all objects in the lazy write set.  If we committed,
             *  then we should unset the lsb of each object's header.  If we
             *  aborted, then for each object that we successfully acquired, we
             *  should swap the pointer of the header from the clone to the
             *  original version.
             */
            void cleanupLazyWrites(unsigned long tx_state);

            /**
             *  Combine add / lookup / validate in the read set.  This lets me
             *  avoid duplicate entries.  If I'm opening O, instead of looking
             *  up O in my read set I just assume it's new, open it, and then
             *  use a validating add.  For each object in the set, I validate
             *  the object.  Then, if that object is O, I can quit early.
             *  Otherwise, once I'm done traversing the list I add O, and I've
             *  both validated and ensured no duplicates.
             */
            void addValidateInvisRead(SharedBase* shared, ObjectBase* version);

            /**
             *  Validate the invisible read and lazy write sets
             */
            void validate();

            /**
             *  Validate the invisible read set and insert a new entry if it
             *  isn't already in the set.
             */
            void validatingInsert(SharedBase* shared, ObjectBase* version);

            /**
             *  Find an element in the read set and remove it.  We use this for
             *  early release, but we don't use it for upgrading reads to
             *  writes.
             */
            void removeInvisRead(SharedBase* shared);

            /**
             *  For each entry in the visible read set, take myself out of the
             *  object's reader list.
             */
            void cleanupVisReads();

            /**
             *  Remove an entry from the visible reader list, as part of early
             *  release.
             */
            void removeVisRead(SharedBase* shared);

            /**
             *  The invisible read list
             */
            MiniVector<invis_bookkeep_t> invisibleReads;

            /**
             *  The visible read list
             */
            MiniVector<SharedBase*> visibleReads;

            /**
             *  The eager write list
             */
            MiniVector<eager_bookkeep_t> eagerWrites;

            /**
             *  The lazy write list
             */
            MiniVector<lazy_bookkeep_t> lazyWrites;

          public:
            /**
             *  MM wrapper for scheduling an object to be deleted if the
             *  current transaction commits.
             */
            void addDtor(SharedBase* sh);

            /**
             *  MM wrapper for scheduling an object to be deleted immediately
             *  (use only with un_ptrs)
             */
            void addDtorUn(SharedBase* sh);

            /**
             * Descriptors are equivalent if their thread ids are equivalent
             */
            bool operator==(const Descriptor& rhs) const
            {
                return (id == rhs.id);
            }

            /**
             * Descriptors are equivalent if their thread ids are equivalent
             */
            bool operator!=(const Descriptor& rhs) const
            {
                return (id != rhs.id);
            }

            /**
             *  Check if shared.header and expected_version agree on the
             *  currently valid version of the data.  Used to validate objects
             */
            const bool isCurrent(const SharedBase* header,
                                 const ObjectBase* expected) const;

            /**
             *  Acquire an object (used for lazy acquire).
             */
            const bool lazyAcquire(SharedBase* header,
                                   const ObjectBase* expected,
                                   const ObjectBase* newer);

            /**
             *  Clean up an object's metadata when its owner transaction
             *  committed.
             */
            static const bool cleanOnCommit(SharedBase* header,
                                            ObjectBase* valid_ver);

            /**
             *  Clean up an object's metadata when its owner aborted
             */
            static const bool cleanOnAbort(SharedBase* header,
                                           const ObjectBase* expect,
                                           const ObjectBase* new_val);

            /**
             *  remove this tx descriptor from the visible readers bitmap
             */
            void removeVisibleReader(SharedBase* header) const;

            /**
             * replace one header with another; thin wrapper around bool_cas
             */
            static const bool swapHeader(SharedBase* header,
                                         ObjectBase* expected,
                                         ObjectBase* replacement);

            /**
             *  abort all visible readers embedded in the reader bitmap
             */
            void abortVisibleReaders(SharedBase* header);

            /**
             * install this tx descriptor as a visible reader of an object
             */
            const bool installVisibleReader(SharedBase* header);

            /**
             *  Get a readable version of an object from its header
             */
            const ObjectBase* open_RO(SharedBase* header, Validator& v);

            /**
             *  Get a writable version of an object from its header
             */
            ObjectBase* open_RW(SharedBase* header, Validator& v);

            /**
             *  Get a version of an object that can be used outside of
             *  transactions
             */
            static ObjectBase* open_un(SharedBase* header);

            /**
             *  Remove an object from the read set of this transaction.
             */
            void release(SharedBase* obj);

            /**
             *  Heavyweight method to ensure that this transaction didn't open
             *  sh for writing and hold on to obj as a readable pointer.
             */
            bool ensure_no_upgrade(const SharedBase* sh,
                                   const ObjectBase* obj) const;
        };  // class Descriptor

        /**
         * Validator is only for obstruction-free privatization
         */
        class Validator
        {
            /**
             *  Store the current transaction, so we don't have to take a TLS
             *  hit to get it.
             */
            Descriptor* m_tx;
          public:

            /**
             *  If we're in a transaction, make sure that a privatizer didn't
             *  commit.
             */
            void validate(const void*) const
            {
                if (m_tx)
                    m_tx->check_pcount();
            }

            /**
             *  Set up the validator by giving it a descriptor
             */
            void config(Descriptor* tx)
            {
                m_tx = tx;
            }
        };

        /**
         *  get the current thread's Descriptor
         */
        inline Descriptor* get_descriptor()
        {
            return &Descriptor::CurrentThreadInstance();
        }
    } // namespace stm::internal

    /**
     *  wrapper to allocate memory.
     */
    inline void* tx_alloc(size_t size)
    {
        internal::Descriptor* tx = internal::get_descriptor();
        tx->timing.GIVE_TIMING();
        void* ret = tx->mm.txAlloc(size);
        tx->timing.TAKE_TIMING(TIMING_MM);
        return ret;
    }

    /**
     *  wrapper to free memory.
     */
    inline void tx_free(void* ptr)
    {
        internal::get_descriptor()->mm.txFree(ptr);
    }

    namespace internal
    {
        inline void Descriptor::abort(bool shouldAbort)
        {
            timing.UPDATE_TIMING(TIMING_BOOKKEEPING);
            tx_state = ABORTED;

            // Contention Manager notification
            cm.onTxAborted();

            if (shouldAbort) {
				#ifdef USE_BIMODAL
			if (reschedule_core_num != -1) {
				cm.onConflictWith(reschedule_core_num);
				reschedule_core_num = -1;
			}
#endif
                throw Aborted();
            }
        }

        inline void Descriptor::validate()
        {
            // Update timing, then do validation
            mm.onValidate();
            timing.UPDATE_TIMING(TIMING_VALIDATION);
            verifyInvisReads();
            verifyLazyWrites();
        }

        inline void Descriptor::validatingInsert(SharedBase* sh,
                                                 ObjectBase* ver)
        {
            // update the timing
            timing.UPDATE_TIMING(TIMING_VALIDATION);
            // now do the insert (and possibly validate)
            addValidateInvisRead(sh, ver);
            // lastly, verify the lazy write set
            verifyLazyWrites();
        }

        inline void Descriptor::verifySelf()
        {
            if (tx_state == ABORTED) {
                ConflictCounter[id].ADD_VALIDATION_FAIL();
                abort();
            }
        }

        inline void Descriptor::beginTransaction()
        {
            timing.UPDATE_TIMING(TIMING_BOOKKEEPING);

            conflicts.onTxBegin();
            mm.onTxBegin();

            // mark myself active
            tx_state = ACTIVE;

            // cm notification
            cm.onBeginTx();

            timing.UPDATE_TIMING(TIMING_REALWORK);
        }

        inline void Descriptor::removeVisRead(SharedBase* shared)
        {
            assert(shared);
            SharedBase** e = visibleReads.elements;

            for (unsigned long i = 0; i < visibleReads.element_count; i++) {
                if (e[i] == shared) {
                    visibleReads.remove(i);
                    return;
                }
            }
        }

        inline void Descriptor::removeInvisRead(SharedBase* shared)
        {
            invis_bookkeep_t* e = invisibleReads.elements;
            for (unsigned long i = 0; i < invisibleReads.element_count; i++) {
                if (e[i].shared == shared) {
                    invisibleReads.remove(i);
                    return;
                }
            }
        }

        inline ObjectBase* Descriptor::lookupLazyWrite(SharedBase* shared)
            const
        {
            if (lazyWrites.is_empty())
                return NULL;

            assert(shared);
            lazy_bookkeep_t* e = lazyWrites.elements;

            for (unsigned long i = 0; i < lazyWrites.element_count; i++)
                if (e[i].shared == shared)
                    return e[i].write_version;
            return NULL;
        }

        inline void Descriptor::check_pcount()
        {
#ifdef PRIVATIZATION_NOFENCE
            while (pcount_cache != pcount) {
                pcount_cache = pcount;
                validate();
                verifySelf();
            }
#endif
        }

        inline Descriptor::Descriptor(unsigned long _id,
                                      std::string dynamic_cm,
                                      std::string validation,
                                      bool _use_static_cm)
            : // set up the id, build bitmap mask based on the ID
            id(_id), id_mask(1 << _id),
            // set up CM
            cm(_use_static_cm, dynamic_cm),
            // set up the DeferredReclamationMMPolicy object
            mm(_id),
            // construct bookkeeping fields that depend on the heap
            conflicts(),
            invisibleReads(mm.getHeap(), 64),
            visibleReads(mm.getHeap(), 64),
            eagerWrites(mm.getHeap(), 64),
            lazyWrites(mm.getHeap(), 64)
        {
			
#ifdef USE_BIMODAL
			iCore = sched_getcpu();
			reschedule_core_num = -1;
			
#endif
            // the state is COMMITTED, in tx #0
            tx_state = stm::COMMITTED;

#ifdef PRIVATIZATION_NOFENCE
            pcount_cache = pcount;
#endif

            // set up the acquire and read rules
            // for now, the read ruls is the max number of visible readers
            if (validation == "invis-eager" || validation == "invis-lazy" ||
                _id > 31)
                isVisible = false;
            else
                isVisible = true;

            // for now, the acquire rule is boolean; 1=eager
            if (validation == "invis-eager" || validation == "vis-eager")
                isLazy = false;
            else
                isLazy = true;

            // set up the timing fields
            timing = TimeAccounting();
        }

        inline bool Descriptor::cleanup()
        {
            timing.UPDATE_TIMING(TIMING_BOOKKEEPING);

            // invariant: status == COMMITTED or ABORTED
            assert(tx_state == COMMITTED || tx_state == ABORTED);

            if (tx_state == COMMITTED) {
                ConflictCounter[id].ADD_COMMIT();
                cm.onTxCommitted();
            }
            else
                cm.onTxAborted();

            // clean up the descriptor

            // at the end of a transaction, we are supposed to restore the
            // headers of any objects that we acquired (regardless of whether
            // the tx aborted or committed).
            cleanupLazyWrites(tx_state);
            cleanupEagerWrites(tx_state);

            // clean up read sets: uninstall myself from any objects I have
            // open for reading visibly, null out my invis read list
            cleanupVisReads();
            invisibleReads.reset();

            // commit memory changes and reset memory logging
            mm.onTxEnd(tx_state);

            // mark ourself as out of accounted time
            timing.UPDATE_TIMING(TIMING_NON_TX);

            if (tx_state == COMMITTED)
                timing.TIMING_TRANSFER_COMMITTED();
            else
                timing.TIMING_TRANSFER_ABORTED();
            return tx_state == COMMITTED;
        }

        inline void Descriptor::commit()
        {
            timing.UPDATE_TIMING(TIMING_BOOKKEEPING);

            // only try to commit if we are still ACTIVE
            if (tx_state == ACTIVE) {
                // Contention Manager notification
                cm.onTryCommitTx();

                // acquire objects that were open_RW'd lazily
                if (isLazy)
                    acquireLazily();

                // validate if necessary
                if (!conflicts.tryCommit()) {
                    timing.UPDATE_TIMING(TIMING_VALIDATION);
                    verifyInvisReads();
                    conflicts.forceCommit();
                }

                // cas status to commit; if this cas fails then I've been
                // aborted
                bool_cas(&(tx_state), ACTIVE, COMMITTED);
            }
        }

        // for each visible reader, we must remove the reader from the object
        // once that is done, we can reset the vis_reads list
        inline void Descriptor::cleanupVisReads()
        {
            if (visibleReads.is_empty())
                return;

            SharedBase** e = visibleReads.elements;

            for (unsigned long i = 0; i < visibleReads.element_count; i++)
                removeVisibleReader(e[i]);

            visibleReads.reset();
        }

        inline void Descriptor::verifyInvisReads()
        {
            if (invisibleReads.is_empty())
                return;

            invis_bookkeep_t* e = invisibleReads.elements;

            for (unsigned long i = 0; i < invisibleReads.element_count; i++) {
                if (!isCurrent(e[i].shared, e[i].read_version)) {
                    ConflictCounter[id].ADD_VALIDATION_FAIL();
                    abort();
                }
            }
        }

        inline void Descriptor::addValidateInvisRead(SharedBase* shared,
                                                     ObjectBase* version)
        {
            if (invisibleReads.is_empty()) {
                invisibleReads.insert(invis_bookkeep_t(shared, version));
                return;
            }

            invis_bookkeep_t* e = invisibleReads.elements;

            for (unsigned long i = 0; i < invisibleReads.element_count; i++) {
                if (!isCurrent(e[i].shared, e[i].read_version)) {
                    ConflictCounter[id].ADD_VALIDATION_FAIL();
                    abort();
                }
                if (e[i].shared == shared)
                    return;
            }
            invisibleReads.insert(invis_bookkeep_t(shared, version));
        }

        inline void Descriptor::verifyLazyWrites()
        {
            if (lazyWrites.is_empty())
                return;

            lazy_bookkeep_t* e = lazyWrites.elements;

            for (unsigned long i = 0; i < lazyWrites.element_count; i++) {
                if (!isCurrent(e[i].shared, e[i].read_version)) {
                    ConflictCounter[id].ADD_VALIDATION_FAIL();
                    abort();
                }
            }
        }

        // acquire all objects lazily opened for writing
        inline void Descriptor::acquireLazily()
        {
            if (lazyWrites.is_empty())
                return;

            lazy_bookkeep_t* e = lazyWrites.elements;

            for (unsigned long i = 0; i < lazyWrites.element_count; i++) {
                assert(e[i].isAcquired == false);
                if (!(lazyAcquire(e[i].shared, e[i].read_version,
                                  e[i].write_version)))
                    abort();
                e[i].isAcquired = true;
            }
        }

        // after COMMIT/ABORT, cleanup headers of all objects that we
        // acquired/cloned lazily
        inline void Descriptor::cleanupLazyWrites(unsigned long tx_state)
        {
            if (lazyWrites.is_empty())
                return;

            lazy_bookkeep_t* e = lazyWrites.elements;

            if (tx_state == ABORTED) {
                for (unsigned long i = 0; i < lazyWrites.element_count; i++) {
                    if (e[i].isAcquired) {
                        cleanOnAbort(e[i].shared, e[i].write_version,
                                     e[i].read_version);
                    }
                }
            }
            else {
                assert(tx_state == COMMITTED);
                for (unsigned long i = 0; i < lazyWrites.element_count; i++) {
                    cleanOnCommit(e[i].shared, e[i].write_version);
                }
            }
            lazyWrites.reset();
        }

        // after COMMIT/ABORT, cleanup headers of all objects that we
        // acquired/cloned eagerly
        inline void Descriptor::cleanupEagerWrites(unsigned long tx_state)
        {
            if (eagerWrites.is_empty())
                return;

            eager_bookkeep_t* e = eagerWrites.elements;
            if (tx_state == stm::ABORTED) {
                for (unsigned long i = 0; i < eagerWrites.element_count; i++)
                    cleanOnAbort(e[i].shared, e[i].write_version,
                                 e[i].read_version);
            }
            else {
                assert(tx_state == stm::COMMITTED);
                for (unsigned long i = 0; i < eagerWrites.element_count; i++)
                    cleanOnCommit(e[i].shared, e[i].write_version);
            }
            eagerWrites.reset();
        }

        inline void Descriptor::addDtor(SharedBase* sh)
        {
            Validator v;
            mm.deleteOnCommit.insert(sh);
            mm.deleteOnCommit.insert(const_cast<ObjectBase*>(open_RO(sh, v)));
        }

        inline void Descriptor::addDtorUn(SharedBase* sh)
        {
            mm.reclaimer.add(sh);
            mm.reclaimer.add(open_un(sh));
        }

        inline const bool
        Descriptor::isCurrent(const SharedBase* header,
                              const ObjectBase* expected) const
        {
            ObjectBase* payload = const_cast<ObjectBase*>(header->m_payload);

            // if the header meets our expectations, validation succeeds
            if (payload == expected)
                return true;

            // the only other way that validation can succeed is if the owner
            // == tx and the installed object is a clone of expected
            ObjectBase* ver = get_data_ptr(payload);

            // [?] do we need to look at next_obj_version or is owner
            // sufficient
            return ((ver->m_owner == this) && (ver->m_next == expected));
        }

        inline const bool Descriptor::lazyAcquire(SharedBase* header,
                                                  const ObjectBase* expected,
                                                  const ObjectBase* newer)
        {
            // while loop to retry if cannot abort visible readers yet
            while (true) {
                timing.UPDATE_TIMING(TIMING_BOOKKEEPING);

                // check if we can abort visible readers, restart loop on
                // failure
                if (!cm.shouldAbortAll(header->m_readers)) {
                    timing.UPDATE_TIMING(TIMING_CM);
                    cm.onContention();
                    verifySelf();
                    continue;
                }

                // try to acquire this Shared<T>, restart loop on failure
                if (!swapHeader(header, unset_lsb(expected), set_lsb(newer))) {
                    ConflictCounter[id].ADD_VALIDATION_FAIL();
                    return false;
                }

                // successfully acquired this Shared<T>; now abort visible
                // readers
                abortVisibleReaders(header);
                cm.onOpenWrite();
                return true;
            }
        }

        inline const bool Descriptor::cleanOnCommit(SharedBase* header,
                                                    ObjectBase* valid_ver)
        {
            // this used to just return the result of swapHeader, but since we
            // want to null out the next pointer we must be more complex
            bool result = swapHeader(header, set_lsb(valid_ver),
                                     unset_lsb(valid_ver));

            if (result)
                valid_ver->m_next = NULL;

            return result;
        }

        inline const bool
        Descriptor::cleanOnAbort(SharedBase* header,
                                 const ObjectBase* expected,
                                 const ObjectBase* replacement)
        {
            return swapHeader(header, set_lsb(expected),
                              unset_lsb(replacement));
        }

        inline void Descriptor::removeVisibleReader(SharedBase* header) const
        {
            // don't bother with the CAS if we're not in the bitmap
            if (header->m_readers & id_mask) {
                unsigned long obj_mask;

                do {
                    obj_mask = header->m_readers;
                }
                while (!bool_cas(&header->m_readers, obj_mask,
                                 obj_mask & (~id_mask)));
            }
        }


        inline const bool Descriptor::swapHeader(SharedBase* header,
                                                 ObjectBase* expected,
                                                 ObjectBase* replacement)
        {
            return
                bool_cas(reinterpret_cast<volatile unsigned long*>
                           (&header->m_payload),
                         reinterpret_cast<unsigned long>(expected),
                         reinterpret_cast<unsigned long>(replacement));
        }

        inline const bool Descriptor::installVisibleReader(SharedBase* header)
        {
            if (header->m_readers & id_mask)
                return false;

            unsigned long obj_mask;
            do {
                obj_mask = header->m_readers;
            } while (!bool_cas(&header->m_readers, obj_mask,
                               obj_mask | id_mask));
            return true;
        }

        inline void Descriptor::abortVisibleReaders(SharedBase* header)
        {
            unsigned long tmp_reader_bitmap = header->m_readers;
            unsigned long desc_array_index = 0;

            while (tmp_reader_bitmap) {
                if (tmp_reader_bitmap & 1) {
                    Descriptor* reader = desc_array[desc_array_index];
                    // abort only if the reader exists, is ACTIVE, and isn't me
                    if (reader && (reader != this) &&
                        (reader->tx_state == ACTIVE))
                    {
                        bool_cas(&(reader->tx_state), ACTIVE, ABORTED);
                    }
                }

                tmp_reader_bitmap >>= 1;
                desc_array_index++;
            }
        }

        inline const ObjectBase* Descriptor::open_RO(SharedBase* header,
                                                     Validator& v)
        {
            // make sure all parameters meet our expectations
            if (!header)
                return NULL;

            // start timing, and ensure this tx isn't aborted
            timing.UPDATE_TIMING(TIMING_BOOKKEEPING);
            verifySelf();

            v.config(this);

            // if we have a RW copy of this that we opened lazily, we must
            // return it
            ObjectBase* ret = lookupLazyWrite(header);
            if (ret) {
                timing.UPDATE_TIMING(TIMING_REALWORK);
                return ret;
            }

            while (true) {
                // read the header of /this/ to a local, opportunistically get
                // data ptr
                timing.UPDATE_TIMING(TIMING_BOOKKEEPING);

                // Initialization because of gcc complaints about uninitialized
                // use (are not always initialized in the first while loop).
                ObjectBase* snap;
                ObjectBase* newer;
                ObjectBase* older = NULL;
                Descriptor* owner = NULL;
                unsigned long ownerState = -1;
                bool isOwned;

                while (true) {
                    snap = const_cast<ObjectBase*>(header->m_payload);
                    newer = get_data_ptr(snap);
                    assert(newer);
                    isOwned = is_owned(snap);
                    if (!isOwned)
                        break;

                    older = newer->m_next;

                    owner = const_cast<Descriptor*>(newer->m_owner);
                    ownerState = owner->tx_state;

                    if (header->m_payload == snap)
                        break;
                }

                // if /this/ is owned, newer may not be the right ptr to use
                if (isOwned) {
                    // if the owner is ACTIVE, either I own it or I must call
                    // CM
                    if (ownerState == ACTIVE) {
                        if (owner == this) {
                            cm.onReOpen();
                            timing.UPDATE_TIMING(TIMING_REALWORK);
                            return newer;
                        }

                        ConflictCounter[id].ADD_CONFLICT();

                        // continue unless we kill the owner; if kill the
                        // owner, use older version
                        if (!cm.shouldAbort(owner->cm.getCM())
                            || !bool_cas(&(owner->tx_state), ACTIVE, ABORTED))
                        {
#ifdef USE_BIMODAL
							owner->reschedule_core_num = iCore;
#endif				
                            timing.UPDATE_TIMING(TIMING_CM);
                            cm.onContention();
                            verifySelf();
                            continue;
                        }

                        ConflictCounter[id].ADD_EXPLICIT_ABORT();
                        // now ensure that we fallthrough to the cleanOnAbort
                        // code
                        ownerState = ABORTED;
                        
#ifdef USE_BIMODAL
						reschedule_core_num = owner->iCore;
#endif
                    }

                    // if current owner is aborted use cleanOnAbort
                    if (ownerState == ABORTED) {
                        if (!cleanOnAbort(header, snap, older)) {
                            // if cleanup failed; if snap != older there is
                            // contention.  Otherwise, a like-minded tx did the
                            // cleanup for us
                            if (header->m_payload != older) {
                                ConflictCounter[id].ADD_CLEANUP_FAIL();
                                timing.UPDATE_TIMING(TIMING_CM);
                                cm.onContention();
                                verifySelf();
                                continue;
                            }
                        }
                        // someone (not necessarily us) cleaned the object to
                        // the state we wanted, so from here on out old version
                        // is valid
                        newer = older;
                        assert(newer);
                    }
                    else {
                        // we had better be looking at a committed object
                        assert(ownerState == COMMITTED);

                        if (!cleanOnCommit(header, newer)) {
                            // cleanOnCommit failed; if payload != newer there
                            // is contention.  Otherwise someone did the
                            // cleanup for us
                            if (header->m_payload != newer) {
                                ConflictCounter[id].ADD_CLEANUP_FAIL();
                                timing.UPDATE_TIMING(TIMING_CM);
                                cm.onContention();
                                verifySelf();
                                continue;
                            }
                        }
                    }
                }

                // try to be a visible reader
                if (isVisible) {
                    // install as a vis reader, and bookkeep so we can
                    // uninstall later
                    if (installVisibleReader(header))
                        visibleReads.insert(header);

                    // verify that the header of /this/ hasn't changed; if it
                    // changed, at least one writer acquired /this/, and we may
                    // be incorrect
                    if (newer != header->m_payload) {
                        abort();
                    }

                    // validate
                    validate();
                }
                else {
                    // can cause duplicates if we don't use
                    // addValidateInvisRead()

                    if (conflicts.shouldValidate()) {
                        if (conflicts.isValidatingInsertSafe()) {
                            validatingInsert(header, newer);
                        }
                        else {
                            invisibleReads.insert(invis_bookkeep_t(header,
                                                                   newer));
                            validate();
                        }
                    }
                    else {
                        invisibleReads.insert(invis_bookkeep_t(header, newer));
                    }
                }
                verifySelf();

                // notify cm, update read count, and return
                cm.onOpenRead();

                timing.UPDATE_TIMING(TIMING_REALWORK);
                return newer;
            } // end while (true)
        }

        inline ObjectBase* Descriptor::open_RW(SharedBase* header,
                                               Validator& v)
        {
            // make sure all parameters meet our expectations
            if (!header)
                return NULL;

            // start timing now, and ensure this tx isn't aborted
            timing.UPDATE_TIMING(TIMING_BOOKKEEPING);
            verifySelf();

            v.config(this);

            // make sure that our conflict detection strategy knows we've got a
            // write
            conflicts.onRW();

            // if we have an RW copy of this that we opened lazily, return it
            ObjectBase* ret = lookupLazyWrite(header);
            if (ret) {
                timing.UPDATE_TIMING(TIMING_REALWORK);
                return ret;
            }

            while (true) {
                // get consistent view of the object:
                timing.UPDATE_TIMING(TIMING_BOOKKEEPING);

                // Initialization because of gcc uninitialized use warnings.
                ObjectBase* snap;
                ObjectBase* newer;
                ObjectBase* older = NULL;
                Descriptor* owner = NULL;
                unsigned long ownerState = -1;
                bool isOwned;

                while (true) {
                    snap = const_cast<ObjectBase*>(header->m_payload);
                    newer = get_data_ptr(snap);
                    assert(newer);
                    isOwned = is_owned(snap);
                    if (!isOwned)
                        break;

                    older = newer->m_next;

                    owner = const_cast<Descriptor*>(newer->m_owner);
                    ownerState = owner->tx_state;

                    if (header->m_payload == snap)
                        break;
                }

                // if /this/ is owned, curr_version may not be right
                if (isOwned) {
                    // if the owner is ACTIVE, either I own it or I must call
                    // CM
                    if (ownerState == ACTIVE) {
                        if (owner == this) {
                            cm.onReOpen();
                            timing.UPDATE_TIMING(TIMING_REALWORK);
                            return newer;
                        }

                        ConflictCounter[id].ADD_CONFLICT();

                        // continue unless we kill the owner; if kill the
                        // owner, use older version
                        if (!cm.shouldAbort(owner->cm.getCM())
                            || !bool_cas(&(owner->tx_state), ACTIVE, ABORTED))
                        {
#ifdef USE_BIMODAL
							owner->reschedule_core_num = iCore;
#endif	
                            timing.UPDATE_TIMING(TIMING_CM);
                            cm.onContention();
                            verifySelf();
                            continue;
                        }

                        ConflictCounter[id].ADD_EXPLICIT_ABORT();
                        // now ensure that we fallthrough to the cleanOnAbort
                        // code
                        ownerState = ABORTED;
#ifdef USE_BIMODAL
						reschedule_core_num = owner->iCore;
#endif
                    }

                    // if current owner is aborted use cleanOnAbort if we are
                    // lazy, else just plan on using older
                    if (ownerState == ABORTED) {
                        if (isLazy) {
                            // if lazy, we must clean header:
                            if (!cleanOnAbort(header, snap, older)) {
                                // cleanup failed; if snap != older there is
                                // contention.  Otherwise, a like-minded tx did
                                // the cleanup for us
                                if (header->m_payload != older) {
                                    ConflictCounter[id].ADD_CLEANUP_FAIL();
                                    timing.UPDATE_TIMING(TIMING_CM);
                                    cm.onContention();
                                    verifySelf();
                                    continue;
                                }
                            }
                        }
                        // ok, either we are lazy and we cleaned the object, or
                        // we're eager and we're going to cas out the old
                        // header shortly.  In either case, older is the valid
                        // version
                        newer = older;
                        assert(newer);
                    }
                    else {
                        // we had better be looking at a committed object
                        assert(ownerState == COMMITTED);
                        if (isLazy) {
                            // if lazy, we must clean header:
                            if (!cleanOnCommit(header, newer)) {
                                // cleanOnCommit failed; if payload != newer
                                // there is contention.  Otherwise someone
                                // cleaned up for us
                                if (header->m_payload != newer) {
                                    ConflictCounter[id].ADD_CLEANUP_FAIL();
                                    timing.UPDATE_TIMING(TIMING_CM);
                                    cm.onContention();
                                    verifySelf();
                                    continue;
                                }
                            }
                        }
                    }
                }

                // EAGER: continue if we can't abort all visible readers
                if (!isLazy && !cm.shouldAbortAll(header->m_readers)) {
                    timing.UPDATE_TIMING(TIMING_CM);
                    cm.onContention();
                    verifySelf();
                    continue;
                }


                // clone the object and make me the owner of the new version
                timing.UPDATE_TIMING(TIMING_COPY);
                ObjectBase* new_version = newer->clone();
                assert(new_version);
                new_version->m_st = header;
                new_version->m_next = newer;
                new_version->m_owner = this;
                timing.UPDATE_TIMING(TIMING_BOOKKEEPING);

                if (isLazy) {
                    // LAZY: just add /this/ to my lazy writeset and mark the
                    // old version for delete on commit
                    lazyWrites.insert(lazy_bookkeep_t(header, newer, new_version));

                    mm.deleteOnCommit.insert(newer);
                }
                else {
                    // EAGER: CAS in a new header, retry open_RW on failure
                    if (!swapHeader(header, snap, set_lsb(new_version))) {
                        ConflictCounter[id].ADD_CLEANUP_FAIL();
                        mm.deleteOnCommit.insert(new_version);
                        new_version = NULL;
                        timing.UPDATE_TIMING(TIMING_CM);
                        cm.onContention();
                        verifySelf();
                        continue;
                    }

                    // CAS succeeded: abort visible readers, add /this/ to my
                    // eager writeset, and mark /curr_version/ for
                    // deleteOnCommit
                    abortVisibleReaders(header);
                    mm.deleteOnCommit.insert(newer);
                    eagerWrites.insert(eager_bookkeep_t(header, newer,
                                                        new_version));
                }

                // Validate, notify cm, reset timing, and return
                if (conflicts.shouldValidate())
                    validate();

                verifySelf();
                cm.onOpenWrite();
                timing.UPDATE_TIMING(TIMING_REALWORK);
                return new_version;
            } // end while (true)
        }

        inline ObjectBase* Descriptor::open_un(SharedBase* header)
        {
#ifdef PRIVATIZATION_TFENCE
            if (!header) {
                return NULL;
            }

            ObjectBase* snap = const_cast<ObjectBase*>(header->m_payload);
            ObjectBase* newer = get_data_ptr(snap);

            assert(!is_owned(snap));

            return newer;
#else
            if (!header)
                return NULL;

            while (true) {
                // read the header of /this/ to a local, opportunistically get
                // data ptr

                // Initialization because of gcc complaints about uninitialized
                // use (are not always initialized in the first while loop).
                ObjectBase* snap;
                ObjectBase* newer;
                ObjectBase* older = NULL;
                Descriptor* owner = NULL;
                unsigned long ownerState = COMMITTED;
                bool isOwned = false;

                while (true) {
                    // get consistent view of metadata
                    snap = const_cast<ObjectBase*>(header->m_payload);
                    newer = get_data_ptr(snap);
                    isOwned = is_owned(snap);
                    if (!isOwned)
                        break;

                    older = newer->m_next;

                    owner = const_cast<Descriptor*>(newer->m_owner);
                    ownerState = owner->tx_state;

                    if (header->m_payload == snap)
                        break;
                }

                // if /this/ is owned, newer may not be the right ptr to use
                if (isOwned) {
                    // if the owner is ACTIVE, abort him
                    if (ownerState == ACTIVE) {
                        // if abort fails, restart loop
                        if (!bool_cas(&(owner->tx_state), ACTIVE, ABORTED))
                        {
                            continue;
                        }

                        // now ensure that we fallthrough to the cleanOnAbort
                        // code
                        ownerState = ABORTED;
                    }

                    // if current owner is aborted use cleanOnAbort
                    if (ownerState == ABORTED) {
                        if (!cleanOnAbort(header, snap, older)) {
                            // if cleanup failed; if snap != older there is
                            // contention.  Otherwise, a like-minded tx did the
                            // cleanup for us
                            if (header->m_payload != older) {
                                continue;
                            }
                        }
                        // someone (not necessarily us) cleaned the object to
                        // the state we wanted, so from here on out old version
                        // is valid
                        newer = older;
                    }
                    else {
                        // we had better be looking at a committed object
                        assert(ownerState == COMMITTED);

                        if (!cleanOnCommit(header, newer)) {
                            // cleanOnCommit failed; if payload != newer there
                            // is contention.  Otherwise someone did the
                            // cleanup for us
                            if (header->m_payload != newer) {
                                continue;
                            }
                        }
                    }
                }

                // we're going to use whatever is in newer from here on out
                assert(newer);
                return newer;
            } // end while (true)
#endif
        }

        inline void Descriptor::release(SharedBase* obj)
        {
            timing.UPDATE_TIMING(TIMING_BOOKKEEPING);

            // NB: no need to check if I own this in my eager or lazy writeset;
            // that's orthogonal to whether it's in my read set

            // branch based on whether this is a visible read or not
            if ((obj->m_readers & id_mask) != 0) {
                // I'm a vis reader: remove self from the visible reader bitmap
                // and remove /this/ from my vis read set
                removeVisibleReader(obj);
                removeVisRead(obj);
            }
            else
                // invis reader:  just remove /this/ from my invis read set
                removeInvisRead(obj);

            timing.UPDATE_TIMING(TIMING_REALWORK);
        }

        inline bool Descriptor::ensure_no_upgrade(const SharedBase* sh,
                                                  const ObjectBase* obj) const
        {
            // if there's no sh, obviously we're done
            if (sh == NULL)
                return true;

            // read the header to a local.  if it is obj, we're good
            ObjectBase* orig = const_cast<ObjectBase*>(sh->m_payload);
            if (orig == obj)
                return true;

            // yikes!  they don't match.  this could mean a correct upgrade, it
            // could mean that we're destined to abort, or it could mean that
            // we have an upgrade error.

            // get the data pointer without the LSB
            ObjectBase* newer = get_data_ptr(orig);
            // if they match now, it means we upgraded correctly
            if (newer == obj)
                return true;
            // if orig didn't have a lsb set, then orig will equal newer.  This
            // means that someone else cleaned my write and aborted me, or else
            // acquired my read, made a change, committed, and cleaned up.  I'm
            // destined to abort, but I don't have an alias error
            if (newer == orig)
                return true;

            // if we're here, then newer's lsb was originally set, so there
            // ought to be an 'older'.  Get the 'older' as well as newer's
            // owner
            ObjectBase* older = const_cast<ObjectBase*>(newer->m_next);
            Descriptor* owner = const_cast<Descriptor*>(newer->m_owner);
            // upgrade error if older == obj and owner == ME
            if ((older == obj) && (owner == this))
                return false;

            // just to be sure: we should not be the owner.
            assert(owner != this);

            // OK, we're destined to abort because someone else opened this
            // object.  Let's just return true and let some other piece of code
            // worry about finding and handling the abort.  NB: if we were to
            // throw here, we'd throw Aborted() through an assert() call, and I
            // don't want to do that
            return true;
        }
    } // namespace stm::internal
} // namespace stm

#endif // __DESCRIPTOR_H__
