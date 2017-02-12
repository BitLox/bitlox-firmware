/** \file stream_comm.h
  *
  * \brief Describes functions, types and constants exported by stream_comm.c.
  *
  *
  * This file is licensed as described by the file LICENCE.
  */

#ifndef STREAM_COMM_H_INCLUDED
#define STREAM_COMM_H_INCLUDED

#include "common.h"

/** Major version number to report to the host. */
#define VERSION_MAJOR					0
/** Minor version number to report to the host. */
#define VERSION_MINOR					62


/**
 * \defgroup PacketTypes Type values for packets.
 *
 * See the file PROTOCOL for more information about the format of packets
 * and what the payload of packets should be.
 *
 * @{
 */
/** Request a response from the wallet. */
#define PACKET_TYPE_PING				0x00

/** Request reset of device PIN */
#define PACKET_TYPE_RESET_DEVICE_PIN	0x01

/** Create a new wallet. */
#define PACKET_TYPE_NEW_WALLET			0x04

/** Create a new address in a wallet. */
// #define PACKET_TYPE_NEW_ADDRESS			0x05

/** Get number of addresses in a wallet. */
// #define PACKET_TYPE_GET_NUM_ADDRESSES	0x06

/** Get an address and its associated public key from a wallet. */
#define PACKET_TYPE_GET_ADDRESS_PUBKEY	0x09

/** Sign a transaction. */
// #define PACKET_TYPE_SIGN_TRANSACTION	0x0A

/** Load (unlock) a wallet. */
#define PACKET_TYPE_LOAD_WALLET			0x0B

/** Format storage area, erasing everything. */
#define PACKET_TYPE_FORMAT				0x0D

/** Change encryption key of a wallet. */
#define PACKET_TYPE_CHANGE_KEY			0x0E

/** Change name of a wallet. */
#define PACKET_TYPE_CHANGE_NAME			0x0F






/** List all wallets. */
#define PACKET_TYPE_LIST_WALLETS		0x10

/** Backup a wallet. */
#define PACKET_TYPE_BACKUP_WALLET		0x11

/** Restore wallet from a backup. */
#define PACKET_TYPE_RESTORE_WALLET		0x12

/** Get device UUID. */
#define PACKET_TYPE_GET_DEVICE_UUID		0x13

/** Get bytes of entropy. */
#define PACKET_TYPE_GET_ENTROPY			0x14

/** Get master public key. */
#define PACKET_TYPE_GET_MASTER_KEY		0x15

/** Delete a wallet. */
#define PACKET_TYPE_DELETE_WALLET		0x16

/** Initialise device's state. */
#define PACKET_TYPE_INITIALIZE			0x17

/** Restore wallet from a backup via device input. */
#define PACKET_TYPE_RESTORE_WALLET_DEVICE		0x18





/** An address from a wallet (response to #PACKET_TYPE_GET_ADDRESS_PUBKEY
  * or #PACKET_TYPE_NEW_ADDRESS). */
#define PACKET_TYPE_ADDRESS_PUBKEY		0x30

/** Number of addresses in a wallet
  * (response to #PACKET_TYPE_GET_NUM_ADDRESSES). */
// #define PACKET_TYPE_NUM_ADDRESSES		0x31

/** Public information about all wallets
  * (response to #PACKET_TYPE_LIST_WALLETS). */
#define PACKET_TYPE_WALLETS				0x32

/** Wallet's response to ping (see #PACKET_TYPE_PING). */
#define PACKET_TYPE_PING_RESPONSE		0x33

/** Packet signifying successful completion of an operation. */
#define PACKET_TYPE_SUCCESS				0x34

/** Packet signifying failure of an operation. */
#define PACKET_TYPE_FAILURE				0x35

/** Device UUID (response to #PACKET_TYPE_GET_DEVICE_UUID). */
#define PACKET_TYPE_DEVICE_UUID			0x36

/** Some bytes of entropy (response to #PACKET_TYPE_GET_ENTROPY). */
#define PACKET_TYPE_ENTROPY				0x37

/** Master public key (response to #PACKET_TYPE_GET_MASTER_KEY). */
#define PACKET_TYPE_MASTER_KEY			0x38

/** Signature (response to #PACKET_TYPE_SIGN_TRANSACTION). */
#define PACKET_TYPE_SIGNATURE			0x39

