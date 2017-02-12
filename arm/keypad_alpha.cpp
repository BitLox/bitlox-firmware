/*
 * keypad_alpha.cpp
 *
 *  Created on: Feb 5, 2015
 *      Author: thesquid
 */

#include <string.h>
#include "eink.h"
#include <Arduino.h>
#include "keypad_alpha.h"
#include "due_epaper_lib/due_epaper.h"
//#include "../sha2_trez.h"
#include "usart.h"
#include "BLE.h"
#include "../storage_common.h"
#include "../hwinterface.h"
#include "../hash.h"
#include "../sha256.h"
#include "../prandom.h"
#include "lcd_and_input.h"
#include "main.h"
#include "../stream_comm.h"
#include "../common.h"

#include "../baseconv.h"



char *mnemonic_input(void);
char alphkeypad_noNumbers_3A8C(int current_x, int current_y);
void writeSelectedCharAndStringBlankingMnemonics(int current_x, int current_y, bool caps);

#if defined(__SAM3A8C__)

const byte DEPTH = 4;
const byte COLS = 3; //three columns
const byte ROWS = 4; //four rows


#define ROW_PIN_1      22
#define ROW_PIN_2      23
#define ROW_PIN_3      24
#define COL_PIN_1      18
#define COL_PIN_2      19
#define COL_PIN_3      20
#define COL_PIN_4      21

//define the symbols on the buttons of the keypads
//char hexaKeys_plastic[DEPTH][COLS][ROWS] = {
//  {
//    {'~', '@', '9', '8'},
//    {'7', '6', '5', '4'},
//    {'3', '2', '1', '0'}
//  },
//  {
//    {'~', '@', 'y', 'v'},
//    {'s', 'p', 'm', 'j'},
//    {'g', 'd', 'a', '^'}
//  },
//  {
//    {'~', '@', 'z', 'w'},
//    {'t', 'q', 'n', 'k'},
//    {'h', 'e', 'b', '^'}
//  },
//  {
//    {'~', '@', 'z', 'x'},
//    {'u', 'r', 'p', 'l'},
//    {'i', 'f', 'c', '^'}
//  }
//};
//

const char hexaKeys[DEPTH][COLS][ROWS] = {
  {
    {'@', '6', '0', '9'},
    {'8', '7', '5', '4'},
    {'3', '2', '1', '~'}
  },
  {
    {'@', 'p', '^', 'y'},
    {'v', 's', 'm', 'j'},
    {'g', 'd', 'a', '~'}
  },
  {
    {'@', 'q', '^', 'z'},
    {'w', 't', 'n', 'k'},
    {'h', 'e', 'b', '~'}
  },
  {
    {'@', 'r', '^', 'z'},
    {'x', 'u', 'o', 'l'},
    {'i', 'f', 'c', '~'}
  }
};

const char hexaKeysCAPS[DEPTH][COLS][ROWS] = {
  {
    {'@', '6', '0', '9'},
    {'8', '7', '5', '4'},
    {'3', '2', '1', '~'}
  },
  {
    {'@', 'P', '^', 'Y'},
    {'V', 'S', 'M', 'J'},
    {'G', 'D', 'A', '~'}
  },
  {
    {'@', 'Q', '^', 'Z'},
    {'W', 'T', 'N', 'K'},
    {'H', 'E', 'B', '~'}
  },
  {
    {'@', 'R', '^', 'Z'},
    {'X', 'U', 'O', 'L'},
    {'I', 'F', 'C', '~'}
  }
};

const char hexaKeysPlusSpace[DEPTH][COLS][ROWS] = {
  {
    {'@', '6', '0', '9'},
    {'8', '7', '5', '4'},
    {'3', '2', '1', '~'}
  },
  {
    {'@', 'p', '^', 'y'},
    {'v', 's', 'm', 'j'},
    {'g', 'd', 'a', '~'}
  },
  {
    {'@', 'q', '_', 'z'},
    {'w', 't', 'n', 'k'},
    {'h', 'e', 'b', '~'}
  },
  {
    {'@', 'r', '^', 'z'},
    {'x', 'u', 'o', 'l'},
    {'i', 'f', 'c', '~'}
  }
};

const char hexaKeysCAPSPlusSpace[DEPTH][COLS][ROWS] = {
  {
    {'@', '6', '0', '9'},
    {'8', '7', '5', '4'},
    {'3', '2', '1', '~'}
  },
  {
    {'@', 'P', '^', 'Y'},
    {'V', 'S', 'M', 'J'},
    {'G', 'D', 'A', '~'}
  },
  {
    {'@', 'Q', '_', 'Z'},
    {'W', 'T', 'N', 'K'},
    {'H', 'E', 'B', '~'}
  },
  {
    {'@', 'R', '^', 'Z'},
    {'X', 'U', 'O', 'L'},
    {'I', 'F', 'C', '~'}
  }
};
#endif


/*
#if defined(__SAM3X8E__)

const byte ROWS = 4; //four rows
const byte COLS = 3; //four columns
const byte DEPTH = 4;


#define COL_PIN_0      69
#define COL_PIN_1      68
#define COL_PIN_2      67
#define ROW_PIN_0      63
#define ROW_PIN_1      64
#define ROW_PIN_2      65
#define ROW_PIN_3      66

//define the symbols on the buttons of the keypads
char hexaKeys[DEPTH][COLS][ROWS] = {
  {
    {'0', '1', '2', '3'},
    {'4', '5', '6', '7'},
    {'8', '9', '@', '~'}
  },
  {
    {'^', 'a', 'd', 'g'},
    {'j', 'm', 'p', 's'},
    {'v', 'y', '@', '~'}
  },
  {
    {'^', 'b', 'e', 'h'},
    {'k', 'n', 'q', 't'},
    {'w', 'z', '@', '~'}
  },
  {
    {'^', 'c', 'f', 'i'},
    {'l', 'o', 'r', 'u'},
    {'x', 'z', '@', '~'}
  }
};

const int r1 = ROW_PIN_0;
const int r2 = ROW_PIN_1;
const int r3 = ROW_PIN_2;
const int r4 = ROW_PIN_3;
const int c1 = COL_PIN_0;
const int c2 = COL_PIN_1;
const int c3 = COL_PIN_2;

#define COL_PIN_3      70
#define COL_PIN_4      71


#endif
*/

const int keyDelay = 50;


int signum (int x) {
  if (x < 0) return -1;
  if (x > 0) return 1;
  return 0;
}

int add (int x, int y) {
  for (int i = 0; i < abs(y); ++i) {
    if (y > 0) ++x;
    else --x;
  }
  return x;
}

int mult (int x, int y) {
  int sign = signum(x) * signum(y);
  x = abs(x);
  y = abs(y);
  int res = 0;
  for (int i = 0; i < y; ++i) {
    res = add(res, x);
  }
  return sign * res;
}

int pow (int x, int y) {
  if (y < 0) return 0;
  int res = 1;
  for (int i = 0; i < y; ++i) {
    res = mult(res, x);
  }
  return res;
}


bool doAEMValidate(bool displayAlpha)
{

	char encryption_phrase_char[17]={};
	char *encryption_phrase_char_ptr;

	uint8_t tempLang[1];
	nonVolatileRead(tempLang, DEVICE_LANG_ADDRESS, 1);

	int lang;
	lang = (int)tempLang[0];

	int zhSizer = 1;

	if (lang == 3)
	{
		zhSizer = 2;
	}


	encryption_phrase_char_ptr = &encryption_phrase_char[0];

	uint8_t encryption_phrase_array[16]={};
	uint8_t *encryption_phrase;

	encryption_phrase = &encryption_phrase_array[0];

//	memset(encryption_phrase, 0, 15);
//	strcpy(encryption_phrase, "");

	if(displayAlpha){
		buttonInterjectionNoAckSetup(ASKUSER_AEM_ENTRY_ALPHA);
	}else{
		buttonInterjectionNoAckSetup(ASKUSER_AEM_ENTRY);
	}



	encryption_phrase_char_ptr = getInputAEM(displayAlpha, false);
//	getInput(displayAlpha, false);
	int i;
	for (i = 0; i<sizeof(encryption_phrase_char)-1;i++)
	{
		encryption_phrase_array[i] = encryption_phrase_char_ptr[i];
	}

	clearDisplay();

//	writeEinkDisplay("decrypt with", false, COL_1_X, LINE_1_Y, encryption_phrase_char_ptr,false,COL_1_X, LINE_2_Y, "",false,5,50, "",false,5,70, "",false,0,0);
//	displayHexStream(encryption_phrase_array, 16);




	uint8_t ciphertext[64];
	nonVolatileRead(ciphertext, AEM_PHRASE_ADDRESS, 64);

//	writeEinkDisplay("to be decrypted", false, COL_1_X, LINE_1_Y, "",false,COL_1_X, LINE_2_Y, "",false,5,50, "",false,5,70, "",false,0,0);
//	displayHexStream(ciphertext, 64);
//

	decryptStreamSized(ciphertext, encryption_phrase_array, 64);
	int j;
	bool goodPhrase = true;
	for(j=0; j<64; j++)
	{
		if((ciphertext[j] > 0x0000 && ciphertext[j] < 0x0030) ||(ciphertext[j] > 0x0039 && ciphertext[j] < 0x0041)||(ciphertext[j] > 0x005A && ciphertext[j] < 0x0061)||ciphertext[j] > 0x007A)
		{
			goodPhrase = false;
		}
	}

	initDisplay();
	if(goodPhrase)
	{
		writeEinkNoDisplay("RECOGNITION PHRASE:",  COL_1_X, LINE_0_Y, (char *)ciphertext,COL_1_X, LINE_2_Y, "",5,50, "",5,70, "",0,0);
	}else
	{
		writeEinkNoDisplay("RECOGNITION PHRASE:",  COL_1_X, LINE_0_Y, "bad decryption phrase",COL_1_X, LINE_2_Y, "",5,50, "",5,70, "",0,0);
	}

	drawCheck(draw_check_X,draw_check_Y);

	display();

	waitForButtonPress();
//	displayHexStream(ciphertext, 64);

	return false;

}



void doAEMSet(void)
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

	if (!approved)
	{
		// Need to explicitly get permission from user.
		// The call to parseTransaction() should have logged all the outputs
		// to the user interface.
		permission_denied = buttonInterjectionNoAckSetup(ASKUSER_USE_AEM);
		if (!permission_denied)
		{
			// User approved action.
			approved = true;
		}
	} // if (!approved)
	if (approved)
	{
		uint8_t display_phrase_array[64]={};
		char display_phrase_char[65] = {};
		char *display_phrase;

		display_phrase = &display_phrase_char[0];

		memset(display_phrase, 0, 64);
		strcpy(display_phrase, "");

		permission_denied = buttonInterjectionNoAckSetup(ASKUSER_AEM_DISPLAYPHRASE);
		if (!permission_denied)
		{
			// User approved action.
			approved2 = true;
		}

		if(approved2)
		{


			buttonInterjectionNoAckSetup(ASKUSER_ALPHA_INPUT_PREFACE);


			display_phrase = getInput(true, false);
			int j;
			for (j = 0; j<sizeof(display_phrase_char)-1;j++)
			{
				display_phrase_array[j] = display_phrase[j];
			}

			clearDisplay();


			permission_denied = buttonInterjectionNoAckPlusData(ASKUSER_SHOW_DISPLAYPHRASE, display_phrase, 0);
			if (!permission_denied)
			{
				// User approved action.
				approved3 = true;
			}
			if(approved3)
			{
				char encryption_phrase_char[17]={};
				char *encryption_phrase_char_ptr;

				encryption_phrase_char_ptr = &encryption_phrase_char[0];

				uint8_t encryption_phrase_array[16]={};
				uint8_t *encryption_phrase;

				encryption_phrase = &encryption_phrase_array[0];


				permission_denied = buttonInterjectionNoAckSetup(ASKUSER_AEM_PASSPHRASE);
				if (!permission_denied)
				{
					// User approved action.
					approved4 = true;
				}
				if(approved4)
				{

					buttonInterjectionNoAckSetup(ASKUSER_ALPHA_INPUT_PREFACE);


					encryption_phrase_char_ptr = getInput(true, false);
					int i;
					for (i = 0; i<sizeof(encryption_phrase_char)-1;i++)
					{
						encryption_phrase_array[i] = encryption_phrase_char_ptr[i];
					}

					clearDisplay();
					permission_denied = buttonInterjectionNoAckPlusData(ASKUSER_SHOW_UNLOCKPHRASE, encryption_phrase_char_ptr, 0);
					if (!permission_denied)
					{
						// User approved action.
						approved5 = true;
					}
					if(approved5)
					{

						encryptStreamSized(display_phrase_array, encryption_phrase_array, 64);

						nonVolatileWrite(display_phrase_array, AEM_PHRASE_ADDRESS, 64);
						toggleAEM(true);
					}
				}
			}
		}
	}
}

int fetchDevicePINWrongCount(void)
{
	int isForm;
	uint8_t tempComms[1];
	nonVolatileRead(tempComms, WRONG_DEVICE_PIN_COUNT_ADDRESS, 1);

	isForm = (int)tempComms[0];

	return isForm;
}

int fetchTransactionPINWrongCount(void)
{
	int isForm;
	uint8_t tempComms[1];
	nonVolatileRead(tempComms, WRONG_TRANSACTION_PIN_COUNT_ADDRESS, 1);

	isForm = (int)tempComms[0];

	return isForm;
}


const int keyDelayNoDisplay = 200;

int entryDelayNoDisplay = 0;

uint8_t *fetchPIN(void)
{
	uint8_t tempPIN[32];
	nonVolatileRead(tempPIN, PIN_ADDRESS, 32);
	return tempPIN;
}

void pinStatusCheckandPremadePIN()
{
	uint8_t temp[32];
	uint8_t ref_compare_hash[32];
	uint8_t j;
	HashState *sig_hash_hs_ptr;
	HashState sig_hash_hs;
	HashState *sig_hash_hs_ptr2;
	HashState sig_hash_hs2;

	sig_hash_hs_ptr = &sig_hash_hs;
	sig_hash_hs_ptr2 = &sig_hash_hs2;

	uint8_t *hash_ptr;
	uint8_t hash[32] = {};
	uint8_t *hash2_ptr;
	uint8_t hash2[32] = {};

	//  char *p;
	//  p=&theStringArray[0];

	char bufferPIN1array[21]={};
	char bufferPIN2array[21]={};
	char bufferPIN3array[21]={};

	char *bufferPIN1;
	char *bufferPIN2;
	char *bufferPIN3;

	bufferPIN1 = &bufferPIN1array[0];
	bufferPIN2 = &bufferPIN2array[0];
	bufferPIN3 = &bufferPIN3array[0];


	int pinStatus;
	pinStatus = checkHasPIN();

	if(pinStatus != 127)
		{
			char rChar;
			int r;
			bool yesOrNo;


			buttonInterjectionNoAckSetup(ASKUSER_DESCRIBE_STANDARD_SETUP_2);

			rChar = waitForNumberButtonPress4to8();
			clearDisplay();
			r = rChar - '0';

			if(rChar == 'N')
			{
				useWhatSetup();
			}else
			{
				generateInsecurePIN(bufferPIN1, r+1);

				sha256Begin(sig_hash_hs_ptr);

				for (j=0;j<20;j++)
				{
					sha256WriteByte(sig_hash_hs_ptr, bufferPIN1[j]);
				}
				sha256FinishDouble(sig_hash_hs_ptr);
				writeHashToByteArray(hash, sig_hash_hs_ptr, false);

				if(1)
				{
					int s = 0;
					uint8_t temp1[1];
					temp1[0] = s;
					nonVolatileWrite(temp1, WRONG_DEVICE_PIN_COUNT_ADDRESS, 1);

					buttonInterjectionNoAckPlusData(ASKUSER_SET_DEVICE_PIN_BIG, bufferPIN1, r);
					yesOrNo = waitForButtonPress();
					clearDisplay();
					if(!yesOrNo)
					{
						nonVolatileWrite(hash, PIN_ADDRESS, 32);

						int s = 127;
						uint8_t temp1[1];
						temp1[0] = s;
						nonVolatileWrite(temp1, HAS_PIN_ADDRESS, 1);

						int type = 127;
						uint8_t temp2[1];
						temp2[0] = type;
						nonVolatileWrite(temp2, SETUP_TYPE_ADDRESS, 1);


					}else if(yesOrNo)
					{
						pinStatusCheckandPremadePIN();
					}
				}
				else
				{
//					writeNotEqual_Screen();
//	//				writeEink("PIN NOT EQUAL", false, STATUS_X, STATUS_Y);
//					pinStatusCheck();
				}
			}
		}
		else if (pinStatus == 127)
		{
			checkDevicePIN(false);
/*
	  		memset(bufferPIN3, 0, 20);
	  		strcpy(bufferPIN3, "");
			writeEink("PIN:", false, STATUS_X, STATUS_Y);
			bufferPIN3 = getInput(false);


			char *duress = "911" ;
			if(*bufferPIN3 == *duress)
			{
//				writeEink("STOMP", false, STATUS_X, STATUS_Y);
				duressFormat();
				while(1){;;};
			}
			checkHashesNoDisplay(bufferPIN3);
*/
		}
		else
		{
			writeEink("PIN ERROR - REFLASH", false, STATUS_X, STATUS_Y);
			while(1){;;};
		}
	return;
}


