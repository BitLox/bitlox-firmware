/** \file transaction.c
  *
  * \brief Contains functions specific to Bitcoin transactions.
  *
  * There are two main things which are dealt with in this file.
  * The first is the parsing of Bitcoin transactions. During the parsing
  * process, useful stuff (such as output addresses and amounts) is
  * extracted. See the code of parseTransactionInternal() for the guts.
  *
  * The second is the generation of Bitcoin-compatible signatures. Bitcoin
  * uses OpenSSL to generate signatures, and OpenSSL insists on encapsulating
  * the "r" and "s" values (see ecdsaSign()) in DER format. See the code of
  * signTransaction() for the guts.
  *
  * This file is licensed as described by the file LICENCE.
  */

#ifdef TEST
#include <stdlib.h>
#include <stdio.h>
#endif // #ifdef TEST

#ifdef TEST_TRANSACTION
#include "test_helpers.h"
#include "stream_comm.h"
#include "wallet.h"
#endif // #ifdef TEST_TRANSACTION

//#define DISPLAY_PARMS


#include "common.h"
#include "endian.h"
#include "ecdsa.h"
#include "baseconv.h"
#include "sha256.h"
#include "bignum256.h"
#include "prandom.h"
#include "hwinterface.h"
#include "transaction.h"
//#include "stream_comm.h"

/** The maximum size of a transaction (in bytes) which parseTransaction()
  * is prepared to handle. */
#define MAX_TRANSACTION_SIZE	2000000

/** The maximum number of inputs that the transaction parser is prepared
  * to handle. This should be small enough that a transaction with the
  * maximum number of inputs is still less than #MAX_TRANSACTION_SIZE bytes in
  * size.
  * \warning This must be < 65536, otherwise an integer overflow may occur.
  */
//#define MAX_INPUTS				5000

/** The maximum number of outputs that the transaction parser is prepared
  * to handle. This should be small enough that a transaction with the
  * maximum number of outputs is still less than #MAX_TRANSACTION_SIZE bytes
  * in size.
  * \warning This must be < 65536, otherwise an integer overflow may occur.
  */
#define MAX_OUTPUTS				2000

#ifdef DISPLAY_STREAM
static uint8_t bigBuffer[1000] = {};
static uint32_t bigCounter = 0;
#endif

/** The maximum amount that can appear in an output, stored as a little-endian
  * multi-precision integer. This represents 21 million BTC. */
static const uint8_t max_money[] = {
0x00, 0x40, 0x07, 0x5A, 0xF0, 0x75, 0x07, 0x00};

/** The transaction fee amount, calculated as output amounts subtracted from
  * input amounts. */
static uint8_t transaction_fee_amount[8];

/** Where the transaction parser is within a transaction. 0 = first byte,
  * 1 = second byte etc. */
static uint32_t transaction_data_index;

/** The total length of the transaction being parsed, in number of bytes. */
static uint32_t transaction_length;

/** If this is true, then as the transaction contents are read from the
  * stream device, they will not be included in the calculation of the
  * transaction hash (see parseTransaction() for what this is all about).
  * If this is false, then they will be included. */
static bool suppress_transaction_hash;

/** If this is false, then as the transaction contents are read from the
  * stream device, they will not be included in the calculation of the
  * transaction hash or the signature hash. If this is true, then they
  * will be included. This is used to stop #sig_hash_hs_ptr
  * and #transaction_hash_hs_ptr from being written to if they don't point
  * to a valid hash state. */
static bool hs_ptr_valid;
static bool sig_ptr_valid;

/** Pointer to hash state used to calculate the signature
  * hash (see parseTransaction() for what this is all about).
  * \warning If this does not point to a valid hash state structure, ensure
  *          that #hs_ptr_valid is false to
  *          stop getTransactionBytes() from attempting to dereference this.
  */
static HashState *sig_hash_hs_ptr;
static HashState *sig_hash_hs_ptr_array[MAX_INPUTS];

/** Pointer to hash state used to calculate the transaction
  * hash (see parseTransaction() for what this is all about).
  * \warning If this does not point to a valid hash state structure, ensure
  *          that #hs_ptr_valid is false to
  *          stop getTransactionBytes() from attempting to dereference this.
  */
static HashState *transaction_hash_hs_ptr;

/** Get transaction data by reading from the stream device, checking that
  * the read operation won't go beyond the end of the transaction data.
  * 
  * Since all transaction data is read using this function, the updating
  * of #sig_hash_hs_ptr and #transaction_hash_hs_ptr is also done.
  * \param buffer An array of bytes which will be filled with the transaction
  *               data (if everything goes well). It must have space for
  *               length bytes.
  * \param length The number of bytes to read from the stream device.
  * \return false on success, true if a stream read error occurred or if the
  *         read would go beyond the end of the transaction data.
  */
static bool getTransactionBytes(uint8_t *buffer, uint8_t length)
{
	uint8_t i;
	uint8_t one_byte;

	if (transaction_data_index > (0xffffffff - (uint32_t)length))
	{
		// transaction_data_index + (uint32_t)length will overflow.
		// Since transaction_length <= 0xffffffff, this implies that the read
		// will go past the end of the transaction.
		return true; // trying to read past end of transaction
	}
	if (transaction_data_index + (uint32_t)length > transaction_length)
	{
		return true; // trying to read past end of transaction
	}
	else
	{
		for (i = 0; i < length; i++)
		{
			one_byte = streamGetOneByte();
			buffer[i] = one_byte;
			if (hs_ptr_valid)
			{
				sha256WriteByte(sig_hash_hs_ptr, one_byte);
				sha256WriteByte(sig_hash_hs_ptr_array[0], one_byte);

				if (!suppress_transaction_hash)
				{
					sha256WriteByte(transaction_hash_hs_ptr, one_byte);
				}
			}
			transaction_data_index++;
#ifdef DISPLAY_STREAM
			bigBuffer[bigCounter] = one_byte;
			bigCounter++;
#endif

		}
		return false;
	}
}
/** Get transaction data by reading from the stream device, checking that
  * the read operation won't go beyond the end of the transaction data.
  *
  * Since all transaction data is read using this function, the updating
  * of #sig_hash_hs_ptr and #transaction_hash_hs_ptr is also done.
  * \param buffer An array of bytes which will be filled with the transaction
  *               data (if everything goes well). It must have space for
  *               length bytes.
  * \param length The number of bytes to read from the stream device.
  * \return false on success, true if a stream read error occurred or if the
  *         read would go beyond the end of the transaction data.
  */
