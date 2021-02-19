/** \file prandom.c
  *
  * \brief Deals with random and pseudo-random number generation.
  *
  * At the moment this covers whitening of random inputs (getRandom256()) and
  * deterministic private key generation (generateDeterministic256()).
  *
  * The suggestion to use a persistent entropy pool, and much of the code
  * associated with the entropy pool, are attributed to Peter Todd (retep).
  *
  * This file is licensed as described by the file LICENCE.
  */

#ifdef TEST
#include <stdlib.h>
#include <assert.h>
#endif // #ifdef TEST

#ifdef TEST_PRANDOM
#include <stdio.h>
#include "test_helpers.h"
#include "wallet.h"

#endif // #ifdef TEST_PRANDOM

#include "common.h"
#include "aes.h"
#include "sha256.h"
#include "ripemd160.h"
#include "hmac_sha512.h"
#include "endian.h"
#include "ecdsa.h"
#include "bignum256.h"
#include "transaction.h" // for swapEndian256()
#include "prandom.h"
#include "hwinterface.h"
#include "storage_common.h"
#include "arm/eink.h"
#include "arm/lcd_and_input.h"

#include <string.h>

#include "base58_trez.h"
#include "bignum_trez.h"
#include "bip32_trez.h"
#include "ecdsa_trez.h"
#include "hmac_trez.h"
#include "ripemd160_trez.h"
#include "sha2_trez.h"


/** Because stdlib.h might not be included, NULL might be undefined. NULL
  * is only used as a placeholder pointer for getRandom256Internal() if
  * there is no appropriate pointer. */
#ifndef NULL
#define NULL ((void *)0) 
#endif // #ifndef NULL

/** The parent public key for the BIP 0032 deterministic key generator (see
  * generateDeterministic256()). The contents of this variable are only valid
  * if #cached_parent_public_key_valid is true.
  *
  * generateDeterministic256() could calculate the parent public key each time
  * a new deterministic key is requested. However, that would slow down
  * deterministic key generation significantly, as point multiplication would
  * be required each time a key was requested. So this variable functions as
  * a cache.
  * \warning The x and y components are stored in little-endian format.
  */
static PointAffine cached_parent_public_key;
/** Specifies whether the contents of #parent_public_key are valid. */
static bool cached_parent_public_key_valid;



//For outputting the xpubs. We need to collect all the utilities in one file.

uint8_t *fromhex(const char *str) {
	static uint8_t buf[128];
	uint8_t c;
	size_t i;
	for (i = 0; i < strlen(str) / 2; i++) {
		c = 0;
		if (str[i * 2] >= '0' && str[i * 2] <= '9')
			c += (str[i * 2] - '0') << 4;
		if (str[i * 2] >= 'a' && str[i * 2] <= 'f')
			c += (10 + str[i * 2] - 'a') << 4;
		if (str[i * 2] >= 'A' && str[i * 2] <= 'F')
			c += (10 + str[i * 2] - 'A') << 4;
		if (str[i * 2 + 1] >= '0' && str[i * 2 + 1] <= '9')
			c += (str[i * 2 + 1] - '0');
		if (str[i * 2 + 1] >= 'a' && str[i * 2 + 1] <= 'f')
			c += (10 + str[i * 2 + 1] - 'a');
		if (str[i * 2 + 1] >= 'A' && str[i * 2 + 1] <= 'F')
			c += (10 + str[i * 2 + 1] - 'A');
		buf[i] = c;
	}
	return buf;
}

char *tohex(const uint8_t *bin, size_t l) {
	char *buf = (char *) malloc(l * 2 + 1);
	static char digits[] = "0123456789abcdef";
	size_t i;
	for (i = 0; i < l; i++) {
		buf[i * 2] = digits[(bin[i] >> 4) & 0xF];
		buf[i * 2 + 1] = digits[bin[i] & 0xF];
	}
	buf[l * 2] = 0;
	return buf;
}









/** Set the parent public key for the deterministic key generator (see
  * generateDeterministic256()). This function will speed up subsequent calls
  * to generateDeterministic256(), by allowing it to use a cached parent
  * public key.
  * \param parent_private_key The parent private key, from which the parent
  *                           public key will be derived. Note that this
  *                           should be in little-endian format.
  */