void checkDevicePIN(bool displayAlpha)
{
	char bufferPIN3array[21]={};
	char *bufferPIN3;

	bufferPIN3 = &bufferPIN3array[0];

//	memset(&bufferPIN3array[0], 0, sizeof(bufferPIN3array));
	memset(bufferPIN3, 0, 21);
	strcpy(bufferPIN3, "");

	if(displayAlpha){
		buttonInterjectionNoAck(ASKUSER_ENTER_PIN_ALPHA);
	}else{
		buttonInterjectionNoAck(ASKUSER_ENTER_PIN);
	}

	bufferPIN3 = getInput(displayAlpha, false);

	clearDisplay();
//	writeEink("before checkhashes", false, STATUS_X, STATUS_Y);
	checkHashes(bufferPIN3, displayAlpha);
//	writeEink("after checkhashes", false, STATUS_X, STATUS_Y);

}

//bool checkDevicePIN(bool displayAlpha)
//{
//	char bufferPIN3array[21]={};
////	char *bufferPIN3;
//	bool results;
////	bufferPIN3 = &bufferPIN3array[0];
//
//	memset(&bufferPIN3array[0], 0, sizeof(bufferPIN3array));
////	memset(bufferPIN3, 0, 21);
////	strcpy(bufferPIN3, "");
//
//	if(displayAlpha){
//		buttonInterjectionNoAck(ASKUSER_ENTER_PIN_ALPHA);
//	}else{
//		buttonInterjectionNoAck(ASKUSER_ENTER_PIN);
//	}
//
//	bufferPIN3array = getInput(displayAlpha, false);
//
//	clearDisplay();
////	writeEink("before checkhashes", false, STATUS_X, STATUS_Y);
//	results = checkHashes(&bufferPIN3array[0], displayAlpha);
////	writeEink("after checkhashes", false, STATUS_X, STATUS_Y);
//	return results;
//}


char *getTransactionPINfromUser(void)
{
	char bufferPIN3array[21]={};
	char *bufferPIN3;

	bufferPIN3 = &bufferPIN3array[0];

	memset(bufferPIN3, 0, 21);
	strcpy(bufferPIN3, "");

	buttonInterjectionNoAck(ASKUSER_ENTER_TRANSACTION_PIN);


	bufferPIN3 = getInput(false, false);


	return bufferPIN3;
}



void duressFormat()
{
//	writeEink("STOMP2", false, STATUS_X, STATUS_Y);
	writeCheck_Screen();
//	writeEink("STOMP3", false, STATUS_X, STATUS_Y);
	initialFormatAutoDuress();
//	writeEink("STOMP4", false, STATUS_X, STATUS_Y);
	useWhatCommsDuress();
}


void pinStatusCheck()
{
	uint8_t temp[32];
	uint8_t ref_compare_hash[32];
	uint8_t j;
	HashState *sig_hash_hs_ptr;
	HashState sig_hash_hs;
	HashState *sig_hash_hs_ptr2;
	HashState sig_hash_hs2;

	sig_hash_hs_ptr = &sig_hash_hs;
	sig_hash_hs_ptr2 = &sig_hash_hs2;

	uint8_t *hash_ptr;
	uint8_t hash[32] = {};
	uint8_t *hash2_ptr;
	uint8_t hash2[32] = {};


	char bufferPIN1array[21]={};
	char bufferPIN2array[21]={};
	char bufferPIN3array[21]={};

	char *bufferPIN1;
	char *bufferPIN2;
	char *bufferPIN3;

	bufferPIN1 = &bufferPIN1array[0];
	bufferPIN2 = &bufferPIN2array[0];
	bufferPIN3 = &bufferPIN3array[0];


	int pinStatus;
	pinStatus = checkHasPIN();

	if(pinStatus != 127)
	{
		char rChar;
		int r;
		bool yesOrNo;

		buttonInterjectionNoAckSetup(ASKUSER_DESCRIBE_ADVANCED_SETUP_2);
//		initDisplay();
//
//		overlayBatteryStatus(BATT_VALUE_DISPLAY);
//		writeEinkNoDisplay("ADVANCED SETUP",  COL_1_X, LINE_1_Y, "TO SET DEVICE PIN",COL_1_X,LINE_2_Y, "-PRESS AND HOLD TO CYCLE",COL_1_X,LINE_3_Y, "-RELEASE TO SELECT",COL_1_X,LINE_4_Y, "",25,80);
//		writeUnderline(STRIPE_X_START, STRIPE_Y_START, STRIPE_X_END, STRIPE_Y_END);
//		writeEinkNoDisplay("",  0, 0, "",0,0, "",0,0, "GO",22,80, "BACK",148,80);
//		drawCheck(3,85);
//		drawX(180,87);
//
//		display();

		yesOrNo = waitForButtonPress();
		clearDisplay();
		if(!yesOrNo)
		{
			buttonInterjectionNoAckSetup(ASKUSER_ALPHA_INPUT_PREFACE);

//			initDisplay();
//			writeEinkNoDisplaySingle("MINIMUM 4 CHARACTERS",  COL_1_X, LINE_1_Y);
//			writeUnderline(STRIPE_X_START, STRIPE_Y_START, STRIPE_X_END, STRIPE_Y_END);
//			display();
//			initDisplay();
//			writeEinkNoDisplaySingle("PRESS/HOLD/RELEASE",  COL_1_X, LINE_1_Y);
//			writeUnderline(STRIPE_X_START, STRIPE_Y_START, STRIPE_X_END, STRIPE_Y_END);
//			display();
			bufferPIN1 = getInput(true, true);
			clearDisplay();
		}else if(yesOrNo)
		{
			useWhatSetup();
		}

		sha256Begin(sig_hash_hs_ptr);

		for (j=0;j<20;j++)
		{
			sha256WriteByte(sig_hash_hs_ptr, bufferPIN1[j]);
		}
		sha256FinishDouble(sig_hash_hs_ptr);
		writeHashToByteArray(hash, sig_hash_hs_ptr, false);


		if(1)
		{
			int s = 0;
			uint8_t temp1[1];
			temp1[0] = s;
			nonVolatileWrite(temp1, WRONG_DEVICE_PIN_COUNT_ADDRESS, 1);

			buttonInterjectionNoAckPlusData(ASKUSER_SET_DEVICE_PIN, bufferPIN1, sizeof(*bufferPIN1));

			yesOrNo = waitForButtonPress();
			clearDisplay();

			if(!yesOrNo)
			{
				nonVolatileWrite(hash, PIN_ADDRESS, 32);

				int s = 127;
				uint8_t temp1[1];
				temp1[0] = s;
				nonVolatileWrite(temp1, HAS_PIN_ADDRESS, 1);

				int type = 128;
				uint8_t temp2[1];
				temp2[0] = type;
				nonVolatileWrite(temp2, SETUP_TYPE_ADDRESS, 1);

				delay(1000);
			}else if(yesOrNo)
			{
				strcpy(bufferPIN1array, "");
				useWhatSetup();
			}
		}
		else
		{
//			writeNotEqual_Screen();
//			writeEink("PINs DO NOT MATCH", false, STATUS_X, STATUS_Y);
//			pinStatusCheck();
		}
	}
	else if (pinStatus == 127)
	{
		checkDevicePIN(true);
//
//		memset(bufferPIN3, 0, 20);
//		strcpy(bufferPIN3, "");
//
//		initDisplay();
//		overlayBatteryStatus();
//		writeEinkNoDisplay("PIN:",  STATUS_X, STATUS_Y, "",5,25, "",5,40, "",5,55, "",25,80);
//		display();
//
//		bufferPIN3 = getInput(true, false);
//		checkHashes(bufferPIN3, true);
	}
	else
	{
		writeEink("PIN ERROR - REFLASH", false, STATUS_X, STATUS_Y);
		while(1){;;};
	}
	return;
}

void pinStatusCheckExpert()
{
	uint8_t temp[32];
	uint8_t ref_compare_hash[32];
	uint8_t j;
	HashState *sig_hash_hs_ptr;
	HashState sig_hash_hs;
	HashState *sig_hash_hs_ptr2;
	HashState sig_hash_hs2;

	sig_hash_hs_ptr = &sig_hash_hs;
	sig_hash_hs_ptr2 = &sig_hash_hs2;

	uint8_t *hash_ptr;
	uint8_t hash[32] = {};
	uint8_t *hash2_ptr;
	uint8_t hash2[32] = {};


	char bufferPIN1array[21]={};
	char bufferPIN2array[21]={};
	char bufferPIN3array[21]={};

	char *bufferPIN1;
	char *bufferPIN2;
	char *bufferPIN3;

	bufferPIN1 = &bufferPIN1array[0];
	bufferPIN2 = &bufferPIN2array[0];
	bufferPIN3 = &bufferPIN3array[0];


	int pinStatus;
	pinStatus = checkHasPIN();

	if(pinStatus != 127)
	{
		char rChar;
		int r;
		bool yesOrNo;

		buttonInterjectionNoAckSetup(ASKUSER_DESCRIBE_EXPERT_SETUP_2);
//		initDisplay();
//
//		overlayBatteryStatus(BATT_VALUE_DISPLAY);
//		writeEinkNoDisplay("EXPERT SETUP",  COL_1_X, LINE_1_Y, "TO SET DEVICE PIN",COL_1_X,LINE_2_Y, "-PRESS AND HOLD TO CYCLE",COL_1_X,LINE_3_Y, "-RELEASE TO SELECT",COL_1_X,LINE_4_Y, "",25,80);
//		writeUnderline(STRIPE_X_START, STRIPE_Y_START, STRIPE_X_END, STRIPE_Y_END);
//		writeEinkNoDisplay("",  0, 0, "",0,0, "",0,0, "GO",22,80, "BACK",148,80);
//		drawCheck(3,85);
//		drawX(180,87);
//
//		display();

		yesOrNo = waitForButtonPress();
		clearDisplay();
		if(!yesOrNo)
		{
			buttonInterjectionNoAckSetup(ASKUSER_ALPHA_INPUT_PREFACE);

//			initDisplay();
//			writeEinkNoDisplaySingle("MINIMUM 4 CHARACTERS",  COL_1_X, LINE_1_Y);
//			writeUnderline(STRIPE_X_START, STRIPE_Y_START, STRIPE_X_END, STRIPE_Y_END);
//			display();
//			initDisplay();
//			writeEinkNoDisplaySingle("PRESS/HOLD/RELEASE",  COL_1_X, LINE_1_Y);
//			writeUnderline(STRIPE_X_START, STRIPE_Y_START, STRIPE_X_END, STRIPE_Y_END);
//			display();
			bufferPIN1 = getInput(true, true);
			clearDisplay();
		}else if(yesOrNo)
		{
			useWhatSetup();
		}

		sha256Begin(sig_hash_hs_ptr);

		for (j=0;j<PIN_MAX_SIZE;j++)
		{
			sha256WriteByte(sig_hash_hs_ptr, bufferPIN1[j]);
		}
		sha256FinishDouble(sig_hash_hs_ptr);
		writeHashToByteArray(hash, sig_hash_hs_ptr, false);


		if(1)
		{
			int s = 0;
			uint8_t temp1[1];
			temp1[0] = s;
			nonVolatileWrite(temp1, WRONG_DEVICE_PIN_COUNT_ADDRESS, 1);

			buttonInterjectionNoAckPlusData(ASKUSER_SET_DEVICE_PIN, bufferPIN1, 0);


			yesOrNo = waitForButtonPress();
			clearDisplay();

			if(!yesOrNo)
			{
				nonVolatileWrite(hash, PIN_ADDRESS, 32);

				int s = 127;
				uint8_t temp1[1];
				temp1[0] = s;
				nonVolatileWrite(temp1, HAS_PIN_ADDRESS, 1);

				int type = 129;
				uint8_t temp2[1];
				temp2[0] = type;
				nonVolatileWrite(temp2, SETUP_TYPE_ADDRESS, 1);

				delay(1000);
			}else if(yesOrNo)
			{
				useWhatSetup();
			}
		}
		else
		{
//			writeNotEqual_Screen();
//			writeEink("PINs DO NOT MATCH", false, STATUS_X, STATUS_Y);
//			pinStatusCheck();
		}
	}
	else if (pinStatus == 127)
	{
		checkDevicePIN(true);
//		memset(bufferPIN3, 0, 20);
//		strcpy(bufferPIN3, "");
//
//		initDisplay();
//		overlayBatteryStatus();
//		writeEinkNoDisplay("PIN:",  STATUS_X, STATUS_Y, "",5,25, "",5,40, "",5,55, "",25,80);
//		display();
//
//		bufferPIN3 = getInput(true, false);
//		checkHashes(bufferPIN3, true);
	}
	else
	{
		writeEink("PIN ERROR - REFLASH", false, STATUS_X, STATUS_Y);
		while(1){;;};
	}
	return;
}



char *mnemonic_input_stacker(int mlen)
{

	int i, j, idx;
	const char *theResult;
	static char mnemo[24 * 10];
	char *p = mnemo;


	char mlenText[1];
	sprintf(mlenText,"%lu", mlen);


	for (i=0; i<mlen;i++)
	{
		char iText[1];
		sprintf(iText,"%lu", i+1);

		initDisplay();
		writeEinkNoDisplay("MNEMONIC",  COL_1_X, LINE_1_Y, iText,COL_1_X+75,LINE_1_Y, "of",COL_1_X+100,LINE_1_Y, mlenText,COL_1_X+125,LINE_1_Y, "",25,80);
		display();
		theResult = mnemonic_input();
		strcpy(p, theResult);
		p += strlen(theResult);
		*p = (i < mlen - 1) ? ' ' : 0;
		p++;
	}
	showWorking();
	return mnemo;
}


//Need to put a wrapper and display on this to loop the query
char *mnemonic_input(void)
{
	  int i;
	  char theChar;
	  static char staticBuffer[21] = {};
	  char tempBuffer[21] = {};
	  int j;
	  bool caps = false;
	  bool displayInput = true;
	  memset(staticBuffer, 0, 21);
	  strcpy(staticBuffer, "");

	  for (i=0; i<20; i++)
	  {
	        #if defined(__SAM3X8E__)
	        theChar = alphkeypad((8*i)+INPUT_X, INPUT_Y);
			#endif

			#if defined(__SAM3A8C__)
	        if(displayInput)
				{
	        	if(caps)
					{
						theChar = alphkeypad_noNumbers_3A8C((8*i)+INPUT_X, INPUT_Y);
					}else
					{
						theChar = alphkeypad_noNumbers_3A8C((8*i)+INPUT_X, INPUT_Y);
					}
				}
				else
				{
					theChar = alphkeypad_noNumbers_3A8C((8*i)+INPUT_X, INPUT_Y);
				}
			#endif

	            if(theChar == '@')
	             {
	             	break;
	             }
	             else if(theChar == '^')
	             {
	             	i = i - 1;
	             	caps = !caps;
	             	writeSelectedCharAndStringBlankingMnemonics((8*i)+INPUT_X, INPUT_Y, caps);
	             }
	             else if(theChar == '~' && (i != 0))
	             {
	             	strncpy(tempBuffer, staticBuffer, i-1);
	     	  		strcpy(staticBuffer, "");
	     	  		strcpy(staticBuffer, tempBuffer);
	             	i= i - 2;
	             	if(displayInput){
	             		writeSelectedCharAndStringBlankingMnemonics((8*i)+INPUT_X, INPUT_Y, caps);
	             	}
	             }
	             else if(theChar == '~' && (i == 0))
	             {
	             	i = i - 1;
	     				if(displayInput){
//	     					checkDevicePIN(false);
	     					break;
	     				}else{
//	     					checkDevicePIN(true);
	     					break;
	     				}
	             }
	             else
	             {
	             	staticBuffer[i] = theChar;
	             }

	  }
	  return staticBuffer;
}



