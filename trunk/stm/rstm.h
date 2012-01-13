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

#ifndef __STM_H__
#define __STM_H__

#include <string>

#include "stm_common.h"
#include "instrumentation.h"
#include "Descriptor_rstm.h"

#include "Shared_rstm.h"
#include "Object_rstm.h"
#include "macros.h"
#include "accessors.h"


#ifndef USE_BIMODAL
#define USE_BIMODAL
#endif

namespace stm
{
    /**
     *  Create a thread's transactional context.  This must be called before
     *  doing any transactions.  Since this function creates the thread's
     *  private allocator, this must be called before doing any allocations of
     *  memory that will become visible to other transactions, too.
     *
     * @param cm_type - string indicating what CM to use.
     *
     * @param validation - string indicating vis-eager, vis-lazy, invis-eager,
     * or invis-lazy
     *
     * @param use_static_cm - if true, use a statically allocated contention
     * manager where all calls are inlined as much as possible.  If false, use
     * a dynamically allocated contention manager where every call is made via
     * virtual method dispatch
     */
    inline static void init(std::string cm_type, std::string validation,
                            bool use_static_cm)
    {
        // get an ID for the new thread
        unsigned long id = idManager.registerThread();

        // create a Descriptor for the new thread and put it in the global
        // table
        internal::desc_array[id] =
            new internal::Descriptor(id, cm_type, validation, use_static_cm);
    }

    /**
     *  Call at thread destruction time to clean up and release any global TM
     *  resources.
     */
    inline static void shutdown()
    {
        // [todo] we should merge this thread's reclaimer into a global
        // reclaimer to prevent free memory from being logically unreclaimable.
    }

    /**
     *  Fence for privatization.  When a transaction's commit makes a piece of
     *  shared data logically private, the transaction must call fence() before
     *  creating an un_ptr to the privatized data.
     */
    void fence();

    namespace internal
    {
        /**
         *  Logic for the BEGIN_TRANSACTION macro.  Get a descriptor, make sure
         *  all logs are ready to use, and move to ACTIVE state.
         */
        inline static Descriptor* begin_transaction()
        {
            Descriptor* tx = get_descriptor();
            tx->beginTransaction();
            return tx;
        }

        /**
         *  Instruct each descriptor, in order, to print any profiling
         *  information that was collected during execution.
         */
        inline static void report_timing()
        {
            // report the tx behavior
            int i = 0;
            while (desc_array[i]) {
                ConflictCounter[desc_array[i]->id].REPORT_CONFLICTS(i);
                i++;
            }
            // report timing
            i = 0;
            while (desc_array[i]) {
                desc_array[i]->timing.REPORT_TIMING(i);
                i++;
            }
        }
    } // namespace stm::internal
} // namespace stm

#endif // __STM_H__
