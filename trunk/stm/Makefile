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

INCLUDEPATH = -I./ -I../

COMMON_OBJS = $(OBJDIR)/instrumentation.o $(OBJDIR)/Epoch.o \
              $(OBJDIR)/IdManager.o $(OBJDIR)/about.o
COMMON_HEADERS = instrumentation.h stm_common.h Epoch.h IdManager.h

CM_OBJS = $(OBJDIR)/ContentionManager.o
CM_HEADERS = ContentionManager.h BiModalCM.hpp atomic_ops.h hrtime.h \
             Descriptor_$(STM_VERSION).h

MM_OBJS = $(OBJDIR)/stm_mm.o
MM_DEPS = stm_mm.h MallocHeap.h GCHeap.h  \
          Descriptor_$(STM_VERSION).h \
          atomic_ops.h StlAllocator.h

CGL_OBJS = $(OBJDIR)/cgl.o $(COMMON_OBJS) $(MM_OBJS)

CGL_HEADERS = cgl.h Descriptor_cgl.h atomic_ops.h \
              stm_common.h accessors.h

RSTM_OBJS = $(OBJDIR)/rstm.o $(CM_OBJS) $(COMMON_OBJS) $(MM_OBJS)

RSTM_HEADERS = Descriptor_rstm.h rstm.h CustomAllocedBase.h \
               ObjectBase_rstm.h \
               Object_rstm.h SharedBase_rstm.h Shared_rstm.h \
               atomic_ops.h Epoch.h \
               policies.h \
               MiniVector.h \
               instrumentation.h ConflictDetector.h \
               ContentionManager.h stm_common.h \
               Reclaimer.h macros.h accessors.h \
               stm_mm.h GCHeap.h MallocHeap.h
               

RL_OBJS = $(OBJDIR)/redo_lock.o $(MM_OBJS) $(CM_OBJS) $(COMMON_OBJS)

RL_HEADERS = Descriptor_redo_lock.h redo_lock.h CustomAllocedBase.h \
             SharedBase_redo_lock.h \
             Shared_redo_lock.h Object_redo_lock.h policies.h \
             atomic_ops.h Epoch.h \
             MiniVector.h \
             instrumentation.h ConflictDetector.h \
             ContentionManager.h stm_common.h \
             Reclaimer.h macros.h accessors.h \
             stm_mm.h GCHeap.h MallocHeap.h 


LIBRL = ./obj/redo_lock/libstm.a
LIBRSTM = ./obj/rstm/libstm.a
LIBCGL = ./obj/cgl/libstm.a


# pick a version of the STM library to build:
ifeq ($(STM_VERSION), cgl)
	LIBSTM = $(LIBCGL)
endif
ifeq ($(STM_VERSION), rstm)
	LIBSTM = $(LIBRSTM)
endif
ifeq ($(STM_VERSION), redo_lock)
	LIBSTM = $(LIBRL)
endif


.PHONY: all
all:  $(OBJDIR) $(LIBSTM)
mm: $(OBJDIR) $(MM_OBJS)
common: $(OBJDIR) $(COMMON_OBJS)
cm: $(OBJDIR) $(CM_OBJS) 

$(OBJDIR)/instrumentation.o: instrumentation.cpp $(COMMON_HEADERS)
	$(CXX) $(CXXFLAGS) $(INCLUDEPATH) -c $< -o $@

$(OBJDIR)/about.o: about.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDEPATH) -c $< -o $@ $(CFG_STRING)

$(OBJDIR)/Epoch.o: Epoch.cpp $(COMMON_HEADERS)
	$(CXX) $(CXXFLAGS) $(INCLUDEPATH) -c $< -o $@

$(OBJDIR)/IdManager.o: IdManager.cpp $(COMMON_HEADERS)
	$(CXX) $(CXXFLAGS) $(INCLUDEPATH) -c $< -o $@

$(OBJDIR)/ContentionManager.o: ContentionManager.cpp $(CM_HEADERS)
	$(CXX) $(CXXFLAGS) $(INCLUDEPATH) -c $< -o $@

$(OBJDIR)/stm_mm.o: stm_mm.cpp $(MM_DEPS)
	$(CXX) $(CXXFLAGS) $(INCLUDEPATH) -c $< -o $@

$(OBJDIR)/cgl.o: cgl.cpp $(CGL_HEADERS)
	$(CXX) $(CXXFLAGS) $(INCLUDEPATH) -c $< -o $@

$(OBJDIR)/rstm.o: rstm.cpp $(RSTM_HEADERS)
	$(CXX) $(CXXFLAGS) $(INCLUDEPATH) -c $< -o $@

$(OBJDIR)/redo_lock.o: redo_lock.cpp $(STM_HEADERS)
	$(CXX) $(CXXFLAGS) $(INCLUDEPATH) -c $< -o $@

$(LIBCGL): $(CGL_OBJS)
	$(AR) cru $@ $^

$(LIBRSTM): $(RSTM_OBJS)
	$(AR) cru $@ $^

$(LIBRL): $(RL_OBJS)
	$(AR) cru $@ $^

# pull in the standard targets
include ../Makefile.targets