char *getInput(bool displayInput, bool initialSetup) {
  int i;
  char theChar;
  static char staticBuffer[21] = {};
  char tempBuffer[21] = {};
  int j;
  bool caps = false;

  memset(&staticBuffer[0], 0, sizeof(staticBuffer));


  for (i=0; i<20; i++)
  {
        #if defined(__SAM3X8E__)
        theChar = alphkeypad((8*i)+INPUT_X, INPUT_Y);
		#endif

		#if defined(__SAM3A8C__)
        if(displayInput)
			{
        	if(caps)
				{
					theChar = alphkeypad_3A8C_CAPS((8*i)+INPUT_X, INPUT_Y);
				}else
				{
					theChar = alphkeypad_3A8C((8*i)+INPUT_X, INPUT_Y);
				}
			}
			else
			{
				theChar = alphkeypad_3A8CnoDisplay((8*i)+INPUT_X, INPUT_Y);
			}
		#endif

        if(initialSetup)
        {
        	j = 3;
            if(theChar == '@' && i > j)
             {
             	break;
             }
             else if(theChar == '@' && i <= j)
             {
             	i = i - 1;
             }
             else if(theChar == '^')
             {
             	i = i - 1;
             	caps = !caps;
             	writeSelectedCharAndStringBlanking((8*i)+INPUT_X, INPUT_Y, caps);
             }
             else if(theChar == '~' && (i != 0))
             {
            	memset(&tempBuffer[0], 0, sizeof(tempBuffer));
             	strncpy(tempBuffer, staticBuffer, i-1);
             	memset(&staticBuffer[0], 0, sizeof(staticBuffer));
     	  		strncpy(staticBuffer, tempBuffer, i-1);
             	i= i - 2;
             	if(display){
             		writeSelectedCharAndStringBlanking((8*i)+INPUT_X, INPUT_Y, caps);
             	}
             }
             else if(theChar == '~' && (i == 0))
             {
             	i = i - 1;
             	if(!initialSetup)
             	{
     				if(displayInput){//############################################################
     	//        		writeEink("before cDP true>false", false, STATUS_X, STATUS_Y);
     					memset(&staticBuffer[0], 0, sizeof(staticBuffer));
     					checkDevicePIN(false);
     	//        		writeEink("stuck cDP true>false", false, STATUS_X, STATUS_Y);

     					break;
     				}else{
     	//        		writeEink("before cDP false>true", false, STATUS_X, STATUS_Y);
     					memset(&staticBuffer[0], 0, sizeof(staticBuffer));
     					checkDevicePIN(true);
     	//        		writeEink("stuck cDP false>true", false, STATUS_X, STATUS_Y);
     					break;
     				}
             	}else if(initialSetup)
             	{
             		useWhatSetup();
             	}
             }
             else
             {
             	staticBuffer[i] = theChar;
             }
        }
        else
        {
            if(theChar == '@')
             {
             	break;
             }
             else if(theChar == '^')
             {
             	i = i - 1;
             	caps = !caps;
             	writeSelectedCharAndStringBlanking((8*i)+INPUT_X, INPUT_Y, caps);
             }
             else if(theChar == '~' && (i != 0) && displayInput)
             {
             	memset(&tempBuffer[0], 0, sizeof(tempBuffer));
              	strncpy(tempBuffer, staticBuffer, i-1);
              	memset(&staticBuffer[0], 0, sizeof(staticBuffer));
      	  		strncpy(staticBuffer, tempBuffer, i-1);
             	i= i - 2;
             	if(display){
             		writeSelectedCharAndStringBlanking((8*i)+INPUT_X, INPUT_Y, caps);
             	}
             }
             else if(theChar == '~' && (i == 0))
             {
             	i = i - 1;
             	if(!initialSetup)
             	{
     				if(displayInput){
     	//        		writeEink("before cDP true>false", false, STATUS_X, STATUS_Y);
     					memset(&staticBuffer[0], 0, sizeof(staticBuffer));
     					checkDevicePIN(false);
     	//        		writeEink("stuck cDP true>false", false, STATUS_X, STATUS_Y);
     					break;
     				}else{
     	//        		writeEink("before cDP false>true", false, STATUS_X, STATUS_Y);
     					memset(&staticBuffer[0], 0, sizeof(staticBuffer));
     					checkDevicePIN(true);
     	//        		writeEink("stuck cDP false>true", false, STATUS_X, STATUS_Y);
     					break;
     				}
             	}else if(initialSetup)
             	{
             		useWhatSetup();
             	}
             }
             else
             {
             	staticBuffer[i] = theChar;
             }
       }
  }
  return staticBuffer;
}





char *getInputIndices(bool displayInput, bool initialSetup) {
  int i;
  char theChar;
  static char staticBuffer[21] = {};
  static char finalBuffer[21] = {};
  char tempBuffer[21] = {};
  int j;
  bool caps = false;

  memset(staticBuffer, 0, 21);
  strcpy(staticBuffer, "");

  for (i=0; i<20; i++)
  {
        #if defined(__SAM3X8E__)
        theChar = alphkeypad((8*i)+INPUT_X, INPUT_Y);
		#endif

		#if defined(__SAM3A8C__)
        if(displayInput)
			{
        	if(caps)
				{
					theChar = alphkeypad_3A8C_CAPS((8*i)+INPUT_X, INPUT_Y);
				}else
				{
					theChar = alphkeypad_3A8C((8*i)+INPUT_X, INPUT_Y);
				}
			}
			else
			{
				theChar = alphkeypad_3A8CnoDisplay((8*i)+INPUT_X, INPUT_Y);
			}
		#endif

        if(initialSetup)
        {
        	j = 3;
            if(theChar == '@' && i > j)
             {
             	break;
             }
             else if(theChar == '@' && i <= j)
             {
             	i = i - 1;
             }
             else if(theChar == '^')
             {
             	i = i - 1;
             	caps = !caps;
             	writeSelectedCharAndStringBlanking((8*i)+INPUT_X, INPUT_Y, caps);
             }
             else if(theChar == '~' && (i != 0))
             {
             	memset(&tempBuffer[0], 0, sizeof(tempBuffer));
              	strncpy(tempBuffer, staticBuffer, i-1);
              	memset(&staticBuffer[0], 0, sizeof(staticBuffer));
      	  		strncpy(staticBuffer, tempBuffer, i-1);
             	i= i - 2;
             	if(display){
             		writeSelectedCharAndStringBlanking((8*i)+INPUT_X, INPUT_Y, caps);
             	}
             }
             else if(theChar == '~' && (i == 0))
             {
             	i = i - 1;
             	if(!initialSetup)
             	{
     				if(displayInput){//############################################################
     	//        		writeEink("before cDP true>false", false, STATUS_X, STATUS_Y);
     					checkDevicePIN(false);
     	//        		writeEink("stuck cDP true>false", false, STATUS_X, STATUS_Y);

     					break;
     				}else{
     	//        		writeEink("before cDP false>true", false, STATUS_X, STATUS_Y);
     					checkDevicePIN(true);
     	//        		writeEink("stuck cDP false>true", false, STATUS_X, STATUS_Y);
     					break;
     				}
             	}else if(initialSetup)
             	{
             		useWhatSetup();
             	}
             }
             else
             {
             	staticBuffer[i] = theChar;
             }
        }
        else
        {
            if(theChar == '@')
             {
             	break;
             }
//             else if(theChar == '^')
//             {
//             	i = i - 1;
//             	caps = !caps;
//             	writeSelectedCharAndStringBlanking((8*i)+INPUT_X, INPUT_Y, caps);
//             }
             else if(theChar == '~' && (i != 0))
             {
             	memset(&tempBuffer[0], 0, sizeof(tempBuffer));
              	strncpy(tempBuffer, staticBuffer, i-1);
              	memset(&staticBuffer[0], 0, sizeof(staticBuffer));
      	  		strncpy(staticBuffer, tempBuffer, i-1);
             	i= i - 1;
             	if(display){
             		writeSelectedCharAndStringBlanking((8*i)+INPUT_X, INPUT_Y, caps);
             	}
             }
//             else if(theChar == '~' && (i == 0))
//             {
//             	i = i - 1;
//             	if(!initialSetup)
//             	{
//     				if(displayInput){
//     	//        		writeEink("before cDP true>false", false, STATUS_X, STATUS_Y);
//     					strcpy(staticBuffer, "");
//     					checkDevicePIN(false);
//     	//        		writeEink("stuck cDP true>false", false, STATUS_X, STATUS_Y);
//     					break;
//     				}else{
//     	//        		writeEink("before cDP false>true", false, STATUS_X, STATUS_Y);
//     					strcpy(staticBuffer, "");
//     					checkDevicePIN(true);
//     	//        		writeEink("stuck cDP false>true", false, STATUS_X, STATUS_Y);
//     					break;
//     				}
//             	}else if(initialSetup)
//             	{
//             		useWhatSetup();
//     //        		initDisplay();
//     //        		overlayBatteryStatus(BATT_VALUE_DISPLAY);
//     //        		writeEinkNoDisplay("SET NUMERIC PIN:",  COL_1_X, LINE_1_Y, "",0,0, "",  0,0, "",  0,0, "",5,35);
//     //        		writeUnderline(STRIPE_X_START, STRIPE_Y_START, STRIPE_X_END, STRIPE_Y_END);
//     //				writeEinkNoDisplay("",  0, 0, "",0,0, "",0,0, "GO",22,80, "NUM",145,80);
//     //				drawCheck(3,85);
//     //           		drawX(180,87);
//     //        		display();
//     //        		break;
//     //        		getInput(!displayInput, true);
//             	}
//             }
//             else
             {
             	staticBuffer[i] = theChar;
             }
       }
  }
//  strncpy(finalBuffer, staticBuffer, i);
  return staticBuffer;
}


//int getInputIndicesInt(bool displayInput, bool initialSetup) {
//  int i;
//  char theChar;
//  static char staticBuffer[21] = {};
//  static char finalBuffer[21] = {};
//  char tempBuffer[21] = {};
//  int j;
//  bool caps = false;
//
//  memset(staticBuffer, 0, 21);
//  strcpy(staticBuffer, "");
//
//  for (i=0; i<20; i++)
//  {
//        #if defined(__SAM3X8E__)
//        theChar = alphkeypad((8*i)+INPUT_X, INPUT_Y);
//		#endif
//
//		#if defined(__SAM3A8C__)
//        if(displayInput)
//			{
//        	if(caps)
//				{
//					theChar = alphkeypad_3A8C_CAPS((8*i)+INPUT_X, INPUT_Y);
//				}else
//				{
//					theChar = alphkeypad_3A8C((8*i)+INPUT_X, INPUT_Y);
//				}
//			}
//			else
//			{
//				theChar = alphkeypad_3A8CnoDisplay((8*i)+INPUT_X, INPUT_Y);
//			}
//		#endif
//
//        if(initialSetup)
//        {
//        	j = 3;
//            if(theChar == '@' && i > j)
//             {
//             	break;
//             }
//             else if(theChar == '@' && i <= j)
//             {
//             	i = i - 1;
//             }
//             else if(theChar == '^')
//             {
//             	i = i - 1;
//             	caps = !caps;
//             	writeSelectedCharAndStringBlanking((8*i)+INPUT_X, INPUT_Y, caps);
//             }
//             else if(theChar == '~' && (i != 0))
//             {
//             	memset(&tempBuffer[0], 0, sizeof(tempBuffer));
//              	strncpy(tempBuffer, staticBuffer, i-1);
//              	memset(&staticBuffer[0], 0, sizeof(staticBuffer));
//      	  		strncpy(staticBuffer, tempBuffer, i-1);
//             	i= i - 2;
//             	if(display){
//             		writeSelectedCharAndStringBlanking((8*i)+INPUT_X, INPUT_Y, caps);
//             	}
//             }
//             else if(theChar == '~' && (i == 0))
//             {
//             	i = i - 1;
//             	if(!initialSetup)
//             	{
//     				if(displayInput){//############################################################
//     	//        		writeEink("before cDP true>false", false, STATUS_X, STATUS_Y);
//     					checkDevicePIN(false);
//     	//        		writeEink("stuck cDP true>false", false, STATUS_X, STATUS_Y);
//
//     					break;
//     				}else{
//     	//        		writeEink("before cDP false>true", false, STATUS_X, STATUS_Y);
//     					checkDevicePIN(true);
//     	//        		writeEink("stuck cDP false>true", false, STATUS_X, STATUS_Y);
//     					break;
//     				}
//             	}else if(initialSetup)
//             	{
//             		useWhatSetup();
//             	}
//             }
//             else
//             {
//             	staticBuffer[i] = theChar;
//             }
//        }
//        else
//        {
//            if(theChar == '@')
//             {
//             	break;
//             }
////             else if(theChar == '^')
////             {
////             	i = i - 1;
////             	caps = !caps;
////             	writeSelectedCharAndStringBlanking((8*i)+INPUT_X, INPUT_Y, caps);
////             }
//             else if(theChar == '~' && (i != 0))
//             {
//             	memset(&tempBuffer[0], 0, sizeof(tempBuffer));
//              	strncpy(tempBuffer, staticBuffer, i-1);
//              	memset(&staticBuffer[0], 0, sizeof(staticBuffer));
//      	  		strncpy(staticBuffer, tempBuffer, i-1);
//             	i= i - 1;
//             	if(display){
//             		writeSelectedCharAndStringBlanking((8*i)+INPUT_X, INPUT_Y, caps);
//             	}
//             }
////             else if(theChar == '~' && (i == 0))
////             {
////             	i = i - 1;
////             	if(!initialSetup)
////             	{
////     				if(displayInput){
////     	//        		writeEink("before cDP true>false", false, STATUS_X, STATUS_Y);
////     					strcpy(staticBuffer, "");
////     					checkDevicePIN(false);
////     	//        		writeEink("stuck cDP true>false", false, STATUS_X, STATUS_Y);
////     					break;
////     				}else{
////     	//        		writeEink("before cDP false>true", false, STATUS_X, STATUS_Y);
////     					strcpy(staticBuffer, "");
////     					checkDevicePIN(true);
////     	//        		writeEink("stuck cDP false>true", false, STATUS_X, STATUS_Y);
////     					break;
////     				}
////             	}else if(initialSetup)
////             	{
////             		useWhatSetup();
////     //        		initDisplay();
////     //        		overlayBatteryStatus(BATT_VALUE_DISPLAY);
////     //        		writeEinkNoDisplay("SET NUMERIC PIN:",  COL_1_X, LINE_1_Y, "",0,0, "",  0,0, "",  0,0, "",5,35);
////     //        		writeUnderline(STRIPE_X_START, STRIPE_Y_START, STRIPE_X_END, STRIPE_Y_END);
////     //				writeEinkNoDisplay("",  0, 0, "",0,0, "",0,0, "GO",22,80, "NUM",145,80);
////     //				drawCheck(3,85);
////     //           		drawX(180,87);
////     //        		display();
////     //        		break;
////     //        		getInput(!displayInput, true);
////             	}
////             }
////             else
//             {
//             	staticBuffer[i] = theChar;
//             }
//       }
//  }
//  strncpy(finalBuffer, staticBuffer, i);
//  return finalBuffer;
//}


