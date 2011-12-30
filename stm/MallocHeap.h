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

#ifndef __MALLOCHEAP_H__
#define __MALLOCHEAP_H__

#include <cstdlib>
#include "stm_mm.h"
namespace stm
{
    namespace mm
    {
        inline void* alignedMalloc(size_t size)
        {
            return malloc(GetAlignedSize(size, 0));
        }

        class TxHeap
        {
            unsigned long id;
          public:
            TxHeap(int _id);
            void* tx_alloc(size_t size) { return alignedMalloc(size); }
            void tx_free(void* mem) { free(mem); }
        };

        // NB: this must be backed in mm.cpp
        extern TxHeap* heaps[stm::MAX_THREADS];

        // use TLS to get the GCHeap object for the current thread
        inline TxHeap* getHeap()
        {
            // get the data object's address with TLS
            TxHeap* heap = heaps[stm::idManager.getThreadId()];
            assert(heap);
            return heap;
        }

        inline TxHeap::TxHeap(int _id) :id(_id) { heaps[_id] = this; }
        inline void txfree(void* mem) { if (mem) free(mem); }
        inline void* txalloc(size_t size) { return alignedMalloc(size); }
    } // stm::mm
} // stm

#endif // __MALLOCHEAP_H__
