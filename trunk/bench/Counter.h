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

#ifndef __BENCH_COUNTER_H__
#define __BENCH_COUNTER_H__

#include <stm_api.h>

#include "Benchmark.h"

namespace bench
{
    class Counter : public stm::Object<Counter>
    {
        GENERATE_FIELD(int, value);

      public:

        // clone method for use in RSTM
        virtual Counter* clone() const
        {
            int value = m_value;
            return new Counter(value);
        }

#ifdef NEED_REDO_METHOD
        // redo method for use in redo_lock
        virtual void redo(SharedBase* c_sh)
        {
            Counter* c = static_cast<Counter*>(c_sh);
            m_value = c->m_value;
        }
#endif


        Counter(int startingValue = 0) : m_value(startingValue) { }


        // increment the counter
        void increment(const stm::internal::Validator& v)
        {
            set_value(get_value(v) + 1);
        }
    };



    class CounterBench : public Benchmark
    {
      private:

        stm::sh_ptr<Counter> m_counter;

      public:

        CounterBench() : m_counter(new Counter())
        { }


        void random_transaction(thread_args_t* args,
                                unsigned int* seed,
                                unsigned int val,
                                unsigned int chance)
        {
            BEGIN_TRANSACTION;
            stm::wr_ptr<Counter> wr(m_counter);
            wr->increment(wr.v());
            END_TRANSACTION;
        }


        bool sanity_check() const
        {
            // not as useful as it could be...
            int val = 0;
            BEGIN_TRANSACTION;
            stm::rd_ptr<Counter> rd(m_counter);
            val = rd->get_value(rd.v());
            END_TRANSACTION;
            return (val > 0);
        }


        // no data structure verification is implemented for the Counter...
        // yet
        virtual bool verify(VerifyLevel_t v) {
            return true;
        }
    };

}   // namespace bench

#endif  // __BENCH_COUNTER_H__
