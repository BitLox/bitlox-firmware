#ifndef DUE_QRENCODE_H
#define DUE_QRENCODE_H
// This code is derived from https://github.com/tz1/qrduino.git
// Copyright 2010, tz@execpc.com.
// Copyright 2013, Steven Pearson
// Released under ther terms of the GNU General Public License v3.
// that can be found in the GPLV3_LICENSE file.

#if defined (__GNUC__) && defined (__AVR__)
#include <avr/pgmspace.h>
#else
#include <stdint.h>
#define PROGMEM /* empty */
#define pgm_read_byte(x) (*(x))
#define pgm_read_word(x) (*(x))
#define pgm_read_float(x) (*(x))
#define __LPM(x) (*(x))

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

#endif

const unsigned char WD = 41;
const unsigned char WDB = 6;

struct qrcontext{
  public:
    void qrencode(const char* strinbuf);
    unsigned char getQRBit(int x,int y) const;

  public:
    //TODO: these lookup tables are only needed during encoding
    unsigned char g0log[256];
    unsigned char g0exp[256];
    //strinbuf is only needed during encoding
    unsigned char strinbuf[246];

    unsigned char qrframe[600];

    void gentables();
    void initrspoly(unsigned char eclen, unsigned char *genpoly);
    void appendrs(unsigned char*, unsigned char, unsigned char*, unsigned char, unsigned char*);
    void stringtoqr();
    void applymask();
    void addfmt();
    void fillframe();
};

#endif
