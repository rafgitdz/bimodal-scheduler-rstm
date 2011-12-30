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

#ifndef __EPOCH_H__
#define __EPOCH_H__

#include "stm_common.h"

namespace stm
{
    /**
     *  The Epoch is just a global timestamp array.  Each transaction's
     *  timestamp is in its own cache line, to limit cache misses.
     */
    class Epoch
    {
        /**
         *  Store the current transaction number for each active thread.
         */
        unsigned long* trans_nums;

      public:
        /**
         *  ctor ... will only be called at elabortation time.  Allocate enough
         *  space for MAX_THREADS * CACHE_LINE_SIZE
         */
        Epoch() { trans_nums = new unsigned long[stm::MAX_THREADS*16]; }

        /**
         *  figure out if one timestamp is strictly dominated by another
         */
        bool isStrictlyOlder(unsigned long* newer, unsigned long* older,
                             unsigned long old_len)
        {
            for (unsigned long i = 0; i < old_len; i++)
                if ((newer[i] <= older[i]) && (newer[i] % 2 == 1))
                    return false;
            return true;
        }

        /**
         * method to copy a timestamp
         */
        void copy_timestamp(unsigned long* ts_ptr, unsigned long length)
        {
            // can't replace with memcopy() since not word-for-word copy
            for (unsigned long i = 0; i < length; i++)
                ts_ptr[i] = trans_nums[i*16];
        }

        /**
         *  Update the epoch to note that the current thread is entering a
         *  transaction
         *
         *  @param  id  id of the tx who is making the call
         */
        void enter_transaction(int id) { trans_nums[id*16]++; }

        /**
         *  Right before we do a validation inside of a transaction, there is a
         *  guarantee that if our validation succeeds, then we aren't looking
         *  at anything that has been deleted correctly.  Consequently, we can
         *  safely increment our timestamp, so that long-running transactions
         *  <i>that are still valid and that are not preempted</i> do not
         *  impede the collection activities of other threads.
         *
         *  NB: We need to increment by 2, so that we don't give the mistaken
         *  impression that we are out of a transaction.
         *
         *  @param  id  id of the tx who is making the call
         */
        void on_validation(int id) { trans_nums[id*16] += 2; }

        /**
         *  Update the gc thread list to note that the current thread is
         *  leaving a transaction
         *
         *  @param  id  id of the tx who is making the call
         */
        void exit_transaction(int id)  { trans_nums[id*16]++; }

        /**
         *  Null out the timestamp for a particular thread.  We only call this
         *  at initialization.
         *
         *  @param  id  id of the tx who is making the call
         */
        void zero_transaction(int id)  { trans_nums[id*16] = 0; }

        /**
         * Block until every transaction has either committed, aborted, or
         * validated.
         */
        void waitForDominatingEpoch();

    } __attribute__ ((aligned(64))) ;

    /**
     *  Declare a single global object of this type.
     */
    extern Epoch globalEpoch;

}; // namespace stm

#endif // __EPOCH_H__
