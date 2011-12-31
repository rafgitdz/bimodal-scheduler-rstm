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

#ifndef __CONTENTIONMANAGER_H__
#define __CONTENTIONMANAGER_H__

#include <string>
#include <cstdlib>
#include <sys/time.h>
#include "atomic_ops.h"
#include "hrtime.h"

#ifndef USE_BIMODAL
#define USE_BIMODAL
#endif

namespace stm
{
    namespace cm
    {
        class ContentionManager
        {
          protected:
            int priority;
          public:
            ContentionManager() : priority(0) { }
            int getPriority() { return priority; }

            // Transaction-level events
            virtual void OnBeginTransaction() { }
            virtual void OnTryCommitTransaction() { }
            virtual void OnTransactionCommitted() { priority = 0; }
            virtual void OnTransactionAborted() { }

            // Object-level events
            virtual void onContention() { }
            virtual void OnOpenRead() { }
            virtual void OnOpenWrite() { }
            virtual void OnReOpen() { }
            virtual bool ShouldAbort(ContentionManager* enemy) = 0;
            virtual bool ShouldAbortAll(unsigned long bitmap) { return true; }
            virtual ~ContentionManager() { }
        };

        // create a contention manager
        ContentionManager* Factory(std::string cm_type);

        // wait by executing a bunch of nops in a loop (not preemption
        // tolerant)
        inline void nano_sleep(unsigned long nops)
        {
            for (unsigned long i = 0; i < nops; i++)
                nop();
        }

        ///////////////////////////////////////////////////////
        //
        // CM Implementations
		//
        // aggressive is very simple; it always aborts the enemy transaction
        class Aggressive: public ContentionManager
        {
          public:
            // ctor only needs what is provided by ContentionManager
            Aggressive() { }

            virtual bool ShouldAbort(ContentionManager* enemy)
            {
                return true;
            }
        };

        // polite is just exponential backoff with some limit
        class Polite: public ContentionManager
        {
            // randomized exponential backoff interface; shared with all
            // descendants
          protected:
            enum Backoff {
                MIN_BACKOFF = 7,
                MAX_BACKOFF = 28,
                MAX_BACKOFF_RETRIES = MAX_BACKOFF - MIN_BACKOFF + 1
            };

            // how many times have we backed off without opening anything
            int tries;

            // seed for randomized exponential backoff
            unsigned int seed;

            // randomized exponential backoff
            void backoff()
            {
                if (tries > 0 && tries <= MAX_BACKOFF_RETRIES) {
                    // what time is it now
                    unsigned long long starttime = getElapsedTime();

                    // how long should we wait (random)
                    unsigned long delay = rand_r(&seed);
                    delay = delay % (1 << (tries + MIN_BACKOFF));

                    // spin until /delay/ nanoseconds have passed.  We can do
                    // whatever we want in the spin, as long as it doesn't have
                    // an impact on other threads
                    unsigned long long endtime;
                    do {
                        endtime = getElapsedTime();
                    } while (endtime < starttime + delay);
                }
                tries++;
            }

          private:
            // every time we Open something, reset the try counter
            void OnOpen() { tries = 0; }

          public:
            Polite() : tries(0), seed(0) { }

            // request permission to abort enemy tx
            virtual bool ShouldAbort(ContentionManager* enemy)
            {
                return (tries > MAX_BACKOFF_RETRIES);
            }

            // event methods
            virtual void onContention() { backoff(); }

            virtual void OnOpenRead()  { OnOpen(); }
            virtual void OnOpenWrite() { OnOpen(); }
            virtual void OnReOpen()    { OnOpen(); }
        };

        // Karma uses linear backoff with some notion of "importance" based on
        // the amount of objects opened
        class Karma: public ContentionManager
        {
          private:
            enum Backoff { INTERVAL = 1000 };

            int tries;

