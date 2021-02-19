/** \file stream_comm.c
  *
  * \brief Deals with packets sent over the stream device.
  *
  * The most important function in this file is processPacket(). It decodes
  * packets from the stream and calls the relevant functions from wallet.c or
  * transaction.c. Some validation of the received data is also handled in
  * this file. Here is a general rule for what validation is done: if the
  * validation can be done without knowing the internal details of how wallets
  * are stored or how transactions are parsed, then the validation is done
  * in this file. Finally, the functions in this file translate the return
  * values from wallet.c and transaction.c into response packets which are
  * sent over the stream device.
  *
  * This file is licensed as described by the file LICENCE.
  */

//#define NOBUTTONS

//#define FIXED_SEED_DANA
//#define FIXED_SEED_NIK
//#define DISPLAY_PARMS
#define DISPLAY_SEED

#define DANA_SEED_1 "neck spider van cherry pet timber twenty correct talent stone gentle rocket"
#define DANA_SEED_2 "lobster stage now all position come multiply endorse lounge stereo saddle shoot"
#define DANA_SEED_3 "cousin relax surround forest dismiss dune rent leader match expose off chat"
#define DANA_SEED_4 "defend ancient under into bachelor supply track liquid upgrade snake again system"
#define DANA_SEED_5 "sponsor only basic current strategy issue friend swing bone zoo cradle forum job bright clinic weapon issue tortoise couple supreme weapon grape latin mango"
#define DANA_SEED_6 "accident trigger measure live coast address enemy artwork grab season gossip parrot"
#define NIK_SEED_1 "unit fresh faculty syrup bicycle peanut rich cloud dry vault truck differ"



#define LINE_LENGTH  25

#include "arm/avr2arm.h"

#include "stream_comm.h"

#include "hexstring.h"
#include "ssp.h"
#include "arm/keypad_alpha.h"
//#include "arm/DueTimer/DueTimer.h"


#include "arm/main.h"


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "common.h"
#include "endian.h"
#include "hwinterface.h"
#include "wallet.h"
#include "bignum256.h"
#include "stream_comm.h"
#include "prandom.h"
#include "xex.h"
#include "ecdsa.h"
#include "storage_common.h"
#include "pb.h"
#include "pb_decode.h"
#include "pb_encode.h"
#include "messages.pb.h"
#include "sha256.h"
#include "arm/lcd_and_input.h"
#include "arm/eink.h"
#include "arm/hwinit.h"
#include "arm/avr2arm.h"
#include "Arduino.h"
#include "arm/BLE.h"
#include "arm/usart.h"
#include "arm/keypad_alpha.h"

#include "bip39_trez_prev.h"
#include "baseconv.h"

#if defined(__SAM3X8E__)
#define ENABLE_PIN2  28
#define BRTS_PIN2  25
#define BCTS_PIN2  24
#endif

#if defined(__SAM3A8C__)
#define ENABLE_PIN2  4
#define BRTS_PIN2  3
#define BCTS_PIN2  10
#endif

int ENABLE2 = ENABLE_PIN2;
int BRTS2 = BRTS_PIN2;
int BCTS2 = BCTS_PIN2;

// Prototypes for forward-referenced functions.
bool mainInputStreamCallback(pb_istream_t *stream, uint8_t *buf, size_t count);
bool mainOutputStreamCallback(pb_ostream_t *stream, const uint8_t *buf, size_t count);
static void writeFailureString(StringSet set, uint8_t spec);
bool hashFieldCallback(pb_istream_t *stream, const pb_field_t *field, void **arg);
bool checkStream(void);
static NOINLINE void getPublicKeyOnly(uint8_t *out_address, AddressHandle ah_root, AddressHandle ah_chain, AddressHandle ah_index);
bool writeSignaturesCallback(pb_ostream_t *stream, const pb_field_t *field, void * const *arg);
static bool passwordInterjection(bool setup);
bool getSignaturesCallback(void);
void showQRcode(AddressHandle ah_root4, AddressHandle ah_chain4, AddressHandle ah_index4);

bool hashFieldCallbackGeneric(pb_istream_t *stream, uint8_t *theHash, uint8_t *theMessage, int *length, bool do_write_big_endian);

uint32_t countExistingWallets(void);
bool checkWalletSlotIsNotEmpty(uint32_t slotToCheck);
void getAddressOnly(uint8_t *out_address3, AddressHandle ah_root3, AddressHandle ah_chain3, AddressHandle ah_index3);


char change_address[TEXT_ADDRESS_LENGTH] = {};
char *change_address_ptr_begin = &change_address;



static int walletLoadAttemptsCounter;

uint8_t message_slice_counter;

/** Maximum size (in bytes) of any protocol buffer message sent by functions
  * in this file. */
#define MAX_SEND_SIZE			40960

/** Because stdlib.h might not be included, NULL might be undefined. NULL
  * is only used as a placeholder pointer for translateWalletError() if
  * there is no appropriate pointer. */
#ifndef NULL
#define NULL ((void *)0) 
#endif // #ifndef NULL

/** Union of field buffers for all protocol buffer messages. They're placed
  * in a union to make memory access more efficient, since the functions in
  * this file only need to deal with one message at any one time. */
union MessageBufferUnion
{
	Initialize initialize;
	Features features;
	Ping ping;
	PingResponse ping_response;
	DeleteWallet delete_wallet;
	NewWallet new_wallet;
	AddressHandleExtended address_handle_extended;
//	NewWalletMnemonic new_wallet_mnemonic;
//	NewAddress new_address;
//	GetNumberOfAddresses get_number_of_addresses;
//	NumberOfAddresses number_of_addresses;
//	GetAddressAndPublicKey get_address_and_public_key;
	LoadWallet load_wallet;
	FormatWalletArea format_wallet_area;
//	ChangeEncryptionKey change_encryption_key;
	ChangeWalletName change_wallet_name;
	ListWallets list_wallets;
	Wallets wallets;
//	BackupWallet backup_wallet;
//	RestoreWallet restore_wallet;
	GetDeviceUUID get_device_uuid;
	DeviceUUID device_uuid;
	GetEntropy get_entropy;
//	GetMasterPublicKey get_master_public_key;
//	MasterPublicKey master_public_key;
	ResetLang reset_lang;
	ScanWallet scan_wallet;
	GetBulk get_bulk;
	SetBulk set_bulk;
	CurrentWalletXPUB current_wallet_xpub;
	SignatureCompleteData signatures;
	DisplayAddressAsQR display_address_as_qr;
	SetChangeAddressIndex set_change_address_index;
};



/** Determines the string that writeStringCallback() will write. */
struct StringSetAndSpec
{
	/** String set (see getString()) of string to be outputted. */
	StringSet next_set;
	/** String specifier (see getString()) of string to be outputted. */
	uint8_t next_spec;
};


/** The transaction hash of the most recently approved transaction. This is
  * stored so that if a transaction needs to be signed multiple times (eg.
  * if it has more than one input), the user doesn't have to approve every
  * one. */
static uint8_t prev_transaction_hash[32];
/** false means disregard #prev_transaction_hash, true means
  * that #prev_transaction_hash is valid. */
static bool prev_transaction_hash_valid;

/** Length of current packet's payload. */
static uint32_t payload_length;

/** Argument for writeStringCallback() which determines what string it will
  * write. Don't put this on the stack, otherwise the consequences of a
  * dangling pointer are less secure. */
static struct StringSetAndSpec string_arg;
/** Alternate copy of #string_arg, for when more than one string needs to be
  * written. */
static struct StringSetAndSpec string_arg_alt;
/** Current number of wallets; used for the listWalletsCallback() callback
  * function. */
static uint32_t number_of_wallets;
/** Pointer to bytes of entropy to send to the host; used for
  * the getEntropyCallback() callback function. */
static uint8_t *entropy_buffer;
/** Number of bytes of entropy to send to the host; used for
  * the getEntropyCallback() callback function. */
static size_t num_entropy_bytes;
/** Pointer to bytes of bulk to send to the host; used for
  * the getBulkCallback() callback function. */
static uint8_t *bulk_buffer;
/** Number of bytes of bulk to send to the host; used for
  * the getBulkCallback() callback function. */
static size_t num_bulk_bytes;

//static uint8_t *bulk_data;

/** Storage for fields of SignTransaction message. Needed for the
  * signTransactionCallback() callback function. */
static SignTransactionExtended sign_transaction_extended;

/** Storage for fields of SignMessage message. Needed for the
  * signMessageCallback() callback function. */
static SignMessage sign_message;


#define INPUTS_LIMITS			128

static AddressHandleExtended globalHandles[MAX_INPUTS] = {};

static uint32_t ahIndex;


//static uint8_t sig_hash_global[32];  //###################################################
static uint8_t sig_hash_global[MAX_INPUTS][32];

static SignatureCompleteData message_buffer_for_sigs[MAX_INPUTS];


/** Storage for fields of SignTransaction (HD) message. Needed for the
  * signTransactionCallbackHD() callback function. */
static uint8_t field_hash[32];
static uint8_t pin_field_hash[32];
/** Whether #field_hash has been set. */
static bool field_hash_set;
static bool pin_field_hash_set;

/** Storage for fields of SignTransaction (HD) message. Needed for the
  * signTransactionCallbackHD() callback function. */
static uint8_t temp_field_hash[32];
static uint8_t temp_pin_field_hash[32];
/** Whether #field_hash has been set. */
//static bool temp_field_hash_set;

/** Number of valid bytes in #session_id. */
static size_t session_id_length;
/** Arbitrary host-supplied bytes which are sent to the host to assure it that
  * a reset hasn't occurred. */
static uint8_t session_id[64];


/** nanopb input stream which uses mainInputStreamCallback() as a stream
  * callback. */
pb_istream_t main_input_stream = {&mainInputStreamCallback, NULL, 0, NULL};
/** nanopb output stream which uses mainOutputStreamCallback() as a stream
  * callback. */
pb_ostream_t main_output_stream = {&mainOutputStreamCallback, NULL, 0, 0, NULL};

#ifdef TEST_STREAM_COMM
/** When sending test packets, the OTP stored here will be used instead of
  * a generated OTP. This allows the test cases to be static. */
static char test_otp[OTP_LENGTH] = {'1', '2', '3', '4', '\0'};
#endif // #ifdef TEST_STREAM_COMM



void CharToByte(char* chars, byte* bytes, unsigned int count){
	unsigned int i;
	for(i = 0; i < count; i++)
    	bytes[i] = (byte)chars[i];
}

void ByteToChar(byte* bytes, char* chars, unsigned int count){
	unsigned int i;
	for(i = 0; i < count; i++)
    	 chars[i] = (char)bytes[i];
}


/** Read bytes from the stream.
  * \param buffer The byte array where the bytes will be placed. This must
  *               have enough space to store length bytes.
  * \param length The number of bytes to read.
  */
static void getBytesFromStream(uint8_t *buffer, uint8_t length)
{
	uint8_t i;

	for (i = 0; i < length; i++)
	{
		buffer[i] = streamGetOneByte();
	}
	payload_length -= length;
}

/** Write a number of bytes to the output stream.
  * \param buffer The array of bytes to be written.
  * \param length The number of bytes to write.
  */
static void writeBytesToStream(const uint8_t *buffer, size_t length)
{
	size_t i;

	for (i = 0; i < length; i++)
	{
		streamPutOneByte(buffer[i]);
	}
}

/** nanopb input stream callback which uses streamGetOneByte() to get the
  * requested bytes.
  * \param stream Input stream object that issued the callback.
  * \param buf Buffer to fill with requested bytes.
  * \param count Requested number of bytes.
  * \return true on success, false on failure (nanopb convention).
  */
bool mainInputStreamCallback(pb_istream_t *stream, uint8_t *buf, size_t count)
{
	size_t i;

	if (buf == NULL)
	{
		fatalError(); // this should never happen
	}
	for (i = 0; i < count; i++)
	{
		if (payload_length == 0)
		{
			// Attempting to read past end of payload.
			stream->bytes_left = 0;
			return false;
		}
		buf[i] = streamGetOneByte();
		payload_length--;
	}
	return true;
}

/** nanopb output stream callback which uses streamPutOneByte() to send a byte
  * buffer.
  * \param stream Output stream object that issued the callback.
  * \param buf Buffer with bytes to send.
  * \param count Number of bytes to send.
  * \return true on success, false on failure (nanopb convention).
  */
bool mainOutputStreamCallback(pb_ostream_t *stream, const uint8_t *buf, size_t count)
{
	writeBytesToStream(buf, count);
	return true;
}

/** Read but ignore #payload_length bytes from input stream. This will also
  * set #payload_length to 0 (if everything goes well). This function is
  * useful for ensuring that the entire payload of a packet is read from the
  * stream device.
  */
static void readAndIgnoreInput(void)
{
	if (payload_length > 0)
	{
		for (; payload_length > 0; payload_length--)
		{
			streamGetOneByte();
		}
	}
}

/** Receive a message from the stream #main_input_stream.
  * \param fields Field description array.
  * \param dest_struct Where field data will be stored.
  * \return false on success, true if a parse error occurred.
  */
static bool receiveMessage(const pb_field_t fields[], void *dest_struct)
{
	bool r;

	r = pb_decode(&main_input_stream, fields, dest_struct);
	// In order for the message to be considered valid, it must also occupy
	// the entire payload of the packet.
	if ((payload_length > 0) || !r)
	{
		readAndIgnoreInput();
		writeFailureString(STRINGSET_MISC, MISCSTR_INVALID_PACKET2);
		return true;
	}
	else
	{
		return false;
	}
}




/** Send a packet.
  * \param message_id The message ID of the packet.
  * \param fields Field description array.
  * \param src_struct Field data which will be serialised and sent.
  */
static void sendPacket(uint16_t message_id, const pb_field_t fields[], const void *src_struct)
{
	uint8_t buffer[4];
	pb_ostream_t substream;

#ifdef TEST_STREAM_COMM
	// From PROTOCOL, the current received packet must be fully consumed
	// before any response can be sent.
	assert(payload_length == 0);
#endif
	// Use a non-writing substream to get the length of the message without
	// storing it anywhere.
	substream.callback = NULL;
	substream.state = NULL;
	substream.max_size = MAX_SEND_SIZE;
	substream.bytes_written = 0;
	if (!pb_encode(&substream, fields, src_struct))
	{
		fatalError();
	}

	if(bluetooth_on==1)
	{
	    digitalWrite(BRTS2, LOW); //BRTS
	    delay(50);
	}


	// Send packet header.
//	GOTTA SLICE THIS UP INTO <200 BYTE PACKETS
	message_slice_counter = 0;
	streamPutOneByte('#');
	streamPutOneByte('#');
	streamPutOneByte((uint8_t)(message_id >> 8));
	streamPutOneByte((uint8_t)message_id);
	writeU32BigEndian(buffer, substream.bytes_written);
	writeBytesToStream(buffer, 4);
	// Send actual message.
	main_output_stream.bytes_written = 0;
	main_output_stream.max_size = substream.bytes_written;
	if (!pb_encode(&main_output_stream, fields, src_struct))
	{
		fatalError();
	}
	if(bluetooth_on==1)
	{
		delay(100);  // this should possibly be variable dependent on substream.bytes_written
		digitalWrite(BRTS2, HIGH); //BRTS
	}

}

/** nanopb field callback which will write the string specified by arg.
  * \param stream Output stream to write to.
  * \param field Field which contains the string.
  * \param arg Pointer to #StringSetAndSpec structure specifying the string
  *            to write.
  * \return true on success, false on failure (nanopb convention).
  */
bool writeStringCallback(pb_ostream_t *stream, const pb_field_t *field, void * const *arg)
{
	uint16_t i;
	uint16_t length;
	char c;
	struct StringSetAndSpec **ptr_arg_s;
	struct StringSetAndSpec *arg_s;

	ptr_arg_s = (struct StringSetAndSpec **)arg;
	if (ptr_arg_s == NULL)
	{
		fatalError(); // this should never happen
	}
	arg_s = *ptr_arg_s;
	if (arg_s == NULL)
	{
		fatalError(); // this should never happen
	}
	length = getStringLength(arg_s->next_set, arg_s->next_spec);
	if (!pb_encode_tag_for_field(stream, field))
	{
		return false;
	}
	// Cannot use pb_encode_string() because it expects a pointer to the
	// contents of an entire string; getString() does not return such a
	// pointer.
	if (!pb_encode_varint(stream, (uint64_t)length))
	{
		return false;
	}
	for (i = 0; i < length; i++)
	{
		c = getString(arg_s->next_set, arg_s->next_spec, i);
		if (!pb_write(stream, (uint8_t *)&c, 1))
		{
			return false;
		}
	}
	return true;
}

/** Sends a Failure message with the specified error message.
  * \param set See getString().
  * \param spec See getString().
  */
