/** \file wallet.c
  *
  * \brief Manages the storage and generation of Bitcoin addresses.
  *
  * Addresses are stored in wallets, which can be
  * "loaded" or "unloaded". A loaded wallet can have operations (eg. new
  * address) performed on it, whereas an unloaded wallet can only sit dormant.
  * Addresses aren't actually physically stored in non-volatile storage;
  * rather a seed for a deterministic private key generation algorithm is
  * stored and private keys are generated when they are needed. This means
  * that obtaining an address is a slow operation (requiring a point
  * multiply), so the host should try to remember all public keys and
  * addresses. The advantage of not storing addresses is that very little
  * non-volatile storage space is needed per wallet.
  *
  * Wallets can be encrypted or unencrypted. Actually, technically, all
  * wallets are encrypted. However, wallets marked as "unencrypted" are
  * encrypted using an encryption key consisting of all zeroes. This purely
  * semantic definition was done to avoid having to insert special cases
  * everytime encrypted storage needed to be accessed.
  *
  * This file is licensed as described by the file LICENCE.
  */

#ifdef TEST
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#endif // #ifdef TEST

#ifdef TEST_WALLET
#include "test_helpers.h"
#endif // #ifdef TEST_WALLET

#include <stddef.h>
#include "common.h"
#include "endian.h"
#include "wallet.h"
#include "prandom.h"
#include "sha256.h"
#include "ripemd160.h"
#include "ecdsa.h"
#include "hwinterface.h"
#include "xex.h"
#include "bignum256.h"
#include "storage_common.h"
#include "hmac_sha512.h"
#include "pbkdf2.h"
#include "arm/keypad_alpha.h"
#include "stream_comm.h"
#include "messages.pb.h"
/** Length of the checksum field of a wallet record. This is 32 since SHA-256
  * is used to calculate the checksum and the output of SHA-256 is 32 bytes
  * long. */
#define CHECKSUM_LENGTH			32


/** Structure of the unencrypted portion of a wallet record. */
struct WalletRecordUnencryptedStruct
{
	/** Wallet version. Should be one of #WalletVersion. */
	uint32_t version;
	/** Name of the wallet. This is purely for the sake of the host; the
	  * name isn't ever used or parsed by the functions in this file. */
	uint8_t name[NAME_LENGTH];
	/** Wallet universal unique identifier (UUID). One way for the host to
	  * identify a wallet. */
	uint8_t uuid[DEVICE_UUID_LENGTH];
	/** Reserved for future use. Set to all zeroes. */
	uint8_t reserved[4];
};

/** Structure of the encrypted portion of a wallet record. */
struct WalletRecordEncryptedStruct
{
	/** Number of addresses in this wallet. */
	uint32_t num_addresses;
	/** Random padding. This is random to try and thwart known-plaintext
	  * attacks. */
	uint8_t padding[8];
	/** Reserved for future use. Set to all zeroes. */
	uint8_t reserved[3];
	/** Contains hash of user-set transaction pin. */
	uint8_t transaction_pin[32];
	/** Has/does not have transaction pin. */
	bool transaction_pin_used;
	/** Seed for deterministic private key generator. */
	uint8_t seed[SEED_LENGTH];
	/** SHA-256 of everything except this. */
	uint8_t checksum[CHECKSUM_LENGTH];
};

/** Structure of a wallet record. */
typedef struct WalletRecordStruct
{
	/** Unencrypted portion. See #WalletRecordUnencryptedStruct for fields.
	  * \warning readWalletRecord() and writeCurrentWalletRecord() both assume
	  *          that this occurs before the encrypted portion.
	  */
	struct WalletRecordUnencryptedStruct unencrypted;
	/** Encrypted portion. See #WalletRecordEncryptedStruct for fields. */
	struct WalletRecordEncryptedStruct encrypted;
} WalletRecord;

/** The most recent error to occur in a function in this file,
  * or #WALLET_NO_ERROR if no error occurred in the most recent function
  * call. See #WalletErrorsEnum for possible values. */
static WalletErrors last_error;
/** This will be false if a wallet is not currently loaded. This will be true
  * if a wallet is currently loaded. */
static bool wallet_loaded;
/** Whether the currently loaded wallet is a hidden wallet. If
  * #wallet_loaded is false (i.e. no wallet is loaded), then the meaning of
  * this variable is undefined. */
static bool is_hidden_wallet;
/** This will only be valid if a wallet is loaded. It contains a cache of the
  * currently loaded wallet record. If #wallet_loaded is false (i.e. no wallet
  * is loaded), then the contents of this variable are undefined. */
static WalletRecord current_wallet;
/** The address in non-volatile memory where the currently loaded wallet
  * record is. If #wallet_loaded is false (i.e. no wallet is loaded), then the
  * contents of this variable are undefined. */
static uint32_t wallet_nv_address;
/** Cache of number of wallets that can fit in non-volatile storage. This will
  * be 0 if a value hasn't been calculated yet. This is set by
  * getNumberOfWallets(). */
static uint32_t num_wallets;

#ifdef TEST
/** The file to perform test non-volatile I/O on. */
FILE *wallet_test_file;
#endif // #ifdef TEST

/** Find out what the most recent error which occurred in any wallet function
  * was. If no error occurred in the most recent wallet function that was
  * called, this will return #WALLET_NO_ERROR.
  * \return See #WalletErrorsEnum for possible values.
  */
WalletErrors walletGetLastError(void)
{
	return last_error;
}

#ifdef TEST

void initWalletTest(void)
{
	wallet_test_file = fopen("wallet_test.bin", "w+b");
	if (wallet_test_file == NULL)
	{
		printf("Could not open \"wallet_test.bin\" for writing\n");
		exit(1);
	}
}

#endif // #ifdef TEST

#ifdef TEST_WALLET
/** Maximum of addresses which can be stored in storage area - for testing
  * only. This should actually be the capacity of the wallet, since one
  * of the tests is to see what happens when the wallet is full. */
#define MAX_TESTING_ADDRESSES	7

/** Set this to true to stop sanitiseNonVolatileStorage() from
  * updating the persistent entropy pool. This is necessary for some test
  * cases which check where sanitiseNonVolatileStorage() writes; updates
  * of the entropy pool would appear as spurious writes to those test cases.
  */
static bool suppress_set_entropy_pool;
#endif // #ifdef TEST_WALLET

/** Calculate the checksum (SHA-256 hash) of the current wallet's contents.
  * \param hash The resulting SHA-256 hash will be written here. This must
  *             be a byte array with space for #CHECKSUM_LENGTH bytes.
  * \return See #NonVolatileReturnEnum.
  */
static void calculateWalletChecksum(uint8_t *hash)
{
	uint8_t *ptr;
	unsigned int i;
	HashState hs;

	sha256Begin(&hs);
	ptr = (uint8_t *)&current_wallet;
	for (i = 0; i < sizeof(WalletRecord); i++)
	{
		// Skip checksum when calculating the checksum.
		if (i == offsetof(WalletRecord, encrypted.checksum))
		{
			i += sizeof(current_wallet.encrypted.checksum);
		}
		if (i < sizeof(WalletRecord))
		{
			sha256WriteByte(&hs, ptr[i]);
		}
	}
	sha256Finish(&hs);
	writeHashToByteArray(hash, &hs, true);
}

