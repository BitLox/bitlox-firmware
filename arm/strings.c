/** \file strings.c
  *
  * \brief Defines and retrieves device-specific strings.
  *
  * It's important that these strings are stored in program memory (flash),
  * using the #PROGMEM attribute, otherwise they eat up valuable RAM.
  *
  * This file is licensed as described by the file LICENCE.
  */

//#include <avr/io.h>
//#include <avr/pgmspace.h>

#include "../common.h"
#include "../hwinterface.h"
#include "avr2arm.h"

/**
 * \defgroup DeviceStrings Device-specific strings.
 *
 * @{
 */
/** Version string. */
static const char str_MISCSTR_VERSION[] PROGMEM = "BITLOX TM";
/** Permission denied (user pressed cancel button) string. */
static const char str_MISCSTR_PERMISSION_DENIED_USER[] PROGMEM = "Permission denied by user";
/** String specifying that processPacket() didn't like the format or
  * contents of a packet. */
static const char str_MISCSTR_INVALID_PACKET[] PROGMEM = "Invalid packet";
/** String specifying that processPacket() didn't like the format or
  * contents of a packet. */
static const char str_MISCSTR_INVALID_PACKET2[] PROGMEM = "-";
/** String specifying that a parameter was unacceptably large. */
static const char str_MISCSTR_PARAM_TOO_LARGE[] PROGMEM = "Parameter too large";
/** Permission denied (host cancelled action) string. */
static const char str_MISCSTR_PERMISSION_DENIED_HOST[] PROGMEM = "Host cancelled action";
/** String specifying that an unexpected message was received. */
static const char str_MISCSTR_UNEXPECTED_PACKET[] PROGMEM = "Unexpected packet";
/** String specifying that the submitted one-time password (OTP) did not match
  * the generated OTP. */
static const char str_MISCSTR_OTP_MISMATCH[] PROGMEM = "OTP mismatch";
/** String for #WALLET_FULL wallet error. */
static const char str_MISCSTR_CONFIG[] PROGMEM = "3X8E_3A8C";
/** String for #WALLET_FULL wallet error. */
static const char str_WALLET_FULL[] PROGMEM = "Wallet has run out of space";
/** String for #WALLET_EMPTY wallet error. */
static const char str_WALLET_EMPTY[] PROGMEM = "Wallet has nothing in it";
/** String for #WALLET_READ_ERROR wallet error. */
static const char str_WALLET_READ_ERROR[] PROGMEM = "EEPROM Read error";
/** String for #WALLET_WRITE_ERROR error. */
static const char str_WALLET_WRITE_ERROR[] PROGMEM = "EEPROM Write error";
/** String for #WALLET_ADDRESS_NOT_FOUND wallet error. */
static const char str_WALLET_ADDRESS_NOT_FOUND[] PROGMEM = "Address not in wallet";
/** String for #WALLET_NOT_THERE wallet error. */
static const char str_WALLET_NOT_THERE[] PROGMEM = "Wallet doesn't exist/PIN incorrect";
/** String for #WALLET_NOT_LOADED wallet error. */
static const char str_WALLET_NOT_LOADED[] PROGMEM = "Wallet not loaded";
/** String for #WALLET_INVALID_HANDLE wallet error. */
static const char str_WALLET_INVALID_HANDLE[] PROGMEM = "Invalid address handle";
/** String for #WALLET_BACKUP_ERROR wallet error. */
static const char str_WALLET_BACKUP_ERROR[] PROGMEM = "Seed could not be written to specified device";
/** String for #WALLET_RNG_FAILURE wallet error. */
static const char str_WALLET_RNG_FAILURE[] PROGMEM = "Failure in random number generation system";
/** String for #WALLET_INVALID_WALLET_NUM wallet error. */
static const char str_WALLET_INVALID_WALLET_NUM[] PROGMEM = "Invalid wallet number";
/** String for #WALLET_INVALID_OPERATION wallet error. */
static const char str_WALLET_INVALID_OPERATION[] PROGMEM = "Operation not allowed";
/** String for #TRANSACTION_INVALID_FORMAT transaction parser error. */
static const char str_TRANSACTION_INVALID_FORMAT[] PROGMEM = "Format of transaction is unknown or invalid";
/** String for #TRANSACTION_TOO_MANY_INPUTS transaction parser error. */
static const char str_TRANSACTION_TOO_MANY_INPUTS[] PROGMEM = "Too many inputs in transaction";
/** String for #TRANSACTION_TOO_MANY_OUTPUTS transaction parser error. */
static const char str_TRANSACTION_TOO_MANY_OUTPUTS[] PROGMEM = "Too many outputs in transaction";
/** String for #TRANSACTION_TOO_LARGE transaction parser error. */
static const char str_TRANSACTION_TOO_LARGE[] PROGMEM = "Transaction size is too large";
/** String for #TRANSACTION_NON_STANDARD transaction parser error. */
static const char str_TRANSACTION_NON_STANDARD[] PROGMEM = "Transaction is non-standard";
/** String for #TRANSACTION_INVALID_AMOUNT transaction parser error. */
static const char str_TRANSACTION_INVALID_AMOUNT[] PROGMEM = "Invalid output amount in transaction";
/** String for #TRANSACTION_INVALID_REFERENCE transaction parser error. */
static const char str_TRANSACTION_INVALID_REFERENCE[] PROGMEM = "Invalid transaction reference";
/** String for unknown error. */
static const char str_UNKNOWN[] PROGMEM = "Unknown error";
/**@}*/

