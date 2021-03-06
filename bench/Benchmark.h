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

#ifndef __BENCHMARK_H__
#define __BENCHMARK_H__

#include <iostream>
#include <string>

#include "stm_api.h"

inline void argError(std::string reason)
{
    std::cerr << "Argument Error: " << reason << std::endl;
    exit(-1);
}

struct BenchmarkConfig
{
    int duration;                       // in seconds
    int datasetsize;                    // number of items
    int threads;
    int verbosity;                      // in {0, 1, 2}, lower => less output
    bool verify;
    std::string cm_type;
    std::string bm_name;
    std::string stm_validation;
    bool use_static_cm;
    // these three are for getting various lookup/insert/remove ratios
    const int NUM_OUTCOMES;
    int lThresh;
    int iThresh;
    // warmup and execute are used when we aren't using a duration but instead
    // are testing the code for a fixed number of transactions
    int warmup;
    int execute;
    char unit_testing;

    BenchmarkConfig()
        : duration(5), datasetsize(256), threads(2), verbosity(1),
          verify(true), cm_type("Polka"), bm_name("RBTree"),
          stm_validation("invis-eager"), use_static_cm(true),
          NUM_OUTCOMES(30), lThresh(10), iThresh(20),
          warmup(0), execute(0), unit_testing(' ') { }

    void verifyParameters();
    void printConfig();
} __attribute__ ((aligned(64)));

// BMCONFIG is declared in BenchMain.cpp
extern BenchmarkConfig BMCONFIG;

enum TxnOps_t { TXN_ID,          TXN_GENERIC,      TXN_INSERT, TXN_REMOVE,
                TXN_LOOKUP_TRUE, TXN_LOOKUP_FALSE, TXN_NUM_OPS};
enum VerifyLevel_t { LIGHT, HEAVY };

class Benchmark;
struct thread_args_t
{
    int id;
    Benchmark* b;
    int duration;
    int threads;
    long count[TXN_NUM_OPS];
    int warmup;
    int execute;
} __attribute__ ((aligned(64)));

// all benchmarks have the same interface
class Benchmark
{
  public:
    virtual void random_transaction(thread_args_t* args, unsigned int* seed,
                                    unsigned int val, unsigned int chance) = 0;
    virtual bool sanity_check() const = 0;
    void measure_speed();
    virtual bool verify(VerifyLevel_t v) = 0;
    virtual ~Benchmark() { }
};

#endif // __BENCHMARK_H__