static void writeFailureString(StringSet set, uint8_t spec)
{
	Failure message_buffer;
	uint32_t code;

	string_arg.next_set = set;
	string_arg.next_spec = spec;
	code = (uint32_t)spec & 0xffff;
	code |= ((uint32_t)set & 0xffff) << 16;
	message_buffer.error_code = code;
	message_buffer.error_message.funcs.encode = &writeStringCallback;
	message_buffer.error_message.arg = &string_arg;
	sendPacket(PACKET_TYPE_FAILURE, Failure_fields, &message_buffer);
}


void sendSuccessPacketOnly(void)
{
	Success message_buffer;
	sendPacket(PACKET_TYPE_SUCCESS, Success_fields, &message_buffer);
}


/** Translates a return value from one of the wallet functions into a Success
  * or Failure response packet which is written to the stream.
  * \param r The return value from the wallet function.
  */
static void translateWalletError(WalletErrors r)
{
	Success message_buffer;

	if (r == WALLET_NO_ERROR)
	{
		writeCheck_Screen();
//		showReady();
		sendPacket(PACKET_TYPE_SUCCESS, Success_fields, &message_buffer);
//		_delay_ms(200);
		// TODO: this should really use a callback function
	}
	else
	{
		writeX_Screen();
		delay(1000);
		showReady();
		writeFailureString(STRINGSET_WALLET, (uint8_t)r);

	}
}

/** Translates a return value from one of the wallet functions into a Success
  * or Failure response packet which is written to the stream.
  * \param r The return value from the wallet function.
  */
static void translateWalletErrorDisplayQR(WalletErrors r)
{
	Success message_buffer;

	if (r == WALLET_NO_ERROR)
	{
		writeCheck_Screen();
		showQRcode(0,0,0);
		sendPacket(PACKET_TYPE_SUCCESS, Success_fields, &message_buffer);
//		_delay_ms(200);
		// TODO: this should really use a callback function
	}
	else
	{
		writeX_Screen();
		delay(1000);
		showReady();
		writeFailureString(STRINGSET_WALLET, (uint8_t)r);

	}
}

/** Translates a return value from one of the wallet functions into a Success
  * or Failure response packet which is written to the stream.
  * \param r The return value from the wallet function.
  */
static void translateWalletErrorDisplayShowReady(WalletErrors r)
{
	Success message_buffer;

	if (r == WALLET_NO_ERROR)
	{
		writeCheck_Screen();
		showReady();
		sendPacket(PACKET_TYPE_SUCCESS, Success_fields, &message_buffer);
//		_delay_ms(200);
		// TODO: this should really use a callback function
	}
	else
	{
		writeX_Screen();
		delay(1000);
		showReady();
		writeFailureString(STRINGSET_WALLET, (uint8_t)r);

	}
}

bool checkStream(void)
{
	uint8_t buffer[1];
//	char temp;
	bool toReturn;
	getBytesFromStream(buffer, 1);

	switch(buffer[0]){
	    case '#'  :
			getBytesFromStream(buffer, 1);
	    	if (buffer[0] == '#') toReturn = false;
	    	break;
	    case '~'  :
			getBytesFromStream(buffer, 1);
			if (buffer[0] == '~'){
				usartSpew();
				toReturn = true;
			}
			break;
	    default : /* Optional */
	       toReturn = true;
	}
	return toReturn;

//	if (buffer[0] != '#')
//	{
//		return true;
//	}
//	else{
//		temp = buffer[0];
//		getBytesFromStream(buffer, 1);
//		if (temp != '#' || buffer[0] != '#'){
//			return true;
//		}
//	}
//	return false;


}

/** Receive packet header.
  * \return Message ID (i.e. command type) of packet.
  */
static uint16_t receivePacketHeader(void)
{
//	writeEinkDisplay("in receivePacketHeader", false, COL_1_X, LINE_0_Y, "",false,5,30, "",false,5,50, "",false,5,70, "",false,0,0);
	uint8_t buffer[4];
	uint16_t message_id;

//	startTimer1();

	do {
		if(moofOn){
//			writeSleepScreen();
//			deactivateBLE();
//			delay(5000);
//			pmc_enable_backupmode();
			;;
		}
		;;
	} while ( checkStream() );

//	stopTimer1();

//  original header check:
//	getBytesFromStream(buffer, 2);
//	if ((buffer[0] != '#') || (buffer[1] != '#'))
//	{
//		fatalError(); // invalid header
//	}
	getBytesFromStream(buffer, 2);
	message_id = (uint16_t)(((uint16_t)buffer[0] << 8) | ((uint16_t)buffer[1]));
	getBytesFromStream(buffer, 4);
	payload_length = readU32BigEndian(buffer);
	// TODO: size_t not generally uint32_t
	main_input_stream.bytes_left = payload_length;
	return message_id;
//	}
}

/** Begin ButtonRequest interjection. This asks the host whether it is okay
  * to prompt the user and wait for a button press.
  * \param command The action to ask the user about. See #AskUserCommandEnum.
  * \return false if the user accepted, true if the user or host denied.
  */
bool buttonInterjection(AskUserCommand command)
{
	uint16_t message_id;
	ButtonRequest button_request;
	ButtonAck button_ack;
	ButtonCancel button_cancel;
	bool receive_failure;

			if (userDenied(command))
			{
				writeFailureString(STRINGSET_MISC, MISCSTR_PERMISSION_DENIED_USER);
				return true;
			}
			else
			{
				return false;
			}
}


bool buttonInterjectionSetup(AskUserCommand command)
{
	uint16_t message_id;
	ButtonRequest button_request;
	ButtonAck button_ack;
	ButtonCancel button_cancel;
	bool receive_failure;

			if (userDeniedSetup(command))
			{
				writeFailureString(STRINGSET_MISC, MISCSTR_PERMISSION_DENIED_USER);
				return true;
			}
			else
			{
				return false;
			}
}


/** Begin ButtonRequest interjection. This asks the host whether it is okay
  * to prompt the user and wait for a button press.
  * \param command The action to ask the user about. See #AskUserCommandEnum.
  * \return false if the user accepted, true if the user or host denied.
  */
bool buttonInterjectionNoAck(AskUserCommand command)
{
	uint16_t message_id;
	ButtonRequest button_request;
	ButtonAck button_ack;
	ButtonCancel button_cancel;
	bool receive_failure;

			if (userDenied(command))
			{
//				writeFailureString(STRINGSET_MISC, MISCSTR_PERMISSION_DENIED_USER);
				return true;
			}
			else
			{
				return false;
			}
}

/** Begin ButtonRequest interjection. This asks the host whether it is okay
  * to prompt the user and wait for a button press.
  * \param command The action to ask the user about. See #AskUserCommandEnum.
  * \return false if the user accepted, true if the user or host denied.
  */
bool buttonInterjectionNoAckSetup(AskUserCommand command)
{
	uint16_t message_id;
	ButtonRequest button_request;
	ButtonAck button_ack;
	ButtonCancel button_cancel;
	bool receive_failure;

			if (userDeniedSetup(command))
			{
//				writeFailureString(STRINGSET_MISC, MISCSTR_PERMISSION_DENIED_USER);
				return true;
			}
			else
			{
				return false;
			}
}


/** Begin ButtonRequest interjection. This asks the host whether it is okay
  * to prompt the user and wait for a button press.
  * \param command The action to ask the user about. See #AskUserCommandEnum.
  * \return false if the user accepted, true if the user or host denied.
  */
bool buttonInterjectionNoAckPlusData(AskUserCommand command, char *the_data, int size_of_data)
{
	uint16_t message_id;
	ButtonRequest button_request;
	ButtonAck button_ack;
	ButtonCancel button_cancel;
	bool receive_failure;

			if (userDeniedPlusData(command, the_data, size_of_data))
			{
//				writeFailureString(STRINGSET_MISC, MISCSTR_PERMISSION_DENIED_USER);
				return true;
			}
			else
			{
				return false;
			}
}



/** Begin ButtonRequest interjection. This asks the host whether it is okay
  * to prompt the user and wait for a button press.
  * \param command The action to ask the user about. See #AskUserCommandEnum.
  * \return false if the user accepted, true if the user or host denied.
  */
char *inputInterjectionNoAck(AskUserCommand command)
{
	uint16_t message_id;
	ButtonRequest button_request;
	ButtonAck button_ack;
	ButtonCancel button_cancel;
	bool receive_failure;
	char *r;

	r = userInput(command);
	if(r == 'N')
	{
		writeFailureString(STRINGSET_MISC, MISCSTR_PERMISSION_DENIED_USER);
	}
	return r;

}



/** Begin PasswordRequest interjection.
 * The device should prompt the user to enter the password
 * for the current wallet on the device keypad
  * If the device does submit a password, then #field_hash_set
  * will be set and #field_hash updated.
  * \return false if the device submitted a password, true on error.
  */
static bool passwordInterjection(bool setup)
{
	HashState *sig_hash_hs_ptr2;
	HashState sig_hash_hs2;
	bool displayAlpha;
	displayAlpha = false;

	sig_hash_hs_ptr2 = &sig_hash_hs2;

	uint8_t j;


	char bufferPIN1array[21]={};

	char *bufferPIN1;

	bufferPIN1 = &bufferPIN1array[0];

	buttonInterjectionNoAck(ASKUSER_ENTER_WALLET_PIN);

	bufferPIN1 = getInputWallets(displayAlpha, setup);
	clearDisplay();

	// Host has just sent password.
	field_hash_set = false;
	memset(field_hash, 0, sizeof(field_hash));


	sha256Begin(sig_hash_hs_ptr2);

	for (j=0;j<20;j++)
	{
		sha256WriteByte(sig_hash_hs_ptr2, bufferPIN1[j]);
	}
	sha256FinishDouble(sig_hash_hs_ptr2);
	writeHashToByteArray(field_hash, sig_hash_hs_ptr2, false);
	field_hash_set = true;

	if (!field_hash_set)
	{
		fatalError(); // should never happen since password is a required field
	}
	return false;
}


bool passwordInterjectionAutoPIN(int level)
{
	bool approved;
	bool approved2;
	bool approved3;
	bool approved4;
	bool approved5;
	bool permission_denied;

	approved = false;
	approved2 = false;
	approved3 = false;
	approved4 = false;
	approved5 = false;


	uint16_t message_id;
	PinRequest pin_request;
	PinAck pin_ack;
	PinCancel pin_cancel;
	bool receive_failure;
	HashState *sig_hash_hs_ptr2;
	HashState sig_hash_hs2;
	HashState *sig_hash_hs_ptr4;
	HashState sig_hash_hs4;

	sig_hash_hs_ptr2 = &sig_hash_hs2;
	sig_hash_hs_ptr4 = &sig_hash_hs4;

	uint8_t j;
	uint8_t *hash2_ptr;
	uint8_t hash2[32] = {};


	char bufferPIN1array[21]={};
	char bufferPIN2array[21]={};
	char bufferPIN3array[21]={};
	char bufferPIN4array[21]={};

	char *bufferPIN1;
	char *bufferPIN2;
	char *bufferPIN3;
	char *bufferPIN4;

	bufferPIN1 = &bufferPIN1array[0];
	bufferPIN2 = &bufferPIN2array[0];
	bufferPIN3 = &bufferPIN3array[0];
	bufferPIN4 = &bufferPIN4array[0];


	char rChar;
	int r;
	bool yesOrNo;



    if(level==1)
    {

    	buttonInterjectionNoAckSetup(ASKUSER_DESCRIBE_STANDARD_SETUP_2_WALLET);

		rChar = waitForNumberButtonPress4to8();
		clearDisplay();
		r = rChar - '0';

		if(rChar == 'N')
		{
			return true;
		}else
		{
			generateInsecurePIN(bufferPIN1, r+1);

			buttonInterjectionNoAckPlusData(ASKUSER_SET_WALLET_PIN_BIG, bufferPIN1, r);

			yesOrNo = waitForButtonPress();
			clearDisplay();
			if(!yesOrNo)
			{
				// Host has just sent password.
				field_hash_set = false;
				memset(field_hash, 0, sizeof(field_hash));


				sha256Begin(sig_hash_hs_ptr2);

				for (j=0;j<20;j++)
				{
					sha256WriteByte(sig_hash_hs_ptr2, bufferPIN1[j]);
				}
				sha256FinishDouble(sig_hash_hs_ptr2);
				writeHashToByteArray(field_hash, sig_hash_hs_ptr2, false);
				field_hash_set = true;
	//			waitForButtonPress();
				showWorking();

				if (!field_hash_set)
				{
					fatalError(); // should never happen since password is a required field
				}
				return false;

			}else if(yesOrNo)
			{
				bool result1;
				result1 = passwordInterjectionAutoPIN(level);
				return result1;
			}
		}

    }else if (level==2)
    {
    	buttonInterjectionNoAckSetup(ASKUSER_DESCRIBE_ADVANCED_SETUP_2_WALLET);

		yesOrNo = waitForButtonPress();
		clearDisplay();
		if(!yesOrNo)
		{
			buttonInterjectionNoAckSetup(ASKUSER_ALPHA_INPUT_PREFACE);

			bufferPIN1 = getInputWallets(true, true);
			clearDisplay();
		}else if(yesOrNo)
		{
			bool result2;
			result2 = passwordInterjectionAutoPIN(1);
			return result2;
		}

		buttonInterjectionNoAckPlusData(ASKUSER_SET_WALLET_PIN, bufferPIN1, 0);


		yesOrNo = waitForButtonPress();
		clearDisplay();

		if(!yesOrNo)
		{
			field_hash_set = false;
			memset(field_hash, 0, sizeof(field_hash));

			sha256Begin(sig_hash_hs_ptr2);

			for (j=0;j<PIN_MAX_SIZE;j++)
			{
				sha256WriteByte(sig_hash_hs_ptr2, bufferPIN1[j]);
			}
			sha256FinishDouble(sig_hash_hs_ptr2);
			writeHashToByteArray(field_hash, sig_hash_hs_ptr2, false);
			field_hash_set = true;

			if (!field_hash_set)
			{
				fatalError(); // should never happen since password is a required field
			}
			return false;
		}else if(yesOrNo){
			bool result3;
			result3 = passwordInterjectionAutoPIN(level);
			return result3;
		}


    }else if (level==3)
    {

    	// SET WALLET PIN
    	buttonInterjectionNoAckSetup(ASKUSER_DESCRIBE_EXPERT_SETUP_2_WALLET);


		yesOrNo = waitForButtonPress();
		clearDisplay();
		if(!yesOrNo)
		{
			buttonInterjectionNoAckSetup(ASKUSER_ALPHA_INPUT_PREFACE);

			bufferPIN1 = getInputWallets(true, true);
			clearDisplay();
		}else if(yesOrNo)
		{
			bool result4;
			result4 = passwordInterjectionAutoPIN(2);
			return result4;
		}

		buttonInterjectionNoAckPlusData(ASKUSER_SET_WALLET_PIN, bufferPIN1, 0);


		yesOrNo = waitForButtonPress();
		clearDisplay();

		if(!yesOrNo)
		{
			field_hash_set = false;
			memset(field_hash, 0, sizeof(field_hash));

			sha256Begin(sig_hash_hs_ptr2);

			for (j=0;j<PIN_MAX_SIZE;j++)
			{
				sha256WriteByte(sig_hash_hs_ptr2, bufferPIN1[j]);
			}
			sha256FinishDouble(sig_hash_hs_ptr2);
			writeHashToByteArray(field_hash, sig_hash_hs_ptr2, false);
			field_hash_set = true;

			if (!field_hash_set)
			{
				fatalError(); // should never happen since password is a required field
			}
		}else if(yesOrNo){
			bool result5;
			result5 = passwordInterjectionAutoPIN(level);
			return result5;
		}

    	// SET TRANSACTION PIN
		permission_denied = buttonInterjectionNoAckSetup(ASKUSER_SET_TRANSACTION_PIN);
		if (!permission_denied)
		{
			// User approved action.
			approved = true;
		}

		if(approved)
		{
			buttonInterjectionNoAckSetup(ASKUSER_ALPHA_INPUT_PREFACE);
			bufferPIN4 = getInputWallets(true, true);
			clearDisplay();

			permission_denied =buttonInterjectionNoAckPlusData(ASKUSER_TRANSACTION_PIN_SET, bufferPIN4, 0);
			if (!permission_denied)
			{
				// User approved action.
				approved2 = true;
			}

			if(approved2)
			{
				pin_field_hash_set = false;
				memset(pin_field_hash, 0, sizeof(pin_field_hash));

				sha256Begin(sig_hash_hs_ptr4);

				for (j=0;j<PIN_MAX_SIZE;j++)
				{
					sha256WriteByte(sig_hash_hs_ptr4, bufferPIN4[j]);
				}
				sha256FinishDouble(sig_hash_hs_ptr4);
				writeHashToByteArray(pin_field_hash, sig_hash_hs_ptr4, false);
				pin_field_hash_set = true;


				if (!pin_field_hash_set)
				{
					fatalError(); // should never happen since password is a required field
				}
				return false;
			}else
			{
				pin_field_hash_set = false;
				bool result7;
				result7 = passwordInterjectionAutoPIN(level);
				return result7;
			}
		}else
		{
			bool result6 = false;
//			result6 = passwordInterjectionAutoPIN(level);
			pin_field_hash_set = false;
			return result6;
		}
    }    return false;
}


