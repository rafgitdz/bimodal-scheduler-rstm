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

#ifndef __SHARED_H__
#define __SHARED_H__

#include "Descriptor_redo_lock.h"
#include "SharedBase_redo_lock.h"

namespace stm
{
    namespace internal
    {
        /**
         *  Layer of abstraction between descriptor and STM api.  Shared<T>
         *  essentially is a type-marshaller for objects.  T always inherits
         *  from Object<T>, and is thus an ObjectBase.  Shared<T> does
         *  conversions between <T> and ObjectBase as user code calls the
         *  library.
         */
        template <class T>
        class Shared : public T
        {
            /**
             *  Private constructor ensures that we don't make Shared<T>s
             *  directly; you have to use CreateShared.
             */
            Shared<T>() : T() { }

          public:
            /**
             *  Ensure that t has a shared header wrapping it (i.e. that
             *  t->m_st is set).
             */
            static Shared<T>* CreateShared(T* t)
            {
                return t->shared();
            }

            /**
             *  It's never correct to destroy an object inside of a
             *  transaction.  Instead, use this method to schedule an object
             *  for destruction upon commit.
             *
             *  @note - the problem with in-tx destruction is that when g++
             *  uses virtual destructors, it modifies the vtable.  Vtable
             *  modifications can't be rolled back on abort, and can have bad
             *  side effects in other concurrent transactions.
             */
            static void DeleteShared(Descriptor& tx, Shared<T>* shared)
            {
                tx.addDtor(shared);
            }

            /**
             *  Delete an object 'unsafely,' i.e. from outside of a
             *  transaction.
             */
            static void DeleteSharedUn(Shared<T>* shared)
            {
                get_descriptor()->addDtorUn(shared);
            }

            /**
             *  Detect alias errors.  Since RSTM uses clones, a readable
             *  version of object O becomes an incorrect alias once the
             *  transaction opens O for writing.  When debugging an
             *  application, it is very useful to have this method for locating
             *  those instances in which the application uses the readable O
             *  incorrectly after it has a writable O.
             *
             *  This is a very expensive function.  Right now, unless you have
             *  assertions turned off, you pay for this.
             */
            bool check_alias(const T* t, Descriptor& tx) const
            {
                return tx.ensure_no_upgrade(this, t);
            }

            /**
             * verify_un is not required in redo_lock, because there aren't
             * alias error problems that can arise from a doomed transaction
             * acquiring an object that I've got an un_ptr to.  We only have
             * this for API compatibility.
             */
            void verify_un(const T* t) { }

            /**
             *  Return a version of this object that can be used outside of
             *  transactions.  This should only be called from outside of a
             *  transaction, and after a fence().
             */
            T* open_un(Validator& v)
            {
                return static_cast<T*>(Descriptor::open_un(this));
            }

            /**
             *  Return a read-safe version of this object.
             */
            inline const T* open_RO(Descriptor& tx, Validator& v)
            {
                return static_cast<const T*>(tx.getReadable(this, v));
            }

            /**
             *  Return a write-safe version of this object.
             */
            inline T* open_RW(Descriptor& tx, Validator& v)
            {
                return static_cast<T*>(tx.getWritable(this, v));
            }
        };
    } // namespace internal
} // namespace stm

#endif // __SHARED_H__
