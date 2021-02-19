/*
 * avr2arm.h
 *
 *  Created on: Oct 9, 2014
 *      Author: thesquid
 */

#ifndef AVR2ARM_H_
#define AVR2ARM_H_

#if defined (__GNUC__) && defined (__AVR__)
#include <avr/pgmspace.h>
#else
#include <stdint.h>
#define PROGMEM /* empty */
#define pgm_read_byte(x) (*(x))
#define pgm_read_word(x) (*(x))
#define pgm_read_float(x) (*(x))
//#define pgm_read_byte_near(x) (*(x))
#define __LPM(x) (*(x))

//typedef const unsigned char prog_uchar;
//#define pgm_read_byte_near(x) (*(prog_uchar*)x)
//#define pgm_read_byte(x) (*(prog_uchar*)x)

#define strtok_P(a,b) strtok((a),(b))
#define memcmp_P(dest, src, num) memcmp((dest), (src), (num))
#define memcpy_P(a, b, c) memcpy((a), (b), (c))
#define strcasecmp_P(a,b,num) strcasecmp((a),(b),(num))
#define strcat_P(a,b) strcat((a),(b))
#define strcmp_P(a,b) strcmp((a),(b))
#define strcpy_P(a,b) strcpy((a),(b))
#define strlcat_P(a,b,c) strlcat((a),(b),(c))
#define strlcpy_P(a,b,c) strlcpy((a),(b),(c))
#define strlen_P(a) strlen((a))
#define strncasecmp_P(a,b,num) strncasecmp((a),(b),(num))
#define strncat_P(a,b,c) strncat((a),(b),(c))
#define strncmp_P(a,b,c) strncmp((a),(b),(c))
#define strncpy_P(a,b,c) strncpy((a),(b),(c))
#define strnlen_P(a,b) strnlen((a),(b))
#define strstr_P(a,b) strstr((a),(b))

#define strcasestr_P(a,b) strcasestr((a),(b))

#define _delay_ms(x) delay(x)
#endif

#ifdef __cplusplus
     extern "C" {
#endif

//     static inline void __ISB() { __asm volatile ("isb"); }
//     static inline void __DSB() { __asm volatile ("dsb"); }
//     static inline void __DMB() { __asm volatile ("dmb"); }


//     void CriticalSectionEnter(void);

#ifdef __cplusplus
     }
#endif



#endif /* AVR2ARM_H_ */
