/** \file endian.h
  *
  * \brief Describes functions exported by endian.c
  *
  * This file is licensed as described by the file LICENCE.
  */

#ifndef ENDIAN_H_INCLUDED
#define ENDIAN_H_INCLUDED

#include "common.h"

#ifdef __cplusplus
     extern "C" {
#endif


extern void writeU32BigEndian(uint8_t *out, uint32_t in);
extern void writeU32LittleEndian(uint8_t *out, uint32_t in);
extern uint32_t readU32BigEndian(uint8_t *in);
extern uint32_t readU32LittleEndian(uint8_t *in);
extern void swapEndian(uint32_t *v);

#ifdef __cplusplus
     }
#endif


#endif // #ifndef ENDIAN_H_INCLUDED
