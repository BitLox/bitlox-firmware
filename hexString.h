/*
 * hexString.h
 *
 *  Created on: Sep 13, 2014
 *      Author: thesquid
 */

#ifndef ARM_BITLOX_54_HEXSTRING_H_
#define ARM_BITLOX_54_HEXSTRING_H_

/*
 *  hexString.h
 *  byteutils
 *
 *  Created by Richard Murphy on 3/7/10.
 *  Copyright 2010 McKenzie-Murphy. All rights reserved.
 *
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

uint8_t
*hexStringToBytes(char *inhex);

char
*bytesToHexString(uint8_t *bytes, size_t buflen);

#endif /* ARM_BITLOX_54_HEXSTRING_H_ */