static void setParentPublicKeyFromPrivateKey(BigNum256 parent_private_key)
{
	HashState hs;


	setToG(&cached_parent_public_key);
	pointMultiply(&cached_parent_public_key, parent_private_key);
	cached_parent_public_key_valid = true;


///////////////////////
//	setToG(out_public_key);
//	pointMultiply(out_public_key, buffer);
/*
	sha256Begin(&hs);

//	sha256WriteByte(&hs, 0x04);
	if ((out_public_key->y[0])%2 == 0)
	{
		sha256WriteByte(&hs, 0x02);
	}
	else
	{
		sha256WriteByte(&hs, 0x03);
	}

	for (i = 32; i--; )
	{
		sha256WriteByte(&hs, out_public_key->x[i]);
	}
//	for (i = 32; i--; )
//	{
//		sha256WriteByte(&hs, out_public_key->y[i]);
//	}
	sha256Finish(&hs);
	writeHashToByteArray(buffer, &hs, true);













*/
///////////////////////
}

/** Clear the parent public key cache (see #parent_private_key). This should
  * be called whenever a wallet is unloaded, so that subsequent calls to
  * generateDeterministic256() don't result in addresses from the old wallet.
  */
void clearParentPublicKeyCache(void)
{
	memset(&cached_parent_public_key, 0xff, sizeof(cached_parent_public_key)); // just to be sure
	memset(&cached_parent_public_key, 0, sizeof(cached_parent_public_key));
	cached_parent_public_key_valid = false;
}

/** Calculate the entropy pool checksum of an entropy pool state.
  * Without integrity checks, an attacker with access to the persistent
  * entropy pool area (in non-volatile memory) could reduce the amount of
  * entropy in the persistent pool. Even if the persistent entropy pool is
  * encrypted, an attacker could reduce the amount of entropy in the pool down
  * to the amount of entropy in the encryption key, which is probably much
  * less than 256 bits.
  * If the persistent entropy pool is unencrypted, then the checksum provides
  * no additional security. In that case, the checksum is only used to check
  * that non-volatile memory is working as expected.
  * \param out The checksum will be written here. This must be a byte array
  *            with space for #POOL_CHECKSUM_LENGTH bytes.
  * \param pool_state The entropy pool state to calculate the checksum of.
  *                   This must be a byte array of
  *                   length #ENTROPY_POOL_LENGTH.
  */
static void calculateEntropyPoolChecksum(uint8_t *out, uint8_t *pool_state)
{
	HashState hs;
	uint8_t hash[32];
	uint8_t i;

	// RIPEMD-160 is used instead of SHA-256 because SHA-256 is already used
	// by getRandom256() to generate output values from the pool state.
	ripemd160Begin(&hs);
	for (i = 0; i < ENTROPY_POOL_LENGTH; i++)
	{
		ripemd160WriteByte(&hs, pool_state[i]);
	}
	ripemd160Finish(&hs);
	writeHashToByteArray(hash, &hs, true);
#if POOL_CHECKSUM_LENGTH > 20
#error "POOL_CHECKSUM_LENGTH is bigger than RIPEMD-160 hash size"
#endif
	memcpy(out, hash, POOL_CHECKSUM_LENGTH);
}

/** Set (overwrite) the persistent entropy pool.
  * \param in_pool_state A byte array specifying the desired contents of the
  *                      persistent entropy pool. This must have a length
  *                      of #ENTROPY_POOL_LENGTH bytes.
  * \return false on success, true if an error (couldn't write to non-volatile
  *         memory) occurred.
  */
bool setEntropyPool(uint8_t *in_pool_state)
{
	uint8_t checksum[POOL_CHECKSUM_LENGTH];

	if (nonVolatileWrite(in_pool_state, ENTROPY_POOL_ADDRESS, ENTROPY_POOL_LENGTH) != NV_NO_ERROR)
	{
//		writeEink("NV write error 1", false, 10, 10);
		return true; // non-volatile write error
	}
	calculateEntropyPoolChecksum(checksum, in_pool_state);
	if (nonVolatileWrite(checksum, POOL_CHECKSUM_ADDRESS, POOL_CHECKSUM_LENGTH) != NV_NO_ERROR)
	{
//		writeEink("NV write error 2", false, 10, 10);
		return true; // non-volatile write error
	}
	if (nonVolatileFlush() != NV_NO_ERROR)
	{
//		writeEink("NV write error 3", false, 10, 10);
		return true; // non-volatile write error
	}
//	writeEink("setEntropyPool:0", false, 10, 10);
	return false; // success
}

/** Obtain the contents of the persistent entropy pool.
  * \param out_pool_state A byte array specifying where the contents of the
  *                       persistent entropy pool should be placed. This must
  *                       have space for #ENTROPY_POOL_LENGTH bytes.
  * \return false on success, true if an error (couldn't read from
  *         non-volatile memory, or invalid checksum) occurred.
  */
