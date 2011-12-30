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

#ifndef __STM_STM_API_H__
#define __STM_STM_API_H__

#ifdef RSTM
#include "rstm.h"
#endif

#ifdef REDO_LOCK
#include "redo_lock.h"
#endif

#ifdef CGL
#include "cgl.h"
#endif

#include <cassert>

namespace stm
{
    template <class> class tx_ptr;
    template <class> class sh_ptr;
    template <class> class rd_ptr;
    template <class> class wr_ptr;
    template <class> class un_ptr;

    template <class T> void tx_release(rd_ptr<T>& to_release);

    template <class T> void tx_delete(sh_ptr<T>& delete_on_tx_commit);
    template <class T> void tx_delete(rd_ptr<T>& delete_on_tx_commit);
    template <class T> void tx_delete(wr_ptr<T>& delete_on_tx_commit);
    template <class T> void tx_delete(un_ptr<T>& delete_immediately);

    namespace internal
    {
        /// tx_ptr groups together some common transactional pointer operations
        /// See Andreis Alexandrescu's "Modern C++ Design" for a discussion of
        /// testing smart pointers for NULL. In particular, we use his Tester
        /// helper class to allow <code>if (sp)</code> tests.
        ///
        /// Note: the <code>if (sp)</code> test is not currently functional.
        template <class T>
        class tx_ptr
        {
          protected:
            Shared<T>* m_shared;

            tx_ptr();
            tx_ptr(Shared<T>*);

          public:

            bool operator!() const;                     /// if !(sp)

            inline friend
            bool stm::operator==(const void* lhs, const tx_ptr<T>& rhs)
            {
                return (lhs == static_cast<void*>(rhs.m_shared));
            }

            inline friend
            bool stm::operator!=(const void* lhs, const tx_ptr<T>& rhs)
            {
                return (lhs != static_cast<void*>(rhs.m_shared));
            }

        }; // template <class> class tx_ptr
    } // namespace internal


    template <class T>
    class sh_ptr : public internal::tx_ptr<T>
    {
        friend class rd_ptr<T>;
        friend class wr_ptr<T>;
        friend class un_ptr<T>;

        friend void tx_delete<>(sh_ptr<T>&);

      public:
        sh_ptr();
        explicit sh_ptr(T*);
        sh_ptr(const sh_ptr<T>& copy);
        sh_ptr(const rd_ptr<T>& cast);
        sh_ptr(const wr_ptr<T>& cast);
        sh_ptr(const un_ptr<T>& cast);

        sh_ptr<T>& operator=(const int must_be_null);
        sh_ptr<T>& operator=(const sh_ptr<T>& assign);
        sh_ptr<T>& operator=(const rd_ptr<T>&);
        sh_ptr<T>& operator=(const wr_ptr<T>&);
        sh_ptr<T>& operator=(const un_ptr<T>&);

        bool operator==(const void*) const;
        bool operator!=(const void*) const;
        bool operator==(const sh_ptr<T>&) const;
        bool operator!=(const sh_ptr<T>&) const;
        bool operator==(const rd_ptr<T>&) const;
        bool operator!=(const rd_ptr<T>&) const;
        bool operator==(const wr_ptr<T>&) const;
        bool operator!=(const wr_ptr<T>&) const;
        bool operator==(const un_ptr<T>&) const;
        bool operator!=(const un_ptr<T>&) const;

        // so that sh_ptr<T>s can be stored in STL collections
        bool operator<(const sh_ptr<T>& rhs) const;
    }; // template <class> class sh_ptr


    template <class T>
    class rd_ptr : public internal::tx_ptr<T>
    {
        friend class sh_ptr<T>;
        friend class wr_ptr<T>;

        friend void tx_release<>(rd_ptr<T>&);
        friend void tx_delete<>(rd_ptr<T>&);

      private:
        mutable const T*      m_t;
        internal::Descriptor& m_tx;
        internal::Validator   m_v;

        void open();

      public:
        // To facilitate private/txnal method templates:
        typedef wr_ptr<T> upgrade;
        template<class O> struct r_analogue {
            typedef rd_ptr<O> type;
        };
        template<class O> struct w_analogue {
            typedef wr_ptr<O> type;
        };