/** Begin OtpRequest interjection. This asks the host to submit a one-time
  * password that is displayed on the device.
  * \return false if the host submitted a matching password, true on error.
  */
static bool otpInterjection(AskUserCommand command)
{
	uint16_t message_id;
	OtpRequest otp_request;
	OtpAck otp_ack;
	OtpCancel otp_cancel;
	bool receive_failure;
	char otp[OTP_LENGTH];

	generateInsecureOTP(otp);
	displayOTP(command, otp);
	memset(&otp_request, 0, sizeof(otp_request));
	sendPacket(PACKET_TYPE_OTP_REQUEST, OtpRequest_fields, &otp_request);
	message_id = receivePacketHeader();
	clearOTP();
	if (message_id == PACKET_TYPE_OTP_ACK)
	{
		// Host has just sent OTP.
		memset(&otp_ack, 0, sizeof(otp_ack));
		receive_failure = receiveMessage(OtpAck_fields, &otp_ack);
		if (receive_failure)
		{
			return true;
		}
		else
		{
			if (memcmp(otp, otp_ack.otp, MIN(OTP_LENGTH, sizeof(otp_ack.otp))))
			{
				writeFailureString(STRINGSET_MISC, MISCSTR_OTP_MISMATCH);
				return true;
			}
			else
			{
				return false;
			}
		}
	}
	else if (message_id == PACKET_TYPE_OTP_CANCEL)
	{
		// Host does not want to send OTP.
		receive_failure = receiveMessage(OtpCancel_fields, &otp_cancel);
		if (!receive_failure)
		{
			writeFailureString(STRINGSET_MISC, MISCSTR_PERMISSION_DENIED_HOST);
		}
		return true;
	}
	else
	{
		// Unexpected message.
		readAndIgnoreInput();
		writeFailureString(STRINGSET_MISC, MISCSTR_UNEXPECTED_PACKET);
		return true;
	}
}



/** reads a succession of address handle structures and sets the results in the global address handles
 * 	Necessary to pass in the AH of transactions into the parsing function, which can then grab the related private key
 */
bool read_AHE_data(pb_istream_t *stream, const pb_field_t *field, void **arg)
{
	while (stream->bytes_left)
    {
        if(!pb_decode(stream, AddressHandleExtended_fields, &globalHandles[ahIndex]))
    		if (PB_GET_ERROR(stream)=="end-of-stream"){
    				stream->bytes_left = 0;
    			return false;
    		}
    }

	ahIndex++;


    return true;
}


bool writeSignaturesCallback(pb_ostream_t *stream, const pb_field_t *field, void * const *arg)
{
	uint32_t i;

	for (i = 0; i < ahIndex; i++)
	{
//		message_buffer_for_sigs[i].signature_data_complete.size = 109;

		if (!pb_encode_tag_for_field(stream, field))
		{
			return false;
		}
		if (!pb_encode_submessage(stream, SignatureCompleteData_fields, &message_buffer_for_sigs[i]))
		{
			return false;
		}
	}
	return true;
}

bool checkTransactionPINexpertMode()
{
		char bufferPIN3array[21]={};
		char *userInputTransactionPIN;
		userInputTransactionPIN = &bufferPIN3array[0];
		memset(userInputTransactionPIN, 0, 21);
		strcpy(userInputTransactionPIN, "");



		uint8_t *pointerToReturnedTransPINHash;


//				from user
		userInputTransactionPIN = getTransactionPINfromUser();
		showWorking();

		uint8_t k;
		HashState *sig_hash_hs_ptr3;
		HashState sig_hash_hs3;
		uint8_t userInputhash3[32] = {};

		sig_hash_hs_ptr3 = &sig_hash_hs3;

		sha256Begin(sig_hash_hs_ptr3);

		for (k=0;k<20;k++)
		{
			sha256WriteByte(sig_hash_hs_ptr3, userInputTransactionPIN[k]);
		}
		sha256FinishDouble(sig_hash_hs_ptr3);
		writeHashToByteArray(userInputhash3, sig_hash_hs_ptr3, false);



//				from wallet record
		pointerToReturnedTransPINHash = getTransactionPINhash();

		uint8_t check = 1;
		check = memcmp(pointerToReturnedTransPINHash,userInputhash3,32);

		if(check == 0)
		{
//					writeEinkDisplay("trans pin GOOD", false, 5, 40, "",false,0,10, "",false,0,60, "",false,0,0, "",false,0,0);
			int wrongTransactionPINCount = fetchTransactionPINWrongCount();
			if(wrongTransactionPINCount != 0)
			{
				uint8_t temp1[1];
				temp1[0] = 0;
				nonVolatileWrite(temp1, WRONG_TRANSACTION_PIN_COUNT_ADDRESS, 1);
			}
			return true;
		}else
		{
//					writeEinkDisplay("trans pin BAD", false, 5, 40, "",false,0,10, "",false,0,60, "",false,0,0, "",false,0,0);
			int wrongTransactionPINCount = fetchTransactionPINWrongCount();
			wrongTransactionPINCount++;
			int s = wrongTransactionPINCount;
			uint8_t temp1[1];
			temp1[0] = s;
			nonVolatileWrite(temp1, WRONG_TRANSACTION_PIN_COUNT_ADDRESS, 1);
			writeX_Screen();
			Software_Reset();
		}
	return false;

}

bool signTransactionCompleteCallback(pb_istream_t *stream, const pb_field_t *field, void **arg)
{
	bool trans_pin_set = false;
	bool approved;
	bool permission_denied;
	TransactionErrors r;
	WalletErrors wallet_return;
	uint8_t transaction_hash[32];
	uint8_t sig_hash[ahIndex][32]; //index of ahIndex
	uint8_t blank_hash[32] = {};
	uint8_t message_builder[ahIndex][109];
	uint32_t ahIndexIn = 0;


//	writeEinkDisplay(change_address_ptr_begin, false, 10, 10, "",false,10,30, "",false,10,50, "",false,10,70, "",false,0,0);

	// Validate transaction and calculate hashes of it.
	clearOutputsSeen();

	ahIndexIn = ahIndex;

	r = parseTransaction(sig_hash, transaction_hash, stream->bytes_left, ahIndexIn, change_address_ptr_begin);
	// parseTransaction() always reads transaction_length bytes, even if parse
	// errors occurs. These next two lines are a bit of a hack to account for
	// differences between streamGetOneByte() and pb_read(stream, buf, 1).
	// The intention is that transaction.c doesn't have to know anything about
	// protocol buffers.

	payload_length -= stream->bytes_left;
	stream->bytes_left = 0;
	if (r != TRANSACTION_NO_ERROR)
	{
		writeFailureString(STRINGSET_TRANSACTION, (uint8_t)r);
		writeEinkDisplay("parse error", false, 10, 10, "",false,10,30, "",false,10,50, "",false,10,70, "",false,0,0);
		// Transaction parse error.
		return true;
	}

	// Get permission from user.
	approved = false;
	// Does transaction_hash match previous approved transaction?
	if (prev_transaction_hash_valid)
	{
		if (bigCompare(transaction_hash, prev_transaction_hash) == BIGCMP_EQUAL)
		{
			approved = false; //was true, set to false to always regenerate
		}
	}

	userDenied(ASKUSER_PREPARING_TRANSACTION);


	if (!approved)
	{
		// Need to explicitly get permission from user.
		// The call to parseTransaction() should have logged all the outputs
		// to the user interface.

		permission_denied = buttonInterjection(ASKUSER_SIGN_TRANSACTION);
		if (!permission_denied)
		{
			// User approved transaction.
			approved = true;
			memcpy(prev_transaction_hash, transaction_hash, 32);
			prev_transaction_hash_valid = true;

			trans_pin_set = hasTransactionPin();
			if(trans_pin_set)
			{
				checkTransactionPINexpertMode();
			}

#ifndef NOBUTTONS
		}else if(permission_denied)
		{
			approved = false;
			memcpy(prev_transaction_hash, 0, 32);
			prev_transaction_hash_valid = false;
			writeX_Screen();
			showReady();
			return true;
		}
#endif
	} // if (!approved)

	if (approved)
	{
		userDenied(ASKUSER_ASSEMBLING_HASHES);
		// Okay to sign transaction.
		uint32_t i;
		for(i=0; i<ahIndex; i++)
		{
			memcpy(sig_hash_global[i], sig_hash[i], 32);
		}

		getSignaturesCallback();

		userDenied(ASKUSER_SENDING_DATA);

		showReady();

		SignatureComplete message_buffer_local;
		message_buffer_local.signature_complete_data.funcs.encode = &writeSignaturesCallback;
		sendPacket(PACKET_TYPE_SIGNATURE_COMPLETE, SignatureComplete_fields, &message_buffer_local);
	}

	return true;
}



/** nanopb field callback for signature data of SignMessage message. This
  * does (or more accurately, delegates) all the "work" of message
  * signing: parsing the transaction, asking the user for approval, generating
  * the signature and sending the signature.
  * \param stream Input stream to read from.
  * \param field Field which contains the signature data.
  * \param arg Unused.
  * \return true on success, false on failure (nanopb convention).
  */
bool getSignaturesCallback(void)
{
	uint32_t i;
	const uint8_t ecdsa_length = 33;

	char ah_index_txt[16];
	sprintf(ah_index_txt,"%lu", (unsigned long)ahIndex);

	for (i = 0; i < ahIndex; i++)
	{
		char i_index_txt[16];
		sprintf(i_index_txt,"%lu", (unsigned long)i+1);

		initDisplay();
		writeEinkNoDisplaySingleBig(i_index_txt,17,30);
		writeEinkNoDisplaySingleBig("/",49,30);
		writeEinkNoDisplaySingleBig(ah_index_txt,81,30);
//		writeEinkDisplay("SIGNING INPUT", false, 5, 40, i_index_txt,false,117,40, "of",false,133,40, ah_index_txt,false,157,40, "",false,0,0);
		display();

		uint8_t private_key[32] = {};
		uint8_t signature_temp_holding[MAX_SIGNATURE_LENGTH] = {};
		uint8_t ecdsa_address[33] = {};
//		uint8_t ecdsa_address[33] = {0x02,0x37,0x11,0x66,0x37,0x83,0xd0,0x8a,0x8e,0xdc,0x7d,0x80,0x41,0x99,0xe4,0xc8,0xda,0x5d,0xbe,0x91,0xda,0xbe,0x8b,0x75,0x6e,0x61,0x2d,0xb4,0xed,0x9c,0x4f,0x3b,0x50};
		uint8_t message_builder[109] = {};
		uint8_t signature_length = 0;
		uint8_t total_length = 0;
		uint8_t padding_size = 0;
		uint8_t ecdsa_begin;
		uint8_t j;
		uint8_t k;
		uint8_t m;
		uint8_t ecdsa_end;


		if (getPrivateKeyExtended(private_key, globalHandles[i].address_handle_root, globalHandles[i].address_handle_chain, globalHandles[i].address_handle_index) == WALLET_NO_ERROR)
//		if (1)
		{
			if (signTransaction(signature_temp_holding, &signature_length, sig_hash_global[i], private_key, true))
//			if (signTransaction(signature_temp_holding, &signature_length, sig_hash_global[i], static_private_key, true))
			{
				translateWalletError(WALLET_RNG_FAILURE);
			}
			else
			{
//				set to static for testing purposes

				getPublicKeyOnly(ecdsa_address, globalHandles[i].address_handle_root, globalHandles[i].address_handle_chain, globalHandles[i].address_handle_index);

				total_length = signature_length + ecdsa_length + 2;
				padding_size = 109 - total_length;

//				The following should ALWAYS be 109 bytes
				message_builder[0] = total_length;
				message_builder[1] = signature_length;

				for (j=0; j < signature_length; j++)
				{
					message_builder[2+j] = signature_temp_holding[j];
				}
//				now at 1+j in the message
				ecdsa_begin = 2 + signature_length;
				message_builder[ecdsa_begin] = ecdsa_length;
				for (k=0; k < ecdsa_length; k++)
				{
					message_builder[ecdsa_begin+1+k] = ecdsa_address[k];
				}
				ecdsa_end = ecdsa_begin+1+k;
				for (m=0; m < padding_size; m++)
				{
					message_builder[ecdsa_end + 1 + m] = 0;
				}

				message_buffer_for_sigs[i].signature_data_complete.size = sizeof(message_builder)*sizeof(uint8_t);
				memcpy(message_buffer_for_sigs[i].signature_data_complete.bytes, message_builder, sizeof(message_builder)*sizeof(uint8_t));

			}
		}

	}
	return true;
}


//static uint8_t line_index;

bool signMessageCallback(pb_istream_t *stream, const pb_field_t *field, void **arg)
{
	bool trans_pin_set = false;
	AddressHandle ah_root;
	AddressHandle ah_chain;
	AddressHandle ah_index;
	bool approved;
	bool permission_denied;
	TransactionErrors r;
	WalletErrors wallet_return;
	uint8_t line_index;

	uint8_t private_key[32];
	uint8_t signature_length;

	SignatureMessage message_buffer;

	uint8_t messageHash[32] = {};
	uint8_t messageData[652] = {};
	int messageRawLength = 0;
	int *messageRawLengthPtr = &messageRawLength;
	int messageTrimmedLength = 0;

	hashFieldCallbackGeneric(stream, messageHash, messageData, messageRawLengthPtr, false);

	messageRawLength = *messageRawLengthPtr;

//	writeEinkDisplayNumberSingleBig(messageRawLength,5,40);

//	Slice off the first 25 bytes, that's just the header
	messageTrimmedLength = messageRawLength - 26;

	int lines = 0;
	char lineToDisplay[25][LINE_LENGTH] = {};
	int leftOver = 0;
	char messageSub[626] = {};
	int h, k, l;
	int m = 0;


	if(messageTrimmedLength<LINE_LENGTH)
	{
		leftOver = messageTrimmedLength;
	}else
	{
		leftOver = messageTrimmedLength % LINE_LENGTH;
	}
//	writeEinkDisplayNumberSingleBig(leftOver,5,5);


	for(h=0; h<messageTrimmedLength; h++)
	{
		messageSub[h] = messageData[h+26];
	}

	lines = (messageTrimmedLength + (LINE_LENGTH-1))/LINE_LENGTH;

	if((messageTrimmedLength + (LINE_LENGTH-1))%LINE_LENGTH != 0){
		lines++;
		m = 0;
	}else{
		m = 1;
	}

//	writeEinkDisplayNumberSingleBig(lines,5,40);

	for(k=0; k<lines; k++)
	{
		if(k==(lines-1) && m==0)
		{
			for(l=0; l<leftOver; l++)
			{
				lineToDisplay[k][l] = messageSub[l+(k*(LINE_LENGTH-1))];
			}
			lineToDisplay[k][leftOver] = '\0';
		}else
		{
			for(l=0; l<LINE_LENGTH-1; l++)
			{
				lineToDisplay[k][l] = messageSub[l+(k*(LINE_LENGTH-1))];
			}
			lineToDisplay[k][LINE_LENGTH-1] = '\0';
		}
	}

	buttonInterjectionNoAck(ASKUSER_PRE_SIGN_MESSAGE);
	showWorking();

	initDisplay();
	writeEinkNoDisplaySingle(lineToDisplay[0],COL_1_X, LINE_0_Y);
	writeEinkNoDisplaySingle(lineToDisplay[1],COL_1_X, LINE_1_Y);
	writeEinkNoDisplaySingle(lineToDisplay[2],COL_1_X, LINE_2_Y);
	writeEinkNoDisplaySingle(lineToDisplay[3],COL_1_X, LINE_3_Y);
	writeEinkNoDisplaySingle(lineToDisplay[4],COL_1_X, LINE_4_Y);
	display();
	waitForButtonPress();
	clearDisplay();

	if(lines > 5)
	{
		initDisplay();
		writeEinkNoDisplaySingle(lineToDisplay[5],COL_1_X, LINE_0_Y);
		writeEinkNoDisplaySingle(lineToDisplay[6],COL_1_X, LINE_1_Y);
		writeEinkNoDisplaySingle(lineToDisplay[7],COL_1_X, LINE_2_Y);
		writeEinkNoDisplaySingle(lineToDisplay[8],COL_1_X, LINE_3_Y);
		writeEinkNoDisplaySingle(lineToDisplay[9],COL_1_X, LINE_4_Y);
		display();
		waitForButtonPress();
		clearDisplay();
	}

	if(lines > 10)
	{
		initDisplay();
		writeEinkNoDisplaySingle(lineToDisplay[10],COL_1_X, LINE_0_Y);
		writeEinkNoDisplaySingle(lineToDisplay[11],COL_1_X, LINE_1_Y);
		writeEinkNoDisplaySingle(lineToDisplay[12],COL_1_X, LINE_2_Y);
		writeEinkNoDisplaySingle(lineToDisplay[13],COL_1_X, LINE_3_Y);
		writeEinkNoDisplaySingle(lineToDisplay[14],COL_1_X, LINE_4_Y);
		display();
		waitForButtonPress();
		clearDisplay();
	}
	if(lines > 15)
	{
		initDisplay();
		writeEinkNoDisplaySingle(lineToDisplay[15],COL_1_X, LINE_0_Y);
		writeEinkNoDisplaySingle(lineToDisplay[16],COL_1_X, LINE_1_Y);
		writeEinkNoDisplaySingle(lineToDisplay[17],COL_1_X, LINE_2_Y);
		writeEinkNoDisplaySingle(lineToDisplay[18],COL_1_X, LINE_3_Y);
		writeEinkNoDisplaySingle(lineToDisplay[19],COL_1_X, LINE_4_Y);
		display();
		waitForButtonPress();
		clearDisplay();
	}
	if(lines > 20)
	{
		initDisplay();
		writeEinkNoDisplaySingle(lineToDisplay[20],COL_1_X, LINE_0_Y);
		writeEinkNoDisplaySingle(lineToDisplay[21],COL_1_X, LINE_1_Y);
		writeEinkNoDisplaySingle(lineToDisplay[22],COL_1_X, LINE_2_Y);
		writeEinkNoDisplaySingle(lineToDisplay[23],COL_1_X, LINE_3_Y);
		writeEinkNoDisplaySingle(lineToDisplay[24],COL_1_X, LINE_4_Y);
		display();
		waitForButtonPress();
		clearDisplay();
	}
	showWorking();

	// Get permission from user.
	approved = false;

	if (!approved)
	{
		// Need to explicitly get permission from user.
		// The call to parseTransaction() should have logged all the outputs
		// to the user interface.
		permission_denied = buttonInterjection(ASKUSER_SIGN_MESSAGE);
		if (!permission_denied)
		{
			// User approved transaction.
			approved = true;
			trans_pin_set = hasTransactionPin();
			if(trans_pin_set)
			{
				checkTransactionPINexpertMode();
			}
		}
	} // if (!approved)
	if (approved)
	{
		// Okay to sign transaction.
		signature_length = 0;

		ah_root  = sign_message.address_handle_extended.address_handle_root;
		ah_chain = sign_message.address_handle_extended.address_handle_chain;
		ah_index = sign_message.address_handle_extended.address_handle_index;

		if (getPrivateKeyExtended(private_key, ah_root, ah_chain, ah_index) == WALLET_NO_ERROR)
		{
			if (sizeof(message_buffer.signature_data_complete_message.bytes) < MAX_SIGNATURE_LENGTH)
			{
				// This should never happen.
				fatalError();
			}
			if (signTransaction(message_buffer.signature_data_complete_message.bytes, &signature_length, messageHash, private_key, false))
			{
				translateWalletError(WALLET_RNG_FAILURE);
			}
			else
			{
				writeCheck_Screen();
				showReady();

				message_buffer.signature_data_complete_message.size = signature_length;
				sendPacket(PACKET_TYPE_SIGN_MESSAGE_RESPONSE, SignatureMessage_fields, &message_buffer);
			}
		}
		else
		{
			wallet_return = walletGetLastError();
			translateWalletError(wallet_return);
		}
	}


	return true;
}