bool getEntropyPool(uint8_t *out_pool_state)
{
	uint8_t checksum_read[POOL_CHECKSUM_LENGTH];
	uint8_t checksum_calculated[POOL_CHECKSUM_LENGTH];

	if (nonVolatileRead(out_pool_state, ENTROPY_POOL_ADDRESS, ENTROPY_POOL_LENGTH) != NV_NO_ERROR)
	{
//		writeEink("non-volatile read error 1", false, 10, 10);
		return true; // non-volatile read error
	}
	calculateEntropyPoolChecksum(checksum_calculated, out_pool_state);
	if (nonVolatileRead(checksum_read, POOL_CHECKSUM_ADDRESS, POOL_CHECKSUM_LENGTH) != NV_NO_ERROR)
	{
//		writeEink("non-volatile read error 2", false, 10, 10);
		return true; // non-volatile read error
	}
	if (memcmp(checksum_read, checksum_calculated, POOL_CHECKSUM_LENGTH))
	{
//		writeEink("checksum doesn't match", false, 10, 10);
		return true; // checksum doesn't match
	}
	return false; // success
}

/** Initialise the persistent entropy pool to a specified state. If the
  * current entropy pool is uncorrupted, then its state will be mixed in with
  * the specified state.
  * \param initial_pool_state The initial entropy pool state. This must be a
  *                           byte array of length #ENTROPY_POOL_LENGTH bytes.
  * \return false on success, true if an error (couldn't write to
  *         non-volatile memory) occurred.
  */
bool initialiseEntropyPool(uint8_t *initial_pool_state)
{
	HashState hs;
	uint8_t current_pool_state[ENTROPY_POOL_LENGTH];
	uint8_t i;

	if (getEntropyPool(current_pool_state))
	{
//		writeEink("pool not valid", false, 10, 10);
		// Current entropy pool is not valid; overwrite it.
		return setEntropyPool(initial_pool_state);
	}
	else
	{
//		writeEink("pool valid", false, 10, 10);
		// Current entropy pool is valid; mix it in with initial_pool_state.
		sha256Begin(&hs);
		for (i = 0; i < ENTROPY_POOL_LENGTH; i++)
		{
			sha256WriteByte(&hs, current_pool_state[i]);
			sha256WriteByte(&hs, initial_pool_state[i]);
		}
		sha256Finish(&hs);
		writeHashToByteArray(current_pool_state, &hs, true);
		return setEntropyPool(current_pool_state);
	}
}

/** Safety factor for entropy accumulation. The hardware random number
  * generator can (but should strive not to) overestimate its entropy. It can
  * overestimate its entropy by this factor without loss of security. */
#define ENTROPY_SAFETY_FACTOR	2


/** Uses a hash function to accumulate entropy from a hardware random number
  * generator (HWRNG), along with the state of a persistent pool. The
  * operations used are: intermediate = H(HWRNG | pool),
  * output = H(H(intermediate)) and new_pool = H(intermediate | padding),
  * where "|" is concatenation, H(x) is the SHA-256 hash of x and padding
  * consists of 32 0x42 bytes.
  *
  * To justify why a cryptographic hash is an appropriate means of entropy
  * accumulation, see the paper "Yarrow-160: Notes on the Design and Analysis
  * of the Yarrow Cryptographic Pseudorandom Number Generator" by J. Kelsey,
  * B. Schneier and N. Ferguson, obtained from
  * http://www.schneier.com/paper-yarrow.html on 14-April-2012. Specifically,
  * section 5.2 addresses entropy accumulation by a hash function.
  *
  * Entropy is accumulated by hashing bytes obtained from the HWRNG until the
  * total entropy (as reported by the HWRNG) is at least
  * 256 * ENTROPY_SAFETY_FACTOR bits.
  * If the HWRNG breaks in a way that is undetected, the (maybe secret) pool
  * of random bits ensures that outputs will still be unpredictable, albeit
  * not strictly meeting their advertised amount of entropy.
  * \param n The final 256 bit random value will be written here.
  * \param pool_state If use_pool_state is true, then the the state of the
  *                   persistent entropy pool will be read from and written to
  *                   this byte array. The byte array must be of
  *                   length #ENTROPY_POOL_LENGTH bytes. If use_pool_state is
  *                   false, this parameter will be ignored.
  * \param use_pool_state Specifies whether to use RAM (true) or
  *                       non-volatile memory (false) to access the persistent
  *                       entropy pool. If this is true, the persistent
  *                       entropy pool will be read/written from/to the byte
  *                       array specified by pool_state. If this is false, the
  *                       persistent entropy pool will be read/written from/to
  *                       non-volatile memory. The option of using RAM is
  *                       provided for cases where random numbers are needed
  *                       but non-volatile memory is being cleared.
  * \return false on success, true if an error (couldn't access
  *         non-volatile memory, or invalid entropy pool checksum) occurred.
  */