            // every time we Try to Open something, backoff once more and
            // increment the try counter
            void OnTryOpen(void)
            {
                if (tries > 1)
                    nano_sleep(INTERVAL);

                tries++;
            }

            // every time we Open something, reset the try counter, increase
            // the karma
            void OnOpen(void)
            {
                priority++;
                tries = 1;
            }

          public:
            // constructor
            Karma() : tries(1) { }

            // request permission to abort enemies
            virtual bool ShouldAbort(ContentionManager* enemy)
            {
                if (!enemy)
                    return true;
                return ((tries + priority) > enemy->getPriority());
            }

            // event methods
            virtual void OnReOpen() { tries = 1; }
            virtual void onContention() { OnTryOpen(); }
            virtual void OnOpenRead() { OnOpen(); }
            virtual void OnOpenWrite() { OnOpen(); }
            virtual void OnTransactionCommitted() { priority = 0; }
        };

        // polka is Polite + Karma
        class Polka : public Polite
        {
          protected:
            // every time we Open something, reset the try counter, increase
            // the karma
            void OnOpen()
            {
                priority++;
                tries = 0;
            }

          public:
            // everything in the Polka ctor is handled by Polite() and
            // ContentionManager()
            Polka() { }

            // query the enemy cm to get its priority, return true if enemy's
            // priority is lower than mine.
            virtual bool ShouldAbort(ContentionManager* enemy)
            {
                if (!enemy)
                    return true;
                return ((tries + priority) > enemy->getPriority());
            }

            // event methods
            virtual void OnReOpen()     { tries = 0; }
            virtual void OnOpenRead()   { OnOpen(); }
            virtual void OnOpenWrite()  { OnOpen(); }
        };

        // Greedy says that until I commit, I keep my age, and if I'm older
        // than you I always win conflicts
        class Greedy : public ContentionManager
        {
          private:
            unsigned long timestamp; // For priority
            bool waiting;

            static volatile unsigned long timeCounter;

          public:
            Greedy() : waiting(false) { timestamp = fai(&timeCounter); }

            virtual void OnBeginTransaction() { waiting = false; }

            virtual void OnTransactionCommitted()
            {
                priority = 0;
                timestamp = fai(&timeCounter);
            }

            virtual bool ShouldAbort(ContentionManager* enemy)
            {
                Greedy* B = dynamic_cast<Greedy*>(enemy);

                if ((timestamp < B->timestamp) || (B->waiting == true)) {
                    // I abort B
                    waiting = false;
                    return true;
                }
                else {
                    waiting = true;
                    return false;
                }
            }

            virtual void OnOpenRead() { waiting = false; }
            virtual void OnOpenWrite() { waiting = false; }
            virtual void OnReOpen() { waiting = false; }
        };

        // serializer is like Greedy, except that when I abort I lose my old
        // timestamp and have to get a new one.
        class Serializer : public ContentionManager
        {
          private:
            unsigned long timestamp; // For priority

            static volatile unsigned long timeCounter;

          public:
            Serializer() { }
            virtual void OnBeginTransaction()
            {
                timestamp = fai(&timeCounter);
            }

            virtual bool ShouldAbort(ContentionManager* enemy)
            {
                Serializer* B = dynamic_cast<Serializer*>(enemy);
                if ((timestamp < B->timestamp))
                    // I abort B
                    return true;
                else
                    return false;
            }
        };

        // KillBlocked reduces the priority of threads that appear to be blocked
        class Killblocked: public ContentionManager
        {
          private:
            enum Backoff { INTERVAL = 1000, MAX_TRIES = 16 };

            int tries;
            bool blocked;

            // once we open the object, become unblocked
            void OnOpen(void)
            {
                blocked = false;
                tries = 0;
            }

          public:
            Killblocked() : tries(0), blocked(false) { }

            bool IsBlocked(void) { return blocked; }