/** Load contents of non-volatile memory into a #WalletRecord structure. This
  * doesn't care if there is or isn't actually a wallet at the specified
  * address.
  * \param wallet_record Where to load the wallet record into.
  * \param address The address in non-volatile memory to read from.
  * \return See #WalletErrors.
  */
static WalletErrors readWalletRecord(WalletRecord *wallet_record, uint32_t address)
{
	uint32_t unencrypted_size;
	uint32_t encrypted_size;

	unencrypted_size = sizeof(wallet_record->unencrypted);
	encrypted_size = sizeof(wallet_record->encrypted);
	// Before doing any reading, do some sanity checks. These ensure that the
	// size of the unencrypted and encrypted portions are an integer multiple
	// of the AES block size.
	if (((unencrypted_size % 16) != 0) || ((encrypted_size % 16) != 0))
	{
		return WALLET_INVALID_OPERATION;
	}

	if (nonVolatileRead((uint8_t *)&(wallet_record->unencrypted), address + offsetof(WalletRecord, unencrypted), unencrypted_size) != NV_NO_ERROR)
	{
		return WALLET_READ_ERROR;
	}
	if (encryptedNonVolatileRead((uint8_t *)&(wallet_record->encrypted), address + offsetof(WalletRecord, encrypted), encrypted_size) != NV_NO_ERROR)
	{
		return WALLET_READ_ERROR;
	}
	return WALLET_NO_ERROR;
}

/** Store contents of #current_wallet into non-volatile memory. This will also
  * call nonVolatileFlush(), since that's usually what's wanted anyway.
  * \param address The address in non-volatile memory to write to.
  * \return See #WalletErrors.
  */
static WalletErrors writeCurrentWalletRecord(uint32_t address)
{
	if (nonVolatileWrite((uint8_t *)&(current_wallet.unencrypted), address, sizeof(current_wallet.unencrypted)) != NV_NO_ERROR)
	{
		return WALLET_WRITE_ERROR;
	}
	if (encryptedNonVolatileWrite((uint8_t *)&(current_wallet.encrypted), address + sizeof(current_wallet.unencrypted), sizeof(current_wallet.encrypted)) != NV_NO_ERROR)
	{
		return WALLET_WRITE_ERROR;
	}
	if (nonVolatileFlush() != NV_NO_ERROR)
	{
		return WALLET_WRITE_ERROR;
	}
	return WALLET_NO_ERROR;
}

/** Using the specified password and UUID (as the salt), derive an encryption
  * key and begin using it.
  *
  * This needs to be in wallet.c because there are situations (creating and
  * restoring a wallet) when the wallet UUID is not known before the beginning
  * of the appropriate function call.
  * \param uuid Byte array containing the wallet UUID. This must be
  *             exactly #DEVICE_UUID_LENGTH bytes long.
  * \param password Password to use in key derivation.
  * \param password_length Length of password, in bytes. Use 0 to specify no
  *                        password (i.e. wallet is unencrypted).
  */
static void deriveAndSetEncryptionKey(const uint8_t *uuid, const uint8_t *password, const unsigned int password_length)
{
	uint8_t derived_key[SHA512_HASH_LENGTH];

	if (sizeof(derived_key) < WALLET_ENCRYPTION_KEY_LENGTH)
	{
		fatalError(); // this should never happen
	}
	if (password_length > 0)
	{
		pbkdf2(derived_key, password, password_length, uuid, DEVICE_UUID_LENGTH);
		setEncryptionKey(derived_key);
	}
	else
	{
		// No password i.e. wallet is unencrypted.
		memset(derived_key, 0, sizeof(derived_key));
		setEncryptionKey(derived_key);
	}
}

/** Initialise a wallet (load it if it's there).
  * \param wallet_spec The wallet number of the wallet to load.
  * \param password Password to use to derive wallet encryption key.
  * \param password_length Length of password, in bytes. Use 0 to specify no
  *                        password (i.e. wallet is unencrypted).
  * \return #WALLET_NO_ERROR on success, or one of #WalletErrorsEnum if an
  *         error occurred.
  */
WalletErrors initWallet(uint32_t wallet_spec, const uint8_t *password, const unsigned int password_length)
{
	WalletErrors r;
	uint8_t hash[CHECKSUM_LENGTH];
	uint8_t uuid[DEVICE_UUID_LENGTH];

	if (uninitWallet() != WALLET_NO_ERROR)
	{
		return last_error; // propagate error code
	}

	if (getNumberOfWallets() == 0)
	{
		return last_error; // propagate error code
	}
	if (wallet_spec >= num_wallets)
	{
		last_error = WALLET_INVALID_WALLET_NUM;
		return last_error;
	}
	wallet_nv_address = WALLET_START_ADDRESS + wallet_spec * sizeof(WalletRecord);

	if (nonVolatileRead(uuid, wallet_nv_address + offsetof(WalletRecord, unencrypted.uuid), DEVICE_UUID_LENGTH) != NV_NO_ERROR)
	{
		last_error = WALLET_READ_ERROR;
		return last_error;
	}
	deriveAndSetEncryptionKey(uuid, password, password_length);

	r = readWalletRecord(&current_wallet, wallet_nv_address);
	if (r != WALLET_NO_ERROR)
	{
		last_error = r;
		return last_error;
	}

	if (current_wallet.unencrypted.version == VERSION_NOTHING_THERE)
	{
		is_hidden_wallet = true;
	}
	else if ((current_wallet.unencrypted.version == VERSION_UNENCRYPTED)
		|| (current_wallet.unencrypted.version == VERSION_IS_ENCRYPTED))
	{
		is_hidden_wallet = false;
	}
	else
	{
		last_error = WALLET_NOT_THERE;
		return last_error;
	}

	// Calculate checksum and check that it matches.
	calculateWalletChecksum(hash);
	if (bigCompareVariableSize(current_wallet.encrypted.checksum, hash, CHECKSUM_LENGTH) != BIGCMP_EQUAL)
	{
		last_error = WALLET_NOT_THERE;
		return last_error;
	}

	if (current_wallet.encrypted.transaction_pin != 0)
	{
		;
	}


	wallet_loaded = true;
	last_error = WALLET_NO_ERROR;
	return last_error;
}