char *getInputAEM(bool displayInput, bool initialSetup)
{
  int i;
  char theChar;
  static char staticBuffer[21] = {};
  static char finalBuffer[21] = {};
  char tempBuffer[21] = {};
  int j;
  bool caps = false;

  memset(staticBuffer, 0, 21);
  strcpy(staticBuffer, "");

  for (i=0; i<20; i++)
  {
        if(displayInput)
		{
			if(caps)
			{
				theChar = alphkeypad_3A8C_CAPS((8*i)+INPUT_X, INPUT_Y);
			}
			else
			{
				theChar = alphkeypad_3A8C((8*i)+INPUT_X, INPUT_Y);
			}
		}
		else
		{
			theChar = alphkeypad_3A8CnoDisplay((8*i)+INPUT_X, INPUT_Y);
		}


		if(theChar == '@')
		 {
			break;
		 }
		 else if(theChar == '^')
		 {
			i = i - 1;
			caps = !caps;
			writeSelectedCharAndStringBlanking((8*i)+INPUT_X, INPUT_Y, caps);
		 }
		 else if(theChar == '~' && (i != 0) && displayInput)
		 {
         	memset(&tempBuffer[0], 0, sizeof(tempBuffer));
          	strncpy(tempBuffer, staticBuffer, i-1);
          	memset(&staticBuffer[0], 0, sizeof(staticBuffer));
  	  		strncpy(staticBuffer, tempBuffer, i-1);
			i= i - 2;
			if(displayInput)
			{
				writeSelectedCharAndStringBlanking((8*i)+INPUT_X, INPUT_Y, caps);
			}
		 }
		 else if(theChar == '~' && (i == 0))
			 {
				i = i - 1;
				if(!initialSetup)
					{
						if(displayInput)
							{
				//        		writeEink("before cDP true>false", false, STATUS_X, STATUS_Y);
//								checkDevicePIN(false);
								memset(&staticBuffer[0], 0, sizeof(staticBuffer));
								doAEMValidate(false);
//								getInputAEM(false, false);
				//        		writeEink("stuck cDP true>false", false, STATUS_X, STATUS_Y);
								break;
							}
						else
							{
				//        		writeEink("before cDP false>true", false, STATUS_X, STATUS_Y);
//								checkDevicePIN(true);
								memset(&staticBuffer[0], 0, sizeof(staticBuffer));
								doAEMValidate(true);
//								getInputAEM(true, false);
				//        		writeEink("stuck cDP false>true", false, STATUS_X, STATUS_Y);
								break;
							}
					}

//				if(!initialSetup)
//					{
//						if(displayInput)
//							{
//				//        		writeEink("before cDP true>false", false, STATUS_X, STATUS_Y);
////								checkDevicePIN(false);
//				          		memset(&staticBuffer[0], 0, sizeof(staticBuffer));
//								doAEMValidate(false);
//				//        		writeEink("stuck cDP true>false", false, STATUS_X, STATUS_Y);
//								break;
//							}
//						else
//							{
//				//        		writeEink("before cDP false>true", false, STATUS_X, STATUS_Y);
////								checkDevicePIN(true);
//				          		memset(&staticBuffer[0], 0, sizeof(staticBuffer));
//								doAEMValidate(true);
//				//        		writeEink("stuck cDP false>true", false, STATUS_X, STATUS_Y);
//								break;
//							}
//					}
			 }
			 else
		 {
			staticBuffer[i] = theChar;
		 }

  } // end for 20
//  strncpy(finalBuffer, staticBuffer, i);
  return staticBuffer;
}
char *getInputVariable(bool displayInput, bool initialSetup, int numChars) {
  int i;
  char theChar;
  static char staticBuffer[21] = {};
  static char finalBuffer[21] = {};
  char tempBuffer[21] = {};
  int j;
  bool caps = false;

  memset(staticBuffer, 0, 21);
  strcpy(staticBuffer, "");

  for (i=0; i<20; i++)
  {
        #if defined(__SAM3X8E__)
        theChar = alphkeypad((8*i)+INPUT_X, INPUT_Y);
		#endif

		#if defined(__SAM3A8C__)
        if(displayInput)
			{
        	if(caps)
				{
					theChar = alphkeypad_3A8C_CAPS((8*i)+INPUT_X, INPUT_Y);
				}else
				{
					theChar = alphkeypad_3A8C((8*i)+INPUT_X, INPUT_Y);
				}
			}
			else
			{
				theChar = alphkeypad_3A8CnoDisplay((8*i)+INPUT_X, INPUT_Y);
			}
		#endif

        if(initialSetup)
        {
        	j = 3;
            if(theChar == '@' && i > j)
             {
             	break;
             }
             else if(theChar == '@' && i <= j)
             {
             	i = i - 1;
             }
             else if(theChar == '^')
             {
             	i = i - 1;
             	caps = !caps;
             	writeSelectedCharAndStringBlanking((8*i)+INPUT_X, INPUT_Y, caps);
             }
             else if(theChar == '~' && (i != 0))
             {
             	memset(&tempBuffer[0], 0, sizeof(tempBuffer));
              	strncpy(tempBuffer, staticBuffer, i-1);
              	memset(&staticBuffer[0], 0, sizeof(staticBuffer));
      	  		strncpy(staticBuffer, tempBuffer, i-1);
             	i= i - 2;
             	if(display){
             		writeSelectedCharAndStringBlanking((8*i)+INPUT_X, INPUT_Y, caps);
             	}
             }
             else if(theChar == '~' && (i == 0))
             {
             	i = i - 1;
             	if(!initialSetup)
             	{
     				if(displayInput){
     	//        		writeEink("before cDP true>false", false, STATUS_X, STATUS_Y);
     					checkDevicePIN(false);
     	//        		writeEink("stuck cDP true>false", false, STATUS_X, STATUS_Y);

     					break;
     				}else{
     	//        		writeEink("before cDP false>true", false, STATUS_X, STATUS_Y);
     					checkDevicePIN(true);
     	//        		writeEink("stuck cDP false>true", false, STATUS_X, STATUS_Y);
     					break;
     				}
             	}else if(initialSetup)
             	{
             		useWhatSetup();
             	}
             }
             else
             {
             	staticBuffer[i] = theChar;
             }
        }
        else
        {
            if(theChar == '@')
             {
             	break;
             }
             else if(theChar == '^')
             {
             	i = i - 1;
             	caps = !caps;
             	writeSelectedCharAndStringBlanking((8*i)+INPUT_X, INPUT_Y, caps);
             }
             else if(theChar == '~' && (i != 0))
             {
             	memset(&tempBuffer[0], 0, sizeof(tempBuffer));
              	strncpy(tempBuffer, staticBuffer, i-1);
              	memset(&staticBuffer[0], 0, sizeof(staticBuffer));
      	  		strncpy(staticBuffer, tempBuffer, i-1);
             	i= i - 2;
             	if(display){
             		writeSelectedCharAndStringBlanking((8*i)+INPUT_X, INPUT_Y, caps);
             	}
             }
             else if(theChar == '~' && (i == 0))
             {
             	i = i - 1;
             	if(!initialSetup)
             	{
     				if(displayInput){
     	//        		writeEink("before cDP true>false", false, STATUS_X, STATUS_Y);
     					checkDevicePIN(false);
     	//        		writeEink("stuck cDP true>false", false, STATUS_X, STATUS_Y);

     					break;
     				}else{
     	//        		writeEink("before cDP false>true", false, STATUS_X, STATUS_Y);
     					checkDevicePIN(true);
     	//        		writeEink("stuck cDP false>true", false, STATUS_X, STATUS_Y);
     					break;
     				}
             	}else if(initialSetup)
             	{
             		useWhatSetup();
              	}
             }
             else
             {
             	staticBuffer[i] = theChar;
             }
       }
  }
//  strncpy(finalBuffer, staticBuffer, i);
  return staticBuffer;
}


char *getInputWallets(bool displayInput, bool initialSetup) {
  int i;
  char theChar;
  static char staticBuffer[21] = {};
  static char finalBuffer[21] = {};
  char tempBuffer[21] = {};
  int j;
  bool caps = false;

  memset(staticBuffer, 0, 21);
  strcpy(staticBuffer, "");

  for (i=0; i<20; i++)
  {
        #if defined(__SAM3X8E__)
        theChar = alphkeypad((8*i)+INPUT_X, INPUT_Y);
		#endif

		#if defined(__SAM3A8C__)
        if(displayInput)
		{
			if(caps)
				{
					theChar = alphkeypad_3A8C_CAPS((8*i)+INPUT_X, INPUT_Y);
				}else
				{
					theChar = alphkeypad_3A8C((8*i)+INPUT_X, INPUT_Y);
				}
		}
		else
		{
			theChar = alphkeypad_3A8CnoDisplay((8*i)+INPUT_X, INPUT_Y);
		}
		#endif

        if(initialSetup)
        {
        	j = 3;
            if(theChar == '@' && i > j)
            {
            	break;
            }
            else if(theChar == '@' && i <= j)
            {
            	i = i - 1;
            }
            else if(theChar == '^')
            {
            	i = i - 1;
            	caps = !caps;
            	writeSelectedCharAndStringBlanking((8*i)+INPUT_X, INPUT_Y, caps);
            }
            else if(theChar == '~' && (i != 0))
            {
            	memset(&tempBuffer[0], 0, sizeof(tempBuffer));
             	strncpy(tempBuffer, staticBuffer, i-1);
             	memset(&staticBuffer[0], 0, sizeof(staticBuffer));
     	  		strncpy(staticBuffer, tempBuffer, i-1);
            	i= i - 2;
            	if(display){
            		writeSelectedCharAndStringBlanking((8*i)+INPUT_X, INPUT_Y, caps);
            	}
            }
            else if(theChar == '~' && (i == 0))
            {
           		passwordInterjectionAutoPIN(1);
            }
            else
            {
            	staticBuffer[i] = theChar;
            }
        }
        else
        {
            if(theChar == '@')
            {
            	return staticBuffer;
            	break;
            }
            else if(theChar == '^')
            {
            	i = i - 1;
            	caps = !caps;
            	writeSelectedCharAndStringBlanking((8*i)+INPUT_X, INPUT_Y, caps);
            }
            else if(theChar == '~' && (i != 0)  && displayInput)
            {
            	memset(&tempBuffer[0], 0, sizeof(tempBuffer));
             	strncpy(tempBuffer, staticBuffer, i-1);
             	memset(&staticBuffer[0], 0, sizeof(staticBuffer));
     	  		strncpy(staticBuffer, tempBuffer, i-1);
            	i= i - 2;
            	if(display){
            		writeSelectedCharAndStringBlanking((8*i)+INPUT_X, INPUT_Y, caps);
            	}
            }
            else if(theChar == '~' && (i == 0))
            {
            	i = i - 1;
				displayInput = !displayInput;
				if(displayInput)
				{
					strcpy(staticBuffer, "");
					buttonInterjectionNoAck(ASKUSER_ENTER_WALLET_PIN_ALPHA);
				}else{
					strcpy(staticBuffer, "");
					buttonInterjectionNoAck(ASKUSER_ENTER_WALLET_PIN);
				}
            }
            else
            {
            	staticBuffer[i] = theChar;
            }
        }

  }
//  strncpy(finalBuffer, staticBuffer, i);
  return staticBuffer;
}



void checkHashes(char *buffer4, bool displayAlpha)
{
	uint8_t k;
	HashState *sig_hash_hs_ptr3;
	HashState sig_hash_hs3;
	uint8_t hash3[32] = {};

	HashState *sig_hash_hs_ptrDuress;
	HashState sig_hash_hsDuress;
	uint8_t hashDuress[32] = {};

	uint8_t toBeMatched[32] = {};
	uint8_t matchCheckDuress;
	uint8_t matchCheck;
	uint8_t tempPIN[32];

	char duressarray[21]={};
	char *duress;

	duress = &duressarray[0];

	memset(duress, 0, 20);
	strcpy(duress, "911");

	sig_hash_hs_ptrDuress = &sig_hash_hsDuress;

	sha256Begin(sig_hash_hs_ptrDuress);

	for (k=0;k<20;k++)
	{
		sha256WriteByte(sig_hash_hs_ptrDuress, duress[k]);
	}
	sha256FinishDouble(sig_hash_hs_ptrDuress);
	writeHashToByteArray(hashDuress, sig_hash_hs_ptrDuress, false);

	sig_hash_hs_ptr3 = &sig_hash_hs3;

	sha256Begin(sig_hash_hs_ptr3);

	for (k=0;k<20;k++)
	{
		sha256WriteByte(sig_hash_hs_ptr3, buffer4[k]);
	}
	sha256FinishDouble(sig_hash_hs_ptr3);
	writeHashToByteArray(hash3, sig_hash_hs_ptr3, false);


	matchCheckDuress = memcmp(hash3,hashDuress,32);

	if(matchCheckDuress == 0){
//		writeEink("STOMP", false, STATUS_X, STATUS_Y);
		duressFormat();
		while(1){;;};
	}



	nonVolatileRead(tempPIN, PIN_ADDRESS, 32);

	int entryCount;
	int wrongTransactionPINCount;
	int entryDelay;
	int entryMultiplier;
	int entryMultiplierTRANS;
	int totalDelay;
	long totalDelaySeconds;
	int haltThreshold = 5;
	char intervalText[20];
	char *secondsText = "seconds";
	char *minutesText = "minutes";
	char *hoursText = "hours";
	char *daysText = "days";
	char *monthsText = "months";
	char *yearsText = "years";
	char *decadesText = "decades";
	char *centuriesText = "centuries";
	char *millenniaText = "millennia";
	long intervalValue;

	entryCount = fetchDevicePINWrongCount();
	wrongTransactionPINCount = fetchTransactionPINWrongCount();



	if(entryCount != 0 || wrongTransactionPINCount != 0)
	{
		entryMultiplier = pow(2, entryCount);
		if(wrongTransactionPINCount != 0)
		{
			wrongTransactionPINCount = wrongTransactionPINCount + 5;
			entryMultiplierTRANS = pow(3, wrongTransactionPINCount);
		}else{
			entryMultiplierTRANS = 0;
		}
		totalDelay = 1000 * (entryMultiplier + entryMultiplierTRANS);
		totalDelaySeconds = totalDelay/1000;
		char entryCountPrint[16];
		sprintf(entryCountPrint,"%lu", entryCount+1);



		if(totalDelaySeconds < 60)
		{
			strcpy(intervalText, secondsText);
			intervalValue = 1;
		}else if(totalDelaySeconds >= 60 &&  totalDelaySeconds < 3600)
		{
			strcpy(intervalText, minutesText);
			intervalValue = 60;
		}else if(totalDelaySeconds >= 3600 && totalDelaySeconds < 86400)
		{
			strcpy(intervalText, hoursText);
			intervalValue = 3600;
		}else if(totalDelaySeconds >= 86400 && totalDelaySeconds < 2592000)
		{
			strcpy(intervalText, daysText);
			intervalValue = 86400;
		}else if(totalDelaySeconds >= 2592000 && totalDelaySeconds < 31104000)
		{
			strcpy(intervalText, monthsText);
			intervalValue = 2592000;
		}else if(totalDelaySeconds >= 31104000 && totalDelaySeconds < 311040000)
		{
			strcpy(intervalText, yearsText);
			intervalValue = 31104000;
		}

		long resultingDelaySeconds;
		resultingDelaySeconds = totalDelaySeconds/intervalValue;
		char entryMultPrint[16];
		sprintf(entryMultPrint,"%lu", resultingDelaySeconds);


		initDisplay();
		overlayBatteryStatus(BATT_VALUE_DISPLAY);
		writeEinkNoDisplay("SECURITY DELAY",  COL_1_X, LINE_0_Y, entryCountPrint,COL_1_X,LINE_2_Y, "attempts",35,LINE_2_Y, entryMultPrint,COL_1_X,LINE_3_Y, intervalText,35,LINE_3_Y);
//		writeEinkNoDisplay("SECURITY DELAY",  COL_1_X, LINE_1_Y, "<<TEST DELAY 5 SEC>>",COL_1_X,LINE_2_Y, "",35,LINE_2_Y, entryMultPrint,COL_1_X,LINE_3_Y, intervalText,35,LINE_3_Y);
		writeUnderline(STRIPE_X_START, STRIPE_Y_START, STRIPE_X_END, STRIPE_Y_END);
		display();

		delay(totalDelay);
//		delay(5000);
	}

	matchCheck = memcmp(hash3,tempPIN,32);

	if(matchCheck != 0)
	{
		entryCount++;
		int s = entryCount;
		uint8_t temp1[1];
		temp1[0] = s;
		nonVolatileWrite(temp1, WRONG_DEVICE_PIN_COUNT_ADDRESS, 1);

		writeX_Screen();
		if(entryCount % 10 == 0){
			writeEink("HALT", false, STATUS_X, STATUS_Y);
			while(1){;;}
		}
		else
		{
			if(displayAlpha){
				pinStatusCheck();
			}else{
				pinStatusCheckandPremadePIN();
			}
		}
	}
	else if(matchCheck == 0)
	{
		writeCheck_Screen();
		if(entryCount != 0)
		{
			int s1 = 0;
			uint8_t temp11[1];
			temp11[0] = s1;
			nonVolatileWrite(temp11, WRONG_DEVICE_PIN_COUNT_ADDRESS, 1);
		}
	}
	else
	{
		writeEink("WTF", false, STATUS_X, STATUS_Y);
		while(1){;;};
	}
}


void writeSelectedCharAndString(char currentChar, int current_x, int current_y, bool caps)
{
	initDisplay();
//	writeEinkNoDisplaySingle("PRESS/HOLD/RELEASE",  5, 3);
//	writeUnderline(5, 29, 195, 29);

	int i;
	if(currentChar != '~'){
		writeUnderline(STRIPE_X_START, STRIPE_Y_START, STRIPE_X_END, STRIPE_Y_END);
		EPAPER.drawChar(currentChar, current_x, current_y);
	}

	for(i=0;i<((current_x - INPUT_X)/8);i++)
	{
		EPAPER.drawChar('-', (8*i)+INPUT_X, INPUT_Y);
	}
	if(caps)
	{
		drawCAPSLOCK(caps_lock_X,caps_lock_Y);
	}

	display();

}