/** Version information and list of features. */
#define PACKET_TYPE_FEATURES			0x3a







/** Device wants to wait for button press (beginning of ButtonRequest
  * interjection). */
#define PACKET_TYPE_BUTTON_REQUEST		0x50

/** Host will allow button press (response to #PACKET_TYPE_BUTTON_REQUEST). */
#define PACKET_TYPE_BUTTON_ACK			0x51

/** Host will not allow button press (response
  * to #PACKET_TYPE_BUTTON_REQUEST). */
#define PACKET_TYPE_BUTTON_CANCEL		0x52

/** Device wants host to send a password (beginning of PinRequest
  * interjection. */
#define PACKET_TYPE_PIN_REQUEST			0x53

/** Host sends password (response to #PACKET_TYPE_PIN_REQUEST). */
#define PACKET_TYPE_PIN_ACK				0x54

/** Host does not want to send password (response
  * to #PACKET_TYPE_PIN_REQUEST). */
#define PACKET_TYPE_PIN_CANCEL			0x55

/** Device wants host to send a one-time password (beginning of OtpRequest
  * interjection. */
#define PACKET_TYPE_OTP_REQUEST			0x56

/** Host sends one-time password (response to #PACKET_TYPE_OTP_REQUEST). */
#define PACKET_TYPE_OTP_ACK				0x57

/** Host does not want to send one-time password (response
  * to #PACKET_TYPE_OTP_REQUEST). */
#define PACKET_TYPE_OTP_CANCEL			0x58

/** Host wants to reset language
  *  */
#define PACKET_TYPE_RESET_LANG			0x59







/** Create a new wallet. */
// #define PACKET_TYPE_NEW_WALLET_MNEMONIC			0x60

/** Fetch current loaded wallet xpub */
#define PACKET_TYPE_SCAN_WALLET			0x61

#define PACKET_TYPE_SCAN_WALLET_RESPONSE			0x62

/** Sign a transaction, using the HD address/signature generation */
// #define PACKET_TYPE_SIGN_TRANSACTION_HD	0x63

/** SignatureComplete (response to #PACKET_TYPE_SIGN_TRANSACTION). Includes ECDSA key concatenated. Presently only single transaction*/
#define PACKET_TYPE_SIGNATURE_COMPLETE			0x64

/** Sign a transaction, using alternate fields */
#define PACKET_TYPE_SIGN_TRANSACTION_EXTENDED 		0x65

#define PACKET_TYPE_SIGN_TRANSACTION_CHANGE_ADDRESS 		0x66


#define PACKET_TYPE_SIGN_MESSAGE 					0x70

#define PACKET_TYPE_SIGN_MESSAGE_RESPONSE 			0x71

#define PACKET_TYPE_DISPLAY_ADDRESS_AS_QR 			0x80

#define PACKET_TYPE_GET_BULK		 				0x81

#define PACKET_TYPE_BULK			 				0x82

#define PACKET_TYPE_SET_BULK			 			0x83



typedef uint32_t AddressHandle;


/**@}*/
#ifdef __cplusplus
     extern "C" {
#endif

void processPacket(void);
bool initialFormat(void);
bool initialFormatAuto(void);
bool createDefaultWallet(void);
bool createDefaultWalletAuto(int strength, int level);
void setupSequence(void);
void sendSuccessPacketOnly(void);
void showQRcode(AddressHandle ah_root4, AddressHandle ah_chain4, AddressHandle ah_index4);
bool initialFormatAutoDuress(void);
bool passwordInterjectionAutoPIN(int level);
bool pseudoPacketNewWallet(int strength, int level);
bool pseudoPacketRestoreWallet(int strength, int level);
void CharToByte(char* chars, byte* bytes, unsigned int count);
void ByteToChar(byte* bytes, char* chars, unsigned int count);
void decryptStream(uint8_t *ciphertext, uint8_t *key);
void decryptStreamSized(uint8_t *ciphertext, uint8_t *key, uint32_t size);
void encryptStream(uint8_t *plaintext, uint8_t *key);
void encryptStreamSized(uint8_t *plaintext, uint8_t *key, uint32_t size);
void getAddressOnly(uint8_t *out_address3, AddressHandle ah_root3, AddressHandle ah_chain3, AddressHandle ah_index3);

#ifdef __cplusplus
     }
#endif


#endif // #ifndef STREAM_COMM_H_INCLUDED