uint32_t readEntireWalletSpace(uint8_t *bulkDataOut)
{
	uint32_t wallet_space_size = 0;
	uint32_t rest_offset = 0;

	if (uninitWallet() != WALLET_NO_ERROR)
	{
		writeEinkDisplay(">unload error", false, 10, 10, "",false,0,0, "",false,0,0, "",false,0,0, "",false,0,0);
	}

	if (getNumberOfWallets() == 0)
	{
		writeEinkDisplay(">no wallets error", false, 10, 10, "",false,0,0, "",false,0,0, "",false,0,0, "",false,0,0);
	}

//	writeEinkDisplayNumberSingleBig(sizeof(WalletRecord),5,40);


//	rest_offset = ENTROPY_POOL_LENGTH + POOL_CHECKSUM_LENGTH + DEVICE_UUID_LENGTH + DEVICE_LANG_LENGTH + DEVICE_LANG_SET_LENGTH + DEVICE_COMMS_SET_LENGTH + IS_FORMATTED_LENGTH + PIN_LENGTH + HAS_PIN_LENGTH + SETUP_TYPE_LENGTH + WRONG_TRANSACTION_PIN_COUNT_LENGTH + WRONG_DEVICE_PIN_COUNT_LENGTH ;
	rest_offset = ENTROPY_POOL_LENGTH + POOL_CHECKSUM_LENGTH + DEVICE_UUID_LENGTH + DEVICE_LANG_LENGTH + DEVICE_LANG_SET_LENGTH + DEVICE_COMMS_SET_LENGTH + IS_FORMATTED_LENGTH + PIN_LENGTH + HAS_PIN_LENGTH + SETUP_TYPE_LENGTH + WRONG_TRANSACTION_PIN_COUNT_LENGTH + WRONG_DEVICE_PIN_COUNT_LENGTH + AEM_USE_LENGTH + AEM_PHRASE_LENGTH;
//	writeEinkDisplayNumberSingleBig(num_wallets,5,40);

	wallet_space_size = (num_wallets * sizeof(WalletRecord)) + rest_offset;
//	writeEinkDisplayNumberSingleBig(wallet_space_size,5,40);


	writeEinkDisplay(">wallet data reading", false, 10, 10, "",false,0,0, "",false,0,0, "",false,0,0, "",false,0,0);

	if (nonVolatileRead(bulkDataOut, ENTROPY_POOL_ADDRESS, wallet_space_size) != NV_NO_ERROR)
	{
		last_error = WALLET_READ_ERROR;
		writeEinkDisplay(">read error", false, 10, 10, "",false,0,0, "",false,0,0, "",false,0,0, "",false,0,0);
	}

	writeEinkDisplay(">wallet data read...", false, 10, 10, "",false,0,0, "",false,0,0, "",false,0,0, "",false,0,0);
	return wallet_space_size;
}

uint32_t writeEntireWalletSpace(uint8_t *bulkDataIn)
{
	uint8_t encrypty[16]={1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1};
	decryptStream(bulkDataIn, encrypty);

	uint32_t wallet_space_size = 0;
	uint32_t rest_offset = 0;

	if (uninitWallet() != WALLET_NO_ERROR)
	{
		writeEinkDisplay(">unload error", false, 10, 10, "",false,0,0, "",false,0,0, "",false,0,0, "",false,0,0);
	}

	if (getNumberOfWallets() == 0)
	{
		writeEinkDisplay(">no wallets error", false, 10, 10, "",false,0,0, "",false,0,0, "",false,0,0, "",false,0,0);
	}

//	rest_offset = ENTROPY_POOL_LENGTH + POOL_CHECKSUM_LENGTH + DEVICE_UUID_LENGTH + DEVICE_LANG_LENGTH + DEVICE_LANG_SET_LENGTH + DEVICE_COMMS_SET_LENGTH + IS_FORMATTED_LENGTH + PIN_LENGTH + HAS_PIN_LENGTH + SETUP_TYPE_LENGTH + WRONG_TRANSACTION_PIN_COUNT_LENGTH + WRONG_DEVICE_PIN_COUNT_LENGTH ;
	rest_offset = ENTROPY_POOL_LENGTH + POOL_CHECKSUM_LENGTH + DEVICE_UUID_LENGTH + DEVICE_LANG_LENGTH + DEVICE_LANG_SET_LENGTH + DEVICE_COMMS_SET_LENGTH + IS_FORMATTED_LENGTH + PIN_LENGTH + HAS_PIN_LENGTH + SETUP_TYPE_LENGTH + WRONG_TRANSACTION_PIN_COUNT_LENGTH + WRONG_DEVICE_PIN_COUNT_LENGTH + AEM_USE_LENGTH + AEM_PHRASE_LENGTH;

	wallet_space_size = (num_wallets * sizeof(WalletRecord)) + rest_offset;
//	writeEinkDisplayNumberSingleBig(wallet_space_size,5,40);


	writeEinkDisplay(">wallet data writing", false, 10, 10, "",false,0,0, "",false,0,0, "",false,0,0, "",false,0,0);

	if (nonVolatileWrite(bulkDataIn, ENTROPY_POOL_ADDRESS, wallet_space_size) != NV_NO_ERROR)
	{
		last_error = WALLET_WRITE_ERROR;
		writeEinkDisplay(">write error", false, 10, 10, "",false,0,0, "",false,0,0, "",false,0,0, "",false,0,0);
	}

	writeEinkDisplay(">wallet data set", false, 10, 10, "",false,10,25, "RESTARTING",false,10,40, "",false,0,0, "",false,0,0);
	Software_Reset();
	return wallet_space_size;
}






bool hasTransactionPin(void)
{
	if (current_wallet.encrypted.transaction_pin_used)
	{
		return true;
	}else
	{
		return false;
	}
}

/** Unload wallet, so that it cannot be used until initWallet() is called.
  * \return #WALLET_NO_ERROR on success, or one of #WalletErrorsEnum if an
  *         error occurred.
  */
WalletErrors uninitWallet(void)
{
	clearParentPublicKeyCache();
	wallet_loaded = false;
	is_hidden_wallet = false;
	wallet_nv_address = 0;
	memset(&current_wallet, 0, sizeof(WalletRecord));
	last_error = WALLET_NO_ERROR;
	return last_error;
}

#ifdef TEST_WALLET
void logVersionFieldWrite(uint32_t address);
#endif // #ifdef TEST_WALLET

/** Sanitise (clear) a selected area of non-volatile storage. This will clear
  * the area between start (inclusive) and end (exclusive).
  * \param start The first address which will be cleared.
  * \param end One byte past the last address which will be cleared.
  * \return #WALLET_NO_ERROR on success, or one of #WalletErrorsEnum if an
  *         error occurred. This will still return #WALLET_NO_ERROR even if
  *         end is an address beyond the end of the non-volatile storage area.
  *         This is done so that using start = 0 and end = 0xffffffff will
  *         clear the entire non-volatile storage area.
  */
