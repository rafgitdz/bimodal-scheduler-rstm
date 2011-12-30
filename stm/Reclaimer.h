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

#ifndef __RECLAIMER_H__
#define __RECLAIMER_H__

#include "stm_mm.h"
#include "CustomAllocedBase.h"
#include "Epoch.h"

namespace stm
{
    /**
     *  Define how many 'deleted' objects will be tied to a single timestamp
     */
    static const unsigned long POOL_SIZE = 32;

    // forward declarations
    namespace mm
    {
        class CustomAllocedBase;
    }

    /**
     * List Node for the limbo list.  The reclaimer object contains a limbo
     * list, where each node has an array of deleted objects (the pool) and an
     * array of thread tx states at some point in the past (the timestamp).
     */
    struct limbo_t
    {
        /**
         *  Deleted objects go into this array.
         */
        mm::CustomAllocedBase* pool[POOL_SIZE];

        /**
         *  Timestamp array (taken from the Epoch)
         */
        unsigned long* ts;

        /**
         *  Size of the timestamp array if the pool is full, or number of
         *  elements in the pool if the pool is not yet full and the timestamp
         *  is not yet allocated.
         */
        unsigned long  length;

        /**
         *  Next pointer for the limbo list
         */
        limbo_t*       older;           // next limbo list

        /**
         *  The constructor for the limbo_t just zeroes out everything.
         */
        limbo_t() : ts(NULL), length(0), older(NULL) { }
    };

    /**
     *  The reclaimer consists of two limbo lists.  The first has one node and
     *  is partially filled.  As things get added to the reclaimer we put them
     *  into this node.  Once the node is filled, we get a timestamp for it and
     *  add it to the front of the limbo list, and sweep the limbo list for
     *  dominated timestamps while we're at it.
     */
    class Reclaimer
    {
        /**
         *  As we delete things, we accumulate them here
         */
        limbo_t* prelimbo;

        /**
         * once prelimbo is full, it moves here
         */
        limbo_t* limbo;

        /**
         *  Cache the allocator so that we can get fast MM calls
         */
        stm::mm::TxHeap* heap;

      public:
        /**
         *  Construct a reclaimer by giving it a heap and allocating a prelimbo
         *  node.
         */
        Reclaimer(stm::mm::TxHeap* gcd) : limbo(NULL), heap(gcd)
        {
            prelimbo = new (heap->tx_alloc(sizeof(limbo_t))) limbo_t();
        }

        /**
         *  To delete an object, use 'add' to add it to the reclaimer's
         *  responsibility.  The reclaimer will figure out when it's safe to
         *  call the object's destructor and operator delete().  This method
         *  simply 'adds' the element to the prelimbo list and then, depending
         *  on the state of the prelimbo, may transfer the prelimbo to the head
         *  of the limbo list.
         */
        void add(mm::CustomAllocedBase* ptr);

      private:
        /**
         *  When the prelimbo list is full, transfer it to the head of the
         *  limbo list, and then reclaim any limbo list entries that are
         *  strictly dominated by the timestamp we gave to the list head.
         */
        void transfer();
    };
};

inline void stm::Reclaimer::add(mm::CustomAllocedBase* ptr)
{
    // insert /ptr/ into the pool and increment the pool size
    prelimbo->pool[prelimbo->length] = ptr;
    prelimbo->length++;

    // check if we need to transfer this prelimbo node onto the limbo list
    if (prelimbo->length == POOL_SIZE) {
        // if so, call transfer() and then make a new prelimbo node.
        transfer();
        prelimbo = new (heap->tx_alloc(sizeof(limbo_t))) limbo_t();
    }
}

inline void stm::Reclaimer::transfer()
{
    // we're going to move prelimbo to the head of the limbo list, and then
    // clean up anything on the limbo list that has become dominated

    // first get memory and create an empty timestamp
    prelimbo->length = stm::idManager.getThreadCount();
    prelimbo->ts = (unsigned long*)
        heap->tx_alloc(prelimbo->length * sizeof(unsigned long));
    assert(prelimbo->ts);

    // now get the current timestamp from the epoch.
    globalEpoch.copy_timestamp(prelimbo->ts, prelimbo->length);

    // push prelimbo onto the front of the limbo list:
    prelimbo->older = limbo;
    limbo = prelimbo;

    // loop through everything after limbo->head, comparing the timestamp to
    // the head's timestamp.  Exit the loop when the list is empty, or when we
    // find something that is strictly dominated.  NB: the list is in sorted
    // order by timestamp.
    limbo_t* current = limbo->older;
    limbo_t* prev = limbo;
    while (current != NULL) {
        if (globalEpoch.isStrictlyOlder(limbo->ts, current->ts,
                                        current->length))
            break;
        else {
            prev = current;
            current = current->older;
        }
    }

    // If current != NULL, then it is the head of the kill list
    if (current) {
        // detach /current/ from the list
        prev->older = NULL;

        // for each node in the list headed by current, delete all blocks in
        // the node's pool, delete the node's timestamp, and delete the node
        while (current != NULL) {
            // free blocks in current's pool
            for (unsigned long i = 0; i < POOL_SIZE; i++) {
                delete(current->pool[i]);
            }

            // free the timestamp
            heap->tx_free(current->ts);

            // free the node and move on
            limbo_t* old = current;
            current = current->older;
            heap->tx_free(old);
        }
    }
}

#endif // __RECLAIMER_H__