static bool getRandom256Internal(BigNum256 n, uint8_t *pool_state, bool use_pool_state)
{
	int r;
	uint16_t total_entropy;
	uint8_t random_bytes[MAX(32, ENTROPY_POOL_LENGTH)];
	uint8_t intermediate[32];
	HashState hs;
	uint8_t i;

	// Hash in HWRNG randomness until we've reached the entropy required.
	// This needs to happen before hashing the pool itself due to the
	// possibility of length extension attacks; see below.
	total_entropy = 0;
	sha256Begin(&hs);
	while (total_entropy < (256 * ENTROPY_SAFETY_FACTOR))
	{
		r = hardwareRandom32Bytes(random_bytes);
		if (r < 0)
		{
//			return false; // TESTING HACK MUST REMOVE
			return true; // HWRNG failure
		}
		// Sometimes hardwareRandom32Bytes() returns 0, which signifies that
		// more samples are needed in order to do statistical testing.
		// hardwareRandom32Bytes() assumes it will be repeatedly called until
		// it returns a non-zero value. If anything in this while loop is
		// changed, make sure the code still respects this assumption.
		total_entropy = (uint16_t)(total_entropy + r);
		for (i = 0; i < 32; i++)
		{
			sha256WriteByte(&hs, random_bytes[i]);
		}
	}

	// Now include the previous state of the pool.
	if (use_pool_state)
	{
		memcpy(random_bytes, pool_state, ENTROPY_POOL_LENGTH);
	}
	else
	{
		if (getEntropyPool(random_bytes))
		{
			return true; // error reading from non-volatile memory, or invalid checksum
		}
	}
	for (i = 0; i < ENTROPY_POOL_LENGTH; i++)
	{
		sha256WriteByte(&hs, random_bytes[i]);
	}
	sha256Finish(&hs);
	writeHashToByteArray(intermediate, &hs, true);

	// Calculate new pool state.
	// We can't use the intermediate state as the new pool state, or an
	// attacker who obtained access to the pool state could determine
	// the most recent returned random output.
	sha256Begin(&hs);
	for (i = 0; i < 32; i++)
	{
		sha256WriteByte(&hs, intermediate[i]);
	}
	for (i = 0; i < 32; i++)
	{
		sha256WriteByte(&hs, 0x42); // padding
	}
	sha256Finish(&hs);
	writeHashToByteArray(random_bytes, &hs, true);

	// Save the pool state to non-volatile memory immediately as we don't want
	// it to be possible to reuse the pool state.
	if (use_pool_state)
	{
		memcpy(pool_state, random_bytes, ENTROPY_POOL_LENGTH);
	}
	else
	{
		if (setEntropyPool(random_bytes))
		{
			return true; // error writing to non-volatile memory
		}
	}
	// Hash the intermediate state twice to generate the random bytes to
	// return.
	// We can't output the pool state directly, or an attacker who knew that
	// the HWRNG was broken, and how it was broken, could then predict the
	// next output. Outputting H(intermediate) is another possibility, but
	// that's kinda cutting it close though, as we're outputting
	// H(intermediate) while the next pool state will be
	// H(intermediate | padding). We've prevented a length extension
	// attack as described above, but there may be other attacks.
	sha256Begin(&hs);
	for (i = 0; i < ENTROPY_POOL_LENGTH; i++)
	{
		sha256WriteByte(&hs, intermediate[i]);
	}
	sha256FinishDouble(&hs);

	writeHashToByteArray(n, &hs, true);



//TEST FIXED NUMBER RETURN SAME AS BRAINWALLET SIGNING
//	BigNum256 nTemp = {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1};
//	BigNum256 nTemp = {0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01};
//	memcpy(n, nTemp, 32);

	return false; // success
}

/** Version of getRandom256Internal() which uses non-volatile memory to store
  * the persistent entropy pool. See getRandom256Internal() for more details.
  * \param n See getRandom256Internal()
  * \return See getRandom256Internal()
  */
bool getRandom256(BigNum256 n)
{
	return getRandom256Internal(n, NULL, false);
}

/** Version of getRandom256Internal() which uses RAM to store
  * the persistent entropy pool. See getRandom256Internal() for more details.
  * \param n See getRandom256Internal()
  * \param pool_state A byte array of length #ENTROPY_POOL_LENGTH which
  *                   contains the persistent entropy pool state. This will
  *                   be both read from and written to.
  * \return See getRandom256Internal()
  */