WalletErrors sanitiseNonVolatileStorage(uint32_t start, uint32_t end, bool quiet, int lang)
{
	uint8_t buffer[32];
	uint8_t pool_state[ENTROPY_POOL_LENGTH];
	uint32_t address;
	uint32_t remaining;
	NonVolatileReturn r;
	uint8_t pass;

	if (getEntropyPool(pool_state))
	{
		last_error = WALLET_RNG_FAILURE;
		return last_error;
	}

	// 4 pass format: all 0s, all 1s, random, random. This ensures that
	// every bit is cleared at least once, set at least once and ends up
	// in an unpredictable state.
	// It is crucial that the last pass is random for two reasons:
	// 1. A new device UUID is written, if necessary.
	// 2. Hidden wallets are actually plausibly deniable.
	for (pass = 0; pass < 4; pass++)
	{
		if(!quiet)
		{
			char passPrt[16];
			sprintf(passPrt,"%lu", pass*25);
//			writeEinkDisplay("pre bINAPD", false, COL_1_X, LINE_1_Y, "",false,5,30, "",false,5,50, "",false,5,70, "",false,0,0);
			buttonInterjectionNoAckPlusData(ASKUSER_FORMAT_WITH_PROGRESS, passPrt, lang);
//			writeEinkDisplay("post bINAPD", false, COL_1_X, LINE_1_Y, "",false,5,30, "",false,5,50, "",false,5,70, "",false,0,0);

//			initDisplay();
//			overlayBatteryStatus(BATT_VALUE_DISPLAY);
//			writeEinkNoDisplay("INITIAL SETUP",  COL_1_X, LINE_1_Y, "FORMATTING",COL_1_X,LINE_2_Y, "",COL_1_X,LINE_3_Y, "",23,LINE_3_Y, "",25,80);
//			writeUnderline(STRIPE_X_START, STRIPE_Y_START, STRIPE_X_END, STRIPE_Y_END);
//			writeEinkNoDisplaySingleBig(passPrt,COL_1_X,LINE_3_Y);
//			writeEinkNoDisplaySingleBig("%",39,LINE_3_Y);
//			display();
		}

		address = start;
		while (address < end)
		{
			if (pass == 0)
			{
				memset(buffer, 0, sizeof(buffer));
			}
			else if (pass == 1)
			{
				memset(buffer, 0xff, sizeof(buffer));
			}
			else
			{
				if (getRandom256TemporaryPool(buffer, pool_state))
				{
					// Before returning, attempt to write the persistent
					// entropy pool state back into non-volatile memory.
					// The return value of setEntropyPool() is ignored because
					// if a failure occurs, then WALLET_RNG_FAILURE is a
					// suitable return value anyway.
#ifdef TEST_WALLET
					if (!suppress_set_entropy_pool)
#endif // #ifdef TEST_WALLET
					{
						setEntropyPool(pool_state);
					}
					last_error = WALLET_RNG_FAILURE;
					return last_error;
				}
			}
			remaining = end - address;
			if (remaining > 32)
			{
				remaining = 32;
			}
			if (remaining > 0)
			{
				r = nonVolatileWrite(buffer, address, remaining);
				if (r == NV_INVALID_ADDRESS)
				{
					// Got to end of non-volatile memory.
					break;
				}
				else if (r != NV_NO_ERROR)
				{
					last_error = WALLET_WRITE_ERROR;
					return last_error;
				}
			}
			if (address <= (0xffffffff - 32))
			{
				address += 32;
			}
			else
			{
				// Overflow in address will occur.
				break;
			}
		} // end while (address < end)

		// After each pass, flush write buffers to ensure that
		// non-volatile memory is actually overwritten.
		r = nonVolatileFlush();
		if (r != NV_NO_ERROR)
		{
			last_error = WALLET_WRITE_ERROR;
			return last_error;
		}
	} // end for (pass = 0; pass < 4; pass++)

#ifdef TEST_WALLET
	if (!suppress_set_entropy_pool)
#endif // #ifdef TEST_WALLET
	{
		// Write back persistent entropy pool state.
		if (setEntropyPool(pool_state))
		{
			last_error = WALLET_RNG_FAILURE;
			return last_error;
		}
	}

	// If the selected area includes the device UUID, then a new device
	// UUID needs to be written. But if the selected area includes the
	// device UUID, then it will be overwritten with random data in the
	// above loop. Thus no additional work is needed.

	// Write VERSION_NOTHING_THERE to all possible locations of the
	// version field. This ensures that a wallet won't accidentally
	// (1 in 2 ^ 31 chance) be recognized as a valid wallet by
	// getWalletInfo().
	if (start < WALLET_START_ADDRESS)
	{
		address = 0;
	}
	else
	{
		address = start - WALLET_START_ADDRESS;
	}
	address /= sizeof(WalletRecord);
	address *= sizeof(WalletRecord);
	address += (WALLET_START_ADDRESS + offsetof(WalletRecord, unencrypted.version));
	// address is now rounded down to the first possible address where
	// the version field of a wallet could be stored.
	memset(buffer, 0, sizeof(uint32_t));
	// The "address <= (0xffffffff - sizeof(uint32_t))" is there to ensure that
	// (address + sizeof(uint32_t)) cannot overflow.
	while ((address <= (0xffffffff - sizeof(uint32_t))) && ((address + sizeof(uint32_t)) <= end))
	{
		// An additional range check against start is needed because the
		// initial value of address is rounded down; thus it could be
		// rounded down below start.
		if (address >= start)
		{
			r = nonVolatileWrite(buffer, address, sizeof(uint32_t));
			if (r == NV_NO_ERROR)
			{
				r = nonVolatileFlush();
			}
			if (r == NV_INVALID_ADDRESS)
			{
				// Got to end of non-volatile memory.
				break;
			}
			else if (r != NV_NO_ERROR)
			{
				last_error = WALLET_WRITE_ERROR;
				return last_error;
			}
#ifdef TEST_WALLET
			if (r == NV_NO_ERROR)
			{
				logVersionFieldWrite(address);
			}
#endif // #ifdef TEST_WALLET
		}
		if (address <= (0xffffffff - sizeof(WalletRecord)))
		{
			address += sizeof(WalletRecord);
		}
		else
		{
			// Overflow in address will occur.
			break;
		}
	} // end while ((address <= (0xffffffff - sizeof(uint32_t))) && ((address + sizeof(uint32_t)) <= end))

	last_error = WALLET_NO_ERROR;
	return last_error;
}

/** Computes wallet version of current wallet. This is in its own function
  * because it's used by both newWallet() and changeEncryptionKey().
  * \return See #WalletErrors.
  */
static WalletErrors updateWalletVersion(void)
{
	if (is_hidden_wallet)
	{
		// Hidden wallets should not ever have their version fields updated;
		// that would give away their presence.
		return WALLET_INVALID_OPERATION;
	}
	if (isEncryptionKeyNonZero())
	{
		current_wallet.unencrypted.version = VERSION_IS_ENCRYPTED;
	}
	else
	{
		current_wallet.unencrypted.version = VERSION_UNENCRYPTED;
	}
	return WALLET_NO_ERROR;
}

/** Delete a wallet, so that it's contents can no longer be retrieved from
  * non-volatile storage.
  * \param wallet_spec The wallet number of the wallet to delete. The wallet
  *                    doesn't have to "exist"; calling this function for a
  *                    non-existent wallet will clear the non-volatile space
  *                    associated with it. This is useful for deleting a
  *                    hidden wallet.
  * \warning This is irreversible; the only way to access the wallet after
  *          deletion is to restore a backup.
  */
WalletErrors deleteWallet(uint32_t wallet_spec)
{
	uint32_t address;

	if (getNumberOfWallets() == 0)
	{
		return last_error; // propagate error code
	}
	if (wallet_spec >= num_wallets)
	{
		last_error = WALLET_INVALID_WALLET_NUM;
		return last_error;
	}
	// Always unload current wallet, just in case the current wallet is the
	// one being deleted.
	if (uninitWallet() != WALLET_NO_ERROR)
	{
		return last_error; // propagate error code
	}
	address = WALLET_START_ADDRESS + wallet_spec * sizeof(WalletRecord);
	last_error = sanitiseNonVolatileStorage(address, address + sizeof(WalletRecord), true, 0);
	return last_error;
}

/** Create new wallet. A brand new wallet contains no addresses and should
  * have a unique, unpredictable deterministic private key generation seed.
  * \param wallet_spec The wallet number of the new wallet.
  * \param name Should point to #NAME_LENGTH bytes (padded with spaces if
  *             necessary) containing the desired name of the wallet.
  * \param use_seed If this is true, then the contents of seed will be
  *                 used as the deterministic private key generation seed.
  *                 If this is false, then the contents of seed will be
  *                 ignored.
  * \param seed The deterministic private key generation seed to use in the
  *             new wallet. This should be a byte array of length #SEED_LENGTH
  *             bytes. This parameter will be ignored if use_seed is false.
  * \param make_hidden Whether to make the new wallet a hidden wallet.
  * \param password Password to use to derive wallet encryption key.
  * \param password_length Length of password, in bytes. Use 0 to specify no
  *                        password (i.e. wallet is unencrypted).
  * \return #WALLET_NO_ERROR on success, or one of #WalletErrorsEnum if an
  *         error occurred. If this returns #WALLET_NO_ERROR, then the
  *         wallet will also be loaded.
  * \warning This will erase the current one.
  */
