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

#ifndef __INSTRUMENTATION_H__
#define __INSTRUMENTATION_H__

#include "stm_common.h"

namespace stm
{
    /**
     *  Implementation of ConflictCounter that actually does something.  All we
     *  do is increment the appropriate counter on each method call.
     */
    class BasicConflictCounter
    {
        /**
         *  set up an enum for all the conflict events that we want to count
         */
        enum EVENTS {CONFLICTS_DETECTED = 0,
                     VALIDATION_FAILURES = 1,
                     EXPLICIT_ABORTS = 2,
                     CLEANUP_FAILURES = 3,
                     COMMITS = 4,
                     EVENTS_ENUM_SIZE = 5}; // this one is the size of the enum

        /**
         *  array for holding all of the counts
         */
        unsigned long counts[EVENTS_ENUM_SIZE];

      public:
        /**
         *  Constructor for BasicConflictCounter.  All we do here is zero out
         *  all of the fields.
         */
        BasicConflictCounter()
        {
            for (int i = 0; i < EVENTS_ENUM_SIZE; i++)
                counts[i] = 0;
        }

        /**
         *  Increment the CONFLICTS_DETECTED field.
         */
        void ADD_CONFLICT() { counts[CONFLICTS_DETECTED]++; }

        /**
         *  Increment the VALIDATION_FAILURES field.
         */
        void ADD_VALIDATION_FAIL() { counts[VALIDATION_FAILURES]++; }

        /**
         *  Increment the EXPLICIT_ABORTS field.
         */
        void ADD_EXPLICIT_ABORT() { counts[EXPLICIT_ABORTS]++; }

        /**
         *  Increment the CLEANUP_FAILURES field.
         */
        void ADD_CLEANUP_FAIL() { counts[CLEANUP_FAILURES]++; }

        /**
         *  Increment the COMMITS field.
         */
        void ADD_COMMIT() { counts[COMMITS]++; }

        /**
         *  Print all collected data.
         *
         *  @param  id  ID of the tx whose data we're printing
         */
        void REPORT_CONFLICTS(unsigned long id);
    } __attribute__ ((aligned(64)));  // BasicConflictCounter

    /**
     *  NopConflictCounter is the version of the ConflictCounter that we use
     *  when we aren't interested in counting conflicts.  This version is all
     *  no-ops, and with compiler inlining should result in no code being added
     *  to the stm binary.
     */
    class NopConflictCounter
    {
      public:
        NopConflictCounter()                    { }
        void ADD_CONFLICT()                     { }
        void ADD_VALIDATION_FAIL()              { }
        void ADD_EXPLICIT_ABORT()               { }
        void ADD_CLEANUP_FAIL()                 { }
        void ADD_COMMIT()                       { }
        void REPORT_CONFLICTS(unsigned long id) { }
    } __attribute__ ((aligned(64))); // NopConflictCounter

    // here's the compile-time switch for selecting which implementation of the
    // ConflictCounterInterface we will use in our code.
#ifdef COUNT_CONFLICTS
    extern BasicConflictCounter ConflictCounter[MAX_THREADS];
#else // COUNT_CONFLICTS not defined
    extern NopConflictCounter ConflictCounter[MAX_THREADS];
#endif // COUNT_CONFLICTS

}; // namespace stm

#include <sys/time.h>
#include <iostream>
#include "atomic_ops.h"

#include "hrtime.h"

//////////////////////////////////////////////////////////////////////
//
// Every descriptor is going to have a TimeAccounting field, and wherever
// accounting ought to happen, we're going to have a call of the form
// TimeAccounting.SOMETHING().  The "SOMETHING" method is defined twice in this
// file, with an #ifdef choosing between the definitions; in one case the call
// is a nop; the inlined nops should not cause any overhead.

// declare the different buckets into which we can add time
static const unsigned long TIMING_REALWORK    = 0;
static const unsigned long TIMING_VALIDATION  = 1;
static const unsigned long TIMING_BOOKKEEPING = 2;
static const unsigned long TIMING_MM          = 3;
static const unsigned long TIMING_CM          = 4;
static const unsigned long TIMING_COPY        = 5;
static const unsigned long TIMING_NON_TX      = 6;

// we never actually give time to TIMING_UNSTARTED, but it is useful
static const unsigned long TIMING_UNSTARTED   = 7;

// declare the number of buckets; used for array size
static const unsigned long TIMING_TYPES       = 7;

#ifdef TIMING_BREAKDOWNS

class TimeAccounting
{
  private:
    // use to figure out how much a getElapsedTime() call costs
    unsigned long long timertime;

    // temp vars during measurement
    unsigned long long last_time;
    unsigned long last_state;

    // during a tx, we store data here
    unsigned long long active_hrtimes[TIMING_TYPES];
    unsigned long long active_occurrences[TIMING_TYPES];

    // then on tx end, we transfer the data to one of these two sets of fields
    unsigned long long commit_hrtimes[TIMING_TYPES];
    unsigned long long commit_occurrences[TIMING_TYPES];

    unsigned long long abort_hrtimes[TIMING_TYPES];
    unsigned long long abort_occurrences[TIMING_TYPES];