        rd_ptr();
        rd_ptr(const int must_be_null);
        rd_ptr(const rd_ptr<T>& copy);
        rd_ptr(const wr_ptr<T>& open);
        explicit rd_ptr(internal::Descriptor& tx);
        explicit rd_ptr(const sh_ptr<T>& to_open);
        rd_ptr(internal::Descriptor& tx, const sh_ptr<T>& to_open);

        rd_ptr<T>& operator=(const int must_be_null);
        rd_ptr<T>& operator=(const rd_ptr<T>& assign);
        rd_ptr<T>& operator=(const wr_ptr<T>& assign);
        rd_ptr<T>& operator=(const sh_ptr<T>& to_open);

        const T* operator->() const;
        const T& operator*() const;

        bool operator==(const void*) const;
        bool operator!=(const void*) const;
        bool operator==(const rd_ptr<T>&) const;
        bool operator!=(const rd_ptr<T>&) const;
        bool operator==(const sh_ptr<T>&) const;
        bool operator!=(const sh_ptr<T>&) const;


        const internal::Validator& v() const;
    }; // template <class> class rd_ptr


    template <class T>
    class wr_ptr : public internal::tx_ptr<T>
    {
        friend class sh_ptr<T>;
        friend class rd_ptr<T>;

        friend void tx_delete<>(wr_ptr<T>&);

      private:
        mutable T*            m_t;
        internal::Descriptor& m_tx;
        internal::Validator   m_v;

        void open();

      public:
        // To facilitate private/txnal method templates:
        template<class O> struct r_analogue {
            typedef rd_ptr<O> type;
        };
        template<class O> struct w_analogue {
            typedef wr_ptr<O> type;
        };

        wr_ptr();
        wr_ptr(const int must_be_null);
        wr_ptr(const wr_ptr<T>&);
        explicit wr_ptr(rd_ptr<T>&);
        explicit wr_ptr(internal::Descriptor&);
        explicit wr_ptr(const sh_ptr<T>&);
        wr_ptr(internal::Descriptor&, const sh_ptr<T>&);

        wr_ptr<T>& operator=(const int must_be_null);
        wr_ptr<T>& operator=(const wr_ptr<T>&);
        wr_ptr<T>& operator=(const sh_ptr<T>&);

        T* operator->() const;
        T& operator*() const;

        bool operator==(const void*) const;
        bool operator!=(const void*) const;
        bool operator==(const wr_ptr<T>&) const;
        bool operator!=(const wr_ptr<T>&) const;
        bool operator==(const sh_ptr<T>&) const;
        bool operator!=(const sh_ptr<T>&) const;


        const internal::Validator& v() const;
    }; // template <class> class wr_ptr


    template <class T>
    class un_ptr : public internal::tx_ptr<T>
    {
        friend class sh_ptr<T>;
        friend void tx_delete<>(un_ptr<T>&);

      private:
        T*                  m_t;
        internal::Validator m_v;    // Validation is not necessary...


        void on_dereference() const;

      public:
        // To facilitate private/txnal method templates:
        typedef un_ptr<T> upgrade;
        template<class O> struct r_analogue {
            typedef un_ptr<O> type;
        };
        template<class O> struct w_analogue {
            typedef un_ptr<O> type;
        };

        un_ptr();
        un_ptr(const int must_be_null);
        un_ptr<T>(const un_ptr<T>& copy);
        explicit un_ptr<T>(const sh_ptr<T>& to_make_private);

        un_ptr<T>& operator=(const int must_be_null);
        un_ptr<T>& operator=(const sh_ptr<T>&);
        un_ptr<T>& operator=(const un_ptr<T>&);

        bool operator==(const void*) const;
        bool operator!=(const void*) const;
        bool operator==(const un_ptr<T>&) const;
        bool operator!=(const un_ptr<T>&) const;
        bool operator==(const sh_ptr<T>&) const;
        bool operator!=(const sh_ptr<T>&) const;

        const T* operator->() const;
        T* operator->();
        const T& operator*() const;
        T& operator*();

