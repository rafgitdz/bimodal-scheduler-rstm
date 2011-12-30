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

#ifndef __BENCH_RANDOM_GRAPH_LIST_H__
#define __BENCH_RANDOM_GRAPH_LIST_H__

#include <stm_api.h>
#include "Benchmark.h"

namespace bench
{
    const int CONNECTIONS_PER_NODE = 4;

    /// the RandomGraph consists of a list of nodes.  Each node in the list
    /// represents a vertex in the RandomGraph.  Each node is a GNode.  The
    /// list is of type GNodeList.  Entries in the GNodeList are of type
    /// LNode.

    /// Each vertex has its own GNodeList.  Edges between vertices are
    /// represented by entries in the GNodeList.  While the edges appear to
    /// be directed, they are in fact undirected, because of the invariant
    /// that V1 can only have an edge to V2 if V2 also has an edge to V1.

    class GNode;


    /**
     *  this give us a linked list node for use in GNodeList.  It's got two
     *  fields, one for the payload (node) and one for the next pointer
     *  (next)
     */
    class LNode : public stm::Object<LNode>
    {
        GENERATE_FIELD(stm::sh_ptr<GNode>, node);
        GENERATE_FIELD(stm::sh_ptr<LNode>, next);

      public:
        // ctor
        LNode()
            : m_node(),
              m_next()
        { }

        LNode(const stm::sh_ptr<GNode>& n, const stm::sh_ptr<LNode>& l)
            : m_node(n),
              m_next(l)
        { }

        // clone
        virtual LNode* clone() const
        {
            // we can't access fields directly anymore, not even in the
            // clone() method
            stm::sh_ptr<GNode> c_node = m_node;
            stm::sh_ptr<LNode> c_next = m_next;

            return new LNode(c_node, c_next);
        }

#ifdef NEED_REDO_METHOD
        // redo
        virtual void redo(stm::internal::SharedBase* l_sh)
        {
            LNode* l = static_cast<LNode*>(l_sh);
            m_node = l->m_node;
            m_next = l->m_next;
        }
#endif
    };


    /**
     *  this is not actually a shared object, it just holds a pointer to a
     *  shared list node
     */
    class GNodeList
    {
        stm::sh_ptr<LNode> m_list;
      public:
        // getter methods
        const stm::sh_ptr<LNode>& getList() const { return m_list; }

        // setter methods
        void setList(const stm::sh_ptr<LNode>& list) { m_list = list; }

        // ctor
        GNodeList()
            : m_list(new LNode())
        { }

        GNodeList(const stm::sh_ptr<LNode>& list)
            : m_list(list)
        { }

        stm::sh_ptr<GNode> lookup(int nval) const;
        bool insert(const stm::sh_ptr<GNode>& n) const;
        stm::sh_ptr<GNode> remove(int nval) const;
    };


    /**
     *  this holds an actual graph node.  A graph node consists of its
     *  value and a linked list of all its neighbors
     */
    class GNode : public stm::Object<GNode>
    {
        GENERATE_FIELD(int, val);
        GNodeList m_neighbors;

      public:

        GNode(int v = -1)
            : m_val(v)
        { }


        GNode(int v, const stm::sh_ptr<LNode>& l)
            : m_val(v)
        {
            m_neighbors.setList(l);
        }

        // clone
        virtual GNode* clone() const
        {
            // we can't access fields directly anymore, not even in the
            // clone() method
            int val = m_val;
            stm::sh_ptr<LNode> list = m_neighbors.getList();

            return new GNode(val, list);
        }

#ifdef NEED_REDO_METHOD
        // redo
        virtual void redo(SharedBase* g_sh)
        {
            GNode* g = static_cast<GNode*>(g_sh);
            m_val = g->m_val;
            m_neighbors.setList(g->m_neighbors.getList());
        }
#endif
        void disconnect(stm::wr_ptr<GNode>& this_w);

        void insert(const stm::sh_ptr<GNode>& this_sh,
                    const stm::sh_ptr<GNode>& n);

    };


    /**
     *  here's the wrapper class that provides something of a benchmark
     *  interface
     */
    class RandomGraph
    {
      private:

        GNodeList nodes;
        int maxNodes;


      public:

        RandomGraph(int _maxNodes)
            : nodes(),
              maxNodes(_maxNodes)
        { }


        /**
         *  This is the entry point for an insert tx from the benchmark
         *  harness
         */
        void insert(unsigned int* seed);


        /**
         *  and this is the entry point for a remove tx from the benchmark
         *  harness
         */
        void remove(unsigned int* seed);

        // sanity check
        bool isSane() const;

        // print
        void print() const;
    };

    template
    <
        class RandomGraphType = RandomGraph
        >
    class RGBench : public Benchmark
    {
      private:
        RandomGraphType* m_graph;

      public:

        RGBench(RandomGraphType* graph, int maxNodes)
            : m_graph(graph)
        { }

        // 50/50 mix of inserts and deletes
        void random_transaction(thread_args_t* args,
                                unsigned int* seed,
                                unsigned int val,
                                unsigned int chance)
        {
            if (rand_r(seed) % 2)
            {
                m_graph->insert(seed);
                ++args->count[TXN_INSERT];
            }
            else
            {
                m_graph->remove(seed);
                ++args->count[TXN_REMOVE];
            }
        }

        // call the graph's sanity check
        bool sanity_check() const
        {
            return m_graph->isSane();
        }

        // no data structure verification is implemented for the RandomGraph
        // yet
        virtual bool verify(VerifyLevel_t v)
        { return true; }
    };


} // namespace bench

#endif // __BENCH_RANDOM_GRAPH_LIST_H__