WalletErrors newWallet(uint32_t wallet_spec, uint8_t *name, bool use_seed, uint8_t *seed, bool make_hidden, const uint8_t *password, const unsigned int password_length, const uint8_t *transaction_pin_hash, bool transaction_pin_used_param)
{


	uint8_t random_buffer[32];
	uint8_t uuid[DEVICE_UUID_LENGTH];
	WalletErrors r;

//	writeEinkDisplay("In newWallet", false, COL_1_X, LINE_1_Y, "",false,5,30, "",false,5,50, "",false,5,70, "",false,0,0);
//	displayBigHexStream(transaction_pin_hash, 32);
//	waitForButtonPress();

	if (uninitWallet() != WALLET_NO_ERROR)
	{
		return last_error; // propagate error code
	}

	if (getNumberOfWallets() == 0)
	{
		return last_error; // propagate error code
	}
	if (wallet_spec >= num_wallets)
	{
		last_error = WALLET_INVALID_WALLET_NUM;
		return last_error;
	}
	wallet_nv_address = WALLET_START_ADDRESS + wallet_spec * sizeof(WalletRecord);

	// Check for existing wallet.
	r = readWalletRecord(&current_wallet, wallet_nv_address);
	if (r != WALLET_NO_ERROR)
	{
		last_error = r;
		return last_error;
	}
	if (current_wallet.unencrypted.version != VERSION_NOTHING_THERE)
	{
		last_error = WALLET_ALREADY_EXISTS;
		return last_error;
	}

	if (make_hidden)
	{
		// The creation of a hidden wallet is supposed to be discreet, so
		// all unencrypted fields should be left untouched. This forces us to
		// use the existing UUID.
		memcpy(uuid, current_wallet.unencrypted.uuid, DEVICE_UUID_LENGTH);
	}
	else
	{
		// Generate wallet UUID now, because it is needed to derive the wallet
		// encryption key.
		if (getRandom256(random_buffer))
		{
			last_error = WALLET_RNG_FAILURE;
			return last_error;
		}
		memcpy(uuid, random_buffer, DEVICE_UUID_LENGTH);
	}
	deriveAndSetEncryptionKey(uuid, password, password_length);

	// Update unencrypted fields of current_wallet.
	if (!make_hidden)
	{
		r = updateWalletVersion();
		if (r != WALLET_NO_ERROR)
		{
			last_error = r;
			return last_error;
		}
		memset(current_wallet.unencrypted.reserved, 0, sizeof(current_wallet.unencrypted.reserved));
		memcpy(current_wallet.unencrypted.name, name, NAME_LENGTH);
		memcpy(current_wallet.unencrypted.uuid, uuid, DEVICE_UUID_LENGTH);
	}

	// Update encrypted fields of current_wallet.
	current_wallet.encrypted.num_addresses = 0;
	if (getRandom256(random_buffer))
	{
		last_error = WALLET_RNG_FAILURE;
		return last_error;
	}
	memcpy(current_wallet.encrypted.padding, random_buffer, sizeof(current_wallet.encrypted.padding));

	if(transaction_pin_used_param)
	{
//		writeEinkDisplay("In if(transpin)", false, COL_1_X, LINE_1_Y, "",false,5,30, "",false,5,50, "",false,5,70, "",false,0,0);
//		displayBigHexStream(transaction_pin_hash, 32);
//		waitForButtonPress();

		current_wallet.encrypted.transaction_pin_used = true;
		memcpy(current_wallet.encrypted.transaction_pin, transaction_pin_hash, 32);

	}else
	{
		current_wallet.encrypted.transaction_pin_used = false;
		memcpy(current_wallet.encrypted.transaction_pin, 0, 32);
	}

//	writeEinkDisplay("after if(transpin)", false, COL_1_X, LINE_1_Y, "",false,5,30, "",false,5,50, "",false,5,70, "",false,0,0);
//	displayBigHexStream(current_wallet.encrypted.transaction_pin, 32);
//	waitForButtonPress();


//	memset(current_wallet.encrypted.transaction_pin, transaction_pin_hash, sizeof(current_wallet.encrypted.transaction_pin));

	if (use_seed)
	{
		memcpy(current_wallet.encrypted.seed, seed, SEED_LENGTH);
	}
	else
	{
		if (getRandom256(random_buffer))
		{
			last_error = WALLET_RNG_FAILURE;
			return last_error;
		}
		memcpy(current_wallet.encrypted.seed, random_buffer, 32);
		if (getRandom256(random_buffer))
		{
			last_error = WALLET_RNG_FAILURE;
			return last_error;
		}
		memcpy(&(current_wallet.encrypted.seed[32]), random_buffer, 32);
	}
	calculateWalletChecksum(current_wallet.encrypted.checksum);

	r = writeCurrentWalletRecord(wallet_nv_address);
	if (r != WALLET_NO_ERROR)
	{
		last_error = r;
		return last_error;
	}

	last_error = initWallet(wallet_spec, password, password_length);
	return last_error;
}

bool checkTransactionPIN(void)
{

	return true;
}

static uint8_t transactionPINHashArray[32]={};
static uint8_t *transactionPINHashPointer;





uint8_t *getTransactionPINhash(void)
{

	transactionPINHashPointer = &transactionPINHashArray[0];
	memset(transactionPINHashPointer, 0, 32);

	memcpy(transactionPINHashPointer, current_wallet.encrypted.transaction_pin, 32);

//	writeEinkDisplay("fetching...cw.e.tp", false, COL_1_X, LINE_1_Y, "",false,5,30, "",false,5,50, "",false,5,70, "",false,0,0);
//	displayBigHexStream(current_wallet.encrypted.transaction_pin, 32);
//	waitForButtonPress();
//
//	writeEinkDisplay("fetching...tpt", false, COL_1_X, LINE_1_Y, "",false,5,30, "",false,5,50, "",false,5,70, "",false,0,0);
//	displayBigHexStream(bufferPIN3, 32);
//	waitForButtonPress();

	return transactionPINHashPointer;
}


/**CHECK THAT THIS FOLLOWS THE BIP32 CURRENT SCHEMA
 *
 * Generate a new address using the deterministic private key generator.
  * \param out_address The new address will be written here (if everything
  *                    goes well). This must be a byte array with space for
  *                    20 bytes.
  * \param out_public_key The public key corresponding to the new address will
  *                       be written here (if everything goes well).
  * \return The address handle of the new address on success,
  *         or #BAD_ADDRESS_HANDLE if an error occurred.
  *         Use walletGetLastError() to get more detail about an error.
  */