/** Get transaction data by reading from the stream device, checking that
  * the read operation won't go beyond the end of the transaction data.
  *
  * Since all transaction data is read using this function, the updating
  * of #sig_hash_hs_ptr and #transaction_hash_hs_ptr is also done.
  * \param buffer An array of bytes which will be filled with the transaction
  *               data (if everything goes well). It must have space for
  *               length bytes.
  * \param length The number of bytes to read from the stream device.
  * \return false on success, true if a stream read error occurred or if the
  *         read would go beyond the end of the transaction data.
  */
/** Checks whether the transaction parser is at the end of the transaction
  * data.
  * \return false if not at the end of the transaction data, true if at the
  *         end of the transaction data.
  */
static bool getTransactionBytesMulti(uint8_t *buffer, uint8_t length, uint32_t number_of_inputs)
{
	uint8_t i;
	uint32_t j;
	uint8_t one_byte;

	if (transaction_data_index > (0xffffffff - (uint32_t)length))
	{
		// transaction_data_index + (uint32_t)length will overflow.
		// Since transaction_length <= 0xffffffff, this implies that the read
		// will go past the end of the transaction.
		return true; // trying to read past end of transaction
	}
	if (transaction_data_index + (uint32_t)length > transaction_length)
	{
		return true; // trying to read past end of transaction
	}
	else
	{
		for (i = 0; i < length; i++)
		{
			one_byte = streamGetOneByte();
			buffer[i] = one_byte;
			if (hs_ptr_valid)
			{
				sha256WriteByte(sig_hash_hs_ptr, one_byte);
//				write to the iteration-th hash, the others should be blanked.
				if(sig_ptr_valid)
				{
					for(j = 0; j < number_of_inputs; j++)
					{
						sha256WriteByte(sig_hash_hs_ptr_array[j], one_byte);
					}
				}
				if (!suppress_transaction_hash)
				{
					sha256WriteByte(transaction_hash_hs_ptr, one_byte);
				}
			}
			transaction_data_index++;
#ifdef DISPLAY_STREAM
			bigBuffer[bigCounter] = one_byte;
			bigCounter++;
#endif

		}
		return false;
	}
}

static bool getTransactionBytesMultiFilter(uint8_t *buffer, uint8_t length, uint32_t number_of_inputs, uint32_t iteration, bool is_script_length)
{
	uint8_t i;
	uint32_t j;
	uint8_t one_byte;

	if (transaction_data_index > (0xffffffff - (uint32_t)length))
	{
		// transaction_data_index + (uint32_t)length will overflow.
		// Since transaction_length <= 0xffffffff, this implies that the read
		// will go past the end of the transaction.
		return true; // trying to read past end of transaction
	}
	if (transaction_data_index + (uint32_t)length > transaction_length)
	{
		return true; // trying to read past end of transaction
	}
	else
	{


		for (i = 0; i < length; i++)
		{
			one_byte = streamGetOneByte();
			buffer[i] = one_byte;
			if (hs_ptr_valid )
			{
				sha256WriteByte(sig_hash_hs_ptr, one_byte);
//				write to the iteration-th hash, the others should be blanked.
				if(sig_ptr_valid)
				{
					for(j = 0; j < number_of_inputs; j++)
					{
						if(j == iteration)
						{
							sha256WriteByte(sig_hash_hs_ptr_array[j], one_byte);
						}
						else if(is_script_length)
						{
							sha256WriteByte(sig_hash_hs_ptr_array[j], 0);
						}
					}
				}
				if (!suppress_transaction_hash)
				{
					sha256WriteByte(transaction_hash_hs_ptr, one_byte);
				}
			}
			transaction_data_index++;
#ifdef DISPLAY_STREAM
			bigBuffer[bigCounter] = one_byte;
			bigCounter++;
#endif

		}
		return false;
	}
}

static bool isEndOfTransactionData(void)
{
	if (transaction_data_index >= transaction_length)
	{
		return true;
	}
	else
	{
		return false;
	}
}


bool showTransactionData(void)
{
	writeEinkDisplay("showTransactionData", false, 10, 10, "",false,10,30, "",false,10,50, "",false,10,70, "",false,0,0);

#ifdef DISPLAY_STREAM
	displayBigHexStream(bigBuffer, bigCounter);
#endif

	return true;
}


/** Parse a variable-sized integer within a transaction. Variable sized
  * integers are commonly used to represent counts or sizes in Bitcoin
  * transactions.
  * This only supports unsigned variable-sized integers up to a maximum
  * value of 2 ^ 32 - 1.
  * \param out The value of the integer will be written to here.
  * \return false on success, true to indicate an error occurred (unexpected
  *         end of transaction data or the value of the integer is too large).
  */
bool getVarInt(uint32_t *out, uint8_t *input)
{
	uint8_t temp[4];

	if (getTransactionBytes(temp, 1))
	{
		return true; // unexpected end of transaction data
	}
	if (temp[0] < 0xfd)
	{
		*out = temp[0];
	}
	else if (temp[0] == 0xfd)
	{
		if (getTransactionBytes(temp, 2))
		{
			return true; // unexpected end of transaction data
		}
		*out = (uint32_t)(temp[0]) | ((uint32_t)(temp[1]) << 8);
	}
	else if (temp[0] == 0xfe)
	{
		if (getTransactionBytes(temp, 4))
		{
			return true; // unexpected end of transaction data
		}
		*out = readU32LittleEndian(temp);
	}
	else
	{
		return true; // varint is too large
	}
	return false; // success
}

/** Parse a variable-sized integer within a transaction. Variable sized
  * integers are commonly used to represent counts or sizes in Bitcoin
  * transactions.
  * This only supports unsigned variable-sized integers up to a maximum
  * value of 2 ^ 32 - 1.
  * \param out The value of the integer will be written to here.
  * \return false on success, true to indicate an error occurred (unexpected
  *         end of transaction data or the value of the integer is too large).
  */
static bool getVarIntMulti(uint32_t *out, uint8_t number_of_inputs)
{
	uint8_t temp[4];

	if (getTransactionBytesMulti(temp, 1, number_of_inputs))
	{
		return true; // unexpected end of transaction data
	}
	if (temp[0] < 0xfd)
	{
		*out = temp[0];
	}
	else if (temp[0] == 0xfd)
	{
		if (getTransactionBytesMulti(temp, 2, number_of_inputs))
		{
			return true; // unexpected end of transaction data
		}
		*out = (uint32_t)(temp[0]) | ((uint32_t)(temp[1]) << 8);
	}
	else if (temp[0] == 0xfe)
	{
		if (getTransactionBytesMulti(temp, 4, number_of_inputs))
		{
			return true; // unexpected end of transaction data
		}
		*out = readU32LittleEndian(temp);
	}
	else
	{
		return true; // varint is too large
	}
	return false; // success
}

