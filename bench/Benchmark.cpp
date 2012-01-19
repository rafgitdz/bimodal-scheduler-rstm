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

#include <unistd.h>

#include <pthread.h>
#include <iostream>
#include <assert.h>
#include <signal.h>
#include <vector>

#include "stm/atomic_ops.h"
#include "stm/hrtime.h"

#include "stm_api.h"
#include "Benchmark.h"

#ifndef USE_BIMODAL
#define USE_BIMODAL
#endif

#ifdef USE_BIMODAL
#include "scheduler/BiModalScheduler.h"
#endif

using std::cout;
using std::endl;

static volatile bool Green_light __attribute__ ((aligned(64))) = false;
static std::string TxnOpDesc[TXN_NUM_OPS]  __attribute__ ((aligned(64))) =
    {"      ID", "Total   ", "Insert  ", "Remove  ", "Lookup T", "Lookup F"};

// signal handler to end the test
static void catch_SIGALRM(int sig_num)
{
    Green_light = false;
    stm::halt_all_transactions();
}

void barrier(int id, unsigned long nthreads)
{
    static struct
    {
        volatile unsigned long count;
        volatile unsigned long sense;
        volatile unsigned long thread_sense[stm::MAX_THREADS];
    } __attribute__ ((aligned(64))) bar = {0};

    bar.thread_sense[id] = !bar.thread_sense[id];
    if (fai(&bar.count) == nthreads - 1) {
        bar.count = 0;
        bar.sense = !bar.sense;
    }
    else
        while (bar.sense != bar.thread_sense[id]);     // spin
}

static void* fast_forward(void* arg)
{
    thread_args_t* args  = (thread_args_t*)arg;
    int            id    = args->id;
    unsigned int   seed  = id;
    int            i;
    int warmup = args->warmup;

    // choose actions before timing starts
    unsigned int* vals   = (unsigned int*)malloc(sizeof(unsigned int)*10000);
    unsigned int* chance = (unsigned int*)malloc(sizeof(unsigned int)*10000);
    for (i = 0; i < 10000; i++) {
        vals[i] = rand_r(&seed);
        chance[i] = rand_r(&seed) % BMCONFIG.NUM_OUTCOMES;
    }

    Benchmark* b = args->b;
    i = 0;

    // do fast_forward
    for (int w = 0; w < warmup; w++) {
        b->random_transaction(args, &seed, vals[i], chance[i]);
        ++i %= 10000;
    }

    return 0;
}

static void* work_thread(void* arg)
{
	

#ifdef USE_BIMODAL
	stm::scheduler::BiModalScheduler::init();
#endif


    thread_args_t* args  = (thread_args_t*)arg;
    int            id    = args->id;
    unsigned int   seed  = id;
    int            i;

    int warmup = args->warmup;
    int execute = args->execute;

    // choose actions before timing starts
    unsigned int* vals   = (unsigned int*)malloc(sizeof(unsigned int)*10000);
    unsigned int* chance = (unsigned int*)malloc(sizeof(unsigned int)*10000);

    for (i = 0; i < 10000; i++) {
        vals[i] = rand_r(&seed);
        chance[i] = rand_r(&seed) % BMCONFIG.NUM_OUTCOMES;
    }

   /* if (id != 0)
        stm::init(BMCONFIG.cm_type, BMCONFIG.stm_validation,
                  BMCONFIG.use_static_cm); */
    Benchmark* b = args->b;
    i = 0;


    barrier(args->id, args->threads);

    // do warmup. for simulation warmup is done by fast forwarding
    for (int w = 0; w < warmup; w++) {
        b->random_transaction(args, &seed, vals[i], chance[i]);
        ++args->count[TXN_GENERIC];
        ++i %= 10000;
    }
    barrier(args->id, args->threads);


    if (execute > 0) {
        for (int e = 0; e < execute; e++) {
            b->random_transaction(args, &seed, vals[i], chance[i]);
            ++args->count[TXN_GENERIC];
            ++i %= 10000;
        }
    }
    else {
        do {
            b->random_transaction(args, &seed, vals[i], chance[i]);
            if (Green_light)
                ++args->count[TXN_GENERIC];
            ++i %= 10000;
        } while (Green_light);
    }

    barrier(args->id, args->threads);

    // Shut down STM for all threads other than thread 0, which may need to run
    // a sanity check
    if (args->id != 0)
        stm::shutdown();

    
    return 0;
}