bool getRandom256TemporaryPool(BigNum256 n, uint8_t *pool_state)
{
	return getRandom256Internal(n, pool_state, true);
}

/** Generate an insecure one-time password.
  * \param otp The generated one-time password will be written here. This must
  *            be a character array with enough space to store #OTP_LENGTH
  *            characters. The OTP will be null-terminated.
  * \warning The password generated by this function has dubious security
  *          properties. Do not use the password for anything private.
  */
void generateInsecureOTP(char *otp)
{
	unsigned int i;
	uint8_t random_bytes[32];
	uint8_t dummy_pool_state[ENTROPY_POOL_LENGTH];

	if (getRandom256(random_bytes))
	{
		// Sometimes an OTP may be required when the entropy pool hasn't
		// been initialised yet (eg. when formatting storage). In those
		// cases, use a RAM-based dummy entropy pool. This has poor security
		// properties, but then again, this function is called
		// generateInsecureOTP() for a reason.
		memset(dummy_pool_state, 42, sizeof(dummy_pool_state));
		if (getRandom256TemporaryPool(random_bytes, dummy_pool_state))
		{
			// This function must return something, even if it's not quite
			// random.
			memset(random_bytes, 42, sizeof(random_bytes));
		}
	}

#if OTP_LENGTH > 32
#error "OTP_LENGTH too big"
#endif // #if OTP_LENGTH > 32
	for (i = 0; i < (OTP_LENGTH - 1); i++)
	{
		// Each character is approximately uniformly distributed between
		// 0 and 9 (inclusive). Here, "approximately" doesn't matter because
		// this function is insecure.
		otp[i] = (char)('0' + (random_bytes[i] % 10));
	}
	otp[OTP_LENGTH - 1] = '\0';
}

/** Generate an insecure one-time password.
  * \param otp The generated one-time password will be written here. This must
  *            be a character array with enough space to store #OTP_LENGTH
  *            characters. The OTP will be null-terminated.
  * \warning The password generated by this function has dubious security
  *          properties. Do not use the password for anything private.
  *
  *          Well, being that we have altered the code so as to use the integrated TRNG
  *          I think we can have a bit more faith. Anyways, they can change it.
  */
void generateInsecurePIN(char *otp, int length)
{
	unsigned int i;
	uint8_t random_bytes[32];
	uint8_t dummy_pool_state[ENTROPY_POOL_LENGTH];

	if (getRandom256(random_bytes))
	{
		// Sometimes an OTP may be required when the entropy pool hasn't
		// been initialised yet (eg. when formatting storage). In those
		// cases, use a RAM-based dummy entropy pool. This has poor security
		// properties, but then again, this function is called
		// generateInsecureOTP() for a reason.
		memset(dummy_pool_state, 42, sizeof(dummy_pool_state));
		if (getRandom256TemporaryPool(random_bytes, dummy_pool_state))
		{
			// This function must return something, even if it's not quite
			// random.
			memset(random_bytes, 42, sizeof(random_bytes));
		}
	}

#if OTP_LENGTH > 32
#error "OTP_LENGTH too big"
#endif // #if OTP_LENGTH > 32
	for (i = 0; i < (length - 1); i++)
	{
		// Each character is approximately uniformly distributed between
		// 0 and 9 (inclusive). Here, "approximately" doesn't matter because
		// this function is insecure.
		otp[i] = (char)('0' + (random_bytes[i] % 10));
	}
	otp[length - 1] = '\0';
}

//*****************************************************************
//*****************************************************************
//*****************************************************************

//STUB FOR OTP STUFF

/** Display a short (maximum 8 characters) one-time password for the user to
  * see. This one-time password is used to reduce the chance of a user
  * accidentally doing something stupid.
  * \param command The action to ask the user about. See #AskUserCommandEnum.
  * \param otp The one-time password to display. This will be a
  *            null-terminated string.
  */
//void displayOTP(AskUserCommand command, char *otp)
//{
//
//}
/** Clear the OTP (one-time password) shown by displayOTP() from the
  * display. */
//void clearOTP(void)
//{
//
//}
//*****************************************************************
//*****************************************************************
//*****************************************************************

