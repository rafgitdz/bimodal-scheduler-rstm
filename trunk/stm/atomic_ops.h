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

#ifndef __ATOMIC_OPS_H__
#define __ATOMIC_OPS_H__

#if defined(X86)

// gcc x86 CAS and TAS

static inline unsigned long
cas(volatile unsigned long* ptr, unsigned long old, unsigned long _new)
{
    unsigned long prev;
    asm volatile("lock;"
                 "cmpxchgl %1, %2;"
                 : "=a"(prev)
                 : "q"(_new), "m"(*ptr), "a"(old)
                 : "memory");
    return prev;
}

static inline unsigned long tas(volatile unsigned long* ptr)
{
    unsigned long result;
    asm volatile("lock;"
                 "xchgl %0, %1;"
                 : "=r"(result), "=m"(*ptr)
                 : "0"(1), "m"(*ptr)
                 : "memory");
    return result;
}

static inline unsigned long
swap(volatile unsigned long* ptr, unsigned long val)
{
    asm volatile("lock;"
                 "xchgl %0, %1"
                 : "=r"(val), "=m"(*ptr)
                 : "0"(val), "m"(*ptr)
                 : "memory");
    return val;
}

static inline void nop()
{
    asm volatile("nop");
}

// casX for x86 (486 or higher)
static inline unsigned long long casX(volatile unsigned long long* addr,
                                      unsigned long long oldVal,
                                      unsigned long long newVal)
{
    unsigned long long local = oldVal;
    unsigned long ecx = newVal >> 32;
    unsigned long ebx = newVal & 0xFFFFFFFF;

    asm volatile("lock; cmpxchg8b (%4);"
                 : "=A" (local) // A -> output 64bits as eax/edx
                 : "0" (local), // local is also an input (could use oldVal)
                   "c" (ecx), // ecx is the top 32 bits of newVal
                   "b" (ebx), // and ebx is the bottom 32 bits of newVal
                   "r" (addr) // address can go anywhere (and e[abcd]x are
                              // taken)
                 : "memory"); // no clobber regs since all register information
                              // is available to GCC
    return local;
}

// When casX is dealing with packed structs, it is convenient to pass each word
// directly
static inline bool casX(volatile unsigned long long* addr,
                        unsigned long expected_high,
                        unsigned long expected_low,
                        unsigned long new_high,
                        unsigned long new_low)
{
    unsigned long old_high = expected_high;
    unsigned long old_low = expected_low;
    asm volatile("lock; cmpxchg8b (%6);"
                 : "=a" (old_low), "=d" (old_high)
                 : "0" (old_low), "1" (old_high),
                   "c" (new_high), "b" (new_low),
                   "r" (addr)
                 : "memory");
    return (old_high == expected_high) && (old_low == expected_low);
}

#elif defined(SPARC)

// gcc SPARC atomic primitives

// memory barrier to prevent reads from bypassing writes
static inline void membar()
{
    asm volatile("membar #StoreLoad");
}

static inline unsigned long
cas(volatile unsigned long* ptr, unsigned long old, unsigned long _new)
{
    asm volatile("cas [%2], %3, %0"                     // instruction
                 : "=&r"(_new)                          // output
                 : "0"(_new), "r"(ptr), "r"(old)        // inputs
                 : "memory");                           // side effects
    return _new;
}

static inline unsigned long tas(volatile unsigned long* ptr)
{
    unsigned long result;
    asm volatile("ldstub [%1], %0"
                 : "=r"(result)
                 : "r"(ptr)
                 : "memory");
    return result;
}

static inline unsigned long
swap(volatile unsigned long* ptr, unsigned long val)
{
    asm volatile("swap [%2], %0"
                 : "=&r"(val)
                 : "0"(val), "r"(ptr)
                 : "memory");
    return val;
}

// NB: When Solaris is in 32-bit mode, it does not save the top 32 bits of a
// 64-bit local (l) register on context switch, so always use an "o" register
// for 64-bit ops in 32-bit mode