/** Parse a variable-sized integer within a transaction. Variable sized
  * integers are commonly used to represent counts or sizes in Bitcoin
  * transactions.
  * This only supports unsigned variable-sized integers up to a maximum
  * value of 2 ^ 32 - 1.
  * \param out The value of the integer will be written to here.
  * \return false on success, true to indicate an error occurred (unexpected
  *         end of transaction data or the value of the integer is too large).
  */
static bool getVarIntMultiFilter(uint32_t *out, uint8_t number_of_inputs, uint32_t iteration, bool is_script_length)
{
	uint8_t temp[4];

	if (getTransactionBytesMultiFilter(temp, 1, number_of_inputs, iteration, is_script_length))
	{
		return true; // unexpected end of transaction data
	}
	if (temp[0] < 0xfd)
	{
		*out = temp[0];
	}
	else if (temp[0] == 0xfd)
	{
		if (getTransactionBytesMultiFilter(temp, 2, number_of_inputs, iteration, is_script_length))
		{
			return true; // unexpected end of transaction data
		}
		*out = (uint32_t)(temp[0]) | ((uint32_t)(temp[1]) << 8);
	}
	else if (temp[0] == 0xfe)
	{
		if (getTransactionBytesMultiFilter(temp, 4, number_of_inputs, iteration, is_script_length))
		{
			return true; // unexpected end of transaction data
		}
		*out = readU32LittleEndian(temp);
	}
	else
	{
		return true; // varint is too large
	}
	return false; // success
}




/** See comments for parseTransaction() for description of what this does
  * and return values. However, the guts of the transaction parser are in
  * the code to this function.
  *
  * This is called once for each input transaction and once for the spending
  * transaction.
  * \param sig_hash See parseTransaction().
  * \param transaction_hash See parseTransaction().
  * \param is_ref_out On success, this will be written with true
  *                   if the transaction parser parsed an input (i.e.
  *                   referenced by input of spending) transaction. This will
  *                   be written with false if the transaction parser parsed
  *                   the main (i.e. spending) transaction.
  * \param ref_compare_hs Reference compare hash. This is used to check that
  *                       the input transactions match the references in the
  *                       main transaction.
  * \return See parseTransaction().
  */
