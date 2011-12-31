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

#include <iostream>

#include "ContentionManager.h"

#ifdef RSTM
#include "Descriptor_rstm.h"
#endif

#ifdef REDO_LOCK
#include "Descriptor_redo_lock.h"
#endif

#include "BiModalCM.h"

// create the appropriate cm based on the input string
stm::cm::ContentionManager* stm::cm::Factory(std::string cm_type)
{
	#ifdef USE_BIMODAL
	if (cm_type == "Bimodal")
		return new BiModalCM();
	#endif
	
    if (cm_type == "Eruption")
        return new Eruption();
    else if (cm_type == "Serializer")
        return new Serializer();
    else if (cm_type == "Justice")
        return new Justice();
    else if (cm_type == "Karma")
        return new Karma();
    else if (cm_type == "Killblocked")
        return new Killblocked();
    else if (cm_type == "Polite")
        return new Polite();
    else if (cm_type == "Polka")
        return new Polka();
    else if (cm_type == "Polkaruption")
        return new Polkaruption();
    else if (cm_type == "Timestamp")
        return new Timestamp();
    else if (cm_type == "Aggressive")
        return new Aggressive();
    else if (cm_type == "Highlander")
        return new Highlander();
    else if (cm_type == "Whpolka")
        return new Whpolka();
    else if (cm_type == "Greedy")
        return new Greedy();
    else if (cm_type == "Polkavis")
        return new PolkaVis();
    else {
        std::cerr << "*** Warning: unknown contention manager "
                  << cm_type << std::endl;
        std::cerr << "*** Defaulting to Polka." << std::endl;
        return new Polka();
    }
}

// provide backing for the Greedy and Serializer timeCounter variables
volatile unsigned long stm::cm::Greedy::timeCounter = 0;
volatile unsigned long stm::cm::Serializer::timeCounter = 0;

// for polkavis, writers get permission to abort vis readers
bool stm::cm::PolkaVis::ShouldAbortAll(unsigned long bitmap)
{
    unsigned int index = 0;
    unsigned int bits = bitmap;

    while (bits) {
        if (bits & 1) {
            stm::internal::Descriptor* reader =
                stm::internal::desc_array[index];

            if (reader->cm.getCM() != this)
                // if can't abort this reader, return false
                if (!Polka::ShouldAbort(reader->cm.getCM()))
                    return false;
        }
        bits >>= 1;
        index++;
    }
    return true;
}
