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

#ifndef USE_BIMODAL
#define USE_BIMODAL
#endif

#include <unistd.h>

#include <cstdlib>
#include <pthread.h>
#include <string>
#include <iostream>

#include "Benchmark.h"
#include "Counter.h"
#include "FGL.h"
#include "CGHash.h"
#include "Hash.h"
#include "LFUCache.h"
#include "LinkedList.h"
#include "LinkedListRelease.h"
#include "PrivList.h"
#include "RBTree.h"
#include "RBTreeLarge.h"
#include "RandomGraphList.h"

#ifdef USE_BIMODAL
#include "LinkedListBM.h"
#endif

using namespace bench;

using std::cerr;
using std::cout;
using std::endl;
using std::string;

using namespace stm;

BenchmarkConfig BMCONFIG;

static void usage()
{
    cerr << "Usage: Bench -B benchmark -C contention manager ";
    cerr << "-V validation strategy [flags]" << endl;
    cerr << "  Benchmarks:" << endl;
    cerr << "    Counter            Shared counter" << endl;
    cerr << "    LinkedList         Sorted linked list" << endl;
    cerr << "    LinkedListRelease  LinkedList, early release" << endl;
    cerr << "    HashTable          256-bucket hash table" << endl;
    cerr << "    RBTree (default)   Red-black tree" << endl;
    cerr << "    RBTreeLarge        Red-black tree with 4KB nodes" << endl;
    cerr << "    LFUCache           Web cache simulation" << endl;
    cerr << "    RandomGraph        Graph with 4 connections per node" << endl;
    cerr << "    PrivList           Linked list privatization test" << endl;
    cerr << "    FineGrainList      Lock-based sorted linked list" << endl;
    cerr << "    CoarseGrainHash    256-bucket hash table, per-node locks"
         << endl;
    cerr << "    FineGrainHash      256-bucket hash table, per-bucket locks"
         << endl;
    cerr << endl;
    cerr << "  Contention Managers:" << endl;
    cerr << "     Polka (default), Eruption, Highlander, Karma, Killblocked, ";
    cerr << "Polite" << endl;
    cerr << "     Polkaruption, Timestamp, Aggressive, Whpolka, Greedy, ";
    cerr << "Polkavis" << endl;
    cerr << endl;
    cerr << "  Validation Strategies:" << endl;
    cerr << "     invis-eager (default), invis-lazy, vis-eager, vis-lazy ";
    cerr << endl;
    cerr << endl;
    cerr << "  Flags:" << endl;
    cerr << "    -d: number of seconds to time (default 5)" << endl;
    cerr << "    -m: number of distinct elements (default 256)" << endl;
    cerr << "    -p: number of threads (default 2)" << endl;
    cerr << endl;
    cerr << "  Other:" << endl;
    cerr << "    -h: print help (this message)" << endl;
    cerr << "    -q: quiet mode" << endl;
    cerr << "    -v: verbose mode" << endl;
    cerr << "    -!: toggle verification at end of benchmark" << endl;
    cerr << "    -1: 80/10/10 lookup/insert/remove breakdown" << endl;
    cerr << "    -2: 33/33/33 lookup/insert/remove breakdown (default)"
         << endl;
    cerr << "    -3: 0/50/50  lookup/insert/remove breakdown" << endl;
    cerr << "    -T:[lh] perform light or heavy unit testing" << endl;
    cerr << "    -W/-X specify warmup and execute numbers" << endl;
    cerr << endl;
}