/** Use a combination of cryptographic primitives to deterministically
  * generate a new 256 bit number.
  *
  * The generator uses the algorithm described in
  * https://en.bitcoin.it/wiki/BIP_0032, accessed 12-November-2012 under the
  * "Specification" header. The generator generates uncompressed keys.
  *
  * \param out The generated 256 bit number will be written here.
  * \param seed Should point to a byte array of length #SEED_LENGTH containing
  *             the seed for the pseudo-random number generator. While the
  *             seed can be considered as an arbitrary array of bytes, the
  *             bytes of the array also admit the following interpretation:
  *             the first 32 bytes are the parent private key in big-endian
  *             format, and the next 32 bytes are the chain code (endian
  *             independent).
  * \param num A counter which determines which number the pseudo-random
  *            number generator will output.
  * \return false upon success, true if the specified seed is not valid (will
  *         produce degenerate private keys).
  *
  * INSERT BIP32 ROUTINES FROM BIP39_arduinoDUE HERE - pass parameters for seed depth etc. such as m/0'/0/0
  * do we need to alter the signing routines to take compressed ECDSA keys?
  *
  * OK, according to https://github.com/bitcoin/bips/commit/6a418689f3ff25730c39c19655b922ea4b7bd64d
  * this routine is completely outmoded. Will have to gut it.
  *
  */
bool generateDeterministic256(BigNum256 out, const uint8_t *seed, const uint32_t chain_lvl_1, const uint32_t chain_lvl_2, const uint32_t chain_lvl_3)
{
	uint8_t i;
	uint8_t one_byte; // current byte of seed

	uint8_t buffer[32];
	PointAffine *out_public_key;
	HashState hs;



	char strBits[3];
	char hexSeed[64] = { };

	strcpy(hexSeed, "");
	for (i = 0; i < 64; i++) {
		one_byte = seed[i];
		strBits[0] = nibbleToHex((uint8_t) (one_byte >> 4));
		strBits[1] = nibbleToHex(one_byte);
		strBits[2] = '\0';
		strcat(hexSeed, strBits);
	}

	HDNode node, node2, node3;
	char str[112];
	int r;


	// [seed]
	hdnode_from_seed(fromhex(hexSeed), 64, &node);


	// [Chain m]
	hdnode_serialize_private(&node, str);
	r = hdnode_deserialize(str, &node2);

	hdnode_serialize_public(&node, str);
	r = hdnode_deserialize(str, &node2);

	memcpy(&node3, &node, sizeof(HDNode));
	memset(&node3.private_key, 0, 32);


	// [Chain m/0']
	r = hdnode_private_ckd_prime(&node, chain_lvl_1);

	hdnode_serialize_private(&node, str);
	r = hdnode_deserialize(str, &node2);

	hdnode_serialize_public(&node, str);	// THIS is the xpub that we have to output for the new schema
	r = hdnode_deserialize(str, &node2);

	memcpy(&node3, &node, sizeof(HDNode));
	memset(&node3.private_key, 0, 32);



	// [Chain m/0'/0]
	r = hdnode_private_ckd(&node, chain_lvl_2); // 0 external, 1 internal

	hdnode_serialize_private(&node, str);
	r = hdnode_deserialize(str, &node2);

	hdnode_serialize_public(&node, str);
	r = hdnode_deserialize(str, &node2);

	memcpy(&node3, &node, sizeof(HDNode));
	memset(&node3.private_key, 0, 32);


	// [Chain m/0'/0/num]
	r = hdnode_private_ckd(&node, chain_lvl_3);

	uint8_t staticPrivate[32] = {};
	memcpy(staticPrivate, &node.private_key, 32);
	swapEndian256(staticPrivate);
	memcpy(out, staticPrivate, 32);

//	setParentPublicKeyFromPrivateKey(out);

	return false; // success
}