        // Validation is not necessary, and since m_t isn't const, we won't
        // actually use the validators, but still we need to have a v() method
        // in order to match the signature of accessor calls
        const internal::Validator& v() const;
    }; // template <class> class un_ptr

} // namespace stm

namespace stm
{
    namespace internal
    {
        template <class T>
        inline tx_ptr<T>::tx_ptr()
            : m_shared(NULL)
        { }

        template <class T>
        inline tx_ptr<T>::tx_ptr(Shared<T>* shared)
            : m_shared(shared)
        { }

        template <typename T>
        inline bool tx_ptr<T>::operator!() const
        {
            return (NULL == m_shared);
        }

    } // namespace internal
} // namespace stm


namespace stm
{
    template <class T>
    inline sh_ptr<T>::sh_ptr() { }

    template <class T>
    inline sh_ptr<T>::sh_ptr(T* t)
        : internal::tx_ptr<T>(internal::Shared<T>::CreateShared(t))
    { }

    template <class T>
    inline sh_ptr<T>::sh_ptr(const sh_ptr<T>& ptr)
        : internal::tx_ptr<T>(ptr.m_shared)
    { }

    template <class T>
    inline sh_ptr<T>::sh_ptr(const rd_ptr<T>& ptr)
        : internal::tx_ptr<T>(ptr.m_shared)
    { }

    template <class T>
    inline sh_ptr<T>::sh_ptr(const wr_ptr<T>& ptr)
        : internal::tx_ptr<T>(ptr.m_shared)
    { }

    template <class T>
    inline sh_ptr<T>::sh_ptr(const un_ptr<T>& ptr)
        : internal::tx_ptr<T>(ptr.m_shared)
    { }

    template <class T>
    inline sh_ptr<T>& sh_ptr<T>::operator=(const int must_be_null)
    {
        assert(must_be_null == 0);
        internal::tx_ptr<T>::m_shared = NULL;
        return *this;
    }

    template <class T>
    inline sh_ptr<T>& sh_ptr<T>::operator=(const sh_ptr<T>& rhs)
    {
        internal::tx_ptr<T>::m_shared = rhs.m_shared;
        return *this;
    }

    template <class T>
    inline sh_ptr<T>& sh_ptr<T>::operator=(const rd_ptr<T>& rhs)
    {
        internal::tx_ptr<T>::m_shared = rhs.m_shared;
        return *this;
    }

    template <class T>
    inline sh_ptr<T>& sh_ptr<T>::operator=(const wr_ptr<T>& rhs)
    {
        internal::tx_ptr<T>::m_shared = rhs.m_shared;
        return *this;
    }

    template <class T>
    inline sh_ptr<T>& sh_ptr<T>::operator=(const un_ptr<T>& rhs)
    {
        internal::tx_ptr<T>::m_shared = rhs.m_shared;
        return *this;
    }

    template <class T>
    inline bool sh_ptr<T>::operator==(const void* rhs) const
    {
        return (internal::tx_ptr<T>::m_shared == rhs);
    }

    template <class T>
    inline bool sh_ptr<T>::operator!=(const void* rhs) const
    {
        return (internal::tx_ptr<T>::m_shared != rhs);
    }

    template <class T>
    inline bool sh_ptr<T>::operator==(const sh_ptr<T>& rhs) const
    {
        return (internal::tx_ptr<T>::m_shared == rhs.m_shared);
    }

    template <class T>
    inline bool sh_ptr<T>::operator!=(const sh_ptr<T>& rhs) const
    {
        return (internal::tx_ptr<T>::m_shared != rhs.m_shared);
    }

    template <class T>
    inline bool sh_ptr<T>::operator==(const rd_ptr<T>& rhs) const
    {
        return (internal::tx_ptr<T>::m_shared == rhs.m_shared);
    }

    template <class T>
    inline bool sh_ptr<T>::operator!=(const rd_ptr<T>& rhs) const
    {
        return (internal::tx_ptr<T>::m_shared != rhs.m_shared);
    }

    template <class T>
    inline bool sh_ptr<T>::operator==(const wr_ptr<T>& rhs) const
    {
        return (internal::tx_ptr<T>::m_shared == rhs.m_shared);
    }