            // request permission to abort enemy tx
            virtual bool ShouldAbort(ContentionManager* enemy)
            {
                Killblocked* k = dynamic_cast<Killblocked*>(enemy);

                if (tries > MAX_TRIES)
                    return true;
                else if (k && k->IsBlocked())
                    return true;

                return false;
            }

            // event methods
            virtual void onContention()
            {
                blocked = true;
                nano_sleep(INTERVAL);
                tries++;
            }

            virtual void OnOpenRead()  { OnOpen(); }
            virtual void OnOpenWrite() { OnOpen(); }
            virtual void OnReOpen()    { OnOpen(); }
        };

        // Eruption says that when I block behind you, I loan you my priority
        // so that you can finish faster.
        class Eruption: public ContentionManager
        {
          private:
            enum Backoff { INTERVAL = 1000 };

            int tries;
            int prio_transferred;

            void OnTryOpen(void)
            {
                if (tries > 1)
                    nano_sleep(INTERVAL);

                tries++;
            }

            void OnOpen(void)
            {
                priority++;
                prio_transferred = 0;
                tries = 1;
            }

          public:
            Eruption() : tries(1), prio_transferred(0) { }

            virtual bool ShouldAbort(ContentionManager* enemy)
            {
                Eruption* e;

                if (!enemy)
                    return true;

                if ((tries + priority) > enemy->getPriority()) {
                    return true;
                }
                else {
                    e = dynamic_cast<Eruption*>(enemy);

                    if (e) {
                        e->givePrio(priority - prio_transferred);
                        prio_transferred = priority;
                    }

                    return false;
                }
            }

            void givePrio(int prio)
            {
                priority += prio;
            }

            // event methods
            virtual void onContention() { OnTryOpen(); }
            virtual void OnOpenRead() { OnOpen(); }
            virtual void OnOpenWrite() { OnOpen(); }
            virtual void OnTransactionCommitted()
            {
                priority = 0;
                prio_transferred = 0;
            }
        };

        // Timestamp is similar to Greedy: it uses a notion of global time to
        // resolve conflicts
        class Timestamp: public ContentionManager
        {
          private:
            enum Backoff { INTERVAL = 1000, MAX_TRIES = 8 };

            int tries;
            int max_tries;
            time_t stamp;
            bool defunct;

            void OnOpen(void)
            {
                defunct = false;
                tries = 0;
                max_tries = MAX_TRIES;
            }

          public:
            Timestamp() : tries(0), max_tries(MAX_TRIES), defunct(false) { }

            virtual bool ShouldAbort(ContentionManager* enemy)
            {
                Timestamp* t = dynamic_cast<Timestamp*>(enemy);

                defunct = false;

                if (!t)
                    return true;

                // always abort younger transactions
                if (t->GetTimestamp() <= stamp) {
                    return true;
                }

                // if it's been a while, mark the enemy defunct
                if (tries == (max_tries/2)) {
                    t->SetDefunct();
                }

                // at some point, finally give up and abort the enemy
                else if (tries == max_tries) {
                    return true;
                }

                // if the enemy was marked defunct and isn't anymore, then we reset a bit
                else if ((tries > (max_tries/2)) && (t->GetDefunct() == false)) {
                    tries = 2;
                    max_tries *= 2;
                }

                return false;
            }

            time_t GetTimestamp() { return stamp; }
            void SetDefunct() { defunct = true; }
            bool GetDefunct() { return defunct; }

            virtual void onContention()
            {
                defunct = false;
                nano_sleep(INTERVAL);
                tries++;
            }

            virtual void OnOpenRead() { OnOpen(); }
            virtual void OnOpenWrite() { OnOpen(); }
            virtual void OnTransactionCommitted() { defunct = false; }
            virtual void OnTryCommitTransaction() { defunct = false; }
            virtual void OnTransactionAborted() { defunct = false; }
            virtual void OnBeginTransaction()
            {
                struct timeval t;

                defunct = false;
                gettimeofday(&t, NULL);
                stamp = t.tv_sec;
            }

        };

