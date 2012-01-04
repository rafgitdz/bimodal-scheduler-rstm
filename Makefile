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

##################################################
#
# Warning: 	only works with GNU make
#
##################################################

include Makefile.in

.PHONY: all common_objects benchmarks mm_objects cm_objects \
        $(STM_VERSION)_lib clean realclean tags

all: info benchmarks

$(STM_VERSION)_lib:
	@cd stm && $(MAKE) $(MFLAGS)
	@cd stm/scheduler && $(MAKE) $(MFLAGS)

benchmarks: $(STM_VERSION)_lib
	@cd bench && $(MAKE) $(MFLAGS)

mesh: info $(STM_VERSION)_lib
	@cd mesh && $(MAKE) $(MFLAGS)

clean:
	@cd stm && $(MAKE) clean
	@cd stm/scheduler && $(MAKE) clean
	@cd bench && $(MAKE) clean
	@cd mesh && $(MAKE) clean
	rm -f TAGS*

realclean:
	@cd stm && $(MAKE) realclean
	@cd stm/scheduler && $(MAKE) realclean
	@cd bench && $(MAKE) realclean
	@cd mesh && $(MAKE) realclean