/** Return a public key.
 * Used to get the public key for a particular address handle. Returns ecdsa only in compressed form
  * \param *out_address ecdsa key in compressed form will be written here
  * \param ah Address handle to use (if generate_new is false).
  */
void getPublicKeyOnly(uint8_t *out_address, AddressHandle ah_root, AddressHandle ah_chain, AddressHandle ah_index) // was static NOINLINE
{
//	uint8_t tempAddressTest[33] = {33,2,33,2,33,2,33,2,33,2,33,2,33,2,33,2,33,2,33,2,33,2,33,2,33,2,33,2,33,2,33,2,33};
	Address message_buffer2;
	PointAffine public_key2;
	WalletErrors r;
	AddressPubKey message_buffer_apk;
	message_buffer2.address.size = 20;
	uint8_t unusedAddress[20] = {};

	r = getAddressAndPublicKey(unusedAddress, &public_key2, ah_root, ah_chain, ah_index);

	if (r == WALLET_NO_ERROR)
	{
//		message_buffer2.address_handle = ah_index;
		// The format of public keys sent is compatible with
		// "SEC 1: Elliptic Curve Cryptography" by Certicom research, obtained
		// 15-August-2011 from: http://www.secg.org/collateral/sec1_final.pdf
		// section 2.3 ("Data Types and Conversions"). The document basically
		// says that integers should be represented big-endian and that a 0x04
		// should be prepended to indicate that the public key is
		// uncompressed.

		if ((public_key2.y[0])%2 == 0)
		{
			message_buffer_apk.public_key.bytes[0] = 0x02;
		}
		else
		{
			message_buffer_apk.public_key.bytes[0] = 0x03;
		}

//		message_buffer.public_key.bytes[0] = 0x04;
		swapEndian256(public_key2.x);
//		swapEndian256(public_key.y);
		memcpy(&(message_buffer_apk.public_key.bytes[1]), public_key2.x, 32);
//		memcpy(&(message_buffer.public_key.bytes[33]), public_key2.y, 32);
//		message_buffer.public_key.size = 65;
		message_buffer_apk.public_key.size = 33;
//		sendPacket(PACKET_TYPE_ADDRESS_PUBKEY, Address_fields, &message_buffer);
//		sendPacket(PACKET_TYPE_SUCCESS, Success_fields, &message_buffer);
//		writeEinkDisplay("a", false, 10, 10, "",false,0,0, "",false,0,0, "",false,0,0, "",false,0,0);

		memcpy(out_address, message_buffer_apk.public_key.bytes, 33);

//		memcpy(out_address, tempAddressTest, 33);
	}
	else
	{
		translateWalletError(r);
	} // end if (r == WALLET_NO_ERROR)
}


/** Return an address pub key
 * Used to get the public key for a particular address handle. Returns ecdsa only in compressed form
  * \param *out_address ecdsa key in compressed form will be written here
  * \param ah Address handle to use (if generate_new is false).
  */
void getAddressOnly(uint8_t *out_address3, AddressHandle ah_root3, AddressHandle ah_chain3, AddressHandle ah_index3)
{
//	uint8_t tempAddressTest[33] = {33,2,33,2,33,2,33,2,33,2,33,2,33,2,33,2,33,2,33,2,33,2,33,2,33,2,33,2,33,2,33,2,33};
	Address message_buffer3;
	PointAffine public_key3;
	WalletErrors r;
	AddressPubKey message_buffer_apk3;
	message_buffer3.address.size = 20;
	uint8_t unusedAddress3[20] = {};

	r = getAddressAndPublicKey(unusedAddress3, &public_key3, ah_root3, ah_chain3, ah_index3);

	if (r == WALLET_NO_ERROR)
	{
//		message_buffer2.address_handle = ah_index;
		// The format of public keys sent is compatible with
		// "SEC 1: Elliptic Curve Cryptography" by Certicom research, obtained
		// 15-August-2011 from: http://www.secg.org/collateral/sec1_final.pdf
		// section 2.3 ("Data Types and Conversions"). The document basically
		// says that integers should be represented big-endian and that a 0x04
		// should be prepended to indicate that the public key is
		// uncompressed.

		if ((public_key3.y[0])%2 == 0)
		{
			message_buffer_apk3.public_key.bytes[0] = 0x02;
		}
		else
		{
			message_buffer_apk3.public_key.bytes[0] = 0x03;
		}

//		message_buffer.public_key.bytes[0] = 0x04;
		swapEndian256(public_key3.x);
//		swapEndian256(public_key.y);
		memcpy(&(message_buffer_apk3.public_key.bytes[1]), public_key3.x, 32);
//		memcpy(&(message_buffer.public_key.bytes[33]), public_key2.y, 32);
//		message_buffer.public_key.size = 65;
		message_buffer_apk3.public_key.size = 33;
//		sendPacket(PACKET_TYPE_ADDRESS_PUBKEY, Address_fields, &message_buffer);
//		sendPacket(PACKET_TYPE_SUCCESS, Success_fields, &message_buffer);
//		writeEinkDisplay("a", false, 10, 10, "",false,0,0, "",false,0,0, "",false,0,0, "",false,0,0);

//		memcpy(out_address, message_buffer_apk.public_key.bytes, 33);
		memcpy(out_address3, unusedAddress3, 20);


//		memcpy(out_address, tempAddressTest, 33);
	}
	else
	{
		translateWalletError(r);
	} // end if (r == WALLET_NO_ERROR)
}



/** nanopb field callback which will write repeated WalletInfo messages; one
  * for each wallet on the device.
  * \param stream Output stream to write to.
  * \param field Field which contains the WalletInfo submessage.
  * \param arg Unused.
  * \return true on success, false on failure (nanopb convention).
  */
bool listWalletsCallback(pb_ostream_t *stream, const pb_field_t *field, void * const *arg)
{
	uint32_t versionLWC = 0;
	uint32_t i, k;
	uint32_t j = 0;
	WalletInfo message_buffer;
	WalletInfo message_buffer_2[105] = {};

//	writeEinkDisplayNumberSingleBig(number_of_wallets,5,40);

	for (i = 0; i < number_of_wallets; i++)
	{
		message_buffer.wallet_number = i;
		message_buffer.wallet_name.size = NAME_LENGTH;
		message_buffer.wallet_uuid.size = DEVICE_UUID_LENGTH;
		memcpy(message_buffer.wallet_uuid.bytes, 0, DEVICE_UUID_LENGTH);
		memcpy(message_buffer.wallet_name.bytes, 0, NAME_LENGTH);
		message_buffer.version = 0;
		versionLWC = 0;

		if (getWalletInfo(
			&versionLWC,
			message_buffer.wallet_name.bytes,
			message_buffer.wallet_uuid.bytes,
			i) != WALLET_NO_ERROR)
		{
			// It's too late to return an error message, so cut off the
			// array now.
			return true;
		}
		message_buffer.version = versionLWC;
		message_buffer.has_version = true;

		if (versionLWC != VERSION_NOTHING_THERE)
		{
			if (versionLWC == VERSION_UNENCRYPTED || versionLWC == VERSION_IS_ENCRYPTED)
			{
				message_buffer_2[j].wallet_number = message_buffer.wallet_number;
				message_buffer_2[j].wallet_name.size = NAME_LENGTH;
				message_buffer_2[j].wallet_uuid.size = DEVICE_UUID_LENGTH;
				message_buffer_2[j].version = message_buffer.version;
				message_buffer_2[j].has_version = message_buffer.has_version;
				memcpy(message_buffer_2[j].wallet_name.bytes, message_buffer.wallet_name.bytes, NAME_LENGTH);
				memcpy(message_buffer_2[j].wallet_uuid.bytes, message_buffer.wallet_uuid.bytes, DEVICE_UUID_LENGTH);
				j++;
			}
		}
	}

	for(k = 0; k < j; k++)
	{
		if (!pb_encode_tag_for_field(stream, field))
		{
			return false;
		}
		if (!pb_encode_submessage(stream, WalletInfo_fields, &message_buffer_2[k]))
		{
			return false;
		}
	}
	return true;
}



/** Returns the number of viewable (non-hidden) wallets that are currently initialized
 * \
 * \
 * \return uint32_t countedWallets
 */
uint32_t countExistingWallets(void)
{
	uint32_t version;
	uint32_t i;
	WalletInfo message_buffer_temp;

	uint32_t countedWallets = 0;

	for (i = 0; i < number_of_wallets; i++)
	{
		message_buffer_temp.wallet_number = i;
		message_buffer_temp.wallet_name.size = NAME_LENGTH;
		message_buffer_temp.wallet_uuid.size = DEVICE_UUID_LENGTH;

		if (getWalletInfo(
			&version,
			message_buffer_temp.wallet_name.bytes,
			message_buffer_temp.wallet_uuid.bytes,
			i) != WALLET_NO_ERROR)
		{
			// It's too late to return an error message, so cut off the
			// array now.
//			return 0;
		}
		message_buffer_temp.version = version;
		message_buffer_temp.has_version = true;

		if (version != VERSION_NOTHING_THERE)
		{
			countedWallets++;
		}
	}
	return countedWallets;
}

/** Checks if there is a wallet already in a particular slot.
 * NOTE, will FALSE even if the slot contains a hidden wallet.
 * \param slotToCheck 	The wallet slot to check.
 * \return true on success, false on failure (nanopb convention).
 */
bool checkWalletSlotIsNotEmpty(uint32_t slotToCheck)
{
	uint32_t version;
	WalletInfo message_buffer_temp;

	message_buffer_temp.wallet_number = slotToCheck;
	message_buffer_temp.wallet_name.size = NAME_LENGTH;
	message_buffer_temp.wallet_uuid.size = DEVICE_UUID_LENGTH;

	if (getWalletInfo(
		&version,
		message_buffer_temp.wallet_name.bytes,
		message_buffer_temp.wallet_uuid.bytes,
		slotToCheck) != WALLET_NO_ERROR)
	{
		// It's too late to return an error message, so cut off the
		// array now.
		return true;
	}
	message_buffer_temp.version = version;
	message_buffer_temp.has_version = true;

	if (version != VERSION_NOTHING_THERE)
	{
		return true;
	}else if(version == VERSION_NOTHING_THERE)
	{
		return false;
	}
	// Just in case, as we never want to overwrite by mistake
	return true;
}

/** nanopb field callback which will write out the contents
  * of #entropy_buffer.
  * \param stream Output stream to write to.
  * \param field Field which contains the the entropy bytes.
  * \param arg Unused.
  * \return true on success, false on failure (nanopb convention).
  */
bool getEntropyCallback(pb_ostream_t *stream, const pb_field_t *field, void * const *arg)
{
	if (entropy_buffer == NULL)
	{
		return false;
	}
	if (!pb_encode_tag_for_field(stream, field))
	{
		return false;
	}
	if (!pb_encode_string(stream, entropy_buffer, num_entropy_bytes))
	{
		return false;
	}
	return true;
}

/** Return bytes of entropy from the random number generation system.
  * \param num_bytes Number of bytes of entropy to send to stream.
  */
static NOINLINE void getBytesOfEntropy(uint32_t num_bytes)
{
	Entropy message_buffer;
	unsigned int random_bytes_index;
	uint8_t random_bytes[20480]; // must be multiple of 32 bytes large. Default:1024

	if (num_bytes > sizeof(random_bytes))
	{
		writeFailureString(STRINGSET_MISC, MISCSTR_PARAM_TOO_LARGE);
		return;
	}

	// All bytes of entropy must be collected before anything can be sent.
	// This is because it is only safe to send those bytes if every call
	// to getRandom256() succeeded.
	random_bytes_index = 0;
	num_entropy_bytes = 0;
	while (num_bytes--)
	{
		if (random_bytes_index == 0)
		{
			if (getRandom256(&(random_bytes[num_entropy_bytes])))
			{
				translateWalletError(WALLET_RNG_FAILURE);
				return;
			}
		}
		num_entropy_bytes++;
		random_bytes_index++;
		random_bytes_index &= 31;
	}
	message_buffer.entropy.funcs.encode = &getEntropyCallback;
	entropy_buffer = random_bytes;
	sendPacket(PACKET_TYPE_ENTROPY, Entropy_fields, &message_buffer);
	num_entropy_bytes = 0;
	entropy_buffer = NULL;
	writeEinkDisplay(">entropy sent", false, 10, 10, "",false,0,0, "",false,0,0, "",false,0,0, "",false,0,0);
}

/** nanopb field callback which will write out the contents
  * of #entropy_buffer.
  * \param stream Output stream to write to.
  * \param field Field which contains the the entropy bytes.
  * \param arg Unused.
  * \return true on success, false on failure (nanopb convention).
  */
bool getBulkCallback(pb_ostream_t *stream, const pb_field_t *field, void * const *arg)
{
	if (bulk_buffer == NULL)
	{
		return false;
	}
	if (!pb_encode_tag_for_field(stream, field))
	{
		return false;
	}
	if (!pb_encode_string(stream, bulk_buffer, num_bulk_bytes))
	{
		return false;
	}
	return true;
}



/** Return bytes of entropy from the random number generation system.
  * \param num_bytes Number of bytes of entropy to send to stream.
  */
void getBytesOfBulk(void) // was static NOINLINE
{
	Bulk message_buffer;
	unsigned int bulk_bytes_index;
	uint8_t bulk_bytes[EEPROM_SIZE]; // must be multiple of 32 bytes large. Default:1024

	num_bulk_bytes = readEntireWalletSpace(bulk_bytes);

	uint8_t encrypty[16]={1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1};

	encryptStream(bulk_bytes, encrypty);

	message_buffer.bulk.funcs.encode = &getBulkCallback;
	bulk_buffer = bulk_bytes;
	sendPacket(PACKET_TYPE_BULK, Bulk_fields, &message_buffer);
	num_bulk_bytes = 0;
	bulk_buffer = NULL;
	writeEinkDisplay(">data sent", false, 10, 10, "READY TO FLASH",false,10,40, "",false,0,0, "",false,0,0, "",false,0,0);
}