static TransactionErrors parseTransactionInternalMulti(uint8_t sig_hash[][32], BigNum256 transaction_hash, bool *is_ref_out, HashState *ref_compare_hs, uint32_t number_of_inputs, uint8_t sig_hash_counter, char *change_address_ptr)
{
	uint8_t temp[32];
	uint8_t ref_compare_hash[32];
	uint32_t num_inputs;
	uint32_t num_outputs;
	uint32_t num_outputs_originating;
	uint32_t script_length;
	uint8_t input_reference_num_buffer[4];
	uint32_t i;
	uint8_t j;
	uint32_t k;
	uint32_t m;
	uint32_t output_num_select;
	bool is_ref;
	char text_amount[TEXT_AMOUNT_LENGTH];
	char text_address[TEXT_ADDRESS_LENGTH];
	char text_address_static[TEXT_ADDRESS_LENGTH] = {};
	char text_address_to_compare[number_of_inputs][TEXT_ADDRESS_LENGTH];
	uint8_t sig_hash_single[32];
	char script_length_char[16];
	int address_match;
	int address_match_specific;
	int address_match_latch;
	int n;
	int p = 0;
	int q;


	if (transaction_length > MAX_TRANSACTION_SIZE)
	{
		return TRANSACTION_TOO_LARGE; // transaction too large
	}

	// Suppress hashing of input stream, otherwise the is_ref byte and
	// output number (which are not part of the transaction data) will
	// be included in the signature/transaction hash.
	hs_ptr_valid = false;

	sig_ptr_valid = false;

	if (getTransactionBytesMulti(temp, 1, number_of_inputs))
	{
		return TRANSACTION_INVALID_FORMAT; // transaction truncated
	}
	if (temp[0] != 0)
	{
		is_ref = true;
	}
	else
	{
		is_ref = false;
	}
	*is_ref_out = is_ref;

	output_num_select = 0;
	if (is_ref)
	{
		// Get output number to add to total amount.
		if (getTransactionBytesMulti(temp, 4, number_of_inputs))
		{
#ifdef DISPLAY_PARMS
			writeEinkDisplay("invalid format", false, 10, 10, "",false,10,30, "",false,10,50, "",false,10,70, "",false,0,0);
#endif
			return TRANSACTION_INVALID_FORMAT; // transaction truncated
		}
		for (j = 0; j < 4; j++)
		{
			sha256WriteByte(ref_compare_hs, temp[j]);  // ##not quite sure what this is hashing for
		}
		output_num_select = readU32LittleEndian(temp);

	}
	//##################################################################################################################################
	//##################################################################################################################################
	else
	{
		// Generate hash of input transaction references for comparison.
		sha256FinishDouble(ref_compare_hs);
		writeHashToByteArray(ref_compare_hash, ref_compare_hs, false);
		sha256Begin(ref_compare_hs);
	}

	sha256Begin(sig_hash_hs_ptr);
	if(!is_ref)
	{
		sig_ptr_valid = true;
		for(m = 0; m < number_of_inputs; m++)
		{
			sha256Begin(sig_hash_hs_ptr_array[m]);
		}
	}
	else
	{
		sig_ptr_valid = false;
	}
	sha256Begin(transaction_hash_hs_ptr);
	hs_ptr_valid = true;
	suppress_transaction_hash = false;

	// Check version.
	if (getTransactionBytesMulti(temp, 4, number_of_inputs))
	{
		#ifdef DISPLAY_PARMS
				writeEinkDisplay("4", false, 10, 10, "",false,10,30, "",false,10,50, "",false,10,70, "",false,0,0);
		#endif
		return TRANSACTION_INVALID_FORMAT; // transaction truncated
	}
	if (readU32LittleEndian(temp) != 0x00000001)
	{
		#ifdef DISPLAY_PARMS
				writeEinkDisplay("5", false, 10, 10, "",false,10,30, "",false,10,50, "",false,10,70, "",false,0,0);
		#endif
		return TRANSACTION_WRONG_VERSION; // unsupported transaction version
	}

	// Get number of inputs.
	if (getVarIntMulti(&num_inputs, number_of_inputs))
	{
		#ifdef DISPLAY_PARMS
				writeEinkDisplay("6", false, 10, 10, "",false,10,30, "",false,10,50, "",false,10,70, "",false,0,0);
		#endif
		return TRANSACTION_INVALID_FORMAT; // transaction truncated or varint too big
	}
	if (num_inputs == 0)
	{
		#ifdef DISPLAY_PARMS
				writeEinkDisplay("7", false, 10, 10, "",false,10,30, "",false,10,50, "",false,10,70, "",false,0,0);
		#endif
		return TRANSACTION_INVALID_FORMAT; // invalid transaction
	}
	if (num_inputs > MAX_INPUTS)
	{
		#ifdef DISPLAY_PARMS
				writeEinkDisplay("8", false, 10, 10, "",false,10,30, "",false,10,50, "",false,10,70, "",false,0,0);
		#endif
		return TRANSACTION_TOO_MANY_INPUTS; // too many inputs
	}

	// Process each input.
	for (i = 0; i < num_inputs; i++)
	{
		// Get input transaction reference hash.
		if (getTransactionBytesMulti(temp, 32, number_of_inputs))
		{
			#ifdef DISPLAY_PARMS
						writeEinkDisplay("9", false, 10, 10, "",false,10,30, "",false,10,50, "",false,10,70, "",false,0,0);
			#endif
			return TRANSACTION_INVALID_FORMAT; // transaction truncated
		}
		// Get input transaction reference number.
		if (getTransactionBytesMulti(input_reference_num_buffer, 4, number_of_inputs))
		{
			#ifdef DISPLAY_PARMS
						writeEinkDisplay("10", false, 10, 10, "",false,10,30, "",false,10,50, "",false,10,70, "",false,0,0);
			#endif
			return TRANSACTION_INVALID_FORMAT; // transaction truncated
		}
		if (!is_ref)
		{
			for (j = 0; j < 4; j++)
			{
				sha256WriteByte(ref_compare_hs, input_reference_num_buffer[j]);
			}
			for (j = 0; j < 32; j++)
			{
				sha256WriteByte(ref_compare_hs, temp[j]);
			}
		}
		// The Bitcoin protocol for signing a transaction involves replacing
		// the corresponding input script with the output script that
		// the input references. This means that the transaction data parsed
		// here will be different depending on which input is being signed
		// for. The transaction hash is supposed to be the same regardless of
		// which input is being signed for, so the calculation of the
		// transaction hash ignores input scripts.
		suppress_transaction_hash = true;
//		check if the input being examined right now is the one that is to be signed.
//		then enable the sig hash while surpressing the sig hash for others. Must we use multiple parallel hashes?
//		Surpress sig hash for all but the input being signed for

		// Get input script length. // sample is 0x8b == 139 bytes OR can be the 19 variant - IT DOESN'T MATTER!!!!
		// have to intercept this to be a zero for the not being hashed inputs


		if(!is_ref)
		{
			if (getVarIntMultiFilter(&script_length, number_of_inputs, i, true))
			{
				#ifdef DISPLAY_PARMS
							writeEinkDisplay("11", false, 10, 10, "",false,10,30, "",false,10,50, "",false,10,70, "",false,0,0);
				#endif
				return TRANSACTION_INVALID_FORMAT; // transaction truncated or varint too big
			}

			for (k = 0; k < script_length; k++)
			{
				if (getTransactionBytesMultiFilter(temp, 1, number_of_inputs, i, false))
				{
					#ifdef DISPLAY_PARMS
									writeEinkDisplay("12", false, 10, 10, "",false,10,30, "",false,10,50, "",false,10,70, "",false,0,0);
					#endif
					return TRANSACTION_INVALID_FORMAT; // transaction truncated
				}
			}
		}
		else
		{
			if (getVarIntMulti(&script_length, number_of_inputs))
			{
				#ifdef DISPLAY_PARMS
							writeEinkDisplay("11", false, 10, 10, "",false,10,30, "",false,10,50, "",false,10,70, "",false,0,0);
				#endif
				return TRANSACTION_INVALID_FORMAT; // transaction truncated or varint too big
			}
				for (k = 0; k < script_length; k++)
				{
					if (getTransactionBytesMulti(temp, 1, number_of_inputs))
					{
						#ifdef DISPLAY_PARMS
										writeEinkDisplay("12", false, 10, 10, "",false,10,30, "",false,10,50, "",false,10,70, "",false,0,0);
						#endif
						return TRANSACTION_INVALID_FORMAT; // transaction truncated
					}
				}
//			}
		}

//		unsupress sig hash
		suppress_transaction_hash = false;
		// Check sequence. Since locktime is checked below, this check
		// is probably superfluous. But it's better to be safe than sorry.
		if (getTransactionBytesMulti(temp, 4, number_of_inputs))
		{
			#ifdef DISPLAY_PARMS
						writeEinkDisplay("13", false, 10, 10, "",false,10,30, "",false,10,50, "",false,10,70, "",false,0,0);
			#endif
			return TRANSACTION_INVALID_FORMAT; // transaction truncated
		}
		if (readU32LittleEndian(temp) != 0xFFFFFFFF)
		{
			#ifdef DISPLAY_PARMS
						writeEinkDisplay("14", false, 10, 10, "",false,10,30, "",false,10,50, "",false,10,70, "",false,0,0);
			#endif
//			return TRANSACTION_NON_STANDARD; // replacement not supported //been getting some transactions WITH a locktime specified!
		}
	} // end for (i = 0; i < num_inputs; i++)


	if (!is_ref)
	{
		// Compare input references with input transactions.
		sha256FinishDouble(ref_compare_hs);
		writeHashToByteArray(temp, ref_compare_hs, false);
		if (memcmp(temp, ref_compare_hash, 32))
		{
			#ifdef DISPLAY_PARMS
						writeEinkDisplay("15", false, 10, 10, "",false,10,30, "",false,10,50, "",false,10,70, "",false,0,0);
			#endif
			return TRANSACTION_INVALID_REFERENCE; // references don't match input transactions
		}
	}

	// Get number of outputs.
	if (getVarIntMulti(&num_outputs, number_of_inputs))
	{
		#ifdef DISPLAY_PARMS
				writeEinkDisplay("16", false, 10, 10, "",false,10,30, "",false,10,50, "",false,10,70, "",false,0,0);
		#endif
		return TRANSACTION_INVALID_FORMAT; // transaction truncated or varint too big
	}
	if (num_outputs == 0)
	{
		#ifdef DISPLAY_PARMS
				writeEinkDisplay("17", false, 10, 10, "",false,10,30, "",false,10,50, "",false,10,70, "",false,0,0);
		#endif
		return TRANSACTION_INVALID_FORMAT; // invalid transaction
	}
	if (num_outputs > MAX_OUTPUTS)
	{
		#ifdef DISPLAY_PARMS
				writeEinkDisplay("18", false, 10, 10, "",false,10,30, "",false,10,50, "",false,10,70, "",false,0,0);
		#endif
		return TRANSACTION_TOO_MANY_OUTPUTS; // too many outputs
	}
	if (is_ref)
	{
		if (output_num_select >= num_outputs)
		{
			#ifdef DISPLAY_PARMS
						writeEinkDisplay("19", false, 10, 10, "",false,10,30, "",false,10,50, "",false,10,70, "",false,0,0);
			#endif
			return TRANSACTION_INVALID_REFERENCE; // bad reference number
		}

	}

	// Process each output.
	for (i = 0; i < num_outputs; i++)
	{
		// Get output amount.
		if (getTransactionBytesMulti(temp, 8, number_of_inputs))
		{
			#ifdef DISPLAY_PARMS
						writeEinkDisplay("20", false, 10, 10, "",false,10,30, "",false,10,50, "",false,10,70, "",false,0,0);
			#endif
			return TRANSACTION_INVALID_FORMAT; // transaction truncated
		}
		if (bigCompareVariableSize(temp, (uint8_t *)max_money, 8) == BIGCMP_GREATER)
		{
			#ifdef DISPLAY_PARMS
						writeEinkDisplay("21", false, 10, 10, "",false,10,30, "",false,10,50, "",false,10,70, "",false,0,0);
			#endif
			return TRANSACTION_INVALID_AMOUNT; // amount too high
		}
		if (is_ref) //commenting out the fee calculation
		{
			if (i == output_num_select)  // seems to only grab one of the values...
			{
				if (bigAddVariableSizeNoModulo(transaction_fee_amount, transaction_fee_amount, temp, 8))
				{
					#ifdef DISPLAY_PARMS
										writeEinkDisplay("22", false, 10, 10, "",false,10,30, "",false,10,50, "",false,10,70, "",false,0,0);
					#endif
					return TRANSACTION_INVALID_AMOUNT; // overflow occurred (carry occurred)
				}
			}
		}
		else
		{
			if (bigSubtractVariableSizeNoModulo(transaction_fee_amount, transaction_fee_amount, temp, 8))
			{
				#ifdef DISPLAY_PARMS
								writeEinkDisplay("23", false, 10, 10, "",false,10,30, "",false,10,50, "",false,10,70, "",false,0,0);
				#endif
				return TRANSACTION_INVALID_AMOUNT; // overflow occurred (borrow occurred)
			}
			amountToText(text_amount, temp);
		}
		// Get output script length.
		if (getVarIntMulti(&script_length, number_of_inputs))
		{
			#ifdef DISPLAY_PARMS
						writeEinkDisplay("24", false, 10, 10, "",false,10,30, "",false,10,50, "",false,10,70, "",false,0,0);
			#endif
			return TRANSACTION_INVALID_FORMAT; // transaction truncated or varint too big
		}
		if (is_ref)
		{
//			 The actual output scripts of input transactions don't need to SHOULD BE HERE!!!!
//			 be parsed (only the amount matters), so skip the script.
			if (i == output_num_select)  // seems to only grab one of the values...
			{

			if (script_length == 0x19)
						{
							// Expect a standard, pay to public key hash output script.
							// Look for: OP_DUP, OP_HASH160, (20 bytes of data).
							if (getTransactionBytesMulti(temp, 3, number_of_inputs))
							{
								#ifdef DISPLAY_PARMS
													writeEinkDisplay("26", false, 10, 10, "",false,10,30, "",false,10,50, "",false,10,70, "",false,0,0);
								#endif
								return TRANSACTION_INVALID_FORMAT; // transaction truncated
							}
							if ((temp[0] != 0x76) || (temp[1] != 0xa9) || (temp[2] != 0x14))
							{
								#ifdef DISPLAY_PARMS
													writeEinkDisplay("27", false, 10, 10, "",false,10,30, "",false,10,50, "",false,10,70, "",false,0,0);
								#endif
								return TRANSACTION_NON_STANDARD; // nonstandard transaction
							}
							if (getTransactionBytesMulti(temp, 20, number_of_inputs))
							{
								#ifdef DISPLAY_PARMS
													writeEinkDisplay("28", false, 10, 10, "",false,10,30, "",false,10,50, "",false,10,70, "",false,0,0);
								#endif
								return TRANSACTION_INVALID_FORMAT; // transaction truncated
							}
							hashToAddr(text_address_to_compare[p], temp, ADDRESS_VERSION_PUBKEY);
							p++;
//							writeEinkDisplay(text_address_static, false, 10, 10, "938",false,10,30, "",false,10,50, "",false,10,70, "",false,0,0);
							// Look for: OP_EQUALVERIFY OP_CHECKSIG.
							if (getTransactionBytesMulti(temp, 2, number_of_inputs))
							{
								#ifdef DISPLAY_PARMS
													writeEinkDisplay("29", false, 10, 10, "",false,10,30, "",false,10,50, "",false,10,70, "",false,0,0);
								#endif
								return TRANSACTION_INVALID_FORMAT; // transaction truncated
							}
							if ((temp[0] != 0x88) || (temp[1] != 0xac))
							{
								#ifdef DISPLAY_PARMS
													writeEinkDisplay("30", false, 10, 10, "",false,10,30, "",false,10,50, "",false,10,70, "",false,0,0);
								#endif
								return TRANSACTION_NON_STANDARD; // nonstandard transaction
							}
						} // end if (script_length == 0x19)
						else if (script_length == 0x17)
						{
							// Expect a standard, pay to script hash output script.
							// Look for: OP_HASH160, (20 bytes of data).
							if (getTransactionBytesMulti(temp, 2, number_of_inputs))
							{
								return TRANSACTION_INVALID_FORMAT; // transaction truncated
							}
							if ((temp[0] != 0xa9) || (temp[1] != 0x14))
							{
								return TRANSACTION_NON_STANDARD; // nonstandard transaction
							}
							if (getTransactionBytesMulti(temp, 20, number_of_inputs))
							{
								return TRANSACTION_INVALID_FORMAT; // transaction truncated
							}
							hashToAddr(text_address_to_compare[p], temp, ADDRESS_VERSION_PUBKEY);
							p++;
							// Look for: OP_EQUAL.
							if (getTransactionBytesMulti(temp, 1, number_of_inputs))
							{
								return TRANSACTION_INVALID_FORMAT; // transaction truncated
							}
							if (temp[0] != 0x87)
							{
								return TRANSACTION_NON_STANDARD; // nonstandard transaction
							}
						} // end if (script_length == 0x17)
						else
						{
							for (k = 0; k < script_length; k++)
							{
								if (getTransactionBytesMulti(temp, 1, number_of_inputs))
								{
									#ifdef DISPLAY_PARMS
														writeEinkDisplay("25", false, 10, 10, "",false,10,30, "",false,10,50, "",false,10,70, "",false,0,0);
									#endif
									return TRANSACTION_INVALID_FORMAT; // transaction truncated
							 	}
							}

						}
			}
			else
			{
				for (k = 0; k < script_length; k++)
				{
					if (getTransactionBytesMulti(temp, 1, number_of_inputs))
					{
						#ifdef DISPLAY_PARMS
											writeEinkDisplay("25", false, 10, 10, "",false,10,30, "",false,10,50, "",false,10,70, "",false,0,0);
						#endif
						return TRANSACTION_INVALID_FORMAT; // transaction truncated
				 	}
				}

			}
		}
		else
		{
			// Parsing a spending transaction; output scripts need to be
			// matched to a template.
			if (script_length == 0x19)
			{
				// Expect a standard, pay to public key hash output script.
				// Look for: OP_DUP, OP_HASH160, (20 bytes of data).
				if (getTransactionBytesMulti(temp, 3, number_of_inputs))
				{
					#ifdef DISPLAY_PARMS
										writeEinkDisplay("26", false, 10, 10, "",false,10,30, "",false,10,50, "",false,10,70, "",false,0,0);
					#endif
					return TRANSACTION_INVALID_FORMAT; // transaction truncated
				}
				if ((temp[0] != 0x76) || (temp[1] != 0xa9) || (temp[2] != 0x14))
				{
					#ifdef DISPLAY_PARMS
										writeEinkDisplay("27", false, 10, 10, "",false,10,30, "",false,10,50, "",false,10,70, "",false,0,0);
					#endif
					return TRANSACTION_NON_STANDARD; // nonstandard transaction
				}
				if (getTransactionBytesMulti(temp, 20, number_of_inputs))
				{
					#ifdef DISPLAY_PARMS
										writeEinkDisplay("28", false, 10, 10, "",false,10,30, "",false,10,50, "",false,10,70, "",false,0,0);
					#endif
					return TRANSACTION_INVALID_FORMAT; // transaction truncated
				}
				hashToAddr(text_address, temp, ADDRESS_VERSION_PUBKEY);
				// Look for: OP_EQUALVERIFY OP_CHECKSIG.
				if (getTransactionBytesMulti(temp, 2, number_of_inputs))
				{
					#ifdef DISPLAY_PARMS
										writeEinkDisplay("29", false, 10, 10, "",false,10,30, "",false,10,50, "",false,10,70, "",false,0,0);
					#endif
					return TRANSACTION_INVALID_FORMAT; // transaction truncated
				}
				if ((temp[0] != 0x88) || (temp[1] != 0xac))
				{
					#ifdef DISPLAY_PARMS
										writeEinkDisplay("30", false, 10, 10, "",false,10,30, "",false,10,50, "",false,10,70, "",false,0,0);
					#endif
					return TRANSACTION_NON_STANDARD; // nonstandard transaction
				}
			} // end if (script_length == 0x19)
			else if (script_length == 0x17)
			{
				// Expect a standard, pay to script hash output script.
				// Look for: OP_HASH160, (20 bytes of data).
				if (getTransactionBytesMulti(temp, 2, number_of_inputs))
				{
					return TRANSACTION_INVALID_FORMAT; // transaction truncated
				}
				if ((temp[0] != 0xa9) || (temp[1] != 0x14))
				{
					return TRANSACTION_NON_STANDARD; // nonstandard transaction
				}
				if (getTransactionBytesMulti(temp, 20, number_of_inputs))
				{
					return TRANSACTION_INVALID_FORMAT; // transaction truncated
				}
				hashToAddr(text_address, temp, ADDRESS_VERSION_P2SH);
				// Look for: OP_EQUAL.
				if (getTransactionBytesMulti(temp, 1, number_of_inputs))
				{
					return TRANSACTION_INVALID_FORMAT; // transaction truncated
				}
				if (temp[0] != 0x87)
				{
					return TRANSACTION_NON_STANDARD; // nonstandard transaction
				}
			} // end if (script_length == 0x17)
			else
			{
				#ifdef DISPLAY_PARMS
								writeEinkDisplay("31", false, 10, 10, "",false,10,30, "",false,10,50, "",false,10,70, "",false,0,0);
				#endif
				return TRANSACTION_NON_STANDARD; // nonstandard transaction
			}
			//			if(i != num_outputs - 1) // MASKIROVKA #################################################
			//			{
			//			writeEinkDisplay(text_address_to_compare[i], false, 10, 10, "",false,10,30, "",false,10,50, "",false,10,70, "",false,0,0);

						address_match = 1;
//						address_match_specific = 1;
						address_match_latch = 0;

			//			if(num_outputs != 1)
			//			{

							for(n=0; n < number_of_inputs; n++)
							{
//								writeEinkDisplay(text_address_to_compare[n], false, 10, 10, text_address,false,10,30, "",false,10,50, "",false,10,70, "",false,0,0);
								#ifdef DISPLAY_PARMS
												writeEinkDisplay("add match loop", false, 10, 10, "",false,10,30, "",false,10,50, "",false,10,70, "",false,0,0);
								#endif
								address_match = strcmp(text_address_to_compare[n],text_address);
								address_match_specific = strcmp(change_address_ptr,text_address);
								if((address_match == 0) || (address_match_specific == 0))
								{
//									writeEinkDisplay("match", false, 10, 10, "",false,10,30, "",false,10,50, "",false,10,70, "",false,0,0);
									#ifdef DISPLAY_PARMS
													writeEinkDisplay("match", false, 10, 10, "",false,10,30, "",false,10,50, "",false,10,70, "",false,0,0);
									#endif
									address_match_latch = 1;
								}
							}

							if (address_match_latch == 0)
							{
								#ifdef DISPLAY_PARMS
												writeEinkDisplay("in latch 0", false, 10, 10, "",false,10,30, "",false,10,50, "",false,10,70, "",false,0,0);
								#endif
								if (newOutputSeen(text_amount, text_address))
								{
									#ifdef DISPLAY_PARMS
													writeEinkDisplay("output seen error", false, 10, 10, "",false,10,30, "",false,10,50, "",false,10,70, "",false,0,0);
									#endif
									return TRANSACTION_TOO_MANY_OUTPUTS; // too many outputs
								}
							}
		} // end if (is_ref)
	} // end for (i = 0; i < num_outputs; i++)

	// Check locktime.
	if (getTransactionBytesMulti(temp, 4, number_of_inputs))
	{
		#ifdef DISPLAY_PARMS
				writeEinkDisplay("33", false, 10, 10, "",false,10,30, "",false,10,50, "",false,10,70, "",false,0,0);
		#endif
		return TRANSACTION_INVALID_FORMAT; // transaction truncated
	}
	if (readU32LittleEndian(temp) != 0x00000000)
	{
		#ifdef DISPLAY_PARMS
				writeEinkDisplay("34", false, 10, 10, "",false,10,30, "",false,10,50, "",false,10,70, "",false,0,0);
		#endif
//		return TRANSACTION_NON_STANDARD; // replacement not supported // jump the locktime validation because if a locktime is specified, we have no way of knowing if it is fulfilled
	}

	if (!is_ref)
	{
		// Check hashtype.
		if (getTransactionBytesMulti(temp, 4, number_of_inputs))
		{
			#ifdef DISPLAY_PARMS
						writeEinkDisplay("35", false, 10, 10, "",false,10,30, "",false,10,50, "",false,10,70, "",false,0,0);
			#endif
			return TRANSACTION_INVALID_FORMAT; // transaction truncated
		}
		if (readU32LittleEndian(temp) != 0x00000001)
		{
			#ifdef DISPLAY_PARMS
						writeEinkDisplay("36", false, 10, 10, "",false,10,30, "",false,10,50, "",false,10,70, "",false,0,0);
			#endif
			return TRANSACTION_NON_STANDARD; // nonstandard transaction
		}

		// Is there junk at the end of the transaction data?
		if (!isEndOfTransactionData())
		{
			#ifdef DISPLAY_PARMS
						writeEinkDisplay("37", false, 10, 10, "",false,10,30, "",false,10,50, "",false,10,70, "",false,0,0);
			#endif
			return TRANSACTION_INVALID_FORMAT; // junk at end of transaction data
		}

		if (!bigIsZeroVariableSize(transaction_fee_amount, sizeof(transaction_fee_amount)))
		{
			amountToText(text_amount, transaction_fee_amount);
			setTransactionFee(text_amount);
		}
	}

	sha256FinishDouble(sig_hash_hs_ptr);
	if(!is_ref)
	{
		for(m = 0; m < number_of_inputs; m++)
		{
			sha256FinishDouble(sig_hash_hs_ptr_array[m]);
		}
	}
	// The signature hash is written in a little-endian format because it
	// is used as a little-endian multi-precision integer in
	// signTransaction().

//	we are going to have to write an array of hashes, looping through for however many inputs there were

	writeHashToByteArray(sig_hash_single, sig_hash_hs_ptr, false);

	if(!is_ref)
	{
		for(m = 0; m < number_of_inputs; m++)
		{
			writeHashToByteArray(sig_hash[m], sig_hash_hs_ptr_array[m], false);
		}
	}
	//	writeHashToByteArray(sig_hash[0], sig_hash_hs_ptr, false);
	sha256FinishDouble(transaction_hash_hs_ptr);
	writeHashToByteArray(transaction_hash, transaction_hash_hs_ptr, false);

	if (is_ref)
	{
//		for(m = 0; m < number_of_inputs; m++)
//		{
			// Why backwards? Because Bitcoin serialises the input reference
			// hashes that way.
			for (j = 32; j--; )
			{
//				sha256WriteByte(ref_compare_hs, sig_hash[m][j]);
				sha256WriteByte(ref_compare_hs, sig_hash_single[j]);
			}
//		}
	}


//	char ah_loop_txtb[16];
//	sprintf(ah_loop_txtb,"%lu", sig_hash_counter);
//	writeEinkDisplay(ah_loop_txtb, false, 10, 10, "",false,10,30, "",false,10,50, "",false,0,0, "",false,0,0);


	#ifdef DISPLAY_PARMS
		writeEinkDisplay("TRANSACTION_NO_ERROR", false, 10, 10, "",false,10,30, "",false,10,50, "",false,10,70, "",false,0,0);
	#endif
	return TRANSACTION_NO_ERROR;
}

