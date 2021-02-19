/**
 * Copyright (c) 2013-2014 Tomas Dzetkulic
 * Copyright (c) 2013-2014 Pavol Rusnak
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES
 * OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "common.h"
#include "hmac_trez.h"
#include "rand_trez.h"
#include "sha2_trez.h"
#include "bip39_english.h"
#include "arm/eink.h"
#include "bip39_trez_prev.h"
#include "pbkdf2_trez_prev.h"
#include "arm/keypad_alpha.h"


const char *mnemonic_from_input_indices(int len);


const char *mnemonic_generate(int strength)
{
	if (strength % 32 || strength < 128 || strength > 256) {
		return 0;
	}
//	this should call the atsha204a for a truly random string OR the internal TRNG YEA!
//	uint8_t data[32] = {199,219,173,7,1,65,77,155,9,160,47,40,174,155,130,112,65,197,246,201,160,3,12,40,208,35,57,231,35,53,209,9};

	uint8_t data[32] = {};
	hardwareRandom32Bytes(data);

	return mnemonic_from_data(data, strength / 8);
}

const char *mnemonic_generate_from_input(int strength)
{
	if (strength % 32 || strength < 128 || strength > 256) {
		return 0;
	}
//	this should call the atsha204a for a truly random string OR the internal TRNG YEA!
//	uint8_t data[32] = {199,219,173,7,1,65,77,155,9,160,47,40,174,155,130,112,65,197,246,201,160,3,12,40,208,35,57,231,35,53,209,9};

//	uint8_t data[32] = {};
//	hardwareRandom32Bytes(data);


	return mnemonic_from_input_indices(strength / 8);
}

const char *mnemonic_from_data(const uint8_t *data, int len)
{
	if (len % 4 || len < 16 || len > 32) {
		return 0;
	}

	uint8_t bits[32 + 1];

	sha256_Raw(data, len, bits);
	// checksum
	bits[len] = bits[0];
	// data
	memcpy(bits, data, len);

	int mlen = len * 3 / 4;
	static char mnemo[24 * 10];
	memset(&mnemo[0], 0, sizeof(mnemo));


	int i, j, idx;
	char *p = mnemo;
	for (i = 0; i < mlen; i++) {
		idx = 0;
		for (j = 0; j < 11; j++) {
			idx <<= 1;
			idx += (bits[(i * 11 + j) / 8] & (1 << (7 - ((i * 11 + j) % 8)))) > 0;
		}
		strcpy(p, wordlist[idx]);
		p += strlen(wordlist[idx]);
		*p = (i < mlen - 1) ? ' ' : 0;
		p++;
	}

	return mnemo;
}

const char *mnemonic_from_input_indices(int len)
{
	if (len % 4 || len < 16 || len > 32) {
		return 0;
	}

	writeEinkDisplay("Please input indices", false, COL_1_X, LINE_1_Y, "",false,5,30, "",false,5,50, "",false,5,70, "",false,0,0);


	int mlen = len * 3 / 4;

	static char mnemo[24 * 10];
	memset(&mnemo[0], 0, sizeof(mnemo));


	char mlenText[1];
	sprintf(mlenText,"%lu", mlen);


	int i, j, idx;
	char *p = mnemo;
	for (i = 0; i < mlen; i++) {
		const char *theResult;
		char iText[1];
		sprintf(iText,"%lu", i+1);
//		writeEinkNoDisplay("INDEX",  COL_1_X, LINE_1_Y, iText,COL_1_X+75,LINE_1_Y, "of",COL_1_X+100,LINE_1_Y, mlenText,COL_1_X+125,LINE_1_Y, "",25,80);

		writeEinkDisplay(iText, false, COL_1_X, LINE_1_Y, "of", false, COL_1_X+24, LINE_1_Y, mlenText, false, COL_1_X+48, LINE_1_Y, "",false,5,70, "",false,0,0);

		theResult = getInputIndices(false, false);
		writeEinkDisplay(theResult, false, COL_1_X+5, LINE_1_Y, "", false, COL_1_X+24, LINE_1_Y, "", false, COL_1_X+48, LINE_1_Y, "",false,5,70, "",false,0,0);

		idx = atoi(theResult);

		writeEinkDisplay(wordlist[idx], false, COL_1_X+5, LINE_1_Y, "", false, COL_1_X+24, LINE_1_Y, "", false, COL_1_X+48, LINE_1_Y, "",false,5,70, "",false,0,0);

		strcpy(p, wordlist[idx]);
		p += strlen(wordlist[idx]);
		*p = (i < mlen - 1) ? ' ' : 0;
		p++;
	}
	showWorking();
	return mnemo;
}



int mnemonic_check(const char *mnemonic)
{
	if (!mnemonic) {
		return 0;
	}

	uint32_t i, n;

	i = 0; n = 0;
	while (mnemonic[i]) {
		if (mnemonic[i] == ' ') {
			n++;
		}
		i++;
	}
	n++;
	// check number of words
	if (n != 12 && n != 18 && n != 24) {
		return 0;
	}

	char current_word[10];
	uint32_t j, k, ki, bi;
	uint8_t bits[32 + 1];
	memset(bits, 0, sizeof(bits));
	i = 0; bi = 0;
	while (mnemonic[i]) {
		j = 0;
		while (mnemonic[i] != ' ' && mnemonic[i] != 0) {
			if (j >= sizeof(current_word)) {
				return 0;
			}
			current_word[j] = mnemonic[i];
			i++; j++;
		}
		current_word[j] = 0;
		if (mnemonic[i] != 0) i++;
		k = 0;
		for (;;) {
			if (!wordlist[k]) { // word not found
				return 0;
			}
			if (strcmp(current_word, wordlist[k]) == 0) { // word found on index k
				for (ki = 0; ki < 11; ki++) {
					if (k & (1 << (10 - ki))) {
						bits[bi / 8] |= 1 << (7 - (bi % 8));
					}
					bi++;
				}
				break;
			}
			k++;
		}
	}
	if (bi != n * 11) {
		return 0;
	}
	bits[32] = bits[n * 4 / 3];
	sha256_Raw(bits, n * 4 / 3, bits);
	if (n == 12) {
		return (bits[0] & 0xF0) == (bits[32] & 0xF0); // compare first 4 bits
	} else
	if (n == 18) {
		return (bits[0] & 0xFC) == (bits[32] & 0xFC); // compare first 6 bits
	} else
	if (n == 24) {
		return bits[0] == bits[32]; // compare 8 bits
	}
	return 0;
}

void mnemonic_to_seed(const char *mnemonic, const char *passphrase, uint8_t seed[512 / 8])
{
	uint8_t salt[8 + 256 + 4];
	int saltlen = strlen(passphrase);
	memcpy(salt, "mnemonic", 8);
	memcpy(salt + 8, passphrase, saltlen);
	saltlen += 8;
	pbkdf2_trez((const uint8_t *)mnemonic, strlen(mnemonic), salt, saltlen, BIP39_PBKDF2_ROUNDS, seed, 512 / 8);
}

const char **mnemonic_wordlist(void)
{
	return wordlist;
}