void encryptStream(uint8_t *plaintext, uint8_t *key)
{
	writeEinkDisplay(">encrypting", false, 10, 10, "",false,0,0, "",false,0,0, "",false,0,0, "",false,0,0);

	uint8_t ciphertext_chunk[16] = {};
	uint8_t plaintext_chunk[16] = {};

	uint8_t tweaky[16]={1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1};
//	uint8_t encrypty[16]={3,1,4,1,5,9,3,1,4,1,5,9,3,1,4,1};
	uint8_t n[16];
	memset(n, 32, 16);
	uint8_t seq_param = 1;
	uint32_t i, j;


	for(i=0;i<num_bulk_bytes;i=i+16)
	{
//		writeEinkDisplayNumberSingleBig(i,5,5);
		for(j=0;j<16; j++)
		{
			plaintext_chunk[j] = plaintext[i+j];
//			writeEinkDisplayNumberSingleBig(j,5,5);
		}
		xexEncryptTweaked(ciphertext_chunk, plaintext_chunk, n, seq_param, tweaky, key);
		for(j=0;j<16; j++)
		{
			plaintext[i+j] = ciphertext_chunk[j];
//			writeEinkDisplayNumberSingleBig(j,5,25);
		}
	}
//	displayHexStream(outie, 16);
}

void encryptStreamSized(uint8_t *plaintext, uint8_t *key, uint32_t size)
{
	writeEinkDisplay(">encrypting", false, 10, 10, "",false,0,0, "",false,0,0, "",false,0,0, "",false,0,0);

	uint8_t ciphertext_chunk[16] = {};
	uint8_t plaintext_chunk[16] = {};

	uint8_t tweaky[16]={1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1};
//	uint8_t encrypty[16]={1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1};
	uint8_t n[16];
	memset(n, 32, 16);
	uint8_t seq_param = 1;
	uint32_t i, j;


	for(i=0;i<size;i=i+16)
	{
//		writeEinkDisplayNumberSingleBig(i,5,5);
		for(j=0;j<16; j++)
		{
			plaintext_chunk[j] = plaintext[i+j];
//			writeEinkDisplayNumberSingleBig(j,5,5);
		}
		xexEncryptTweaked(ciphertext_chunk, plaintext_chunk, n, seq_param, tweaky, key);
		for(j=0;j<16; j++)
		{
			plaintext[i+j] = ciphertext_chunk[j];
//			writeEinkDisplayNumberSingleBig(j,5,25);
		}
	}
//	displayHexStream(outie, 16);
}

void decryptStream(uint8_t *ciphertext, uint8_t *key)
{
	writeEinkDisplay(">decrypting", false, 10, 10, "",false,0,0, "",false,0,0, "",false,0,0, "",false,0,0);

	uint8_t ciphertext_chunk[16] = {};
	uint8_t plaintext_chunk[16] = {};

	uint8_t tweaky[16]={1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1};
//	uint8_t encrypty[16]={1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1};
	uint8_t n[16];
	memset(n, 32, 16);
	uint8_t seq_param = 1;
	uint32_t i, j;

	for(i=0;i<EEPROM_SIZE;i=i+16)
	{
//		writeEinkDisplayNumberSingleBig(i,5,5);
		for(j=0;j<16; j++)
		{
			ciphertext_chunk[j] = ciphertext[i+j];
//			writeEinkDisplayNumberSingleBig(j,5,5);
		}
		xexDecryptTweaked(plaintext_chunk, ciphertext_chunk, n, seq_param, tweaky, key);
//		displayHexStream(plaintext_chunk, 16);
		for(j=0;j<16; j++)
		{
			ciphertext[i+j] = plaintext_chunk[j];
//			writeEinkDisplayNumberSingleBig(j,5,25);
		}
	}
//	displayHexStream(outie, 16);
}

void decryptStreamSized(uint8_t *ciphertext, uint8_t *key, uint32_t size)
{
	writeEinkDisplay(">decrypting", false, 10, 10, "",false,0,0, "",false,0,0, "",false,0,0, "",false,0,0);

	uint8_t ciphertext_chunk[16] = {};
	uint8_t plaintext_chunk[16] = {};

	uint8_t tweaky[16]={1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1};
//	uint8_t encrypty[16]={1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1};
	uint8_t n[16];
	memset(n, 32, 16);
	uint8_t seq_param = 1;
	uint32_t i, j;

	for(i=0;i<size;i=i+16)
	{
//		writeEinkDisplayNumberSingleBig(i,5,5);
		for(j=0;j<16; j++)
		{
			ciphertext_chunk[j] = ciphertext[i+j];
//			writeEinkDisplayNumberSingleBig(j,5,5);
		}
		xexDecryptTweaked(plaintext_chunk, ciphertext_chunk, n, seq_param, tweaky, key);
//		displayHexStream(plaintext_chunk, 16);
		for(j=0;j<16; j++)
		{
			ciphertext[i+j] = plaintext_chunk[j];
//			writeEinkDisplayNumberSingleBig(j,5,25);
		}
	}
//	displayHexStream(outie, 16);
}

/** nanopb field callback which calculates the double SHA-256 of an arbitrary
  * number of bytes. This is useful if we don't care about the contents of a
  * field but want to compress an arbitrarily-sized field into a fixed-length
  * variable.
  * \param stream Input stream to read from.
  * \param field Field which contains an arbitrary number of bytes.
  * \param arg Unused.
  * \return true on success, false on failure (nanopb convention).
  */
bool hashFieldCallback(pb_istream_t *stream, const pb_field_t *field, void **arg)
{
	uint8_t one_byte;
	HashState hs;

	sha256Begin(&hs);
	while (stream->bytes_left > 0)
    {
		if (!pb_read(stream, &one_byte, 1))
		{
			return false;
		}
        sha256WriteByte(&hs, one_byte);
    }
	sha256FinishDouble(&hs);
	writeHashToByteArray(field_hash, &hs, true);
	field_hash_set = true;
    return true;
}
/** nanopb field callback which calculates the double SHA-256 of an arbitrary
  * number of bytes. This is useful if we don't care about the contents of a
  * field but want to compress an arbitrarily-sized field into a fixed-length
  * variable.
  * \param stream Input stream to read from.
  * \param field Field which contains an arbitrary number of bytes.
  * \param arg Unused.
  * \return true on success, false on failure (nanopb convention).
  */
bool hashFieldCallbackGeneric(pb_istream_t *stream, uint8_t *theHash, uint8_t *theMessage, int *theLength, bool do_write_big_endian)
{
	uint8_t one_byte;
	HashState hs;
	int byteIndex = 0;

	sha256Begin(&hs);
	while (stream->bytes_left > 0)
    {
		if (!pb_read(stream, &one_byte, 1))
		{
			return false;
		}
		theMessage[byteIndex] = one_byte;
        sha256WriteByte(&hs, one_byte);
        byteIndex++;
    }
	*theLength = byteIndex;
	sha256FinishDouble(&hs);
	writeHashToByteArray(theHash, &hs, do_write_big_endian);
    return true;
}



//bool initialFormat(void)
//{
//	uint8_t langHold[1];
//	uint8_t langHoldStatus[1];
//	uint8_t pinStatusHold[1];
//	uint8_t pinTypeHold[1];
//	uint8_t pinHold[32];
//	uint8_t aem_use_Hold[1];
//	uint8_t aem_phrase_Hold[64];
//	uint8_t countHold[1] = {};
//
//	bool permission_denied;
//	WalletErrors wallet_return;
//	uint8_t entropy_initialation[32] = {66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66};
//
//	nonVolatileRead(langHold, DEVICE_LANG_ADDRESS, 1);
//	nonVolatileRead(langHoldStatus, DEVICE_LANG_SET_ADDRESS, 1);
//	nonVolatileRead(pinStatusHold, HAS_PIN_ADDRESS, 1);
//	nonVolatileRead(pinTypeHold, SETUP_TYPE_ADDRESS, 1);
//	nonVolatileRead(pinHold, PIN_ADDRESS, 32);
//	nonVolatileRead(aem_use_Hold, AEM_USE_ADDRESS, 1);
//	nonVolatileRead(aem_phrase_Hold, AEM_PHRASE_ADDRESS, 64);
//
//	int tempLang;
//	tempLang = (int)langHold[0];
//
//	permission_denied = buttonInterjectionNoAck(ASKUSER_FORMAT);
//	if (!permission_denied)
//	{
//			if (initialiseEntropyPool(entropy_initialation))
//			{
//				translateWalletError(WALLET_RNG_FAILURE);
//			}
//			else
//			{
//				wallet_return = sanitiseNonVolatileStorage(0, 0xffffffff, false, tempLang);
//				translateWalletError(wallet_return);
//				uninitWallet(); // force wallet to unload
//				int s = 123;
//				uint8_t temp1[1];
//				temp1[0] = s;
//				nonVolatileWrite(temp1, IS_FORMATTED_ADDRESS, 1);
//				is_formatted = s;
//			}
//	}
//
//	nonVolatileWrite(langHold, DEVICE_LANG_ADDRESS, 1);
//	nonVolatileWrite(langHoldStatus, DEVICE_LANG_SET_ADDRESS, 1);
//	nonVolatileWrite(pinStatusHold, HAS_PIN_ADDRESS, 1);
//	nonVolatileWrite(pinTypeHold, SETUP_TYPE_ADDRESS, 1);
//	nonVolatileWrite(pinHold, PIN_ADDRESS, 32);
//	nonVolatileWrite(aem_use_Hold, AEM_USE_ADDRESS, 1);
//	nonVolatileWrite(aem_phrase_Hold, AEM_PHRASE_ADDRESS, 64);
//	nonVolatileWrite(countHold, WRONG_DEVICE_PIN_COUNT_ADDRESS, 1);
//	nonVolatileWrite(countHold, WRONG_TRANSACTION_PIN_COUNT_ADDRESS, 1);
//
//
//	if (!permission_denied)
//	{
//		return true;
//	}
//	else
//	{
//		return false;
//	}
//}


//All timers must be stopped before formatting
bool initialFormatAuto(void)
{
	uint8_t langHold[1];
	uint8_t langHoldStatus[1];
	uint8_t pinStatusHold[1];
	uint8_t pinTypeHold[1];
	uint8_t countHold[1] = {};
	uint8_t pinHold[32];
	bool permission_denied;
	WalletErrors wallet_return;
	uint8_t entropy_initialation[32] = {66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66};
	uint8_t aem_use_Hold[1];
	uint8_t aem_phrase_Hold[64];


	nonVolatileRead(langHold, DEVICE_LANG_ADDRESS, 1);
	nonVolatileRead(langHoldStatus, DEVICE_LANG_SET_ADDRESS, 1);
	nonVolatileRead(pinStatusHold, HAS_PIN_ADDRESS, 1);
	nonVolatileRead(pinTypeHold, SETUP_TYPE_ADDRESS, 1);
	nonVolatileRead(pinHold, PIN_ADDRESS, 32);
//	nonVolatileRead(countHold, WRONG_DEVICE_PIN_COUNT, 1);
//	nonVolatileRead(countHold, WRONG_TRANSACTION_PIN_COUNT_ADDRESS, 1);
	nonVolatileRead(aem_use_Hold, AEM_USE_ADDRESS, 1);
	nonVolatileRead(aem_phrase_Hold, AEM_PHRASE_ADDRESS, 64);

	int tempLang;
	tempLang = (int)langHold[0];

	permission_denied = buttonInterjectionNoAck(ASKUSER_FORMAT_AUTO);
//	permission_denied = false;
	if (!permission_denied)
	{
			if (initialiseEntropyPool(entropy_initialation))
			{
				translateWalletError(WALLET_RNG_FAILURE);
			}
			else
			{
				wallet_return = sanitiseNonVolatileStorage(0, 0xffffffff, false, tempLang);
				translateWalletError(wallet_return);
				uninitWallet(); // force wallet to unload
				int s = 123;
				uint8_t temp1[1];
				temp1[0] = s;
				nonVolatileWrite(temp1, IS_FORMATTED_ADDRESS, 1);
				is_formatted = s;
			}
	}

	nonVolatileWrite(langHold, DEVICE_LANG_ADDRESS, 1);
	nonVolatileWrite(langHoldStatus, DEVICE_LANG_SET_ADDRESS, 1);
	nonVolatileWrite(pinStatusHold, HAS_PIN_ADDRESS, 1);
	nonVolatileWrite(pinTypeHold, SETUP_TYPE_ADDRESS, 1);
	nonVolatileWrite(pinHold, PIN_ADDRESS, 32);
	nonVolatileWrite(countHold, WRONG_DEVICE_PIN_COUNT_ADDRESS, 1);
	nonVolatileWrite(countHold, WRONG_TRANSACTION_PIN_COUNT_ADDRESS, 1);
	nonVolatileWrite(aem_use_Hold, AEM_USE_ADDRESS, 1);
	nonVolatileWrite(aem_phrase_Hold, AEM_PHRASE_ADDRESS, 64);


	if (!permission_denied)
	{
		return true;
	}
	else
	{
		return false;
	}
}


bool initialFormatAutoDuress(void)
{
//	uint8_t entropy_initialation[32] = {66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66};

	sanitiseNonVolatileStorage(0, 0xffffffff, true, 0);

	return true;
}



bool createDefaultWalletAuto(int strength, int level)
{
//	union MessageBufferUnion message_buffer;
	uint32_t wallet_number;
	unsigned int password_length;
	bool trans_pin_used;
	WalletErrors wallet_return;
//	bool receive_failure;
	bool permission_denied;
	bool cancelWalletCreation = false;
	int passwordMatch;
	bool use_seed;
	bool is_hidden;
	bool use_mnemonic_pass;

	uint8_t defaultName[40] = {	68,101,102, 97,117,108,116, 32, 32, 32,
								32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
								32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
								32, 32, 32, 32, 32, 32, 32, 32, 32, 00};

//	defaultName[39] = '\0';

	// Create new wallet.
	field_hash_set = true;
	memset(field_hash, 0, sizeof(field_hash));
	memset(temp_field_hash, 0, sizeof(temp_field_hash));

	pin_field_hash_set = false;
	memset(pin_field_hash, 0, sizeof(pin_field_hash));
	memset(temp_pin_field_hash, 0, sizeof(temp_pin_field_hash));

	wallet_number = 0;

	permission_denied = false;
	if (!permission_denied)
	{
		if (field_hash_set)
		{
			cancelWalletCreation = passwordInterjectionAutoPIN(level);
			password_length = sizeof(field_hash);
			if(pin_field_hash_set)
			{
//				writeEinkDisplay("In if(set)", false, COL_1_X, LINE_1_Y, "",false,5,30, "",false,5,50, "",false,5,70, "",false,0,0);
//				displayBigHexStream(pin_field_hash, 32);
//				waitForButtonPress();

				trans_pin_used = true;
			}else
			{
				trans_pin_used = false;
			}
		}
		else
		{
			password_length = 0; // no password
		}

//			INSERT ROUTINE TO RUN BIP39 HERE	false & NULL will be replaced by true & the seed - MUST DISPLAY & VERIFY WORD LIST
		if (!cancelWalletCreation)
		{
			char* pass = 0;
			use_mnemonic_pass = buttonInterjectionNoAck(ASKUSER_USE_MNEMONIC_PASSPHRASE);
			if(!use_mnemonic_pass)
			{
				buttonInterjectionNoAckSetup(ASKUSER_ALPHA_INPUT_PREFACE);

				pass = getInput(true, false);
			}
			clearDisplay();
			uint8_t seed[512 / 8] = { };

			uint8_t empty[512 / 8] = { };
			uint8_t i;
			uint8_t one_byte; // current byte of seed
			char strBits[3];
			char lineTemp[64] = { };
#ifdef FIXED_SEED_DANA
			const char * mnem128 = DANA_SEED_2;
#endif

#ifdef FIXED_SEED_NIK
			const char * mnem128 = NIK_SEED_1;
#endif

#ifndef FIXED_SEED_NIK
#ifndef FIXED_SEED_DANA
			const char * mnem128 = mnemonic_generate(strength);
#endif
#endif
			mnemonic_to_seed(mnem128, pass, seed); // creating the seed BEFORE displaying mnemonic seems to generate the CORRECT seed!
#ifdef DISPLAY_SEED
			int len = strength / 8;
			int mlen = len * 3 / 4;
			displayMnemonic(mnem128, mlen); //################################################################################################# REMOVE THIS after test
#endif

			is_hidden = false;
			use_seed = true;
			wallet_return = newWallet(
				wallet_number,
				defaultName,
				use_seed,
				seed,
				is_hidden,
				field_hash,
				password_length,
				pin_field_hash,
				trans_pin_used);
			translateWalletError(wallet_return);
		}else if(cancelWalletCreation)
		{
			initDisplay();
			overlayBatteryStatus(BATT_VALUE_DISPLAY);
			writeEinkNoDisplay("CANCELING",  COL_1_X, LINE_1_Y, "NO WALLET CREATED",COL_1_X,LINE_2_Y, "",33,LINE_2_Y, "",COL_1_X,LINE_3_Y, "",33,LINE_3_Y);
			writeUnderline(STRIPE_X_START, STRIPE_Y_START, STRIPE_X_END, STRIPE_Y_END);
			display();
			return true;
		}
	}
	return false;
}

