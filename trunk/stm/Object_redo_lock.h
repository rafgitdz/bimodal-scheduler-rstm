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

#ifndef __OBJECT_H__
#define __OBJECT_H__

#include "Shared_redo_lock.h"
#include "Descriptor_redo_lock.h"

namespace stm
{
    /**
     *  Any class used transactionally must derive from Object<T>.  Object<T>
     *  is pretty much just a type-safety wrapper around SharedBase.
     */
    template <class T>
    class Object : public internal::SharedBase
    {
      public:
        Object() : SharedBase() { }

        /**
         *  Return a Shared<T> wrapper for /this/; if shared() is called many
         *  times, the value returned should always be the same.
         *
         *  @param  tx  The current transaction.
         *
         *  @returns    a pointer to the shared object with which /this/ is
         *              associated.
         *
         *  If no Shared<T> currently exists for /this/, a new one is
         *  generated.  This is the primary way of creating a Shared<T>. At
         *  some point, this will become the exclusive way of creating a
         *  Shared<T>.
         */
        internal::Shared<T>* shared(internal::Descriptor* tx = NULL) const
        {
            if (!this)
                return NULL;

            // get a snapshot of the header
            internal::metadata_dword_t snap;
            snap.dword = m_metadata.dword;

            // assume that we're calling retval on the inplace version; strip
            // off the const-ness of /this/
            SharedBase* retval = const_cast<Object<T>*>(this);

            // if the redoLog is not null and the version is odd, then we're
            // looking at the redo log (and the redoLog field is actually
            // pointing to the inplace version)
            if ((snap.fields.redoLog != NULL) &&
                (snap.fields.ver.version & 1 == 1))
            {
                retval = snap.fields.redoLog;
            }

            // put the type-ness on retval and return it
            return static_cast<internal::Shared<T>*>(retval);
        }

    };
} // namespace stm

#endif // __OBJECT_H__
