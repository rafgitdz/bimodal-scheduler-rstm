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

#ifndef __SHAREDBASE_H__
#define __SHAREDBASE_H__

#include <cassert>

#include "CustomAllocedBase.h"
#include "ObjectBase_rstm.h"

namespace stm
{
    namespace internal
    {
        /**
         *  Check if the object is dirty, i.e. its lsb == 1.
         */
        inline bool is_owned(const ObjectBase* header)
        {
            return (reinterpret_cast<unsigned long>(header) & 0x1);
        }

        /**
         *  Strip the lsb from the header pointer, so that it is safe to
         *  dereference it.
         */
        inline ObjectBase* get_data_ptr(const ObjectBase* header)
        {
            return reinterpret_cast<ObjectBase*>
                (reinterpret_cast<unsigned long>(header) & (~1));
        }

        /**
         *  Turn on the lsb of an ObjectBase*.
         */
        inline ObjectBase* set_lsb(const ObjectBase* curr_data)
        {
            return reinterpret_cast<ObjectBase*>
                (reinterpret_cast<unsigned long>(curr_data) | 0x1);
        }

        /**
         *  Turn off the lsb of an ObjectBase*.
         */
        inline ObjectBase* unset_lsb(const ObjectBase* curr_data)
        {
            return reinterpret_cast<ObjectBase*>
                (reinterpret_cast<unsigned long>(curr_data) & (~1));
        }

        // forward declaration of Descriptor so that we can make it a friend of
        // SharedBase.
        class Descriptor;

        /**
         *  SharedBase is the non-templated version of the Shared header for
         *  transactional objects.  It stores all of the metadata for the
         *  shared pointer. User code should not ever inherit from SharedBase.
         *
         *  Note that SharedBase inherits from CustomAllocedBase, and thus uses
         *  the 'malloc' and 'free' in namespace stm::mm that were specified at
         *  compile time, with a wrapper to make those calls tx-safe.
         */
        class SharedBase : public mm::CustomAllocedBase
        {
            friend class Descriptor;
          protected:
            /**
             *  Pointer to the shared data.  The lsb of m_payload is set if
             *  /this/ has been acquired by a writer and the writer has not
             *  cleaned up yet.
             *
             *  NB: __attribute__ used to avoid warning about strict aliasing
             *  rules when we cast this to an unsigned long in order to pass it
             *  to CAS
             *
             *  NB: the 'volatile' keyword is placed so that the actual bits of
             *  this field are volatile, not the bits on the other side of the
             *  pointer.  This is safe because the only fields that one might
             *  look at in the referenced ObjectBase are already volatile.
             */
            ObjectBase* volatile __attribute__((__may_alias__)) m_payload;

            /**
             *  Visible reader bitmap.  Up to 32 transactions can read an
             *  object visibly by getting permission to 'own' one of the 32
             *  global bits and then setting that bit in a shared object's
             *  visible reader bitmap.
             */
            volatile unsigned long m_readers;

            /**
             *  Constructor for shared objects. It is only used through
             *  Shared<T>::CreateShared() calls.  All this constructor does is
             *  initialize a SharedBase to wrap the object for transactional
             *  use.
             *
             *  @param t - The ObjectBase that this shared manages
             *
             *  @param tx - If the shared is created during a transaction, this
             *  is a pointer to that transaction's Descriptor.
             */
            SharedBase(ObjectBase* t, Descriptor* tx = NULL)
                : m_payload(t), m_readers(0)
            {
                assert(m_payload);

                // set up the payload so that its back pointer to a SharedBase
                // points here.
                m_payload->m_st = this;
                m_payload->m_next = NULL;
                m_payload->m_owner = tx;
            }
        };
    }   // namespace internal
}   // namespace stm

#endif // __SHAREDBASE_H__