bool generateDeterministic256Trezor(BigNum256 out, const uint8_t *seed, const uint32_t chain_lvl_1, const uint32_t chain_lvl_2, const uint32_t chain_lvl_3)
{
	uint8_t i;
	uint8_t one_byte; // current byte of seed

	uint8_t buffer[32];
	PointAffine *out_public_key;
	HashState hs;

	char derivationPath[] = {'0p','0','0'};

	char strBits[3];
	char hexSeed[64] = { };

	strcpy(hexSeed, "");
	for (i = 0; i < 64; i++) {
		one_byte = seed[i];
		strBits[0] = nibbleToHex((uint8_t) (one_byte >> 4));
		strBits[1] = nibbleToHex(one_byte);
		strBits[2] = '\0';
		strcat(hexSeed, strBits);
	}

	HDNode node, node2, node3;
	char str[112];
	int r;


	// [seed]
	hdnode_from_seed(fromhex(hexSeed), 64, &node);


	// [Chain m]
	hdnode_serialize_private(&node, str);
	r = hdnode_deserialize(str, &node2);

	hdnode_serialize_public(&node, str);
	r = hdnode_deserialize(str, &node2);

	memcpy(&node3, &node, sizeof(HDNode));
	memset(&node3.private_key, 0, 32);


	// [Chain m/44']
	r = hdnode_private_ckd_prime(&node, 44);

	hdnode_serialize_private(&node, str);
	r = hdnode_deserialize(str, &node2);

	hdnode_serialize_public(&node, str);
	r = hdnode_deserialize(str, &node2);

	memcpy(&node3, &node, sizeof(HDNode));
	memset(&node3.private_key, 0, 32);

	// [Chain m/44'/0']
	r = hdnode_private_ckd_prime(&node, 0);

	hdnode_serialize_private(&node, str);
	r = hdnode_deserialize(str, &node2);

	hdnode_serialize_public(&node, str);
	r = hdnode_deserialize(str, &node2);

	memcpy(&node3, &node, sizeof(HDNode));
	memset(&node3.private_key, 0, 32);

	// [Chain m/44'/0'/0']
	r = hdnode_private_ckd_prime(&node, 0);

	hdnode_serialize_private(&node, str);
	r = hdnode_deserialize(str, &node2);

	hdnode_serialize_public(&node, str);	// THIS is the xpub that we have to output for the new schema
	r = hdnode_deserialize(str, &node2);

	memcpy(&node3, &node, sizeof(HDNode));
	memset(&node3.private_key, 0, 32);



	// [Chain m/44'/0'/0'/0]
	r = hdnode_private_ckd(&node, chain_lvl_2); // 0 external, 1 internal

	hdnode_serialize_private(&node, str);
	r = hdnode_deserialize(str, &node2);

	hdnode_serialize_public(&node, str);
	r = hdnode_deserialize(str, &node2);

	memcpy(&node3, &node, sizeof(HDNode));
	memset(&node3.private_key, 0, 32);


	// [Chain m/44'/0'/0'/0/num]
	r = hdnode_private_ckd(&node, chain_lvl_3);

	uint8_t staticPrivate[32] = {};
	memcpy(staticPrivate, &node.private_key, 32);
	swapEndian256(staticPrivate);
	memcpy(out, staticPrivate, 32);

//	setParentPublicKeyFromPrivateKey(out);

	return false; // success
}


bool getXPUBfromNode(BigNum256 out, const uint8_t *seed, const uint32_t num)
{
	uint8_t i;
	uint8_t one_byte; // current byte of seed

	char strBits[3];
	char hexSeed[64] = { };

	strcpy(hexSeed, "");
	for (i = 0; i < 64; i++) {
		one_byte = seed[i];
		strBits[0] = nibbleToHex((uint8_t) (one_byte >> 4));
		strBits[1] = nibbleToHex(one_byte);
		strBits[2] = '\0';
		strcat(hexSeed, strBits);
	}

	HDNode node, node2, node3;
	char str[112];
	int r;


	// [seed]
	hdnode_from_seed(fromhex(hexSeed), 64, &node);


	// [Chain m]
	hdnode_serialize_private(&node, str);
	r = hdnode_deserialize(str, &node2);

	hdnode_serialize_public(&node, str);
	r = hdnode_deserialize(str, &node2);

	memcpy(&node3, &node, sizeof(HDNode));
	memset(&node3.private_key, 0, 32);


	// [Chain m/0']
	r = hdnode_private_ckd_prime(&node, 0);

	hdnode_serialize_private(&node, str);
	r = hdnode_deserialize(str, &node2);

	hdnode_serialize_public(&node, str);	// THIS is the xpub that we have to output for the new schema
	memcpy(out, str, 112);

//
//
//	r = hdnode_deserialize(str, &node2);
//
//	memcpy(&node3, &node, sizeof(HDNode));
//	memset(&node3.private_key, 0, 32);
//
//
//
//	// [Chain m/0'/0]
//	r = hdnode_private_ckd(&node, 0);
//
//	hdnode_serialize_private(&node, str);
//	r = hdnode_deserialize(str, &node2);
//
//	hdnode_serialize_public(&node, str);
//
//	r = hdnode_deserialize(str, &node2);
//
//	memcpy(&node3, &node, sizeof(HDNode));
//	memset(&node3.private_key, 0, 32);
//
//
//	// [Chain m/0'/1/1]
//	r = hdnode_private_ckd(&node, 1);
//
//	uint8_t staticPrivate[32] = {};
//	memcpy(staticPrivate, &node.private_key, 32);
//	swapEndian256(staticPrivate);
//	memcpy(out, staticPrivate, 32);


	return false; // success
}


