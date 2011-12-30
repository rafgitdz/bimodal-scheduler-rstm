/*  tm_hash_set.h
 *
 *  Transactional hash set.
 *  Element type must be the same size as unsigned long.
 *  Currently has only built-in hash function, which is designed to work
 *  well for pointers and stm::sh_ptr<>s.
 */

#ifndef TM_HASH_SET_H
#define TM_HASH_SET_H

#include "tm_set.h"
#include "tm_list_set.h"

template<typename T>
class tm_hash_set : public tm_set<T>
{
    tm_list_set<T> **bucket;
    int num_buckets;

    tm_hash_set(const tm_hash_set&);
        // no implementation; forbids passing by value
    tm_hash_set& operator=(const tm_hash_set&);
        // no implementation; forbids copying

    // Hash function that should work reasonably well for pointers.
    // Basically amounts to cache line number.
    //
    unsigned long hash(T item)
    {
        // [mfs] verbose attributing to avoid gcc error
        T*  __attribute__ ((__may_alias__)) f = &item;
        unsigned long*  __attribute__((__may_alias__)) val =
            reinterpret_cast<unsigned long*>(f);
        return (*val >> 6) % num_buckets;
    }

public:
    virtual void tx_insert(const T item)
    {
        bucket[hash(item)]->tx_insert(item);
    }

    virtual void tx_remove(const T item)
    {
        bucket[hash(item)]->tx_remove(item);
    }

    virtual bool tx_lookup(const T item)
    {
        return bucket[hash(item)]->tx_lookup(item);
    }

    virtual void pv_insert(const T item)
    {
        bucket[hash(item)]->pv_insert(item);
    }

    virtual void pv_remove(const T item)
    {
        bucket[hash(item)]->pv_remove(item);
    }

    virtual bool pv_lookup(const T item)
    {
        return bucket[hash(item)]->pv_lookup(item);
    }

    virtual void pv_apply_to_all(void (*f)(T item))
    {
        for (int i = 0; i < num_buckets; i++)
            bucket[i]->pv_apply_to_all(f);
    }

    tm_hash_set(int capacity)
    {
        // get space for buckets (load factor 1.5)
        num_buckets = capacity + (capacity >> 1);

        bucket = new tm_list_set<T>*[num_buckets];
        for (int i = 0; i < num_buckets; i++)
            bucket[i] = new tm_list_set<T>();
    }

    // Destruction not currently supported.
    virtual ~tm_hash_set() { assert(false); }

    // for debugging (non-thread-safe):
    void print_stats()
    {
        for (int b = 0; b < num_buckets; b++) {
            if (b % 8 == 0)
                cout << "\n";
            cout << "\t" << bucket[b]->pv_size();
        }
        if (num_buckets % 8 != 0)
            cout << "\n";
    }
};

#endif