        // whpolka makes writes heavier than reads, otherwise it's polka
        class Whpolka: public Polka
        {
          private:
            enum Weights { READ_WEIGHT = 0, WRITE_WEIGHT = 1 };

          public:
            Whpolka() { }

            // event methods
            virtual void OnOpenRead()
            {
                priority += READ_WEIGHT;
                tries = 0;
            }

            virtual void OnOpenWrite()
            {
                priority += WRITE_WEIGHT;
                tries = 0;
            }
        };

        // polkavis is just polka with ShouldAbort improved to handle visible
        // readers
        class PolkaVis : public Polka
        {
          public:
            // simple constructor
            PolkaVis() { }

            // for polkavis, writers get permission to abort vis readers
            virtual bool ShouldAbortAll(unsigned long bitmap);
        };


        // combine Polka with Eruption
        // NB: we're re-rolling the Eruption part by hand
        class Polkaruption: public Polka
        {
          private:
            int prio_transferred;

            // every time we Open something, reset the try counter, increase
            // the karma
            void OnOpen()
            {
                priority++;
                prio_transferred = 0;
                tries = 1;
            }

          public:
            // simple constructor
            Polkaruption() : prio_transferred(0) { }

            // request permission to abort enemy tx
            virtual bool ShouldAbort(ContentionManager* enemy)
            {
                Polkaruption* p;

                if (!enemy)
                    return true;

                if ((tries + priority) > enemy->getPriority()) {
                    return true;
                }
                else {
                    p = dynamic_cast<Polkaruption*>(enemy);

                    if (p) {
                        p->givePrio(priority - prio_transferred);
                        prio_transferred = priority;
                    }

                    return false;
                }
            }

            // transfer priority
            void givePrio(int prio)
            {
                priority += prio;
            }

            // event methods
            virtual void OnOpenRead() { OnOpen(); }
            virtual void OnOpenWrite() { OnOpen(); }

            virtual void OnTransactionCommitted()
            {
                priority = 0;
                prio_transferred = 0;
            }
        };

        // the point of Justice is to assign weights to different actions more
        // appropriately within Polka
        class Justice : public Polite
        {
            enum Weights {
                READWEIGHT  = 4,
                WRITEWEIGHT = 16,
                TRYWEIGHT   = 1
            };

          protected:
            unsigned long reads;
            unsigned long writes;

          public:
            // everything in the Justice ctor is handled by Polite() and
            // ContentionManager()
            Justice() : reads(0), writes(0) { }

            // query the enemy cm to get its priority, return true if enemy's
            // priority is lower than mine.
            virtual bool ShouldAbort(ContentionManager* enemy)
            {
                Justice* e = dynamic_cast<Justice*>(enemy);
                return jprio() > e->jprio();
            }

            unsigned long jprio()
            {
                unsigned long mass =
                    reads*READWEIGHT + writes*WRITEWEIGHT + tries*TRYWEIGHT;
                return mass;
            }

            // event methods
            virtual void OnReOpen()     { tries = 0; }
            virtual void OnOpenRead()   { reads++; tries = 0; }
            virtual void OnOpenWrite()  { writes++; tries = 0; }

            virtual void OnTransactionCommitted()
            {
                reads = 0;
                writes = 0;
                priority = 0;
            }
        };

        // highlander is a derivative of polka; when I kill a tx, I take its
        // karma
        class Highlander: public Polka
        {
          public:
            Highlander() { }

            virtual bool ShouldAbort(ContentionManager* enemy)
            {
                if (!enemy)
                    return true;

                if ((tries + priority) > enemy->getPriority()) {
                    priority += enemy->getPriority(); // there can only be one
                    return true;
                }

                return false;
            }
        };


    } // namespace stm::cm
} // namespace stm

#endif  // __CONTENTIONMANAGER_H__
