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

#ifndef __ACCESSORS_H__
#define __ACCESSORS_H__

/**
 *  macro for concatenating two tokens into a new token.  To be safe, this
 *  macro that performs the CONCAT must be separate from the macros that call
 *  it
 */
#define TOKEN_CONCAT(a,b)  a ## b

/**
 *  using the type t and the name n, generate a protected declaration for the
 *  field, as well as public getters and setters
 */
#define GENERATE_FIELD(t, n)                                            \
  /* declare the field, with its name prefixed by m_ */                 \
  protected:                                                            \
    t TOKEN_CONCAT(m_, n);                                              \
  public:                                                               \
  /* declare a getter for a readable (const) version of the object */   \
  t TOKEN_CONCAT(get_, n)(const stm::internal::Validator& v) const      \
  {                                                                     \
      t ret = TOKEN_CONCAT(m_, n);                                      \
      v.validate(this);                                                 \
      return ret;                                                       \
  }                                                                     \
  /* declare a getter for a writable version of the object */           \
  t TOKEN_CONCAT(get_, n)(const stm::internal::Validator& v)            \
  {                                                                     \
      return TOKEN_CONCAT(m_, n);                                       \
  }                                                                     \
  /* declare a setter for a writable version of the object*/            \
  void TOKEN_CONCAT(set_, n)(t TOKEN_CONCAT(tmp_, n))                   \
  {                                                                     \
      TOKEN_CONCAT(m_, n) = TOKEN_CONCAT(tmp_, n);                      \
  }

/**
 *  using the type t and the name n, generate a protected declaration for a
 *  field that is an array of size s, as well as public getters and setters
 */
#define GENERATE_ARRAY(t, n, s)                                         \
  /* declare the field, with its name prefixed by m_ */                 \
  protected:                                                            \
    t TOKEN_CONCAT(m_, n)[s];                                           \
  public:                                                               \
  /* declare a getter for a readale (const) version of the object */    \
  t TOKEN_CONCAT(get_, n)(int i, const stm::internal::Validator& v) const \
  {                                                                     \
      t ret = TOKEN_CONCAT(m_, n)[i];                                   \
      v.validate(this);                                                 \
      return ret;                                                       \
  }                                                                     \
  /* declare a getter for a writable version of the object */           \
  t TOKEN_CONCAT(get_, n)(int i, const stm::internal::Validator& v)     \
  {                                                                     \
      return TOKEN_CONCAT(m_, n)[i];                                    \
  }                                                                     \
  /* declare a setter for a writable version of the object */           \
  void TOKEN_CONCAT(set_, n)(int i, t TOKEN_CONCAT(tmp_, n))            \
  {                                                                     \
      TOKEN_CONCAT(m_, n)[i] = TOKEN_CONCAT(tmp_, n);                   \
  }

/**
 * using the type t and the name n, generate a protected declaration for a
 * field that is a 2D array of size [xx,yy], as well as public getters and
 * setters
 */
#define GENERATE_2DARRAY(t, n, xx, yy)                                  \
  /* declare the field, with its name prefixed by m_ */                 \
  protected:                                                            \
    t TOKEN_CONCAT(m_, n)[xx][yy];                                      \
  public:                                                               \
  /* declare a getter for a readale (const) version of the object */    \
  t TOKEN_CONCAT(get_, n)(int x, int y, const stm::internal::Validator& v) \
                          const                                         \
  {                                                                     \
      t ret = TOKEN_CONCAT(m_, n)[x][y];                                \
      v.validate(this);                                                 \
      return ret;                                                       \
  }                                                                     \
  /* declare a getter for a writable version of the object */           \
  t TOKEN_CONCAT(get_, n)(int x, int y,                                 \
                          const stm::internal::Validator& v)            \
  {                                                                     \
      return TOKEN_CONCAT(m_, n)[x][y];                                 \
  }                                                                     \
  /* declare a setter for a writable version of the object */           \
  void TOKEN_CONCAT(set_, n)(int x, int y,                              \
                             t TOKEN_CONCAT(tmp_, n))                   \
  {                                                                     \
      TOKEN_CONCAT(m_, n)[x][y] = TOKEN_CONCAT(tmp_, n);                \
  }

#endif // __ACCESSORS_H__
