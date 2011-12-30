/*  tm_list_set.h
 *
 *  Transactional list-based set.
 *  Element type must be the same size as unsigned long.
 */

#ifndef TM_LIST_SET_H
#define TM_LIST_SET_H

#include <assert.h>
#include "stm_api.h"
#include "tm_set.h"

template<typename T>
class tm_list_set;

// LLNode is a single node in a sorted linked list
//
template<class T>
class LLNode : public stm::Object<LLNode<T> >
{
    friend class tm_list_set<T>;   // all fields are nominally private

    GENERATE_FIELD(T, val);
    GENERATE_FIELD(stm::sh_ptr<LLNode<T> >, next_node);

    // constructors
    //
    LLNode() : m_val(), m_next_node(NULL) { }
    LLNode(T val, const stm::sh_ptr<LLNode<T> >& next)
        : m_val(val), m_next_node(next) { }

    // clone method for use in RSTM
    virtual LLNode* clone() const
    {
        T val = m_val;
        stm::sh_ptr<LLNode<T> > next = m_next_node;
        return new LLNode(val, next);
    }

#ifdef NEED_REDO_METHOD
    virtual void redo(stm::internal::SharedBase* l_sh)
    {
        LLNode* l = static_cast<LLNode*>(l_sh);
        m_val = l->m_val;
        m_next_node = l->m_next_node;
    }
#endif
};

template<typename T>
class tm_list_set : public tm_set<T>
{
    stm::sh_ptr<LLNode<T> > head_node;

    // no implementation; forbids passing by value
    tm_list_set(const tm_list_set&);
    // no implementation; forbids copying
    tm_list_set& operator=(const tm_list_set&);

    // Share code among transactional and nontransactional
    // implementations of methods.
    //
    template<class rdp, class wrp> void insert(const T item);
    template<class rdp, class wrp> void remove(const T item);
    template<class rdp> bool lookup(const T item);

  public:
    // note that assert in constructor guarantees that items and
    // unsigned ints are the same size
    //
    virtual void tx_insert(const T item)
    {
        insert<stm::rd_ptr<LLNode<T> >, stm::wr_ptr<LLNode<T> > >(item);
    }

    virtual void tx_remove(const T item)
    {
        remove<stm::rd_ptr<LLNode<T> >, stm::wr_ptr<LLNode<T> > >(item);
    }

    virtual bool tx_lookup(const T item)
    {
        return lookup<stm::rd_ptr<LLNode<T> > >(item);
    }

    virtual void pv_insert(const T item)
    {
        insert<stm::un_ptr<LLNode<T> >, stm::un_ptr<LLNode<T> > >(item);
    }

    virtual void pv_remove(const T item)
    {
        remove<stm::un_ptr<LLNode<T> >, stm::un_ptr<LLNode<T> > >(item);
    }

    virtual bool pv_lookup(const T item)
    {
        return lookup<stm::un_ptr<LLNode<T> > >(item);
    }

    virtual void pv_apply_to_all(void (*f)(T item));

    tm_list_set() : head_node(new LLNode<T>())
    {
        assert(sizeof(T) == sizeof(unsigned long));
    }

    // Destruction not currently supported.
    virtual ~tm_list_set() { assert(false); }

    int pv_size() const;
};

// insert into the list, unless the item is already in the list
template<typename T>
template<class rdp, class wrp>
void tm_list_set<T>::insert(const T item)
{
    // traverse the list to find the insertion point
    rdp prev(head_node);
    rdp curr(prev->get_next_node(prev.v()));
    while (curr != 0) {
        if (curr->get_val(curr.v()) == item)
            return;
        prev = curr;
        curr = prev->get_next_node(prev.v());
    }

    wrp insert_point(prev);      // upgrade to writable
    insert_point->set_next_node(stm::sh_ptr<LLNode<T> >(new LLNode<T>(item, curr)));
}


// remove a node if its value == item
template<typename T>
template<class rdp, class wrp>
void tm_list_set<T>::remove(const T item)
{
    // find the node whose val matches the request
    rdp prev(head_node);
    rdp curr(prev->get_next_node(prev.v()));
    while (curr != 0) {
        // if we find the node, disconnect it and end the search
        if (curr->get_val(curr.v()) == item) {
            wrp mod_point(prev);     // upgrade to writable
            mod_point->set_next_node(curr->get_next_node(curr.v()));
            tx_delete(curr);
            break;
        }
        prev = curr;
        curr = prev->get_next_node(prev.v());
    }
}

// find out whether item is in list
template<typename T>
template<class rdp>
bool tm_list_set<T>::lookup(const T item)
{
    rdp curr(head_node);
    curr = curr->get_next_node(curr.v());
    while (curr != 0) {
        if (curr->get_val(curr.v()) == item)
            return true;
        curr = curr->get_next_node(curr.v());
    }
    return false;
}

// apply a function to every element of the list
template<typename T>
void tm_list_set<T>::pv_apply_to_all(void (*f)(T item))
{
    stm::un_ptr<LLNode<T> > curr(head_node);
    curr = curr->get_next_node(curr.v());
    while (curr != 0) {
        f(curr->get_val(curr.v()));
        curr = curr->get_next_node(curr.v());
    }
}

// count elements in the list
template<typename T>
int tm_list_set<T>::pv_size() const
{
    int rtn = -1;
    stm::un_ptr<LLNode<T> > curr(head_node);
    while (curr != 0) {
        rtn++;
        curr = curr->get_next_node(curr.v());
    }
    return rtn;
}

#endif