/** Parse a Bitcoin transaction, extracting the output amounts/addresses,
  * validating the transaction (ensuring that it is "standard") and computing
  * a double SHA-256 hash of the transaction. This double SHA-256 hash is the
  * "signature hash" because it is the hash which is passed on to the signing
  * function signTransaction().
  *
  * The Bitcoin protocol for signing a transaction involves replacing
  * the corresponding input script with the output script that
  * the input references. This means that for a transaction with n
  * inputs, there will be n different signature hashes - one for each input.
  * Requiring the user to approve a transaction n times would be very
  * annoying, so there needs to be a way to determine whether a bunch of
  * transactions are actually "the same".
  * So in addition to the signature hash, a "transaction hash" will be
  * computed. The transaction hash is just like the signature hash, except
  * input scripts are not included.
  *
  * This expects the input stream to contain many concatenated transactions;
  * it should contain each input transaction (of the spending transaction)
  * followed by the spending transaction. This is necessary
  * to calculate the transaction fee. A transaction does directly contain the
  * output amounts, but not the input amounts. The only way to get input
  * amounts is to look at the output amounts of the transactions the inputs
  * refer to.
  *
  * \param sig_hash The signature hash will be written here (if everything
  *                 goes well), as a 32 byte little-endian multi-precision
  *                 number.
  * \param transaction_hash The transaction hash will be written here (if
  *                         everything goes well), as a 32 byte little-endian
  *                         multi-precision number.
  * \param length The total length of the transaction. If no stream read
  *               errors occurred, then exactly length bytes will be read from
  *               the stream, even if the transaction was not parsed
  *               correctly.
  * \return One of the values in #TransactionErrorsEnum.
  */