void showQRcode(AddressHandle ah_root4, AddressHandle ah_chain4, AddressHandle ah_index4)
{
	uint8_t address_hash[20] = {};
	char *toEncode[36] = {};

	getAddressOnly(address_hash, ah_root4, ah_chain4, ah_index4);
	hashToAddr(toEncode, address_hash, ADDRESS_VERSION_PUBKEY);
	writeQRcode(toEncode);
}

void setChangeAddress(AddressHandle ah_root5, AddressHandle ah_chain5, AddressHandle ah_index5)
{
	uint8_t address_hash[20] = {};

	char *toEncode[36] = {};

	getAddressOnly(address_hash, ah_root5, ah_chain5, ah_index5);
	hashToAddr(toEncode, address_hash, ADDRESS_VERSION_PUBKEY);
	strcpy(change_address_ptr_begin, toEncode);
}





/** Get packet from stream and deal with it. This basically implements the
  * protocol described in the file PROTOCOL.
  * 
  * This function will always completely
  * read a packet before sending a response packet. As long as the host
  * does the same thing, deadlocks cannot occur. Thus a productive
  * communication session between the hardware Bitcoin wallet and a host
  * should consist of the wallet and host alternating between sending a
  * packet and receiving a packet.
  */
 void processPacket(void)
{
	uint16_t message_id;
	union MessageBufferUnion message_buffer;
//	PointAffine master_public_key;
	bool receive_failure;
	bool permission_denied;
	bool invalid_otp;
	unsigned int password_length;
	WalletErrors wallet_return;
	char ping_greeting[sizeof(message_buffer.ping.greeting)];
	bool has_ping_greeting;
	int lang;
	bool use_seed;
//	uint8_t testSeed[] = {};
	uint8_t langHold[1];
	uint8_t langHoldStatus[1];
	uint8_t pinStatusHold[1];
	uint8_t pinHold[32];
	uint8_t name_temp[] = {102, 121, 117, 116, 98, 0};



	uint8_t pinTypeHold[1];
	uint8_t countHold[1] = {};
	uint8_t aem_use_Hold[1];
	uint8_t aem_phrase_Hold[64];


	int strength;
	int level;
	char *levelChar;

	int passwordMatch;

	moofOn = false;

	message_id = receivePacketHeader();


	// Checklist for each case:
	// 1. Have you checked or dealt with length?
	// 2. Have you fully read the input stream before writing (to avoid
	//    deadlocks)?
	// 3. Have you asked permission from the user (for potentially dangerous
	//    operations)?
	// 4. Have you checked for errors from wallet functions?
	// 5. Have you used the right check for the wallet functions?

	memset(&message_buffer, 0, sizeof(message_buffer));



	switch (message_id)
	{

	case PACKET_TYPE_INITIALIZE:
		// Reset state and report features.
		session_id_length = 0; // just in case receiveMessage() fails
		receive_failure = receiveMessage(Initialize_fields, &(message_buffer.initialize));
		if (!receive_failure)
		{
			session_id_length = message_buffer.initialize.session_id.size;
			if (session_id_length >= sizeof(session_id))
			{
				fatalError(); // sanity check failed
			}
			memcpy(session_id, message_buffer.initialize.session_id.bytes, session_id_length);
			prev_transaction_hash_valid = false;
//			sanitiseRam();   // this function is useless
			wallet_return = uninitWallet();
			if (wallet_return == WALLET_NO_ERROR)
			{
				memset(&message_buffer, 0, sizeof(message_buffer));
				message_buffer.features.echoed_session_id.size = session_id_length;
				if (session_id_length >= sizeof(message_buffer.features.echoed_session_id.bytes))
				{
					fatalError(); // sanity check failed
				}
				memcpy(message_buffer.features.echoed_session_id.bytes, session_id, session_id_length);
				string_arg.next_set = STRINGSET_MISC;
				string_arg.next_spec = MISCSTR_VENDOR;
				message_buffer.features.vendor.funcs.encode = &writeStringCallback;
				message_buffer.features.vendor.arg = &string_arg;
				message_buffer.features.has_major_version = true;
				message_buffer.features.major_version = VERSION_MAJOR;
				message_buffer.features.has_minor_version = true;
				message_buffer.features.minor_version = VERSION_MINOR;
				string_arg_alt.next_set = STRINGSET_MISC;
				string_arg_alt.next_spec = MISCSTR_CONFIG;
				message_buffer.features.config.funcs.encode = &writeStringCallback;
				message_buffer.features.config.arg = &string_arg_alt;
				message_buffer.features.has_otp = true;
				message_buffer.features.otp = true;
				message_buffer.features.has_pin = true;
				message_buffer.features.pin = true;
				message_buffer.features.has_spv = true;
				message_buffer.features.spv = true;
				message_buffer.features.algo_count = 1;
				message_buffer.features.algo[0] = Algorithm_BIP32;
				message_buffer.features.has_debug_link = true;
				message_buffer.features.debug_link = false;
				message_buffer.features.has_is_formatted = true;
				if(is_formatted == 123)
				{
					message_buffer.features.is_formatted = true;
				}
				else
				{
					message_buffer.features.is_formatted = false;
				}
				message_buffer.features.has_device_name = true;
				message_buffer.features.device_name.size = sizeof(name_temp);
				memcpy(message_buffer.features.device_name.bytes, name_temp, sizeof(name_temp));
//				writeEinkDisplay("BITLOX READY", false, 5, 5, "Multi Input", false, 5, 20, "BL_24:SB UNI QR",false,5,35, "BIP39.32HD.BLE.protoBuf",false,5,50, "Eink-opt.8192.fmtTgl",false,5,65);

				sendPacket(PACKET_TYPE_FEATURES, Features_fields, &(message_buffer.features));
			}
			else
			{
				translateWalletError(wallet_return);
			}
		}
		break;

	case PACKET_TYPE_PING:
		// Ping request.
		receive_failure = receiveMessage(Ping_fields, &(message_buffer.ping));
		if (!receive_failure)
		{
			has_ping_greeting = message_buffer.ping.has_greeting;
			if (sizeof(message_buffer.ping.greeting) != sizeof(ping_greeting))
			{
				fatalError(); // sanity check failed
			}
			if (has_ping_greeting)
			{
				memcpy(ping_greeting, message_buffer.ping.greeting, sizeof(ping_greeting));
//				writeEink(ping_greeting, false, 40, 40);
			}
			ping_greeting[sizeof(ping_greeting) - 1] = '\0'; // ensure that string is terminated
			// Generate ping response message.
			memset(&message_buffer, 0, sizeof(message_buffer));
			message_buffer.ping_response.has_echoed_greeting = has_ping_greeting;
			if (sizeof(ping_greeting) != sizeof(message_buffer.ping_response.echoed_greeting))
			{
				fatalError(); // sanity check failed
			}
			if (has_ping_greeting)
			{
				memcpy(message_buffer.ping_response.echoed_greeting, ping_greeting, sizeof(message_buffer.ping_response.echoed_greeting));
			}
			message_buffer.ping_response.echoed_session_id.size = session_id_length;
			if (session_id_length >= sizeof(message_buffer.ping_response.echoed_session_id.bytes))
			{
				fatalError(); // sanity check failed
			}
			memcpy(message_buffer.ping_response.echoed_session_id.bytes, session_id, session_id_length);
			sendPacket(PACKET_TYPE_PING_RESPONSE, PingResponse_fields, &(message_buffer.ping_response));
		}
		break;

	case PACKET_TYPE_DELETE_WALLET:
		// Delete existing wallet.
		receive_failure = receiveMessage(DeleteWallet_fields, &(message_buffer.delete_wallet));
		if (!receive_failure)
		{
			permission_denied = buttonInterjection(ASKUSER_DELETE_WALLET);
			if (!permission_denied)
			{
				invalid_otp = otpInterjection(ASKUSER_DELETE_WALLET);
				if (!invalid_otp)
				{
					wallet_return = deleteWallet(message_buffer.delete_wallet.wallet_handle);
					translateWalletErrorDisplayShowReady(wallet_return);
				}else{
					writeX_Screen();
					delay(1000);
					showReady();
				}
			}else{
				writeX_Screen();
				delay(1000);
				showReady();
			}
		}
		break;

	case PACKET_TYPE_NEW_WALLET:
		permission_denied = pseudoPacketNewWallet(0,0);
		if(permission_denied)
		{
			writeX_Screen();
			showReady();
			writeFailureString(STRINGSET_MISC, MISCSTR_PERMISSION_DENIED_USER);
		}
		break;

	case PACKET_TYPE_RESTORE_WALLET_DEVICE:
		permission_denied = pseudoPacketRestoreWallet(0,0);
		if(permission_denied)
		{
			writeX_Screen();
			showReady();
			writeFailureString(STRINGSET_MISC, MISCSTR_PERMISSION_DENIED_USER);
		}
		break;


	case PACKET_TYPE_SIGN_TRANSACTION_EXTENDED:
		// Sign a transaction.
		ahIndex = 0;
		sign_transaction_extended.address_handle_extended.funcs.decode = &read_AHE_data;
		sign_transaction_extended.transaction_data.funcs.decode = &signTransactionCompleteCallback;
		// Everything else is handled in signTransactionCallback().
		receiveMessage(SignTransactionExtended_fields, &sign_transaction_extended);
		break;


	case PACKET_TYPE_SIGN_MESSAGE:
		// Sign a message.
		sign_message.message_data.funcs.decode = &signMessageCallback;
		// Everything else is handled in signTransactionCallback().
		receiveMessage(SignMessage_fields, &sign_message);
		break;



	case PACKET_TYPE_DISPLAY_ADDRESS_AS_QR:
		// Load wallet.
		receive_failure = receiveMessage(DisplayAddressAsQR_fields, &(message_buffer.display_address_as_qr));
		if (!receive_failure)
		{
			if (checkWalletLoaded())
			{
				if (message_buffer.display_address_as_qr.has_address_handle_index == true)
				{
					showQRcode(0,0,message_buffer.display_address_as_qr.address_handle_index);
				}
				else
				{
					showQRcode(0,0,0);
				}
//				sendSuccessPacketOnly();
			}
		}

		break;


	case PACKET_TYPE_SIGN_TRANSACTION_CHANGE_ADDRESS:
		// Load wallet.
		receive_failure = receiveMessage(SetChangeAddressIndex_fields, &(message_buffer.set_change_address_index));
		if (!receive_failure)
		{
			if (checkWalletLoaded())
			{
				if (message_buffer.set_change_address_index.has_address_handle_index == true)
				{
					setChangeAddress(0,1,message_buffer.set_change_address_index.address_handle_index);
				}
				else
				{
					setChangeAddress(0,1,0);
				}
				sendSuccessPacketOnly();
			}
		}

		break;


	case PACKET_TYPE_LOAD_WALLET:
		// Load wallet.
		receive_failure = receiveMessage(LoadWallet_fields, &(message_buffer.load_wallet));
		if (!receive_failure)
		{
			// Attempt load with no password.
			wallet_return = initWallet(message_buffer.load_wallet.wallet_number, field_hash, 0);
			if (wallet_return == WALLET_NOT_THERE)
			{
				// Attempt load with password.
				permission_denied = passwordInterjection(false);
				if (!permission_denied)
				{
					if (!field_hash_set)
					{
						fatalError(); // this should never happen
					}
					wallet_return = initWallet(message_buffer.load_wallet.wallet_number, field_hash, sizeof(field_hash));
					translateWalletError(wallet_return);
					if (wallet_return == WALLET_NO_ERROR)
					{
//						walletLoadAttemptsCounter = 0;
						showQRcode(0,0,0);
					}else if(wallet_return == WALLET_NOT_THERE)
					{
						int setupType;
						setupType = checkSetupType();
						int delayJump = 0;
						int delayThreshold = 0;

						if (setupType == 127)
						{
							delayJump = 10;
							delayThreshold = 7;
						}else if (setupType == 128)
						{
							delayJump = 11;
							delayThreshold = 5;
						}else if (setupType == 129)
						{
							delayJump = 12;
							delayThreshold = 3;
						}

						uint8_t delayJumpTemp[1] = {};
						delayJumpTemp[0] = delayJump;

						walletLoadAttemptsCounter++;

						if(walletLoadAttemptsCounter == delayThreshold)
						{
							nonVolatileWrite(delayJumpTemp, WRONG_DEVICE_PIN_COUNT_ADDRESS, 1);
							Software_Reset();
						}
					}
				}
			}
			else
			{
				translateWalletError(wallet_return);
				if (wallet_return == WALLET_NO_ERROR)
				{
//					walletLoadAttemptsCounter = 0;
					showQRcode(0,0,0);
				}
			}
		}

		break;


	case PACKET_TYPE_FORMAT:
		// Format storage.

		nonVolatileRead(langHold, DEVICE_LANG_ADDRESS, 1);
		nonVolatileRead(langHoldStatus, DEVICE_LANG_SET_ADDRESS, 1);
		nonVolatileRead(pinStatusHold, HAS_PIN_ADDRESS, 1);
		nonVolatileRead(pinTypeHold, SETUP_TYPE_ADDRESS, 1);
		nonVolatileRead(pinHold, PIN_ADDRESS, 32);
	//	nonVolatileRead(countHold, WRONG_DEVICE_PIN_COUNT, 1);
	//	nonVolatileRead(countHold, WRONG_TRANSACTION_PIN_COUNT_ADDRESS, 1);
		nonVolatileRead(aem_use_Hold, AEM_USE_ADDRESS, 1);
		nonVolatileRead(aem_phrase_Hold, AEM_PHRASE_ADDRESS, 64);



//		nonVolatileRead(langHold, DEVICE_LANG_ADDRESS, 1);
//		nonVolatileRead(langHoldStatus, DEVICE_LANG_SET_ADDRESS, 1);
//		nonVolatileRead(pinStatusHold, HAS_PIN_ADDRESS, 1);
//		nonVolatileRead(pinHold, PIN_ADDRESS, 32);


		receive_failure = receiveMessage(FormatWalletArea_fields, &(message_buffer.format_wallet_area));
		if (!receive_failure)
		{
			permission_denied = buttonInterjection(ASKUSER_FORMAT);
			if (!permission_denied)
			{
				invalid_otp = otpInterjection(ASKUSER_FORMAT);
				if (!invalid_otp)
				{
//					writeEinkDisplay("!invalid_otp", false, COL_1_X, LINE_1_Y, "",false,5,30, "",false,5,50, "",false,5,70, "",false,0,0);

					if (initialiseEntropyPool(message_buffer.format_wallet_area.initial_entropy_pool.bytes))
					{
						writeEinkDisplay("WALLET_RNG_FAILURE", false, COL_1_X, LINE_0_Y, "",false,5,30, "",false,5,50, "",false,5,70, "",false,0,0);
						translateWalletError(WALLET_RNG_FAILURE);
					}
					else
					{
//						writeEinkDisplay("pre sanitise", false, COL_1_X, LINE_1_Y, "",false,5,30, "",false,5,50, "",false,5,70, "",false,0,0);
						wallet_return = sanitiseNonVolatileStorage(0, 0xffffffff, false, (int)langHold[0]);
						translateWalletError(wallet_return);
						uninitWallet(); // force wallet to unload
						int s = 123;
						uint8_t temp1[1];
						temp1[0] = s;
						nonVolatileWrite(temp1, IS_FORMATTED_ADDRESS, 1);
						is_formatted = s;
						showReady();
					}
				}else
				{
					writeX_Screen();
					showReady();
				}
			}else
			{
				writeX_Screen();
				showReady();
			}
		}
		if(is_formatted==123)
		{
//			nonVolatileWrite(langHold, DEVICE_LANG_ADDRESS, 1);
//			nonVolatileWrite(langHoldStatus, DEVICE_LANG_SET_ADDRESS, 1);
//			nonVolatileWrite(pinStatusHold, HAS_PIN_ADDRESS, 1);
//			nonVolatileWrite(pinHold, PIN_ADDRESS, 32);

			nonVolatileWrite(langHold, DEVICE_LANG_ADDRESS, 1);
			nonVolatileWrite(langHoldStatus, DEVICE_LANG_SET_ADDRESS, 1);
			nonVolatileWrite(pinStatusHold, HAS_PIN_ADDRESS, 1);
			nonVolatileWrite(pinTypeHold, SETUP_TYPE_ADDRESS, 1);
			nonVolatileWrite(pinHold, PIN_ADDRESS, 32);
			nonVolatileWrite(countHold, WRONG_DEVICE_PIN_COUNT_ADDRESS, 1);
			nonVolatileWrite(countHold, WRONG_TRANSACTION_PIN_COUNT_ADDRESS, 1);
			nonVolatileWrite(aem_use_Hold, AEM_USE_ADDRESS, 1);
			nonVolatileWrite(aem_phrase_Hold, AEM_PHRASE_ADDRESS, 64);

		}

		break;


//	case PACKET_TYPE_CHANGE_KEY:
//		// Change wallet encryption key.
//		field_hash_set = false;
//		memset(field_hash, 0, sizeof(field_hash));
//		message_buffer.change_encryption_key.password.funcs.decode = &hashFieldCallback;
//		message_buffer.change_encryption_key.password.arg = NULL;
//		receive_failure = receiveMessage(ChangeEncryptionKey_fields, &(message_buffer.change_encryption_key));
//		if (!receive_failure)
//		{
//			permission_denied = buttonInterjection(ASKUSER_CHANGE_KEY);
//			if (!permission_denied)
//			{
//				invalid_otp = otpInterjection(ASKUSER_CHANGE_KEY);
//				if (!invalid_otp)
//				{
//					if (field_hash_set)
//					{
//						password_length = sizeof(field_hash);
//					}
//					else
//					{
//						password_length = 0; // no password
//					}
//					wallet_return = changeEncryptionKey(field_hash, password_length);
//					translateWalletError(wallet_return);
//				}
//			}
//		}
//		break;


	case PACKET_TYPE_CHANGE_NAME:
		// Change wallet name.
		receive_failure = receiveMessage(ChangeWalletName_fields, &(message_buffer.change_wallet_name));
		if (!receive_failure)
		{
			permission_denied = buttonInterjection(ASKUSER_CHANGE_NAME);
			if (!permission_denied)
			{
				wallet_return = changeWalletName(message_buffer.change_wallet_name.wallet_name.bytes);
				translateWalletErrorDisplayQR(wallet_return);
			}

		}
		break;


	case PACKET_TYPE_LIST_WALLETS:
		// List wallets.
		receive_failure = receiveMessage(ListWallets_fields, &(message_buffer.list_wallets));
		if (!receive_failure)
		{
			number_of_wallets = getNumberOfWallets();

			if (number_of_wallets == 0)
			{
				wallet_return = walletGetLastError();
				translateWalletError(wallet_return);
			}
			else
			{
				message_buffer.wallets.wallet_info.funcs.encode = &listWalletsCallback;
				sendPacket(PACKET_TYPE_WALLETS, Wallets_fields, &(message_buffer.wallets));
			}
		}
		break;

//	case PACKET_TYPE_BACKUP_WALLET:
//		// Backup wallet.
//		toggleSerial();
////		receive_failure = receiveMessage(BackupWallet_fields, &(message_buffer.backup_wallet));
////		if (!receive_failure)
////		{
////			permission_denied = buttonInterjection(ASKUSER_BACKUP_WALLET);
////			if (!permission_denied)
////			{
////				wallet_return = backupWallet(message_buffer.backup_wallet.is_encrypted, message_buffer.backup_wallet.device);
////				translateWalletError(wallet_return);
////			}
////		}
//		break;

//	case PACKET_TYPE_RESTORE_WALLET:
//		// Restore wallet.
//		field_hash_set = false;
//		memset(field_hash, 0, sizeof(field_hash));
//		message_buffer.restore_wallet.new_wallet.password.funcs.decode = &hashFieldCallback;
//		message_buffer.restore_wallet.new_wallet.password.arg = NULL;
//		receive_failure = receiveMessage(RestoreWallet_fields, &(message_buffer.restore_wallet));
//		if (!receive_failure)
//		{
//			if (message_buffer.restore_wallet.seed.size != SEED_LENGTH)
//			{
//				writeFailureString(STRINGSET_MISC, MISCSTR_INVALID_PACKET);
//			}
//			else
//			{
//				permission_denied = buttonInterjection(ASKUSER_RESTORE_WALLET);
//				if (!permission_denied)
//				{
//					if (field_hash_set)
//					{
//						password_length = sizeof(field_hash);
//					}
//					else
//					{
//						password_length = 0; // no password
//					}
//					wallet_return = newWallet(
//						message_buffer.restore_wallet.new_wallet.wallet_number,
//						message_buffer.restore_wallet.new_wallet.wallet_name.bytes,
//						true,
//						message_buffer.restore_wallet.seed.bytes,
//						message_buffer.restore_wallet.new_wallet.is_hidden,
//						field_hash,
//						password_length);
//					translateWalletError(wallet_return);
//				}
//			}
//		}
//		break;

	case PACKET_TYPE_GET_DEVICE_UUID:
		// Get device UUID.
		receive_failure = receiveMessage(GetDeviceUUID_fields, &(message_buffer.get_device_uuid));
		if (!receive_failure)
		{
			message_buffer.device_uuid.device_uuid.size = DEVICE_UUID_LENGTH;
			if (nonVolatileRead(message_buffer.device_uuid.device_uuid.bytes, DEVICE_UUID_ADDRESS, DEVICE_UUID_LENGTH) == NV_NO_ERROR)
			{
				sendPacket(PACKET_TYPE_DEVICE_UUID, DeviceUUID_fields, &(message_buffer.device_uuid));
			}
			else
			{
				translateWalletError(WALLET_READ_ERROR);
			}
		}
		break;


	case PACKET_TYPE_GET_ENTROPY:
		// Get an arbitrary number of bytes of entropy.
		receive_failure = receiveMessage(GetEntropy_fields, &(message_buffer.get_entropy));
		if (!receive_failure)
		{
			getBytesOfEntropy(message_buffer.get_entropy.number_of_bytes);
		}
		break;


	case PACKET_TYPE_GET_BULK:
		// Send the entire wallet storage space encrypted
		receive_failure = receiveMessage(GetBulk_fields, &(message_buffer.get_bulk));
		if (!receive_failure)
		{
			getBytesOfBulk();
			usartSpew();
		}
		break;

	case PACKET_TYPE_SET_BULK:
//		writeEinkDisplay("got restore packet", false, 5, 40, "",false,0,10, "",false,0,60, "",false,0,0, "",false,0,0);
		// Write the entire wallet storage space with a byte array
		receive_failure = receiveMessage(SetBulk_fields, &(message_buffer.set_bulk));
		if (!receive_failure)
		{
			writeEntireWalletSpace(message_buffer.set_bulk.bulk.bytes);
		}else{
			writeEinkDisplay("restore packet malformed", false, 5, 40, "",false,0,10, "",false,0,60, "",false,0,0, "",false,0,0);
		}
		break;






//	case PACKET_TYPE_GET_MASTER_KEY:
//		// Get master public key and chain code.
//		receive_failure = receiveMessage(GetMasterPublicKey_fields, &(message_buffer.get_master_public_key));
//		if (!receive_failure)
//		{
//			permission_denied = buttonInterjection(ASKUSER_GET_MASTER_KEY);
//			if (!permission_denied)
//			{
//				invalid_otp = otpInterjection(ASKUSER_GET_MASTER_KEY);
//				if (!invalid_otp)
//				{
//					wallet_return = getMasterPublicKey(&master_public_key, message_buffer.master_public_key.chain_code.bytes);
//					if (wallet_return == WALLET_NO_ERROR)
//					{
//						message_buffer.master_public_key.chain_code.size = 32;
//						message_buffer.master_public_key.public_key.size = 65;
//						message_buffer.master_public_key.public_key.bytes[0] = 0x04;
//						memcpy(&(message_buffer.master_public_key.public_key.bytes[1]), master_public_key.x, 32);
//						swapEndian256(&(message_buffer.master_public_key.public_key.bytes[1]));
//						memcpy(&(message_buffer.master_public_key.public_key.bytes[33]), master_public_key.y, 32);
//						swapEndian256(&(message_buffer.master_public_key.public_key.bytes[33]));
//						sendPacket(PACKET_TYPE_MASTER_KEY, MasterPublicKey_fields, &(message_buffer.master_public_key));
//					}
//					else
//					{
//						translateWalletError(wallet_return);
//					}
//				}
//			}
//		}
//		break;

	case PACKET_TYPE_RESET_LANG:
		// Change interface language
		receive_failure = receiveMessage(ResetLang_fields, &(message_buffer.reset_lang));
		if (!receive_failure)
		{
			resetLang();
			languageMenu();
			showReady();
		}
		break;

	case PACKET_TYPE_SCAN_WALLET:
		// fetch the m/0'/0 xpub for the currently loaded wallet
		receive_failure = receiveMessage(ScanWallet_fields, &(message_buffer.scan_wallet));
		if (!receive_failure)
		{
//			char tempxpub[112] = "xpub695JbU1zQrk9vznW6y66vUPbpKRJsnqy91VMQxhE17DnagUFYTo9xPdUZaZFnzDvMxf5D9PksSJpcciYigFq3cnGHVWuBmkqrMe7M7QdtGW";
			char tempxpub[112] = {};
			AddressHandle ah;

			wallet_return = getPublicExtendedKey(tempxpub, ah);

			memcpy(message_buffer.current_wallet_xpub.xpub, tempxpub, 112);
			wallet_return = walletGetLastError();
			if (wallet_return == WALLET_NO_ERROR)
			{
				sendPacket(PACKET_TYPE_SCAN_WALLET_RESPONSE, CurrentWalletXPUB_fields, &(message_buffer.current_wallet_xpub.xpub));
//				translateWalletError(wallet_return); // including this causes the xpub to emerge corrupted
			}
			else
			{
				translateWalletError(wallet_return);
			}
		}
		break;


	default:
		// Unknown message ID.
		readAndIgnoreInput();
		writeFailureString(STRINGSET_MISC, MISCSTR_UNEXPECTED_PACKET);
		break;

	}
}

