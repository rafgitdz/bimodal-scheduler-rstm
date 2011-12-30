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

#ifndef __STM_COMMON_H__
#define __STM_COMMON_H__

#include "IdManager.h"

// the following bits are common to all implementations of the RSTM API
namespace stm
{
    /// print the build information for this library
    void about();

    /**
     *  we'll set a limit for the number of transactional threads, since we're
     *  going to store them in an array
     */
    static const int MAX_THREADS = 256;

    /**
     *  When a tx needs to abort, it will throw this.
     */
    class Aborted { };

    namespace internal
    {
        class Descriptor;

        // create a global table for all Descriptor objects
        // NB:  this array must be backed by a declaration in stm.cpp
        extern internal::Descriptor* desc_array[MAX_THREADS];
    }


    /**
     *  Transactions must be in one of these four states.
     *
     *  NB:  FINISHING is not currently used
     */
    enum TxState { ACTIVE = 0, ABORTED = 1, FINISHING = 2, COMMITTED = 3 };

    /**
     *  back this in stm.cpp and initialize it to false: if terminate_stm is
     *  true, then an aborted transaction will not retry.  This lets us stop
     *  the livelocks that plague RandomGraph invis eager
     */
    extern bool terminate;

    /**
     *  Shut down the current transaction in each thread, so that we can halt a
     *  livelocked benchmark.
     */
    inline void halt_all_transactions() { terminate = true; }
};

#endif // __STM_COMMON_H__