// we can't mov 64 bits directly from c++ to a register, so we must ldx
// pointers to get the data into registers
static inline bool casX(volatile unsigned long long* ptr,
                        unsigned long long* expected_value,
                        unsigned long long* new_value)
{
    bool success = false;

    asm volatile("ldx   [%1], %%o4;"
                 "ldx   [%2], %%o5;"
                 "casx  [%3], %%o4, %%o5;"
                 "cmp   %%o4, %%o5;"
                 "mov   %%g0, %0;"
                 "move  %%xcc, 1, %0"   // predicated move... should do this
                                        // for bool_cas too
                 : "=r"(success)
                 : "r"(expected_value), "r"(new_value), "r"(ptr)
                 : "o4", "o5", "memory");
    return success;
}

// When casX is dealing with packed structs, it is convenient to pass each word
// directly
static inline bool volatile casX(volatile unsigned long long* ptr,
                                 unsigned long expected_high,
                                 unsigned long expected_low,
                                 unsigned long new_high,
                                 unsigned long new_low)
{
    bool success = false;
    asm volatile("sllx %1, 32, %%o4;"
                 "or   %%o4, %2, %%o4;"
                 "sllx %3, 32, %%o5;"
                 "or   %%o5, %4, %%o5;"
                 "casx [%5], %%o4, %%o5;"
                 "cmp  %%o4, %%o5;"
                 "be,pt %%xcc,1f;"
                 "mov  1, %0;"
                 "mov  %%g0, %0;"
                 "1:"
                 : "=r"(success)
                 : "r"(expected_high), "r"(expected_low), "r"(new_high),
                   "r"(new_low), "r"(ptr)
                 : "o4", "o5", "memory");
    return success;
}

static inline void
mvx(volatile unsigned long long* from, volatile unsigned long long* to)
{
    asm volatile("ldx  [%0], %%o4;"
                 "stx  %%o4, [%1];"
                 :
                 : "r"(from), "r"(to)
                 : "o4", "memory");
}

static inline void nop()
{
    asm volatile("nop");
}

#endif

static inline bool
bool_cas(volatile unsigned long* ptr, unsigned long old, unsigned long _new)
{
    return cas(ptr, old, _new) == old;
}

static inline unsigned long fai(volatile unsigned long* ptr)
{
    unsigned long found = *ptr;
    unsigned long expected;
    do {
        expected = found;
    } while ((found = cas(ptr, expected, expected + 1)) != expected);
    return found;
}

// exponential backoff
static inline void backoff(int *b)
{
    for (int i = *b; i; i--)
        nop();

    if (*b < 4096)
        *b <<= 1;
}

////////////////////////////////////////
// tatas lock

typedef volatile unsigned long tatas_lock_t;

static inline void tatas_acquire_slowpath(tatas_lock_t* L)
{
    int b = 64;

    do
    {
        backoff(&b);
    }
    while (tas(L));
}

static inline void tatas_acquire(tatas_lock_t* L)
{
    if (tas(L))
        tatas_acquire_slowpath(L);
}

static inline void tatas_release(tatas_lock_t* L)
{
    *L = 0;
}

////////////////////////////////////////
// ticket lock

extern "C"
{
    typedef struct
    {
        volatile unsigned long next_ticket;
        volatile unsigned long now_serving;
    } ticket_lock_t;
}


static inline void ticket_acquire(ticket_lock_t* L)
{
    unsigned long my_ticket = fai(&L->next_ticket);
    while (L->now_serving != my_ticket);
}

static inline void ticket_release(ticket_lock_t* L)
{
    L->now_serving += 1;
}

////////////////////////////////////////
// MCS lock

extern "C"
{
    typedef volatile struct _mcs_qnode_t
    {
        bool flag;
        volatile struct _mcs_qnode_t* next;
    } mcs_qnode_t;
}

static inline void mcs_acquire(mcs_qnode_t** L, mcs_qnode_t* I)
{
    I->next = 0;
    mcs_qnode_t* pred =
        (mcs_qnode_t*)swap((volatile unsigned long*)L, (unsigned long)I);

    if (pred != 0) {
        I->flag = true;
        pred->next = I;

        while (I->flag)
            ;    // spin
    }
}

static inline void mcs_release(mcs_qnode_t** L, mcs_qnode_t* I)
{
    if (I->next == 0)
    {
        if (bool_cas((volatile unsigned long*)L, (unsigned long)I, 0))
            return;

        while (I->next == 0)
            ;    // spin
    }

    I->next->flag = false;
}

#endif // __ATOMIC_OPS_H__
