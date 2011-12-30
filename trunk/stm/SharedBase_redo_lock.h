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

#include "CustomAllocedBase.h"

namespace stm
{
    namespace internal
    {
        // forward declarations so that we can build out the metadata type for
        // redo_lock as a union
        class Descriptor;
        class SharedBase;

        /**
         *  Since a pointer to an owner is always even, we can overload the
         *  version# of an unacquired object with the owner pointer of an
         *  acquired object.
         */
        union owner_version_t
        {
            volatile unsigned long version;
            Descriptor* volatile owner;
        };

        /**
         * The entire metadata packet for a SharedBase consists of a version
         * number, an owner pointer, and a redo log.  We union the version and
         * owner pointer, so that it all fits into 2 words.
         */
        struct shared_metadata_t
        {
            owner_version_t ver;
            SharedBase* volatile redoLog;
        };

        /**
         *  In order to ensure that we access the entire metadata packet of an
         *  object in a single atomic operation, we'll union the metadata
         *  packet with an unsigned long long.
         */
        union metadata_dword_t
        {
            shared_metadata_t fields;
            volatile unsigned long long dword;
        };

        // tell the world that if you're using SharedBase, you need a redo
        // method
#define NEED_REDO_METHOD

        // define the Validator class now, make it a friend of SharedBase, and
        // then put its definition later:
        class Validator;

        /**
         *  SharedBase provides the interface and metadata that a Shared<T>
         *  needs during a transaction, to insure that a Shared<T> pointer
         *  (where T derives from Object<U> which derives from SharedBase) can
         *  be read and written correctly. SharedBase inherits from
         *  CustomAllocedBase so that it gets access to whatever custom
         *  allocation strategy is currently in place.
         */
        class SharedBase : public mm::CustomAllocedBase
        {
            friend class Validator;
            friend class Descriptor;
          protected:
            /**
             *  Super-union of all the to represent version number, owner
             *  pointer, and lock.  The object is locked if the verison is 2,
             *  the object is available if the version is odd, and the object
             *  is owned (ver# = descriptor*) if the version is even.  Of
             *  course, 2 is even but 2 will never be a valid pointer.  Also
             *  note that if /this/ is serving as a redo log, then the version
             *  is the old version, not the version to be set upon successful
             *  redo.
             */
            metadata_dword_t m_metadata;

            /**
             *  We never make SharedBase objects directly (the ctor is
             *  protected), only through Object<T>.  Since this is the case, it
             *  makes the most sense to have this ctor just zero all fields,
             *  and then if the Object<T> knows something about what should be
             *  set in any of these fields, then the Object<T> constructor can
             *  set the fields.
             */
            SharedBase()
            {
                m_metadata.fields.ver.version = 1;
                m_metadata.fields.redoLog = NULL;
            }

            /**
             *  The user must provide clone, which is a tx-safe copy of the
             *  required depth. The user's clone method should return it's own
             *  type via C++'s covariant return type functionality, ie:
             *
             *  Foo : Object<Foo>
             *  {
             *      virtual Foo* clone() { return new Foo(this); }
             *  }
             */
            virtual SharedBase* clone() const = 0;

            // when the tx commits, we need to 'redo' changes to the object by
            // applying the redo log
            virtual void redo(SharedBase* s) = 0;

            /**
             *  If the clone() isn't shallow, then the pre-delete deactivation
             *  method becomes important. Resources acquired by a clone()ed
             *  object (files etc) should be released here.
             */
            virtual void deactivate() { }
        };
    } // namespace internal
} // namespace stm

#endif // __SHAREDBASE_H__