//void showSeed(void)
// {
//		uint16_t message_id;
//		union MessageBufferUnion message_buffer;
//		PointAffine master_public_key;
//		bool receive_failure;
//		bool permission_denied;
//		bool invalid_otp;
//		unsigned int password_length;
//		WalletErrors wallet_return;
//		char ping_greeting[sizeof(message_buffer.ping.greeting)];
//		bool has_ping_greeting;
//		int lang;
//		bool use_seed;
//	//	uint8_t testSeed[] = {};
//		uint8_t langHold[1];
//
//	 backupWallet(message_buffer.backup_wallet.is_encrypted, message_buffer.backup_wallet.device);
// }

/*
bool pseudoPacketNewWallet(int strength, int level)
{
	bool cancelWalletCreation = false;
	uint16_t message_id;
	union MessageBufferUnion message_buffer;
	bool receive_failure;
	bool permission_denied;
	unsigned int password_length;
	WalletErrors wallet_return;
	int lang;
	bool use_seed;
	bool trans_pin_used = false;
	uint32_t wallet_number_derived;
	uint32_t wallet_number_hidden;

	bool is_hidden;
	bool ask_hidden = true;

	char *wallet_numberChar;

//	int strength;
	char *strengthChar;
//	int level;
	char *levelChar;


	int passwordMatch;

//	void __WFE(void);

	message_id = PACKET_TYPE_NEW_WALLET;


	memset(&message_buffer, 0, sizeof(message_buffer));


	wallet_number_derived = countExistingWallets();

	 while (checkWalletSlotIsNotEmpty(wallet_number_derived))
	 {
		 wallet_number_derived--;
	 }


	// Create new wallet.
	field_hash_set = false;
	memset(field_hash, 0, sizeof(field_hash));
	memset(temp_field_hash, 0, sizeof(temp_field_hash));
	message_buffer.new_wallet.password.funcs.decode = &hashFieldCallback;
	message_buffer.new_wallet.password.arg = NULL;

	pin_field_hash_set = false;
	memset(pin_field_hash, 0, sizeof(pin_field_hash));
	memset(temp_pin_field_hash, 0, sizeof(temp_pin_field_hash));

	receive_failure = receiveMessage(NewWallet_fields, &(message_buffer.new_wallet));

	message_buffer.new_wallet.wallet_name.bytes[39] = '\0';

	if (!receive_failure)
	{
		permission_denied = buttonInterjection(ASKUSER_NEW_WALLET_2); // need to ask hidden/level/strength
		if (!permission_denied)
		{
			if(strength==0)
			{
				strengthChar = inputInterjectionNoAck(ASKUSER_NEW_WALLET_STRENGTH);
				if(strengthChar == 'N')
				{
					cancelWalletCreation = true;
				}
				else
				{
					int strengthNum;
					strengthNum = strengthChar - '0';
					if(strengthNum==1){
						strength = 128;
					}else if(strengthNum==2){
						strength = 192;
					}else if(strengthNum==3){
						strength = 256;
					}
				}
			}

			if (!cancelWalletCreation)
			{
				if (field_hash_set)
				{
					if(level==0)
					{
						levelChar = inputInterjectionNoAck(ASKUSER_NEW_WALLET_LEVEL);
						if(levelChar == 'N')
						{
							cancelWalletCreation = true;
						}

						level = levelChar - '0';
					}

					if (!cancelWalletCreation)
					{
						if(ask_hidden)
						{
							is_hidden = buttonInterjectionNoAck(ASKUSER_NEW_WALLET_IS_HIDDEN);
							if(!is_hidden){
								message_buffer.new_wallet.is_hidden = true;
								wallet_number_derived = 0;

								do{
									wallet_numberChar = inputInterjectionNoAck(ASKUSER_NEW_WALLET_NUMBER);
									wallet_numberChar = getInput(false,false);
									wallet_number_derived = atoi(wallet_numberChar);
								}while (wallet_number_derived<51||wallet_number_derived>100);
								clearDisplay();


								char hidden_wallet_text[3];
								sprintf(hidden_wallet_text,"%lu", wallet_number_derived);

								int size_of_chosen = 2;
								if(wallet_number_derived == 100)
								{
									size_of_chosen = 3;
								}

								permission_denied = buttonInterjectionNoAckPlusData(ASKUSER_CONFIRM_HIDDEN_WALLET_NUMBER, hidden_wallet_text, size_of_chosen);
								if(permission_denied)
								{
									cancelWalletCreation = true;
								}

							}else{
								message_buffer.new_wallet.is_hidden = false;
							}
						}
					}
					if (!cancelWalletCreation)
					{

						cancelWalletCreation = passwordInterjectionAutoPIN(level);
						if(pin_field_hash_set)
						{
							trans_pin_used = true;
						}else
						{
							trans_pin_used = false;
						}

						memcpy(temp_field_hash, field_hash, 32);
						password_length = sizeof(field_hash);
					}
				}
				else
				{
					buttonInterjectionNoAck(ASKUSER_NEW_WALLET_NO_PASSWORD);
					password_length = 0; // no password
					trans_pin_used = false;
				}
			}
//			INSERT ROUTINE TO RUN BIP39 HERE	false & NULL will be replaced by true & the seed - MUST DISPLAY & VERIFY WORD LIST
			if (!cancelWalletCreation)
			{
				const char* pass = 0;
				uint8_t seed[512 / 8] = { };

				uint8_t empty[512 / 8] = { };
	//			uint8_t i;
	//			uint8_t one_byte; // current byte of seed
	//			char strBits[3];
	//			char lineTemp[64] = { };
		#ifdef FIXED_SEED_DANA
					const char * mnem128 = DANA_SEED_2;
		#endif


		#ifdef FIXED_SEED_NIK
					const char * mnem128 = NIK_SEED_1;
		#endif
				int len = strength / 8;
				int mlen = len * 3 / 4;

		#ifndef FIXED_SEED_NIK
		#ifndef FIXED_SEED_DANA
				const char * mnem128 = mnemonic_generate(strength);
		#endif
		#endif
				mnemonic_to_seed(mnem128, pass, seed); // creating the seed BEFORE displaying mnemonic seems to generate the CORRECT seed!
		#ifdef DISPLAY_SEED
				displayMnemonic(mnem128, mlen); //################################################################################################# REMOVE THIS after test
		#endif

				use_seed = true;
				wallet_return = newWallet(
					wallet_number_derived,
					message_buffer.new_wallet.wallet_name.bytes,
					use_seed,
					seed,
					message_buffer.new_wallet.is_hidden,
					field_hash,
					password_length,
					pin_field_hash,
					trans_pin_used);
				translateWalletError(wallet_return);
				if(wallet_return == WALLET_NO_ERROR)
				{
					showQRcode(0,0,0);
				}
			}else if(cancelWalletCreation)
			{
				initDisplay();
				overlayBatteryStatus(BATT_VALUE_DISPLAY);
				writeEinkNoDisplay("CANCELING",  COL_1_X, LINE_0_Y, "NO WALLET CREATED",COL_1_X,LINE_1_Y, "",33,LINE_2_Y, "",COL_1_X,LINE_3_Y, "",33,LINE_3_Y);
				writeUnderline(STRIPE_X_START, STRIPE_Y_START, STRIPE_X_END, STRIPE_Y_END);
				display();
				return true;
			}
		}else if(permission_denied){
			return true;
		}
	}
	return false;
}
*/


 bool pseudoPacketNewWallet(int strength, int level)
 {
 	bool cancelWalletCreation = false;
 	uint16_t message_id;
 	union MessageBufferUnion message_buffer;
 	bool receive_failure;
 	bool permission_denied;
 	unsigned int password_length;
 	WalletErrors wallet_return;
 	int lang;
 	bool use_seed;
 	bool trans_pin_used = false;
 	uint32_t wallet_number_derived;
 	uint32_t wallet_number_hidden;

 	bool is_hidden;
 	bool is_mnemonic_password;
 	bool ask_hidden = true;
 	bool ask_mnemonic_password = true;
 	bool use_mnemonic_pass;

 	char *wallet_numberChar;

 //	int strength;
 	char *strengthChar;
 //	int level;
 	char *levelChar;


 	int passwordMatch;

 //	void __WFE(void);

 	message_id = PACKET_TYPE_NEW_WALLET;


 	memset(&message_buffer, 0, sizeof(message_buffer));


 	wallet_number_derived = countExistingWallets();

 	 while (checkWalletSlotIsNotEmpty(wallet_number_derived))
 	 {
 		 wallet_number_derived--;
 	 }


 	// Create new wallet.
 	field_hash_set = false;
 	memset(field_hash, 0, sizeof(field_hash));
 	memset(temp_field_hash, 0, sizeof(temp_field_hash));
 	message_buffer.new_wallet.password.funcs.decode = &hashFieldCallback;
 	message_buffer.new_wallet.password.arg = NULL;

 	pin_field_hash_set = false;
 	memset(pin_field_hash, 0, sizeof(pin_field_hash));
 	memset(temp_pin_field_hash, 0, sizeof(temp_pin_field_hash));


 	receive_failure = receiveMessage(NewWallet_fields, &(message_buffer.new_wallet));

 	message_buffer.new_wallet.wallet_name.bytes[39] = '\0';

 	if (!receive_failure)
 	{
 		permission_denied = buttonInterjection(ASKUSER_NEW_WALLET_2); // need to ask hidden/level/strength
 		if (!permission_denied)
 		{
 			if(strength==0)
 			{
 				strengthChar = inputInterjectionNoAck(ASKUSER_NEW_WALLET_STRENGTH);
 				if(strengthChar == 'N')
 				{
 					cancelWalletCreation = true;
 				}
 				else
 				{
 					int strengthNum;
 					strengthNum = strengthChar - '0';
 					if(strengthNum==1){
 						strength = 128;
 					}else if(strengthNum==2){
 						strength = 192;
 					}else if(strengthNum==3){
 						strength = 256;
 					}
 				}
 			}

 			if (!cancelWalletCreation)
 			{
 				if (field_hash_set)
 				{
 					if(level==0)
 					{
 						levelChar = inputInterjectionNoAck(ASKUSER_NEW_WALLET_LEVEL);
 						if(levelChar == 'N')
 						{
 							cancelWalletCreation = true;
 						}

 						level = levelChar - '0';
 					}

 					if (!cancelWalletCreation)
 					{
 						if(ask_hidden)
 						{
 							is_hidden = buttonInterjectionNoAck(ASKUSER_NEW_WALLET_IS_HIDDEN);
 							if(!is_hidden){
 								message_buffer.new_wallet.is_hidden = true;
 								wallet_number_derived = 0;

 								do{
 									wallet_numberChar = inputInterjectionNoAck(ASKUSER_NEW_WALLET_NUMBER);
 									wallet_numberChar = getInput(false,false);
 									wallet_number_derived = atoi(wallet_numberChar);
 								}while (wallet_number_derived<51||wallet_number_derived>100);
 								clearDisplay();


 								char hidden_wallet_text[3];
 								sprintf(hidden_wallet_text,"%lu", wallet_number_derived);

 								int size_of_chosen = 2;
 								if(wallet_number_derived == 100)
 								{
 									size_of_chosen = 3;
 								}

 								permission_denied = buttonInterjectionNoAckPlusData(ASKUSER_CONFIRM_HIDDEN_WALLET_NUMBER, hidden_wallet_text, size_of_chosen);
 								if(permission_denied)
 								{
 									cancelWalletCreation = true;
 								}

 							}else{
 								message_buffer.new_wallet.is_hidden = false;
 							}
 						}
 					}
 					if (!cancelWalletCreation)
 					{

 						cancelWalletCreation = passwordInterjectionAutoPIN(level);
 						if(pin_field_hash_set)
 						{
 							trans_pin_used = true;
 						}else
 						{
 							trans_pin_used = false;
 						}

 						memcpy(temp_field_hash, field_hash, 32);
 						password_length = sizeof(field_hash);
 					}
// 					if (!cancelWalletCreation)
// 					{
// 						if(ask_mnemonic_password)
// 						{
// 							is_mnemonic_password = buttonInterjectionNoAck(ASKUSER_NEW_WALLET_IS_MNEMONIC_PASSWORD);
// 							if(!is_mnemonic_password){
// 								message_buffer.new_wallet.is_hidden = true;
// 								wallet_number_derived = 0;
//
// 								do{
// 									wallet_numberChar = inputInterjectionNoAck(ASKUSER_NEW_WALLET_NUMBER);
// 									wallet_numberChar = getInput(false,false);
// 									wallet_number_derived = atoi(wallet_numberChar);
// 								}while (wallet_number_derived<51||wallet_number_derived>100);
// 								clearDisplay();
//
//
// 								char hidden_wallet_text[3];
// 								sprintf(hidden_wallet_text,"%lu", wallet_number_derived);
//
// 								int size_of_chosen = 2;
// 								if(wallet_number_derived == 100)
// 								{
// 									size_of_chosen = 3;
// 								}
//
// 								permission_denied = buttonInterjectionNoAckPlusData(ASKUSER_CONFIRM_HIDDEN_WALLET_NUMBER, hidden_wallet_text, size_of_chosen);
// 								if(permission_denied)
// 								{
// 									cancelWalletCreation = true;
// 								}
//
// 							}else{
// 								message_buffer.new_wallet.is_hidden = false;
// 							}
// 						}
// 					}

 				}
 				else
 				{
 					buttonInterjectionNoAck(ASKUSER_NEW_WALLET_NO_PASSWORD);
 					password_length = 0; // no password
 					trans_pin_used = false;
 				}
 			}
 //			INSERT ROUTINE TO RUN BIP39 HERE	false & NULL will be replaced by true & the seed - MUST DISPLAY & VERIFY WORD LIST
 			if (!cancelWalletCreation)
 			{
 				char* pass = 0;
 				use_mnemonic_pass = buttonInterjectionNoAck(ASKUSER_USE_MNEMONIC_PASSPHRASE);
 				if(!use_mnemonic_pass)
 				{
 					buttonInterjectionNoAckSetup(ASKUSER_ALPHA_INPUT_PREFACE);

 					pass = getInput(true, false);
 				}
 				clearDisplay();
 				uint8_t seed[512 / 8] = { };

 				uint8_t empty[512 / 8] = { };
 	//			uint8_t i;
 	//			uint8_t one_byte; // current byte of seed
 	//			char strBits[3];
 	//			char lineTemp[64] = { };
 		#ifdef FIXED_SEED_DANA
 					const char * mnem128 = DANA_SEED_2;
 		#endif


 		#ifdef FIXED_SEED_NIK
 					const char * mnem128 = NIK_SEED_1;
 		#endif
 				int len = strength / 8;
 				int mlen = len * 3 / 4;

 		#ifndef FIXED_SEED_NIK
 		#ifndef FIXED_SEED_DANA
 				const char * mnem128 = mnemonic_generate(strength);
 		#endif
 		#endif
 				mnemonic_to_seed(mnem128, pass, seed); // creating the seed BEFORE displaying mnemonic seems to generate the CORRECT seed!
 		#ifdef DISPLAY_SEED
 				displayMnemonic(mnem128, mlen); //################################################################################################# REMOVE THIS after test
 		#endif

 				use_seed = true;
 				wallet_return = newWallet(
 					wallet_number_derived,
 					message_buffer.new_wallet.wallet_name.bytes,
 					use_seed,
 					seed,
 					message_buffer.new_wallet.is_hidden,
 					field_hash,
 					password_length,
 					pin_field_hash,
 					trans_pin_used);
 				translateWalletError(wallet_return);
 				if(wallet_return == WALLET_NO_ERROR)
 				{
 					showQRcode(0,0,0);
 				}
 			}else if(cancelWalletCreation)
 			{
 				initDisplay();
 				overlayBatteryStatus(BATT_VALUE_DISPLAY);
 				writeEinkNoDisplay("CANCELING",  COL_1_X, LINE_1_Y, "NO WALLET CREATED",COL_1_X,LINE_2_Y, "",33,LINE_2_Y, "",COL_1_X,LINE_3_Y, "",33,LINE_3_Y);
 				writeUnderline(STRIPE_X_START, STRIPE_Y_START, STRIPE_X_END, STRIPE_Y_END);
 				display();
 				return true;
 			}
 		}else if(permission_denied){
 			return true;
 		}
 	}
 	return false;
 }


