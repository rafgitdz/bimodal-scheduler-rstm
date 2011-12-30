/*  tm_set.h
 *
 *  'pv' versions of functions are meant to be called only when the set
 *  has been privatized.
 */

#ifndef TM_SET_H
#define TM_SET_H

template<typename T>
class tm_set {
  public:
    virtual void tx_insert(const T item) = 0;
    virtual void tx_remove(const T item) = 0;
    virtual bool tx_lookup(const T item) = 0;

    virtual void pv_insert(const T item) = 0;
    virtual void pv_remove(const T item) = 0;
    virtual bool pv_lookup(const T item) = 0;

    virtual void pv_apply_to_all(void (*f)(T item)) = 0;

    virtual ~tm_set() { }
};

#endif