TransactionErrors parseTransaction(uint8_t sig_hash[][32], BigNum256 transaction_hash, uint32_t length, uint32_t number_of_inputs, char *change_address_ptr_original)
{
	TransactionErrors r;
	uint8_t junk;
	bool is_ref;
	HashState sig_hash_hs;
	HashState sig_hash_hs_array[number_of_inputs];
	HashState transaction_hash_hs;
	HashState ref_compare_hs;
	uint32_t i;


	hs_ptr_valid = false;
	transaction_data_index = 0;
	transaction_length = length;
	memset(transaction_fee_amount, 0, sizeof(transaction_fee_amount));
	sig_hash_hs_ptr = &sig_hash_hs;
	for(i=0;i<number_of_inputs;i++)
	{
		sig_hash_hs_ptr_array[i] = &sig_hash_hs_array[i];
	}

	transaction_hash_hs_ptr = &transaction_hash_hs;
	sha256Begin(&ref_compare_hs);

	uint8_t sig_hash_counter = 0;

	hs_ptr_valid = true;




	do
	{
		r = parseTransactionInternalMulti(sig_hash, transaction_hash, &is_ref, &ref_compare_hs, number_of_inputs, sig_hash_counter, change_address_ptr_original);
	} while ((r == TRANSACTION_NO_ERROR) && is_ref);
	hs_ptr_valid = false;

	// Always try to consume the entire stream.
	while (!isEndOfTransactionData())
	{
		if (getTransactionBytes(&junk, 1))
		{
			break;
		}
	}
	return r;
}

