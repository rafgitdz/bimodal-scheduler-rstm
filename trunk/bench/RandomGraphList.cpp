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

#include <iostream>

using std::cout;
using std::endl;

#include "RandomGraphList.h"

using namespace stm;
using namespace bench;


// try to find an entry in /this/ whose value is nval
sh_ptr<GNode>
GNodeList::lookup(int nval) const
{
    // open the head of the list (which may already be open)
    // NB:  first entry is a sentinel, so we can skip it
    rd_ptr<LNode> list_node(getList());
    list_node = list_node->get_next(list_node.v());

    // This is outside the loop so that we don't keep initializing
    // the pointer inside the loop.
    rd_ptr<GNode> node_contents;

    // linear search through the list
    while (list_node != NULL)
    {
        // open the current node and look at its value
        node_contents = list_node->get_node(list_node.v());

        if (node_contents->get_val(node_contents.v()) > nval)
        {
            return sh_ptr<GNode>();   // nobody in list with val==nval
        }
        if (node_contents->get_val(node_contents.v()) == nval)
        {
            return node_contents;
        }

        list_node = list_node->get_next(list_node.v());
    }

    return sh_ptr<GNode>();
}


// this inserts "n" into the list, but does not do any linking
bool
GNodeList::insert(const sh_ptr<GNode>& add_node) const
{
    // first get the value of the node being added
    rd_ptr<GNode> node(add_node);
    int nval = node->get_val(node.v());

    // now traverse the list to find where to place the add_node
    rd_ptr<LNode> curr_node(getList());
    rd_ptr<LNode> next_node(curr_node->get_next(curr_node.v()));
    rd_ptr<GNode> next_content;

    while (next_node != NULL)
    {
        next_content = next_node->get_node(next_node.v());

        if (next_content->get_val(next_content.v()) > nval)
        {
            break;
        }
        else if (next_content->get_val(next_content.v()) == nval)
        {
            return false;
        }

        curr_node = next_node;
        next_node = next_node->get_next(next_node.v());
    }

    // now insert a new LNode between curr_node and next_node
    wr_ptr<LNode> insert_point(curr_node);
    insert_point->set_next(sh_ptr<LNode>(new LNode(add_node, next_node)));

    return true;
}

// this code is responsible for finding the entry in the list whose value is
// nval, and removing that entry from the list.  we return the entry we
// removed, in case it is needed for more work (most notably, for disconnecting
// the removed node from its peers)
sh_ptr<GNode>
GNodeList::remove(int nval) const
{
    // first go through the list and try to find a GNode with val==nval
    rd_ptr<LNode> curr_node(getList());
    rd_ptr<LNode> next_node(curr_node->get_next(curr_node.v()));
    rd_ptr<GNode> next_content;

    while (next_node != NULL)
    {
        next_content = next_node->get_node(next_node.v());

        if (next_content->get_val(next_content.v()) > nval)
        {
            return sh_ptr<GNode>();
        }
        else if (next_content->get_val(next_content.v()) == nval)
        {
            break;
        }

        curr_node = next_node;
        next_node = next_node->get_next(next_node.v());
    }

    // if we get here and we have a next node, then we need to remove next_node
    // from the linked list.
    if (next_node != NULL) {
        wr_ptr<LNode> curr_w(curr_node);
        curr_w->set_next(next_node->get_next(next_node.v()));
        tx_delete(next_node);
        return next_content;
    }

    return sh_ptr<GNode>();
}

// this is supposed to make /this/ have a neighbor entry of n, and n have a
// neighbor entry of /this/
void
GNode::insert(const sh_ptr<GNode>& this_sh, const sh_ptr<GNode>& new_peer)
{
    m_neighbors.insert(new_peer);

    rd_ptr<GNode> rnp(new_peer);
    rnp->m_neighbors.insert(this_sh);
}


// this method is responsible for going through /this/.neighbors and for each
// entry E, calling E.neighbors.remove(this.nval)
void
GNode::disconnect(wr_ptr<GNode>& this_w)
{
    // skip the first node of the neighbor list since it is a sentinel
    rd_ptr<LNode> curr_node(m_neighbors.getList());
    curr_node = curr_node->get_next(curr_node.v());

    // Used to avoid temporary generation in the loop
    rd_ptr<GNode> neighbors;

    // traverse the list
    while (curr_node != NULL)
    {
        // open the payload, get its neighbor list, and call remove on that
        neighbors = curr_node->get_node(curr_node.v());
        neighbors->m_neighbors.remove(get_val(this_w.v()));
        curr_node = curr_node->get_next(curr_node.v());
    }
}