    template <class T>
    inline bool sh_ptr<T>::operator!=(const wr_ptr<T>& rhs) const
    {
        return (internal::tx_ptr<T>::m_shared != rhs.m_shared);
    }

    template <class T>
    inline bool sh_ptr<T>::operator==(const un_ptr<T>& rhs) const
    {
        return (internal::tx_ptr<T>::m_shared == rhs.m_shared);
    }

    template <class T>
    inline bool sh_ptr<T>::operator!=(const un_ptr<T>& rhs) const
    {
        return (internal::tx_ptr<T>::m_shared != rhs.m_shared);
    }

    template <class T>
    inline bool sh_ptr<T>::operator<(const sh_ptr<T>& rhs) const
    {
        return (internal::tx_ptr<T>::m_shared < rhs.m_shared);
    }
}

namespace stm
{
    template <class T>
    inline void rd_ptr<T>::open()
    {
        m_t = internal::tx_ptr<T>::m_shared->open_RO(m_tx, m_v);
    }

    template <class T>
    inline rd_ptr<T>::rd_ptr()
        : m_t(NULL),
          m_tx(internal::Descriptor::CurrentThreadInstance()),
          m_v()
    {
        internal::Descriptor::assert_transactional();
    }

    template <class T>
    inline rd_ptr<T>::rd_ptr(const int must_be_null)
        : m_t(NULL),
          m_tx(internal::Descriptor::CurrentThreadInstance()),
          m_v()
    {
        assert(must_be_null == 0);
        internal::Descriptor::assert_transactional();
    }

    template <class T>
    inline rd_ptr<T>::rd_ptr(const rd_ptr<T>& ptr)
        : internal::tx_ptr<T>(ptr.m_shared),
          m_t(ptr.m_t),
          m_tx(ptr.m_tx),
          m_v(ptr.m_v)
    {
        internal::Descriptor::assert_transactional();
    }

    template <class T>
    inline rd_ptr<T>::rd_ptr(internal::Descriptor& tx)
        : m_t(NULL),
          m_tx(tx),
          m_v()
    {
        internal::Descriptor::assert_transactional();
    }

    template <class T>
    inline rd_ptr<T>::rd_ptr(const sh_ptr<T>& shared)
        : internal::tx_ptr<T>(shared.m_shared),
          m_t(NULL),
          m_tx(internal::Descriptor::CurrentThreadInstance()),
          m_v()
    {
        internal::Descriptor::assert_transactional();
        open();
    }

    template <class T>
    inline rd_ptr<T>::rd_ptr(internal::Descriptor& tx, const sh_ptr<T>& shared)
        : internal::tx_ptr<T>(shared.m_shared),
          m_t(NULL),
          m_tx(tx),
          m_v()
    {
        internal::Descriptor::assert_transactional();
        open();
    }

    template <class T>
    inline rd_ptr<T>::rd_ptr(const wr_ptr<T>& wr)
        : internal::tx_ptr<T>(wr.m_shared),
          m_t(wr.m_t),
          m_tx(wr.m_tx),
          m_v(wr.m_v)
    {
        internal::Descriptor::assert_transactional();
    }

    template <class T>
    inline rd_ptr<T>& rd_ptr<T>::operator=(const int must_be_null)
    {
        assert(must_be_null == 0);
        internal::tx_ptr<T>::m_shared = NULL;
        return *this;
    }

    template <class T>
    inline rd_ptr<T>& rd_ptr<T>::operator=(const wr_ptr<T>& rhs)
    {
        internal::tx_ptr<T>::m_shared = rhs.m_shared;
        m_t = rhs.m_t;
        m_v = rhs.m_v;
        return *this;
    }

    template <class T>
    inline rd_ptr<T>& rd_ptr<T>::operator=(const rd_ptr<T>& rhs)
    {
        internal::tx_ptr<T>::m_shared = rhs.m_shared;
        m_t = rhs.m_t;
        m_v = rhs.m_v;
        return *this;
    }