/** Obtain one character from one of the device's strings.
  * \param set Specifies which set of strings to use; should be
  *            one of #StringSetEnum.
  * \param spec Specifies which string to get the character from. The
  *             interpretation of this depends on the value of set;
  *             see #StringSetEnum for clarification.
  * \param pos The position of the character within the string; 0 means first,
  *            1 means second etc.
  * \return The character from the specified string.
  */
char getString(StringSet set, uint8_t spec, uint16_t pos)
{
	if (pos >= getStringLength(set, spec))
	{
		// Attempting to read beyond end of string.
		return 0;
	}
	if (set == STRINGSET_MISC)
	{
		switch (spec)
		{
		case MISCSTR_VENDOR:
			return (char)pgm_read_byte(&(str_MISCSTR_VERSION[pos]));
			break;
		case MISCSTR_PERMISSION_DENIED_USER:
			return (char)pgm_read_byte(&(str_MISCSTR_PERMISSION_DENIED_USER[pos]));
			break;
		case MISCSTR_INVALID_PACKET:
			return (char)pgm_read_byte(&(str_MISCSTR_INVALID_PACKET[pos]));
			break;
		case MISCSTR_PARAM_TOO_LARGE:
			return (char)pgm_read_byte(&(str_MISCSTR_PARAM_TOO_LARGE[pos]));
			break;
		case MISCSTR_PERMISSION_DENIED_HOST:
			return (char)pgm_read_byte(&(str_MISCSTR_PERMISSION_DENIED_HOST[pos]));
			break;
		case MISCSTR_UNEXPECTED_PACKET:
			return (char)pgm_read_byte(&(str_MISCSTR_UNEXPECTED_PACKET[pos]));
			break;
		case MISCSTR_OTP_MISMATCH:
			return (char)pgm_read_byte(&(str_MISCSTR_OTP_MISMATCH[pos]));
			break;
		case MISCSTR_CONFIG:
			return (char)pgm_read_byte(&(str_MISCSTR_CONFIG[pos]));
			break;
		case MISCSTR_INVALID_PACKET2:
			return (char)pgm_read_byte(&(str_MISCSTR_INVALID_PACKET2[pos]));
			break;
		default:
			return (char)pgm_read_byte(&(str_UNKNOWN[pos]));
			break;
		}
	}
	else if (set == STRINGSET_WALLET)
	{
		switch (spec)
		{
		case WALLET_FULL:
			return (char)pgm_read_byte(&(str_WALLET_FULL[pos]));
			break;
		case WALLET_EMPTY:
			return (char)pgm_read_byte(&(str_WALLET_EMPTY[pos]));
			break;
		case WALLET_READ_ERROR:
			return (char)pgm_read_byte(&(str_WALLET_READ_ERROR[pos]));
			break;
		case WALLET_WRITE_ERROR:
			return (char)pgm_read_byte(&(str_WALLET_WRITE_ERROR[pos]));
			break;
		case WALLET_ADDRESS_NOT_FOUND:
			return (char)pgm_read_byte(&(str_WALLET_ADDRESS_NOT_FOUND[pos]));
			break;
		case WALLET_NOT_THERE:
			return (char)pgm_read_byte(&(str_WALLET_NOT_THERE[pos]));
			break;
		case WALLET_NOT_LOADED:
			return (char)pgm_read_byte(&(str_WALLET_NOT_LOADED[pos]));
			break;
		case WALLET_INVALID_HANDLE:
			return (char)pgm_read_byte(&(str_WALLET_INVALID_HANDLE[pos]));
			break;
		case WALLET_BACKUP_ERROR:
			return (char)pgm_read_byte(&(str_WALLET_BACKUP_ERROR[pos]));
			break;
		case WALLET_RNG_FAILURE:
			return (char)pgm_read_byte(&(str_WALLET_RNG_FAILURE[pos]));
			break;
		case WALLET_INVALID_WALLET_NUM:
			return (char)pgm_read_byte(&(str_WALLET_INVALID_WALLET_NUM[pos]));
			break;
		case WALLET_INVALID_OPERATION:
			return (char)pgm_read_byte(&(str_WALLET_INVALID_OPERATION[pos]));
			break;
		default:
			return (char)pgm_read_byte(&(str_UNKNOWN[pos]));
			break;
		}
	}
	else if (set == STRINGSET_TRANSACTION)
	{
		switch (spec)
		{
		case TRANSACTION_INVALID_FORMAT:
			return (char)pgm_read_byte(&(str_TRANSACTION_INVALID_FORMAT[pos]));
			break;
		case TRANSACTION_TOO_MANY_INPUTS:
			return (char)pgm_read_byte(&(str_TRANSACTION_TOO_MANY_INPUTS[pos]));
			break;
		case TRANSACTION_TOO_MANY_OUTPUTS:
			return (char)pgm_read_byte(&(str_TRANSACTION_TOO_MANY_OUTPUTS[pos]));
			break;
		case TRANSACTION_TOO_LARGE:
			return (char)pgm_read_byte(&(str_TRANSACTION_TOO_LARGE[pos]));
			break;
		case TRANSACTION_NON_STANDARD:
			return (char)pgm_read_byte(&(str_TRANSACTION_NON_STANDARD[pos]));
			break;
		case TRANSACTION_INVALID_AMOUNT:
			return (char)pgm_read_byte(&(str_TRANSACTION_INVALID_AMOUNT[pos]));
			break;
		case TRANSACTION_INVALID_REFERENCE:
			return (char)pgm_read_byte(&(str_TRANSACTION_INVALID_REFERENCE[pos]));
			break;
		default:
			return (char)pgm_read_byte(&(str_UNKNOWN[pos]));
			break;
		}
	}
	else
	{
		return (char)pgm_read_byte(&(str_UNKNOWN[pos]));
	}
}