/** Swap endian representation of a 256 bit integer.
  * \param buffer An array of 32 bytes representing the integer to change.
  */
void swapEndian256(BigNum256 buffer)
{
	uint8_t i;
	uint8_t temp;

	for (i = 0; i < 16; i++)
	{
		temp = buffer[i];
		buffer[i] = buffer[31 - i];
		buffer[31 - i] = temp;
	}
}

/**
 * \defgroup DEROffsets Offsets for DER signature encapsulation.
 *
 * @{
 */
/** Initial offset of r in signature. It's 4 because 4 bytes are needed for
  * the SEQUENCE/length and INTEGER/length bytes. */
#define R_OFFSET	4
/** Initial offset of s in signature. It's 39 because: r is initially 33
  * bytes long, and 2 bytes are needed for INTEGER/length. 4 + 33 + 2 = 39. */
#define S_OFFSET	39
/**@}*/

/** Encapsulate an ECDSA signature in the DER format which OpenSSL uses.
  * This function does not fail.
  * \param signature This must be a byte array with space for at
  *                  least #MAX_SIGNATURE_LENGTH bytes. On exit, the
  *                  encapsulated signature will be written here.
  * \param r The r value of the ECDSA signature. This should be a 32 byte
  *          little-endian multi-precision integer.
  * \param s The s value of the ECDSA signature. This should be a 32 byte
  *          little-endian multi-precision integer.
  * \return The length of the signature, in number of bytes.
  */
