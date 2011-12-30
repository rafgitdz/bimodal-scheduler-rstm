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

// mm and cm interfaces
#include "stm_mm.h"
#include "policies.h"

// instrumentation for timing
#include "instrumentation.h"

// object metadata for this TM implementation
#include "SharedBase_redo_lock.h"

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
            SharedBase*   shared;
            SharedBase*   redo_log;
            unsigned long version;
            bool          isAcq;

            lazy_bookkeep_t(SharedBase* _sh = NULL, SharedBase* _redo = NULL,
                            unsigned long _ver = 0, bool _acq = false)
                : shared(_sh), redo_log(_redo), version(_ver), isAcq(_acq) { }
        };

        /**
         *  Tuple for storing all the info we need in an eager write set
         */
        struct eager_bookkeep_t
        {
            SharedBase* shared;
            SharedBase* redo_log;

            eager_bookkeep_t(SharedBase* _sh = NULL, SharedBase* _redo = NULL)
                : shared(_sh), redo_log(_redo) { }
        };

        /**
         *  Tuple for storing all the info we need in an invisible read set
         */
        struct invis_bookkeep_t
        {
            SharedBase*   shared;
            unsigned long version;

            invis_bookkeep_t(SharedBase* _sh = NULL, unsigned long _ver = 0)
                : shared(_sh), version(_ver) { }
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
             *
             *  [todo] - At one time this was only called at the end of the
             *  transaction.  Is that still true?
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
            SharedBase* lookupLazyWrite(SharedBase* shared);

            /**
             *  clean up all objects in the lazy write set.  If we committed,
             *  then we should unset the lsb of each object's header.  If we
             *  aborted, then for each object that we successfully acquired, we
             *  should swap the pointer of the header from the clone to the
             *  original version.
             *
             *  [todo] - right now we use a per-entry bool to track which
             *  objects are acquired.  A counter would be faster and would have
             *  less storage overhead.
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
            void addValidateInvisRead(SharedBase* shared,
                                      unsigned long version);

            /**
             *  Validate the invisible read and lazy write sets
             */
            void validate();

            /**
             *  Validate the invisible read set and insert a new entry if it
             *  isn't already in the set.
             */
            void validatingInsert(SharedBase* shared, unsigned long version);

            /**
             *  Find an element in the read set and remove it.  We use this for
             *  early release, but we don't use it for upgrading reads to
             *  writes.
             *
             *  [todo] - should we use this for upgrades too?
             */
            void removeInvisRead(SharedBase* shared);

            /**
             *  The invisible read list
             */
            MiniVector<invis_bookkeep_t> invisibleReads;

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
             *  currently valid version of the data.  Used to validate objects.
             */
            bool isCurrent(SharedBase* obj, unsigned long expected_ver) const;

            /**
             * acquire an object (used for lazy acquire)
             */
            bool acquire(SharedBase* obj, unsigned long exp_ver,
                         SharedBase* new_log);

            /**
             *  Clean up an object's metadata when its owner transaction
             *  committed.  This entails trying to lock the object and apply
             *  the redo log.
             */
            static bool cleanOnCommit(SharedBase* obj, Descriptor* exp_owner,
                                      SharedBase* redo_log);

            /**
             *  Clean up an object's metadata when its owner aborted.  This
             *  really just zeroes out the redo log and removes the owner.
             */
            static bool cleanOnAbort(SharedBase* obj, Descriptor* exp_owner,
                                     SharedBase* exp_log);

            /**
             *  Get a readable version of an object
             */
            const SharedBase* getReadable(SharedBase* obj, Validator& v);

            /**
             *  Get a writable version of an object
             */
            SharedBase* getWritable(SharedBase* obj, Validator& v);

            /**
             *  Get a version of an object that can be used outside of
             *  transactions
             */
            static SharedBase* open_un(SharedBase* obj);

            /**
             *  Remove an object from the read set of this transaction
             */
            void release(SharedBase* obj);

            /**
             *  Heavyweight method to ensure that this transaction didn't open
             *  sh for writing and hold on to obj as a readable pointer.
             */
            bool ensure_no_upgrade(const SharedBase* sh, const SharedBase* t);
        };

        // if the validator is 0, that means we're working on a writable
        // version and we don't need to validate reads
        class Validator
        {
            unsigned long m_cachedVersion;
            Descriptor* tx;
          public:
            void config(unsigned long v, Descriptor* _tx)
            {
                m_cachedVersion = v;
                tx = _tx;
            }

            Validator() : m_cachedVersion(0), tx(NULL) { }

            void validate(const SharedBase* sh) const
            {
                if (m_cachedVersion != 0 &&
                    sh->m_metadata.fields.ver.version != m_cachedVersion)
                    tx->abort();
#ifndef PRIVATIZATION_BARRIER
                tx->check_pcount();
#endif
            }
        };

        /**
         *  get the current thread's Descriptor
         *
         *  [todo] - do we still need this?
         */
        inline Descriptor* get_descriptor()
        {
            return &Descriptor::CurrentThreadInstance();
        }
    } // namespace stm::internal

    /**
     *  wrapper to allocate memory.
     *
     *  [todo] this implementation is in the wrong place
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
     *
     *  [todo] - this implementation is in the wrong place
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

            if (shouldAbort)
                throw Aborted();
        }

        inline void Descriptor::validate()
        {
            mm.onValidate();
            timing.UPDATE_TIMING(TIMING_VALIDATION);
            verifyInvisReads();
            verifyLazyWrites();
        }

        inline void Descriptor::validatingInsert(SharedBase* sh,
                                                 unsigned long ver)
        {
            // if anything needs to be validated, then update the timing
            timing.UPDATE_TIMING(TIMING_VALIDATION);
            // now do the insert (and possibly validate)
            addValidateInvisRead(sh, ver);
            // lastly, verify the lazy write set
            verifyLazyWrites();
        }

        // explicit validation of a transaction: make sure tx_state isn't
        // ABORTED
        inline void Descriptor::verifySelf()
        {
            if (tx_state == stm::ABORTED)
                abort();
        }

        inline void Descriptor::beginTransaction()
        {
            timing.UPDATE_TIMING(TIMING_BOOKKEEPING);

            conflicts.onTxBegin();
            mm.onTxBegin();

            // mark myself active
            tx_state = stm::ACTIVE;

            // cm notification
            cm.onBeginTx();

            timing.UPDATE_TIMING(TIMING_REALWORK);
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

        inline SharedBase* Descriptor::lookupLazyWrite(SharedBase* shared)
        {
            if (lazyWrites.is_empty())
                return NULL;

            assert(shared);
            lazy_bookkeep_t* e = lazyWrites.elements;

            for (unsigned long i = 0; i < lazyWrites.element_count; i++)
                if (e[i].shared == shared)
                    return e[i].redo_log;
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
            : // set up the id
            id(_id),
            // set up CM
            cm(_use_static_cm, dynamic_cm),
            // set up the DefferedReclamationMMPolicy object
            mm(_id),
            // construct bookkeeping fields that depend on the heap
            conflicts(),
            invisibleReads(mm.getHeap(), 64),
            eagerWrites(mm.getHeap(), 64),
            lazyWrites(mm.getHeap(), 64)
        {
            // the state is COMMITTED, in tx #0
            tx_state = stm::COMMITTED;

#ifdef PRIVATIZATION_NOFENCE
            pcount_cache = pcount;
#endif

            // for now, the acquire rule is boolean
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
            assert(tx_state == stm::COMMITTED || tx_state == stm::ABORTED);

            if (tx_state == stm::COMMITTED)
                cm.onTxCommitted();
            else
                cm.onTxAborted();

            // clean up the descriptor

            // at the end of a transaction, we are supposed to restore the
            // headers of any objects that we acquired (regardless of whether
            // the tx aborted or committed), and to apply redo logs
            cleanupLazyWrites(tx_state);
            cleanupEagerWrites(tx_state);
            // reset read set
            invisibleReads.reset();

            // commit memory changes and reset memory logging
            mm.onTxEnd(tx_state);

            // mark ourself as out of accounted time
            timing.UPDATE_TIMING(TIMING_NON_TX);

            if (tx_state == stm::COMMITTED)
                timing.TIMING_TRANSFER_COMMITTED();
            else
                timing.TIMING_TRANSFER_ABORTED();
            return tx_state == COMMITTED;
        }

        inline void Descriptor::commit()
        {
            timing.UPDATE_TIMING(TIMING_BOOKKEEPING);

            // only try to commit if we are still ACTIVE
            if (tx_state == stm::ACTIVE) {
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

                // cas status to commit; if cas fails then I've been aborted
                bool_cas(&(tx_state), ACTIVE, COMMITTED);
            }
        }

        inline void Descriptor::verifyInvisReads()
        {
            if (invisibleReads.is_empty())
                return;

            invis_bookkeep_t* e = invisibleReads.elements;

            for (unsigned long i = 0; i < invisibleReads.element_count; i++)
                if (!isCurrent(e[i].shared, e[i].version))
                    abort();
        }

        inline void Descriptor::addValidateInvisRead(SharedBase* shared,
                                                     unsigned long version)
        {
            if (invisibleReads.is_empty()) {
                invisibleReads.insert(invis_bookkeep_t(shared, version));
                return;
            }

            invis_bookkeep_t* e = invisibleReads.elements;

            for (unsigned long i = 0; i < invisibleReads.element_count; i++) {
                if (!isCurrent(e[i].shared, e[i].version))
                    abort();
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

            for (unsigned long i = 0; i < lazyWrites.element_count; i++)
                if (!isCurrent(e[i].shared, e[i].version))
                    abort();
        }

        // acquire all objects lazily opened for writing
        inline void Descriptor::acquireLazily()
        {
            if (lazyWrites.is_empty())
                return;

            lazy_bookkeep_t* e = lazyWrites.elements;

            for (unsigned long i = 0; i < lazyWrites.element_count; i++) {
                assert(e[i].isAcq == false);
                // try to acquire all objects I opened in read-write mode
                if (!acquire(e[i].shared, e[i].version, e[i].redo_log))
                    // lazy acquire failed, so we conservatively abort
                    abort();

                // record that acquire() succeeded for this object:
                e[i].isAcq = true;
            }
        }

        // after COMMIT/ABORT, cleanup headers of all objects that we
        // acquired/cloned lazily
        inline void Descriptor::cleanupLazyWrites(unsigned long tx_state)
        {
            if (lazyWrites.is_empty())
                return;

            lazy_bookkeep_t* e = lazyWrites.elements;

            if (tx_state == stm::ABORTED) {
                for (unsigned long i = 0; i < lazyWrites.element_count; i++) {
                    if (e[i].isAcq) {
                        cleanOnAbort(e[i].shared, this, e[i].redo_log);
                    }
                }
            }
            else {
                assert(tx_state == stm::COMMITTED);
                for (unsigned long i = 0; i < lazyWrites.element_count; i++) {
                    assert(e[i].isAcq);
                    cleanOnCommit(e[i].shared, this, e[i].redo_log);
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
                    cleanOnAbort(e[i].shared, this, e[i].redo_log);
            }
            else {
                assert(tx_state == stm::COMMITTED);
                for (unsigned long i = 0; i < eagerWrites.element_count; i++)
                    cleanOnCommit(e[i].shared, this, e[i].redo_log);
            }
            eagerWrites.reset();
        }

        inline void Descriptor::addDtor(SharedBase* _sh)
        {
            mm.deleteOnCommit.insert(_sh);
        }

        inline void Descriptor::addDtorUn(SharedBase* _sh)
        {
            mm.reclaimer.add(_sh);
        }

        /**
         *  utility that determines if the exptected object is still current in
         *  the SharedBase's header.
         *
         *  @param  expected  The object we're expecting to see in current.
         *  @param  tx        The transaction we expected to be the current
         *                    owner of the current header.
         */
        // check if shared.header and expected_version agree on the currently
        // valid version of the data
        inline bool Descriptor::isCurrent(SharedBase* obj,
                                          unsigned long expected_ver) const
        {
            metadata_dword_t snap;

            snap.dword = obj->m_metadata.dword;

            // if nobody has acquired this object, then the version will match
            // expected_ver
            if (snap.fields.ver.version == expected_ver)
                return true;

            // uh-oh!  It doesn't match.  If the version number is odd or 2,
            // then we must immediately fail.  If the version number is not a
            // pointer to tx, then we must fail.  If the version number is a
            // pointer to tx, then we succeed only if the redo log's version is
            // expected_ver.  So to keep it easy, let's compare the ver.owner
            // to tx...
            if (snap.fields.ver.owner != this)
                return false;

            // ok, it should be safe to dereference the redoLog
            return
                snap.fields.redoLog->m_metadata.fields.ver.version ==
                expected_ver;
        }

        /**
         *  lazily acquire /this/; this method is invoked by a txn at commit
         *  time if it is executing in lazy acquire mode; the acquisition
         *  succeeds if the valid object in /this/ has not changed, and the
         *  acquire CAS succeeds
         *
         *  @param  expected  The object we expect to see as the current header
         *  @param  newer     The object we're going to swap in.
         *  @param  tx        The current transaction descriptor.
         */
        // lazy acquire method for commit time object acquisition
        inline bool Descriptor::acquire(SharedBase* obj,
                                        unsigned long exp_ver,
                                        SharedBase* new_log)
        {
            if (!casX(&obj->m_metadata.dword, exp_ver, 0,
                      reinterpret_cast<unsigned long>(this),
                      reinterpret_cast<unsigned long>(new_log)))
                return false;

            // successfully acquired this Shared<T>
            cm.onOpenWrite();
            return true;
        }

        /**
         *  Called to "clean" the shared pointer when a transaction COMMITS
         *  changes.  In this context, "cleaning" means setting the
         *  shared->header->next pointers so that other transactions see the
         *  correct versions.
         *
         *  @param  expected    The object we expect to be current.
         *  @param  replacement The object that we are trying to swap in.
         */
        // if owner and redo log pointers match, then lock the object, apply
        // the redo log, and then cas the header to (redo_log.version, NULL)
        inline bool Descriptor::cleanOnCommit(SharedBase* obj,
                                              Descriptor* exp_owner,
                                              SharedBase* redo_log)
        {
            // try to lock the object; if we can't lock, then return false
            if (!casX(&obj->m_metadata.dword,
                      reinterpret_cast<unsigned long>(exp_owner),
                      reinterpret_cast<unsigned long>(redo_log),
                      2,
                      reinterpret_cast<unsigned long>(redo_log)))
                return false;

            // we locked the object; now apply the redo log
            obj->redo(redo_log);

            // instead of CASing to release the lock, let's just do an atomic
            // 2-word store
            metadata_dword_t replace;
            replace.fields.ver.version =
                redo_log->m_metadata.fields.ver.version + 2;
            replace.fields.redoLog = 0;
            obj->m_metadata.dword = replace.dword;
            return true;
        }

        /**
         *  Opens /this/ for reading.  If /this/ is NULL, returns NULL. If a tx
         *  opens the same Shared<T> twice, the pointers retured will be the
         *  same.
         *
         *  @param    tx  The current transaction descriptor.
         *
         *  @returns A read-only version of the SharedBase that is being
         *  shared.
         */
        inline const SharedBase*
        Descriptor::getReadable(SharedBase* obj, Validator& v)
        {
            // make sure all parameters meet our expectations
            if (!obj)
                return NULL;

            // set the validator to a null version number; change only if we
            // haven't RW'd /this/
            v.config(0, this);

            // start timing, and ensure this tx isn't aborted
            timing.UPDATE_TIMING(TIMING_BOOKKEEPING);
            verifySelf();

            // if we have a RW copy of this that we opened lazily, we must
            // return it
            SharedBase* ret = lookupLazyWrite(obj);
            if (ret) {
                timing.UPDATE_TIMING(TIMING_REALWORK);
                return ret;
            }

            while (true) {
                // read the header of /this/ to a local, opportunistically get
                // data ptr
                metadata_dword_t snap;
                unsigned long ownerState = COMMITTED;
                bool isOwned = false;

                // read the header consistently
                while (true) {
                    // read the metadata
                    snap.dword = obj->m_metadata.dword;
                    // if the version is odd, then /this/ is unowned
                    if (snap.fields.ver.version & 1 == 1) {
                        isOwned = false;
                        break;
                    }

                    // if the version is 2, then /this/ is locked
                    else if (snap.fields.ver.version == 2) {
                        timing.UPDATE_TIMING(TIMING_CM);
                        cm.onContention();
                        verifySelf();
                        timing.UPDATE_TIMING(TIMING_BOOKKEEPING);
                        continue;
                    }

                    // otherwise we need to get the owner's state and then
                    // verify consistency of the snap
                    else {
                        isOwned = true;
                        ownerState = snap.fields.ver.owner->tx_state;
                        if (obj->m_metadata.dword == snap.dword)
                            break;
                    }
                }

                // if /this/ is owned, we might have to call CM or commit a
                // redo log
                if (isOwned) {
                    // if owner is ACTIVE, either I own it (reopen) or I must
                    // call CM
                    if (ownerState == ACTIVE) {
                        if (snap.fields.ver.owner == this) {
                            cm.onReOpen();
                            timing.UPDATE_TIMING(TIMING_REALWORK);
                            return snap.fields.redoLog;
                        }

                        // we aren't the owner; if we can't abort the owner we
                        // restart (with a contention manager call).  If we can
                        // abort the owner we abort him, try to cleanup, and
                        // then restart
                        if (!cm.shouldAbort(snap.fields.ver.owner->cm.getCM())
                            || (!bool_cas(&(snap.fields.ver.owner->tx_state),
                                          ACTIVE, ABORTED)))
                        {
                            timing.UPDATE_TIMING(TIMING_CM);
                            cm.onContention();
                            verifySelf();
                            timing.UPDATE_TIMING(TIMING_BOOKKEEPING);
                            continue;
                        }

                        // now ensure that we fallthrough to the cleanOnAbort
                        // code
                        ownerState = ABORTED;
                    }

                    // if current owner is aborted use cleanOnAbort
                    if (ownerState == ABORTED) {
                        cleanOnAbort(obj, snap.fields.ver.owner,
                                     snap.fields.redoLog);
                    }
                    else {
                        // we had better be looking at a committed object
                        assert(ownerState == COMMITTED);
                        cleanOnCommit(obj, snap.fields.ver.owner,
                                      snap.fields.redoLog);
                    }
                    // restart the loop so we can get a clean version number
                    continue;
                }

                // the snapshot should have an odd version number
                assert(snap.fields.ver.version & 1 == 1);

                // set up the validator
                v.config(snap.fields.ver.version, this);

                // can cause duplicates if we don't use addValidateInvisRead()
                if (conflicts.shouldValidate()) {
                    if (conflicts.isValidatingInsertSafe()) {
                        validatingInsert(obj, snap.fields.ver.version);
                    }
                    else {
                        invisibleReads.insert(invis_bookkeep_t(obj,
                                                     snap.fields.ver.version));
                        validate();
                    }
                }
                else {
                    invisibleReads.insert(invis_bookkeep_t(obj,
                                                 snap.fields.ver.version));
                }

                verifySelf();

                // notify cm and return
                cm.onOpenRead();
                timing.UPDATE_TIMING(TIMING_REALWORK);
                return obj;
            } // end while (true)
        }

        /**
         *  Open an object in write mode. If /this/ is null, return null,
         *  otherwise return a clone of the object; if called multiple times by
         *  a tx on the same SharedBase, return same pointer each time
         *
         *  @param    tx  The current transaction descriptor.
         *
         *  @returns      A writeable version of the shared SharedBase.
         */
        inline SharedBase*
        Descriptor::getWritable(SharedBase* obj, Validator& v)
        {
            // make sure all parameters meet our expectations
            if (!obj)
                return NULL;

            // set the validator to a null version number
            v.config(0, this);

            // start timing now, and ensure this tx isn't aborted
            timing.UPDATE_TIMING(TIMING_BOOKKEEPING);
            verifySelf();

            // make sure that our conflict detection strategy knows we've got a
            // write
            conflicts.onRW();

            // if we have an RW copy of this that we opened lazily, return it
            SharedBase* ret = lookupLazyWrite(obj);
            if (ret) {
                timing.UPDATE_TIMING(TIMING_REALWORK);
                return ret;
            }

            while (true) {
                // read the header of /this/ to a local, opportunistically get
                // data ptr
                metadata_dword_t snap;
                unsigned long ownerState = COMMITTED;
                bool isOwned = false;

                // read the header consistently
                while (true) {
                    // read the metadata
                    snap.dword = obj->m_metadata.dword;
                    // if the version is odd, then /this/ is unowned
                    if (snap.fields.ver.version & 1 == 1) {
                        isOwned = false;
                        break;
                    }

                    // if the version is 2, then /this/ is locked
                    else if (snap.fields.ver.version == 2) {
                        timing.UPDATE_TIMING(TIMING_CM);
                        cm.onContention();
                        verifySelf();
                        timing.UPDATE_TIMING(TIMING_BOOKKEEPING);
                        continue;
                    }

                    // otherwise we need to get the owner's state and then
                    // verify consistency of the snap
                    else {
                        isOwned = true;
                        ownerState = snap.fields.ver.owner->tx_state;
                        if (obj->m_metadata.dword == snap.dword)
                            break;
                    }
                }

                // if /this/ is owned, we might have to call CM or commit a
                // redo log
                if (isOwned) {
                    // if owner is ACTIVE, either I own it (reopen) or I must
                    // call CM
                    if (ownerState == ACTIVE) {
                        if (snap.fields.ver.owner == this) {
                            cm.onReOpen();
                            timing.UPDATE_TIMING(TIMING_REALWORK);
                            return snap.fields.redoLog;
                        }
                        // we aren't the owner; if we can't abort the owner we
                        // restart (with a contention manager call).  If we can
                        // abort the owner we abort him, try to cleanup, and
                        // then restart
                        if (!cm.shouldAbort(snap.fields.ver.owner->cm.getCM())
                            || !bool_cas(&(snap.fields.ver.owner->tx_state),
                                         ACTIVE, ABORTED))
                        {
                            timing.UPDATE_TIMING(TIMING_CM);
                            cm.onContention();
                            verifySelf();
                            timing.UPDATE_TIMING(TIMING_BOOKKEEPING);
                            continue;
                        }

                        // now ensure that we fallthrough to the cleanOnAbort
                        // code
                        ownerState = ABORTED;
                    }

                    // if current owner is aborted use cleanOnAbort
                    if (ownerState == ABORTED) {
                        cleanOnAbort(obj, snap.fields.ver.owner,
                                     snap.fields.redoLog);
                    }
                    else {
                        // we had better be looking at a committed object
                        assert(ownerState == COMMITTED);
                        cleanOnCommit(obj, snap.fields.ver.owner,
                                      snap.fields.redoLog);
                    }
                    // restart loop so we can get a clean version number
                    continue;
                }

                // the snapshot should have an odd version number
                assert(snap.fields.ver.version & 1 == 1);

                // clone the object and make me the owner of the new version
                timing.UPDATE_TIMING(TIMING_COPY);

                SharedBase* new_version = obj->clone();

                assert(new_version);
                // redoLog of clone is a backpointer to the parent
                new_version->m_metadata.fields.redoLog = obj;
                // version# of the clone is the version# of the parent
                new_version->m_metadata.fields.ver.version =
                    snap.fields.ver.version;

                timing.UPDATE_TIMING(TIMING_BOOKKEEPING);

                if (isLazy) {
                    // LAZY: just add /this/ to my lazy writeset... don't
                    // acquire
                    lazyWrites.insert(lazy_bookkeep_t(obj, new_version,
                                                     snap.fields.ver.version));
                }
                else {
                    // EAGER: CAS in a new header, retry open_RW on failure
                    if (!acquire(obj, snap.fields.ver.version, new_version)) {
                        mm.deleteOnCommit.insert(new_version);
                        new_version = NULL;
                        timing.UPDATE_TIMING(TIMING_CM);
                        cm.onContention();
                        verifySelf();
                        continue;
                    }

                    // CAS succeeded: bookkeep the eager write
                    eagerWrites.insert(eager_bookkeep_t(obj, new_version));
                }

                // The redoLog is already deleteOnAbort; mark it deleteOnCommit
                // too, since it will always be deleted once this tx is
                // finished
                mm.deleteOnCommit.insert(new_version);

                // Validate, notify cm, reset timing, and return
                if (conflicts.shouldValidate()) {
                    validate();
                }

                verifySelf();
                // don't need this call, since we openWrite only when we
                // acquire...
                cm.onOpenWrite();
                timing.UPDATE_TIMING(TIMING_REALWORK);
                return new_version;
            } // end while (true)
        }

        // make sure that this tx didn't upgrade sh from obj to something new
        inline bool Descriptor::ensure_no_upgrade(const SharedBase* sh,
                                                  const SharedBase* obj)
        {
            // if there's no sh, obviously we're done
            if (sh == NULL)
                return true;

            // if the sh is the t, then we're all good too
            if (sh == obj)
                return true;

            // read the header of /sh/ to a local
            metadata_dword_t snap;

            // read the metadata
            snap.dword = sh->m_metadata.dword;
            // if the version is odd, then /this/ is unowned.  since it isn't
            // our t, we must have been aborted.  That's fine, just return
            // true.
            if (snap.fields.ver.version & 1 == 1) {
                return true;
            }

            // if the version is 2, then /this/ is locked, which is fine too
            else if (snap.fields.ver.version == 2) {
                return true;
            }

            // otherwise we have a problem if /this/ is the owner and obj !=
            // sh->redo_log

            if ((snap.fields.ver.owner == this) &&
                (obj != snap.fields.redoLog))
            {
                return false;
            }
            return true;
        }

        /**
         *  Opens /this/ unsafely.
         *
         *  @returns A writable version of the ObjectBase that is being shared.
         */
        inline SharedBase* Descriptor::open_un(SharedBase* obj)
        {
#ifdef PRIVATIZATION_TFENCE
            return obj;
#else
            if (obj == NULL)
                return obj;

            while (true) {
                // read the header of /this/ to a local, opportunistically get
                // data ptr
                metadata_dword_t snap;
                unsigned long ownerState = COMMITTED;
                bool isOwned = false;

                // read the header consistently
                while (true) {
                    // read the metadata
                    snap.dword = obj->m_metadata.dword;
                    // if the version is odd, then /this/ is unowned
                    if (snap.fields.ver.version & 1 == 1) {
                        isOwned = false;
                        break;
                    }

                    // if the version is 2, then /this/ is locked so spin a bit
                    // and retry
                    else if (snap.fields.ver.version == 2) {
                        for (int i = 0; i < 128; i++)
                            nop();
                        continue;
                    }

                    // otherwise we need to get the owner's state and then
                    // verify consistency of the snap
                    else {
                        isOwned = true;
                        ownerState = snap.fields.ver.owner->tx_state;
                        if (obj->m_metadata.dword == snap.dword)
                            break;
                    }
                }

                // if /this/ is owned, kill the owner
                if (isOwned) {
                    // if owner is ACTIVE, abort him
                    if (ownerState == ACTIVE) {
                        // if abort fails, restart loop
                        if (!bool_cas(&(snap.fields.ver.owner->tx_state),
                                      ACTIVE, ABORTED))
                        {
                            continue;
                        }

                        // ensure that we fallthrough to the cleanOnAbort code
                        ownerState = ABORTED;
                    }

                    // if current owner is aborted use cleanOnAbort
                    if (ownerState == ABORTED) {
                        cleanOnAbort(obj, snap.fields.ver.owner,
                                     snap.fields.redoLog);
                    }
                    else {
                        // we had better be looking at a committed object
                        assert(ownerState == COMMITTED);
                        cleanOnCommit(obj, snap.fields.ver.owner,
                                      snap.fields.redoLog);
                    }
                    // restart the loop so we can get a clean version number
                    continue;
                }

                // the snapshot should have an odd version number
                assert(snap.fields.ver.version & 1 == 1);

                // object is clean.  return /this/
                return obj;
            }
#endif
        }

        /**
         *  Called to "clean" the shared pointer when a transaction ABORTS. In
         *  this context, "cleaning" means setting the shared->header->next
         *  pointers so that the correct version is current, and seen by other
         *  transactions.
         *
         *  @param expected The object we expect to be current.  @param
         *  replacement The object that we are trying to swap in.
         */
        // to clean on abort, we need to CASX from (owner, log) to (log->ver,
        // NULL)
        inline bool Descriptor::cleanOnAbort(SharedBase* obj,
                                             Descriptor* exp_owner,
                                             SharedBase* exp_log)
        {
            return casX(&obj->m_metadata.dword,
                        reinterpret_cast<unsigned long>(exp_owner),
                        reinterpret_cast<unsigned long>(exp_log),
                        exp_log->m_metadata.fields.ver.version, 0);
        }

        inline void Descriptor::release(SharedBase* obj)
        {
            timing.UPDATE_TIMING(TIMING_BOOKKEEPING);

            // NB: no need to check if I own this in my eager or lazy writeset;
            // that's orthogonal to whether it's in my read set

            // invis reader:  just remove /this/ from my invis read set
            removeInvisRead(obj);

            timing.UPDATE_TIMING(TIMING_REALWORK);
        }
    } // namespace stm::internal
} // namespace stm

#endif // __DESCRIPTOR_H__
