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

#ifndef __OBJECTBASE_H__
#define __OBJECTBASE_H__

#include "CustomAllocedBase.h"

namespace stm
{
    namespace internal
    {
        // forward declarations
        class SharedBase;
        class Descriptor;

        /**
         *  ObjectBase provides the interface and metadata that a transaction
         *  needs to detect and resolve conflicts, and to create and destroy
         *  clones.  ObjectBase inherits from CustomAllocedBase so that it gets
         *  access to whatever custom allocation strategy is currently in
         *  place.
         */
        class ObjectBase : public mm::CustomAllocedBase
        {
            friend class SharedBase;
            friend class Descriptor;

          protected:
            /**
             *  Version from which /this/ was cloned.  The bits of this field
             *  are volatile, so that we can zero them out when a transaction
             *  commits, and be sure that other threads immediately see that
             *  the bits are zeroed.
             */
            ObjectBase* volatile m_next;

            /**
             *  The tx who created /this/.  Once this is set, it never changes,
             *  but we can't actually make this const because of the convoluted
             *  path for object construction.
             */
            Descriptor* m_owner;

            /**
             *  The header through which /this/ is accessed.  m_st is
             *  immutable; once it is set it will never change.
             */
            SharedBase* m_st;

            /**
             *  Ctor: zeros out all fields.  Note that we never make ObjectBase
             *  objects directly, only through Object<T>.
             */
            ObjectBase() : m_next(NULL), m_owner(NULL), m_st(NULL) { }

            /**
             *  The user must provide clone, which is a tx-safe copy of the
             *  required depth. The user's clone method should return its own
             *  type via C++'s covariant return type functionality, ie:
             *
             *  Foo : Object<Foo> // NB: Object<T> inherits from ObjectBase
             *  {
             *      virtual Foo* clone() { return new Foo(this); }
             *  }
             */
            virtual ObjectBase* clone() const = 0;

            /**
             *  If the clone() isn't shallow, then the pre-delete deactivation
             *  method becomes important. Resources acquired by a clone()ed
             *  object (files, memory, etc) should be released here.
             */
            virtual void deactivate() = 0;
        };
    }   // stm::internal
}   // stm

#endif // __OBJECTBASE_H__