void writeSelectedCharAndStringBlanking(int current_x, int current_y, bool caps)
{
	initDisplay();

	int i;

	for(i=0;i<(((current_x - INPUT_X)+8)/8);i++)
	{
		EPAPER.drawChar('-', (8*i)+INPUT_X, INPUT_Y);
	}

	if(i<4)
	{
		buttonInterjectionNoAckSetup(ASKUSER_DELETE_ONLY_EX_DISPLAY);
//		writeUnderline(STRIPE_X_START, STRIPE_Y_START, STRIPE_X_END, STRIPE_Y_END);
//		writeEinkNoDisplay("",  0, 0, "",0,0, "",0,0, "",22,80, "DELETE",132,80);
////		drawCheck(3,85);
//		drawX(180,87);
	}else
	{
		buttonInterjectionNoAckSetup(ASKUSER_ACCEPT_AND_DELETE_EX_DISPLAY);
//		writeUnderline(STRIPE_X_START, STRIPE_Y_START, STRIPE_X_END, STRIPE_Y_END);
//		writeEinkNoDisplay("",  0, 0, "",0,0, "",0,0, "ACCEPT",22,80, "DELETE",132,80);
//		drawCheck(3,85);
//		drawX(180,87);
	}

	if(caps)
	{
		drawCAPSLOCK(caps_lock_X,caps_lock_Y);
	}
	display();
}

void writeSelectedCharAndStringBlankingMnemonics(int current_x, int current_y, bool caps)
{
	initDisplay();

	int i;

	for(i=0;i<(((current_x - INPUT_X)+8)/8);i++)
	{
		EPAPER.drawChar('-', (8*i)+INPUT_X, INPUT_Y);
	}

	if(i<3)
	{
		buttonInterjectionNoAckSetup(ASKUSER_DELETE_ONLY_EX_DISPLAY);
//		writeUnderline(STRIPE_X_START, STRIPE_Y_START, STRIPE_X_END, STRIPE_Y_END);
//		writeEinkNoDisplay("",  0, 0, "",0,0, "",0,0, "",22,80, "DELETE",132,80);
////		drawCheck(3,85);
//		drawX(180,87);
	}else
	{
		buttonInterjectionNoAckSetup(ASKUSER_ACCEPT_AND_DELETE_EX_DISPLAY);
//		writeUnderline(STRIPE_X_START, STRIPE_Y_START, STRIPE_X_END, STRIPE_Y_END);
//		writeEinkNoDisplay("",  0, 0, "",0,0, "",0,0, "ACCEPT",22,80, "DELETE",132,80);
//		drawCheck(3,85);
//		drawX(180,87);
	}

	if(caps)
	{
		drawCAPSLOCK(caps_lock_X,caps_lock_Y);
	}
	display();
}


void writeSelectedCharAndStringBlankingPlus(int current_x, int current_y)
{
	initDisplay();
	writeEinkNoDisplaySingle("OK",  5, 3);
	writeUnderline(STRIPE_X_START, STRIPE_Y_START, STRIPE_X_END, STRIPE_Y_END);

	int i;

	for(i=0;i<(((current_x - INPUT_X)+8)/8)-1;i++)
	{
		EPAPER.drawChar('+', (8*i)+INPUT_X, INPUT_Y);
	}
//    overlayBatteryStatus();
//
//	writeEinkNoDisplay("PIN SETUP",  5, 3, "MANUALLY SET DEVICE PIN",5,25, "PRESS AND HOLD TO SELECT",5,40, "",5,55, "",25,80);
//	writeUnderline(STRIPE_X_START, STRIPE_Y_START, STRIPE_X_END, STRIPE_Y_END);
//			writeEinkNoDisplay("",  0, 0, "",0,0, "",0,0, "GO",22,80, "BACK",148,80);
//
//			drawCheck(3,85);
//			drawX(180,87);
//
	display();
}



#if defined(__SAM3A8C__)

char alphkeypad_3A8C(int current_x, int current_y)
{
	pinMode(ROW_PIN_3, INPUT_PULLUP);
	pinMode(ROW_PIN_2, INPUT_PULLUP);
	pinMode(ROW_PIN_1, INPUT_PULLUP);
	pinMode(COL_PIN_1, OUTPUT);
	pinMode(COL_PIN_2, OUTPUT);
	pinMode(COL_PIN_3, OUTPUT);
	pinMode(COL_PIN_4, OUTPUT);
	digitalWrite(COL_PIN_1, HIGH);
	digitalWrite(COL_PIN_2, HIGH);
	digitalWrite(COL_PIN_3, HIGH);
	digitalWrite(COL_PIN_4, HIGH);

	int recoverDelay = 1;
	int postDelay = 5;
	bool displayOrNot = true;
	char selectedChar;

	bool caps = false;

	char a = 'a';

  while (a != 'c') {




    //  @
    digitalWrite(COL_PIN_1, LOW); digitalWrite(COL_PIN_2, HIGH);
    digitalWrite(COL_PIN_3, HIGH); digitalWrite(COL_PIN_4, HIGH);
	delay(postDelay);
    while(digitalRead(ROW_PIN_3) == LOW){
		if (digitalRead(ROW_PIN_3) == LOW) {
		  selectedChar = hexaKeys[0][0][0];
//		  writeSelectedCharAndString(selectedChar, current_x, current_y, caps);       delay(keyDelay);
//		  if (digitalRead(ROW_PIN_3) == LOW) {
//			selectedChar = hexaKeys[1][0][0];
//			writeSelectedCharAndString(selectedChar, current_x, current_y, caps);       delay(keyDelay);
////			if (digitalRead(ROW_PIN_3) == LOW) {
////			  selectedChar = hexaKeys[2][0][0];
////			  writeSelectedCharAndString(selectedChar, current_x, current_y, caps);       delay(keyDelay);
//////			  if (digitalRead(ROW_PIN_3) == LOW) {
//////				selectedChar = hexaKeys[3][0][0];
//////				writeSelectedCharAndString(selectedChar, current_x, current_y, caps);       delay(keyDelay);
//////			  }
////			}
//		  }
		}
      a = 'c';
    }
	delay(recoverDelay);


    //  4
//    digitalWrite(COL_PIN_1, LOW); digitalWrite(COL_PIN_2, HIGH);
//    digitalWrite(COL_PIN_3, HIGH); digitalWrite(COL_PIN_4, HIGH);
    while(digitalRead(ROW_PIN_2) == LOW){
		if (digitalRead(ROW_PIN_2) == LOW) {
		  selectedChar = hexaKeys[0][1][0];
		  writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
		  if (digitalRead(ROW_PIN_2) == LOW) {
			selectedChar = hexaKeys[1][1][0];
			writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
			if (digitalRead(ROW_PIN_2) == LOW) {
			  selectedChar = hexaKeys[2][1][0];
			  writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
			  if (digitalRead(ROW_PIN_2) == LOW) {
				selectedChar = hexaKeys[3][1][0];
				writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
			  }
			}
		  }
		}
      a = 'c';
    }
	delay(recoverDelay);

    //   8 hexaKeys[0][2][0]
//    digitalWrite(COL_PIN_1, LOW); digitalWrite(COL_PIN_2, HIGH);
//    digitalWrite(COL_PIN_3, HIGH); digitalWrite(COL_PIN_4, HIGH);
    while(digitalRead(ROW_PIN_1) == LOW){
		if (digitalRead(ROW_PIN_1) == LOW) {
		  selectedChar = hexaKeys[0][2][0];
		  writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
		  if (digitalRead(ROW_PIN_1) == LOW) {
			selectedChar = hexaKeys[1][2][0];
			writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
			if (digitalRead(ROW_PIN_1) == LOW) {
			  selectedChar = hexaKeys[2][2][0];
			  writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
			  if (digitalRead(ROW_PIN_1) == LOW) {
				selectedChar = hexaKeys[3][2][0];
				writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
			  }
			}
		  }
		}
      a = 'c';
    }
	delay(recoverDelay);




    //   @ hexaKeys[0][0][1]
    digitalWrite(COL_PIN_1, HIGH); digitalWrite(COL_PIN_2, LOW);
    digitalWrite(COL_PIN_3, HIGH); digitalWrite(COL_PIN_4, HIGH);
	delay(postDelay);
    while(digitalRead(ROW_PIN_3) == LOW){
		if (digitalRead(ROW_PIN_3) == LOW) {
		  selectedChar = hexaKeys[0][0][1];
		  writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
		  if (digitalRead(ROW_PIN_3) == LOW) {
			selectedChar = hexaKeys[1][0][1];
			writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
			if (digitalRead(ROW_PIN_3) == LOW) {
			  selectedChar = hexaKeys[2][0][1];
			  writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
			  if (digitalRead(ROW_PIN_3) == LOW) {
				selectedChar = hexaKeys[3][0][1];
				writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
			  }
			}
		  }
		}
      a = 'c';
    }
	delay(recoverDelay);


    //     5  hexaKeys[0][1][1]
//    digitalWrite(COL_PIN_1, HIGH); digitalWrite(COL_PIN_2, LOW);
//    digitalWrite(COL_PIN_3, HIGH); digitalWrite(COL_PIN_4, HIGH);
    while(digitalRead(ROW_PIN_2) == LOW){
		if (digitalRead(ROW_PIN_2) == LOW) {
		  selectedChar = hexaKeys[0][1][1];
		  writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
		  if (digitalRead(ROW_PIN_2) == LOW) {
			selectedChar = hexaKeys[1][1][1];
			writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
			if (digitalRead(ROW_PIN_2) == LOW) {
			  selectedChar = hexaKeys[2][1][1];
			  writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
			  if (digitalRead(ROW_PIN_2) == LOW) {
				selectedChar = hexaKeys[3][1][1];
				writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
			  }
			}
		  }
		}
      a = 'c';
    }
	delay(recoverDelay);


    //     9  hexaKeys[0][2][1]
//    digitalWrite(COL_PIN_1, HIGH); digitalWrite(COL_PIN_2, LOW);
//    digitalWrite(COL_PIN_3, HIGH); digitalWrite(COL_PIN_4, HIGH);
    while(digitalRead(ROW_PIN_1) == LOW){
		if (digitalRead(ROW_PIN_1) == LOW) {
		  selectedChar = hexaKeys[0][2][1];
		  writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
		  if (digitalRead(ROW_PIN_1) == LOW) {
			selectedChar = hexaKeys[1][2][1];
			writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
			if (digitalRead(ROW_PIN_1) == LOW) {
			  selectedChar = hexaKeys[2][2][1];
			  writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
			  if (digitalRead(ROW_PIN_1) == LOW) {
				selectedChar = hexaKeys[3][2][1];
				writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
			  }
			}
		  }
		}
      a = 'c';
    }
	delay(recoverDelay);





    //      9  hexaKeys[0][0][2]
    digitalWrite(COL_PIN_1, HIGH); digitalWrite(COL_PIN_2, HIGH);
    digitalWrite(COL_PIN_3, LOW); digitalWrite(COL_PIN_4, HIGH);
	delay(postDelay);
    while(digitalRead(ROW_PIN_3) == LOW){
		if (digitalRead(ROW_PIN_3) == LOW) {
		  selectedChar = hexaKeys[0][0][2];
		  writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
		  if (digitalRead(ROW_PIN_3) == LOW) {
			selectedChar = hexaKeys[1][0][2];
			writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
//			if (digitalRead(ROW_PIN_3) == LOW) {
//			  selectedChar = hexaKeys[2][0][2];
//			  writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
//			  if (digitalRead(ROW_PIN_3) == LOW) {
//				selectedChar = hexaKeys[3][0][2];
//				writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
//			  }
//			}
		  }
		}
      a = 'c'   ;
    }
	delay(recoverDelay);



    //     6  hexaKeys[0][1][2]
//    digitalWrite(COL_PIN_1, HIGH); digitalWrite(COL_PIN_2, HIGH);
//    digitalWrite(COL_PIN_3, LOW); digitalWrite(COL_PIN_4, HIGH);
    while(digitalRead(ROW_PIN_2) == LOW){
		if (digitalRead(ROW_PIN_2) == LOW) {
		  selectedChar = hexaKeys[0][1][2];
		  writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
		  if (digitalRead(ROW_PIN_2) == LOW) {
			selectedChar = hexaKeys[1][1][2];
			writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
			if (digitalRead(ROW_PIN_2) == LOW) {
			  selectedChar = hexaKeys[2][1][2];
			  writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
			  if (digitalRead(ROW_PIN_2) == LOW) {
				selectedChar = hexaKeys[3][1][2];
				writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
			  }
			}
		  }
		}
      a = 'c';
    }
	delay(recoverDelay);


    //  Y=@  hexaKeys[0][2][2]
//    digitalWrite(COL_PIN_1, HIGH); digitalWrite(COL_PIN_2, HIGH);
//    digitalWrite(COL_PIN_3, LOW); digitalWrite(COL_PIN_4, HIGH);
	while(digitalRead(ROW_PIN_1) == LOW){
		if (digitalRead(ROW_PIN_1) == LOW) {
			selectedChar = hexaKeys[0][2][2];
			writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
			if (digitalRead(ROW_PIN_1) == LOW) {
				selectedChar = hexaKeys[1][2][2];
				writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
				if (digitalRead(ROW_PIN_1) == LOW) {
					selectedChar = hexaKeys[2][2][2];
					writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
					if (digitalRead(ROW_PIN_1) == LOW) {
						selectedChar = hexaKeys[3][2][2];
						writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
					}
				}
			}
		}
		a = 'c';
	}
	delay(recoverDelay);




    //  3  hexaKeys[0][0][3]
    digitalWrite(COL_PIN_1, HIGH); digitalWrite(COL_PIN_2, HIGH);
    digitalWrite(COL_PIN_3, HIGH); digitalWrite(COL_PIN_4, LOW);
	delay(postDelay);
    while(digitalRead(ROW_PIN_3) == LOW){
		if (digitalRead(ROW_PIN_3) == LOW) {
		  selectedChar = hexaKeys[0][0][3];
		  writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
		  if (digitalRead(ROW_PIN_3) == LOW) {
			selectedChar = hexaKeys[1][0][3];
			writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
			if (digitalRead(ROW_PIN_3) == LOW) {
			  selectedChar = hexaKeys[2][0][3];
			  writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
//			  if (digitalRead(ROW_PIN_3) == LOW) {
//				selectedChar = hexaKeys[3][0][3];
//				writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
//			  }
			}
		  }
		}
      a = 'c';
    }
	delay(recoverDelay);


    //  7  hexaKeys[0][1][3]
//    digitalWrite(COL_PIN_1, HIGH); digitalWrite(COL_PIN_2, HIGH);
//    digitalWrite(COL_PIN_3, HIGH); digitalWrite(COL_PIN_4, LOW);
    while(digitalRead(ROW_PIN_2) == LOW){
		if (digitalRead(ROW_PIN_2) == LOW) {
		  selectedChar = hexaKeys[0][1][3];
		  writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
		  if (digitalRead(ROW_PIN_2) == LOW) {
			selectedChar = hexaKeys[1][1][3];
			writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
			if (digitalRead(ROW_PIN_2) == LOW) {
			  selectedChar = hexaKeys[2][1][3];
			  writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
			  if (digitalRead(ROW_PIN_2) == LOW) {
				selectedChar = hexaKeys[3][1][3];
				writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
			  }
			}
		  }
		}
      a = 'c';
    }
	delay(recoverDelay);


	//  N  hexaKeys[0][2][3]
//    digitalWrite(COL_PIN_1, HIGH); digitalWrite(COL_PIN_2, HIGH);
//    digitalWrite(COL_PIN_3, HIGH); digitalWrite(COL_PIN_4, LOW);
    while(digitalRead(ROW_PIN_1) == LOW){
		if (digitalRead(ROW_PIN_1) == LOW) {
		  selectedChar = hexaKeys[0][2][3];
//		  writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
//		  if (digitalRead(ROW_PIN_1) == LOW) {
//			selectedChar = hexaKeys[1][2][3];
//			writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
//			if (digitalRead(ROW_PIN_1) == LOW) {
//			  selectedChar = hexaKeys[2][2][3];
//			  writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
//			  if (digitalRead(ROW_PIN_1) == LOW) {
//				selectedChar = hexaKeys[3][2][3];
//				writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
//			  }
//			}
//		  }
		}
      a = 'c';
    }
	delay(recoverDelay);




  }//WHILE ENDING
  if(selectedChar == '@')
  {
//	  writeSelectedCharAndStringBlankingPlus(current_x, current_y);
  }
  else if(selectedChar == '~')
  {
	  return selectedChar;
  }
  else if(selectedChar == '^')
  {
	  return selectedChar;
  }
  else
  {
	  writeSelectedCharAndStringBlanking(current_x, current_y, caps);
  }
  return selectedChar;

}//ALPHKEYPAD_3A8C  ENDING