    template <class T>
    inline rd_ptr<T>& rd_ptr<T>::operator=(const sh_ptr<T>& rhs)
    {
        internal::tx_ptr<T>::m_shared = rhs.m_shared;
        open();
        return *this;
    }

    template <class T>
    const T* rd_ptr<T>::operator->() const
    {
        assert(internal::tx_ptr<T>::m_shared->check_alias(m_t, m_tx));
        return m_t;
    }

    template <class T>
    const T& rd_ptr<T>::operator*() const
    {
        assert(internal::tx_ptr<T>::m_shared->check_alias(m_t, m_tx));
        return *m_t;
    }

    template <class T>
    inline bool rd_ptr<T>::operator==(const void* rhs) const
    {
        return (internal::tx_ptr<T>::m_shared == rhs);
    }

    template <class T>
    inline bool rd_ptr<T>::operator!=(const void* rhs) const
    {
        return (internal::tx_ptr<T>::m_shared != rhs);
    }

    template <class T>
    inline bool rd_ptr<T>::operator==(const rd_ptr<T>& rhs) const
    {
        return (internal::tx_ptr<T>::m_shared == rhs.m_shared);
    }

    template <class T>
    inline bool rd_ptr<T>::operator!=(const rd_ptr<T>& rhs) const
    {
        return (internal::tx_ptr<T>::m_shared != rhs.m_shared);
    }

    template <class T>
    inline bool rd_ptr<T>::operator==(const sh_ptr<T>& rhs) const
    {
        return (internal::tx_ptr<T>::m_shared == rhs.m_shared);
    }

    template <class T>
    inline bool rd_ptr<T>::operator!=(const sh_ptr<T>& rhs) const
    {
        return (internal::tx_ptr<T>::m_shared != rhs.m_shared);
    }

    template <class T>
    inline const internal::Validator& rd_ptr<T>::v() const
    {
        return m_v;
    }
}

namespace stm
{
    template <class T>
    inline void wr_ptr<T>::open()
    {
        m_t = internal::tx_ptr<T>::m_shared->open_RW(m_tx, m_v);
    }

    template <class T>
    inline wr_ptr<T>::wr_ptr()
        : m_t(NULL),
          m_tx(internal::Descriptor::CurrentThreadInstance()),
          m_v()
    {
        internal::Descriptor::assert_transactional();
    }

    template <class T>
    inline wr_ptr<T>::wr_ptr(const int must_be_null)
        : m_t(NULL),
          m_tx(internal::Descriptor::CurrentThreadInstance()),
          m_v()
    {
        assert(must_be_null == 0);
        internal::Descriptor::assert_transactional();
    }

    template <class T>
    inline wr_ptr<T>::wr_ptr(const wr_ptr<T>& ptr)
        : internal::tx_ptr<T>(ptr.m_shared),
          m_t(ptr.m_t),
          m_tx(ptr.m_tx),
          m_v(ptr.m_v)
    {
        internal::Descriptor::assert_transactional();
    }

    template <class T>
    inline wr_ptr<T>::wr_ptr(rd_ptr<T>& ptr)
        : internal::tx_ptr<T>(ptr.m_shared),
          m_t(NULL),
          m_tx(ptr.m_tx),
          m_v(ptr.m_v)
    {
        internal::Descriptor::assert_transactional();
        open();

        // Reset the readable so that it's pointing to the write copy
        ptr.m_t = m_t;
        ptr.m_v = m_v;
    }

    template <class T>
    inline wr_ptr<T>::wr_ptr(internal::Descriptor& tx)
        : m_t(NULL),
          m_tx(tx),
          m_v()
    {
        internal::Descriptor::assert_transactional();
    }

    template <class T>
    inline wr_ptr<T>::wr_ptr(const sh_ptr<T>& ptr)
        : internal::tx_ptr<T>(ptr.m_shared),
          m_t(NULL),
          m_tx(internal::Descriptor::CurrentThreadInstance()),
          m_v()
    {
        internal::Descriptor::assert_transactional();
        open();
    }

    template <class T>
    inline wr_ptr<T>::wr_ptr(internal::Descriptor& tx, const sh_ptr<T>& ptr)
        : internal::tx_ptr<T>(ptr.m_shared),
          m_t(NULL),
          m_tx(tx),
          m_v()
    {
        internal::Descriptor::assert_transactional();
        open();
    }

