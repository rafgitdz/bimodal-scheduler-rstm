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

#include "Epoch.h"
#include "stm_mm.h"
#include "atomic_ops.h"
#include <cassert>

namespace stm
{
    /**
     *  Epoch.h will extern to this instance of Epoch.
     */
    Epoch globalEpoch = Epoch();
}

void stm::Epoch::waitForDominatingEpoch()
{
    // However many threads exist RIGHT NOW is the number of threads to wait
    // on.  Future arrivals don't matter.
    unsigned long numThreads = stm::idManager.getThreadCount();

    // get memory that is big enough to hold a timestamp for each active thread
    unsigned long* ts =
        (unsigned long*)stm::mm::txalloc(numThreads * sizeof(unsigned long));

    // make sure the alloc worked
    assert(ts);

    // copy the global epoch into ts
    copy_timestamp(ts, numThreads);

    // now iterate through ts, and spin on any element that is odd and still
    // matches the global epoch
    unsigned long currentThread = 0;
    while (currentThread < numThreads) {
        // if current entry in ts is odd we have to spin until it has
        // incremented
        if (ts[currentThread] & 1 == 1) {
            // get the global value
            unsigned long currEntry =
                trans_nums[currentThread*16];
            if (currEntry == ts[currentThread]) {
                // spin a bit
                for (int i = 0; i < 128; i++)
                    nop();

                // retry
                continue;
            }
        }
        // ok, ready to move to next element
        currentThread++;
    }

    // fence is complete.  Free the memory we allocated before for the
    // timestamp.
    stm::mm::txfree(ts);
}
