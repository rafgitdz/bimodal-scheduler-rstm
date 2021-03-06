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

#ifndef __GCHEAP_H__
#define __GCHEAP_H__

#include <cstdlib>
#include <cassert>
#include <new>                  // necessary for the "placement new" operator
#include "stm_common.h"

namespace stm
{
    namespace mm
    {
        // this is the pointer to the top of the metadata within a block
        typedef unsigned char* block_ptr_t;

        // The header is 8 bytes
        const int BLOCK_HEADER_SIZE = sizeof(block_ptr_t) +
                                      sizeof(unsigned long);

        // given a block pointer, return the block pointer of the next item in
        // the list
        inline block_ptr_t get_next_bptr(block_ptr_t block)
        {
            return (block_ptr_t)(*((int*)block));
        }

        // set block's next pointer to next
        inline void set_next_bptr(block_ptr_t block, block_ptr_t next)
        {
            *((unsigned long*)block) = (unsigned long)next;
        }

        // get the block pointer for a payload pointer
        inline block_ptr_t get_bptr(void* payload)
        {
            return (block_ptr_t)((unsigned long)payload - BLOCK_HEADER_SIZE);
        }

        // get the payload pointer for a block pointer
        inline void* get_payload(block_ptr_t block)
        {
            return (void*)((unsigned long)block + BLOCK_HEADER_SIZE);
        }

        // get a bool flag for whether the block is in use or not
        // (deleted block) === (lsb of size == 1)
        inline bool isInUse(block_ptr_t block)
        {
            return (0 == (1&(*((unsigned long*)((unsigned long)block + 4)))));
        }

        // given a payload pointer, set the deleted flag
        inline void set_payload_deleted(void* ptr)
        {
            unsigned int* sizeptr = (unsigned int*)((unsigned long)ptr - 4);
            assert((*sizeptr & 1) == 0);
            *sizeptr = *sizeptr | 1;
        }

        // get the size from the block header
        inline unsigned long get_block_size(block_ptr_t block)
        {
            return (*((unsigned long*)((unsigned long)block + 4))) &
                (0xFFFFFFFC);
        }

        inline void initialize_bptr(block_ptr_t begin_addr, size_t size)
        {
            // set next to "NULL"
            *((int*)begin_addr) = 0;

            // set size field, with bit0 = bit1 = 0
            *((int*)(begin_addr+4)) = size & 0xFFFFFFFC;
        }

        // the amount of memory for each thread's heap
        static const unsigned long int SLAB_SIZE = 1 * 1024 * 1024;

        // how many homogeneous freelists will we have
        static const int SMALLBLOCK_SIZES = 6;

        // number of generations in the gc
        static const unsigned long GENERATIONS = 4;

        // thresholds for sweeping the allocated memory lists to find dead
        // blocks
        static const int NURSERY_THRESH    = 64; // clean after 64 block adds
        static const int GENERATION_THRESH = 8;  // clean after 8 << generation
                                                 // list adds

        // define a type for lists of allocated objects
        struct block_list_t
        {
            // store head and tail for fast insertions
            block_ptr_t head;
            block_ptr_t tail;

            // every list has an inspection rule:
            int metric;             // iterations since last rule check
            int metric_thresh;      // threshold for iterations between checks

            block_list_t()
                : head(NULL), tail(NULL), metric(0), metric_thresh(0) { }
        };

        // mark data as ready for lazy reclamation
        inline void gcdelete(void* mem) { if (mem) set_payload_deleted(mem); }

        class TxHeap
        {
            // this private block describes how we maintain freelists (aka
            // "FreeStore")
          private:
            // here's the big hunk of memory we initially get, represented as
            // 32-bit begin and end addresses
            unsigned long slab_start, slab_end;

            // homogeneous lists of small memory blocks
            block_ptr_t small_blocks[SMALLBLOCK_SIZES];

            // hetergeneous list of large memory blocks
            block_ptr_t big_blocks;

            // allocate memory
            block_ptr_t getBlock(size_t size);

            // return memory to the allocator
            void returnBlock(block_ptr_t block_list);

            // this private block describes how we find dead objects (aka
            // "DeadFinder")
          private:
            // generations of objects are stored here
            block_list_t nursery;
            block_list_t gc_generations[GENERATIONS];

            // manage generations
            void generation_add_list(block_list_t* list,
                                     unsigned long generation);
            void sweep(block_list_t* block_list);
            void nursery_clean();