    template <class T>
    inline wr_ptr<T>& wr_ptr<T>::operator=(const int must_be_null)
    {
        assert(must_be_null == 0);
        internal::tx_ptr<T>::m_shared = NULL;
        return *this;
    }

    template <class T>
    inline wr_ptr<T>& wr_ptr<T>::operator=(const wr_ptr<T>& ptr)
    {
        internal::tx_ptr<T>::m_shared = ptr.m_shared;
        m_t = ptr.m_t;
        m_v = ptr.m_v;
        return *this;
    }

    template <class T>
    inline wr_ptr<T>& wr_ptr<T>::operator=(const sh_ptr<T>& ptr)
    {
        internal::tx_ptr<T>::m_shared = ptr.m_shared;
        open();
        return *this;
    }

    template <class T>
    T* wr_ptr<T>::operator->() const
    {
        return m_t;
    }

    template <class T>
    T& wr_ptr<T>::operator*() const
    {
        return *m_t;
    }

    template <class T>
    inline bool wr_ptr<T>::operator==(const void* rhs) const
    {
        return (internal::tx_ptr<T>::m_shared == rhs);
    }

    template <class T>
    inline bool wr_ptr<T>::operator!=(const void* rhs) const
    {
        return (internal::tx_ptr<T>::m_shared != rhs);
    }

    template <class T>
    inline bool wr_ptr<T>::operator==(const wr_ptr<T>& rhs) const
    {
        return (internal::tx_ptr<T>::m_shared == rhs.m_shared);
    }

    template <class T>
    inline bool wr_ptr<T>::operator!=(const wr_ptr<T>& rhs) const
    {
        return (internal::tx_ptr<T>::m_shared != rhs.m_shared);
    }

    template <class T>
    inline bool wr_ptr<T>::operator==(const sh_ptr<T>& rhs) const
    {
        return (internal::tx_ptr<T>::m_shared == rhs.m_shared);
    }

    template <class T>
    inline bool wr_ptr<T>::operator!=(const sh_ptr<T>& rhs) const
    {
        return (internal::tx_ptr<T>::m_shared != rhs.m_shared);
    }

    template <class T>
    inline const internal::Validator& wr_ptr<T>::v() const
    {
        return m_v;
    }
}

#ifdef rstm
#include "Descriptor_rstm.h"
#endif

#ifdef redo_lock
#include "Descriptor_redo_lock.h"
#endif

#ifdef cgl
#include "Descriptor_cgl.h"
#endif

namespace stm
{
    template <class T>
    inline void un_ptr<T>::on_dereference() const
    {
        internal::tx_ptr<T>::m_shared->verify_un(m_t);
    }

    template <class T>
    inline un_ptr<T>::un_ptr()
        : internal::tx_ptr<T>(NULL),
          m_t(NULL),
          m_v()
    {
        internal::Descriptor::assert_nontransactional();
    }

    template <class T>
    inline un_ptr<T>::un_ptr(const int must_be_null)
        : internal::tx_ptr<T>(NULL),
          m_t(NULL),
          m_v()
    {
        internal::Descriptor::assert_nontransactional();
        assert(must_be_null == 0);
    }

    template <class T>
    inline un_ptr<T>::un_ptr(const un_ptr<T>& copy)
        : internal::tx_ptr<T>(copy.m_shared),
          m_t(copy.m_t),
          m_v()
    {
        internal::Descriptor::assert_nontransactional();
    }

    template <class T>
    inline un_ptr<T>::un_ptr(const sh_ptr<T>& shared)
        : internal::tx_ptr<T>(shared.m_shared),
          m_t(),
          m_v()
    {
        internal::Descriptor::assert_nontransactional();
        m_t = internal::tx_ptr<T>::m_shared->open_un(m_v);
    }

    template <class T>
    inline un_ptr<T>& un_ptr<T>::operator=(const int must_be_null)
    {
        internal::Descriptor::assert_nontransactional();
        assert(must_be_null == 0);
        internal::tx_ptr<T>::m_shared = NULL;
        return *this;
    }

