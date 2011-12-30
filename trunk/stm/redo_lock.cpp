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

#include "redo_lock.h"
#include "Descriptor_redo_lock.h"

/**
 *  Provide backing for the descriptor array declared in stm_common.
 */
stm::internal::Descriptor*
stm::internal::desc_array[MAX_THREADS] __attribute__ ((aligned(64))) = {0};

/**
 *  Provide backing for the terminate flag declared in stm_common.
 */
bool stm::terminate __attribute__ ((aligned(64))) = false;

#ifdef VALIDATION_HEURISTICS
/**
 *  Ensure that the ValidationPolicy statics are backed
 */
volatile unsigned long
stm::GlobalCommitCounterValidationPolicy::global_counter = 0;
#endif

#ifdef PRIVATIZATION_NOFENCE
/**
 *  Provide backing for the privatization counter, if we're using nonblocking
 *  privatization.
 */
volatile unsigned long stm::internal::pcount = 0;
#endif

/**
 *  Perform the 'transactional fence'.  If we're using a Validation fence or
 *  Transactional fence, then globalEpoch.waitForDominatingEpoch() will do all
 *  the work (its behavior varies based on compile flags).  If we're doing
 *  obstruction-free privatization, then simply increment the global
 *  privatization counter.
 */
void stm::fence()
{
#ifdef PRIVATIZATION_NOFENCE
    fai(&internal::pcount);
#else
    globalEpoch.waitForDominatingEpoch();
#endif
}