AddressHandle makeNewAddress(uint8_t *out_address, PointAffine *out_public_key)
{
	WalletErrors r;

	if (!wallet_loaded)
	{
		last_error = WALLET_NOT_LOADED;
		return BAD_ADDRESS_HANDLE;
	}
#ifdef TEST_WALLET
	if (current_wallet.encrypted.num_addresses >= MAX_TESTING_ADDRESSES)
#else
	if (current_wallet.encrypted.num_addresses >= MAX_ADDRESSES)
#endif // #ifdef TEST_WALLET
	{
		last_error = WALLET_FULL;
		return BAD_ADDRESS_HANDLE;
	}
	(current_wallet.encrypted.num_addresses)++;
	calculateWalletChecksum(current_wallet.encrypted.checksum);
	r = writeCurrentWalletRecord(wallet_nv_address);
	if (r != WALLET_NO_ERROR)
	{
		last_error = r;
		return BAD_ADDRESS_HANDLE;
	}
//	last_error = getAddressAndPublicKey(out_address, out_public_key, current_wallet.encrypted.num_addresses);
	if (last_error != WALLET_NO_ERROR)
	{
		return BAD_ADDRESS_HANDLE;
	}
	else
	{
		return current_wallet.encrypted.num_addresses;
	}
}

bool checkWalletLoaded(void)
{
	if (!wallet_loaded)
	{
		return false;
	}
	else if (wallet_loaded)
	{
		return true;
	}
	else
	{
		return false;
	}
}

/** Given an address handle, use the deterministic private key
  * generator to generate the address and public key associated
  * with that address handle.
  * \param out_address The address will be written here (if everything
  *                    goes well). This must be a byte array with space for
  *                    20 bytes.
  * \param out_public_key The public key corresponding to the address will
  *                       be written here (if everything goes well).
  * \param ah The address handle to obtain the address/public key of.
  * \return #WALLET_NO_ERROR on success, or one of #WalletErrorsEnum if an
  *         error occurred.
  */
WalletErrors getPublicKey(uint8_t *out_address, PointAffine *out_public_key, AddressHandle ah_root, AddressHandle ah_chain, AddressHandle ah_index)
{
	uint8_t buffer[32];
	HashState hs;
	WalletErrors r;
	uint8_t i;

	if (!wallet_loaded)
	{
		last_error = WALLET_NOT_LOADED;
		return last_error;
	}
//	if (current_wallet.encrypted.num_addresses == 0)
//	{
//		last_error = WALLET_EMPTY;
//		return last_error;
//	}
//	if ((ah == 0) || (ah > current_wallet.encrypted.num_addresses) || (ah == BAD_ADDRESS_HANDLE))
//	{
//		last_error = WALLET_INVALID_HANDLE;
//		return last_error;
//	}

	// Calculate private key.
	r = getPrivateKeyExtended(buffer, ah_root, ah_chain, ah_index);
	if (r != WALLET_NO_ERROR)
	{
		last_error = r;
		return r;
	}
////	e5df169fd796f83cecbf9625dfa168edb568902d1a2e5e35180bc1da7159b882 private key that generates 1LkaAZ365ob99kAJo9UgZnBx5NoVmCHRzW (uncompressed) and 1MCrH2bbUBeT1kHVVa6i5trmPbz2Lbs3R4 (compressed)
//	uint8_t staticPrivate[32] = {0xe5,0xdf,0x16,0x9f,0xd7,0x96,0xf8,0x3c,0xec,0xbf,0x96,0x25,0xdf,0xa1,0x68,0xed,0xb5,0x68,0x90,0x2d,0x1a,0x2e,0x5e,0x35,0x18,0x0b,0xc1,0xda,0x71,0x59,0xb8,0x82};
////	gives 1JeK65VGgFqfwh71ztfTytSTNQjBq4rGHH - needs to be end swapped
//	swapEndian256(staticPrivate);
//	memcpy(buffer, staticPrivate, 32);

	// Calculate public key.
	setToG(out_public_key);
	pointMultiply(out_public_key, buffer);
	// Calculate address. The Bitcoin convention is to hash the public key in
	// big-endian format, which is why the counters run backwards in the next
	// two loops.
	// This should calculate the COMPRESSED VERION for new compatibility



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
	writeHashToByteArray(out_address, &hs, true);
//	writeHashToByteArray(buffer, &hs, true);
//	ripemd160Begin(&hs);
//	for (i = 0; i < 32; i++)
//	{
//		ripemd160WriteByte(&hs, buffer[i]);
//	}
//	ripemd160Finish(&hs);
//	writeHashToByteArray(buffer, &hs, true);
//	memcpy(out_address, buffer, 20);

	last_error = WALLET_NO_ERROR;
	return last_error;
}


/** Given an address handle, use the deterministic private key
  * generator to generate the address and public key associated
  * with that address handle.
  * \param out_address The address will be written here (if everything
  *                    goes well). This must be a byte array with space for
  *                    20 bytes.
  * \param out_public_key The public key corresponding to the address will
  *                       be written here (if everything goes well).
  * \param ah The address handle to obtain the address/public key of.
  * \return #WALLET_NO_ERROR on success, or one of #WalletErrorsEnum if an
  *         error occurred.
  */
WalletErrors getAddressAndPublicKey(uint8_t *out_address, PointAffine *out_public_key, AddressHandle ah_root, AddressHandle ah_chain, AddressHandle ah_index)
{
	uint8_t buffer[32];
	HashState hs;
	WalletErrors r;
	uint8_t i;

	if (!wallet_loaded)
	{
		last_error = WALLET_NOT_LOADED;
		return last_error;
	}
//	if (current_wallet.encrypted.num_addresses == 0)
//	{
//		last_error = WALLET_EMPTY;
//		return last_error;
//	}
//	if ((ah == 0) || (ah > current_wallet.encrypted.num_addresses) || (ah == BAD_ADDRESS_HANDLE))
//	{
//		last_error = WALLET_INVALID_HANDLE;
//		return last_error;
//	}

	// Calculate private key.
	r = getPrivateKeyExtended(buffer, ah_root, ah_chain, ah_index);
	if (r != WALLET_NO_ERROR)
	{
		last_error = r;
		return r;
	}
////	e5df169fd796f83cecbf9625dfa168edb568902d1a2e5e35180bc1da7159b882 private key that generates 1LkaAZ365ob99kAJo9UgZnBx5NoVmCHRzW (uncompressed) and 1MCrH2bbUBeT1kHVVa6i5trmPbz2Lbs3R4 (compressed)
//	uint8_t staticPrivate[32] = {0xe5,0xdf,0x16,0x9f,0xd7,0x96,0xf8,0x3c,0xec,0xbf,0x96,0x25,0xdf,0xa1,0x68,0xed,0xb5,0x68,0x90,0x2d,0x1a,0x2e,0x5e,0x35,0x18,0x0b,0xc1,0xda,0x71,0x59,0xb8,0x82};
////	gives 1JeK65VGgFqfwh71ztfTytSTNQjBq4rGHH - needs to be end swapped
//	swapEndian256(staticPrivate);
//	memcpy(buffer, staticPrivate, 32);

	// Calculate public key.
	setToG(out_public_key);
	pointMultiply(out_public_key, buffer);
	// Calculate address. The Bitcoin convention is to hash the public key in
	// big-endian format, which is why the counters run backwards in the next
	// two loops.
	// This should calculate the COMPRESSED VERION for new compatibility



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
	ripemd160Begin(&hs);
	for (i = 0; i < 32; i++)
	{
		ripemd160WriteByte(&hs, buffer[i]);
	}
	ripemd160Finish(&hs);
	writeHashToByteArray(buffer, &hs, true);
	memcpy(out_address, buffer, 20);

	last_error = WALLET_NO_ERROR;
	return last_error;
}

