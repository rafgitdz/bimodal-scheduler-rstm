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

#ifndef __MINIVECTOR_H__
#define __MINIVECTOR_H__

#include <cassert>
#include "stm_mm.h"

namespace stm
{
    /**
     *  Simple self-growing array.  This is a lot like the stl vector, except
     *  that we don't destroy list elements when we clear the list, resulting
     *  in an O(1) clear() instead of the standard stl O(n) clear overhead.
     *
     *  A lot of thought went into this data structure, even if it doesn't seem
     *  like it.  std::vector and our old stm::SearchList data structures both
     *  perform worse.
     */
    template <class T>
    class MiniVector
    {
        /**
         *  Cache the thread-local allocator
         */
        stm::mm::TxHeap* heap;

      public:
        /**
         *  Track the maximum number of elements that the current array can
         *  hold
         */
        unsigned long max_size;

        /**
         *  Track how many elements are currently stored.
         */
        unsigned long element_count;

        /**
         *  Array of elements
         */
        T* elements;

        // Construct a minivector with a default size and a pointer to the
        // thread-local allocator.
        MiniVector(stm::mm::TxHeap* h, unsigned long initial_size)
            : heap(h), max_size(initial_size), element_count(0)
        {
            elements =
                reinterpret_cast<T*>(heap->tx_alloc(sizeof(T) * max_size));
            assert(elements);
        }

        /**
         *  Reset the vector without destroying the elements it holds
         */
        void reset() { element_count = 0; }

        /**
         *  insert an element into the minivector
         */
        void insert(T data);

        /**
         *  delete by index; just move the last element into this index, and
         *  decrease the count by 1.
         */
        void remove(unsigned long index);

        /**
         *  Return true if the list has no elements.
         */
        bool is_empty() const { return element_count == 0; }
    };
};

template <class T>
inline void stm::MiniVector<T>::insert(T data)
{
    // put /data/ into element_count and increment element_count
    elements[element_count] = data;
    element_count++;
    // if the list is full, double the list size
    if (element_count == max_size) {
        // keep reference to old list of elements
        T* old_elements = elements;
        // make a new list of elements that is twice as big
        max_size = max_size * 2;
        elements = reinterpret_cast<T*>(heap->tx_alloc(sizeof(T) * max_size));
        assert(elements);
        // copy all old elements to new list
        for (unsigned long i = 0; i < element_count; i++)
            elements[i] = old_elements[i];
        // free old list without calling destructors
        heap->tx_free(old_elements);
    }

    // NB: There is a tradeoff here.  If we put the element into the list
    // first, we are going to have to copy one more object any time we double
    // the list.  However, by doing things in this order we avoid constructing
    // /data/ on the stack if (1) it has a simple constructor and (2) /data/
    // isn't that big relative to the number of available registers.
}

template <class T>
inline void stm::MiniVector<T>::remove(unsigned long index)
{
    // get the index of the last element
    unsigned long oldcount = element_count - 1;

    // if /index/ isn't the last element, move the last element into the
    // /index/ position
    if (index != oldcount)
        elements[index] = elements[oldcount];

    // decrease the element count by one
    element_count = oldcount;
}

#endif // __MINIVECTOR_H__
