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


########################################
# this portion of the makefile is for when you want to build the mesh with
# fine-grained locks.

dummy = $(STM_VERSION)
# I don't understand why, but that dummy assignment is necessary.
# Without it gmake fails to override fgl below.
ifeq ($(origin STM_VERSION), undefined)
    STM_VERSION = rstm
    FGL = false
else
	ifeq ($(STM_VERSION), fgl)
		override STM_VERSION := cgl
		FGL = true
	else
		FGL = false
	endif
endif

PRIVATIZATION = BARRIER

include ../Makefile.in

STM_LIB		:= ../stm/obj/$(STM_VERSION)/libstm.a
INCLUDEPATH	:= -I. -I../ -I../stm

ifeq ($(FGL), true)
	CXXFLAGS += -DFGL
else
	CXXFLAGS += -D$(shell echo $(STM_VERSION)|tr '[a-z]' '[A-Z]')
endif

########################################
# standard makefile follows

MESH_HEADERS = config.h lock.h barrier.h my_thread.h point.h edge.h \
               edge_set.h side.h dwyer.h queues.h worker.h
OBJFILES = $(OBJDIR)/point.o $(OBJDIR)/edge.o $(OBJDIR)/edge_set.o \
           $(OBJDIR)/side.o $(OBJDIR)/dwyer.o $(OBJDIR)/worker.o \
           $(OBJDIR)/my_thread.o $(OBJDIR)/mesh.o $(OBJDIR)/queues.o

.PHONY: all

all : $(OBJDIR) $(OBJROOT)/mesh_$(STM_VERSION)
	@echo "     "$(OBJROOT)/mesh_$(STM_VERSION) complete

$(OBJROOT)/mesh_$(STM_VERSION): $(OBJFILES) $(STM_LIB)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

$(OBJDIR)/point.o: point.cc $(STM_LIB) $(MESH_HEADERS)
	$(CXX) $(CXXFLAGS) $(INCLUDEPATH) -o $@ -c $<

$(OBJDIR)/edge.o: edge.cc  $(STM_LIB) $(MESH_HEADERS)
	$(CXX) $(CXXFLAGS) $(INCLUDEPATH) -o $@ -c $<

$(OBJDIR)/edge_set.o: edge_set.cc  $(STM_LIB) $(MESH_HEADERS)
	$(CXX) $(CXXFLAGS) $(INCLUDEPATH) -o $@ -c $<

$(OBJDIR)/side.o: side.cc  $(STM_LIB) $(MESH_HEADERS)
	$(CXX) $(CXXFLAGS) $(INCLUDEPATH) -o $@ -c $<

$(OBJDIR)/dwyer.o: dwyer.cc  $(STM_LIB) $(MESH_HEADERS)
	$(CXX) $(CXXFLAGS) $(INCLUDEPATH) -o $@ -c $<

$(OBJDIR)/worker.o: worker.cc  $(STM_LIB) $(MESH_HEADERS)
	$(CXX) $(CXXFLAGS) $(INCLUDEPATH) -o $@ -c $<

$(OBJDIR)/my_thread.o: my_thread.cc  $(STM_LIB) $(MESH_HEADERS)
	$(CXX) $(CXXFLAGS) $(INCLUDEPATH) -o $@ -c $<

$(OBJDIR)/mesh.o: mesh.cc  $(STM_LIB) $(MESH_HEADERS)
	$(CXX) $(CXXFLAGS) $(INCLUDEPATH) -o $@ -c $<

$(OBJDIR)/queues.o: queues.cc  $(STM_LIB) $(MESH_HEADERS)
	$(CXX) $(CXXFLAGS) $(INCLUDEPATH) -o $@ -c $<

include ../Makefile.targets
