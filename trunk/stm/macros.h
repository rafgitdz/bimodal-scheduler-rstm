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

#ifndef __MACROS_H__
#define __MACROS_H__

// this is a temporary place for keeping the API parts that are common to many
// (but not all) of our TMs

/**
 *  Macro for starting a transaction.  This macro hides the ugliness of our
 *  while/try double nesting.  with the new API, one should not need to name
 *  the descriptor and pass it around.  We're keeping it in right now because
 *  it avoids a pthread call in END_TRANSACTION for getting the current
 *  descriptor.
 */
#define BEGIN_TRANSACTION                                               \
    stm::internal::Descriptor* locallyCachedTransactionContext = NULL;  \
    do {                                                                \
        try {                                                           \
            locallyCachedTransactionContext =                           \
                stm::internal::begin_transaction();
            // body of transaction goes here //


/**
 *  Macro for ending a transaction.
 *
 * NB: catch (...) needs to actually release the objects owned by this tx and
 * then it needs to reset the descriptor.  Otherwise, if there is a catch
 * outside of the TX and then we try to run a new TX, the descriptor we reuse
 * will have stale information that we don't want to lose (for example, it
 * might have visible read bits set)
 */
#define END_TRANSACTION                                     \
            locallyCachedTransactionContext->commit();      \
        } catch (stm::Aborted) {                            \
        } catch (...) {                                     \
            locallyCachedTransactionContext->abort(false);  \
            locallyCachedTransactionContext->cleanup();     \
            throw;                                          \
        }                                                   \
    } while (!locallyCachedTransactionContext->cleanup()    \
             && !stm::terminate);                           \
    locallyCachedTransactionContext = NULL;

#endif // __MACROS_H__