// This is the entry point for an insert tx from the benchmark harness
void
RandomGraph::insert(unsigned int* seed)
{
    // get the rand vals outside of the tx, so that the tx doesn't succeed by
    // retrying until it gets a lucky set of values
    int val = rand_r(seed) % maxNodes;

    int linkups[CONNECTIONS_PER_NODE];

    for (int i = 0; i < CONNECTIONS_PER_NODE; i++)
        // select a random node to connect to
        linkups[i] = rand_r(seed) % maxNodes;


    BEGIN_TRANSACTION;

    // create the node we'll try to insert
    sh_ptr<GNode> new_node = sh_ptr<GNode>(new GNode(val));

    // now try to insert the new node into the RandomGraphList
    if (nodes.insert(new_node))
    {
        // Used in the loop
        sh_ptr<GNode> peer;
        wr_ptr<GNode> new_content(new_node);

        // if the insert succeeded, then try to link this guy to some peers
        for (int i = 0; i < CONNECTIONS_PER_NODE; i++)
        {
            if (linkups[i] == val)
            {
                continue;       // don't link to self
            }

            // for each neighbor, if it exists, connect to it
            if ((peer = nodes.lookup(linkups[i])) != NULL)
            {
                new_content->insert(new_node, peer);
            }
        }
    }
    else
    {
        // the value is already in the graph, so quit this tx
        tx_delete(new_node);
    }

    END_TRANSACTION;
}


// This is the entry point for a remove tx from the benchmark harness
void RandomGraph::remove(unsigned int* seed)
{
    int val = rand_r(seed) % maxNodes;

    BEGIN_TRANSACTION;

    // first remove the node from the main list of nodes
    sh_ptr<GNode> remove_node = nodes.remove(val);

    // now disconnect the node from its peers
    if (remove_node != NULL)
    {
        wr_ptr<GNode> w(remove_node);
        w->disconnect(w);
    }

    END_TRANSACTION;
}


// sanity check of the RandomGraph structure
bool
RandomGraph::isSane() const
{
    bool sane = false;

    BEGIN_TRANSACTION;

    sane = true;        // assume we'll succeed

    // outer loop:  make sure that the main list of nodes is sorted
    int last_val = -1;
    rd_ptr<LNode> curr_node(nodes.getList());
    rd_ptr<GNode> curr_content;
    rd_ptr<LNode> nbor_node;
    rd_ptr<GNode> nbor_content;

    while (curr_node != NULL)
    {
        if (curr_node->get_node(curr_node.v()) == NULL)
        {
            // move to the next node in the outer loop
            curr_node = curr_node->get_next(curr_node.v());
            continue;
        }

        // check that the main list is sorted
        curr_content = curr_node->get_node(curr_node.v());

        if (curr_content->get_val(curr_content.v()) < last_val)
        {
            sane = false;
            break;
        }

        last_val = curr_content->get_val(curr_content.v());

        // drill down on curr_content to make sure its neighbors are sorted,
        // and to make sure that each neighbor is a bidirectional link
        int last_nbor = -1;

        nbor_node = curr_content->m_neighbors.getList();

        while (nbor_node != NULL)
        {
            if (nbor_node->get_node(nbor_node.v()) == NULL)
            {
                nbor_node = nbor_node->get_next(nbor_node.v());
                continue;
            }

            // make sure the neighbors are sorted:
            nbor_content = nbor_node->get_node(nbor_node.v());

            if (nbor_content->get_val(nbor_content.v()) < last_nbor)
            {
                sane = false;
                break;
            }

            last_nbor = nbor_content->get_val(nbor_content.v());

            // Make sure the neighbor is in the main list
            // Implicit conversion here
            if (nodes.lookup(last_nbor) != nbor_content)
            {
                sane = false;
                break;
            }

            // Make sure the neighbor links back to this node
            // Implicit conversion here
            if (nbor_content->m_neighbors.lookup(curr_content->get_val(curr_content.v()))
                != curr_content)
            {
                sane = false;
                break;
            }

            // This neighbor passed.  Move to the next one
            nbor_node = nbor_node->get_next(nbor_node.v());
        }

        // the break statements in the inner while () only get us to here; we
        // need to break a second time if the data structure isn't sane
        if (!sane)
            break;

        // move to the next node in the outer loop
        curr_node = curr_node->get_next(curr_node.v());
    }

    END_TRANSACTION;

    return sane;
}

// print the RandomGraph structure
void
RandomGraph::print() const
{
    BEGIN_TRANSACTION;

    cout << "graph:" << endl;

    rd_ptr<LNode> curr_node(nodes.getList());
    rd_ptr<GNode> curr_content;
    rd_ptr<LNode> nbor_node;
    rd_ptr<GNode> nbor_content;

    // outer loop:  go through all nodes in the graph
    while (curr_node != NULL)
    {
        if (curr_node->get_node(curr_node.v()) == NULL)
        {
            // move to the next node in the outer loop
            curr_node = curr_node->get_next(curr_node.v());
            continue;
        }

        // print this node's value
        curr_content = curr_node->get_node(curr_node.v());
        cout << " node " << curr_content->get_val(curr_content.v());

        // print the neighbor list
        nbor_node = curr_content->m_neighbors.getList();

        while (nbor_node != NULL)
        {
            if (nbor_node->get_node(nbor_node.v()) == NULL)
            {
                nbor_node = nbor_node->get_next(nbor_node.v());
                continue;
            }

            // print the neighbor
            nbor_content = nbor_node->get_node(nbor_node.v());
            cout << "::" << nbor_content->get_val(nbor_content.v());
            nbor_node = nbor_node->get_next(nbor_node.v());
        }

        // move to the next node in the outer loop
        curr_node = curr_node->get_next(curr_node.v());

        cout << endl;
    }

    END_TRANSACTION;
}