  public:
    TimeAccounting();
    void UPDATE_TIMING(unsigned long state);
    void GIVE_TIMING();
    void TAKE_TIMING(unsigned long state);
    void TIMING_TRANSFER_ABORTED();
    void TIMING_TRANSFER_COMMITTED();
    void REPORT_TIMING(int id);
};

// zero out all the fields in the struct, and then compute the cost of an
// hrtime call
inline TimeAccounting::TimeAccounting()
{
    // zero out temp fields
    last_time  = 0;
    last_state = TIMING_UNSTARTED;

    // zero out result fields
    for (unsigned long i = 0; i < TIMING_TYPES; i++) {
        active_hrtimes[i] = 0;
        active_occurrences[i] = 0;

        commit_hrtimes[i] = 0;
        commit_occurrences[i] = 0;

        abort_hrtimes[i] = 0;
        abort_occurrences[i] = 0;
    }
    // get an estimate of the cost of one getElapsedTime() call
    unsigned long long t1, t2, t3;
    t1 = getElapsedTime();
    for (int i = 0; i < 300; i++)
        t3 = getElapsedTime();
    t2 = getElapsedTime();
    timertime = (t2 - t1)/300;
}

// assign all elapsed time to the last state, and set the state to S
inline void TimeAccounting::UPDATE_TIMING(unsigned long state)
{
    unsigned long long time = getElapsedTime();
    if (last_state != TIMING_UNSTARTED) {
        active_occurrences[last_state]++;
        active_hrtimes[last_state] += (time - last_time);
    }
    last_state = state;
    last_time = time;
}

// push elapsed time to /last_state/ but don't change the state.  We need this
// for MM, since the MM call might come from user code and we're not going to
// instrument it
inline void TimeAccounting::GIVE_TIMING()
{
    unsigned long long time = getElapsedTime();
    if (last_state != TIMING_UNSTARTED) {
        active_occurrences[last_state]++;
        active_hrtimes[last_state] += (time - last_time);
    }
    last_time = time;
}

// give STATE all of the elapsed time and then update last_time
inline void TimeAccounting::TAKE_TIMING(unsigned long state)
{
    unsigned long long time = getElapsedTime();
    if (last_state != TIMING_UNSTARTED) {
        active_occurrences[state]++;
        active_hrtimes[state] += (time - last_time);
    }
    last_time = time;
}

// transfer all of the timing values from active to aborted
inline void TimeAccounting::TIMING_TRANSFER_ABORTED()
{
    for (unsigned long i = 0; i < TIMING_TYPES; i++) {
        abort_hrtimes[i] += active_hrtimes[i];
        abort_occurrences[i] += active_occurrences[i];
        active_hrtimes[i] = (unsigned long long)0.0;
        active_occurrences[i] = 0;
    }
}

// transfer all of the timing values from active to committed
inline void TimeAccounting::TIMING_TRANSFER_COMMITTED()
{
    for (unsigned long i = 0; i < TIMING_TYPES; i++) {
        commit_hrtimes[i] += active_hrtimes[i];
        commit_occurrences[i] += active_occurrences[i];
        active_hrtimes[i] = (unsigned long long)0.0;
        active_occurrences[i] = 0;
    }
}

// this will make output a little bit cleaner
static char* TIMING_TITLES[TIMING_TYPES] =
    {"Real Work  ", "Validation ", "Bookkeeping", "MM         ", "CM         ",
     "Copy       ", "Non-Tx     "};

// print all the fields from the timing data structure
inline void TimeAccounting::REPORT_TIMING(int id)
{
    std::cout << "Thread: " << id << std::endl;
    std::cout << "hrtime cost:      " << timertime << std::endl;
    for (unsigned long i = 0; i < TIMING_TYPES; i++) {
        std::cout << "commit:  " << TIMING_TITLES[i] << " :";
        std::cout.width(12);
        std::cout << std::right << commit_hrtimes[i];
        std::cout << " ns :";
        std::cout.width(12);
        std::cout << std::right << commit_occurrences[i];
        std::cout << " timers" << std::endl;
    }
    for (unsigned long i = 0; i < TIMING_TYPES; i++) {
        std::cout << "abort:  " << TIMING_TITLES[i] << " :";
        std::cout.width(12);
        std::cout << std::right << abort_hrtimes[i];
        std::cout << " ns :";
        std::cout.width(12);
        std::cout << std::right << abort_occurrences[i];
        std::cout << " timers" << std::endl;
    }

}

#else  // TIMING_BREAKDOWNS isn't set, so these should all be NOPs

// just a bunch of inlined nops, which should get optimized out.
class TimeAccounting
{
  public:
    TimeAccounting() { }
    void UPDATE_TIMING(unsigned long state) { }
    void GIVE_TIMING() { }
    void TAKE_TIMING(unsigned long state) { }
    void TIMING_TRANSFER_ABORTED() { }
    void TIMING_TRANSFER_COMMITTED() { }
    void REPORT_TIMING(int id) { }
};

#endif // TIMING_BREAKDOWNS

#endif // __INSTRUMENTATION_H__