static uint8_t encapsulateSignature(uint8_t *signature, BigNum256 r, BigNum256 s, bool add_hash_type)
{
	uint8_t sequence_length;
	uint8_t i;

	memcpy(&(signature[R_OFFSET + 1]), r, 32);
	memcpy(&(signature[S_OFFSET + 1]), s, 32);
	// Place an extra leading zero in front of r and s, just in case their
	// most significant bit is 1.
	// Integers in DER are always 2s-complement signed, but r and s are
	// non-negative. Thus if the most significant bit of r or s is 1,
	// a leading zero must be placed in front of the integer to signify that
	// it is non-negative.
	// If the most significant bit is not 1, the extraneous leading zero will
	// be removed in a check below.
	signature[R_OFFSET] = 0x00;
	signature[S_OFFSET] = 0x00;

	// Integers in DER are big-endian.
	swapEndian256(&(signature[R_OFFSET + 1]));
	swapEndian256(&(signature[S_OFFSET + 1]));

	sequence_length = 0x46; // 2 + 33 + 2 + 33
	signature[R_OFFSET - 2] = 0x02; // INTEGER
	signature[R_OFFSET - 1] = 0x21; // length of INTEGER
	signature[S_OFFSET - 2] = 0x02; // INTEGER
	signature[S_OFFSET - 1] = 0x21; // length of INTEGER
	if(add_hash_type){
		signature[S_OFFSET + 33] = 0x01; // hashtype
	}
	// According to DER, integers should be represented using the shortest
	// possible representation. This implies that leading zeroes should
	// always be removed. The exception to this is that if removing the
	// leading zero would cause the value of the integer to change (eg.
	// positive to negative), the leading zero should remain.

	// Remove unnecessary leading zeroes from s. s is pruned first
	// because pruning r will modify the offset where s begins.
	while ((signature[S_OFFSET] == 0) && ((signature[S_OFFSET + 1] & 0x80) == 0))
	{
		for (i = S_OFFSET; i < 72; i++)
		{
			signature[i] = signature[i + 1];
		}
		sequence_length--;
		signature[S_OFFSET - 1]--;
		if (signature[S_OFFSET - 1] == 1)
		{
			break;
		}
	}

	// Remove unnecessary leading zeroes from r.
	while ((signature[R_OFFSET] == 0) && ((signature[R_OFFSET + 1] & 0x80) == 0))
	{
		for (i = R_OFFSET; i < 72; i++)
		{
			signature[i] = signature[i + 1];
		}
		sequence_length--;
		signature[R_OFFSET - 1]--;
		if (signature[R_OFFSET - 1] == 1)
		{
			break;
		}
	}

	signature[0] = 0x30; // SEQUENCE
	signature[1] = sequence_length; // length of SEQUENCE
	// 2 or 3 extra bytes: SEQUENCE/length and hashtype (if transaction)
	int extra_bytes;
	if(add_hash_type){
		extra_bytes = 3;
	}
	else
	{
		extra_bytes = 2;
	}
	return (uint8_t)(sequence_length + extra_bytes);
}

/** Sign a transaction. This should be called after the transaction is parsed
  * and a signature hash has been computed. The primary purpose of this
  * function is to call ecdsaSign() and encapsulate the ECDSA signature in
  * the DER format which OpenSSL uses.
  * \param signature The encapsulated signature will be written here. This
  *                  must be a byte array with space for
  *                  at least #MAX_SIGNATURE_LENGTH bytes.
  * \param out_length The length of the signature, in number of bytes, will be
  *                   written here (on success). This length includes the hash
  *                   type byte.
  * \param sig_hash The signature hash of the transaction (see
  *                 parseTransaction()).
  * \param private_key The private key to sign the transaction with. This must
  *                    be a 32 byte little-endian multi-precision integer.
  * \param add_hash_type Whether or not to add the hashtype byte at the end of the DER encoded signature.
  * 					 This is needed for the signMessageCallback, as the signature needs to be without hashtype.
  * \return false on success, or true if an error occurred while trying to
  *         obtain a random number.
  */
bool signTransaction(uint8_t *signature, uint8_t *out_length, BigNum256 sig_hash, BigNum256 private_key, bool add_hash_type)
{
	uint8_t k[32];

//	TEST k NOT RANDOM !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
//	uint8_t k[32] = {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1};

	uint8_t r[32];
	uint8_t s[32];

	*out_length = 0;
	do
	{
		if (getRandom256(k))
		{
			return true; // problem with RNG system
		}
		memset(&r[0], 0, sizeof(r));
		memset(&s[0], 0, sizeof(s));
	} while (ecdsaSign(r, s, sig_hash, private_key, k));

//	displayBigHexStream(r, 32);
//	waitForButtonPress();
//
//	displayBigHexStream(s, 32);
//	waitForButtonPress();


	*out_length = encapsulateSignature(signature, r, s, add_hash_type);
	return false; // success
}