    template <class T>
    inline un_ptr<T>& un_ptr<T>::operator=(const un_ptr<T>& rhs)
    {
        internal::Descriptor::assert_nontransactional();
        internal::tx_ptr<T>::m_shared = rhs.m_shared;
        m_t = rhs.m_t;
        m_v = rhs.m_v;
        return *this;
    }

    template <class T>
    inline un_ptr<T>& un_ptr<T>::operator=(const sh_ptr<T>& ptr)
    {
        internal::Descriptor::assert_nontransactional();
        internal::tx_ptr<T>::m_shared = ptr.m_shared;
        m_t = internal::tx_ptr<T>::m_shared->open_un(m_v);
        return *this;
    }

    template <class T>
    inline bool un_ptr<T>::operator==(const void* rhs) const
    {
        internal::Descriptor::assert_nontransactional();
        return (static_cast<void*>(internal::tx_ptr<T>::m_shared) == rhs);
    }

    template <class T>
    inline bool un_ptr<T>::operator!=(const void* rhs) const
    {
        internal::Descriptor::assert_nontransactional();
        return (static_cast<void*>(internal::tx_ptr<T>::m_shared) != rhs);
    }

    template <class T>
    inline bool un_ptr<T>::operator==(const un_ptr<T>& copy) const
    {
        internal::Descriptor::assert_nontransactional();
        return (internal::tx_ptr<T>::m_shared == copy.m_shared);
    }

    template <class T>
    inline bool un_ptr<T>::operator!=(const un_ptr<T>& copy) const
    {
        internal::Descriptor::assert_nontransactional();
        return (internal::tx_ptr<T>::m_shared != copy.m_shared);
    }

    template <class T>
    inline bool un_ptr<T>::operator==(const sh_ptr<T>& rhs) const
    {
        internal::Descriptor::assert_nontransactional();
        return (internal::tx_ptr<T>::m_shared == rhs.m_shared);
    }

    template <class T>
    inline bool un_ptr<T>::operator!=(const sh_ptr<T>& rhs) const
    {
        internal::Descriptor::assert_nontransactional();
        return (internal::tx_ptr<T>::m_shared != rhs.m_shared);
    }

    template <class T>
    inline const T* un_ptr<T>::operator->() const
    {
        internal::Descriptor::assert_nontransactional();
        on_dereference();
        return m_t;
    }

    template <class T>
    inline T* un_ptr<T>::operator->()
    {
        internal::Descriptor::assert_nontransactional();
        on_dereference();
        return m_t;
    }

    template <class T>
    inline const T& un_ptr<T>::operator*() const
    {
        internal::Descriptor::assert_nontransactional();
        on_dereference();
        return *m_t;
    }

    template <class T>
    inline T& un_ptr<T>::operator*()
    {
        internal::Descriptor::assert_nontransactional();
        on_dereference();
        return *m_t;
    }

    template <class T>
    inline const internal::Validator& un_ptr<T>::v() const
    {
        return m_v;
    }
}

namespace stm
{
    template <class T>
    inline void tx_release(rd_ptr<T>& to_release)
    {
        to_release.m_tx.release(to_release.m_shared);
    }

    template <class T>
    inline void tx_delete(sh_ptr<T>& to_delete)
    {
        // Can only tx_delete a sh_ptr inside a transaction... we'll open the
        // shared as ro and delete that
        rd_ptr<T> rd(to_delete);
        tx_delete(rd);
    }

    template <class T>
    inline void tx_delete(rd_ptr<T>& to_delete)
    {
        internal::Shared<T>::DeleteShared(to_delete.m_tx, to_delete.m_shared);
    }

    template <class T>
    inline void tx_delete(wr_ptr<T>& to_delete)
    {
        internal::Shared<T>::DeleteShared(to_delete.m_tx, to_delete.m_shared);
    }

    template <class T>
    inline void tx_delete(un_ptr<T>& to_delete)
    {
        internal::Shared<T>::DeleteSharedUn(to_delete.m_shared);
    }
}


#endif // __STM_STM_API_H__
