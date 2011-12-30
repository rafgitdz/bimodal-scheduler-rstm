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

#ifndef __MM_H__
#define __MM_H__

#include "stm_common.h"

namespace stm
{
    namespace mm
    {
        // all block sizes must be multiples of 64 in order to ensure alignment
        // of longlong variables and to ensure that we don't have
        // cacheline-based conflicts.  Also, we're going to make all sizes upto
        // 2048 be exact powers of 2
        inline size_t GetAlignedSize(size_t initial_size, size_t padding)
        {
            // add the header size to get the minimum contiguous block
            size_t aligned_size = initial_size + padding;

            // pad everything to a multiple of 8 in case size > 1024
            if (aligned_size % 64 != 0)
                aligned_size += 64 - (aligned_size % 64);

            if      (aligned_size > 2048) return aligned_size;
            else if (aligned_size > 1024) return 2048;
            else if (aligned_size >  512) return 1024;
            else if (aligned_size >  256) return 512;
            else if (aligned_size >  128) return 256;
            else if (aligned_size >   64) return 128;
            else return 64;
        }

    };
};

#ifdef ALLOCATION_GCHEAP
#include "GCHeap.h"
#else
#include "MallocHeap.h"
#endif

#include "StlAllocator.h"

#endif // __MM_H__




