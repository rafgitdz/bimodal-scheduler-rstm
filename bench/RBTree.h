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

#ifndef __BENCH_RB_TREE_H__
#define __BENCH_RB_TREE_H__

#include <stm_api.h>
#include "IntSet.h"

namespace bench
{
    // Node of an RBTree
    class RBNode : public stm::Object<RBNode>
    {
        GENERATE_FIELD(Color, color);
        GENERATE_FIELD(int, val);
        // invariant: parent->child[ID] == this
        GENERATE_FIELD(stm::sh_ptr<RBNode>, parent);
        GENERATE_FIELD(int, ID);
        GENERATE_ARRAY(stm::sh_ptr<RBNode>, child, 2);
      public:

        RBNode();

        virtual RBNode* clone() const
        {
            Color c = m_color;
            int val = m_val;
            stm::sh_ptr<RBNode> parent = m_parent;
            int ID = m_ID;
            stm::sh_ptr<RBNode> c0 = m_child[0];
            stm::sh_ptr<RBNode> c1 = m_child[1];

            RBNode* newnode = new RBNode();
            newnode->set_color(c);
            newnode->set_val(val);
            newnode->set_parent(parent);
            newnode->set_ID(ID);
            newnode->set_child(0, c0);
            newnode->set_child(1, c1);

            return newnode;
        }

#ifdef NEED_REDO_METHOD
        virtual void redo(stm::internal::SharedBase* n_sh)
        {
            RBNode* n = static_cast<RBNode*>(n_sh);
            m_color = n->m_color;
            m_val = n->m_val;
            m_parent = n->m_parent;
            m_ID = n->m_ID;
            m_child[0] = n->m_child[0];
            m_child[1] = n->m_child[1];
        }
#endif
    };

    // set of RBNode objects
    class RBTree : public IntSet
    {
      public:
        stm::sh_ptr<RBNode> sentinel;

        RBTree();

        // standard IntSet methods
        virtual bool lookup(int val) const;
        virtual void insert(int val);
        virtual void remove(int val);
        virtual bool isSane() const;

        // for debugging only
        virtual void print(int indent = 0) const;
    };

} // namespace bench

#endif // __BENCH_RB_TREE_H__