char alphkeypad_noNumbers_3A8C(int current_x, int current_y)
{
	pinMode(ROW_PIN_3, INPUT_PULLUP);
	pinMode(ROW_PIN_2, INPUT_PULLUP);
	pinMode(ROW_PIN_1, INPUT_PULLUP);
	pinMode(COL_PIN_1, OUTPUT);
	pinMode(COL_PIN_2, OUTPUT);
	pinMode(COL_PIN_3, OUTPUT);
	pinMode(COL_PIN_4, OUTPUT);
	digitalWrite(COL_PIN_1, HIGH);
	digitalWrite(COL_PIN_2, HIGH);
	digitalWrite(COL_PIN_3, HIGH);
	digitalWrite(COL_PIN_4, HIGH);

	int recoverDelay = 1;
	int postDelay = 5;
	bool displayOrNot = true;
	char selectedChar;

	bool caps = false;

	char a = 'a';

  while (a != 'c') {




    //  @
    digitalWrite(COL_PIN_1, LOW); digitalWrite(COL_PIN_2, HIGH);
    digitalWrite(COL_PIN_3, HIGH); digitalWrite(COL_PIN_4, HIGH);
	delay(postDelay);
    while(digitalRead(ROW_PIN_3) == LOW){
		if (digitalRead(ROW_PIN_3) == LOW) {
		  selectedChar = hexaKeys[0][0][0];
//		  writeSelectedCharAndString(selectedChar, current_x, current_y, caps);       delay(keyDelay);
//		  if (digitalRead(ROW_PIN_3) == LOW) {
//			selectedChar = hexaKeys[1][0][0];
//			writeSelectedCharAndString(selectedChar, current_x, current_y, caps);       delay(keyDelay);
////			if (digitalRead(ROW_PIN_3) == LOW) {
////			  selectedChar = hexaKeys[2][0][0];
////			  writeSelectedCharAndString(selectedChar, current_x, current_y, caps);       delay(keyDelay);
//////			  if (digitalRead(ROW_PIN_3) == LOW) {
//////				selectedChar = hexaKeys[3][0][0];
//////				writeSelectedCharAndString(selectedChar, current_x, current_y, caps);       delay(keyDelay);
//////			  }
////			}
//		  }
		}
      a = 'c';
    }
	delay(recoverDelay);


    //  4
//    digitalWrite(COL_PIN_1, LOW); digitalWrite(COL_PIN_2, HIGH);
//    digitalWrite(COL_PIN_3, HIGH); digitalWrite(COL_PIN_4, HIGH);
    while(digitalRead(ROW_PIN_2) == LOW){
//		if (digitalRead(ROW_PIN_2) == LOW) {
//		  selectedChar = hexaKeys[0][1][0];
//		  writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
		  if (digitalRead(ROW_PIN_2) == LOW) {
			selectedChar = hexaKeys[1][1][0];
			writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
			if (digitalRead(ROW_PIN_2) == LOW) {
			  selectedChar = hexaKeys[2][1][0];
			  writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
			  if (digitalRead(ROW_PIN_2) == LOW) {
				selectedChar = hexaKeys[3][1][0];
				writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
			  }
			}
		  }
//		}
      a = 'c';
    }
	delay(recoverDelay);

    //   8 hexaKeys[0][2][0]
//    digitalWrite(COL_PIN_1, LOW); digitalWrite(COL_PIN_2, HIGH);
//    digitalWrite(COL_PIN_3, HIGH); digitalWrite(COL_PIN_4, HIGH);
    while(digitalRead(ROW_PIN_1) == LOW){
//		if (digitalRead(ROW_PIN_1) == LOW) {
//		  selectedChar = hexaKeys[0][2][0];
//		  writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
		  if (digitalRead(ROW_PIN_1) == LOW) {
			selectedChar = hexaKeys[1][2][0];
			writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
			if (digitalRead(ROW_PIN_1) == LOW) {
			  selectedChar = hexaKeys[2][2][0];
			  writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
			  if (digitalRead(ROW_PIN_1) == LOW) {
				selectedChar = hexaKeys[3][2][0];
				writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
			  }
			}
		  }
//		}
      a = 'c';
    }
	delay(recoverDelay);




    //   @ hexaKeys[0][0][1]
    digitalWrite(COL_PIN_1, HIGH); digitalWrite(COL_PIN_2, LOW);
    digitalWrite(COL_PIN_3, HIGH); digitalWrite(COL_PIN_4, HIGH);
	delay(postDelay);
    while(digitalRead(ROW_PIN_3) == LOW){
//		if (digitalRead(ROW_PIN_3) == LOW) {
//		  selectedChar = hexaKeys[0][0][1];
//		  writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
		  if (digitalRead(ROW_PIN_3) == LOW) {
			selectedChar = hexaKeys[1][0][1];
			writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
			if (digitalRead(ROW_PIN_3) == LOW) {
			  selectedChar = hexaKeys[2][0][1];
			  writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
			  if (digitalRead(ROW_PIN_3) == LOW) {
				selectedChar = hexaKeys[3][0][1];
				writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
			  }
			}
		  }
//		}
      a = 'c';
    }
	delay(recoverDelay);


    //     5  hexaKeys[0][1][1]
//    digitalWrite(COL_PIN_1, HIGH); digitalWrite(COL_PIN_2, LOW);
//    digitalWrite(COL_PIN_3, HIGH); digitalWrite(COL_PIN_4, HIGH);
    while(digitalRead(ROW_PIN_2) == LOW){
//		if (digitalRead(ROW_PIN_2) == LOW) {
//		  selectedChar = hexaKeys[0][1][1];
//		  writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
		  if (digitalRead(ROW_PIN_2) == LOW) {
			selectedChar = hexaKeys[1][1][1];
			writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
			if (digitalRead(ROW_PIN_2) == LOW) {
			  selectedChar = hexaKeys[2][1][1];
			  writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
			  if (digitalRead(ROW_PIN_2) == LOW) {
				selectedChar = hexaKeys[3][1][1];
				writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
			  }
			}
		  }
//		}
      a = 'c';
    }
	delay(recoverDelay);


    //     9  hexaKeys[0][2][1]
//    digitalWrite(COL_PIN_1, HIGH); digitalWrite(COL_PIN_2, LOW);
//    digitalWrite(COL_PIN_3, HIGH); digitalWrite(COL_PIN_4, HIGH);
    while(digitalRead(ROW_PIN_1) == LOW){
//		if (digitalRead(ROW_PIN_1) == LOW) {
//		  selectedChar = hexaKeys[0][2][1];
//		  writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
		  if (digitalRead(ROW_PIN_1) == LOW) {
			selectedChar = hexaKeys[1][2][1];
			writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
			if (digitalRead(ROW_PIN_1) == LOW) {
			  selectedChar = hexaKeys[2][2][1];
			  writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
			  if (digitalRead(ROW_PIN_1) == LOW) {
				selectedChar = hexaKeys[3][2][1];
				writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
			  }
			}
		  }
//		}
      a = 'c';
    }
	delay(recoverDelay);





    //      0  hexaKeys[0][0][2]
    digitalWrite(COL_PIN_1, HIGH); digitalWrite(COL_PIN_2, HIGH);
    digitalWrite(COL_PIN_3, LOW); digitalWrite(COL_PIN_4, HIGH);
	delay(postDelay);
    while(digitalRead(ROW_PIN_3) == LOW){
//		if (digitalRead(ROW_PIN_3) == LOW) {
//		  selectedChar = hexaKeys[0][0][2];
//		  writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
//		  if (digitalRead(ROW_PIN_3) == LOW) {
//			selectedChar = hexaKeys[1][0][2];
//			writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
//			if (digitalRead(ROW_PIN_3) == LOW) {
//			  selectedChar = hexaKeys[2][0][2];
//			  writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
//			  if (digitalRead(ROW_PIN_3) == LOW) {
//				selectedChar = hexaKeys[3][0][2];
//				writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
//			  }
//			}
//		  }
//		}
      a = 'c'   ;
    }
	delay(recoverDelay);



    //     6  hexaKeys[0][1][2]
//    digitalWrite(COL_PIN_1, HIGH); digitalWrite(COL_PIN_2, HIGH);
//    digitalWrite(COL_PIN_3, LOW); digitalWrite(COL_PIN_4, HIGH);
    while(digitalRead(ROW_PIN_2) == LOW){
//		if (digitalRead(ROW_PIN_2) == LOW) {
//		  selectedChar = hexaKeys[0][1][2];
//		  writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
		  if (digitalRead(ROW_PIN_2) == LOW) {
			selectedChar = hexaKeys[1][1][2];
			writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
			if (digitalRead(ROW_PIN_2) == LOW) {
			  selectedChar = hexaKeys[2][1][2];
			  writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
			  if (digitalRead(ROW_PIN_2) == LOW) {
				selectedChar = hexaKeys[3][1][2];
				writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
			  }
			}
		  }
//		}
      a = 'c';
    }
	delay(recoverDelay);


    //  Y=@  hexaKeys[0][2][2]
//    digitalWrite(COL_PIN_1, HIGH); digitalWrite(COL_PIN_2, HIGH);
//    digitalWrite(COL_PIN_3, LOW); digitalWrite(COL_PIN_4, HIGH);
	while(digitalRead(ROW_PIN_1) == LOW){
//		if (digitalRead(ROW_PIN_1) == LOW) {
//			selectedChar = hexaKeys[0][2][2];
//			writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
			if (digitalRead(ROW_PIN_1) == LOW) {
				selectedChar = hexaKeys[1][2][2];
				writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
				if (digitalRead(ROW_PIN_1) == LOW) {
					selectedChar = hexaKeys[2][2][2];
					writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
					if (digitalRead(ROW_PIN_1) == LOW) {
						selectedChar = hexaKeys[3][2][2];
						writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
					}
				}
//			}
		}
		a = 'c';
	}
	delay(recoverDelay);




    //  3  hexaKeys[0][0][3]
    digitalWrite(COL_PIN_1, HIGH); digitalWrite(COL_PIN_2, HIGH);
    digitalWrite(COL_PIN_3, HIGH); digitalWrite(COL_PIN_4, LOW);
	delay(postDelay);
    while(digitalRead(ROW_PIN_3) == LOW){
//		if (digitalRead(ROW_PIN_3) == LOW) {
//		  selectedChar = hexaKeys[0][0][3];
//		  writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
		  if (digitalRead(ROW_PIN_3) == LOW) {
			selectedChar = hexaKeys[1][0][3];
			writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
			if (digitalRead(ROW_PIN_3) == LOW) {
			  selectedChar = hexaKeys[2][0][3];
			  writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
//			  if (digitalRead(ROW_PIN_3) == LOW) {
//				selectedChar = hexaKeys[3][0][3];
//				writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
//			  }
			}
		  }
//		}
      a = 'c';
    }
	delay(recoverDelay);


    //  7  hexaKeys[0][1][3]
//    digitalWrite(COL_PIN_1, HIGH); digitalWrite(COL_PIN_2, HIGH);
//    digitalWrite(COL_PIN_3, HIGH); digitalWrite(COL_PIN_4, LOW);
    while(digitalRead(ROW_PIN_2) == LOW){
//		if (digitalRead(ROW_PIN_2) == LOW) {
//		  selectedChar = hexaKeys[0][1][3];
//		  writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
		  if (digitalRead(ROW_PIN_2) == LOW) {
			selectedChar = hexaKeys[1][1][3];
			writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
			if (digitalRead(ROW_PIN_2) == LOW) {
			  selectedChar = hexaKeys[2][1][3];
			  writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
			  if (digitalRead(ROW_PIN_2) == LOW) {
				selectedChar = hexaKeys[3][1][3];
				writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
			  }
			}
		  }
//		}
      a = 'c';
    }
	delay(recoverDelay);


	//  N  hexaKeys[0][2][3]
//    digitalWrite(COL_PIN_1, HIGH); digitalWrite(COL_PIN_2, HIGH);
//    digitalWrite(COL_PIN_3, HIGH); digitalWrite(COL_PIN_4, LOW);
    while(digitalRead(ROW_PIN_1) == LOW){
		if (digitalRead(ROW_PIN_1) == LOW) {
		  selectedChar = hexaKeys[0][2][3];
//		  writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
//		  if (digitalRead(ROW_PIN_1) == LOW) {
//			selectedChar = hexaKeys[1][2][3];
//			writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
//			if (digitalRead(ROW_PIN_1) == LOW) {
//			  selectedChar = hexaKeys[2][2][3];
//			  writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
//			  if (digitalRead(ROW_PIN_1) == LOW) {
//				selectedChar = hexaKeys[3][2][3];
//				writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
//			  }
//			}
//		  }
		}
      a = 'c';
    }
	delay(recoverDelay);




  }//WHILE ENDING
  if(selectedChar == '@')
  {
//	  writeSelectedCharAndStringBlankingPlus(current_x, current_y);
  }
  else if(selectedChar == '~')
  {
	  return selectedChar;
  }
  else if(selectedChar == '^')
  {
	  return selectedChar;
  }
  else
  {
	  writeSelectedCharAndStringBlanking(current_x, current_y, caps);
  }
  return selectedChar;

}//ALPHKEYPAD_3A8C_NO_NUMBERS  ENDING