/** Get the length of one of the device's strings.
  * \param set Specifies which set of strings to use; should be
  *            one of #StringSetEnum.
  * \param spec Specifies which string to get the character from. The
  *             interpretation of this depends on the value of set;
  *             see #StringSetEnum for clarification.
  * \return The length of the string, in number of characters.
  */
uint16_t getStringLength(StringSet set, uint8_t spec)
{
	if (set == STRINGSET_MISC)
	{
		switch (spec)
		{
		case MISCSTR_VENDOR:
			return (uint16_t)(sizeof(str_MISCSTR_VERSION) - 1);
			break;
		case MISCSTR_PERMISSION_DENIED_USER:
			return (uint16_t)(sizeof(str_MISCSTR_PERMISSION_DENIED_USER) - 1);
			break;
		case MISCSTR_INVALID_PACKET:
			return (uint16_t)(sizeof(str_MISCSTR_INVALID_PACKET) - 1);
			break;
		case MISCSTR_PARAM_TOO_LARGE:
			return (uint16_t)(sizeof(str_MISCSTR_PARAM_TOO_LARGE) - 1);
			break;
		case MISCSTR_PERMISSION_DENIED_HOST:
			return (uint16_t)(sizeof(str_MISCSTR_PERMISSION_DENIED_HOST) - 1);
			break;
		case MISCSTR_UNEXPECTED_PACKET:
			return (uint16_t)(sizeof(str_MISCSTR_UNEXPECTED_PACKET) - 1);
			break;
		case MISCSTR_OTP_MISMATCH:
			return (uint16_t)(sizeof(str_MISCSTR_OTP_MISMATCH) - 1);
			break;
		case MISCSTR_CONFIG:
			return (uint16_t)(sizeof(str_MISCSTR_CONFIG) - 1);
			break;
		default:
			return (uint16_t)(sizeof(str_UNKNOWN) - 1);
			break;
		}
	}
	else if (set == STRINGSET_WALLET)
	{
		switch (spec)
		{
		case WALLET_FULL:
			return (uint16_t)(sizeof(str_WALLET_FULL) - 1);
			break;
		case WALLET_EMPTY:
			return (uint16_t)(sizeof(str_WALLET_EMPTY) - 1);
			break;
		case WALLET_READ_ERROR:
			return (uint16_t)(sizeof(str_WALLET_READ_ERROR) - 1);
			break;
		case WALLET_WRITE_ERROR:
			return (uint16_t)(sizeof(str_WALLET_WRITE_ERROR) - 1);
			break;
		case WALLET_ADDRESS_NOT_FOUND:
			return (uint16_t)(sizeof(str_WALLET_ADDRESS_NOT_FOUND) - 1);
			break;
		case WALLET_NOT_THERE:
			return (uint16_t)(sizeof(str_WALLET_NOT_THERE) - 1);
			break;
		case WALLET_NOT_LOADED:
			return (uint16_t)(sizeof(str_WALLET_NOT_LOADED) - 1);
			break;
		case WALLET_INVALID_HANDLE:
			return (uint16_t)(sizeof(str_WALLET_INVALID_HANDLE) - 1);
			break;
		case WALLET_BACKUP_ERROR:
			return (uint16_t)(sizeof(str_WALLET_BACKUP_ERROR) - 1);
			break;
		case WALLET_RNG_FAILURE:
			return (uint16_t)(sizeof(str_WALLET_RNG_FAILURE) - 1);
			break;
		case WALLET_INVALID_WALLET_NUM:
			return (uint16_t)(sizeof(str_WALLET_INVALID_WALLET_NUM) - 1);
			break;
		case WALLET_INVALID_OPERATION:
			return (uint16_t)(sizeof(str_WALLET_INVALID_OPERATION) - 1);
			break;
		default:
			return (uint16_t)(sizeof(str_UNKNOWN) - 1);
			break;
		}
	}
	else if (set == STRINGSET_TRANSACTION)
	{
		switch (spec)
		{
		case TRANSACTION_INVALID_FORMAT:
			return (uint16_t)(sizeof(str_TRANSACTION_INVALID_FORMAT) - 1);
			break;
		case TRANSACTION_TOO_MANY_INPUTS:
			return (uint16_t)(sizeof(str_TRANSACTION_TOO_MANY_INPUTS) - 1);
			break;
		case TRANSACTION_TOO_MANY_OUTPUTS:
			return (uint16_t)(sizeof(str_TRANSACTION_TOO_MANY_OUTPUTS) - 1);
			break;
		case TRANSACTION_TOO_LARGE:
			return (uint16_t)(sizeof(str_TRANSACTION_TOO_LARGE) - 1);
			break;
		case TRANSACTION_NON_STANDARD:
			return (uint16_t)(sizeof(str_TRANSACTION_NON_STANDARD) - 1);
			break;
		case TRANSACTION_INVALID_AMOUNT:
			return (uint16_t)(sizeof(str_TRANSACTION_INVALID_AMOUNT) - 1);
			break;
		case TRANSACTION_INVALID_REFERENCE:
			return (uint16_t)(sizeof(str_TRANSACTION_INVALID_REFERENCE) - 1);
			break;
		default:
			return (uint16_t)(sizeof(str_UNKNOWN) - 1);
			break;
		}
	}
	else
	{
		return (uint16_t)(sizeof(str_UNKNOWN) - 1);
	}
}