void Benchmark::measure_speed()
{
    std::vector<thread_args_t> args;
    args.resize(BMCONFIG.threads);

    std::vector<pthread_t> tid;
    tid.resize(BMCONFIG.threads);


    // create <threads> threads, passing info via args
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);
    for (int i = 0; i < BMCONFIG.threads; i++) {
        args[i].id = i;
        args[i].b = this;
        args[i].duration = BMCONFIG.duration;
        args[i].threads = BMCONFIG.threads;
        args[i].warmup = BMCONFIG.warmup;
        args[i].execute = BMCONFIG.execute;
        for (int op = 0; op < TXN_NUM_OPS; op++)
            args[i].count[op] = 0;
    }

    fast_forward((void*)(&args[0]));

    // set the start flag so that worker threads can execute transactions
    Green_light = true;

    for (int j = 1; j < BMCONFIG.threads; j++)
        pthread_create(&tid[j], &attr, &work_thread, &args[j]);

    // set the signal handler unless warmup/execute are nonzero
    if (BMCONFIG.warmup * BMCONFIG.execute == 0)
        signal(SIGALRM, catch_SIGALRM);
    // start the timer, then set a signal to end the experiment
    unsigned long long starttime = getElapsedTime();

    if (BMCONFIG.warmup * BMCONFIG.execute == 0)
        alarm(BMCONFIG.duration);

    // now have the main thread start doing transactions, too
    work_thread((void*)(&args[0]));

    // stop everyone, then join all threads and stop the timer
    Green_light = false;
    for (int k = 1; k < BMCONFIG.threads; k++)
        pthread_join(tid[k], NULL);
    unsigned long long endtime = getElapsedTime();

    // Report the timing
    stm::internal::report_timing();

    // Run the sanity check
    if (BMCONFIG.verify) {
        assert((args[0].b)->sanity_check());
        if (BMCONFIG.verbosity > 0)
            cout << "Completed sanity check." << endl;
    }

    // count the work performed
    long Total_count = 0;
    for (int l = 0; l < BMCONFIG.threads; l++) {
        Total_count += args[l].count[TXN_GENERIC];
    }

    // raw output:  total tx, total time
    cout << "Transactions: " << Total_count
         << ",  time: " << endtime - starttime << endl;

    // prettier output:  total tx/time
    if (BMCONFIG.verbosity > 0) {
        cout << (1000000000LL * Total_count) / (endtime - starttime)
             << " txns per second (whole system)" << endl;
    }

    // even prettier output:  print the number of each type of tx
    if (BMCONFIG.verbosity > 1) {
        for (int op = 0; op < TXN_NUM_OPS; op++) {
            cout << TxnOpDesc[op];
            for (int l = 0; l < BMCONFIG.threads; l++) {
                cout << " | ";
                cout.width(7);
                if (op == TXN_ID)
                    cout << l;

                else cout << args[l].count[op];
                cout.width(0);
            }
            cout << " |" << endl;
        }
    }
    
	#ifdef USE_BIMODAL
		stm::scheduler::BiModalScheduler::shutdown();
	#endif
}

// make sure all the parameters make sense
void BenchmarkConfig::verifyParameters()
{
    if (duration <= 0 && (warmup * execute == 0))
        argError("d must be positive or you must provide Warmup and "
                 "Xecute parameters");
    if (datasetsize <= 0)
        argError("m must be positive");
    if (threads <= 0)
        argError("p must be positive");
    if ((stm_validation != "vis-eager") && (stm_validation != "vis-lazy") &&
        (stm_validation != "invis-eager") && (stm_validation != "mixed") &&
        (stm_validation != "invis-lazy"))
        argError("Invalid validation strategy");
    if ((stm_validation == "vis-eager" || stm_validation == "vis-lazy") &&
        (threads > 31))
        argError("only up to 31 visible readers are currently supported");
    if (unit_testing != 'l' && unit_testing != 'h' && unit_testing != ' ')
        argError("Invalid unit testing parameter: " + unit_testing);
}

// print parameters if verbosity level permits
void BenchmarkConfig::printConfig()
{
    if (verbosity >= 1) {
        cout << "Bench: use -h for help." << endl;
        cout << bm_name;
        cout << ", d=" << duration << " seconds, m="
             << datasetsize
             << " elements, " << threads << " thread(s)" << endl;
        cout << "Validation Strategy: " << stm_validation << endl;
    }
}