char alphkeypad_3A8C_CAPS(int current_x, int current_y)
{
	pinMode(ROW_PIN_3, INPUT_PULLUP);
	pinMode(ROW_PIN_2, INPUT_PULLUP);
	pinMode(ROW_PIN_1, INPUT_PULLUP);
	pinMode(COL_PIN_1, OUTPUT);
	pinMode(COL_PIN_2, OUTPUT);
	pinMode(COL_PIN_3, OUTPUT);
	pinMode(COL_PIN_4, OUTPUT);
	digitalWrite(COL_PIN_1, HIGH);
	digitalWrite(COL_PIN_2, HIGH);
	digitalWrite(COL_PIN_3, HIGH);
	digitalWrite(COL_PIN_4, HIGH);

	int recoverDelay = 1;
	int postDelay = 5;
	bool displayOrNot = true;
	char selectedChar;

	bool caps = true;

	char a = 'a';

  while (a != 'c') {




    //  @
    digitalWrite(COL_PIN_1, LOW); digitalWrite(COL_PIN_2, HIGH);
    digitalWrite(COL_PIN_3, HIGH); digitalWrite(COL_PIN_4, HIGH);
	delay(postDelay);
    while(digitalRead(ROW_PIN_3) == LOW){
		if (digitalRead(ROW_PIN_3) == LOW) {
		  selectedChar = hexaKeysCAPS[0][0][0];
//		  writeSelectedCharAndString(selectedChar, current_x, current_y, caps);       delay(keyDelay);
//		  if (digitalRead(ROW_PIN_3) == LOW) {
//			selectedChar = hexaKeys[1][0][0];
//			writeSelectedCharAndString(selectedChar, current_x, current_y, caps);       delay(keyDelay);
////			if (digitalRead(ROW_PIN_3) == LOW) {
////			  selectedChar = hexaKeys[2][0][0];
////			  writeSelectedCharAndString(selectedChar, current_x, current_y, caps);       delay(keyDelay);
//////			  if (digitalRead(ROW_PIN_3) == LOW) {
//////				selectedChar = hexaKeys[3][0][0];
//////				writeSelectedCharAndString(selectedChar, current_x, current_y, caps);       delay(keyDelay);
//////			  }
////			}
//		  }
		}
      a = 'c';
    }
	delay(recoverDelay);


    //  4
//    digitalWrite(COL_PIN_1, LOW); digitalWrite(COL_PIN_2, HIGH);
//    digitalWrite(COL_PIN_3, HIGH); digitalWrite(COL_PIN_4, HIGH);
    while(digitalRead(ROW_PIN_2) == LOW){
		if (digitalRead(ROW_PIN_2) == LOW) {
		  selectedChar = hexaKeysCAPS[0][1][0];
		  writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
		  if (digitalRead(ROW_PIN_2) == LOW) {
			selectedChar = hexaKeysCAPS[1][1][0];
			writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
			if (digitalRead(ROW_PIN_2) == LOW) {
			  selectedChar = hexaKeysCAPS[2][1][0];
			  writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
			  if (digitalRead(ROW_PIN_2) == LOW) {
				selectedChar = hexaKeysCAPS[3][1][0];
				writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
			  }
			}
		  }
		}
      a = 'c';
    }
	delay(recoverDelay);

    //   8 hexaKeys[0][2][0]
//    digitalWrite(COL_PIN_1, LOW); digitalWrite(COL_PIN_2, HIGH);
//    digitalWrite(COL_PIN_3, HIGH); digitalWrite(COL_PIN_4, HIGH);
    while(digitalRead(ROW_PIN_1) == LOW){
		if (digitalRead(ROW_PIN_1) == LOW) {
		  selectedChar = hexaKeysCAPS[0][2][0];
		  writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
		  if (digitalRead(ROW_PIN_1) == LOW) {
			selectedChar = hexaKeysCAPS[1][2][0];
			writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
			if (digitalRead(ROW_PIN_1) == LOW) {
			  selectedChar = hexaKeysCAPS[2][2][0];
			  writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
			  if (digitalRead(ROW_PIN_1) == LOW) {
				selectedChar = hexaKeysCAPS[3][2][0];
				writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
			  }
			}
		  }
		}
      a = 'c';
    }
	delay(recoverDelay);




    //   @ hexaKeys[0][0][1]
    digitalWrite(COL_PIN_1, HIGH); digitalWrite(COL_PIN_2, LOW);
    digitalWrite(COL_PIN_3, HIGH); digitalWrite(COL_PIN_4, HIGH);
	delay(postDelay);
    while(digitalRead(ROW_PIN_3) == LOW){
		if (digitalRead(ROW_PIN_3) == LOW) {
		  selectedChar = hexaKeysCAPS[0][0][1];
		  writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
		  if (digitalRead(ROW_PIN_3) == LOW) {
			selectedChar = hexaKeysCAPS[1][0][1];
			writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
			if (digitalRead(ROW_PIN_3) == LOW) {
			  selectedChar = hexaKeysCAPS[2][0][1];
			  writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
			  if (digitalRead(ROW_PIN_3) == LOW) {
				selectedChar = hexaKeysCAPS[3][0][1];
				writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
			  }
			}
		  }
		}
      a = 'c';
    }
	delay(recoverDelay);


    //     5  hexaKeys[0][1][1]
//    digitalWrite(COL_PIN_1, HIGH); digitalWrite(COL_PIN_2, LOW);
//    digitalWrite(COL_PIN_3, HIGH); digitalWrite(COL_PIN_4, HIGH);
    while(digitalRead(ROW_PIN_2) == LOW){
		if (digitalRead(ROW_PIN_2) == LOW) {
		  selectedChar = hexaKeysCAPS[0][1][1];
		  writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
		  if (digitalRead(ROW_PIN_2) == LOW) {
			selectedChar = hexaKeysCAPS[1][1][1];
			writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
			if (digitalRead(ROW_PIN_2) == LOW) {
			  selectedChar = hexaKeysCAPS[2][1][1];
			  writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
			  if (digitalRead(ROW_PIN_2) == LOW) {
				selectedChar = hexaKeysCAPS[3][1][1];
				writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
			  }
			}
		  }
		}
      a = 'c';
    }
	delay(recoverDelay);


    //     9  hexaKeys[0][2][1]
//    digitalWrite(COL_PIN_1, HIGH); digitalWrite(COL_PIN_2, LOW);
//    digitalWrite(COL_PIN_3, HIGH); digitalWrite(COL_PIN_4, HIGH);
    while(digitalRead(ROW_PIN_1) == LOW){
		if (digitalRead(ROW_PIN_1) == LOW) {
		  selectedChar = hexaKeysCAPS[0][2][1];
		  writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
		  if (digitalRead(ROW_PIN_1) == LOW) {
			selectedChar = hexaKeysCAPS[1][2][1];
			writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
			if (digitalRead(ROW_PIN_1) == LOW) {
			  selectedChar = hexaKeysCAPS[2][2][1];
			  writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
			  if (digitalRead(ROW_PIN_1) == LOW) {
				selectedChar = hexaKeysCAPS[3][2][1];
				writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
			  }
			}
		  }
		}
      a = 'c';
    }
	delay(recoverDelay);





    //      9  hexaKeys[0][0][2]
    digitalWrite(COL_PIN_1, HIGH); digitalWrite(COL_PIN_2, HIGH);
    digitalWrite(COL_PIN_3, LOW); digitalWrite(COL_PIN_4, HIGH);
	delay(postDelay);
    while(digitalRead(ROW_PIN_3) == LOW){
		if (digitalRead(ROW_PIN_3) == LOW) {
		  selectedChar = hexaKeysCAPS[0][0][2];
		  writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
		  if (digitalRead(ROW_PIN_3) == LOW) {
			selectedChar = hexaKeysCAPS[1][0][2];
			writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
			if (digitalRead(ROW_PIN_3) == LOW) {
			  selectedChar = hexaKeysCAPS[2][0][2];
			  writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
			  if (digitalRead(ROW_PIN_3) == LOW) {
				selectedChar = hexaKeysCAPS[3][0][2];
				writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
			  }
			}
		  }
		}
      a = 'c'   ;
    }
	delay(recoverDelay);



    //     6  hexaKeys[0][1][2]
//    digitalWrite(COL_PIN_1, HIGH); digitalWrite(COL_PIN_2, HIGH);
//    digitalWrite(COL_PIN_3, LOW); digitalWrite(COL_PIN_4, HIGH);
    while(digitalRead(ROW_PIN_2) == LOW){
		if (digitalRead(ROW_PIN_2) == LOW) {
		  selectedChar = hexaKeysCAPS[0][1][2];
		  writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
		  if (digitalRead(ROW_PIN_2) == LOW) {
			selectedChar = hexaKeysCAPS[1][1][2];
			writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
			if (digitalRead(ROW_PIN_2) == LOW) {
			  selectedChar = hexaKeysCAPS[2][1][2];
			  writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
			  if (digitalRead(ROW_PIN_2) == LOW) {
				selectedChar = hexaKeysCAPS[3][1][2];
				writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
			  }
			}
		  }
		}
      a = 'c';
    }
	delay(recoverDelay);


    //  Y=@  hexaKeys[0][2][2]
//    digitalWrite(COL_PIN_1, HIGH); digitalWrite(COL_PIN_2, HIGH);
//    digitalWrite(COL_PIN_3, LOW); digitalWrite(COL_PIN_4, HIGH);
	while(digitalRead(ROW_PIN_1) == LOW){
		if (digitalRead(ROW_PIN_1) == LOW) {
			selectedChar = hexaKeysCAPS[0][2][2];
			writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
			if (digitalRead(ROW_PIN_1) == LOW) {
				selectedChar = hexaKeysCAPS[1][2][2];
				writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
				if (digitalRead(ROW_PIN_1) == LOW) {
					selectedChar = hexaKeysCAPS[2][2][2];
					writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
					if (digitalRead(ROW_PIN_1) == LOW) {
						selectedChar = hexaKeysCAPS[3][2][2];
						writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
					}
				}
			}
		}
		a = 'c';
	}
	delay(recoverDelay);




    //  3  hexaKeys[0][0][3]
    digitalWrite(COL_PIN_1, HIGH); digitalWrite(COL_PIN_2, HIGH);
    digitalWrite(COL_PIN_3, HIGH); digitalWrite(COL_PIN_4, LOW);
	delay(postDelay);
    while(digitalRead(ROW_PIN_3) == LOW){
		if (digitalRead(ROW_PIN_3) == LOW) {
		  selectedChar = hexaKeysCAPS[0][0][3];
		  writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
		  if (digitalRead(ROW_PIN_3) == LOW) {
			selectedChar = hexaKeysCAPS[1][0][3];
			writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
			if (digitalRead(ROW_PIN_3) == LOW) {
			  selectedChar = hexaKeysCAPS[2][0][3];
			  writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
//			  if (digitalRead(ROW_PIN_3) == LOW) {
//				selectedChar = hexaKeysCAPS[3][0][3];
//				writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
//			  }
			}
		  }
		}
      a = 'c';
    }
	delay(recoverDelay);


    //  7  hexaKeys[0][1][3]
//    digitalWrite(COL_PIN_1, HIGH); digitalWrite(COL_PIN_2, HIGH);
//    digitalWrite(COL_PIN_3, HIGH); digitalWrite(COL_PIN_4, LOW);
    while(digitalRead(ROW_PIN_2) == LOW){
		if (digitalRead(ROW_PIN_2) == LOW) {
		  selectedChar = hexaKeysCAPS[0][1][3];
		  writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
		  if (digitalRead(ROW_PIN_2) == LOW) {
			selectedChar = hexaKeysCAPS[1][1][3];
			writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
			if (digitalRead(ROW_PIN_2) == LOW) {
			  selectedChar = hexaKeysCAPS[2][1][3];
			  writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
			  if (digitalRead(ROW_PIN_2) == LOW) {
				selectedChar = hexaKeysCAPS[3][1][3];
				writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
			  }
			}
		  }
		}
      a = 'c';
    }
	delay(recoverDelay);


	//  N  hexaKeys[0][2][3]
//    digitalWrite(COL_PIN_1, HIGH); digitalWrite(COL_PIN_2, HIGH);
//    digitalWrite(COL_PIN_3, HIGH); digitalWrite(COL_PIN_4, LOW);
    while(digitalRead(ROW_PIN_1) == LOW){
		if (digitalRead(ROW_PIN_1) == LOW) {
		  selectedChar = hexaKeysCAPS[0][2][3];
//		  writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
//		  if (digitalRead(ROW_PIN_1) == LOW) {
//			selectedChar = hexaKeysCAPS[1][2][3];
//			writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
//			if (digitalRead(ROW_PIN_1) == LOW) {
//			  selectedChar = hexaKeysCAPS[2][2][3];
//			  writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
//			  if (digitalRead(ROW_PIN_1) == LOW) {
//				selectedChar = hexaKeysCAPS[3][2][3];
//				writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
//			  }
//			}
//		  }
		}
      a = 'c';
    }
	delay(recoverDelay);




  }//WHILE ENDING
  if(selectedChar == '@')
  {
//	  writeSelectedCharAndStringBlankingPlus(current_x, current_y);
  }
  else if(selectedChar == '~')
  {
	  return selectedChar;
  }
  else
  {
	  writeSelectedCharAndStringBlanking(current_x, current_y, caps);
  }
  return selectedChar;

}//ALPHKEYPAD_3A8C_CAPS  ENDING

char alphkeypad_3A8CnoDisplay(int current_x, int current_y)
{
	pinMode(ROW_PIN_3, INPUT_PULLUP);
	pinMode(ROW_PIN_2, INPUT_PULLUP);
	pinMode(ROW_PIN_1, INPUT_PULLUP);
	pinMode(COL_PIN_1, OUTPUT);
	pinMode(COL_PIN_2, OUTPUT);
	pinMode(COL_PIN_3, OUTPUT);
	pinMode(COL_PIN_4, OUTPUT);
	digitalWrite(COL_PIN_1, HIGH);
	digitalWrite(COL_PIN_2, HIGH);
	digitalWrite(COL_PIN_3, HIGH);
	digitalWrite(COL_PIN_4, HIGH);

	int recoverDelay = 1;
	int postDelay = 5;
	bool displayOrNot = true;
	char selectedChar;
	bool caps = false;

	char a = 'a';

  while (a != 'c') {




    //  ~
    digitalWrite(COL_PIN_1, LOW); digitalWrite(COL_PIN_2, HIGH);
    digitalWrite(COL_PIN_3, HIGH); digitalWrite(COL_PIN_4, HIGH);
	delay(postDelay);
    while(digitalRead(ROW_PIN_3) == LOW){
		if (digitalRead(ROW_PIN_3) == LOW) {
		  selectedChar = hexaKeys[0][0][0];
//		  writeSelectedCharAndString(selectedChar, current_x, current_y, caps);       delay(keyDelay);
//		  if (digitalRead(ROW_PIN_3) == LOW) {
//			selectedChar = hexaKeys[1][0][0];
//			writeSelectedCharAndString(selectedChar, current_x, current_y, caps);       delay(keyDelay);
//			if (digitalRead(ROW_PIN_3) == LOW) {
//			  selectedChar = hexaKeys[2][0][0];
//			  writeSelectedCharAndString(selectedChar, current_x, current_y, caps);       delay(keyDelay);
//			  if (digitalRead(ROW_PIN_3) == LOW) {
//				selectedChar = hexaKeys[3][0][0];
//				writeSelectedCharAndString(selectedChar, current_x, current_y, caps);       delay(keyDelay);
//			  }
//			}
//		  }
		}
      a = 'c';
    }
	delay(recoverDelay);


    //  4
//    digitalWrite(COL_PIN_1, LOW); digitalWrite(COL_PIN_2, HIGH);
//    digitalWrite(COL_PIN_3, HIGH); digitalWrite(COL_PIN_4, HIGH);
    while(digitalRead(ROW_PIN_2) == LOW){
		if (digitalRead(ROW_PIN_2) == LOW) {
		  selectedChar = hexaKeys[0][1][0];
//		  writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
//		  if (digitalRead(ROW_PIN_2) == LOW) {
//			selectedChar = hexaKeys[1][1][0];
//			writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
//			if (digitalRead(ROW_PIN_2) == LOW) {
//			  selectedChar = hexaKeys[2][1][0];
//			  writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
//			  if (digitalRead(ROW_PIN_2) == LOW) {
//				selectedChar = hexaKeys[3][1][0];
//				writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
//			  }
//			}
//		  }
		}
      a = 'c';
    }
	delay(recoverDelay);

    //   8 hexaKeys[0][2][0]
//    digitalWrite(COL_PIN_1, LOW); digitalWrite(COL_PIN_2, HIGH);
//    digitalWrite(COL_PIN_3, HIGH); digitalWrite(COL_PIN_4, HIGH);
    while(digitalRead(ROW_PIN_1) == LOW){
		if (digitalRead(ROW_PIN_1) == LOW) {
		  selectedChar = hexaKeys[0][2][0];
//		  writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
//		  if (digitalRead(ROW_PIN_1) == LOW) {
//			selectedChar = hexaKeys[1][2][0];
//			writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
//			if (digitalRead(ROW_PIN_1) == LOW) {
//			  selectedChar = hexaKeys[2][2][0];
//			  writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
//			  if (digitalRead(ROW_PIN_1) == LOW) {
//				selectedChar = hexaKeys[3][2][0];
//				writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
//			  }
//			}
//		  }
		}
      a = 'c';
    }
	delay(recoverDelay);




    //   @ hexaKeys[0][0][1]
    digitalWrite(COL_PIN_1, HIGH); digitalWrite(COL_PIN_2, LOW);
    digitalWrite(COL_PIN_3, HIGH); digitalWrite(COL_PIN_4, HIGH);
	delay(postDelay);
    while(digitalRead(ROW_PIN_3) == LOW){
		if (digitalRead(ROW_PIN_3) == LOW) {
		  selectedChar = hexaKeys[0][0][1];
//		  writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
//		  if (digitalRead(ROW_PIN_3) == LOW) {
//			selectedChar = hexaKeys[1][0][1];
//			writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
//			if (digitalRead(ROW_PIN_3) == LOW) {
//			  selectedChar = hexaKeys[2][0][1];
//			  writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
//			  if (digitalRead(ROW_PIN_3) == LOW) {
//				selectedChar = hexaKeys[3][0][1];
//				writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
//			  }
//			}
//		  }
		}
      a = 'c';
    }
	delay(recoverDelay);


    //     5  hexaKeys[0][1][1]
//    digitalWrite(COL_PIN_1, HIGH); digitalWrite(COL_PIN_2, LOW);
//    digitalWrite(COL_PIN_3, HIGH); digitalWrite(COL_PIN_4, HIGH);
    while(digitalRead(ROW_PIN_2) == LOW){
		if (digitalRead(ROW_PIN_2) == LOW) {
		  selectedChar = hexaKeys[0][1][1];
//		  writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
//		  if (digitalRead(ROW_PIN_2) == LOW) {
//			selectedChar = hexaKeys[1][1][1];
//			writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
//			if (digitalRead(ROW_PIN_2) == LOW) {
//			  selectedChar = hexaKeys[2][1][1];
//			  writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
//			  if (digitalRead(ROW_PIN_2) == LOW) {
//				selectedChar = hexaKeys[3][1][1];
//				writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
//			  }
//			}
//		  }
		}
      a = 'c';
    }
	delay(recoverDelay);


    //     9  hexaKeys[0][2][1]
//    digitalWrite(COL_PIN_1, HIGH); digitalWrite(COL_PIN_2, LOW);
//    digitalWrite(COL_PIN_3, HIGH); digitalWrite(COL_PIN_4, HIGH);
    while(digitalRead(ROW_PIN_1) == LOW){
		if (digitalRead(ROW_PIN_1) == LOW) {
		  selectedChar = hexaKeys[0][2][1];
//		  writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
//		  if (digitalRead(ROW_PIN_1) == LOW) {
//			selectedChar = hexaKeys[1][2][1];
//			writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
//			if (digitalRead(ROW_PIN_1) == LOW) {
//			  selectedChar = hexaKeys[2][2][1];
//			  writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
//			  if (digitalRead(ROW_PIN_1) == LOW) {
//				selectedChar = hexaKeys[3][2][1];
//				writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
//			  }
//			}
//		  }
		}
      a = 'c';
    }
	delay(recoverDelay);





    //      9  hexaKeys[0][0][2]
    digitalWrite(COL_PIN_1, HIGH); digitalWrite(COL_PIN_2, HIGH);
    digitalWrite(COL_PIN_3, LOW); digitalWrite(COL_PIN_4, HIGH);
	delay(postDelay);
    while(digitalRead(ROW_PIN_3) == LOW){
		if (digitalRead(ROW_PIN_3) == LOW) {
		  selectedChar = hexaKeys[0][0][2];
//		  writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
//		  if (digitalRead(ROW_PIN_3) == LOW) {
//			selectedChar = hexaKeys[1][0][2];
//			writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
//			if (digitalRead(ROW_PIN_3) == LOW) {
//			  selectedChar = hexaKeys[2][0][2];
//			  writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
//			  if (digitalRead(ROW_PIN_3) == LOW) {
//				selectedChar = hexaKeys[3][0][2];
//				writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
//			  }
//			}
//		  }
		}
      a = 'c'   ;
    }
	delay(recoverDelay);



    //     6  hexaKeys[0][1][2]
//    digitalWrite(COL_PIN_1, HIGH); digitalWrite(COL_PIN_2, HIGH);
//    digitalWrite(COL_PIN_3, LOW); digitalWrite(COL_PIN_4, HIGH);
    while(digitalRead(ROW_PIN_2) == LOW){
		if (digitalRead(ROW_PIN_2) == LOW) {
		  selectedChar = hexaKeys[0][1][2];
//		  writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
//		  if (digitalRead(ROW_PIN_2) == LOW) {
//			selectedChar = hexaKeys[1][1][2];
//			writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
//			if (digitalRead(ROW_PIN_2) == LOW) {
//			  selectedChar = hexaKeys[2][1][2];
//			  writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
//			  if (digitalRead(ROW_PIN_2) == LOW) {
//				selectedChar = hexaKeys[3][1][2];
//				writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
//			  }
//			}
//		  }
		}
      a = 'c';
    }
	delay(recoverDelay);


    //  Y=@  hexaKeys[0][2][2]
//    digitalWrite(COL_PIN_1, HIGH); digitalWrite(COL_PIN_2, HIGH);
//    digitalWrite(COL_PIN_3, LOW); digitalWrite(COL_PIN_4, HIGH);
	while(digitalRead(ROW_PIN_1) == LOW){
		if (digitalRead(ROW_PIN_1) == LOW) {
			selectedChar = hexaKeys[0][2][2];
//			writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
//			if (digitalRead(ROW_PIN_1) == LOW) {
//				selectedChar = hexaKeys[1][2][2];
//				writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
//				if (digitalRead(ROW_PIN_1) == LOW) {
//					selectedChar = hexaKeys[2][2][2];
//					writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
//					if (digitalRead(ROW_PIN_1) == LOW) {
//						selectedChar = hexaKeys[3][2][2];
//						writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
//					}
//				}
//			}
		}
		a = 'c';
	}
	delay(recoverDelay);




    //  3  hexaKeys[0][0][3]
    digitalWrite(COL_PIN_1, HIGH); digitalWrite(COL_PIN_2, HIGH);
    digitalWrite(COL_PIN_3, HIGH); digitalWrite(COL_PIN_4, LOW);
	delay(postDelay);
    while(digitalRead(ROW_PIN_3) == LOW){
		if (digitalRead(ROW_PIN_3) == LOW) {
		  selectedChar = hexaKeys[0][0][3];
//		  writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
//		  if (digitalRead(ROW_PIN_3) == LOW) {
//			selectedChar = hexaKeys[1][0][3];
//			writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
//			if (digitalRead(ROW_PIN_3) == LOW) {
//			  selectedChar = hexaKeys[2][0][3];
//			  writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
//			  if (digitalRead(ROW_PIN_3) == LOW) {
//				selectedChar = hexaKeys[3][0][3];
//				writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
//			  }
//			}
//		  }
		}
      a = 'c';
    }
	delay(recoverDelay);


    //  7  hexaKeys[0][1][3]
//    digitalWrite(COL_PIN_1, HIGH); digitalWrite(COL_PIN_2, HIGH);
//    digitalWrite(COL_PIN_3, HIGH); digitalWrite(COL_PIN_4, LOW);
    while(digitalRead(ROW_PIN_2) == LOW){
		if (digitalRead(ROW_PIN_2) == LOW) {
		  selectedChar = hexaKeys[0][1][3];
//		  writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
//		  if (digitalRead(ROW_PIN_2) == LOW) {
//			selectedChar = hexaKeys[1][1][3];
//			writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
//			if (digitalRead(ROW_PIN_2) == LOW) {
//			  selectedChar = hexaKeys[2][1][3];
//			  writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
//			  if (digitalRead(ROW_PIN_2) == LOW) {
//				selectedChar = hexaKeys[3][1][3];
//				writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
//			  }
//			}
//		  }
		}
      a = 'c';
    }
	delay(recoverDelay);


	//  N  hexaKeys[0][2][3]
//    digitalWrite(COL_PIN_1, HIGH); digitalWrite(COL_PIN_2, HIGH);
//    digitalWrite(COL_PIN_3, HIGH); digitalWrite(COL_PIN_4, LOW);
    while(digitalRead(ROW_PIN_1) == LOW){
		if (digitalRead(ROW_PIN_1) == LOW) {
		  selectedChar = hexaKeys[0][2][3];
//		  writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
//		  if (digitalRead(ROW_PIN_1) == LOW) {
//			selectedChar = hexaKeys[1][2][3];
//			writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
//			if (digitalRead(ROW_PIN_1) == LOW) {
//			  selectedChar = hexaKeys[2][2][3];
//			  writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
//			  if (digitalRead(ROW_PIN_1) == LOW) {
//				selectedChar = hexaKeys[3][2][3];
//				writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
//			  }
//			}
//		  }
		}
      a = 'c';
    }
	delay(recoverDelay);




  }//WHILE ENDING
  if(selectedChar == '@')
  {
//	  writeSelectedCharAndStringBlankingPlus(current_x, current_y);
  }
  else if(selectedChar == '~')
  {
	  return selectedChar;
  }
  else
  {
//	  writeSelectedCharAndStringBlanking(current_x, current_y);
  }
  return selectedChar;

}//ALPHKEYPAD_3A8CNoDisplay  ENDING
#endif