bool pseudoPacketRestoreWallet(int strength, int level)
{
	bool cancelWalletCreation = false;
	uint16_t message_id;
	union MessageBufferUnion message_buffer;
	bool receive_failure;
	bool permission_denied;
	unsigned int password_length;
	WalletErrors wallet_return;
	int lang;
	bool use_seed;
	bool trans_pin_used;
	uint32_t wallet_number_derived;


	bool is_hidden;
	bool ask_hidden = true;
	bool use_mnemonic_pass;

//	int strength;
	char *strengthChar;
	char *wallet_numberChar;
//	int level;
	char *levelChar;

	int passwordMatch;

//	void __WFE(void);

	message_id = PACKET_TYPE_RESTORE_WALLET_DEVICE;


	memset(&message_buffer, 0, sizeof(message_buffer));

	wallet_number_derived = countExistingWallets();
	while (checkWalletSlotIsNotEmpty(wallet_number_derived))
	{
	 wallet_number_derived--;
	}

	// Create new wallet.
	field_hash_set = true;
	memset(field_hash, 0, sizeof(field_hash));
	memset(temp_field_hash, 0, sizeof(temp_field_hash));
	message_buffer.new_wallet.password.funcs.decode = &hashFieldCallback;
	message_buffer.new_wallet.password.arg = NULL;

	pin_field_hash_set = false;
	memset(pin_field_hash, 0, sizeof(pin_field_hash));
	memset(temp_pin_field_hash, 0, sizeof(temp_pin_field_hash));


	receive_failure = receiveMessage(NewWallet_fields, &(message_buffer.new_wallet));

	message_buffer.new_wallet.wallet_name.bytes[39] = '\0';

	if (!receive_failure)
	{
		permission_denied = buttonInterjectionNoAck(ASKUSER_RESTORE_WALLET_DEVICE); // need to ask hidden/level/strength
		if (!permission_denied)
		{

//			if(1)
//			{
//				wallet_numberChar = inputInterjectionNoAck(ASKUSER_NEW_WALLET_NUMBER);
//				message_buffer.new_wallet.wallet_number = atoi(wallet_numberChar);
//			}
			if(strength==0)
			{
				strengthChar = inputInterjectionNoAck(ASKUSER_NEW_WALLET_STRENGTH);
				int strengthNum;
				if(strengthChar == 'N')
				{
					cancelWalletCreation = true;
				}
				strengthNum = strengthChar - '0';
				if(strengthNum==1){
					strength = 128;
				}else if(strengthNum==2){
					strength = 192;
				}else if(strengthNum==3){
					strength = 256;
				}
			}

			if (!cancelWalletCreation)
			{
				if (field_hash_set)
				{
					if(level==0)
					{
						levelChar = inputInterjectionNoAck(ASKUSER_NEW_WALLET_LEVEL);
						if(levelChar == 'N')
						{
							cancelWalletCreation = true;
						}

						level = levelChar - '0';
					}

					if (!cancelWalletCreation)
					{
						if(ask_hidden)
						{
							is_hidden = buttonInterjectionNoAck(ASKUSER_NEW_WALLET_IS_HIDDEN);
							if(!is_hidden){
								message_buffer.new_wallet.is_hidden = true;
								wallet_number_derived = 0;

								do{
									wallet_numberChar = inputInterjectionNoAck(ASKUSER_NEW_WALLET_NUMBER);
									wallet_numberChar = getInput(false,false);
									wallet_number_derived = atoi(wallet_numberChar);
								}while (wallet_number_derived<51||wallet_number_derived>100);

								char hidden_wallet_text[3];
								sprintf(hidden_wallet_text,"%lu", wallet_number_derived);

								int size_of_chosen = 2;
								if(wallet_number_derived == 100)
								{
									size_of_chosen = 3;
								}

								permission_denied = buttonInterjectionNoAckPlusData(ASKUSER_CONFIRM_HIDDEN_WALLET_NUMBER, hidden_wallet_text, size_of_chosen);
								if(permission_denied)
								{
									cancelWalletCreation = true;
								}


							}else{
								message_buffer.new_wallet.is_hidden = false;
							}
						}
					}
					if (!cancelWalletCreation)
					{

						cancelWalletCreation = passwordInterjectionAutoPIN(level);
						if(pin_field_hash_set)
						{
							trans_pin_used = true;
						}else
						{
							trans_pin_used = false;
						}

						memcpy(temp_field_hash, field_hash, 32);
						password_length = sizeof(field_hash);
					}
				}
				else
				{
					buttonInterjectionNoAck(ASKUSER_NEW_WALLET_NO_PASSWORD);
					password_length = 0; // no password
					trans_pin_used = false;
				}
			}
//			INSERT ROUTINE TO RUN BIP39 HERE	false & NULL will be replaced by true & the seed - MUST DISPLAY & VERIFY WORD LIST
			if (!cancelWalletCreation)
			{
 				char* pass = 0;
 				use_mnemonic_pass = buttonInterjectionNoAck(ASKUSER_USE_MNEMONIC_PASSPHRASE);
 				if(!use_mnemonic_pass)
 				{
 					buttonInterjectionNoAckSetup(ASKUSER_ALPHA_INPUT_PREFACE);

 					pass = getInput(true, false);
 				}
 				clearDisplay();
				uint8_t seed[512 / 8] = { };

				uint8_t empty[512 / 8] = { };
	//			uint8_t i;
	//			uint8_t one_byte; // current byte of seed
	//			char strBits[3];
	//			char lineTemp[64] = { };
				const char * mnem128;
		#ifdef FIXED_SEED_DANA
					mnem128 = DANA_SEED_2;
		#endif


		#ifdef FIXED_SEED_NIK
					const char * mnem128 = NIK_SEED_1;
		#endif

		#ifndef FIXED_SEED_NIK
		#ifndef FIXED_SEED_DANA
	//				const char * mnem128 = mnemonic_generate(strength);
		#endif
		#endif
				int len = strength / 8;
				int mlen = len * 3 / 4;


				char *inputMethod = inputInterjectionNoAck(ASKUSER_RESTORE_WALLET_DEVICE_INPUT_TYPE);
				if(inputMethod == '1')
				{
					mnem128 = mnemonic_generate_from_input(strength);
				}else if(inputMethod == '2')
				{
					mnem128 = mnemonic_input_stacker(mlen);
				}

				mnemonic_to_seed(mnem128, pass, seed); // creating the seed BEFORE displaying mnemonic seems to generate the CORRECT seed!
		#ifdef DISPLAY_SEED
				displayMnemonic(mnem128, mlen); //################################################################################################# REMOVE THIS after test
		#endif
				uint8_t defaultName[40] = {	82,101,115,116,111,114,101,100, 32, 32,
											32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
											32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
											32, 32, 32, 32, 32, 32, 32, 32, 32, 00};
	//			message_buffer.new_wallet.wallet_number = 10;

				use_seed = true;
				wallet_return = newWallet(
					wallet_number_derived,
					defaultName,
					use_seed,
					seed,
					message_buffer.new_wallet.is_hidden,
					field_hash,
					password_length,
					pin_field_hash,
					trans_pin_used);
				translateWalletError(wallet_return);
				if(wallet_return == WALLET_NO_ERROR)
				{
					showQRcode(0,0,0);
				}
			}else if(cancelWalletCreation)
			{
				initDisplay();
				overlayBatteryStatus(BATT_VALUE_DISPLAY);
				writeEinkNoDisplay("CANCELING",  COL_1_X, LINE_0_Y, "NO WALLET CREATED",COL_1_X,LINE_1_Y, "",33,LINE_2_Y, "",COL_1_X,LINE_3_Y, "",33,LINE_3_Y);
				writeUnderline(STRIPE_X_START, STRIPE_Y_START, STRIPE_X_END, STRIPE_Y_END);
				display();
				return true;
			}
		}else if(permission_denied){
			return true;
		}
	}
	return false;
}


