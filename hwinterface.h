/** \file hwinterface.h
  *
  * \brief Defines the platform-dependent interface.
  *
  * All the platform-independent code makes reference to some functions
  * which are strongly platform-dependent. This file describes all the
  * functions which must be implemented on the platform-dependent side.
  *
  * This file is licensed as described by the file LICENCE.
  */

#ifndef HWINTERFACE_H_INCLUDED
#define HWINTERFACE_H_INCLUDED

#include "common.h"
#include "wallet.h"
#include "transaction.h"

#ifdef __cplusplus
     extern "C" {
#endif

#define MAX_INPUTS				128

//     must be multiple of 256
//#define EEPROM_SIZE		 1024
#define EEPROM_SIZE		 20992
//20896

#define ACCEPT_X_START	20
#define DENY_X_START	185

#define draw_X_X 185
#define draw_X_Y 87
#define draw_check_X 1
#define draw_check_Y 85


#define MAX_STRING_DISPLAY 60

/** Return values for non-volatile storage I/O functions. */
typedef enum NonVolatileReturnEnum
{
	/** No error actually occurred. */
	NV_NO_ERROR					=	0,
	/** Invalid address supplied (or, I/O would go beyond end of storage
	  * space). */
	NV_INVALID_ADDRESS			=	1,
	/** Catch-all for all other read/write errors. */
	NV_IO_ERROR					=	2,
} NonVolatileReturn;

/** Values for userDenied() function which specify what to ask the user
  * about. */
typedef enum AskUserCommandEnum
{
	/** Do you want to create a new wallet? */
	ASKUSER_NEW_DEFAULT_WALLET			=	1,
	/** Do you want to create a new address in this wallet? */
	ASKUSER_NEW_ADDRESS			=	2,
	/** Do you want to do this transaction? */
	ASKUSER_SIGN_TRANSACTION	=	3,
	/** Do you want to delete everything? */
	ASKUSER_FORMAT				=	4,
	/** Do you want to change the name of a wallet? */
	ASKUSER_CHANGE_NAME			=	5,
	/** Do wallet backup? */
	ASKUSER_BACKUP_WALLET		=	6,
	/** Restore wallet from backup? */
	ASKUSER_RESTORE_WALLET		=	7,
	/** Do you want to change the encryption key of a wallet? */
	ASKUSER_CHANGE_KEY			=	8,
	/** Do you want to give the host access to the master public key? */
	ASKUSER_GET_MASTER_KEY		=	9,
	/** Do you want to delete an existing wallet? */
	ASKUSER_DELETE_WALLET		=	10,
	/** Do you want to create a new wallet? */
	ASKUSER_NEW_WALLET_MNEMONIC	=	11,
	/** Do you want to sign a message? */
	ASKUSER_SIGN_MESSAGE		=	12,
	/** Do you want to create a new default wallet? */
	ASKUSER_NEW_WALLET			=	13,

	ASKUSER_FORMAT_AUTO			=	14,
	ASKUSER_FORMAT_PROGRESS		=	15,

	ASKUSER_INITIAL_SETUP		=	16,
	ASKUSER_NEW_WALLET_LEVEL	=	17,
	ASKUSER_NEW_WALLET_STRENGTH =	18,
	ASKUSER_NEW_WALLET_IS_HIDDEN= 	19,
	ASKUSER_RESTORE_WALLET_DEVICE =	20,
	ASKUSER_RESTORE_WALLET_DEVICE_INPUT_TYPE =	21,
	ASKUSER_NEW_WALLET_NUMBER 	=	22,
	ASKUSER_NEW_WALLET_NO_PASSWORD= 23,
	ASKUSER_PRE_SIGN_MESSAGE 	= 	24,
	ASKUSER_USE_AEM 			= 	25,
	ASKUSER_AEM_DISPLAYPHRASE	=	26,
	ASKUSER_AEM_PASSPHRASE		=	27,
	ASKUSER_AEM_ENTRY			=	28,
	ASKUSER_DESCRIBE_STANDARD_SETUP = 29,
	ASKUSER_DESCRIBE_ADVANCED_SETUP = 30,
	ASKUSER_DESCRIBE_EXPERT_SETUP = 31,
	ASKUSER_DESCRIBE_STANDARD_SETUP_2 = 32,
	ASKUSER_DESCRIBE_ADVANCED_SETUP_2 = 33,
	ASKUSER_DESCRIBE_EXPERT_SETUP_2 = 34,
	ASKUSER_ALPHA_INPUT_PREFACE		= 35,
	ASKUSER_SET_DEVICE_PIN			= 36,
	ASKUSER_SET_WALLET_PIN			= 37,
	ASKUSER_SHOW_DISPLAYPHRASE		= 38,
	ASKUSER_SHOW_UNLOCKPHRASE		= 39,
	ASKUSER_DESCRIBE_STANDARD_SETUP_2_WALLET = 40,
	ASKUSER_MNEMONIC_PREP			= 41,
	ASKUSER_MNEMONIC_PREP_2			= 42,
	ASKUSER_SET_DEVICE_PIN_BIG		= 43,
	ASKUSER_SET_WALLET_PIN_BIG		= 44,
	ASKUSER_FORMAT_WITH_PROGRESS	= 45,
	ASKUSER_ENTER_PIN				= 46,
	ASKUSER_DELETE_ONLY_EX_DISPLAY	= 47,
	ASKUSER_ACCEPT_AND_DELETE_EX_DISPLAY		= 48,
	ASKUSER_DESCRIBE_ADVANCED_SETUP_2_WALLET = 49,
	ASKUSER_DESCRIBE_EXPERT_SETUP_2_WALLET = 50,
	ASKUSER_ENTER_WALLET_PIN		= 51,
	ASKUSER_SET_TRANSACTION_PIN		= 52,
	ASKUSER_TRANSACTION_PIN_SET		= 53,
	ASKUSER_ENTER_PIN_ALPHA			= 54,
	ASKUSER_ENTER_WALLET_PIN_ALPHA	= 55,
	ASKUSER_ENTER_TRANSACTION_PIN	= 56,
	ASKUSER_CONFIRM_HIDDEN_WALLET_NUMBER = 57,
	ASKUSER_AEM_ENTRY_ALPHA			= 58,
	ASKUSER_PREPARING_TRANSACTION	= 59,
	ASKUSER_ASSEMBLING_HASHES		= 60,
	ASKUSER_SENDING_DATA			= 61,
	ASKUSER_SIGN_WITH_PROGRESS		= 62,
	ASKUSER_NEW_WALLET_2			= 63,
	ASKUSER_USE_MNEMONIC_PASSPHRASE = 64

} AskUserCommand;

/** Values for getString() function which specify which set of strings
  * the "spec" parameter selects from. */
typedef enum StringSetEnum
{
	/** "spec" refers to one of the values in #MiscStringsEnum.
	  * See #MiscStringsEnum for what each value should correspond to. */
	STRINGSET_MISC				=	1,
	/** "spec" refers to one of the values in #WalletErrors. The corresponding
	  * string should be a textual representation of the wallet error
	  * (eg. #WALLET_FULL should correspond to something like "Wallet has run
	  * out of space"). */
	STRINGSET_WALLET			=	2,
	/** "spec" refers to one of the values in #TransactionErrors. The
	  * corresponding string should be a textual representation of the
	  * transaction error (eg. #TRANSACTION_TOO_MANY_INPUTS should correspond
	  * to something like "Transaction has too many inputs"). */
	STRINGSET_TRANSACTION		=	3
} StringSet;

/** The miscellaneous strings that can be output. */
typedef enum MiscStringsEnum
{
	/** The device's vendor string. */
	MISCSTR_VENDOR					=	1,
	/** Text explaining that the operation was denied by the user. */
	MISCSTR_PERMISSION_DENIED_USER	=	2,
	/** Text explaining that a packet was malformed or unrecognised. */
	MISCSTR_INVALID_PACKET			=	3,
	/** Text explaining that a parameter was too large. */
	MISCSTR_PARAM_TOO_LARGE			=	4,
	/** Text explaining that the operation was denied by the host. */
	MISCSTR_PERMISSION_DENIED_HOST	=	5,
	/** Text explaining that an unexpected packet was received. */
	MISCSTR_UNEXPECTED_PACKET		=	6,
	/** Text explaining that the submitted one-time password (OTP) did not
	  * match the generated OTP. */
	MISCSTR_OTP_MISMATCH			=	7,
	/** The device's configuration (eg. compile options) string. */
	MISCSTR_CONFIG					=	8,
	/** Text explaining that a packet was malformed or unrecognised. */
	MISCSTR_INVALID_PACKET2			=	9
} MiscStrings;

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
extern char getString(StringSet set, uint8_t spec, uint16_t pos);
/** Get the length of one of the device's strings.
  * \param set Specifies which set of strings to use; should be
  *            one of #StringSetEnum.
  * \param spec Specifies which string to get the character from. The
  *             interpretation of this depends on the value of set;
  *             see #StringSetEnum for clarification.
  * \return The length of the string, in number of characters.
  */
extern uint16_t getStringLength(StringSet set, uint8_t spec);

/** Grab one byte from the communication stream. There is no way for this
  * function to indicate a read error. This is intentional; it
  * makes program flow simpler (no need to put checks everywhere). As a
  * consequence, this function should only return if the received byte is
  * free of read errors.
  *
  * Previously, if a read or write error occurred, processPacket() would
  * return, an error message would be displayed and execution would halt.
  * There is no reason why this couldn't be done inside streamGetOneByte()
  * or streamPutOneByte(). So nothing was lost by omitting the ability to
  * indicate read or write errors.
  *
  * Perhaps the argument can be made that if this function indicated read
  * errors, the caller could attempt some sort of recovery. Perhaps
  * processPacket() could send something to request the retransmission of
  * a packet. But retransmission requests are something which can be dealt
  * with by the implementation of the stream. Thus a caller of
  * streamGetOneByte() will assume that the implementation handles things
  * like automatic repeat request, flow control and error detection and that
  * if a true "stream read error" occurs, the communication link is shot to
  * bits and nothing the caller can do will fix that.
  * \return The received byte.
  */
extern uint8_t streamGetOneByte(void);
/** Send one byte to the communication stream. There is no way for this
  * function to indicate a write error. This is intentional; it
  * makes program flow simpler (no need to put checks everywhere). As a
  * consequence, this function should only return if the byte was sent
  * free of write errors.
  *
  * See streamGetOneByte() for some justification about why write errors
  * aren't indicated by a return value.
  * \param one_byte The byte to send.
  */
extern void streamPutOneByte(uint8_t one_byte);

/** Notify the user interface that the transaction parser has seen a new
  * Bitcoin amount/address pair.
  * \param text_amount The output amount, as a null-terminated text string
  *                    such as "0.01".
  * \param text_address The output address, as a null-terminated text string
  *                     such as "1RaTTuSEN7jJUDiW1EGogHwtek7g9BiEn".
  * \return false if no error occurred, true if there was not enough space to
  *         store the amount/address pair.
  */

//extern bool newMessageSeen(char *text_message, int length);

extern bool newOutputSeen(char *text_amount, char *text_address);
/** Notify the user interface that the transaction parser has seen the
  * transaction fee. If there is no transaction fee, the transaction parser
  * will not call this.
  * \param text_amount The transaction fee, as a null-terminated text string
  *                    such as "0.01".
  */
extern void setTransactionFee(char *text_amount);
/** Notify the user interface that the list of Bitcoin amount/address pairs
  * should be cleared. */
extern void clearOutputsSeen(void);
/** Inform the user that an address has been generated.
  * \param address The output address, as a null-terminated text string
  *                such as "1RaTTuSEN7jJUDiW1EGogHwtek7g9BiEn".
  * \param num_sigs The number of required signatures to redeem Bitcoins from
  *                 the address. For a non-multi-signature address, this
  *                 should be 1.
  * \param num_pubkeys The number of public keys involved in the address. For
  *                    a non-multi-signature address, this should be 1.
  */
extern void displayAddress(char *address, uint8_t num_sigs, uint8_t num_pubkeys);
/** Ask user if they want to allow some action.
  * \param command The action to ask the user about. See #AskUserCommandEnum.
  * \return false if the user accepted, true if the user denied.
  */
extern bool userDenied(AskUserCommand command);
/** Display a short (maximum 8 characters) one-time password for the user to
  * see. This one-time password is used to reduce the chance of a user
  * accidentally doing something stupid.
  * \param command The action to ask the user about. See #AskUserCommandEnum.
  * \param otp The one-time password to display. This will be a
  *            null-terminated string.
  */
extern void displayOTP(AskUserCommand command, char *otp);
/** Clear the OTP (one-time password) shown by displayOTP() from the
  * display. */
extern void clearOTP(void);

/** Fill buffer with 32 random bytes from a hardware random number generator.
  * \param buffer The buffer to fill. This should have enough space for 32
  *               bytes.
  * \return An estimate of the total number of bits (not bytes) of entropy in
  *         the buffer on success, or a negative number if the hardware random
  *         number generator failed in any way. This may also return 0 to tell
  *         the caller that more samples are needed in order to do any
  *         meaningful statistical testing. If this returns 0, the caller
  *         should continue to call this until it returns a non-zero value.
  */
extern int hardwareRandom32Bytes(uint8_t *buffer);

/** Write to non-volatile storage.
  * \param data A pointer to the data to be written.
  * \param address Byte offset specifying where in non-volatile storage to
  *                start writing to.
  * \param length The number of bytes to write.
  * \return See #NonVolatileReturnEnum for return values.
  * \warning Writes may be buffered; use nonVolatileFlush() to be sure that
  *          data is actually written to non-volatile storage.
  */

extern int batteryLevel(void);

extern NonVolatileReturn nonVolatileWrite(uint8_t *data, uint32_t address, uint32_t length);
/** Read from non-volatile storage.
  * \param data A pointer to the buffer which will receive the data.
  * \param address Byte offset specifying where in non-volatile storage to
  *                start reading from.
  * \param length The number of bytes to read.
  * \return See #NonVolatileReturnEnum for return values.
  */
extern NonVolatileReturn nonVolatileReadNoPtr(uint8_t data, uint32_t address, uint32_t length);
extern NonVolatileReturn nonVolatileRead(uint8_t *data, uint32_t address, uint32_t length);

/** Ensure that all buffered writes are committed to non-volatile storage.
  * \return See #NonVolatileReturnEnum for return values.
  */
extern NonVolatileReturn nonVolatileFlush(void);

/** Overwrite anything in RAM which could contain sensitive data. "Sensitive
  * data" includes secret things like encryption keys and wallet private keys.
  * It also includes derived things like expanded keys and intermediate
  * results from elliptic curve calculations. Even past transaction data,
  * addresses and intermediate results from hash calculations could be 
  * considered sensitive and should be overwritten.
  */
extern void sanitiseRam(void);

/** This will be called whenever something very unexpected occurs. This
  * function must not return. */
extern void fatalError(void);

/** Write backup seed to some output device. The choice of output device and
  * seed representation is up to the platform-dependent code. But a typical
  * example would be displaying the seed as a hexadecimal string on a LCD.
  * \param seed A byte array of length #SEED_LENGTH bytes which contains the
  *             backup seed.
  * \param is_encrypted Specifies whether the seed has been encrypted.
  * \param destination_device Specifies which (platform-dependent) device the
  *                           backup seed should be sent to.
  * \return false on success, true if the backup seed could not be written
  *         to the destination device.
  */
extern bool writeBackupSeed(uint8_t *seed, bool is_encrypted, uint32_t destination_device);

/** PBKDF2 is used to derive encryption keys. In order to make brute-force
  * attacks more expensive, this should return a number which is as large
  * as possible, without being so large that key derivation requires an
  * excessive amount of time (> 1 s). This is a platform-dependent function
  * because key derivation speed is platform-dependent.
  *
  * In order to permit key recovery when the number of iterations is unknown,
  * this should be a power of 2. That way, an implementation can use
  * successively greater powers of 2 until the correct number of iterations is
  * found.
  * \return Number of iterations to use in PBKDF2 algorithm.
  */
extern uint32_t getPBKDF2Iterations(void);

extern void passToPrint(char *toPrint);

extern uint8_t *fromhex(const char *str);

extern void Software_Reset(void);

extern uint8_t bluetooth_on;
extern uint8_t is_formatted;
extern uint8_t message_slice_counter;
extern bool moofOn;

extern bool buttonInterjectionNoAck(AskUserCommand command);
extern bool buttonInterjectionNoAckSetup(AskUserCommand command);
extern char *inputInterjectionNoAck(AskUserCommand command);
extern bool buttonInterjectionNoAckPlusData(AskUserCommand command, char *the_data, int size_of_data);
//extern bool buttonInterjectionNoAckPlusDataBig(AskUserCommand command, char *the_data);


#ifdef __cplusplus
     }
#endif

#endif // #ifndef HWINTERFACE_H_INCLUDED
