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

#ifndef __BENCH_LINKED_LIST_BM_H__
#define __BENCH_LINKED_LIST_BM_H__

#include <stm_api.h>
#include "LinkedList.h"
#include "IntSet.h"

namespace bench
{

    // We construct other data structures from the Linked List; in order to
    // do their sanity checks correctly, we might need to pass in a
    // validation function of this type
    typedef bool (*verifier)(unsigned long, unsigned long);


    // Set of LLNodes represented as a linked list in sorted order
    class LinkedListBM : public IntSet
    {
      private:

        stm::sh_ptr<LLNode> sentinel;

      public:

        LinkedListBM();

        // insert a node if it doesn't already exist
        virtual void insert(int val);

        // true iff val is in the data structure
        virtual bool lookup(int val) const;

        // remove a node if its value = val
        virtual void remove(int val);

        // make sure the list is in sorted order
        virtual bool isSane() const;

        // print the whole list (assumes isolation)
        virtual void print() const;

        // make sure the list is in sorted order and for each node x,
        // v(x, verifier_param) is true
        virtual bool extendedSanityCheck(verifier v,
                                         unsigned long param) const;
                                         
		virtual void insert2(int val);

        // true iff val is in the data structure
        virtual bool lookup2(int val) const;

        // remove a node if its value = val
        virtual void remove2(int val);

        // make sure the list is in sorted order
        virtual bool isSane2() const;

        // print the whole list (assumes isolation)
        virtual void print2() const;

        // make sure the list is in sorted order and for each node x,
        // v(x, verifier_param) is true
        virtual bool extendedSanityCheck2(verifier v,
                                         unsigned long param) const;

    };

} // namespace bench
#endif // __BENCH_LINKED_LIST_BM_H__