int main(int argc, char** argv)
{
    int opt;

    // parse the command-line options
    while ((opt = getopt(argc, argv, "B:C:H:a:d:m:p:hqv!xV:1234T:W:X:")) != -1)
    {
        switch(opt) {
          case 'B':
            BMCONFIG.bm_name = string(optarg);
            break;
          case 'V':
            BMCONFIG.stm_validation = string(optarg);
            break;
          case 'T':
            BMCONFIG.unit_testing = string(optarg)[0];
            break;
          case 'C':
            BMCONFIG.cm_type = string(optarg);
            BMCONFIG.use_static_cm = false;
            break;
          case 'W':
            BMCONFIG.warmup = atoi(optarg);
            break;
          case 'X':
            BMCONFIG.execute = atoi(optarg);
            break;
          case 'd':
            BMCONFIG.duration = atoi(optarg);
            break;
          case 'm':
            BMCONFIG.datasetsize = atoi(optarg);
            break;
          case 'p':
            BMCONFIG.threads = atoi(optarg);
            break;
          case 'h':
            stm::about();
            usage();
            return 0;
          case 'q':
            BMCONFIG.verbosity = 0;
            break;
          case 'v':
            BMCONFIG.verbosity = 2;
            break;
          case '!':
            BMCONFIG.verify = !BMCONFIG.verify;
            break;
          case '1':
            BMCONFIG.lThresh = 24;
            BMCONFIG.iThresh = 27;
            break;
          case '2':
            BMCONFIG.lThresh = 10;
            BMCONFIG.iThresh = 20;
            break;
          case '3':
            BMCONFIG.lThresh = 0;
            BMCONFIG.iThresh = 15;
            break;
          case '4':
			BMCONFIG.lThresh = 15;
            BMCONFIG.iThresh = 0;
            break;
        }
    }

    // make sure that the parameters all make sense
    BMCONFIG.verifyParameters();

    // initialize stm so that we have transactional mm, then verify the
    // benchmark parameter and construct the benchmark object
    stm::init(BMCONFIG.cm_type, BMCONFIG.stm_validation,
              BMCONFIG.use_static_cm);

    Benchmark* B = 0;

    if (BMCONFIG.bm_name == "Counter")
        B = new CounterBench();
    else if (BMCONFIG.bm_name == "FineGrainList")
        B = new IntSetBench(new FGL(), BMCONFIG.datasetsize);
    else if (BMCONFIG.bm_name == "FineGrainHash")
        B = new IntSetBench(new bench::HashTable<FGL>(), BMCONFIG.datasetsize);
    else if (BMCONFIG.bm_name == "CoarseGrainHash")
        B = new IntSetBench(new CGHash(), BMCONFIG.datasetsize);
    else if (BMCONFIG.bm_name == "RandomGraph")
        B = new RGBench<>(new RandomGraph(BMCONFIG.datasetsize),
                          BMCONFIG.datasetsize);
    else if (BMCONFIG.bm_name == "HashTable")
        B = new IntSetBench(new bench::Hash(), BMCONFIG.datasetsize);
    else if (BMCONFIG.bm_name == "LFUCache")
        B = new LFUTest();
    else if (BMCONFIG.bm_name == "LinkedList")
        B = new IntSetBench(new LinkedList(), BMCONFIG.datasetsize);
#ifdef USE_BIMODAL
	else if (BMCONFIG.bm_name == "LinkedListBM")
        B = new IntSetBench(new LinkedListBM(), BMCONFIG.datasetsize);
#endif
    else if (BMCONFIG.bm_name == "LinkedListRelease")
        B = new IntSetBench(new LinkedListRelease(), BMCONFIG.datasetsize);
    else if (BMCONFIG.bm_name == "PrivList")
        B = new PrivList(BMCONFIG.datasetsize);
    else if (BMCONFIG.bm_name == "RBTree")
        B = new IntSetBench(new RBTree(), BMCONFIG.datasetsize);
    else if (BMCONFIG.bm_name == "RBTreeLarge")
        B = new IntSetBench(new RBTreeLarge(), BMCONFIG.datasetsize);
    else
        argError("Unrecognized benchmark name " + BMCONFIG.bm_name);

    // print the configuration for this run of the benchmark
    BMCONFIG.printConfig();

    // either verify the data structure or run a timing experiment
    if (BMCONFIG.unit_testing != ' ') {
        if (B->verify(BMCONFIG.unit_testing == 'l' ? LIGHT : HEAVY))
            cout << "Verification succeeded" << endl;
        else
            cout << "Verification failed" << endl;
    }
    else {
        B->measure_speed();
    }

    stm::shutdown();
    return 0;
}
