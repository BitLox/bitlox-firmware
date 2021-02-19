/** \file storage_common.h
  *
  * \brief Defines the overall layout of non-volatile storage.
  *
  * The overall layout of non-volatile storage consists of the global (stuff
  * that applies to all wallets) data followed by each wallet record.
  * This file does not describe the format of an individual
  * wallet record; rather it describes where those records go in non-volatile
  * storage.
  *
  * This file is licensed as described by the file LICENCE.
  */

#ifndef STORAGE_COMMON_H_INCLUDED
#define STORAGE_COMMON_H_INCLUDED


/** Length of any UUID.
  * \warning This must also be a multiple of 16, since the block size of
  *          AES is 128 bits.
  */
#define DEVICE_UUID_LENGTH				16

#define ENTROPY_POOL_LENGTH				32

#define POOL_CHECKSUM_LENGTH			32

/** Address where the persistent entropy pool is located. */
#define ENTROPY_POOL_ADDRESS			64
/** Address where the checksum of the persistent entropy pool is located. */
#define POOL_CHECKSUM_ADDRESS			96
/** Address where device UUID is located. */
#define DEVICE_UUID_ADDRESS				128


#define DEVICE_LANG_LENGTH				16
#define DEVICE_LANG_ADDRESS				(DEVICE_UUID_ADDRESS + DEVICE_UUID_LENGTH)

#define DEVICE_LANG_SET_LENGTH			16
#define DEVICE_LANG_SET_ADDRESS			(DEVICE_LANG_ADDRESS + DEVICE_LANG_LENGTH)

#define DEVICE_COMMS_SET_LENGTH			16
#define DEVICE_COMMS_SET_ADDRESS		(DEVICE_LANG_SET_ADDRESS + DEVICE_LANG_SET_LENGTH)

#define IS_FORMATTED_LENGTH				16
#define IS_FORMATTED_ADDRESS			(DEVICE_COMMS_SET_ADDRESS + DEVICE_COMMS_SET_LENGTH)

#define PIN_LENGTH						32
#define PIN_ADDRESS						(IS_FORMATTED_ADDRESS + IS_FORMATTED_LENGTH)

#define HAS_PIN_LENGTH					16
#define HAS_PIN_ADDRESS 				(PIN_ADDRESS + PIN_LENGTH)

#define SETUP_TYPE_LENGTH				16
#define SETUP_TYPE_ADDRESS 				(HAS_PIN_ADDRESS + HAS_PIN_LENGTH)

#define WRONG_TRANSACTION_PIN_COUNT_LENGTH	16
#define WRONG_TRANSACTION_PIN_COUNT_ADDRESS (SETUP_TYPE_ADDRESS + SETUP_TYPE_LENGTH)

#define WRONG_DEVICE_PIN_COUNT_LENGTH		16
#define WRONG_DEVICE_PIN_COUNT_ADDRESS 		(WRONG_TRANSACTION_PIN_COUNT_ADDRESS + WRONG_TRANSACTION_PIN_COUNT_LENGTH)


#define AEM_USE_LENGTH					16
#define AEM_USE_ADDRESS 				(WRONG_DEVICE_PIN_COUNT_ADDRESS + WRONG_DEVICE_PIN_COUNT_LENGTH)

#define AEM_PHRASE_LENGTH				64
#define AEM_PHRASE_ADDRESS 				(AEM_USE_ADDRESS + AEM_USE_LENGTH)

/** Address where wallet records start at. From here on, non-volatile storage
  * will consist of sequential wallet records. */
#define WALLET_START_ADDRESS				(AEM_PHRASE_ADDRESS + AEM_PHRASE_LENGTH)

#endif // #ifndef STORAGE_COMMON_H_INCLUDED

