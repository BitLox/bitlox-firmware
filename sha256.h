/** \file sha256.h
  *
  * \brief Describes functions exported by sha256.c.
  *
  * To calculate a SHA-256 hash, call sha256Begin(), then call
  * sha256WriteByte() for each byte of the message, then call
  * sha256Finish() (or sha256FinishDouble(), if you want a double SHA-256
  * hash). The hash will be in HashState#h, but it can also be
  * extracted and placed into to a byte array using writeHashToByteArray().
  *
  * This file is licensed as described by the file LICENCE.
  */

#ifndef SHA256_H_INCLUDED
#define SHA256_H_INCLUDED

#include "common.h"
#include "hash.h"

#ifdef __cplusplus
     extern "C" {
#endif


extern void sha256Begin(HashState *hs);
extern void sha256WriteByte(HashState *hs, uint8_t byte);
extern void sha256Finish(HashState *hs);
extern void sha256FinishDouble(HashState *hs);

#ifdef __cplusplus
     }
#endif


#endif // #ifndef SHA256_H_INCLUDED