            void nursery_add(block_ptr_t block);

            // this private block is for stuff that the public interface
            // requires
          private:
            unsigned long id;

            // and here's all that a TxHeap must expose
          public:
            TxHeap(int id);
            void* tx_alloc(size_t size);
            void tx_free(void* mem) { gcdelete(mem); }
        };

        // just for convenience, we're going to keep a global table of the
        // TxHeaps so that a tls call that returns an ID is all it takes to get
        // your TxHeap, without knowing anything about Descriptors...

        // NB: this must be backed in mm.cpp
        extern TxHeap* heaps[stm::MAX_THREADS];

        // use TLS to get the GCHeap object for the current thread
        // NB: if we don't bother with accounting for the pthread call, we can
        // inline this!
        inline TxHeap* getHeap()
        {
            // get the data object's address with TLS
            TxHeap* heap = heaps[stm::idManager.getThreadId()];
            assert(heap);
            return heap;
        }

        inline void* txalloc(size_t size) { return getHeap()->tx_alloc(size); }
        inline void txfree(void* mem) { gcdelete(mem); }

        // set up the standard sizes for freelists:
        // index : range       index : range       index : range
        //     0 :   0 ->   64     1 :  65 ->  128     2 :  129 ->  256
        //     3 : 257 ->  512     4 : 513 -> 1024     5 : 1025 -> 2048
        inline int smallbucket_map(size_t size)
        {
            // For rtm_hw, we need 64 byte aligned blocks.
            if      (size <=  64)  return 0;
            else if (size <= 128)  return 1;
            else if (size <= 256)  return 2;
            else if (size <= 512)  return 3;
            else if (size <= 1024) return 4;
            else if (size <= 2048) return 5;
            else return -1;
        }
    } // stm::mm
} // stm

// return a pointer to a hunk of memory; ensure that the pointer's metadata is
// set s.t. size is correct, next pointer is NULL
inline stm::mm::block_ptr_t stm::mm::TxHeap::getBlock(size_t size)
{
    // translate the requested size into a standard size
    size_t new_size = GetAlignedSize(size, BLOCK_HEADER_SIZE);

    // First case:  can we get memory from a smallbucket?
    int bucket_index = smallbucket_map(new_size);
    if ((bucket_index != -1) && (small_blocks[bucket_index] != NULL)) {
        block_ptr_t new_block = small_blocks[bucket_index];
        small_blocks[bucket_index] = get_next_bptr(new_block);
        // NB:  the size is already set, just set the next ptr
        set_next_bptr(new_block, NULL);
        assert(isInUse(new_block));
        return new_block;
    }

    // Second case: we might be able to get memory from the bigbucket
    if (bucket_index == -1) {
        block_ptr_t last = NULL;
        block_ptr_t next = big_blocks;
        while (next != NULL) {
            if (get_block_size(next) >= new_size) {
                if (last)
                    set_next_bptr(last, get_next_bptr(next));
                else
                    big_blocks = get_next_bptr(next);
                // NB:  the size should already be set, just set the next ptr
                set_next_bptr(next, NULL);
                assert(isInUse(next));
                return next;
            }
            // advance to next entry in bigbucket
            last = next;
            next = get_next_bptr(next);
        }
    }

    // Third case:  we can get memory from the slab
    if (slab_start + new_size < slab_end) {
        block_ptr_t new_block = (block_ptr_t)slab_start;
        slab_start += new_size;
        // set the size and the next ptr
        initialize_bptr(new_block, new_size);
        assert(isInUse(new_block));
        return new_block;
    }

    // Fourth case: we need to expand the heap by getting a new slab

    // first, let's turn the existing slab into an entry on the big_blocks list
    // [?] we intentionally leak the slab if it is as small or smaller than the
    // smallest block size
    if (slab_end > slab_start + 64) {
        unsigned long oldslab_size = slab_end - slab_start;
        block_ptr_t oldslab = (block_ptr_t)slab_start;
        set_next_bptr(oldslab, NULL);
        initialize_bptr(oldslab, oldslab_size);
        returnBlock(oldslab);
    }

    // grab a hunk of memory:
    slab_start = (unsigned long)malloc(SLAB_SIZE);
    assert(slab_start);
    slab_end = slab_start + SLAB_SIZE;

    block_ptr_t new_block = (block_ptr_t)slab_start;
    slab_start += new_size;
    // set the size and the next ptr
    initialize_bptr(new_block, new_size);
    assert(new_block);
    return new_block;

}

