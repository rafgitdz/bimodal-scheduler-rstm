###############################################################################
# 
#  Copyright (c) 2005, 2006
#  University of Rochester
#  Department of Computer Science
#  All rights reserved.
# 
#  Redistribution and use in source and binary forms, with or without
#  modification, are permitted provided that the following conditions are met:
# 
#     * Redistributions of source code must retain the above copyright notice,
#       this list of conditions and the following disclaimer.
# 
#     * Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in the
#       documentation and/or other materials provided with the distribution.
# 
#     * Neither the name of the University of Rochester nor the names of its
#       contributors may be used to endorse or promote products derived from
#       this software without specific prior written permission.
# 
# 
#  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
#  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
#  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
#  ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
#  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
#  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
#  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
#  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
#  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
#  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
#  POSSIBILITY OF SUCH DAMAGE.

# pull in all global Make variable definitions
include ../Makefile.in

INCLUDEPATH = -I. -I../ -I../stm -I../stm/scheduler
STM_LIB = ../stm/$(OBJDIR)/libstm.a ../stm/obj/libscheduler.a
OBJFILES = $(OBJDIR)/BenchMain.o          $(OBJDIR)/Benchmark.o \
           $(OBJDIR)/FGL.o                $(OBJDIR)/LFUCache.o \
           $(OBJDIR)/LinkedList.o         $(OBJDIR)/LinkedListRelease.o \
           $(OBJDIR)/RandomGraphList.o    $(OBJDIR)/RBTree.o \
           $(OBJDIR)/RBTreeLarge.o \
           $(OBJDIR)/CGHash.o

BM_HEADERS = Counter.h FGL.h Hash.h LinkedList.h LFUCache.h \
             LinkedListRelease.h RBTree.h RandomGraphList.h CGHash.h \
             RBTreeLarge.h IntSet.h PrivList.h \
             ../stm/stm_api.h \
             ../stm/atomic_ops.h


.PHONY: all
all: $(OBJDIR) $(OBJROOT)/Bench_$(STM_VERSION)
	@echo "     "$(OBJROOT)/Bench_$(STM_VERSION) complete

$(OBJDIR)/BenchMain.o: BenchMain.cpp Benchmark.h $(BM_HEADERS) $(STM_LIB)
	$(CXX) $(CXXFLAGS) $(INCLUDEPATH) -o $@ -c $<

$(OBJDIR)/Benchmark.o: Benchmark.cpp Benchmark.h $(STM_LIB)
	$(CXX) $(CXXFLAGS) $(INCLUDEPATH) -o $@ -c $<

$(OBJDIR)/FGL.o: FGL.cpp FGL.h $(STM_LIB) IntSet.h Benchmark.h
	$(CXX) $(CXXFLAGS) $(INCLUDEPATH) -o $@ -c $<

$(OBJDIR)/CGHash.o: CGHash.cpp CGHash.h FGL.h $(STM_LIB) IntSet.h Benchmark.h
	$(CXX) $(CXXFLAGS) $(INCLUDEPATH) -o $@ -c $<

$(OBJDIR)/LFUCache.o: LFUCache.cpp LFUCache.h $(STM_LIB) Benchmark.h
	$(CXX) $(CXXFLAGS) $(INCLUDEPATH) -o $@ -c $<

$(OBJDIR)/LinkedList.o: LinkedList.cpp LinkedList.h $(STM_LIB) IntSet.h \
                        Benchmark.h
	$(CXX) $(CXXFLAGS) $(INCLUDEPATH) -o $@ -c $<

$(OBJDIR)/LinkedListRelease.o: LinkedListRelease.cpp LinkedListRelease.h \
                               $(STM_LIB) IntSet.h Benchmark.h
	$(CXX) $(CXXFLAGS) $(INCLUDEPATH) -o $@ -c $<

$(OBJDIR)/RandomGraphList.o: RandomGraphList.cpp RandomGraphList.h $(STM_LIB) \
                             Benchmark.h
	$(CXX) $(CXXFLAGS) $(INCLUDEPATH) -o $@ -c $<

$(OBJDIR)/RBTree.o: RBTree.cpp RBTree.h IntSet.h $(STM_LIB) Benchmark.h
	$(CXX) $(CXXFLAGS) $(INCLUDEPATH) -o $@ -c $<

$(OBJDIR)/RBTreeLarge.o: RBTreeLarge.cpp RBTreeLarge.h IntSet.h $(STM_LIB) \
                         Benchmark.h
	$(CXX) $(CXXFLAGS) $(INCLUDEPATH) -o $@ -c $<

# this is the executable
$(OBJROOT)/Bench_$(STM_VERSION): $(OBJFILES) $(STM_LIB)
	$(CXX) $(CXXFLAGS) $(INCLUDEPATH) -o $@ $^ $(LDFLAGS)

# pull in the standard targets
include ../Makefile.targets