bool getXPUBfromNodeTrezor(BigNum256 out, const uint8_t *seed, const uint32_t num)
{
	uint8_t i;
	uint8_t one_byte; // current byte of seed

	char strBits[3];
	char hexSeed[64] = { };

	strcpy(hexSeed, "");
	for (i = 0; i < 64; i++) {
		one_byte = seed[i];
		strBits[0] = nibbleToHex((uint8_t) (one_byte >> 4));
		strBits[1] = nibbleToHex(one_byte);
		strBits[2] = '\0';
		strcat(hexSeed, strBits);
	}

	HDNode node, node2, node3;
	char str[112];
	int r;



	hdnode_from_seed(fromhex(hexSeed), 64, &node);

	// [Chain m]
	hdnode_serialize_private(&node, str);
	r = hdnode_deserialize(str, &node2);
	hdnode_serialize_public(&node, str);
	r = hdnode_deserialize(str, &node2);

	memcpy(&node3, &node, sizeof(HDNode));
	memset(&node3.private_key, 0, 32);

//	Serial.println("m/44'");
	r = hdnode_private_ckd_prime(&node, 44);
	hdnode_serialize_private(&node, str);
	r = hdnode_deserialize(str, &node2);
	hdnode_serialize_public(&node, str);
	r = hdnode_deserialize(str, &node2);
	memcpy(&node3, &node, sizeof(HDNode));
	memset(&node3.private_key, 0, 32);

//	Serial.println("m/44'/0'");
	r = hdnode_private_ckd_prime(&node, 0);
	hdnode_serialize_private(&node, str);
	r = hdnode_deserialize(str, &node2);
	hdnode_serialize_public(&node, str);
	r = hdnode_deserialize(str, &node2);
	memcpy(&node3, &node, sizeof(HDNode));
	memset(&node3.private_key, 0, 32);

//	Serial.println("m/44'/0'/0'");
	r = hdnode_private_ckd_prime(&node, 0);
	hdnode_serialize_private(&node, str);
	r = hdnode_deserialize(str, &node2);
	hdnode_serialize_public(&node, str);
	r = hdnode_deserialize(str, &node2);
	memcpy(&node3, &node, sizeof(HDNode));
	memset(&node3.private_key, 0, 32);

	memcpy(out, str, 112);

	return false; // success
}

bool generateDeterministic256_original(BigNum256 out, const uint8_t *seed, const uint32_t num)
{
	BigNum256 i_l;
	uint8_t k_par[32];
	uint8_t hash[SHA512_HASH_LENGTH];
	uint8_t hmac_message[69]; // 04 (1 byte) + x (32 bytes) + y (32 bytes) + num (4 bytes)

	setFieldToN();
	memcpy(k_par, seed, 32);
	swapEndian256(k_par); // since seed is big-endian
	bigModulo(k_par, k_par); // just in case
	// k_par cannot be 0. If it is zero, then the output of this generator
	// will always be 0.
	if (bigIsZero(k_par))
	{
		return true; // invalid seed
	}
	if (!cached_parent_public_key_valid)
	{
		setParentPublicKeyFromPrivateKey(k_par);
	}
	// BIP 0032 specifies that the public key should be represented in a way
	// that is compatible with "SEC 1: Elliptic Curve Cryptography" by
	// Certicom research, obtained 15-August-2011 from:
	// http://www.secg.org/collateral/sec1_final.pdf section 2.3 ("Data Types
	// and Conversions"). The gist of it is: 0x04, followed by x, then y in
	// big-endian format.
	hmac_message[0] = 0x04;
	memcpy(&(hmac_message[1]), cached_parent_public_key.x, 32);
	swapEndian256(&(hmac_message[1]));
	memcpy(&(hmac_message[33]), cached_parent_public_key.y, 32);
	swapEndian256(&(hmac_message[33]));
	writeU32BigEndian(&(hmac_message[65]), num);
	hmacSha512(hash, &(seed[32]), 32, hmac_message, sizeof(hmac_message));

	setFieldToN();
	i_l = (BigNum256)hash;
	swapEndian256(i_l); // since hash is big-endian
	bigModulo(i_l, i_l); // just in case
	bigMultiply(out, i_l, k_par);

#ifdef TEST_PRANDOM
	memcpy(test_chain_code, &(hash[32]), sizeof(test_chain_code));
#endif // #ifdef TEST_PRANDOM
//	BigNum256 temp;
//	memcpy(&temp, &out, 32);
//	swapEndian256(temp);
//	passToPrint(tohex(temp, 32));
	return false; // success
}