// give a block back to the free store
inline void stm::mm::TxHeap::returnBlock(block_ptr_t block)
{
    assert(block);

    // figure out what bucket the block goes onto
    unsigned long size = get_block_size(block);
    int index = smallbucket_map(size);

    initialize_bptr(block, size);

    if (index != -1) {
        set_next_bptr(block, small_blocks[index]);
        small_blocks[index] = block;
    }
    else {
        set_next_bptr(block, big_blocks);
        big_blocks = block;
    }
}

// add a block to the nursery
inline void stm::mm::TxHeap::nursery_add(block_ptr_t block)
{
    nursery.metric++;

    if (nursery.metric == nursery.metric_thresh)
        nursery_clean();

    // add block to begining of nursery
    set_next_bptr(block, nursery.head);
    nursery.head = block;
    if (nursery.tail == NULL)
        nursery.tail = nursery.head;
}

// sweep the nursery
inline void stm::mm::TxHeap::nursery_clean()
{
    // remove deleted blocks
    sweep(&nursery);

    // promote the rest to the first generation
    generation_add_list(&nursery, 0);

    // reset nursery age and size
    nursery.metric = 0;
}

// add the "keep set" from a sweep() into the next generation
inline void stm::mm::TxHeap::generation_add_list(block_list_t* swept_list,
                                                 unsigned long gen)
{
    // if the list is empty, just return
    if (swept_list->head == NULL)
        return;

    // if this list is due for a sweep, sweep it
    if (++gc_generations[gen].metric >= gc_generations[gen].metric_thresh) {
        sweep(&gc_generations[gen]);

        // if this is not the oldest generation, then promote the rest of this
        // list
        if ((gen + 1) < GENERATIONS)
            generation_add_list(&gc_generations[gen], gen+1);

        // now reset this generation's metric
        gc_generations[gen].metric = 0;
    }

    // since we have the head and tail of each list, this should be easy!
    set_next_bptr(swept_list->tail, gc_generations[gen].head);
    gc_generations[gen].head = swept_list->head;

    swept_list->head = NULL;
    swept_list->tail = NULL;
}

/**
 *  Remove all deleted blocks from a list of allocated blocks.  The deleted
 *  blocks are immediately given back using returnBlock.  All of the live
 *  blocks are returned since we update the input parameter (a pointer)
 *  directly.
 *
 *  We might be able to optimize this by making modifications to the list in
 *  place.
 *
 *  @param   list   a 'generation' of allocated blocks in the gc.
 */
inline void stm::mm::TxHeap::sweep(block_list_t* list)
{
    block_ptr_t live_head  = NULL;
    block_ptr_t live_tail  = NULL;

    block_ptr_t ptr = list->head;
    block_ptr_t nxt = NULL;

    while (ptr != NULL) {
        nxt = get_next_bptr(ptr);

        if (isInUse(ptr)) {
            set_next_bptr(ptr, live_head);
            live_head = ptr;
            if (live_tail == NULL)
                live_tail = live_head;
        }
        else {
            returnBlock(ptr);
        }
        ptr = nxt;
    }

    list->head = live_head;
    list->tail = live_tail;
}

// constructor
inline stm::mm::TxHeap::TxHeap(int _id)
{
    id = _id;

    // initialize the freestore

    // grab a hunk of memory:
    slab_start = (unsigned long)malloc(SLAB_SIZE);
    assert(slab_start);
    slab_end = slab_start + SLAB_SIZE;

    // set up the small homogeneous lists
    for (int i = 0; i < SMALLBLOCK_SIZES; i++)
        small_blocks[i] = NULL;

    // set up the big hetergeneous list
    big_blocks = NULL;

    // initialize the deadfinder (nursery and generations):
    nursery.metric_thresh = NURSERY_THRESH;
    for (unsigned long i = 0; i < GENERATIONS; i++)
        gc_generations[i].metric_thresh = GENERATION_THRESH << i;

    // now put self into the global heap table
    heaps[_id] = this;
}

inline void* stm::mm::TxHeap::tx_alloc(size_t size)
{
    // get memory through the standard interface
    block_ptr_t new_block = getBlock(size);

    // add the new block to the nursery
    nursery_add(new_block);

    void* block = get_payload(new_block);
    assert(block);

    return block;
}

#endif // __GCHEAP_H__