/** Get the master public key of the currently loaded wallet. Every public key
  * (and address) in a wallet can be derived from the master public key and
  * chain code. However, even with posession of the master public key, all
  * private keys are still secret.
  * \param out_public_key The master public key will be written here.
  * \param out_chain_code The chain code will be written here. This must be a
  *                       byte array with space for 32 bytes.
  * \return #WALLET_NO_ERROR on success, or one of #WalletErrorsEnum if an
  *         error occurred.
  */
WalletErrors getMasterPublicKey(PointAffine *out_public_key, uint8_t *out_chain_code)
{
	uint8_t local_seed[SEED_LENGTH]; // need a local copy to modify
	BigNum256 k_par;

	if (!wallet_loaded)
	{
		last_error = WALLET_NOT_LOADED;
		return last_error;
	}
	memcpy(local_seed, current_wallet.encrypted.seed, SEED_LENGTH);
	memcpy(out_chain_code, &(local_seed[32]), 32);
	k_par = (BigNum256)local_seed;
	swapEndian256(k_par); // since seed is big-endian
	setFieldToN();
	bigModulo(k_par, k_par); // just in case
	setToG(out_public_key);
	pointMultiply(out_public_key, k_par);
	last_error = WALLET_NO_ERROR;
	return last_error;
}

/** Get the current number of addresses in a wallet.
  * \return The current number of addresses on success, or 0 if an error
  *         occurred. Use walletGetLastError() to get more detail about
  *         an error.
  */
uint32_t getNumAddresses(void)
{
	if (!wallet_loaded)
	{
		last_error = WALLET_NOT_LOADED;
		return 0;
	}
	if (current_wallet.encrypted.num_addresses == 0)
	{
		last_error = WALLET_EMPTY;
		return 0;
	}
	else
	{
		last_error = WALLET_NO_ERROR;
		return current_wallet.encrypted.num_addresses;
	}
}

/** Given an address handle, use the deterministic private key
  * generator to generate the private key associated with that address handle.
  * \param out The private key will be written here (if everything goes well).
  *            This must be a byte array with space for 32 bytes.
  * \param ah The address handle to obtain the private key of.
  * \return #WALLET_NO_ERROR on success, or one of #WalletErrorsEnum if an
  *         error occurred.
  */
WalletErrors getPrivateKey(uint8_t *out, AddressHandle ah)
{
//	char textToWrite[ 16 ];
//	uint32_t currentScore = 42;
	// as per comment from LS_dev, platform is int 16bits
//	int size;
//	size = sprintf(textToWrite,"%lu", ah);
//	writeEinkDisplay(textToWrite, false, 10, 10, "",false,0,0,"",false,0,0,"",false,0,0, "",false,0,0);

	if (!wallet_loaded)
	{
		last_error = WALLET_NOT_LOADED;
		return last_error;
	}
//	if (current_wallet.encrypted.num_addresses == 0)
//	{
//		last_error = WALLET_EMPTY;
//		return last_error;
//	}
//	if ((ah == 0) || (ah > current_wallet.encrypted.num_addresses) || (ah == BAD_ADDRESS_HANDLE))
//	{
//		last_error = WALLET_INVALID_HANDLE;
//		return last_error;
//	}

	uint32_t chain_lvl_1 = 0;
	uint32_t chain_lvl_2 = 0; // 0 external, 1 internal
	uint32_t chain_lvl_3;
	chain_lvl_3 = ah;

	if (generateDeterministic256(out, current_wallet.encrypted.seed, chain_lvl_1, chain_lvl_2, chain_lvl_3))
	{
		// This should never happen.
		last_error = WALLET_RNG_FAILURE;
		return last_error;
	}

	last_error = WALLET_NO_ERROR;
	return last_error;
}

/** Given an address handle, use the deterministic private key
  * generator to generate the private key associated with that address handle.
  * \param out The private key will be written here (if everything goes well).
  *            This must be a byte array with space for 32 bytes.
  * \param ah The address handle to obtain the private key of.
  * \return #WALLET_NO_ERROR on success, or one of #WalletErrorsEnum if an
  *         error occurred.
  */
WalletErrors getPrivateKeyExtended(uint8_t *out, AddressHandle ah_root, AddressHandle ah_chain, AddressHandle ah_index)
{
//	char textToWrite[ 16 ];
//	uint32_t currentScore = 42;
	// as per comment from LS_dev, platform is int 16bits
//	int size;
//	size = sprintf(textToWrite,"%lu", ah);
//	writeEinkDisplay(textToWrite, false, 10, 10, "",false,0,0,"",false,0,0,"",false,0,0, "",false,0,0);

	if (!wallet_loaded)
	{
		last_error = WALLET_NOT_LOADED;
		return last_error;
	}
//	if (current_wallet.encrypted.num_addresses == 0)
//	{
//		last_error = WALLET_EMPTY;
//		return last_error;
//	}
//	if ((ah == 0) || (ah > current_wallet.encrypted.num_addresses) || (ah == BAD_ADDRESS_HANDLE))
//	{
//		last_error = WALLET_INVALID_HANDLE;
//		return last_error;
//	}

	uint32_t chain_lvl_1 = ah_root;
	uint32_t chain_lvl_2 = ah_chain; // 0 external, 1 internal
	uint32_t chain_lvl_3 = ah_index;
//	chain_lvl_3 = ah;

	if (generateDeterministic256(out, current_wallet.encrypted.seed, chain_lvl_1, chain_lvl_2, chain_lvl_3))
	{
		// This should never happen.
		last_error = WALLET_RNG_FAILURE;
		return last_error;
	}

	last_error = WALLET_NO_ERROR;
	return last_error;
}


/** Given a node depth and indices, generate an xpub for the deterministic wallet
  * \param out The xpub will be written here (if everything goes well).
  *            This must be a byte array with space for 32 bytes.
  * \param ah The address handle to obtain the xpub key of.
  * \return #WALLET_NO_ERROR on success, or one of #WalletErrorsEnum if an
  *         error occurred.
  */
WalletErrors getPublicExtendedKey(uint8_t *out, AddressHandle ah)
{
	if (!wallet_loaded)
	{
		last_error = WALLET_NOT_LOADED;
		return last_error;
	}
//	if (current_wallet.encrypted.num_addresses == 0)
//	{
//		last_error = WALLET_EMPTY;
//		return last_error;
//	}
//	if ((ah == 0) || (ah > current_wallet.encrypted.num_addresses) || (ah == BAD_ADDRESS_HANDLE))
//	{
//		last_error = WALLET_INVALID_HANDLE;
//		return last_error;
//	}
	if (getXPUBfromNode(out, current_wallet.encrypted.seed, ah))
	{
		// This should never happen.
		last_error = WALLET_RNG_FAILURE;
		return last_error;
	}

	last_error = WALLET_NO_ERROR;
	return last_error;
}

/** Change the encryption key of a wallet.
  * \param password Password to use to derive wallet encryption key.
  * \param password_length Length of password, in bytes. Use 0 to specify no
  *                        password (i.e. wallet is unencrypted).
  * \return #WALLET_NO_ERROR on success, or one of #WalletErrorsEnum if an
  *         error occurred.
  */
