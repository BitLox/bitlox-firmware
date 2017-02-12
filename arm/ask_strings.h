/*
 * ask_strings.h
 *
 *  Created on: Jan 4, 2016
 *      Author: thesquid
 */

#ifndef ARM_BITLOX_54_ARM_ASK_STRINGS_H_
#define ARM_BITLOX_54_ARM_ASK_STRINGS_H_



#include <string.h>

#include "../common.h"

#ifdef __cplusplus
     extern "C" {
#endif

/**
 * \defgroup AskStrings String literals for user prompts.
 *
 * The code would be much more readable if the string literals were all
 * implicitly defined within userDenied(). However, then they eat up valuable
 * RAM. Declaring them here means that they can have the # attribute
 * added (to place them in program memory).
 *
 * @{
 */

/** Language names */
//const unsigned int str_numbers_UNICODE[][10]  = {
//		{0x0030,0x0031,0x0032,0x0033,0x0034,0x0035,0x0036,0x0037,0x0038,0x0039}
//};
//

//splash screen of language choices
const unsigned int str_lang_list_UNICODE[][24]  = {
		{0x0020,0x0031,0x002E,0x0045,0x004E,0x0047,0x004C,0x0049,0x0053,0x0048,0x0020,0x0020,0x0036,0x002E,0x0046,0x0052,0x0041,0x004E,0x00C7,0x0041,0x0049,0x0053},
		{0x0020,0x0032,0x002E,0x0044,0x0045,0x0055,0x0054,0x0053,0x0043,0x0048,0x0020,0x0020,0x0037,0x002E,0x0045,0x0053,0x0050,0x0041,0x00D1,0x004F,0x004C},
		{0x0020,0x0033,0x002E,0x0420,0x0423,0x0421,0x0421,0x041A,0x0418,0x0419,0x0020,0x0020,0x0038,0x002E,0x0050,0x004F,0x0052,0x0054,0x0055,0x0047,0x0055,0x00CA,0x0053},
		{0x0020,0x0034,0x002E,0xD6D0,0xCEC4,0x0020,0x0020,0x0020,0x0020,0x0020,0x0039,0x002E,0x0054,0x00DC,0x0052,0x004B,0x00C7,0x0045},
		{0x0020,0x0035,0x002E,0x010C,0x0045,0x0160,0x0054,0x0049,0x004E,0x0041,0x0020,0x0020,0x0030,0x002E,0x0049,0x0054,0x0041,0x004c,0x0049,0x0041,0x004e,0x004f}
};


/** What will be appended to the transaction fee amount
  * for #ASKUSER_SIGN_TRANSACTION prompt. */


static const char str_fee_part1[]  = " BTC";
const unsigned int str_fee_part1_UNICODE[][7]  = {
		{0x0052,0x0045,0x0041,0x0044,0x0059},//EN
		{0x0042,0x0045,0x0052,0x0045,0x0049,0x0054},//DE
		{0x0413,0x041E,0x0422,0x041E,0x0412},//RU
		{0x0052,0x0045,0x0041,0x0044,0x0059},//ZH
		{0x0052,0x0045,0x0041,0x0044,0x0059},//CZ
		{0x0052,0x0045,0x0041,0x0044,0x0059},
		{0x0052,0x0045,0x0041,0x0044,0x0059},
		{0x0052,0x0045,0x0041,0x0044,0x0059},
		{0x0052,0x0045,0x0041,0x0044,0x0059},
		{0x0052,0x0045,0x0041,0x0044,0x0059}
	};




/** First line of unknown prompt. */
//static const char str_unknown_line0[]  = "Unknown command in userDenied()";
const unsigned int str_unknown_line0_UNICODE[][7]  = {
		{0x0052,0x0045,0x0041,0x0044,0x0059},//EN
		{0x0042,0x0045,0x0052,0x0045,0x0049,0x0054},//DE
		{0x0413,0x041E,0x0422,0x041E,0x0412},//RU
		{0x0052,0x0045,0x0041,0x0044,0x0059},//ZH
		{0x0052,0x0045,0x0041,0x0044,0x0059},//CZ
		{0x0052,0x0045,0x0041,0x0044,0x0059},
		{0x0052,0x0045,0x0041,0x0044,0x0059},
		{0x0052,0x0045,0x0041,0x0044,0x0059},
		{0x0052,0x0045,0x0041,0x0044,0x0059},
		{0x0052,0x0045,0x0041,0x0044,0x0059}
	};

/** Second line of unknown prompt. */
//static const char str_unknown_line1[]  = "Press any button to continue";
const unsigned int str_unknown_line1_UNICODE[][7]  = {
		{0x0052,0x0045,0x0041,0x0044,0x0059},//EN
		{0x0042,0x0045,0x0052,0x0045,0x0049,0x0054},//DE
		{0x0413,0x041E,0x0422,0x041E,0x0412},//RU
		{0x0052,0x0045,0x0041,0x0044,0x0059},//ZH
		{0x0052,0x0045,0x0041,0x0044,0x0059},//CZ
		{0x0052,0x0045,0x0041,0x0044,0x0059},
		{0x0052,0x0045,0x0041,0x0044,0x0059},
		{0x0052,0x0045,0x0041,0x0044,0x0059},
		{0x0052,0x0045,0x0041,0x0044,0x0059},
		{0x0052,0x0045,0x0041,0x0044,0x0059}
	};
const wchar_t STREAM_ERROR_line0[][3] = {
		L"EN", //EN
		L"DE", //DE
		L"RU", //RU
		L"ZH", //ZH
		L"CZ", //CZ
		L"FR", //FR
		L"ES", //ES
		L"PT", //PT
		L"TU", //TU
		L"IT"  //IT
} ;

/** What will be displayed if a stream read or write error occurs. */
static const char str_stream_error[]  = "Stream error";
const unsigned int str_stream_error_UNICODE[][7]  = {
		{0x0052,0x0045,0x0041,0x0044,0x0059},//EN
		{0x0042,0x0045,0x0052,0x0045,0x0049,0x0054},//DE
		{0x0413,0x041E,0x0422,0x041E,0x0412},//RU
		{0x0052,0x0045,0x0041,0x0044,0x0059},//ZH
		{0x0052,0x0045,0x0041,0x0044,0x0059},//CZ
		{0x0052,0x0045,0x0041,0x0044,0x0059},
		{0x0052,0x0045,0x0041,0x0044,0x0059},
		{0x0052,0x0045,0x0041,0x0044,0x0059},
		{0x0052,0x0045,0x0041,0x0044,0x0059},
		{0x0052,0x0045,0x0041,0x0044,0x0059}
	};
/**@}*/

#ifdef __cplusplus
     }
#endif

#endif /* ARM_BITLOX_54_ARM_ASK_STRINGS_H_ */