/*
#if defined(__SAM3X8E__)
char alphkeypad(int current_x, int current_y)
{
  pinMode(r1, OUTPUT);
  pinMode(r2, OUTPUT);
  pinMode(r3, OUTPUT);
  pinMode(r4, OUTPUT);
  pinMode(c1, INPUT);
  pinMode(c2, INPUT);
  pinMode(c3, INPUT);

	bool displayOrNot = true;
	char selectedChar;

	char a = 'a';

  while (a != 'c') {






    digitalWrite(c1, HIGH);  digitalWrite(c2, HIGH); digitalWrite(c3, HIGH);

    //  0
    digitalWrite(r1, LOW); digitalWrite(r2, HIGH);
    digitalWrite(r3, HIGH); digitalWrite(r4, HIGH);
    while(digitalRead(c1) == LOW){
		if (digitalRead(c1) == LOW) {
		  selectedChar = hexaKeys[0][0][0];
		  writeSelectedCharAndString(selectedChar, current_x, current_y, caps);       delay(keyDelay);
		  if (digitalRead(c1) == LOW) {
			selectedChar = hexaKeys[1][0][0];
			writeSelectedCharAndString(selectedChar, current_x, current_y, caps);       delay(keyDelay);
			if (digitalRead(c1) == LOW) {
			  selectedChar = hexaKeys[2][0][0];
			  writeSelectedCharAndString(selectedChar, current_x, current_y, caps);       delay(keyDelay);
			  if (digitalRead(c1) == LOW) {
				selectedChar = hexaKeys[3][0][0];
				writeSelectedCharAndString(selectedChar, current_x, current_y, caps);       delay(keyDelay);
			  }
			}
		  }
		}
      a = 'c';
    }


    //  4
    digitalWrite(r1, LOW); digitalWrite(r2, HIGH);
    digitalWrite(r3, HIGH); digitalWrite(r4, HIGH);
    while(digitalRead(c2) == LOW){
		if (digitalRead(c2) == LOW) {
		  selectedChar = hexaKeys[0][1][0];
		  writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
		  if (digitalRead(c2) == LOW) {
			selectedChar = hexaKeys[1][1][0];
			writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
			if (digitalRead(c2) == LOW) {
			  selectedChar = hexaKeys[2][1][0];
			  writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
			  if (digitalRead(c2) == LOW) {
				selectedChar = hexaKeys[3][1][0];
				writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
			  }
			}
		  }
		}
      a = 'c';

    }

    //   8 hexaKeys[0][2][0]
    digitalWrite(r1, LOW); digitalWrite(r2, HIGH);
    digitalWrite(r3, HIGH); digitalWrite(r4, HIGH);
    while(digitalRead(c3) == LOW){
		if (digitalRead(c3) == LOW) {
		  selectedChar = hexaKeys[0][2][0];
		  writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
		  if (digitalRead(c3) == LOW) {
			selectedChar = hexaKeys[1][2][0];
			writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
			if (digitalRead(c3) == LOW) {
			  selectedChar = hexaKeys[2][2][0];
			  writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
			  if (digitalRead(c3) == LOW) {
				selectedChar = hexaKeys[3][2][0];
				writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
			  }
			}
		  }
		}
      a = 'c';
    }










    //   1 hexaKeys[0][0][1]
    digitalWrite(r1, HIGH); digitalWrite(r2, LOW);
    digitalWrite(r3, HIGH); digitalWrite(r4, HIGH);
    while(digitalRead(c1) == LOW){
		if (digitalRead(c1) == LOW) {
		  selectedChar = hexaKeys[0][0][1];
		  writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
		  if (digitalRead(c1) == LOW) {
			selectedChar = hexaKeys[1][0][1];
			writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
			if (digitalRead(c1) == LOW) {
			  selectedChar = hexaKeys[2][0][1];
			  writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
			  if (digitalRead(c1) == LOW) {
				selectedChar = hexaKeys[3][0][1];
				writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
			  }
			}
		  }
		}
      a = 'c';
    }

    //     5  hexaKeys[0][1][1]
    digitalWrite(r1, HIGH); digitalWrite(r2, LOW);
    digitalWrite(r3, HIGH); digitalWrite(r4, HIGH);
    while(digitalRead(c2) == LOW){
		if (digitalRead(c2) == LOW) {
		  selectedChar = hexaKeys[0][1][1];
		  writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
		  if (digitalRead(c2) == LOW) {
			selectedChar = hexaKeys[1][1][1];
			writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
			if (digitalRead(c2) == LOW) {
			  selectedChar = hexaKeys[2][1][1];
			  writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
			  if (digitalRead(c2) == LOW) {
				selectedChar = hexaKeys[3][1][1];
				writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
			  }
			}
		  }
		}
      a = 'c';
    }


    //     9  hexaKeys[0][2][1]
    digitalWrite(r1, HIGH); digitalWrite(r2, LOW);
    digitalWrite(r3, HIGH); digitalWrite(r4, HIGH);
    while(digitalRead(c3) == LOW){
		if (digitalRead(c3) == LOW) {
		  selectedChar = hexaKeys[0][2][1];
		  writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
		  if (digitalRead(c3) == LOW) {
			selectedChar = hexaKeys[1][2][1];
			writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
			if (digitalRead(c3) == LOW) {
			  selectedChar = hexaKeys[2][2][1];
			  writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
			  if (digitalRead(c3) == LOW) {
				selectedChar = hexaKeys[3][2][1];
				writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
			  }
			}
		  }
		}
      a = 'c';
    }

    //      2  hexaKeys[0][0][2]
    digitalWrite(r1, HIGH); digitalWrite(r2, HIGH);
    digitalWrite(r3, LOW); digitalWrite(r4, HIGH);
    while(digitalRead(c1) == LOW){
		if (digitalRead(c1) == LOW) {
		  selectedChar = hexaKeys[0][0][2];
		  writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
		  if (digitalRead(c1) == LOW) {
			selectedChar = hexaKeys[1][0][2];
			writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
			if (digitalRead(c1) == LOW) {
			  selectedChar = hexaKeys[2][0][2];
			  writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
			  if (digitalRead(c1) == LOW) {
				selectedChar = hexaKeys[3][0][2];
				writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
			  }
			}
		  }
		}
      a = 'c'   ;
    }

    //     6  hexaKeys[0][1][2]
    digitalWrite(r1, HIGH); digitalWrite(r2, HIGH);
    digitalWrite(r3, LOW); digitalWrite(r4, HIGH);
    while(digitalRead(c2) == LOW){
		if (digitalRead(c2) == LOW) {
		  selectedChar = hexaKeys[0][1][2];
		  writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
		  if (digitalRead(c2) == LOW) {
			selectedChar = hexaKeys[1][1][2];
			writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
			if (digitalRead(c2) == LOW) {
			  selectedChar = hexaKeys[2][1][2];
			  writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
			  if (digitalRead(c2) == LOW) {
				selectedChar = hexaKeys[3][1][2];
				writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
			  }
			}
		  }
		}
      a = 'c';
    }

    //  Y=@  hexaKeys[0][2][2]
    digitalWrite(r1, HIGH); digitalWrite(r2, HIGH);
    digitalWrite(r3, LOW); digitalWrite(r4, HIGH);
    while(digitalRead(c3) == LOW){
		if (digitalRead(c3) == LOW) {
		  selectedChar = hexaKeys[0][2][2];
//		  writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
	//      if (digitalRead(c3) == LOW) {
	//        selectedChar = hexaKeys[1][2][2];
	//        writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
	//        if (digitalRead(c3) == LOW) {
	//          selectedChar = hexaKeys[2][2][2];
	//          writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
	//        }
	//      }
		}
      a = 'c';
    }

    //  3  hexaKeys[0][0][3]
    digitalWrite(r1, HIGH); digitalWrite(r2, HIGH);
    digitalWrite(r3, HIGH); digitalWrite(r4, LOW);
    while(digitalRead(c1) == LOW){
		if (digitalRead(c1) == LOW) {
		  selectedChar = hexaKeys[0][0][3];
		  writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
		  if (digitalRead(c1) == LOW) {
			selectedChar = hexaKeys[1][0][3];
			writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
			if (digitalRead(c1) == LOW) {
			  selectedChar = hexaKeys[2][0][3];
			  writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
			  if (digitalRead(c1) == LOW) {
				selectedChar = hexaKeys[3][0][3];
				writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
			  }
			}
		  }
		}
      a = 'c';
    }

    //  7  hexaKeys[0][1][3]
    digitalWrite(r1, HIGH); digitalWrite(r2, HIGH);
    digitalWrite(r3, HIGH); digitalWrite(r4, LOW);
    while(digitalRead(c2) == LOW){
		if (digitalRead(c2) == LOW) {
		  selectedChar = hexaKeys[0][1][3];
		  writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
		  if (digitalRead(c2) == LOW) {
			selectedChar = hexaKeys[1][1][3];
			writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
			if (digitalRead(c2) == LOW) {
			  selectedChar = hexaKeys[2][1][3];
			  writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
			  if (digitalRead(c2) == LOW) {
				selectedChar = hexaKeys[3][1][3];
				writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
			  }
			}
		  }
		}
      a = 'c';
    }
    //  N  hexaKeys[0][2][3]
    digitalWrite(r1, HIGH); digitalWrite(r2, HIGH);
    digitalWrite(r3, HIGH); digitalWrite(r4, LOW);
    while(digitalRead(c3) == LOW){
		if (digitalRead(c3) == LOW) {
		  selectedChar = hexaKeys[0][2][3];
//		  writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
//		  if (digitalRead(c3) == LOW) {
//			selectedChar = hexaKeys[1][2][3];
//			writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
//			if (digitalRead(c3) == LOW) {
//			  selectedChar = hexaKeys[2][2][3];
//			  writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
//			  if (digitalRead(c3) == LOW) {
//				selectedChar = hexaKeys[3][2][3];
//				writeSelectedCharAndString(selectedChar, current_x, current_y, caps);         delay(keyDelay);
//			  }
//			}
//		  }
		}
      a = 'c';
    }

  }//WHILE ENDING
  if(selectedChar == '@')
  {
	  writeSelectedCharAndStringBlankingPlus(current_x, current_y);
  }
  else if(selectedChar == '~')
  {
	  return selectedChar;
  }
  else
  {
	  writeSelectedCharAndStringBlanking(current_x, current_y);
  }
  return selectedChar;

}//ALPHKEYPAD  ENDING
#endif
*/

