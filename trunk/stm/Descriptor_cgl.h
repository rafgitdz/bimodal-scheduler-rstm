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

#include "stm_mm.h"
#include "instrumentation.h"

namespace stm
{
    namespace internal
    {
        class SharedBase;
        class ObjectBase;

        class Descriptor
        {
            const unsigned long id;
            stm::mm::TxHeap* heap;

          public:
            mcs_qnode_t mcsnode;

            Descriptor(unsigned long _id) : id(_id)
            {
                heap = ::new stm::mm::TxHeap(id);
#ifdef TIMING_BREAKDOWNS
                // set up the timing fields
                timing = TimeAccounting();
#endif
            }

            ~Descriptor() { }

#ifdef TIMING_BREAKDOWNS
            // for timing
            TimeAccounting timing;
#endif

            void* txAlloc(size_t size)
            {
#ifdef TIMING_BREAKDOWNS
                desc_array[id]->timing.GIVE_TIMING();
#endif
                void* ptr = heap->tx_alloc(size);
#ifdef TIMING_BREAKDOWNS
                desc_array[id]->timing.TAKE_TIMING(TIMING_MM);
#endif
                return ptr;
            }

            void txFree(void* ptr)
            {
                if (ptr == NULL)
                    return;
                heap->tx_free(ptr);
            }

            void addDtor(SharedBase* ptr);

            /**
             *  Allows comparison of Descriptors.
             */
            bool operator==(const Descriptor& rhs) const
            { return (id == rhs.id); }

            bool operator!=(const Descriptor& rhs) const
            { return (id != rhs.id); }

            // Static access for the current thread's descriptor. Heavyweight
            // function (needs to access pthread library). Cache if possible.
            static Descriptor& CurrentThreadInstance()
            {
                Descriptor* tx = desc_array[idManager.getThreadId()];
                assert(tx);
                return *tx;
            }

            // Make sure we are not in an active transaction.
            // No-op in CGL.
            inline static void assert_nontransactional() { }
            inline static void assert_transactional() { }

            void release(SharedBase* sh) { }
        } __attribute__ ((aligned(64)));

        // get the current thread's Descriptor, legacy report
        inline Descriptor* get_descriptor()
        {
            return &Descriptor::CurrentThreadInstance();
        }
    } // namespace internal
} // namespace stm

#endif // __DESCRIPTOR_H__