WalletErrors changeEncryptionKey(const uint8_t *password, const unsigned int password_length)
{
	WalletErrors r;

	if (!wallet_loaded)
	{
		last_error = WALLET_NOT_LOADED;
		return last_error;
	}

	deriveAndSetEncryptionKey(current_wallet.unencrypted.uuid, password, password_length);
	// Updating the version field for a hidden wallet would reveal
	// where it is, so don't do it.
	if (!is_hidden_wallet)
	{
		r = updateWalletVersion();
		if (r != WALLET_NO_ERROR)
		{
			last_error = r;
			return last_error;
		}
	}

	calculateWalletChecksum(current_wallet.encrypted.checksum);
	last_error = writeCurrentWalletRecord(wallet_nv_address);
	return last_error;
}

/** Change the name of the currently loaded wallet.
  * \param new_name This should point to #NAME_LENGTH bytes (padded with
  *                 spaces if necessary) containing the new desired name of
  *                 the wallet.
  * \return #WALLET_NO_ERROR on success, or one of #WalletErrorsEnum if an
  *         error occurred.
  */
WalletErrors changeWalletName(uint8_t *new_name)
{
	if (!wallet_loaded)
	{
		last_error = WALLET_NOT_LOADED;
		return last_error;
	}
	if (is_hidden_wallet)
	{
		// Wallet name updates on a hidden wallet would reveal where it is
		// (since names are publicly visible), so don't allow name changes.
		last_error = WALLET_INVALID_OPERATION;
		return last_error;
	}

	memcpy(current_wallet.unencrypted.name, new_name, NAME_LENGTH);
	calculateWalletChecksum(current_wallet.encrypted.checksum);
	last_error = writeCurrentWalletRecord(wallet_nv_address);
	return last_error;
}

/** Obtain publicly available information about a wallet. "Publicly available"
  * means that the leakage of that information would have a relatively low
  * impact on security (compared to the leaking of, say, the deterministic
  * private key generator seed).
  *
  * Note that unlike most of the other wallet functions, this function does
  * not require the wallet to be loaded. This is so that a user can be
  * presented with a list of all the wallets stored on a hardware Bitcoin
  * wallet, without having to know the encryption key to each wallet.
  * \param out_version The version (see #WalletVersion) of the wallet will be
  *                    written to here (if everything goes well).
  * \param out_name The (space-padded) name of the wallet will be written
  *                 to here (if everything goes well). This should be a
  *                 byte array with enough space to store #NAME_LENGTH bytes.
  * \param out_uuid The wallet UUID will be written to here (if everything
  *                 goes well). This should be a byte array with enough space
  *                 to store #DEVICE_UUID_LENGTH bytes.
  * \param out_protected boolean to describe if the wallet is passworded or not
  * \param wallet_spec The wallet number of the wallet to query.
  * \return #WALLET_NO_ERROR on success, or one of #WalletErrorsEnum if an
  *         error occurred.
  */
WalletErrors getWalletInfo(uint32_t *out_version, uint8_t *out_name, uint8_t *out_uuid, uint32_t wallet_spec)
{
	WalletErrors r;
	WalletRecord local_wallet_record;
	uint32_t local_wallet_nv_address = 0;

	local_wallet_record.unencrypted.version = 0;

	if (getNumberOfWallets() == 0)
	{
		return last_error; // propagate error code
	}
	if (wallet_spec >= num_wallets)
	{
		last_error = WALLET_INVALID_WALLET_NUM;
		return last_error;
	}
	local_wallet_nv_address = WALLET_START_ADDRESS + wallet_spec * sizeof(WalletRecord);
	r = readWalletRecord(&local_wallet_record, local_wallet_nv_address);
	if (r != WALLET_NO_ERROR)
	{
		last_error = r;
		return last_error;
	}
	*out_version = local_wallet_record.unencrypted.version;
	memcpy(out_name, local_wallet_record.unencrypted.name, NAME_LENGTH);
	memcpy(out_uuid, local_wallet_record.unencrypted.uuid, DEVICE_UUID_LENGTH);


	last_error = WALLET_NO_ERROR;
	return last_error;
}

/** Initiate a wallet backup of the currently loaded wallet.
  * \param do_encrypt Whether the wallet backup will be written in encrypted
  *                   form.
  * \param destination_device See writeBackupSeed().
  * \return #WALLET_NO_ERROR on success, or one of #WalletErrorsEnum if an
  *         error occurred.
  */
WalletErrors backupWallet(bool do_encrypt, uint32_t destination_device)
{
	uint8_t encrypted_seed[SEED_LENGTH];
	uint8_t n[16];
	bool r;
	uint8_t i;

	if (!wallet_loaded)
	{
		last_error = WALLET_NOT_LOADED;
		return last_error;
	}

	if (do_encrypt)
	{
#ifdef TEST
		assert(SEED_LENGTH % 16 == 0);
#endif
		memset(n, 0, 16);
		for (i = 0; i < SEED_LENGTH; i = (uint8_t)(i + 16))
		{
			writeU32LittleEndian(n, i);
			xexEncrypt(&(encrypted_seed[i]), &(current_wallet.encrypted.seed[i]), n, 1);
		}
		r = writeBackupSeed(encrypted_seed, do_encrypt, destination_device);
	}
	else
	{
		r = writeBackupSeed(current_wallet.encrypted.seed, do_encrypt, destination_device);
	}
	if (r)
	{
		last_error = WALLET_BACKUP_ERROR;
		return last_error;
	}
	else
	{
		last_error = WALLET_NO_ERROR;
		return last_error;
	}
}

/** Obtain the size of non-volatile storage by doing a bunch of test reads.
  * \return The size in bytes, less one, of non-volatile storage. 0 indicates
  *         that a read error occurred. For example, a return value of 9999
  *         means that non-volatile storage is 10000 bytes large (or
  *         equivalently, 9999 is the largest valid address).
  */
static uint32_t findOutNonVolatileSize(void)
{
	uint32_t test_bit;
	uint32_t size;
	uint8_t junk;
	NonVolatileReturn r;

	// Find out size using binary search.
	test_bit = 0x80000000;
	size = 0;
	while (test_bit != 0)
	{
		size |= test_bit;
		r = nonVolatileRead(&junk, size, 1);
		if (r == NV_INVALID_ADDRESS)
		{
			size ^= test_bit; // too big; clear it
		}
		else if (r != NV_NO_ERROR)
		{
			last_error = WALLET_READ_ERROR;
			return 0; // read error occurred
		}
		test_bit >>= 1;
	}
	last_error = WALLET_NO_ERROR;
	return size;
}

/** Get the number of wallets which can fit in non-volatile storage, assuming
  * the storage format specified in storage_common.h.
  * This will set #num_wallets.
  * \return The number of wallets on success, or 0 if a read error occurred.
  */
uint32_t getNumberOfWallets(void)
{
	uint32_t size;

	last_error = WALLET_NO_ERROR;
	if (num_wallets == 0)
	{
		// Need to calculate number of wallets that can fit in non-volatile
		// storage.
		size = findOutNonVolatileSize();
		if (size != 0)
		{
			if (size != 0xffffffff)
			{
				// findOutNonVolatileSize() returns the size of non-volatile
				// storage, less one byte.
				size++;
			}
			num_wallets = (size - WALLET_START_ADDRESS) / sizeof(WalletRecord);
		}
		// If findOutNonVolatileSize() returned 0, num_wallets will still be
		// 0, signifying that a read error occurred. last_error will also
		// be set appropriately.
	}
	return num_wallets;
}

