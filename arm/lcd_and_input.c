/** \file lcd_and_input.c
  *
  * \brief HD44780-based LCD driver and input button reader.
  *
  * It's assumed that the LCD has 2 lines, each character is 5x8 dots and
  * there are 40 bytes per line of DDRAM.
  * The datasheet was obtained on 22-September-2011, from:
  * http://lcd-linux.sourceforge.net/pdfdocs/hd44780.pdf
  *
  * All references to "the datasheet" refer to this document.
  *
  * This also (incidentally) deals with button inputs, since there's a
  * timer ISR which can handle the debouncing. The pin assignments in this
  * file are referred to by their Arduino pin mapping; if not using an
  * Arduino, see http://arduino.cc/en/Hacking/PinMapping168
  * for pin mappings.
  *
  * This file is licensed as described by the file LICENCE.
  */

#include <string.h>

#include "ask_strings.h"

//#include "lcd_and_input.h"
#include "../common.h"
#include "../hwinterface.h"
#include "../baseconv.h"
#include "../prandom.h"
#include "eink.h"
#include "Arduino.h"
#include "keypad_arm.h"
#include "keypad_alpha.h"
#include "avr2arm.h"
#include "../stream_comm.h"


#define COUNT_OF(x) ((sizeof(x)/sizeof(0[x])) / ((size_t)(!(sizeof(x) % sizeof(0[x])))))

char waitForNumberButtonPress(void);
void showWorking(void);
void showDenied(void);

char transStringBuffer0[MAX_STRING_DISPLAY] = {0};
char transStringBuffer1[MAX_STRING_DISPLAY] = {0};


void setLang(void){
	char langChar;
	int lang;
	langChar = waitForNumberButtonPress();

	lang = langChar - '0';

	if (lang == 0){
		lang = 9;
	}
	else
	{
		lang = lang - 1;
	}
	if (langChar == 'Y'){
		lang = 0;
	}
	uint8_t temp1[1];
	temp1[0] = (uint8_t*)lang;

	nonVolatileWrite(temp1, DEVICE_LANG_ADDRESS, 1);

	int s = 123;
	uint8_t set[1];
	set[0] = s;
	nonVolatileWrite(set, DEVICE_LANG_SET_ADDRESS, 1);

	sendSuccessPacketOnly();
}


void setLangInitially(void){
	char langChar;
	int lang;
	langChar = waitForNumberButtonPress(); // temp deactivated

	lang = langChar - '0';

	if (lang == 0){
		lang = 9;
	}
	else
	{
		lang = lang - 1;
	}
	if (langChar == 'Y'){
		lang = 0;
	}


	uint8_t temp1[1];
	temp1[0] = (uint8_t*)lang;

	nonVolatileWrite(temp1, DEVICE_LANG_ADDRESS, 1);

	int s = 123;
	uint8_t set[1];
	set[0] = s;
	nonVolatileWrite(set, DEVICE_LANG_SET_ADDRESS, 1);
}


void resetLang(void){
	uint8_t set[1];
	set[0] = 0;
	nonVolatileWrite(set, DEVICE_LANG_SET_ADDRESS, 1);
}



/** Maximum number of address/amount pairs that can be stored in RAM waiting
  * for approval from the user. This incidentally sets the maximum
  * number of outputs per transaction that parseTransaction() can deal with.
  * \warning This must be < 256.
  */
#define MAX_OUTPUTS		32 //originally 2


/** The Arduino pin number that the accept button is connected to. */
#define ACCEPT_PIN		0 // 6  =original setup arduino mega pin 31, now PC0
/** The Arduino pin number that the cancel button is connected to. */
#define	CANCEL_PIN		14 // 7 = original setup arduino mega pin 30, now PK6


/** Number of consistent samples (each sample is 5 ms apart) required to
  * register a button press.
  * \warning This must be < 256.
  */
#define DEBOUNCE_COUNT	4


/*
 *      Remove given section from string. Negative len means remove
 *      everything up to the end.
 */
int str_cut(char *str, int begin, int len)
{
    int l = strlen(str);

    if (len < 0) len = l - begin;
    if (begin + len > l) len = l - begin;
    memmove(str + begin, str + begin + len, l - len + 1);

    return len;
}



/** Status of accept button; false = not pressed, true = pressed. */
static volatile bool accept_button;
/** Status of cancel button; false = not pressed, true = pressed. */
static volatile bool cancel_button;
/** NUMBER BUTTONS */
static volatile bool num_1_button;
/** NUMBER BUTTONS */
static volatile bool num_2_button;
/** NUMBER BUTTONS */
static volatile bool num_3_button;
/** NUMBER BUTTONS */
static volatile bool num_4_button;
/** NUMBER BUTTONS */
static volatile bool num_5_button;
/** NUMBER BUTTONS */
static volatile bool num_6_button;
/** NUMBER BUTTONS */
static volatile bool num_7_button;
/** NUMBER BUTTONS */
static volatile bool num_8_button;
/** NUMBER BUTTONS */
static volatile bool num_9_button;
/** NUMBER BUTTONS */
static volatile bool num_0_button;
///** Debounce counter for accept button. */
//static uint8_t accept_debounce;
///** Debounce counter for cancel button. */
//static uint8_t cancel_debounce;


/** Storage for the text of transaction output amounts. */
static char list_amount[MAX_OUTPUTS][TEXT_AMOUNT_LENGTH];
/** Storage for the text of transaction output addresses. */
static char list_address[MAX_OUTPUTS][TEXT_ADDRESS_LENGTH];
/** Index into #list_amount and #list_address which specifies where the next
  * output amount/address will be copied into. */
static uint8_t list_index;
/** Whether the transaction fee has been set. If
  * the transaction fee still hasn't been set after parsing, then the
  * transaction is free. */
static bool transaction_fee_set;
/** Storage for transaction fee amount. This is only valid
  * if #transaction_fee_set is true. */
static char transaction_fee_amount[TEXT_AMOUNT_LENGTH];



/** Notify the user interface that the transaction parser has seen a new
  * Bitcoin amount/address pair.
  * \param text_amount The output amount, as a null-terminated text string
  *                    such as "0.01".
  * \param text_address The output address, as a null-terminated text string
  *                     such as "1RaTTuSEN7jJUDiW1EGogHwtek7g9BiEn".
  * \return false if no error occurred, true if there was not enough space to
  *         store the amount/address pair.
  */
bool newOutputSeen(char *text_amount, char *text_address)
{
	char *amount_dest;
	char *address_dest;

	if (list_index >= MAX_OUTPUTS)
	{
		return true; // not enough space to store the amount/address pair
	}
	amount_dest = list_amount[list_index];
	address_dest = list_address[list_index];
	strncpy(amount_dest, text_amount, TEXT_AMOUNT_LENGTH);
	strncpy(address_dest, text_address, TEXT_ADDRESS_LENGTH);
	amount_dest[TEXT_AMOUNT_LENGTH - 1] = '\0';
	address_dest[TEXT_ADDRESS_LENGTH - 1] = '\0';
	list_index++;
	return false; // success
}


/** Notify the user interface that the transaction parser has seen the
  * transaction fee. If there is no transaction fee, the transaction parser
  * will not call this.
  * \param text_amount The transaction fee, as a null-terminated text string
  *                    such as "0.01".
  */
void setTransactionFee(char *text_amount)
{
	strncpy(transaction_fee_amount, text_amount, TEXT_AMOUNT_LENGTH);
	transaction_fee_amount[TEXT_AMOUNT_LENGTH - 1] = '\0';
	transaction_fee_set = true;
}


/** Notify the user interface that the list of Bitcoin amount/address pairs
  * should be cleared. */
void clearOutputsSeen(void)
{
	list_index = 0;
	transaction_fee_set = false;
}


/** Wait until neither accept nor cancel buttons are being pressed. */
static void waitForNoButtonPress(void)
{
	const char* ptr_pressed;
	char pressed;

	do
	{

//		ptr_pressed = getAcceptCancelKeys();
		pressed = getAcceptCancelKeys();
//		pressed = *ptr_pressed;
		if (pressed == 'Y')
		{
			accept_button = 1;
		}
		else if (pressed == 'N')
		{
			cancel_button = 1;
		}
		else
		{
			accept_button = 0;
			cancel_button = 0;
		}		// do nothing

	} while (accept_button || cancel_button);
}



/** Wait until a numerical, accept or cancel button is pressed.
  * \return int of the button pressed.
  */
//static const char * waitForNumberButtonPress(void)
bool waitForButtonPress(void)
{
	bool current_accept_button;
	bool current_cancel_button;
//	bool current_1_button;
//	bool current_2_button;
//	bool current_3_button;
//	bool current_4_button;
//	bool current_5_button;
//	bool current_6_button;
//	bool current_7_button;
//	bool current_8_button;
//	bool current_9_button;
//	bool current_0_button;
	const char* ptr_pressed;
	char pressed;

	do
	{
		// Copy to avoid race condition.

//		ptr_pressed = getAcceptCancelKeys();
		pressed = getAcceptCancelKeys();
//		pressed = *ptr_pressed;

		switch (pressed){
		case 'Y'	:
			accept_button = 1;
			break;
		case 'N'	:
			cancel_button = 1;
			break;
//		case '1'	:
//
//			break;
//		case '2'	:
//
//			break;
//		case '3'	:
//
//			break;
//		case '4'	:
//
//			break;
//		case '5'	:
//
//			break;
//		case '6'	:
//
//			break;
//		case '7'	:
//
//			break;
//		case '8'	:
//
//			break;
//		case '9'	:
//
//			break;
//		case '0'	:
//
//			break;
		default:
			accept_button = 0;
			cancel_button = 0;
		}

		current_accept_button = accept_button;
		current_cancel_button = cancel_button;

	} while (!current_accept_button && !current_cancel_button);

	if (current_accept_button)
		{
			return false;
		}
	else
		{
			return true;
		}
}


/** Wait until a numerical, accept or cancel button is pressed.
  * \return int of the button pressed.
  */
//static const char * waitForNumberButtonPress(void)
char waitForNumberButtonPress(void)
{
	bool current_accept_button;
	bool current_cancel_button;
	bool current_1_button;
	bool current_2_button;
	bool current_3_button;
	bool current_4_button;
	bool current_5_button;
	bool current_6_button;
	bool current_7_button;
	bool current_8_button;
	bool current_9_button;
	bool current_0_button;
	const char* ptr_pressed;
	char pressed;

	do
	{
		// Copy to avoid race condition.

//		ptr_pressed = getFullKeys();
		pressed = getFullKeys();
//		pressed = *ptr_pressed;

		switch (pressed){
		case 'Y'	:
			accept_button = 1;
			break;
		case 'N'	:
			cancel_button = 1;
			break;
		case '1'	:
			num_1_button = 1;
			break;
		case '2'	:
			num_2_button = 1;
			break;
		case '3'	:
			num_3_button = 1;
			break;
		case '4'	:
			num_4_button = 1;
			break;
		case '5'	:
			num_5_button = 1;
			break;
		case '6'	:
			num_6_button = 1;
			break;
		case '7'	:
			num_7_button = 1;
			break;
		case '8'	:
			num_8_button = 1;
			break;
		case '9'	:
			num_9_button = 1;
			break;
		case '0'	:
			num_0_button = 1;
			break;
		default:
			accept_button = 0;
			cancel_button = 0;
			num_1_button  = 0;
			num_2_button  = 0;
			num_3_button  = 0;
			num_4_button  = 0;
			num_5_button  = 0;
			num_6_button  = 0;
			num_7_button  = 0;
			num_8_button  = 0;
			num_9_button  = 0;
			num_0_button  = 0;
		}

		current_accept_button = accept_button;
		current_cancel_button = cancel_button;
		current_1_button = num_1_button;
		current_2_button = num_2_button;
		current_3_button = num_3_button;
		current_4_button = num_4_button;
		current_5_button = num_5_button;
		current_6_button = num_6_button;
		current_7_button = num_7_button;
		current_8_button = num_8_button;
		current_9_button = num_9_button;
		current_0_button = num_0_button;


	} while (!current_accept_button && !current_cancel_button && !current_1_button && !current_2_button && !current_3_button && !current_4_button && !current_5_button && !current_6_button && !current_7_button && !current_8_button && !current_9_button && !current_0_button);

	return pressed;
}


/** Wait until a numerical, accept or cancel button is pressed.
  * \return int of the button pressed.
  */
//static const char * waitForNumberButtonPress(void)
char waitForNumberButtonPress4to8(void)
{
	bool current_accept_button;
	bool current_cancel_button;
	bool current_1_button;
	bool current_2_button;
	bool current_3_button;
	bool current_4_button;
	bool current_5_button;
	bool current_6_button;
	bool current_7_button;
	bool current_8_button;
	bool current_9_button;
	bool current_0_button;
	const char* ptr_pressed;
	char pressed;

	do
	{
		// Copy to avoid race condition.

//		ptr_pressed = getFullKeys();
		pressed = getFullKeys();
//		pressed = *ptr_pressed;

		switch (pressed){
		case 'Y'	:
//			accept_button = 1;
			break;
		case 'N'	:
			cancel_button = 1;
			break;
		case '1'	:
//			num_1_button = 1;
			break;
		case '2'	:
//			num_2_button = 1;
			break;
		case '3'	:
//			num_3_button = 1;
			break;
		case '4'	:
			num_4_button = 1;
			break;
		case '5'	:
			num_5_button = 1;
			break;
		case '6'	:
			num_6_button = 1;
			break;
		case '7'	:
			num_7_button = 1;
			break;
		case '8'	:
			num_8_button = 1;
			break;
		case '9'	:
//			num_9_button = 1;
			break;
		case '0'	:
//			num_0_button = 1;
			break;
		default:
			accept_button = 0;
			cancel_button = 0;
			num_1_button  = 0;
			num_2_button  = 0;
			num_3_button  = 0;
			num_4_button  = 0;
			num_5_button  = 0;
			num_6_button  = 0;
			num_7_button  = 0;
			num_8_button  = 0;
			num_9_button  = 0;
			num_0_button  = 0;
		}

		current_accept_button = accept_button;
		current_cancel_button = cancel_button;
		current_1_button = num_1_button;
		current_2_button = num_2_button;
		current_3_button = num_3_button;
		current_4_button = num_4_button;
		current_5_button = num_5_button;
		current_6_button = num_6_button;
		current_7_button = num_7_button;
		current_8_button = num_8_button;
		current_9_button = num_9_button;
		current_0_button = num_0_button;


	} while (!current_accept_button && !current_cancel_button && !current_1_button && !current_2_button && !current_3_button && !current_4_button && !current_5_button && !current_6_button && !current_7_button && !current_8_button && !current_9_button && !current_0_button);

	return pressed;
}

char* getTransString0(char* str0) {
	strcpy_P(transStringBuffer0, (char*)str0);
	return transStringBuffer0;
}
char* getTransString1(char* str1) {
	strcpy_P(transStringBuffer1, (char*)str1);
	return transStringBuffer1;
}
/*
###########################################################
###########################################################
###########################################################
###########################################################
###########################################################
*/





//const wchar_t INITIAL_SETUP_line1[][18] = {
//		L"STANDARD     1...",
//		L"STANDARD     1...",
//		L"СТАНДАРТНАЯ  1...",
//		L"常态    1...",
//		L"STANDARDNÍ   1...",
//		L"STANDARD     1...",
//		L"ESTÁNDAR     1...",
//		L"PADRÃO       1...",
//		L"STANDART     1...",
//		L"STANDARD     1..."
//} ;
//
//const wchar_t INITIAL_SETUP_line2[][18] = {
//		L"ADVANCED     2...", //EN
//		L"ERWEITERTE   2...", //DE
//		L"РАСШИРЕННАЯ  2...", //RU
//		L"高级    2...", //ZH
//		L"POKROČILÉ    2...", //CZ
//		L"AVANCÉE      2...", //FR
//		L"AVANZADA     2...", //ES
//		L"AVANÇADOS    2...", //PT
//		L"GELİŞMİŞ     2...", //TU
//		L"AVANZATO     2..."  //IT
//} ;
//
//const wchar_t INITIAL_SETUP_line3[][18] = {
//		L"EXPERT       3...", //EN
//		L"EXPERT       3...", //DE
//		L"ЭКСПЕРТ      3...", //RU
//		L"专家    3...", //ZH
//		L"EXPERT       3...", //CZ
//		L"EXPERT       3...", //FR
//		L"EXPERTO      3...", //ES
//		L"ESPECIALISTA 3...", //PT
//		L"UZMAN        3...", //TU
//		L"ESPERTO      3..."  //IT
//} ;


/*
###########################################################
###########################################################
###########################################################
###########################################################
###########################################################
*/


//#####################################################################################
//#####################################################################################
//#####################################################################################

/** Ask user for input.
  * \param command The action to ask the user about. See #AskUserCommandEnum.
  * \return char with the results of a key event.
  */
char *userInput(AskUserCommand command)
{
	uint8_t i;
	uint8_t tempLang[1];
	nonVolatileRead(tempLang, DEVICE_LANG_ADDRESS, 1);

	int lang;
	lang = (int)tempLang[0];

	int zhSizer = 1;

	if (lang == 3)
	{
		zhSizer = 2;
	}


	char *r; // what will be returned


	if (command == ASKUSER_INITIAL_SETUP)
		{
		const wchar_t INITIAL_SETUP_line0[][25] = {
				L"INITIAL SETUP",
				L"ERSTEINRICHTUNG",
				L"НАЧАЛЬНАЯ НАСТРОЙКА",
				L"初始设置",
				L"POČÁTEČNÍ KONFIGURACE",
				L"CONFIGURATION INITIALE",
				L"CONFIGURACIÓN INICIAL",
				L"CONFIGURAÇÃO INICIAL",
				L"İLK KURULUM",
				L"CONFIGURAZIONE INIZIALE"
		} ;

		const wchar_t INITIAL_SETUP_line1[][18] = {
				L"STANDARD     1...",
				L"STANDARD     1...",
				L"СТАНДАРТНАЯ  1...",
				L"常态    1...",
				L"STANDARDNÍ   1...",
				L"STANDARD     1...",
				L"ESTÁNDAR     1...",
				L"PADRÃO       1...",
				L"STANDART     1...",
				L"STANDARD     1..."
		} ;

		const wchar_t INITIAL_SETUP_line2[][18] = {
				L"ADVANCED     2...", //EN
				L"ERWEITERTE   2...", //DE
				L"РАСШИРЕННАЯ  2...", //RU
				L"高级    2...", //ZH
				L"POKROČILÉ    2...", //CZ
				L"AVANCÉE      2...", //FR
				L"AVANZADA     2...", //ES
				L"AVANÇADOS    2...", //PT
				L"GELİŞMİŞ     2...", //TU
				L"AVANZATO     2..."  //IT
		} ;

		const wchar_t INITIAL_SETUP_line3[][18] = {
				L"EXPERT       3...", //EN
				L"EXPERT       3...", //DE
				L"ЭКСПЕРТ      3...", //RU
				L"专家    3...", //ZH
				L"EXPERT       3...", //CZ
				L"EXPERT       3...", //FR
				L"EXPERTO      3...", //ES
				L"ESPECIALISTA 3...", //PT
				L"UZMAN        3...", //TU
				L"ESPERTO      3..."  //IT
		} ;


		waitForNoButtonPress();
		initDisplay();
	    overlayBatteryStatus(BATT_VALUE_DISPLAY);
//		writeEinkDrawUnicodeSingle(temp, tempLength, COL_1_X, LINE_0_Y);
		writeEinkDrawUnicodeSingle((unsigned int*)INITIAL_SETUP_line0[lang], wcslen(INITIAL_SETUP_line0[lang]), COL_1_X, LINE_0_Y);
		writeUnderline(STRIPE_X_START, STRIPE_Y_START, STRIPE_X_END, STRIPE_Y_END);
		writeEinkDrawUnicodeSingle((unsigned int*)INITIAL_SETUP_line1[lang], wcslen(INITIAL_SETUP_line1[lang]), COL_1_X, LINE_1_Y);
		writeEinkDrawUnicodeSingle((unsigned int*)INITIAL_SETUP_line2[lang], wcslen(INITIAL_SETUP_line2[lang]), COL_1_X, LINE_2_Y);
		writeEinkDrawUnicodeSingle((unsigned int*)INITIAL_SETUP_line3[lang], wcslen(INITIAL_SETUP_line3[lang]), COL_1_X, LINE_3_Y);
//		waitForNumberButtonPress();
		display();


//		r = waitForNumberButtonPress();
//		clearDisplay();
	}
	if (command == ASKUSER_NEW_WALLET_NUMBER)
		{
		const wchar_t NEW_WALLET_NUMBER_line0[][25] = {
				L"HIDDEN WALLET NUMBER", //EN
				L"VERSTECKTE WALLET NUMMER", //DE
				L"НОМЕР ТАЙНОГО КОШЕЛЬКА", //RU
				L"秘密钱袋号码", //ZH
				L"ČÍSLO SKRYTÉ PENĚŽENKY", //CZ
				L"NUMÉRO POUR LA CACHÉE", //FR
				L"NÚMERO OCULTO", //ES
				L"NUMERO ESCONDIDA", //PT
				L"GİZLİ CÜZDAN NUMARASI", //TU
				L"NUMERO NASCOSTO"  //IT
		} ;

		const wchar_t NEW_WALLET_NUMBER_line1[][25] = {
				L"PLEASE ENTER A WALLET", //EN
				L"WAHLEN SIE EIN WALLET", //DE
				L"ВВЕДИТЕ НОМЕР КОШЕЛЬКА", //RU
				L"请输入51与100之", //ZH
				L"PROSÍM VLOŽTE ČÍSLO", //CZ
				L"S'IL VOUS PLAÎT ENTREZ", //FR
				L"INTRODUZCA UN NÚMERO", //ES
				L"INSIRA UMA CARTEIRA", //PT
				L"LÜTFEN 51-100 ARASI", //TU
				L"INSERISCI UN NUMERO"  //IT
		} ;


		const wchar_t NEW_WALLET_NUMBER_line2[][25] = {
				L"NUMBER 51-100", //EN
				L"NUMMER ZWISCHEN 51 & 100", //DE
				L"ОТ 51 ДО 100", //RU
				L"间的钱袋号码", //ZH
				L"PENĚŽENKY 51-100", //CZ
				L"UN NUMÉRO COMPRIS 51-100", //FR
				L"51-100", //ES
				L"NUMERO 51-100", //PT
				L"BİR CÜZDAN NUMARASI GİRİ", //TU
				L"IL PORTAFOGLIO DA 51-100"  //IT
		} ;


		const wchar_t NEW_WALLET_NUMBER_line3[][25] = {
				L"NOW USING THE KEYPAD", //EN
				L"JETZT", //DE
				L"ЗАПОМНИТЕ ЭТОТ НОМЕР!", //RU
				L"您必须牢记这个号码", //ZH
				L"NESMÍTE JEJ ZAPOMENOUT #", //CZ
				L"VOUS SOUVENIR DE CE #", //FR
				L"USTED DEBE RECORDARLO #", //ES
				L"LEMBREM-SE DESTE NUMERO", //PT
				L"BU NUMARAYI HATIRLAMANIZ", //TU
				L"MEMORIZZARE NUMERO #"  //IT
		} ;
		const wchar_t uiBACK_line0[][9] = {
				L"BACK", //EN
				L"ZURÜCK", //DE
				L"НАЗАД", //RU
				L"退回", //ZH
				L"ZPĚT", //CZ
				L"RETOUR", //FR
				L"ATRÁS", //ES
				L"COSTAS", //PT
				L"GERİ", //TU
				L"INDIETRO"  //IT
		} ;
		const wchar_t uiGO_line0[][12] = {
				L"GO", //EN
				L"GEHEN", //DE
				L"ПОДТВЕРДИТЬ", //RU
				L"确认", //ZH
				L"DÁLE", //CZ
				L"CONTINUER", //FR
				L"IR", //ES
				L"IR", //PT
				L"İLERLE", //TU
				L"GO"  //IT
		} ;


		waitForNoButtonPress();
		initDisplay();
	    overlayBatteryStatus(BATT_VALUE_DISPLAY);
		writeEinkDrawUnicodeSingle((unsigned int*)NEW_WALLET_NUMBER_line0[lang], wcslen(NEW_WALLET_NUMBER_line0[lang]), COL_1_X, LINE_0_Y);
		writeUnderline(STRIPE_X_START, STRIPE_Y_START, STRIPE_X_END, STRIPE_Y_END);
		writeEinkDrawUnicodeSingle((unsigned int*)NEW_WALLET_NUMBER_line1[lang], wcslen(NEW_WALLET_NUMBER_line1[lang]), COL_1_X, LINE_1_Y);
		writeEinkDrawUnicodeSingle((unsigned int*)NEW_WALLET_NUMBER_line2[lang], wcslen(NEW_WALLET_NUMBER_line2[lang]), COL_1_X, LINE_2_Y);
		writeEinkDrawUnicodeSingle((unsigned int*)NEW_WALLET_NUMBER_line3[lang], wcslen(NEW_WALLET_NUMBER_line3[lang]), COL_1_X, LINE_3_Y);

		writeEinkDrawUnicodeSingle((unsigned int*)uiGO_line0[lang], wcslen(uiGO_line0[lang]), ACCEPT_X_START, LINE_4_Y);
		writeEinkDrawUnicodeSingle((unsigned int*)uiBACK_line0[lang], wcslen(uiBACK_line0[lang]), (DENY_X_START)-((zhSizer*wcslen(uiBACK_line0[lang]))*8), LINE_4_Y);
//		writeEinkDrawUnicodeSingle(str_BACK_line0_UNICODE_sized[lang][0], line5length, (DENY_X_START)-(line5length*8), LINE_4_Y);

//#define draw_X_X 180
//#define draw_X_Y 87
//#define draw_check_X 3
//#define draw_check_Y 85



		drawX(draw_X_X,draw_X_Y);
		drawCheck(draw_check_X,draw_check_Y);
		display();


//		r = waitForNumberButtonPress();
//		clearDisplay();
	}
	if (command == ASKUSER_RESTORE_WALLET_DEVICE_INPUT_TYPE)
		{
		const wchar_t RESTORE_WALLET_DEVICE_INPUT_TYPE_line0[][25] = {
				L"MNEMONIC INPUT", //EN
				L"MNEMONIC INPUT", //DE
				L"МНЕМОНИЧЕСКИЕ ВВОД", //RU
				L"助记符输入", //ZH
				L"MNEMONICKÉ INPUT", //CZ
				L"ENTRÉE MNEMONIC", //FR
				L"ENTRADA MNEMÓNICO", //ES
				L"ENTRADA MNEMÔNICO", //PT
				L"ANIMSATICI GİRDİ", //TU
				L"INGRESSO MNEMONICO"  //IT
		} ;


		const wchar_t RESTORE_WALLET_DEVICE_INPUT_TYPE_line1[][25] = {
				L"INDEX    1...", //EN
				L"INDEX    1...", //DE
				L"ИНДЕКС   1...", //RU
				L"指数      1...", //ZH
				L"INDEX    1...", //CZ
				L"INDICE   1...", //FR
				L"ÍNDICE   1...", //ES
				L"ÍNDICE   1...", //PT
				L"INDEX    1...", //TU
				L"INDICE   1..."  //IT
		} ;


		const wchar_t RESTORE_WALLET_DEVICE_INPUT_TYPE_line2[][25] = {
				L"DIRECT   2...", //EN
				L"DIREKT   2...", //DE
				L"ПРЯМОЙ   2...", //RU
				L"直接      2...", //ZH
				L"ŘÍDIT    2...", //CZ
				L"DIRECT   2...", //FR
				L"DIRECTO  2...", //ES
				L"DIRETO   2...", //PT
				L"DOĞRUDAN 2...", //TU
				L"DIRETTO  2..."  //IT
		} ;
		const wchar_t uiBACK_line0[][9] = {
				L"BACK", //EN
				L"ZURÜCK", //DE
				L"НАЗАД", //RU
				L"退回", //ZH
				L"ZPĚT", //CZ
				L"RETOUR", //FR
				L"ATRÁS", //ES
				L"COSTAS", //PT
				L"GERİ", //TU
				L"INDIETRO"  //IT
		} ;

		waitForNoButtonPress();
		initDisplay();
	    overlayBatteryStatus(BATT_VALUE_DISPLAY);
		writeEinkDrawUnicodeSingle((unsigned int*)RESTORE_WALLET_DEVICE_INPUT_TYPE_line0[lang], wcslen(RESTORE_WALLET_DEVICE_INPUT_TYPE_line0[lang]), COL_1_X, LINE_0_Y);
		writeUnderline(STRIPE_X_START, STRIPE_Y_START, STRIPE_X_END, STRIPE_Y_END);
		writeEinkDrawUnicodeSingle((unsigned int*)RESTORE_WALLET_DEVICE_INPUT_TYPE_line1[lang], wcslen(RESTORE_WALLET_DEVICE_INPUT_TYPE_line1[lang]), COL_1_X, LINE_1_Y);
		writeEinkDrawUnicodeSingle((unsigned int*)RESTORE_WALLET_DEVICE_INPUT_TYPE_line2[lang], wcslen(RESTORE_WALLET_DEVICE_INPUT_TYPE_line2[lang]), COL_1_X, LINE_2_Y);
//		writeEinkDrawUnicodeSingle((unsigned int*)RESTORE_WALLET_DEVICE_INPUT_TYPE_line3[lang], wcslen(RESTORE_WALLET_DEVICE_INPUT_TYPE_line3[lang]), COL_1_X, LINE_3_Y);
//		writeEinkDrawUnicodeSingle(str_ACCEPT_line0_UNICODE_sized[lang][0], line3length, ACCEPT_X_START, 80);
		writeEinkDrawUnicodeSingle((unsigned int*)uiBACK_line0[lang], wcslen(uiBACK_line0[lang]), (DENY_X_START)-((zhSizer*wcslen(uiBACK_line0[lang]))*8), LINE_4_Y);

		drawX(draw_X_X,draw_X_Y);
//		drawCheck(draw_check_X,draw_check_Y);
		display();


		r = waitForNumberButtonPress();
		clearDisplay();
	}
	if (command == ASKUSER_NEW_WALLET_STRENGTH)
		{
		const wchar_t NEW_WALLET_STRENGTH_line0[][25] = {
				L"MNEMONIC STRENGTH", //EN
				L"MNEMONIC STÄRKE", //DE
				L"ВЫБОР СЛОЖНОСТИ МНЕМОНИК", //RU
				L"助记符级别选择", //ZH
				L"MNEMOTECHNICKÁ SÍLA", //CZ
				L"FORCE DE LA MNÉMONIQUE", //FR
				L"LONGITUD MNEMÓNICA", //ES
				L"MNEMÔNICO FORÇA", //PT
				L"MNEMONIC ŞİFRE", //TU
				L"LUNGHEZZA MNEMONIC"  //IT
		} ;


		const wchar_t NEW_WALLET_STRENGTH_line1[][25] = {
				L"12 WORDS    1...", //EN
				L"12 WÖRTEN   1...", //DE
				L"12 СЛОВ     1...", //RU
				L"12个单词     1...", //ZH
				L"12 SLOV     1...", //CZ
				L"12 MOTS     1...", //FR
				L"12 PALABRAS 1...", //ES
				L"12 PALAVRAS 1...", //PT
				L"12 KELİME   1...", //TU
				L"12 PAROLE   1..."  //IT
		} ;


		const wchar_t NEW_WALLET_STRENGTH_line2[][25] = {
				L"18 WORDS    2...", //EN
				L"18 WÖRTEN   2...", //DE
				L"18 СЛОВ     2...", //RU
				L"18个单词     2...", //ZH
				L"18 SLOV     2...", //CZ
				L"18 MOTS     2...", //FR
				L"18 PALABRAS 2...", //ES
				L"18 PALAVRAS 2...", //PT
				L"18 KELİME   2...", //TU
				L"18 PAROLE   2..."  //IT
		} ;


		const wchar_t NEW_WALLET_STRENGTH_line3[][25] = {
				L"24 WORDS    3...", //EN
				L"24 WÖRTEN   3...", //DE
				L"24 СЛОВ     3...", //RU
				L"24个单词     3...", //ZH
				L"24 SLOV     3...", //CZ
				L"24 MOTS     3...", //FR
				L"24 PALABRAS 3...", //ES
				L"24 PALAVRAS 3...", //PT
				L"24 KELİME   3...", //TU
				L"24 PAROLE   3..."  //IT
		} ;
		const wchar_t uiCANCEL_line0[][9] = {
				L"CANCEL", //EN
				L"ABSAGEN", //DE
				L"ОТМЕНА", //RU
				L"退回", //ZH
				L"ZRUŠIT", //CZ
				L"ANNULER", //FR
				L"CANCELAR", //ES
				L"CANCELAR", //PT
				L"İPTAL", //TU
				L"ANNULLA"  //IT
		} ;



		waitForNoButtonPress();
		initDisplay();
	    overlayBatteryStatus(BATT_VALUE_DISPLAY);
		writeEinkDrawUnicodeSingle((unsigned int*)NEW_WALLET_STRENGTH_line0[lang], wcslen(NEW_WALLET_STRENGTH_line0[lang]), COL_1_X, LINE_0_Y);
		writeUnderline(STRIPE_X_START, STRIPE_Y_START, STRIPE_X_END, STRIPE_Y_END);
		writeEinkDrawUnicodeSingle((unsigned int*)NEW_WALLET_STRENGTH_line1[lang], wcslen(NEW_WALLET_STRENGTH_line1[lang]), COL_1_X, LINE_1_Y);
		writeEinkDrawUnicodeSingle((unsigned int*)NEW_WALLET_STRENGTH_line2[lang], wcslen(NEW_WALLET_STRENGTH_line2[lang]), COL_1_X, LINE_2_Y);
		writeEinkDrawUnicodeSingle((unsigned int*)NEW_WALLET_STRENGTH_line3[lang], wcslen(NEW_WALLET_STRENGTH_line3[lang]), COL_1_X, LINE_3_Y);
//		writeEinkDrawUnicodeSingle(str_ACCEPT_line0_UNICODE_sized[lang][0], line3length, ACCEPT_X_START, 80);
		writeEinkDrawUnicodeSingle((unsigned int*)uiCANCEL_line0[lang], wcslen(uiCANCEL_line0[lang]), (DENY_X_START)-((zhSizer*wcslen(uiCANCEL_line0[lang]))*8), LINE_4_Y);

		drawX(draw_X_X,draw_X_Y);
//		drawCheck(draw_check_X,draw_check_Y);
		display();


		r = waitForNumberButtonPress();
		clearDisplay();
	}
	if (command == ASKUSER_NEW_WALLET_LEVEL)
		{
		const wchar_t NEW_WALLET_LEVEL_line0[][25] = {
				L"WALLET PIN TYPE", //EN
				L"WALLET PIN SORTE", //DE
				L"ТИП ПАРОЛЯ КОШЕЛЬКА", //RU
				L"钱袋密码类型", //ZH
				L"TYP PINu PENĚŽENKY", //CZ
				L"TYPE DE NIP DU PORTEF.", //FR
				L"MODO DEL CÓDIGO", //ES
				L"PIN DA CARTEIRA TIPO DE", //PT
				L"CÜZDAN PIN KODU ÇEŞİDİ", //TU
				L"TIPO DI PIN PER IL PORT."  //IT
		} ;


		const wchar_t NEW_WALLET_LEVEL_line1[][25] = {
				L"STANDARD     1...",
				L"STANDARD     1...",
				L"СТАНДАРТНЫЙ  1...",
				L"常态    1...",
				L"STANDARDNÍ   1...",
				L"STANDARD     1...",
				L"ESTÁNDAR     1...",
				L"PADRÃO       1...",
				L"STANDART     1...",
				L"STANDARD     1..."
		} ;

		const wchar_t NEW_WALLET_LEVEL_line2[][25] = {
				L"ADVANCED     2...", //EN
				L"ERWEITERTE   2...", //DE
				L"РАСШИРЕННЫЙ  2...", //RU
				L"高级    2...", //ZH
				L"POKROČILÉ    2...", //CZ
				L"AVANCÉE      2...", //FR
				L"AVANZADA     2...", //ES
				L"AVANÇADOS    2...", //PT
				L"GELİŞMİŞ     2...", //TU
				L"AVANZATO     2..."  //IT
		} ;

		const wchar_t NEW_WALLET_LEVEL_line3[][25] = {
				L"EXPERT       3...", //EN
				L"EXPERT       3...", //DE
				L"ЭКСПЕРТ      3...", //RU
				L"专家    3...", //ZH
				L"EXPERT       3...", //CZ
				L"EXPERT       3...", //FR
				L"EXPERTO      3...", //ES
				L"ESPECIALISTA 3...", //PT
				L"UZMAN        3...", //TU
				L"ESPERTO      3..."  //IT
		} ;
		const wchar_t uiCANCEL_line0[][9] = {
				L"CANCEL", //EN
				L"ABSAGEN", //DE
				L"ОТМЕНА", //RU
				L"退回", //ZH
				L"ZRUŠIT", //CZ
				L"ANNULER", //FR
				L"CANCELAR", //ES
				L"CANCELAR", //PT
				L"İPTAL", //TU
				L"ANNULLA"  //IT
		} ;



		waitForNoButtonPress();
		initDisplay();
	    overlayBatteryStatus(BATT_VALUE_DISPLAY);
		writeEinkDrawUnicodeSingle((unsigned int*)NEW_WALLET_LEVEL_line0[lang], wcslen(NEW_WALLET_LEVEL_line0[lang]), COL_1_X, LINE_0_Y);
		writeUnderline(STRIPE_X_START, STRIPE_Y_START, STRIPE_X_END, STRIPE_Y_END);
		writeEinkDrawUnicodeSingle((unsigned int*)NEW_WALLET_LEVEL_line1[lang], wcslen(NEW_WALLET_LEVEL_line1[lang]), COL_1_X, LINE_1_Y);
		writeEinkDrawUnicodeSingle((unsigned int*)NEW_WALLET_LEVEL_line2[lang], wcslen(NEW_WALLET_LEVEL_line2[lang]), COL_1_X, LINE_2_Y);
		writeEinkDrawUnicodeSingle((unsigned int*)NEW_WALLET_LEVEL_line3[lang], wcslen(NEW_WALLET_LEVEL_line3[lang]), COL_1_X, LINE_3_Y);
//		writeEinkDrawUnicodeSingle(str_ACCEPT_line0_UNICODE_sized[lang][0], line3length, ACCEPT_X_START, 80);
		writeEinkDrawUnicodeSingle((unsigned int*)uiCANCEL_line0[lang], wcslen(uiCANCEL_line0[lang]), (DENY_X_START)-((zhSizer*wcslen(uiCANCEL_line0[lang]))*8), LINE_4_Y);

		drawX(draw_X_X,draw_X_Y);
//		drawCheck(draw_check_X,draw_check_Y);
		display();


		r = waitForNumberButtonPress();
		clearDisplay();
	}


	return r;
}


//#####################################################################################
//#####################################################################################
//#####################################################################################


/** Ask user if they want to allow some action.
  * \param command The action to ask the user about. See #AskUserCommandEnum.
  * \return false if the user accepted, true if the user denied.
  */
bool userDeniedPlusData(AskUserCommand command, char *passed_data, int size_of_data)
{
	uint8_t i;
	uint8_t tempLang[1];
	nonVolatileRead(tempLang, DEVICE_LANG_ADDRESS, 1);

	int lang;
	lang = (int)tempLang[0];

	int zhSizer = 1;

	if (lang == 3)
	{
		zhSizer = 2;
	}


	bool r; // what will be returned

	r = true;
	if (command == ASKUSER_FORMAT_WITH_PROGRESS)
	{
		const wchar_t FORMAT_WITH_PROGRESS_line1[][25] = {
				L"FORMATTING", //EN
				L"FORMATIERUNG", //DE
				L"ФОРМАТИРОВАНИЕ", //RU
				L"格式化", //ZH
				L"FORMÁTOVÁNÍ", //CZ
				L"FORMATAGE", //FR
				L"INICIANDO", //ES
				L"FORMATAÇÃO", //PT
				L"FORMATLANIYOR", //TU
				L"FORMATTAZIONE"  //IT
		} ;


		//		waitForNoButtonPress();
		lang = size_of_data;

		initDisplay();
	    overlayBatteryStatus(BATT_VALUE_DISPLAY);
//		writeEinkDrawUnicodeSingle((unsigned int*)INITIAL_SETUP_line0[lang], wcslen(INITIAL_SETUP_line0[lang]), COL_1_X, LINE_0_Y);
		writeUnderline(STRIPE_X_START, STRIPE_Y_START, STRIPE_X_END, STRIPE_Y_END);
		writeEinkDrawUnicodeSingle((unsigned int*)FORMAT_WITH_PROGRESS_line1[lang], wcslen(FORMAT_WITH_PROGRESS_line1[lang]), COL_1_X, LINE_1_Y);
		writeEinkNoDisplaySingleBig(passed_data,COL_1_X,LINE_2_Y);
		writeEinkNoDisplaySingleBig("%",39,LINE_2_Y);

//		writeEinkDrawUnicodeSingle(str_CONFIRM_line0_UNICODE_sized[lang][0], line4length, ACCEPT_X_START, 80);
//		writeEinkDrawUnicodeSingle((unsigned int*)BACK_line0[lang], wcslen(BACK_line0[lang]), (DENY_X_START)-((zhSizer*wcslen(BACK_line0[lang]))*8), LINE_4_Y);

//		drawX(draw_X_X,draw_X_Y);
//		drawCheck(draw_check_X,draw_check_Y);
		display();

//		r = waitForButtonPress();
//		if (!r){
//			clearDisplay();
//		}else{
//			writeX_Screen();
//			showReady();
//		};
	}
	else if (command == ASKUSER_SET_DEVICE_PIN)
	{
		const wchar_t BACK_line0[][9] = {
				L"BACK", //EN
				L"ZURÜCK", //DE
				L"НАЗАД", //RU
				L"退回", //ZH
				L"ZPĚT", //CZ
				L"RETOUR", //FR
				L"ATRÁS", //ES
				L"COSTAS", //PT
				L"GERİ", //TU
				L"INDIETRO"  //IT
		} ;
		const wchar_t GO_line0[][12] = {
				L"GO", //EN
				L"GEHEN", //DE
				L"ПОДТВЕРДИТЬ", //RU
				L"确认", //ZH
				L"DÁLE", //CZ
				L"CONTINUER", //FR
				L"IR", //ES
				L"IR", //PT
				L"İLERLE", //TU
				L"GO"  //IT
		} ;

		const wchar_t SET_DEVICE_PIN_line0[][25] = {
				L"DEVICE PIN", //EN
				L"GERÄTE-PIN", //DE
				L"ПАРОЛЬ УСТРОЙСТВА", //RU
				L"设具密码", //ZH
				L"PIN ZAŘÍZENÍ", //CZ
				L"NIP DE L'APPAREIL", //FR
				L"CÓDIGO DEL DISPOSITIVO", //ES
				L"PIN DO DISPOSITIVO", //PT
				L"CİHAZ PIN", //TU
				L"PIN DEL DISPOSITIVO"  //IT
		} ;
		const wchar_t SET_PIN_line1[][25] = {
				L"PLEASE WRITE DOWN", //EN
				L"BITTE SCHREIBEN SIE", //DE
				L"ПОЖАЛУЙСТА, ЗАПИШИТЕ", //RU
				L"请记下", //ZH
				L"PROSÍM ZAPIŠTE SI", //CZ
				L"S'IL VOUS PLAÎT NOTER", //FR
				L"PORFAVOR TOME NOTA", //ES
				L"POR FAVOR, ANOTE", //PT
				L"LÜTFEN YAZIN", //TU
				L"PER FAVORE SCRIVI IL"  //IT
		} ;
		const wchar_t SET_PIN_line2[][25] = {
				L"YOUR PIN", //EN
				L"IHR PIN", //DE
				L"ПАРОЛЬ", //RU
				L"您的密码", //ZH
				L"VÁŠ PIN", //CZ
				L"VOTRE NIP", //FR
				L"EL CÓDIGO", //ES
				L"SEU PIN", //PT
				L"PIN KODUNUZ", //TU
				L"IL PIN"  //IT
		} ;

		waitForNoButtonPress();

		initDisplay();
	    overlayBatteryStatus(BATT_VALUE_DISPLAY);
		writeEinkDrawUnicodeSingle((unsigned int*)SET_DEVICE_PIN_line0[lang], wcslen(SET_DEVICE_PIN_line0[lang]), COL_1_X, LINE_0_Y);
		writeUnderline(STRIPE_X_START, STRIPE_Y_START, STRIPE_X_END, STRIPE_Y_END);
		writeEinkDrawUnicodeSingle((unsigned int*)SET_PIN_line1[lang], wcslen(SET_PIN_line1[lang]), COL_1_X, LINE_1_Y);
		writeEinkDrawUnicodeSingle((unsigned int*)SET_PIN_line2[lang], wcslen(SET_PIN_line2[lang]), COL_1_X, LINE_2_Y);
		writeEinkNoDisplaySingle(passed_data, COL_1_X, LINE_3_Y);

		writeEinkDrawUnicodeSingle((unsigned int*)GO_line0[lang], wcslen(GO_line0[lang]), ACCEPT_X_START, LINE_4_Y);
		writeEinkDrawUnicodeSingle((unsigned int*)BACK_line0[lang], wcslen(BACK_line0[lang]), (DENY_X_START)-((zhSizer*wcslen(BACK_line0[lang]))*8), LINE_4_Y);

		drawX(draw_X_X,draw_X_Y);
		drawCheck(draw_check_X,draw_check_Y);
		display();

//		r = waitForButtonPress();
//		if (!r){
//			clearDisplay();
//		}else{
//			writeX_Screen();
//			showReady();
//		};
	}
	else if (command == ASKUSER_SET_DEVICE_PIN_BIG)
	{
		const wchar_t BACK_line0[][9] = {
				L"BACK", //EN
				L"ZURÜCK", //DE
				L"НАЗАД", //RU
				L"退回", //ZH
				L"ZPĚT", //CZ
				L"RETOUR", //FR
				L"ATRÁS", //ES
				L"COSTAS", //PT
				L"GERİ", //TU
				L"INDIETRO"  //IT
		} ;
		const wchar_t GO_line0[][12] = {
				L"GO", //EN
				L"GEHEN", //DE
				L"ПОДТВЕРДИТЬ", //RU
				L"确认", //ZH
				L"DÁLE", //CZ
				L"CONTINUER", //FR
				L"IR", //ES
				L"IR", //PT
				L"İLERLE", //TU
				L"GO"  //IT
		} ;
		const wchar_t SET_DEVICE_PIN_line0[][25] = {
				L"DEVICE PIN", //EN
				L"GERÄTE-PIN", //DE
				L"ПАРОЛЬ УСТРОЙСТВА", //RU
				L"设具密码", //ZH
				L"PIN ZAŘÍZENÍ", //CZ
				L"NIP DE L'APPAREIL", //FR
				L"CÓDIGO DEL DISPOSITIVO", //ES
				L"PIN DO DISPOSITIVO", //PT
				L"CİHAZ PIN", //TU
				L"PIN DEL DISPOSITIVO"  //IT
		} ;
		const wchar_t SET_PIN_line1[][25] = {
				L"PLEASE WRITE DOWN", //EN
				L"BITTE SCHREIBEN SIE", //DE
				L"ПОЖАЛУЙСТА, ЗАПИШИТЕ", //RU
				L"请记下", //ZH
				L"PROSÍM ZAPIŠTE SI", //CZ
				L"S'IL VOUS PLAÎT NOTER", //FR
				L"PORFAVOR TOME NOTA", //ES
				L"POR FAVOR, ANOTE", //PT
				L"LÜTFEN YAZIN", //TU
				L"PER FAVORE SCRIVI IL"  //IT
		} ;
		const wchar_t SET_PIN_line2[][25] = {
				L"YOUR PIN", //EN
				L"IHR PIN", //DE
				L"ПАРОЛЬ", //RU
				L"您的密码", //ZH
				L"VÁŠ PIN", //CZ
				L"VOTRE NIP", //FR
				L"EL CÓDIGO", //ES
				L"SEU PIN", //PT
				L"PIN KODUNUZ", //TU
				L"IL PIN"  //IT
		} ;


		waitForNoButtonPress();

		initDisplay();
	    overlayBatteryStatus(BATT_VALUE_DISPLAY);
		writeEinkDrawUnicodeSingle((unsigned int*)SET_DEVICE_PIN_line0[lang], wcslen(SET_DEVICE_PIN_line0[lang]), COL_1_X, LINE_0_Y);
		writeUnderline(STRIPE_X_START, STRIPE_Y_START, STRIPE_X_END, STRIPE_Y_END);
		writeEinkDrawUnicodeSingle((unsigned int*)SET_PIN_line1[lang], wcslen(SET_PIN_line1[lang]), COL_1_X, LINE_1_Y);
		writeEinkDrawUnicodeSingle((unsigned int*)SET_PIN_line2[lang], wcslen(SET_PIN_line2[lang]), COL_1_X, LINE_2_Y);
		writeEinkNoDisplaySingleBig(passed_data, 100-(size_of_data*8), LINE_3_Y-5);

		writeEinkDrawUnicodeSingle((unsigned int*)GO_line0[lang], wcslen(GO_line0[lang]), ACCEPT_X_START, LINE_4_Y);
		writeEinkDrawUnicodeSingle((unsigned int*)BACK_line0[lang], wcslen(BACK_line0[lang]), (DENY_X_START)-((zhSizer*wcslen(BACK_line0[lang]))*8), LINE_4_Y);

		drawX(draw_X_X,draw_X_Y);
		drawCheck(draw_check_X,draw_check_Y);
		display();

//		r = waitForButtonPress();
//		if (!r){
//			clearDisplay();
//		}else{
//			writeX_Screen();
//			showReady();
//		};
	}
	else if (command == ASKUSER_SET_WALLET_PIN)
	{
		const wchar_t BACK_line0[][9] = {
				L"BACK", //EN
				L"ZURÜCK", //DE
				L"НАЗАД", //RU
				L"退回", //ZH
				L"ZPĚT", //CZ
				L"RETOUR", //FR
				L"ATRÁS", //ES
				L"COSTAS", //PT
				L"GERİ", //TU
				L"INDIETRO"  //IT
		} ;
		const wchar_t GO_line0[][12] = {
				L"GO", //EN
				L"GEHEN", //DE
				L"ПОДТВЕРДИТЬ", //RU
				L"确认", //ZH
				L"DÁLE", //CZ
				L"CONTINUER", //FR
				L"IR", //ES
				L"IR", //PT
				L"İLERLE", //TU
				L"GO"  //IT
		} ;
		const wchar_t SET_DEVICE_PIN_line0[][25] = {
				L"DEVICE PIN", //EN
				L"GERÄTE-PIN", //DE
				L"ПАРОЛЬ УСТРОЙСТВА", //RU
				L"设具密码", //ZH
				L"PIN ZAŘÍZENÍ", //CZ
				L"NIP DE L'APPAREIL", //FR
				L"CÓDIGO DEL DISPOSITIVO", //ES
				L"PIN DO DISPOSITIVO", //PT
				L"CİHAZ PIN", //TU
				L"PIN DEL DISPOSITIVO"  //IT
		} ;

		const wchar_t SET_WALLET_PIN_line0[][25] = {
				L"WALLET PIN", //EN
				L"WALLET PIN", //DE
				L"ПАРОЛЬ КОШЕЛЬКА", //RU
				L"钱袋密码", //ZH
				L"PIN PENĚŽENKY", //CZ
				L"NIP DU PORTEFEUILLE", //FR
				L"CÓDIGO DE LA CARTERA", //ES
				L"PIN CARTEIRA", //PT
				L"CÜZDAN PIN KODU", //TU
				L"PIN PORTAFOGLIO"  //IT
		} ;

		const wchar_t SET_PIN_line1[][25] = {
				L"PLEASE WRITE DOWN", //EN
				L"BITTE SCHREIBEN SIE", //DE
				L"ПОЖАЛУЙСТА, ЗАПИШИТЕ", //RU
				L"请记下", //ZH
				L"PROSÍM ZAPIŠTE SI", //CZ
				L"S'IL VOUS PLAÎT NOTER", //FR
				L"PORFAVOR TOME NOTA", //ES
				L"POR FAVOR, ANOTE", //PT
				L"LÜTFEN YAZIN", //TU
				L"PER FAVORE SCRIVI IL"  //IT
		} ;
		const wchar_t SET_PIN_line2[][25] = {
				L"YOUR PIN", //EN
				L"IHR PIN", //DE
				L"ПАРОЛЬ", //RU
				L"您的密码", //ZH
				L"VÁŠ PIN", //CZ
				L"VOTRE NIP", //FR
				L"EL CÓDIGO", //ES
				L"SEU PIN", //PT
				L"PIN KODUNUZ", //TU
				L"IL PIN"  //IT
		} ;

		waitForNoButtonPress();

		initDisplay();
	    overlayBatteryStatus(BATT_VALUE_DISPLAY);
		writeEinkDrawUnicodeSingle((unsigned int*)SET_WALLET_PIN_line0[lang], wcslen(SET_DEVICE_PIN_line0[lang]), COL_1_X, LINE_0_Y);
		writeUnderline(STRIPE_X_START, STRIPE_Y_START, STRIPE_X_END, STRIPE_Y_END);
		writeEinkDrawUnicodeSingle((unsigned int*)SET_PIN_line1[lang], wcslen(SET_PIN_line1[lang]), COL_1_X, LINE_1_Y);
		writeEinkDrawUnicodeSingle((unsigned int*)SET_PIN_line2[lang], wcslen(SET_PIN_line2[lang]), COL_1_X, LINE_2_Y);
		writeEinkNoDisplaySingle(passed_data, COL_1_X, LINE_3_Y);

		writeEinkDrawUnicodeSingle((unsigned int*)GO_line0[lang], wcslen(GO_line0[lang]), ACCEPT_X_START, LINE_4_Y);
		writeEinkDrawUnicodeSingle((unsigned int*)BACK_line0[lang], wcslen(BACK_line0[lang]), (DENY_X_START)-((zhSizer*wcslen(BACK_line0[lang]))*8), LINE_4_Y);

		drawX(draw_X_X,draw_X_Y);
		drawCheck(draw_check_X,draw_check_Y);
		display();

//		r = waitForButtonPress();
//		if (!r){
//			clearDisplay();
//		}else{
//			writeX_Screen();
//			showReady();
//		};
	}
	else if (command == ASKUSER_SET_WALLET_PIN_BIG)
	{
		const wchar_t BACK_line0[][9] = {
				L"BACK", //EN
				L"ZURÜCK", //DE
				L"НАЗАД", //RU
				L"退回", //ZH
				L"ZPĚT", //CZ
				L"RETOUR", //FR
				L"ATRÁS", //ES
				L"COSTAS", //PT
				L"GERİ", //TU
				L"INDIETRO"  //IT
		} ;
		const wchar_t GO_line0[][12] = {
				L"GO", //EN
				L"GEHEN", //DE
				L"ПОДТВЕРДИТЬ", //RU
				L"确认", //ZH
				L"DÁLE", //CZ
				L"CONTINUER", //FR
				L"IR", //ES
				L"IR", //PT
				L"İLERLE", //TU
				L"GO"  //IT
		} ;

		const wchar_t SET_DEVICE_PIN_line0[][25] = {
				L"DEVICE PIN", //EN
				L"GERÄTE-PIN", //DE
				L"ПАРОЛЬ УСТРОЙСТВА", //RU
				L"设具密码", //ZH
				L"PIN ZAŘÍZENÍ", //CZ
				L"NIP DE L'APPAREIL", //FR
				L"CÓDIGO DEL DISPOSITIVO", //ES
				L"PIN DO DISPOSITIVO", //PT
				L"CİHAZ PIN", //TU
				L"PIN DEL DISPOSITIVO"  //IT
		} ;
		const wchar_t SET_WALLET_PIN_line0[][25] = {
				L"WALLET PIN", //EN
				L"WALLET PIN", //DE
				L"ПАРОЛЬ КОШЕЛЬКА", //RU
				L"钱袋密码", //ZH
				L"PIN PENĚŽENKY", //CZ
				L"NIP DU PORTEFEUILLE", //FR
				L"CÓDIGO DE LA CARTERA", //ES
				L"PIN CARTEIRA", //PT
				L"CÜZDAN PIN KODU", //TU
				L"PIN PORTAFOGLIO"  //IT
		} ;

		const wchar_t SET_PIN_line1[][25] = {
				L"PLEASE WRITE DOWN", //EN
				L"BITTE SCHREIBEN SIE", //DE
				L"ПОЖАЛУЙСТА, ЗАПИШИТЕ", //RU
				L"请记下", //ZH
				L"PROSÍM ZAPIŠTE SI", //CZ
				L"S'IL VOUS PLAÎT NOTER", //FR
				L"PORFAVOR TOME NOTA", //ES
				L"POR FAVOR, ANOTE", //PT
				L"LÜTFEN YAZIN", //TU
				L"PER FAVORE SCRIVI IL"  //IT
		} ;
		const wchar_t SET_PIN_line2[][25] = {
				L"YOUR PIN", //EN
				L"IHR PIN", //DE
				L"ПАРОЛЬ", //RU
				L"您的密码", //ZH
				L"VÁŠ PIN", //CZ
				L"VOTRE NIP", //FR
				L"EL CÓDIGO", //ES
				L"SEU PIN", //PT
				L"PIN KODUNUZ", //TU
				L"IL PIN"  //IT
		} ;

		waitForNoButtonPress();


		initDisplay();
	    overlayBatteryStatus(BATT_VALUE_DISPLAY);
		writeEinkDrawUnicodeSingle((unsigned int*)SET_WALLET_PIN_line0[lang], wcslen(SET_DEVICE_PIN_line0[lang]), COL_1_X, LINE_0_Y);
		writeUnderline(STRIPE_X_START, STRIPE_Y_START, STRIPE_X_END, STRIPE_Y_END);
		writeEinkDrawUnicodeSingle((unsigned int*)SET_PIN_line1[lang], wcslen(SET_PIN_line1[lang]), COL_1_X, LINE_1_Y);
		writeEinkDrawUnicodeSingle((unsigned int*)SET_PIN_line2[lang], wcslen(SET_PIN_line2[lang]), COL_1_X, LINE_2_Y);
		writeEinkNoDisplaySingleBig(passed_data, 100-(size_of_data*8), LINE_3_Y-5); //r has to be sorted out

		writeEinkDrawUnicodeSingle((unsigned int*)GO_line0[lang], wcslen(GO_line0[lang]), ACCEPT_X_START, LINE_4_Y);
		writeEinkDrawUnicodeSingle((unsigned int*)BACK_line0[lang], wcslen(BACK_line0[lang]), (DENY_X_START)-((zhSizer*wcslen(BACK_line0[lang]))*8), LINE_4_Y);

		drawX(draw_X_X,draw_X_Y);
		drawCheck(draw_check_X,draw_check_Y);
		display();

//		r = waitForButtonPress();
//		if (!r){
//			clearDisplay();
//		}else{
//			writeX_Screen();
//			showReady();
//		};
	}
	else if (command == ASKUSER_SHOW_DISPLAYPHRASE)
	{
		const wchar_t CANCEL_line0[][9] = {
				L"CANCEL", //EN
				L"ABSAGEN", //DE
				L"ОТМЕНА", //RU
				L"退回", //ZH
				L"ZRUŠIT", //CZ
				L"ANNULER", //FR
				L"CANCELAR", //ES
				L"CANCELAR", //PT
				L"İPTAL", //TU
				L"ANNULLA"  //IT
		} ;
		const wchar_t CONFIRM_line0[][12] = {
				L"CONFIRM", //EN
				L"BESTÄTIGEN", //DE
				L"ПОДТВЕРДИТЬ", //RU
				L"确认", //ZH
				L"POTVRDIT", //CZ
				L"CONFIRMER", //FR
				L"CONFIRMAR", //ES
				L"CONFIRMAR", //PT
				L"ONAYLA", //TU
				L"CONFERMARE"  //IT
		} ;


		const wchar_t udpdAEM_DISPLAYPHRASE_line0[][25] = {
				L"AEM PROTECTION SETUP", //EN
				L"AEM SCHUTZKONFIGURATION", //DE
				L"УСТАНОВКА AEM ЗАЩИТЫ", //RU
				L"AEM防护设置", //ZH
				L"AEM KONFIGURACE", //CZ
				L"CONFIGURATION AEM", //FR
				L"CONFIGURACIÓN AEM", //ES
				L"AEM CONFIGURAÇÃO", //PT
				L"AEM KORUMA KURULUMU", //TU
				L"IMPOSTAZIONI AEM"  //IT
		} ;
		const wchar_t SHOW_DISPLAYPHRASE_line1[][25] = {
				L"MEMORIZE THE SECRET", //EN
				L"VERMERKEN SIE IHRE", //DE
				L"ЗАПОМНИТЕ УСТАНОВЛЕННУЮ", //RU
				L"请记牢您所设", //ZH
				L"POZNAMENEJTE SI FRÁZI,", //CZ
				L"MÉMORISEZ", //FR
				L"MEMORIZE", //ES
				L"MEMORIZAR FRASES", //PT
				L"AYARLADIĞINIZ", //TU
				L"MEMORIZZA LA FRASE CHE"  //IT
		} ;
		const wchar_t SHOW_DISPLAYPHRASE_line2[][25] = {
				L"PHRASE YOU HAVE SET", //EN
				L"GEHEIM PHRASE", //DE
				L"СЕКРЕТНУЮ ФРАЗУ", //RU
				L"置的秘密短语", //ZH
				L"KTEROU JSTE NASTAVILI", //CZ
				L"LA PHRASE SECRÈTE", //FR
				L"LA FRASE SECRETA", //ES
				L"VOCÊ TEM DE DEFINIR", //PT
				L"GİZLİ KELİMELERİ", //TU
				L"È STATA IMPOSTATA"  //IT
		} ;

		const wchar_t SHOW_DISPLAYPHRASE_line3[][25] = {
				L"", //EN
				L"", //DE
				L"", //RU
				L"", //ZH
				L"", //CZ
				L"QUE VOUS AVEZ DÉFINIE", //FR
				L"QUE HA ESTABLECIDO", //ES
				L"", //PT
				L"SAKLAYIN", //TU
				L""  //IT
		} ;


		waitForNoButtonPress();


		initDisplay();
	    overlayBatteryStatus(BATT_VALUE_DISPLAY);
		writeEinkDrawUnicodeSingle((unsigned int*)udpdAEM_DISPLAYPHRASE_line0[lang], wcslen(udpdAEM_DISPLAYPHRASE_line0[lang]), COL_1_X, LINE_0_Y);
		writeUnderline(STRIPE_X_START, STRIPE_Y_START, STRIPE_X_END, STRIPE_Y_END);
		writeEinkDrawUnicodeSingle((unsigned int*)SHOW_DISPLAYPHRASE_line1[lang], wcslen(SHOW_DISPLAYPHRASE_line1[lang]), COL_1_X, LINE_1_Y);
		writeEinkDrawUnicodeSingle((unsigned int*)SHOW_DISPLAYPHRASE_line2[lang], wcslen(SHOW_DISPLAYPHRASE_line2[lang]), COL_1_X, LINE_2_Y);
		writeEinkDrawUnicodeSingle((unsigned int*)SHOW_DISPLAYPHRASE_line3[lang], wcslen(SHOW_DISPLAYPHRASE_line3[lang]), COL_1_X, LINE_3_Y);
		writeEinkNoDisplaySingle(passed_data, COL_1_X, LINE_3_Y);

		writeEinkDrawUnicodeSingle((unsigned int*)CONFIRM_line0[lang], wcslen(CONFIRM_line0[lang]), ACCEPT_X_START, LINE_4_Y);
		writeEinkDrawUnicodeSingle((unsigned int*)CANCEL_line0[lang], wcslen(CANCEL_line0[lang]), (DENY_X_START)-((zhSizer*wcslen(CANCEL_line0[lang]))*8), LINE_4_Y);

		drawX(draw_X_X,draw_X_Y);
		drawCheck(draw_check_X,draw_check_Y);
		display();

		r = waitForButtonPress();
		if (!r){
			clearDisplay();
		}else{
			writeX_Screen();
//			showReady();
		};
	}
	else if (command == ASKUSER_SHOW_UNLOCKPHRASE)
	{
		const wchar_t CANCEL_line0[][9] = {
				L"CANCEL", //EN
				L"ABSAGEN", //DE
				L"ОТМЕНА", //RU
				L"退回", //ZH
				L"ZRUŠIT", //CZ
				L"ANNULER", //FR
				L"CANCELAR", //ES
				L"CANCELAR", //PT
				L"İPTAL", //TU
				L"ANNULLA"  //IT
		} ;
		const wchar_t CONFIRM_line0[][12] = {
				L"CONFIRM", //EN
				L"BESTÄTIGEN", //DE
				L"ПОДТВЕРДИТЬ", //RU
				L"确认", //ZH
				L"POTVRDIT", //CZ
				L"CONFIRMER", //FR
				L"CONFIRMAR", //ES
				L"CONFIRMAR", //PT
				L"ONAYLA", //TU
				L"CONFERMARE"  //IT
		} ;
		const wchar_t udpdAEM_DISPLAYPHRASE_line0[][25] = {
				L"AEM PROTECTION SETUP", //EN
				L"AEM SCHUTZKONFIGURATION", //DE
				L"УСТАНОВКА AEM ЗАЩИТЫ", //RU
				L"AEM防护设置", //ZH
				L"AEM KONFIGURACE", //CZ
				L"CONFIGURATION AEM", //FR
				L"CONFIGURACIÓN AEM", //ES
				L"AEM CONFIGURAÇÃO", //PT
				L"AEM KORUMA KURULUMU", //TU
				L"IMPOSTAZIONI AEM"  //IT
		} ;
		const wchar_t SHOW_UNLOCKPHRASE_line1[][25] = {
				L"MEMORIZE THE PHRASE", //EN
				L"VERMERKEN SIE DIE PHRASE", //DE
				L"ЗАПОМНИТЕ ФРАЗУ", //RU
				L"请记牢您所设置", //ZH
				L"POZNAMENEJTE SI FRÁZI", //CZ
				L"MÉMORISEZ LA PHRASE", //FR
				L"MEMORIZE LA FRASE", //ES
				L"MEMORIZAR FRASES", //PT
				L"ŞİFREYİ ÇÖZMEK İÇİN", //TU
				L"MEMORIZZA LA FRASE"  //IT
		} ;

		const wchar_t SHOW_UNLOCKPHRASE_line2[][25] = {
				L"FOR DECRYPTING", //EN
				L"ZUM ENTSCHLÜSSELN", //DE
				L"ДЛЯ РАСШИФРОВКИ", //RU
				L"的自定义解锁密码", //ZH
				L"PRO DEŠIFROVÁNÍ", //CZ
				L"POUR DÉCRYPTER", //FR
				L"PARA DESCIFRAR", //ES
				L"PARA DESENCRIPTAR", //PT
				L"KELİMELERİ SAKLAYIN", //TU
				L"PER DECIFRARE"  //IT
		} ;

		waitForNoButtonPress();


		initDisplay();
	    overlayBatteryStatus(BATT_VALUE_DISPLAY);
		writeEinkDrawUnicodeSingle((unsigned int*)udpdAEM_DISPLAYPHRASE_line0[lang], wcslen(udpdAEM_DISPLAYPHRASE_line0[lang]), COL_1_X, LINE_0_Y);
		writeUnderline(STRIPE_X_START, STRIPE_Y_START, STRIPE_X_END, STRIPE_Y_END);
		writeEinkDrawUnicodeSingle((unsigned int*)SHOW_UNLOCKPHRASE_line1[lang], wcslen(SHOW_UNLOCKPHRASE_line1[lang]), COL_1_X, LINE_1_Y);
		writeEinkDrawUnicodeSingle((unsigned int*)SHOW_UNLOCKPHRASE_line2[lang], wcslen(SHOW_UNLOCKPHRASE_line2[lang]), COL_1_X, LINE_2_Y);
		writeEinkNoDisplaySingle(passed_data, COL_1_X, LINE_3_Y);

		writeEinkDrawUnicodeSingle((unsigned int*)CONFIRM_line0[lang], wcslen(CONFIRM_line0[lang]), ACCEPT_X_START, LINE_4_Y);
		writeEinkDrawUnicodeSingle((unsigned int*)CANCEL_line0[lang], wcslen(CANCEL_line0[lang]), (DENY_X_START)-((zhSizer*wcslen(CANCEL_line0[lang]))*8), LINE_4_Y);

		drawX(draw_X_X,draw_X_Y);
		drawCheck(draw_check_X,draw_check_Y);
		display();

		r = waitForButtonPress();
		if (!r){
			clearDisplay();
		}else{
			writeX_Screen();
//			showReady();
		};
	}
	else if (command == ASKUSER_TRANSACTION_PIN_SET)
	{
		const wchar_t CANCEL_line0[][9] = {
				L"CANCEL", //EN
				L"ABSAGEN", //DE
				L"ОТМЕНА", //RU
				L"退回", //ZH
				L"ZRUŠIT", //CZ
				L"ANNULER", //FR
				L"CANCELAR", //ES
				L"CANCELAR", //PT
				L"İPTAL", //TU
				L"ANNULLA"  //IT
		} ;
		const wchar_t GO_line0[][12] = {
				L"GO", //EN
				L"GEHEN", //DE
				L"ПОДТВЕРДИТЬ", //RU
				L"确认", //ZH
				L"DÁLE", //CZ
				L"CONTINUER", //FR
				L"IR", //ES
				L"IR", //PT
				L"İLERLE", //TU
				L"GO"  //IT
		} ;
		const wchar_t udpdDESCRIBE_EXPERT_SETUP_line0[][25] = {
				L"EXPERT SETUP", //EN
				L"EXPERT KONFIGURATION", //DE
				L"ЭКСПЕРТНЫЕ НАСТРОЙКИ", //RU
				L"专家设置", //ZH
				L"EXPERT KONFIGURACE", //CZ
				L"CONFIGURATION EXPERT", //FR
				L"CONFIGURACIÓN EXPERTO", //ES
				L"INSTALAÇÃO DE ESPECIAL.", //PT
				L"UZMAN KURULUMU", //TU
				L"IMPOSTAZIONIE ESPERTO"  //IT
		} ;
		const wchar_t TRANSACTION_PIN_SET_line1[][25] = {
				L"SETTING TRANSACTION PIN:", //EN
				L"EINSTELLUNG DER", //DE
				L"УСТАНОВКА ПАРОЛЯ", //RU
				L"设置交易密码", //ZH
				L"PIN TRANSAKCE NASTAVEN:", //CZ
				L"CONFIGURATION DU NIP", //FR
				L"CREAR CÓDIGO", //ES
				L"DEFINIÇÃO DE", //PT
				L"İŞLEM PIN", //TU
				L"IMPOSTAZIONE"  //IT
		} ;



		waitForNoButtonPress();


		initDisplay();
//	    overlayBatteryStatus(BATT_VALUE_DISPLAY);
		writeEinkDrawUnicodeSingle((unsigned int*)udpdDESCRIBE_EXPERT_SETUP_line0[lang], wcslen(udpdDESCRIBE_EXPERT_SETUP_line0[lang]), COL_1_X, LINE_0_Y);
		writeUnderline(STRIPE_X_START, STRIPE_Y_START, STRIPE_X_END, STRIPE_Y_END);
		writeEinkDrawUnicodeSingle((unsigned int*)TRANSACTION_PIN_SET_line1[lang], wcslen(TRANSACTION_PIN_SET_line1[lang]), COL_1_X, LINE_1_Y);
		writeEinkNoDisplaySingle(passed_data, COL_1_X, LINE_3_Y);
		writeEinkDrawUnicodeSingle((unsigned int*)GO_line0[lang], wcslen(GO_line0[lang]), ACCEPT_X_START, LINE_4_Y);
		writeEinkDrawUnicodeSingle((unsigned int*)CANCEL_line0[lang], wcslen(CANCEL_line0[lang]), (DENY_X_START)-((zhSizer*wcslen(CANCEL_line0[lang]))*8), LINE_4_Y);

		drawX(draw_X_X,draw_X_Y);
		drawCheck(draw_check_X,draw_check_Y);
		display();

		r = waitForButtonPress();
		if (!r){
			clearDisplay();
		}else{
			writeX_Screen();
//			showReady();
		};
	}
	else if (command == ASKUSER_CONFIRM_HIDDEN_WALLET_NUMBER)
	{
		const wchar_t CANCEL_line0[][9] = {
				L"CANCEL", //EN
				L"ABSAGEN", //DE
				L"ОТМЕНА", //RU
				L"退回", //ZH
				L"ZRUŠIT", //CZ
				L"ANNULER", //FR
				L"CANCELAR", //ES
				L"CANCELAR", //PT
				L"İPTAL", //TU
				L"ANNULLA"  //IT
		} ;
		const wchar_t CONFIRM_line0[][12] = {
				L"CONFIRM", //EN
				L"BESTÄTIGEN", //DE
				L"ПОДТВЕРДИТЬ", //RU
				L"确认", //ZH
				L"POTVRDIT", //CZ
				L"CONFIRMER", //FR
				L"CONFIRMAR", //ES
				L"CONFIRMAR", //PT
				L"ONAYLA", //TU
				L"CONFERMARE"  //IT
		} ;

		const wchar_t CONFIRM_HIDDEN_WALLET_NUMBER_line1[][25] = {
				L"USE THIS WALLET NUMBER?", //EN
				L"NUTZEN SIE DIESE", //DE
				L"ИСПОЛЬЗОВАТЬ ЭТОТ", //RU
				L"用这个钱袋是多少?", //ZH
				L"TUTO PENĚŽENKA ČÍSLO?", //CZ
				L"UTILISEZ CE NUMÉRO", //FR
				L"UTILICE ESTE", //ES
				L"USE ESTE", //PT
				L"BU CÜZDAN", //TU
				L"UTILIZZARE QUESTO"  //IT
		} ;

		waitForNoButtonPress();


		initDisplay();
//	    overlayBatteryStatus(BATT_VALUE_DISPLAY);
//		writeEinkDrawUnicodeSingle((unsigned int*)CONFIRM_HIDDEN_WALLET_NUMBER_line0[lang], wcslen(CONFIRM_HIDDEN_WALLET_NUMBER_line0[lang]), COL_1_X, LINE_0_Y);
		writeUnderline(STRIPE_X_START, STRIPE_Y_START, STRIPE_X_END, STRIPE_Y_END);
		writeEinkDrawUnicodeSingle((unsigned int*)CONFIRM_HIDDEN_WALLET_NUMBER_line1[lang], wcslen(CONFIRM_HIDDEN_WALLET_NUMBER_line1[lang]), COL_1_X, LINE_1_Y);
		writeEinkNoDisplaySingleBig(passed_data, COL_1_X, LINE_2_Y);
		writeEinkDrawUnicodeSingle((unsigned int*)CONFIRM_line0[lang], wcslen(CONFIRM_line0[lang]), ACCEPT_X_START, LINE_4_Y);
		writeEinkDrawUnicodeSingle((unsigned int*)CANCEL_line0[lang], wcslen(CANCEL_line0[lang]), (DENY_X_START)-((zhSizer*wcslen(CANCEL_line0[lang]))*8), LINE_4_Y);

		drawX(draw_X_X,draw_X_Y);
		drawCheck(draw_check_X,draw_check_Y);
		display();

		r = waitForButtonPress();
		if (!r){
			clearDisplay();
		}else{
			writeX_Screen();
			showReady();
		};
	}
	return r;
}


//#####################################################################################
//#####################################################################################
//#####################################################################################

/** Ask user if they want to allow some action.
  * \param command The action to ask the user about. See #AskUserCommandEnum.
  * \return false if the user accepted, true if the user denied.
  */
bool userDenied(AskUserCommand command)
{
	uint8_t i;
	uint8_t tempLang[1];
	nonVolatileRead(tempLang, DEVICE_LANG_ADDRESS, 1);

	int lang;
	lang = (int)tempLang[0];

	int zhSizer = 1;

	if (lang == 3)
	{
		zhSizer = 2;
	}


	bool r; // what will be returned

	r = true;
	if (command == ASKUSER_NEW_WALLET_2)
	{
		const wchar_t NEW_WALLET_line0[][25] = {
				L"NEW WALLET", //EN
				L"NEUE WALLET", //DE
				L"НОВЫЙ КОШЕЛЕК", //RU
				L"新钱袋", //ZH
				L"NOVÁ PENĚŽENKA", //CZ
				L"NOUVELLE PORTEFEUILLE", //FR
				L"NUEVA CARTERA", //ES
				L"NOVA CARTEIRA", //PT
				L"YENİ CÜZDAN", //TU
				L"NUOVO PORTAFOGLIO"  //IT
		} ;

		const wchar_t NEW_WALLET_line1[][25] = {
				L"CREATE A NEW WALLET?", //EN
				L"NEUE WALLET ERSTELLEN?", //DE
				L"СОЗДАТЬ НОВЫЙ КОШЕЛЕК?", //RU
				L"要创建新钱袋吗？", //ZH
				L"VYTVOŘIT PENĚŽENKU?", //CZ
				L"CRÉER UN PORTEFEUILLE?", //FR
				L"CREAR UNA CARTERA?", //ES
				L"CRIAR UMA CARTEIRA?", //PT
				L"YENİ CÜZDAN OLUŞTUR?", //TU
				L"CREA UN PORTAFOGLIO?"  //IT
		} ;
		const wchar_t udCANCEL_line0[][9] = {
				L"CANCEL", //EN
				L"ABSAGEN", //DE
				L"ОТМЕНА", //RU
				L"退回", //ZH
				L"ZRUŠIT", //CZ
				L"ANNULER", //FR
				L"CANCELAR", //ES
				L"CANCELAR", //PT
				L"İPTAL", //TU
				L"ANNULLA"  //IT
		} ;

		const wchar_t udACCEPT_line0[][12] = {
				L"ACCEPT", //EN
				L"AKZEPTIEREN", //DE
				L"ПОДТВЕРДИТЬ", //RU
				L"确认", //ZH
				L"POTVRDIT", //CZ
				L"ACCEPTER", //FR
				L"ACEPTAR", //ES
				L"ACEITAR", //PT
				L"KABUL ET", //TU
				L"ACCETTA"  //IT
		} ;


		waitForNoButtonPress();

		initDisplay();
	    overlayBatteryStatus(BATT_VALUE_DISPLAY);
		writeEinkDrawUnicodeSingle((unsigned int*)NEW_WALLET_line0[lang], wcslen(NEW_WALLET_line0[lang]), COL_1_X, LINE_0_Y);
		writeUnderline(STRIPE_X_START, STRIPE_Y_START, STRIPE_X_END, STRIPE_Y_END);
		writeEinkDrawUnicodeSingle((unsigned int*)NEW_WALLET_line1[lang], wcslen(NEW_WALLET_line1[lang]), COL_1_X, LINE_1_Y);
//		writeEinkDrawUnicodeSingle((unsigned int*)NEW_WALLET_line2[lang], wcslen(NEW_WALLET_line2[lang]), COL_1_X, LINE_2_Y);
		writeEinkDrawUnicodeSingle((unsigned int*)udACCEPT_line0[lang], wcslen(udACCEPT_line0[lang]), ACCEPT_X_START, LINE_4_Y);
		writeEinkDrawUnicodeSingle((unsigned int*)udCANCEL_line0[lang], wcslen(udCANCEL_line0[lang]), (DENY_X_START)-((zhSizer*wcslen(udCANCEL_line0[lang]))*8), LINE_4_Y);

		drawX(draw_X_X,draw_X_Y);
		drawCheck(draw_check_X,draw_check_Y);
		display();

		r = waitForButtonPress();
		if (!r){
			clearDisplay();
		}else{
			writeX_Screen();
			showReady();
		};
	}
	else if (command == ASKUSER_DELETE_WALLET)
	{
		const wchar_t DELETE_WALLET_line0[][25] = {
				L"DELETE WALLET", //EN
				L"LÖSCHEN GELDBÖRSEN", //DE
				L"УДАЛ БУМАЖНИКА", //RU
				L"删除钱袋", //ZH
				L"ODSTRANIT PENĚŽENKY", //CZ
				L"SUPPRIMER PORTEFEUILLE", //FR
				L"BORRAR LA CARPETA", //ES
				L"APAGAR CARTEIRA", //PT
				L"WALLET SİL", //TU
				L"DELETE PORTAFOGLIO"  //IT
		} ;

		const wchar_t DELETE_WALLET_line1[][25] = {
				L"ERASE WALLET AND", //EN
				L"LÖSCHEN GELDBÖRSEN", //DE
				L"УДАЛ БУМАЖНИК", //RU
				L"擦除钱袋和所有内容吗？", //ZH
				L"ODSTRANÍ PENĚŽENKU", //CZ
				L"ERASE PORTEFEUILLE", //FR
				L"BORRAR CARPETA", //ES
				L"ERASE CARTEIRA", //PT
				L"BT CÜZDAN VE HER", //TU
				L"CANCELLARE PORTAFOGLIO"  //IT
		} ;

		const wchar_t DELETE_WALLET_line2[][25] = {
				L"EVERYTHING IN IT?", //EN
				L"UND ALLES IN IHR?", //DE
				L"И ВСЕ В НЕМ?", //RU
				L"ZH", //ZH
				L"A VŠECHNO V NĚM?", //CZ
				L"ET TOUT CE QU'IL?", //FR
				L"Y TODO EN ÉL?", //ES
				L"E TUDO NELE?", //PT
				L"ŞEY SİLECEKTİR?", //TU
				L"E TUTTO CIÒ CHE?"  //IT
		} ;
		const wchar_t udCANCEL_line0[][9] = {
				L"CANCEL", //EN
				L"ABSAGEN", //DE
				L"ОТМЕНА", //RU
				L"退回", //ZH
				L"ZRUŠIT", //CZ
				L"ANNULER", //FR
				L"CANCELAR", //ES
				L"CANCELAR", //PT
				L"İPTAL", //TU
				L"ANNULLA"  //IT
		} ;
		const wchar_t udACCEPT_line0[][12] = {
				L"ACCEPT", //EN
				L"AKZEPTIEREN", //DE
				L"ПОДТВЕРДИТЬ", //RU
				L"确认", //ZH
				L"POTVRDIT", //CZ
				L"ACCEPTER", //FR
				L"ACEPTAR", //ES
				L"ACEITAR", //PT
				L"KABUL ET", //TU
				L"ACCETTA"  //IT
		} ;


		waitForNoButtonPress();// comment this out and it just flies through without waiting for a YES...

		initDisplay();
	    overlayBatteryStatus(BATT_VALUE_DISPLAY);
		writeEinkDrawUnicodeSingle((unsigned int*)DELETE_WALLET_line0[lang], wcslen(DELETE_WALLET_line0[lang]), COL_1_X, LINE_0_Y);
		writeUnderline(STRIPE_X_START, STRIPE_Y_START, STRIPE_X_END, STRIPE_Y_END);
		writeEinkDrawUnicodeSingle((unsigned int*)DELETE_WALLET_line1[lang], wcslen(DELETE_WALLET_line1[lang]), COL_1_X, LINE_1_Y);
		writeEinkDrawUnicodeSingle((unsigned int*)DELETE_WALLET_line2[lang], wcslen(DELETE_WALLET_line2[lang]), COL_1_X, LINE_2_Y);
		writeEinkDrawUnicodeSingle((unsigned int*)udACCEPT_line0[lang], wcslen(udACCEPT_line0[lang]), ACCEPT_X_START, LINE_4_Y);
		writeEinkDrawUnicodeSingle((unsigned int*)udCANCEL_line0[lang], wcslen(udCANCEL_line0[lang]), (DENY_X_START)-((zhSizer*wcslen(udCANCEL_line0[lang]))*8), LINE_4_Y);

		drawX(draw_X_X,draw_X_Y);
		drawCheck(draw_check_X,draw_check_Y);
		display();

		r = waitForButtonPress();
		if (!r){
			showWorking();
		}else{
			showDenied();
		};
	}
	else if (command == ASKUSER_USE_MNEMONIC_PASSPHRASE)
	{
		const wchar_t USE_MNEMONIC_PASSPHRASE_line0[][25] = {
				L"MNEMONIC PASSPHRASE", //EN
				L"MNEMONIC-PASSWORT", //DE
				L"МНЕМОНИКА ПАРОЛЬ", //RU
				L"助记符密码", //ZH
				L"MNEMONICKÉ HESLO", //CZ
				L"MOT DE PASSE MNEMONIC", //FR
				L"CONTRASEÑA MNEMÓNICO", //ES
				L"SENHA MNEMÔNICO", //PT
				L"ANIMSATICI ŞIFRE", //TU
				L"PASSWORD MNEMONICO"  //IT
		} ;

		const wchar_t USE_MNEMONIC_PASSPHRASE_line1[][25] = {
				L"USE PASSPHRASE", //EN
				L"VERWENDEN PASSWORT", //DE
				L"ИСПОЛЬЗОВАТЬ ПАРОЛЬ ДЛЯ", //RU
				L"使用密码来保护记忆?", //ZH
				L"POUŽÍVAT HESLO", //CZ
				L"UTILISER UN MOT DE PASSE", //FR
				L"UTILIZAR CONTRASEÑA ", //ES
				L"UTILIZAR SENHA PARA", //PT
				L"ANIMSATICI GÜVENLİ", //TU
				L"UTILIZZARE PASSWORD PER"  //IT
		} ;

		const wchar_t USE_MNEMONIC_PASSPHRASE_line2[][25] = {
				L"TO SECURE MNEMONIC?", //EN
				L"MNEMONIC ZU SICHERN?", //DE
				L"ОБЕСПЕЧЕНИЯ", //RU
				L"", //ZH
				L"K ZAJIŠTĚNÍ", //CZ
				L"POUR SÉCURISER", //FR
				L"PARA ASEGURAR MNEMÓNICO?", //ES
				L"PROTEGER MNEMÔNICO?", //PT
				L"ŞİFRE KULLANIN?", //TU
				L"PROTEGGERE MNEMONICO?"  //IT
		} ;
		const wchar_t USE_MNEMONIC_PASSPHRASE_line3[][25] = {
				L"", //EN
				L"", //DE
				L"МНЕМОНИЧЕСКИЕ?", //RU
				L"", //ZH
				L"MNEMOTECHNICKÁ POMŮCKA?", //CZ
				L"MNÉMOTECHNIQUE?", //FR
				L"", //ES
				L"", //PT
				L"", //TU
				L""  //IT
		} ;
		const wchar_t udYES_line0[][5] = {
					L"YES", //EN
					L"JA", //DE
					L"ДА", //RU
					L"是", //ZH
					L"ANO", //CZ
					L"OUI", //FR
					L"SÍ", //ES
					L"SIM", //PT
					L"EVET", //TU
					L"SÌ"  //IT
			} ;
		const wchar_t udNO_line0[][6] = {
				L"NO", //EN
				L"NEIN", //DE
				L"НЕТ", //RU
				L"否", //ZH
				L"NE", //CZ
				L"NON", //FR
				L"NO", //ES
				L"NÃO", //PT
				L"HAYIR", //TU
				L"NO"  //IT
		};

		waitForNoButtonPress();// comment this out and it just flies through without waiting for a YES...

		initDisplay();
	    overlayBatteryStatus(BATT_VALUE_DISPLAY);
		writeEinkDrawUnicodeSingle((unsigned int*)USE_MNEMONIC_PASSPHRASE_line0[lang], wcslen(USE_MNEMONIC_PASSPHRASE_line0[lang]), COL_1_X, LINE_0_Y);
		writeUnderline(STRIPE_X_START, STRIPE_Y_START, STRIPE_X_END, STRIPE_Y_END);
		writeEinkDrawUnicodeSingle((unsigned int*)USE_MNEMONIC_PASSPHRASE_line1[lang], wcslen(USE_MNEMONIC_PASSPHRASE_line1[lang]), COL_1_X, LINE_1_Y);
		writeEinkDrawUnicodeSingle((unsigned int*)USE_MNEMONIC_PASSPHRASE_line2[lang], wcslen(USE_MNEMONIC_PASSPHRASE_line2[lang]), COL_1_X, LINE_2_Y);
		writeEinkDrawUnicodeSingle((unsigned int*)USE_MNEMONIC_PASSPHRASE_line3[lang], wcslen(USE_MNEMONIC_PASSPHRASE_line3[lang]), COL_1_X, LINE_3_Y);
		writeEinkDrawUnicodeSingle((unsigned int*)udYES_line0[lang], wcslen(udYES_line0[lang]), ACCEPT_X_START, LINE_4_Y);
		writeEinkDrawUnicodeSingle((unsigned int*)udNO_line0[lang], wcslen(udNO_line0[lang]), (DENY_X_START)-((zhSizer*wcslen(udNO_line0[lang]))*8), LINE_4_Y);

		drawX(draw_X_X,draw_X_Y);
		drawCheck(draw_check_X,draw_check_Y);
		display();

		r = waitForButtonPress();
		if (!r){
//			showWorking();
		}else{
//			showDenied();
		};
	}
	else if (command == ASKUSER_ENTER_PIN)
	{
		const wchar_t ENTER_PIN_line0[][8] = {
				L"PIN:", //EN
				L"PIN:", //DE
				L"ПАРОЛЬ:", //RU
				L"密码:", //ZH
				L"PIN:", //CZ
				L"NIP:", //FR
				L"PIN:", //ES
				L"PIN:", //PT
				L"PIN:", //TU
				L"PIN:"  //IT
		} ;

		const wchar_t udNUM_line0[][4] = {
				L"NUM", //EN
				L"NUM", //DE
				L"NUM", //RU
				L"字", //ZH
				L"NUM", //CZ
				L"NUM", //FR
				L"NUM", //ES
				L"NUM", //PT
				L"NUM", //TU
				L"NUM"  //IT
		} ;

		const wchar_t udGO_line0[][12] = {
				L"GO", //EN
				L"GEHEN", //DE
				L"ПОДТВЕРДИТЬ", //RU
				L"确认", //ZH
				L"DÁLE", //CZ
				L"CONTINUER", //FR
				L"IR", //ES
				L"IR", //PT
				L"İLERLE", //TU
				L"GO"  //IT
		} ;



		waitForNoButtonPress();

		initDisplay();
	    overlayBatteryStatus(BATT_VALUE_DISPLAY);
		writeEinkDrawUnicodeSingle((unsigned int*)ENTER_PIN_line0[lang], wcslen(ENTER_PIN_line0[lang]), COL_1_X, LINE_0_Y);
//		writeUnderline(STRIPE_X_START, STRIPE_Y_START, STRIPE_X_END, STRIPE_Y_END);
//		writeEinkDrawUnicodeSingle(str_ASKUSER_DESCRIBE_STANDARD_SETUP_line1_UNICODE_SIZED[lang][0], line1length, COL_1_X, LINE_2_Y);
//		writeEinkDrawUnicodeSingle(str_ASKUSER_DESCRIBE_STANDARD_SETUP_line2_UNICODE_SIZED[lang][0], line2length, COL_1_X, LINE_3_Y);
//		writeEinkDrawUnicodeSingle(str_ASKUSER_DESCRIBE_STANDARD_SETUP_line3_UNICODE_SIZED[lang][0], line3length, COL_1_X, LINE_4_Y);
		writeEinkDrawUnicodeSingle((unsigned int*)udGO_line0[lang], wcslen(udGO_line0[lang]), ACCEPT_X_START, LINE_4_Y);
		writeEinkDrawUnicodeSingle((unsigned int*)udNUM_line0[lang], wcslen(udNUM_line0[lang]), (DENY_X_START)-((zhSizer*wcslen(udNUM_line0[lang]))*8), LINE_4_Y);

		drawX(draw_X_X,draw_X_Y);
		drawCheck(draw_check_X,draw_check_Y);
		display();

//		r = waitForButtonPress();
//		if (!r){
//			clearDisplay();
//		}else{
//			writeX_Screen();
//			showReady();
//		};
	}
	else if (command == ASKUSER_ENTER_PIN_ALPHA)
	{
		const wchar_t ENTER_PIN_line0[][5] = {
				L"PIN:", //EN
				L"PIN:", //DE
				L"ПАРОЛЬ:", //RU
				L"密码:", //ZH
				L"PIN:", //CZ
				L"NIP:", //FR
				L"PIN:", //ES
				L"PIN:", //PT
				L"PIN:", //TU
				L"PIN:"  //IT
		} ;
		const wchar_t udALPHA_line0[][6] = {
				L"ALPHA", //EN
				L"ALPHA", //DE
				L"ALPHA", //RU
				L"拼音", //ZH
				L"ALPHA", //CZ
				L"ALPHA", //FR
				L"ALPHA", //ES
				L"ALPHA", //PT
				L"ALPHA", //TU
				L"ALPHA"  //IT
		} ;

		waitForNoButtonPress();

		initDisplay();
	    overlayBatteryStatus(BATT_VALUE_DISPLAY);
		writeEinkDrawUnicodeSingle((unsigned int*)ENTER_PIN_line0[lang], wcslen(ENTER_PIN_line0[lang]), COL_1_X, LINE_0_Y);
//		writeUnderline(STRIPE_X_START, STRIPE_Y_START, STRIPE_X_END, STRIPE_Y_END);
//		writeEinkDrawUnicodeSingle(str_ASKUSER_DESCRIBE_STANDARD_SETUP_line1_UNICODE_SIZED[lang][0], line1length, COL_1_X, LINE_2_Y);
//		writeEinkDrawUnicodeSingle(str_ASKUSER_DESCRIBE_STANDARD_SETUP_line2_UNICODE_SIZED[lang][0], line2length, COL_1_X, LINE_3_Y);
//		writeEinkDrawUnicodeSingle(str_ASKUSER_DESCRIBE_STANDARD_SETUP_line3_UNICODE_SIZED[lang][0], line3length, COL_1_X, LINE_4_Y);
//		writeEinkDrawUnicodeSingle((unsigned int*)GO_line0[lang], wcslen(GO_line0[lang]), ACCEPT_X_START, 80);
		writeEinkDrawUnicodeSingle((unsigned int*)udALPHA_line0[lang], wcslen(udALPHA_line0[lang]), (DENY_X_START)-((zhSizer*wcslen(udALPHA_line0[lang]))*8), LINE_4_Y);

		drawX(draw_X_X,draw_X_Y);
//		drawCheck(draw_check_X,draw_check_Y);
		display();

//		r = waitForButtonPress();
//		if (!r){
//			clearDisplay();
//		}else{
//			writeX_Screen();
//			showReady();
//		};
	}
	else if (command == ASKUSER_ENTER_TRANSACTION_PIN)
	{
		const wchar_t ENTER_TRANSACTION_PIN_line0[][25] = {
				L"TRANSACTION PIN:", //EN
				L"TRANSAKTION PIN:", //DE
				L"ПАРОЛЬ ДЛЯ ТРАНЗАКЦИЙ:", //RU
				L"交易密码:", //ZH
				L"PIN TRANSAKCE:", //CZ
				L"NIP DE TRANSACTION:", //FR
				L"CÓDIGO DE TRANSACCIÓN:", //ES
				L"TRANSACTION PIN:", //PT
				L"İŞLEM PIN KODUNU:", //TU
				L"PIN DELLE TRANSAZIONI:"  //IT
		} ;

		const wchar_t udNUM_line0[][4] = {
				L"NUM", //EN
				L"NUM", //DE
				L"NUM", //RU
				L"字", //ZH
				L"NUM", //CZ
				L"NUM", //FR
				L"NUM", //ES
				L"NUM", //PT
				L"NUM", //TU
				L"NUM"  //IT
		} ;

		const wchar_t udGO_line0[][12] = {
				L"GO", //EN
				L"GEHEN", //DE
				L"ПОДТВЕРДИТЬ", //RU
				L"确认", //ZH
				L"DÁLE", //CZ
				L"CONTINUER", //FR
				L"IR", //ES
				L"IR", //PT
				L"İLERLE", //TU
				L"GO"  //IT
		} ;


		waitForNoButtonPress();

		initDisplay();
	    overlayBatteryStatus(BATT_VALUE_DISPLAY);
		writeEinkDrawUnicodeSingle((unsigned int*)ENTER_TRANSACTION_PIN_line0[lang], wcslen(ENTER_TRANSACTION_PIN_line0[lang]), COL_1_X, LINE_0_Y);
//		writeUnderline(STRIPE_X_START, STRIPE_Y_START, STRIPE_X_END, STRIPE_Y_END);
//		writeEinkDrawUnicodeSingle(str_ASKUSER_DESCRIBE_STANDARD_SETUP_line1_UNICODE_SIZED[lang][0], line1length, COL_1_X, LINE_2_Y);
//		writeEinkDrawUnicodeSingle(str_ASKUSER_DESCRIBE_STANDARD_SETUP_line2_UNICODE_SIZED[lang][0], line2length, COL_1_X, LINE_3_Y);
//		writeEinkDrawUnicodeSingle(str_ASKUSER_DESCRIBE_STANDARD_SETUP_line3_UNICODE_SIZED[lang][0], line3length, COL_1_X, LINE_4_Y);
		writeEinkDrawUnicodeSingle((unsigned int*)udGO_line0[lang], wcslen(udGO_line0[lang]), ACCEPT_X_START, LINE_4_Y);
		writeEinkDrawUnicodeSingle((unsigned int*)udNUM_line0[lang], wcslen(udNUM_line0[lang]), (DENY_X_START)-((zhSizer*wcslen(udNUM_line0[lang]))*8), LINE_4_Y);

		drawX(draw_X_X,draw_X_Y);
		drawCheck(draw_check_X,draw_check_Y);
		display();

//		r = waitForButtonPress();
//		if (!r){
//			clearDisplay();
//		}else{
//			writeX_Screen();
//			showReady();
//		};
	}
	else if (command == ASKUSER_ENTER_WALLET_PIN)
	{
		const wchar_t ENTER_WALLET_PIN_line0[][25] = {
				L"WALLET PIN:", //EN
				L"WALLET PIN:", //DE
				L"ПАРОЛЬ КОШЕЛЬКА:", //RU
				L"钱袋密码:", //ZH
				L"PIN PENĚŽENKY:", //CZ
				L"NIP DU PORTEFEUILLE:", //FR
				L"CÓDIGO DE LA CARTERA:", //ES
				L"PIN CARTEIRA:", //PT
				L"CÜZDAN PIN KODU:", //TU
				L"PIN PORTAFOGLIO:"  //IT
		} ;
		const wchar_t udNUM_line0[][4] = {
				L"NUM", //EN
				L"NUM", //DE
				L"NUM", //RU
				L"字", //ZH
				L"NUM", //CZ
				L"NUM", //FR
				L"NUM", //ES
				L"NUM", //PT
				L"NUM", //TU
				L"NUM"  //IT
		} ;

		const wchar_t udGO_line0[][12] = {
				L"GO", //EN
				L"GEHEN", //DE
				L"ПОДТВЕРДИТЬ", //RU
				L"确认", //ZH
				L"DÁLE", //CZ
				L"CONTINUER", //FR
				L"IR", //ES
				L"IR", //PT
				L"İLERLE", //TU
				L"GO"  //IT
		} ;



		waitForNoButtonPress();

		initDisplay();
	    overlayBatteryStatus(BATT_VALUE_DISPLAY);
		writeEinkDrawUnicodeSingle((unsigned int*)ENTER_WALLET_PIN_line0[lang], wcslen(ENTER_WALLET_PIN_line0[lang]), COL_1_X, LINE_0_Y);
//		writeUnderline(STRIPE_X_START, STRIPE_Y_START, STRIPE_X_END, STRIPE_Y_END);
//		writeEinkDrawUnicodeSingle(str_ASKUSER_DESCRIBE_STANDARD_SETUP_line1_UNICODE_SIZED[lang][0], line1length, COL_1_X, LINE_2_Y);
//		writeEinkDrawUnicodeSingle(str_ASKUSER_DESCRIBE_STANDARD_SETUP_line2_UNICODE_SIZED[lang][0], line2length, COL_1_X, LINE_3_Y);
//		writeEinkDrawUnicodeSingle(str_ASKUSER_DESCRIBE_STANDARD_SETUP_line3_UNICODE_SIZED[lang][0], line3length, COL_1_X, LINE_4_Y);
		writeEinkDrawUnicodeSingle((unsigned int*)udGO_line0[lang], wcslen(udGO_line0[lang]), ACCEPT_X_START, LINE_4_Y);
		writeEinkDrawUnicodeSingle((unsigned int*)udNUM_line0[lang], wcslen(udNUM_line0[lang]), (DENY_X_START)-((zhSizer*wcslen(udNUM_line0[lang]))*8), LINE_4_Y);

		drawX(draw_X_X,draw_X_Y);
		drawCheck(draw_check_X,draw_check_Y);
		display();

//		r = waitForButtonPress();
//		if (!r){
//			clearDisplay();
//		}else{
//			writeX_Screen();
//			showReady();
//		};
	}
	else if (command == ASKUSER_ENTER_WALLET_PIN_ALPHA)
	{
		const wchar_t ENTER_WALLET_PIN_ALPHA_line0[][25] = {
				L"WALLET PIN:", //EN
				L"WALLET PIN:", //DE
				L"ПАРОЛЬ КОШЕЛЬКА:", //RU
				L"钱袋密码:", //ZH
				L"PIN PENĚŽENKY:", //CZ
				L"NIP DU PORTEFEUILLE:", //FR
				L"CÓDIGO DE LA CARTERA:", //ES
				L"PIN CARTEIRA:", //PT
				L"CÜZDAN PIN KODU:", //TU
				L"PIN PORTAFOGLIO:"  //IT
		} ;
		const wchar_t udALPHA_line0[][6] = {
				L"ALPHA", //EN
				L"ALPHA", //DE
				L"ALPHA", //RU
				L"拼音", //ZH
				L"ALPHA", //CZ
				L"ALPHA", //FR
				L"ALPHA", //ES
				L"ALPHA", //PT
				L"ALPHA", //TU
				L"ALPHA"  //IT
		} ;

		waitForNoButtonPress();

		initDisplay();
	    overlayBatteryStatus(BATT_VALUE_DISPLAY);
		writeEinkDrawUnicodeSingle((unsigned int*)ENTER_WALLET_PIN_ALPHA_line0[lang], wcslen(ENTER_WALLET_PIN_ALPHA_line0[lang]), COL_1_X, LINE_0_Y);
//		writeUnderline(STRIPE_X_START, STRIPE_Y_START, STRIPE_X_END, STRIPE_Y_END);
//		writeEinkDrawUnicodeSingle(str_ASKUSER_DESCRIBE_STANDARD_SETUP_line1_UNICODE_SIZED[lang][0], line1length, COL_1_X, LINE_2_Y);
//		writeEinkDrawUnicodeSingle(str_ASKUSER_DESCRIBE_STANDARD_SETUP_line2_UNICODE_SIZED[lang][0], line2length, COL_1_X, LINE_3_Y);
//		writeEinkDrawUnicodeSingle(str_ASKUSER_DESCRIBE_STANDARD_SETUP_line3_UNICODE_SIZED[lang][0], line3length, COL_1_X, LINE_4_Y);
//		writeEinkDrawUnicodeSingle((unsigned int*)GO_line0[lang], wcslen(GO_line0[lang]), ACCEPT_X_START, 80);
		writeEinkDrawUnicodeSingle((unsigned int*)udALPHA_line0[lang], wcslen(udALPHA_line0[lang]), (DENY_X_START)-((zhSizer*wcslen(udALPHA_line0[lang]))*8), LINE_4_Y);

		drawX(draw_X_X,draw_X_Y);
//		drawCheck(draw_check_X,draw_check_Y);
		display();

//		r = waitForButtonPress();
//		if (!r){
//			clearDisplay();
//		}else{
//			writeX_Screen();
//			showReady();
//		};
	}
//	else if (command == ASKUSER_ALPHA_INPUT_PREFACE)
//	{
//		const wchar_t udALPHA_INPUT_PREFACE_A_line0[][25] = {
//				L"MINIMUM 4 CHARACTERS", //EN
//				L"MINIMUM 4 ZEICHEN", //DE
//				L"НЕ МЕНЕЕ 4 СИМВОЛОВ", //RU
//				L"最少4个字符", //ZH
//				L"ALESPOŇ 4 ZNAKY", //CZ
//				L"4 CARACTÈRES MINIMUM", //FR
//				L"MÍNIMO 4 CARACTERES", //ES
//				L"MÍNIMAS 4 CARACTERES", //PT
//				L"MİNİMUM 4 KARAKTER", //TU
//				L"MINIMO 4 CARATTERI"  //IT
//		} ;
//
//		const wchar_t udALPHA_INPUT_PREFACE_B_line0[][25] = {
//				L"PRESS/HOLD/RELEASE", //EN
//				L"DRÜCKEN/HALTEN/LOSLASSEN", //DE
//				L"НАЖАТЬ/ДЕРЖАТЬ/ОТПУСТИТЬ", //RU
//				L"推下/推住/放开", //ZH
//				L"STISKNOUT/PODRŽET/UVOLNT", //CZ
//				L"APPUYER/MAINT./RELÂCHER", //FR
//				L"PULSAR/MANTENER/SOLTAR", //ES
//				L"PRESSIONE/AGUARDE/SOLTE", //PT
//				L"BAS/TUT/BIRAK", //TU
//				L"PREMI/TIENI/RILASCIA"  //IT
//		} ;
//
//		waitForNoButtonPress();
//
//		initDisplay();
//		writeEinkDrawUnicodeSingle((unsigned int*)udALPHA_INPUT_PREFACE_A_line0[lang], wcslen(udALPHA_INPUT_PREFACE_A_line0[lang]), COL_1_X, LINE_0_Y);
//		writeUnderline(STRIPE_X_START, STRIPE_Y_START, STRIPE_X_END, STRIPE_Y_END);
//		display();
//
//		initDisplay();
//		writeEinkDrawUnicodeSingle((unsigned int*)udALPHA_INPUT_PREFACE_B_line0[lang], wcslen(udALPHA_INPUT_PREFACE_B_line0[lang]), COL_1_X, LINE_0_Y);
//		writeUnderline(STRIPE_X_START, STRIPE_Y_START, STRIPE_X_END, STRIPE_Y_END);
////		writeEinkDrawUnicodeSingle(str_ASKUSER_ALPHA_INPUT_PREFACE_line2_UNICODE_SIZED[lang][0], line2length, COL_1_X, LINE_2_Y);
//		display();
//
////		r = waitForButtonPress();
////		if (!r){
////			clearDisplay();
////		}else{
////			writeX_Screen();
////			showReady();
////		};
//	}
	else if (command == ASKUSER_RESTORE_WALLET_DEVICE)
	{
		const wchar_t RESTORE_WALLET_DEVICE_line0[][25] = {
				L"RESTORE WALLET", //EN
				L"RESTORE WALLET", //DE
				L"ВОССТАНОВЛЕНИЕ БУМАЖНИКА", //RU
				L"恢复钱袋", //ZH
				L"RESTORE WALLET", //CZ
				L"RESTORE PORTEFEUILLE", //FR
				L"RESTORE CARTERA", //ES
				L"RESTAURAR CARTEIRA", //PT
				L"WALLET RESTORE", //TU
				L"RESTORE PORTAFOGLIO"  //IT
		} ;

		const wchar_t RESTORE_WALLET_DEVICE_line1[][25] = {
				L"RESTORE A WALLET", //EN
				L"WIEDERHERSTELLEN EINER", //DE
				L"ВОССТАНОВЛЕНИЕ КОШЕЛЕК", //RU
				L"恢复从助记符列表钱袋？", //ZH
				L"OBNOVENÍ WALLET FROM", //CZ
				L"RESTORE UN PORTEFEUILLE", //FR
				L"RESTAURAR UNA CARPETA", //ES
				L"RESTAURAR A CARTEIRA", //PT
				L"ANIMSATICI LİSTEDEKİ", //TU
				L"RESTORE UN RACCOGLITORE"  //IT
		} ;

		const wchar_t RESTORE_WALLET_DEVICE_line2[][25] = {
				L"FROM A MNEMONIC LIST?", //EN
				L"GELDBÖRSE AUS LISTE?", //DE
				L"ИЗ МНЕМОНИЧЕСКОЙ СПИСОК?", //RU
				L"ZH", //ZH
				L"MNEMOTECHNICKÁ SEZNAMU?", //CZ
				L"D'UNE LISTE DE MNEMONIC?", //FR
				L"DE UNA LISTA MNEMÓNICO?", //ES
				L"DE UMA LISTA MNEMÔNICO?", //PT
				L"BIR CÜZDAN RESTORE?", //TU
				L"DA UNA LISTA MNEMONICO?"  //IT
		} ;
		const wchar_t udCANCEL_line0[][9] = {
				L"CANCEL", //EN
				L"ABSAGEN", //DE
				L"ОТМЕНА", //RU
				L"退回", //ZH
				L"ZRUŠIT", //CZ
				L"ANNULER", //FR
				L"CANCELAR", //ES
				L"CANCELAR", //PT
				L"İPTAL", //TU
				L"ANNULLA"  //IT
		} ;
		const wchar_t udACCEPT_line0[][12] = {
				L"ACCEPT", //EN
				L"AKZEPTIEREN", //DE
				L"ПОДТВЕРДИТЬ", //RU
				L"确认", //ZH
				L"POTVRDIT", //CZ
				L"ACCEPTER", //FR
				L"ACEPTAR", //ES
				L"ACEITAR", //PT
				L"KABUL ET", //TU
				L"ACCETTA"  //IT
		} ;


		waitForNoButtonPress();

		initDisplay();
	    overlayBatteryStatus(BATT_VALUE_DISPLAY);

		writeEinkDrawUnicodeSingle((unsigned int*)RESTORE_WALLET_DEVICE_line0[lang], wcslen(RESTORE_WALLET_DEVICE_line0[lang]), COL_1_X, LINE_0_Y);
		writeUnderline(STRIPE_X_START, STRIPE_Y_START, STRIPE_X_END, STRIPE_Y_END);
		writeEinkDrawUnicodeSingle((unsigned int*)RESTORE_WALLET_DEVICE_line1[lang], wcslen(RESTORE_WALLET_DEVICE_line1[lang]), COL_1_X, LINE_1_Y);
		writeEinkDrawUnicodeSingle((unsigned int*)RESTORE_WALLET_DEVICE_line2[lang], wcslen(RESTORE_WALLET_DEVICE_line2[lang]), COL_1_X, LINE_2_Y);
		writeEinkDrawUnicodeSingle((unsigned int*)udACCEPT_line0[lang], wcslen(udACCEPT_line0[lang]), ACCEPT_X_START, LINE_4_Y);
		writeEinkDrawUnicodeSingle((unsigned int*)udCANCEL_line0[lang], wcslen(udCANCEL_line0[lang]), (DENY_X_START)-((zhSizer*wcslen(udCANCEL_line0[lang]))*8), LINE_4_Y);

		drawX(draw_X_X,draw_X_Y);
		drawCheck(draw_check_X,draw_check_Y);
		display();

		r = waitForButtonPress();
		if (!r){
			clearDisplay();
		}else{
			writeX_Screen();
			showReady();
		};
	}
	else if (command == ASKUSER_NEW_WALLET_IS_HIDDEN)
	{
		const wchar_t NEW_WALLET_IS_HIDDEN_line0[][25] = {
				L"WALLET VISIBILITY", //EN
				L"SICHTBARKEIT WALLET", //DE
				L"ВИДИМОСТЬ КОШЕЛЬКА", //RU
				L"钱袋可见度", //ZH
				L"VIDITELNOST PENĚŽENKY", //CZ
				L"VISIBILITÉ DU PORTEF.", //FR
				L"VISIBILIDAD CARTERA", //ES
				L"CARTEIRA DE VISIBILIDADE", //PT
				L"CÜZDAN GÖRÜNÜRLÜĞÜ", //TU
				L"VISIBILITÀ PORTAFOGLIO"  //IT
		} ;


		const wchar_t NEW_WALLET_IS_HIDDEN_line1[][25] = {
				L"MAKE WALLET HIDDEN?", //EN
				L"WALLET VERSTECKT MACHEN?", //DE
				L"СДЕЛАТЬ КОШЕЛЕК ТАЙНЫМ?", //RU
				L"是否要生成秘密钱袋？", //ZH
				L"SCHOVAT PENĚŽENKU?", //CZ
				L"MASQUER LE PORTEFEUILLE?", //FR
				L"OCULTAR LA CARTERA?", //ES
				L"FAZER CARTEIRA ESCONDIDA", //PT
				L"CÜZDANI GİZLE?", //TU
				L"PORTAFOGLIO INVISIBILE?"  //IT
		} ;

		const wchar_t udYES_line0[][5] = {
					L"YES", //EN
					L"JA", //DE
					L"ДА", //RU
					L"是", //ZH
					L"ANO", //CZ
					L"OUI", //FR
					L"SÍ", //ES
					L"SIM", //PT
					L"EVET", //TU
					L"SÌ"  //IT
			} ;
		const wchar_t udNO_line0[][6] = {
				L"NO", //EN
				L"NEIN", //DE
				L"НЕТ", //RU
				L"否", //ZH
				L"NE", //CZ
				L"NON", //FR
				L"NO", //ES
				L"NÃO", //PT
				L"HAYIR", //TU
				L"NO"  //IT
		} ;

		waitForNoButtonPress();// comment this out and it just flies through without waiting for a YES...

		initDisplay();
	    overlayBatteryStatus(BATT_VALUE_DISPLAY);
		writeEinkDrawUnicodeSingle((unsigned int*)NEW_WALLET_IS_HIDDEN_line0[lang], wcslen(NEW_WALLET_IS_HIDDEN_line0[lang]), COL_1_X, LINE_0_Y);
		writeUnderline(STRIPE_X_START, STRIPE_Y_START, STRIPE_X_END, STRIPE_Y_END);
		writeEinkDrawUnicodeSingle((unsigned int*)NEW_WALLET_IS_HIDDEN_line1[lang], wcslen(NEW_WALLET_IS_HIDDEN_line1[lang]), COL_1_X, LINE_1_Y);
		writeEinkDrawUnicodeSingle((unsigned int*)udYES_line0[lang], wcslen(udYES_line0[lang]), ACCEPT_X_START, LINE_4_Y);
		writeEinkDrawUnicodeSingle((unsigned int*)udNO_line0[lang], wcslen(udNO_line0[lang]), (DENY_X_START)-((zhSizer*wcslen(udNO_line0[lang]))*8), LINE_4_Y);

		drawX(draw_X_X,draw_X_Y);
		drawCheck(draw_check_X,draw_check_Y);
		display();

		r = waitForButtonPress();
//		if (!r){
//			writeEinkDisplay("HIDDEN", false, 5, 10, "#50",false,5,25, "",false,0,0, "",false,0,0, "",false,0,0);
//			showWorking();
//		}else{
			clearDisplay();
//		};
	}
	else if (command == ASKUSER_NEW_WALLET_NO_PASSWORD)
	{
		const wchar_t NEW_WALLET_NO_PASSWORD_line0[][25] = {
				L"WALLET PIN", //EN
				L"GELDBÖRSE PIN", //DE
				L"PIN-КОШЕЛЕК", //RU
				L"钱袋密码", //ZH
				L"WALLET PIN", //CZ
				L"NIP DU PORTEFEUILLE", //FR
				L"PIN CARTERA", //ES
				L"PIN CARTEIRA", //PT
				L"CÜZDAN PIN", //TU
				L"PIN PORTAFOGLIO"  //IT
		} ;


		const wchar_t NEW_WALLET_NO_PASSWORD_line1[][25] = {
				L"CREATE A WALLET", //EN
				L"ERSTELLEN EINE", //DE
				L"СОЗДАТЬ КОШЕЛЕК", //RU
				L"创建一个没有密码钱袋？", //ZH
				L"VYTVOŘIT PENĚŽENKU", //CZ
				L"CRÉER UN PORTEFEUILLE", //FR
				L"CREAR UNA CARPETA", //ES
				L"CRIAR UMA CARTEIRA", //PT
				L"BIR PIN OLMADAN BIR", //TU
				L"CREARE UN RACCOGLITORE"  //IT
		} ;

		const wchar_t NEW_WALLET_NO_PASSWORD_line2[][25] = {
				L"WITHOUT A PIN?", //EN
				L"GELDBÖRSE OHNE PIN?", //DE
				L"БЕЗ PIN-КОДА?", //RU
				L"", //ZH
				L"BEZ KÓDU PIN?", //CZ
				L"SANS CODE NIP?", //FR
				L"SIN PIN?", //ES
				L"SEM PIN?", //PT
				L"CÜZDAN OLUŞTURMA?", //TU
				L"SENZA PIN?"  //IT
		} ;
		const wchar_t udCANCEL_line0[][9] = {
				L"CANCEL", //EN
				L"ABSAGEN", //DE
				L"ОТМЕНА", //RU
				L"退回", //ZH
				L"ZRUŠIT", //CZ
				L"ANNULER", //FR
				L"CANCELAR", //ES
				L"CANCELAR", //PT
				L"İPTAL", //TU
				L"ANNULLA"  //IT
		} ;
		const wchar_t udACCEPT_line0[][12] = {
				L"ACCEPT", //EN
				L"AKZEPTIEREN", //DE
				L"ПОДТВЕРДИТЬ", //RU
				L"确认", //ZH
				L"POTVRDIT", //CZ
				L"ACCEPTER", //FR
				L"ACEPTAR", //ES
				L"ACEITAR", //PT
				L"KABUL ET", //TU
				L"ACCETTA"  //IT
		} ;


		waitForNoButtonPress();// comment this out and it just flies through without waiting for a YES...

		initDisplay();
	    overlayBatteryStatus(BATT_VALUE_DISPLAY);
		writeEinkDrawUnicodeSingle((unsigned int*)NEW_WALLET_NO_PASSWORD_line0[lang], wcslen(NEW_WALLET_NO_PASSWORD_line0[lang]), COL_1_X, LINE_0_Y);
		writeUnderline(STRIPE_X_START, STRIPE_Y_START, STRIPE_X_END, STRIPE_Y_END);
		writeEinkDrawUnicodeSingle((unsigned int*)NEW_WALLET_NO_PASSWORD_line1[lang], wcslen(NEW_WALLET_NO_PASSWORD_line1[lang]), COL_1_X, LINE_1_Y);
		writeEinkDrawUnicodeSingle((unsigned int*)NEW_WALLET_NO_PASSWORD_line2[lang], wcslen(NEW_WALLET_NO_PASSWORD_line2[lang]), COL_1_X, LINE_2_Y);
		writeEinkDrawUnicodeSingle((unsigned int*)udACCEPT_line0[lang], wcslen(udACCEPT_line0[lang]), ACCEPT_X_START, LINE_4_Y);
		writeEinkDrawUnicodeSingle((unsigned int*)udCANCEL_line0[lang], wcslen(udCANCEL_line0[lang]), (DENY_X_START)-((zhSizer*wcslen(udCANCEL_line0[lang]))*8), LINE_4_Y);

		drawX(draw_X_X,draw_X_Y);
		drawCheck(draw_check_X,draw_check_Y);
		display();

		r = waitForButtonPress();
		if (!r){
			showWorking();
		}else{
			clearDisplay();
		};
	}
	else if (command == ASKUSER_SIGN_TRANSACTION)
	{
		const wchar_t SIGN_part0[][25] = {
				L"SEND", //EN
				L"SENDEN", //DE
				L"ОТПРАВИТЬ", //RU
				L"发送", //ZH
				L"POSLAT", //CZ
				L"ENVOYER", //FR
				L"ENVIAR", //ES
				L"MANDAR", //PT
				L"GÖNDER", //TU
				L"INVIARE"  //IT
		} ;

		const wchar_t trCANCEL_line0[][9] = {
				L"CANCEL", //EN
				L"ABSAGEN", //DE
				L"ОТМЕНА", //RU
				L"退回", //ZH
				L"ZRUŠIT", //CZ
				L"ANNULER", //FR
				L"CANCELAR", //ES
				L"CANCELAR", //PT
				L"İPTAL", //TU
				L"ANNULLA"  //IT
		} ;

		const wchar_t trCANCEL_line1[][9] = {
				L"CANCEL", //EN
				L"ABSAGEN", //DE
				L"ОТМЕНА", //RU
				L"退回", //ZH
				L"ZRUŠIT", //CZ
				L"ANNULER", //FR
				L"CANCELAR", //ES
				L"CANCELAR", //PT
				L"İPTAL", //TU
				L"ANNULLA"  //IT
		} ;

		const wchar_t FEE_part0[][25] = {
				L"TRANSACTION FEE", //EN
				L"BEARBEITUNGSGEBÜHR", //DE
				L"КОМИССИЯ НА ПЕРЕВОД", //RU
				L"手续费", //ZH
				L"POPLATEK ZA TRANSAKCI", //CZ
				L"FRAIS DE TRANSACTION", //FR
				L"TARIFA DE TRANSACCIÓN", //ES
				L"TAXA DE TRANSAÇÃO", //PT
				L"İŞLEM ÜCRETİ", //TU
				L"COSTO DELLA TRANSAZIONE"  //IT
		} ;

		const wchar_t udYES_line0[][5] = {
					L"YES", //EN
					L"JA", //DE
					L"ДА", //RU
					L"是", //ZH
					L"ANO", //CZ
					L"OUI", //FR
					L"SÍ", //ES
					L"SIM", //PT
					L"EVET", //TU
					L"SÌ"  //IT
			} ;
		const wchar_t udACCEPT_line0[][12] = {
				L"ACCEPT", //EN
				L"AKZEPTIEREN", //DE
				L"ПОДТВЕРДИТЬ", //RU
				L"确认", //ZH
				L"POTVRDIT", //CZ
				L"ACCEPTER", //FR
				L"ACEPTAR", //ES
				L"ACEITAR", //PT
				L"KABUL ET", //TU
				L"ACCETTA"  //IT
		} ;

//		const wchar_t FEE_part1[][25] = {
//				L" BTC", //EN
//				L" BTC", //DE
//				L" BTC", //RU
//				L" BTC", //ZH
//				L" BTC", //CZ
//				L" BTC", //FR
//				L" BTC", //ES
//				L" BTC", //PT
//				L" BTC", //TU
//				L" BTC"  //IT
//		} ;
//
		const char str_sign_part1[]  = " BTC >>>";

		for (i = 0; i < list_index; i++)
		{

			waitForNoButtonPress();

//			char *part0;
			char *part1;
			char str[80];
//			part0 = getTransString0(str_sign_part0);
//			strcpy (str,part0);
//			strcat (str,list_amount[i]);
			strcpy (str,list_amount[i]);
			part1 = getTransString1(str_sign_part1);
			strcat (str,part1);

			char add1[40];
			strcpy (add1, list_address[i]);
			str_cut(add1, 17, -1);
			char add2[40];
			strcpy (add2, list_address[i]);
			str_cut(add2, 0, 17);


			initDisplay();
			overlayBatteryStatus(BATT_VALUE_DISPLAY);

			writeEinkDrawUnicodeSingle((unsigned int*)SIGN_part0[lang], wcslen(SIGN_part0[lang]), COL_1_X, 5);
			writeEinkNoDisplaySingle(str, COL_1_X, 25);
			writeEinkNoDisplaySingle(add1, COL_1_X, 45);
			writeEinkNoDisplaySingle(add2, COL_1_X, 60);
			writeEinkDrawUnicodeSingle((unsigned int*)udYES_line0[lang], wcslen(udYES_line0[lang]), ACCEPT_X_START, LINE_4_Y);
			writeEinkDrawUnicodeSingle((unsigned int*)trCANCEL_line0[lang], wcslen(trCANCEL_line0[lang]), DENY_X_START-(zhSizer*wcslen(trCANCEL_line0[lang])*8), LINE_4_Y);



			drawX(draw_X_X,draw_X_Y);
			drawCheck(draw_check_X,draw_check_Y);

			display();


			r = waitForButtonPress();
			clearDisplay();
			if (r)
			{
				// All outputs must be approved in order for a transaction
				// to be signed. Thus if the user denies spending to one
				// output, the entire transaction is forfeit.
//				showDenied();
				break;
			}

		}
		if (!r && transaction_fee_set)
		{
			waitForNoButtonPress();

			char strFee[80];
			char *feePart1;
			strcpy (strFee, transaction_fee_amount);
			feePart1 = getTransString0(str_fee_part1);
			strcat (strFee,feePart1);


			initDisplay();
			overlayBatteryStatus(BATT_VALUE_DISPLAY);


			writeEinkDrawUnicodeSingle((unsigned int*)FEE_part0[lang], wcslen(FEE_part0[lang]), COL_1_X, LINE_0_Y);
			writeEinkDrawUnicodeSingle((unsigned int*)udACCEPT_line0[lang], wcslen(udACCEPT_line0[lang]), ACCEPT_X_START, LINE_4_Y);
			writeEinkDrawUnicodeSingle((unsigned int*)trCANCEL_line1[lang], wcslen(trCANCEL_line1[lang]), DENY_X_START-(zhSizer*wcslen(trCANCEL_line1[lang])*8), LINE_4_Y);
			writeEinkNoDisplaySingle(strFee, COL_1_X, LINE_2_Y);

			drawX(draw_X_X,draw_X_Y);
			drawCheck(draw_check_X,draw_check_Y);

			display();

			r = waitForButtonPress();
			clearDisplay();
//			showWorking();
		}
	}
	else if (command == ASKUSER_FORMAT)
	{
		const wchar_t FORMAT_DEVICE_line0[][25] = {
				L"FORMAT DEVICE", //EN
				L"GERÄT FORMATIEREN", //DE
				L"ОТФОРМАТ. УСТРОЙСТВО", //RU
				L"设具格式化", //ZH
				L"VYMAZAT ZAŘÍZENÍ", //CZ
				L"FORMATER L'APPAREIL", //FR
				L"INICIALIZAR DISPOSITIVO", //ES
				L"DISPOSITIVO DE FORMATO", //PT
				L"CİHAZI SIFIRLA", //TU
				L"FORMATTA IL DISPOSITIVO"  //IT
		} ;


		const wchar_t FORMAT_DEVICE_line1[][25] = {
				L"ERASE ALL WALLETS,", //EN
				L"ALLE WALLETS LÖSCHEN,", //DE
				L"УДАЛИТЬ ВСЕ КОШЕЛЬКИ ", //RU
				L"删除所有的钱袋并", //ZH
				L"SMAZAT VŠECHNY PENĚŽENKY", //CZ
				L"EFFACER TOUS", //FR
				L"BORRAR TODAS ", //ES
				L"APAGAR TODAS AS ", //PT
				L"TÜM CÜZDANLARI SİL,", //TU
				L"CANCELLA TUTTI I TUOI"  //IT
		} ;


		const wchar_t FORMAT_DEVICE_line2[][25] = {
				L"DESTROYING ALL COINS?", //EN
				L"ZERSTÖREN ALLE COINS?", //DE
				L"И УНИЧТОЖИТЬ ВСЕ", //RU
				L"销毁全部存款吗？", //ZH
				L"A ZNIČIT VŠECHNY MINCE?", //CZ
				L"LES PORTEFEUILLES, ET", //FR
				L"LAS CARTERAS DESTRUYENDO", //ES
				L"CARTEIRAS DESTRUINDO ", //PT
				L"TÜM COIN'LERİ YOK ET?", //TU
				L"PORTAFOGLI, DISTRUGGENDO"  //IT
		} ;

		const wchar_t FORMAT_DEVICE_line3[][25] = {
				L"", //EN
				L"", //DE
				L"СБЕРЕЖЕНИЯ?", //RU
				L"", //ZH
				L"", //CZ
				L"DÉTRUIRES TOUTES?", //FR
				L"TODAS LAS MONEDAS?", //ES
				L"TODAS AS MOEDAS", //PT
				L"", //TU
				L"TUTTI I COINS?"  //IT
		} ;
		const wchar_t udCANCEL_line0[][9] = {
				L"CANCEL", //EN
				L"ABSAGEN", //DE
				L"ОТМЕНА", //RU
				L"退回", //ZH
				L"ZRUŠIT", //CZ
				L"ANNULER", //FR
				L"CANCELAR", //ES
				L"CANCELAR", //PT
				L"İPTAL", //TU
				L"ANNULLA"  //IT
		} ;

		const wchar_t udACCEPT_line0[][12] = {
				L"ACCEPT", //EN
				L"AKZEPTIEREN", //DE
				L"ПОДТВЕРДИТЬ", //RU
				L"确认", //ZH
				L"POTVRDIT", //CZ
				L"ACCEPTER", //FR
				L"ACEPTAR", //ES
				L"ACEITAR", //PT
				L"KABUL ET", //TU
				L"ACCETTA"  //IT
		} ;

		waitForNoButtonPress();// comment this out and it just flies through without waiting for a YES...

		initDisplay();
	    overlayBatteryStatus(BATT_VALUE_DISPLAY);
		writeEinkDrawUnicodeSingle((unsigned int*)FORMAT_DEVICE_line0[lang], wcslen(FORMAT_DEVICE_line0[lang]), COL_1_X, LINE_0_Y);
		writeUnderline(STRIPE_X_START, STRIPE_Y_START, STRIPE_X_END, STRIPE_Y_END);
		writeEinkDrawUnicodeSingle((unsigned int*)FORMAT_DEVICE_line1[lang], wcslen(FORMAT_DEVICE_line1[lang]), COL_1_X, LINE_1_Y);
		writeEinkDrawUnicodeSingle((unsigned int*)FORMAT_DEVICE_line2[lang], wcslen(FORMAT_DEVICE_line2[lang]), COL_1_X, LINE_2_Y);
		writeEinkDrawUnicodeSingle((unsigned int*)FORMAT_DEVICE_line3[lang], wcslen(FORMAT_DEVICE_line3[lang]), COL_1_X, LINE_3_Y);
		writeEinkDrawUnicodeSingle((unsigned int*)udACCEPT_line0[lang], wcslen(udACCEPT_line0[lang]), ACCEPT_X_START, LINE_4_Y);
		writeEinkDrawUnicodeSingle((unsigned int*)udCANCEL_line0[lang], wcslen(udCANCEL_line0[lang]), (DENY_X_START)-((zhSizer*wcslen(udCANCEL_line0[lang]))*8), LINE_4_Y);

		drawX(draw_X_X,draw_X_Y);
		drawCheck(draw_check_X,draw_check_Y);
		display();

		r = waitForButtonPress();
		if (!r){
			showWorking();
		}else{
			writeX_Screen();
			showReady();
		};
	}
	else if (command == ASKUSER_PREPARING_TRANSACTION)
	{
		const wchar_t PREPARING_TRANSACTION_line0[][25] = {
				L"PREPARING", //EN
				L"VORBEREITUNG", //DE
				L"ПОДГОТОВКА", //RU
				L"这使事务...", //ZH
				L"PŘÍPRAVA", //CZ
				L"PRÉPARATION", //FR
				L"PREPARACIÓN DE", //ES
				L"PREPARA", //PT
				L"İŞLEM", //TU
				L"PREPARAZIONE"  //IT
		} ;


		const wchar_t PREPARING_TRANSACTION_line1[][25] = {
				L"TRANSACTION...", //EN
				L"TRANSACTION...", //DE
				L"СДЕЛКИ...", //RU
				L"", //ZH
				L"TRANSACTION...", //CZ
				L"TRANSACTION...", //FR
				L"TRANSACCIÓN...", //ES
				L"OPERAÇÃO...", //PT
				L"HAZIRLAMA...", //TU
				L"TRANSAZIONE..."  //IT
		} ;


		initDisplay();

		writeEinkDrawUnicodeSingle((unsigned int*)PREPARING_TRANSACTION_line0[lang], wcslen(PREPARING_TRANSACTION_line0[lang]), COL_1_X, LINE_0_Y);
		writeEinkDrawUnicodeSingle((unsigned int*)PREPARING_TRANSACTION_line1[lang], wcslen(PREPARING_TRANSACTION_line1[lang]), COL_1_X, LINE_1_Y);

		display();

	}
	else if (command == ASKUSER_ASSEMBLING_HASHES)
	{
		const wchar_t ASSEMBLING_HASHES_line0[][25] = {
				L"ASSEMBLING HASHES...", //EN
				L"AUFBAU HASHES...", //DE
				L"МОНТАЖ ХЭШЕЙ...", //RU
				L"收集数据...", //ZH
				L"SESTAVENÍ HASHES...", //CZ
				L"MONTAGE HACHAGES...", //FR
				L"MONTAJE HASHES...", //ES
				L"MONTAGEM HASHES", //PT
				L"HASH'LER MONTAJ", //TU
				L"MONTAGGIO HASH"  //IT
		} ;




		initDisplay();

		writeEinkDrawUnicodeSingle((unsigned int*)ASSEMBLING_HASHES_line0[lang], wcslen(ASSEMBLING_HASHES_line0[lang]), COL_1_X, LINE_0_Y);
//		writeEinkDrawUnicodeSingle((unsigned int*)ASSEMBLING_HASHES_line1[lang], wcslen(ASSEMBLING_HASHES_line1[lang]), COL_1_X, LINE_1_Y);

		display();

	}
	else if (command == ASKUSER_SENDING_DATA)
	{
		const wchar_t SENDING_DATA_line0[][25] = {
				L"SENDING DATA...", //EN
				L"SENDEN VON DATEN...", //DE
				L"ОТПРАВКА ДАННЫХ...", //RU
				L"发送数据...", //ZH
				L"ODESÍLÁNÍ DAT...", //CZ
				L"ENVOI DE DONNÉES...", //FR
				L"DATOS DE ENVÍO...", //ES
				L"O ENVIO DE DADOS...", //PT
				L"GÖNDEREN VERİLERİ...", //TU
				L"DATI INVIO..."  //IT
		} ;




		initDisplay();

		writeEinkDrawUnicodeSingle((unsigned int*)SENDING_DATA_line0[lang], wcslen(SENDING_DATA_line0[lang]), COL_1_X, LINE_0_Y);
//		writeEinkDrawUnicodeSingle((unsigned int*)SENDING_DATA_line1[lang], wcslen(SENDING_DATA_line1[lang]), COL_1_X, LINE_1_Y);

		display();

	}
	else if (command == ASKUSER_FORMAT_AUTO)
	{
		r = false;
		if (!r){
//			showWorking();
		}else{
			writeX_Screen();
			showReady();
		};
	}
	else if (command == ASKUSER_CHANGE_NAME)
	{
		const wchar_t CHANGE_NAME_line0[][25] = {
				L"CHANGE WALLET NAME", //EN
				L"WALLET-NAMEN ÄNDERN", //DE
				L"ПЕРЕИМЕНОВАТЬ КОШЕЛЕК", //RU
				L"重名钱袋", //ZH
				L"ZMĚNIT NÁZEV", //CZ
				L"CHANGER LE NOM", //FR
				L"CAMBIAR NOMBRE", //ES
				L"ALTERAR A NOME", //PT
				L"CÜZDAN İSMİNİ DEĞİŞTİR", //TU
				L"CAMBIA NOME"  //IT
		} ;


		const wchar_t CHANGE_NAME_line1[][25] = {
				L"CHANGE THE NAME", //EN
				L"WALLET-NAMEN ÄNDERN? ", //DE
				L"ПЕРЕИМЕНОВАТЬ КОШЕЛЕК?", //RU
				L"是否要重名钱袋？", //ZH
				L"ZMĚNIT NÁZEV", //CZ
				L"CHANGER LE NOM ", //FR
				L"CAMBIAR EL NOMBRE", //ES
				L"MUDAR O NOME", //PT
				L"CÜZDANINIZIN İSMİNİ", //TU
				L"VUOI CAMBIARE IL NOME"  //IT
		} ;


		const wchar_t CHANGE_NAME_line2[][25] = {
				L"OF YOUR WALLET?", //EN
				L"", //DE
				L"", //RU
				L"", //ZH
				L"VAŠÍ PENĚŽENKY?", //CZ
				L"DE VOTRE PORTEFEUILLE?", //FR
				L"DE SU CARTERA?", //ES
				L"DE SUA CARTEIRA?", //PT
				L"DEĞİŞTİRMEK İSTİYOR", //TU
				L"DEL TUO PORTAFOGLIO?"  //IT
		} ;

		const wchar_t CHANGE_NAME_line3[][25] = {
				L"", //EN
				L"", //DE
				L"", //RU
				L"", //ZH
				L"", //CZ
				L"", //FR
				L"", //ES
				L"", //PT
				L"MUSUNUZ?", //TU
				L""  //IT
		} ;
		const wchar_t udCANCEL_line0[][9] = {
				L"CANCEL", //EN
				L"ABSAGEN", //DE
				L"ОТМЕНА", //RU
				L"退回", //ZH
				L"ZRUŠIT", //CZ
				L"ANNULER", //FR
				L"CANCELAR", //ES
				L"CANCELAR", //PT
				L"İPTAL", //TU
				L"ANNULLA"  //IT
		} ;
		const wchar_t udACCEPT_line0[][12] = {
				L"ACCEPT", //EN
				L"AKZEPTIEREN", //DE
				L"ПОДТВЕРДИТЬ", //RU
				L"确认", //ZH
				L"POTVRDIT", //CZ
				L"ACCEPTER", //FR
				L"ACEPTAR", //ES
				L"ACEITAR", //PT
				L"KABUL ET", //TU
				L"ACCETTA"  //IT
		} ;


		waitForNoButtonPress();

		initDisplay();
	    overlayBatteryStatus(BATT_VALUE_DISPLAY);
		writeEinkDrawUnicodeSingle((unsigned int*)CHANGE_NAME_line0[lang], wcslen(CHANGE_NAME_line0[lang]), COL_1_X, LINE_0_Y);
		writeUnderline(STRIPE_X_START, STRIPE_Y_START, STRIPE_X_END, STRIPE_Y_END);
		writeEinkDrawUnicodeSingle((unsigned int*)CHANGE_NAME_line1[lang], wcslen(CHANGE_NAME_line1[lang]), COL_1_X, LINE_1_Y);
		writeEinkDrawUnicodeSingle((unsigned int*)CHANGE_NAME_line2[lang], wcslen(CHANGE_NAME_line2[lang]), COL_1_X, LINE_2_Y);
		writeEinkDrawUnicodeSingle((unsigned int*)CHANGE_NAME_line3[lang], wcslen(CHANGE_NAME_line3[lang]), COL_1_X, LINE_3_Y);
		writeEinkDrawUnicodeSingle((unsigned int*)udACCEPT_line0[lang], wcslen(udACCEPT_line0[lang]), ACCEPT_X_START, LINE_4_Y);
		writeEinkDrawUnicodeSingle((unsigned int*)udCANCEL_line0[lang], wcslen(udCANCEL_line0[lang]), (DENY_X_START)-((zhSizer*wcslen(udCANCEL_line0[lang]))*8), LINE_4_Y);

		drawX(draw_X_X,draw_X_Y);
		drawCheck(draw_check_X,draw_check_Y);
		display();

		r = waitForButtonPress();
		if (!r){
			showWorking();
		}else{
			writeX_Screen();
			showReady();
		};
	}
	else if (command == ASKUSER_SIGN_MESSAGE)
	{
		const wchar_t SIGN_MESSAGE_line0[][25] = {
				L"SIGN MESSAGE", //EN
				L"NACHRICHT UNTERSCHREIBEN", //DE
				L"ПОДПИСАТЬ СООБЩЕНИЕ", //RU
				L"信息签署", //ZH
				L"PODEPSAT ZPRÁVU", //CZ
				L"SIGNER MESSAGE", //FR
				L"SIGNAR MENSAJE", //ES
				L"SINAL DE MENSAGEM", //PT
				L"MESAJ İMZALA", //TU
				L"FIRMA MESSAGGIO"  //IT
		} ;


		const wchar_t SIGN_MESSAGE_line1[][25] = {
				L"SIGN THE MESSAGE", //EN
				L"NACHRICHT UNTERSCHREIBEN", //DE
				L"ПОДПИСАТЬ СООБЩЕНИЕ", //RU
				L"是否用您的密钥签署信息?", //ZH
				L"PODEPSAT ZPRÁVU", //CZ
				L"SIGNER LE MESSAGE", //FR
				L"SIGNAR EL MENSAJE", //ES
				L"ASSINAR A MENSAGEM", //PT
				L"MESAJI ANAHTARINIZLA", //TU
				L"FIRMA IL MESSAGGIO"  //IT
		} ;


		const wchar_t SIGN_MESSAGE_line2[][25] = {
				L"WITH YOUR KEY?", //EN
				L"MIT IHREN SCHLÜSSEL?", //DE
				L"ВАШИМ КЛЮЧЕМ?", //RU
				L"", //ZH
				L"VAŠIM KLÍČEM?", //CZ
				L"AVEC VOTRE CLÉ?", //FR
				L"CON SU LLAVE?", //ES
				L"COM A SUA CHAVE?", //PT
				L"İMZALAYIN?", //TU
				L"CON LA TUA CHIAVE?"  //IT
		} ;
		const wchar_t udCANCEL_line0[][9] = {
				L"CANCEL", //EN
				L"ABSAGEN", //DE
				L"ОТМЕНА", //RU
				L"退回", //ZH
				L"ZRUŠIT", //CZ
				L"ANNULER", //FR
				L"CANCELAR", //ES
				L"CANCELAR", //PT
				L"İPTAL", //TU
				L"ANNULLA"  //IT
		} ;
		const wchar_t udACCEPT_line0[][12] = {
				L"ACCEPT", //EN
				L"AKZEPTIEREN", //DE
				L"ПОДТВЕРДИТЬ", //RU
				L"确认", //ZH
				L"POTVRDIT", //CZ
				L"ACCEPTER", //FR
				L"ACEPTAR", //ES
				L"ACEITAR", //PT
				L"KABUL ET", //TU
				L"ACCETTA"  //IT
		} ;


		waitForNoButtonPress();

		initDisplay();
	    overlayBatteryStatus(BATT_VALUE_DISPLAY);
		writeEinkDrawUnicodeSingle((unsigned int*)SIGN_MESSAGE_line0[lang], wcslen(SIGN_MESSAGE_line0[lang]), COL_1_X, LINE_0_Y);
		writeUnderline(STRIPE_X_START, STRIPE_Y_START, STRIPE_X_END, STRIPE_Y_END);
		writeEinkDrawUnicodeSingle((unsigned int*)SIGN_MESSAGE_line1[lang], wcslen(SIGN_MESSAGE_line1[lang]), COL_1_X, LINE_1_Y);
		writeEinkDrawUnicodeSingle((unsigned int*)SIGN_MESSAGE_line2[lang], wcslen(SIGN_MESSAGE_line2[lang]), COL_1_X, LINE_2_Y);
		writeEinkDrawUnicodeSingle((unsigned int*)udACCEPT_line0[lang], wcslen(udACCEPT_line0[lang]), ACCEPT_X_START, LINE_4_Y);
		writeEinkDrawUnicodeSingle((unsigned int*)udCANCEL_line0[lang], wcslen(udCANCEL_line0[lang]), (DENY_X_START)-((zhSizer*wcslen(udCANCEL_line0[lang]))*8), LINE_4_Y);

		drawX(draw_X_X,draw_X_Y);
		drawCheck(draw_check_X,draw_check_Y);
		display();

		r = waitForButtonPress();
		if (!r){
			showWorking();
		}else{
			writeX_Screen();
			showReady();
		};
	}
	else if (command == ASKUSER_PRE_SIGN_MESSAGE)
	{
		const wchar_t PRE_SIGN_MESSAGE_line0[][25] = {
				L"SIGN MESSAGE", //EN
				L"NACHRICHT UNTERSCHREIBEN", //DE
				L"ПОДПИСАТЬ СООБЩЕНИЕ", //RU
				L"信息签署", //ZH
				L"PODEPSAT ZPRÁVU", //CZ
				L"SIGNER LE MESSAGE", //FR
				L"SIGNAR MENSAJE", //ES
				L"SINAL DE MENSAGEM", //PT
				L"MESAJ İMZALA", //TU
				L"FIRMA MESSAGGIO"  //IT
		} ;


		const wchar_t PRE_SIGN_MESSAGE_line1[][25] = {
				L"MESSAGE TO BE SIGNED:", //EN
				L"ZU UNTERSCHREIBEN:", //DE
				L"СООБЩЕНИЕ ДЛЯ ПОДПИСАНИЯ", //RU
				L"所签署的信息:", //ZH
				L"ZPRÁVA K PODEPSÁNÍ:", //CZ
				L"MESSAGE QUI SIGNÉ:", //FR
				L"MENSAJE A SIGNAR:", //ES
				L"MENSAGEM A SER ASSINADO:", //PT
				L"İMZALANACAK MESAJ:", //TU
				L"MESSAGGIO DA FIRMARE:"  //IT
		} ;


		const wchar_t PRE_SIGN_MESSAGE_line2[][25] = {
				L"[PRESS CHECK TO SCROLL]", //EN
				L"[CHECK ZUM FORTFAHREN]", //DE
				L"[НАЖМИТЕ ГАЛОЧКУ", //RU
				L"[要浏览请推勾键]", //ZH
				L"[STISKNĚTE CHECK ]", //CZ
				L"[APPUYEZ SUR CHECK", //FR
				L"[PULSE VERIFICAR", //ES
				L"[PRESSIONE A TECLA", //PT
				L"[DEVAM ETMEK İÇİN", //TU
				L"[PREMI CHECK]"  //IT
		} ;

		const wchar_t PRE_SIGN_MESSAGE_line3[][25] = {
				L"", //EN
				L"", //DE
				L"ДЛЯ ПРОСМОТРА]", //RU
				L"", //ZH
				L"", //CZ
				L"POUR FAIRE DÉFILER]", //FR
				L"PARA DESPLAZARSE]", //ES
				L"DE SELEÇÃO PARA ROLAR]", //PT
				L"KONTROLE BASIN]", //TU
				L""  //IT
		} ;
		const wchar_t udCANCEL_line0[][9] = {
				L"CANCEL", //EN
				L"ABSAGEN", //DE
				L"ОТМЕНА", //RU
				L"退回", //ZH
				L"ZRUŠIT", //CZ
				L"ANNULER", //FR
				L"CANCELAR", //ES
				L"CANCELAR", //PT
				L"İPTAL", //TU
				L"ANNULLA"  //IT
		} ;

		const wchar_t udGO_line0[][12] = {
				L"GO", //EN
				L"GEHEN", //DE
				L"ПОДТВЕРДИТЬ", //RU
				L"确认", //ZH
				L"DÁLE", //CZ
				L"CONTINUER", //FR
				L"IR", //ES
				L"IR", //PT
				L"İLERLE", //TU
				L"GO"  //IT
		} ;


		waitForNoButtonPress();

		initDisplay();
	    overlayBatteryStatus(BATT_VALUE_DISPLAY);
		writeEinkDrawUnicodeSingle((unsigned int*)PRE_SIGN_MESSAGE_line0[lang], wcslen(PRE_SIGN_MESSAGE_line0[lang]), COL_1_X, LINE_0_Y);
		writeUnderline(STRIPE_X_START, STRIPE_Y_START, STRIPE_X_END, STRIPE_Y_END);
		writeEinkDrawUnicodeSingle((unsigned int*)PRE_SIGN_MESSAGE_line1[lang], wcslen(PRE_SIGN_MESSAGE_line1[lang]), COL_1_X, LINE_1_Y);
		writeEinkDrawUnicodeSingle((unsigned int*)PRE_SIGN_MESSAGE_line2[lang], wcslen(PRE_SIGN_MESSAGE_line2[lang]), COL_1_X, LINE_2_Y);
		writeEinkDrawUnicodeSingle((unsigned int*)PRE_SIGN_MESSAGE_line3[lang], wcslen(PRE_SIGN_MESSAGE_line3[lang]), COL_1_X, LINE_3_Y);
		writeEinkDrawUnicodeSingle((unsigned int*)udGO_line0[lang], wcslen(udGO_line0[lang]), ACCEPT_X_START, LINE_4_Y);
		writeEinkDrawUnicodeSingle((unsigned int*)udCANCEL_line0[lang], wcslen(udCANCEL_line0[lang]), (DENY_X_START)-((zhSizer*wcslen(udCANCEL_line0[lang]))*8), LINE_4_Y);

		drawX(draw_X_X,draw_X_Y);
		drawCheck(draw_check_X,draw_check_Y);
		display();

		r = waitForButtonPress();
		if (!r){
//			showWorking();
		}else{
			writeX_Screen();
			showReady();
		};
	}




	else
	{
		const wchar_t udCANCEL_line0[][9] = {
				L"CANCEL", //EN
				L"ABSAGEN", //DE
				L"ОТМЕНА", //RU
				L"退回", //ZH
				L"ZRUŠIT", //CZ
				L"ANNULER", //FR
				L"CANCELAR", //ES
				L"CANCELAR", //PT
				L"İPTAL", //TU
				L"ANNULLA"  //IT
		} ;

		waitForNoButtonPress();


				initDisplay();
			    overlayBatteryStatus(BATT_VALUE_DISPLAY);
				writeUnderline(STRIPE_X_START, STRIPE_Y_START, STRIPE_X_END, STRIPE_Y_END);
				writeEinkDrawUnicodeSingle((unsigned int*)udCANCEL_line0[lang], wcslen(udCANCEL_line0[lang]), (DENY_X_START)-((zhSizer*wcslen(udCANCEL_line0[lang]))*8), LINE_4_Y);

				drawX(draw_X_X,draw_X_Y);
				drawCheck(draw_check_X,draw_check_Y);
				display();
		waitForButtonPress();
		r = true; // unconditionally deny
	}

	return r;
}

bool userDeniedSetup(AskUserCommand command)
{
	uint8_t i;
	uint8_t tempLang[1];
	nonVolatileRead(tempLang, DEVICE_LANG_ADDRESS, 1);

	int lang;
	lang = (int)tempLang[0];

	int zhSizer = 1;

	if (lang == 3)
	{
		zhSizer = 2;
	}


	bool r; // what will be returned

	r = true;
	if (command == ASKUSER_DESCRIBE_STANDARD_SETUP)
	{
		const wchar_t DESCRIBE_STANDARD_SETUP_line1[][25] = {
				L"12 WORD MNEMONIC", //EN
				L"12 WORT MNEMONIC", //DE
				L"12 МНЕМОНИЧЕСКИХ СЛОВ", //RU
				L"12个单词助记符", //ZH
				L"12 MNEMOTECHNICKÝCH SLOV", //CZ
				L"MNÉMONIQUE DE 12 MOTS", //FR
				L"12 PALABRAS MNEMÓNICAS", //ES
				L"12 PALAVRA MNEMÔNICA", //PT
				L"12 KELİME MNEMONIC", //TU
				L"12 PAROLE MNEMONICHE"  //IT
		} ;

		const wchar_t DESCRIBE_STANDARD_SETUP_line2[][18] = {
				L"NUMERIC PINs", //EN
				L"NUMERISCHE PIN", //DE
				L"ЦИФРОВЫЕ ПАРОЛИ", //RU
				L"数字密码", //ZH
				L"ČÍSELNÉ PINY", //CZ
				L"NIP NUMÉRIQUES", //FR
				L"CÓDIGOS NUMÉRICOS", //ES
				L"PINS NUMÉRICOS", //PT
				L"NÜMERİK PIN", //TU
				L"PIN NUMERICI"  //IT
		} ;

		const wchar_t DESCRIBE_STANDARD_SETUP_line3[][25] = {
				L"AUTO PIN GENERATION", //EN
				L"AUTO PIN GENERATION", //DE
				L"АВТО ГЕНЕРАЦИЯ ПАРОЛЕЙ", //RU
				L"密码自动生成", //ZH
				L"AUTOM. GENEROVÁNÍ PINU", //CZ
				L"GÉNÉR. AUTO. D'UN NIP", //FR
				L"GENERACIÓN DEL CÓDIGO", //ES
				L"GERAÇÃO PIN AUTO", //PT
				L"OTOMATİK PIN OLUŞTURUCU", //TU
				L"GENERAZIONE PIN AUTOMAT."  //IT
		} ;
		const wchar_t udsBACK_line0[][9] = {
				L"BACK", //EN
				L"ZURÜCK", //DE
				L"НАЗАД", //RU
				L"退回", //ZH
				L"ZPĚT", //CZ
				L"RETOUR", //FR
				L"ATRÁS", //ES
				L"COSTAS", //PT
				L"GERİ", //TU
				L"INDIETRO"  //IT
		} ;
		const wchar_t udsGO_line0[][12] = {
				L"GO", //EN
				L"GEHEN", //DE
				L"ПОДТВЕРДИТЬ", //RU
				L"确认", //ZH
				L"DÁLE", //CZ
				L"CONTINUER", //FR
				L"IR", //ES
				L"IR", //PT
				L"İLERLE", //TU
				L"GO"  //IT
		} ;

		const wchar_t DESCRIBE_STANDARD_SETUP_line0[][25] = {
				L"STANDARD SETUP", //EN
				L"STANDARD EINRICHTUNG", //DE
				L"СТАНДАРТНАЯ НАСТРОЙКА", //RU
				L"常态设置", //ZH
				L"STANDARNí KONFIGURACE", //CZ
				L"CONFIGURATION STANDARD", //FR
				L"CONFIGURACIÓN ESTÁNDAR", //ES
				L"CONFIGURAÇÃO PADRÃO", //PT
				L"STANDART KURULUM", //TU
				L"CONFIGURAZIONE STANDARD"  //IT
		} ;


		waitForNoButtonPress();

		initDisplay();
	    overlayBatteryStatus(BATT_VALUE_DISPLAY);
		writeEinkDrawUnicodeSingle((unsigned int*)DESCRIBE_STANDARD_SETUP_line0[lang], wcslen(DESCRIBE_STANDARD_SETUP_line0[lang]), COL_1_X, LINE_0_Y);
		writeUnderline(STRIPE_X_START, STRIPE_Y_START, STRIPE_X_END, STRIPE_Y_END);
		writeEinkDrawUnicodeSingle((unsigned int*)DESCRIBE_STANDARD_SETUP_line1[lang], wcslen(DESCRIBE_STANDARD_SETUP_line1[lang]), COL_1_X, LINE_1_Y);
		writeEinkDrawUnicodeSingle((unsigned int*)DESCRIBE_STANDARD_SETUP_line2[lang], wcslen(DESCRIBE_STANDARD_SETUP_line2[lang]), COL_1_X, LINE_2_Y);
		writeEinkDrawUnicodeSingle((unsigned int*)DESCRIBE_STANDARD_SETUP_line3[lang], wcslen(DESCRIBE_STANDARD_SETUP_line3[lang]), COL_1_X, LINE_3_Y);
		writeEinkDrawUnicodeSingle((unsigned int*)udsGO_line0[lang], wcslen(udsGO_line0[lang]), ACCEPT_X_START, LINE_4_Y);
		writeEinkDrawUnicodeSingle((unsigned int*)udsBACK_line0[lang], wcslen(udsBACK_line0[lang]), (DENY_X_START)-((zhSizer*wcslen(udsBACK_line0[lang]))*8), LINE_4_Y);

		drawX(draw_X_X,draw_X_Y);
		drawCheck(draw_check_X,draw_check_Y);
		display();

//		r = waitForButtonPress();
//		if (!r){
//			clearDisplay();
//		}else{
//			writeX_Screen();
//			showReady();
//		};
	}
	else if (command == ASKUSER_DESCRIBE_STANDARD_SETUP_2)
	{

		const wchar_t DESCRIBE_STANDARD_SETUP_2_line1[][25] = {
				L"DIGITS IN DEVICE PIN", //EN
				L"NUMMERN IN GERÄTE-PIN", //DE
				L"КОЛИЧЕСТВО ЦИФР", //RU
				L"设具数字密码位数", //ZH
				L"ČÍSLICE PINu ZAŘÍZENÍ", //CZ
				L"LES CHIFFRES DU NIP", //FR
				L"DIGITOS DEL CÓDIGO", //ES
				L"DÍGITOS NO PIN", //PT
				L"CİHAZ PIN NUMARASI", //TU
				L"CIFRE DEL PIN"  //IT
		} ;

		const wchar_t DESCRIBE_STANDARD_SETUP_2_line2[][25] = {
				L"CHOOSE: 4, 5, 6, 7 OR 8", //EN
				L"WÄHLEN: 4, 5, 6, 7, 8", //DE
				L"В ПАРОЛЕ УСТРОЙСТВА", //RU
				L"选择4 5 6 7或8位数密码", //ZH
				L"VYBER: 4, 5, 6, 7 NEBO 8", //CZ
				L"L'APPAREIL", //FR
				L"DE DISPOSITIVO", //ES
				L"DO DISPOSITIVO", //PT
				L"SEÇİN:4, 5, 6, 7 YA DA 8", //TU
				L"DEL DISPOSITIVO"  //IT
		} ;

		const wchar_t DESCRIBE_STANDARD_SETUP_2_line3[][25] = {
				L"", //EN
				L"", //DE
				L"ВЫБРАТЬ 4, 5, 6, 7 ИЛИ 8", //RU
				L"", //ZH
				L"", //CZ
				L"CHOISISSEZ 4, 5, 6, 7, 8", //FR
				L"ELEGIR: 4, 5, 6, 7 u 8", //ES
				L"ESCOLHA: 4, 5, 6, 7 ou 8", //PT
				L"", //TU
				L"SCELTA: 4, 5, 6, 7 o 8"  //IT
		} ;
		const wchar_t udsBACK_line0[][9] = {
				L"BACK", //EN
				L"ZURÜCK", //DE
				L"НАЗАД", //RU
				L"退回", //ZH
				L"ZPĚT", //CZ
				L"RETOUR", //FR
				L"ATRÁS", //ES
				L"COSTAS", //PT
				L"GERİ", //TU
				L"INDIETRO"  //IT
		} ;

		const wchar_t DESCRIBE_STANDARD_SETUP_line0[][25] = {
				L"STANDARD SETUP", //EN
				L"STANDARD EINRICHTUNG", //DE
				L"СТАНДАРТНАЯ НАСТРОЙКА", //RU
				L"常态设置", //ZH
				L"STANDARNí KONFIGURACE", //CZ
				L"CONFIGURATION STANDARD", //FR
				L"CONFIGURACIÓN ESTÁNDAR", //ES
				L"CONFIGURAÇÃO PADRÃO", //PT
				L"STANDART KURULUM", //TU
				L"CONFIGURAZIONE STANDARD"  //IT
		} ;

		waitForNoButtonPress();


		initDisplay();
	    overlayBatteryStatus(BATT_VALUE_DISPLAY);
		writeEinkDrawUnicodeSingle((unsigned int*)DESCRIBE_STANDARD_SETUP_line0[lang], wcslen(DESCRIBE_STANDARD_SETUP_line0[lang]), COL_1_X, LINE_0_Y);
		writeUnderline(STRIPE_X_START, STRIPE_Y_START, STRIPE_X_END, STRIPE_Y_END);
		writeEinkDrawUnicodeSingle((unsigned int*)DESCRIBE_STANDARD_SETUP_2_line1[lang], wcslen(DESCRIBE_STANDARD_SETUP_2_line1[lang]), COL_1_X, LINE_1_Y);
		writeEinkDrawUnicodeSingle((unsigned int*)DESCRIBE_STANDARD_SETUP_2_line2[lang], wcslen(DESCRIBE_STANDARD_SETUP_2_line2[lang]), COL_1_X, LINE_2_Y);
		writeEinkDrawUnicodeSingle((unsigned int*)DESCRIBE_STANDARD_SETUP_2_line3[lang], wcslen(DESCRIBE_STANDARD_SETUP_2_line3[lang]), COL_1_X, LINE_3_Y);
//		writeEinkDrawUnicodeSingle((unsigned int*)GO_line0[lang], wcslen(GO_line0[lang]), ACCEPT_X_START, 80);
		writeEinkDrawUnicodeSingle((unsigned int*)udsBACK_line0[lang], wcslen(udsBACK_line0[lang]), (DENY_X_START)-((zhSizer*wcslen(udsBACK_line0[lang]))*8), LINE_4_Y);

		drawX(draw_X_X,draw_X_Y);
//		drawCheck(draw_check_X,draw_check_Y);
		display();

//		r = waitForButtonPress();
//		if (!r){
//			clearDisplay();
//		}else{
//			writeX_Screen();
//			showReady();
//		};
	}
	else if (command == ASKUSER_DESCRIBE_ADVANCED_SETUP)
	{
		const wchar_t DESCRIBE_ADVANCED_SETUP_line1[][25] = {
				L"18 WORD MNEMONIC", //EN
				L"18 WORT MNEMONIC", //DE
				L"18 МНЕМОНИЧЕСКИХ СЛОВ", //RU
				L"18个单词助记符", //ZH
				L"18 MNEMOTECHNICKÝCH SLOV", //CZ
				L"MNÉMONIQUE DE 18 MOTS", //FR
				L"18 PALABRAS MNEMÓNICAS", //ES
				L"18 PALAVRA MNEMÔNICA", //PT
				L"18 KELİME MNEMONIC", //TU
				L"18 PAROLE MNEMONICHE"  //IT
		} ;

		const wchar_t DESCRIBE_ADVANCED_SETUP_line2[][25] = {
				L"ALPHANUMERIC PINS", //EN
				L"ALPHANUMERISCHE PINs", //DE
				L"БУКВЕННО-ЦИФРОВЫЕ ПАРОЛИ", //RU
				L"字母与数字混合密码", //ZH
				L"ALFANUMERICKÉ PINy", //CZ
				L"NIP ALPHANUMÉRIQUE", //FR
				L"CODIGOS ALFANUMÉRICOS", //ES
				L"PINs ALFANUMERICOS", //PT
				L"ALFANÜMERİK PIN", //TU
				L"PIN ALFANUMERICI"  //IT
		} ;

		const wchar_t DESCRIBE_ADVANCED_SETUP_line3[][25] = {
				L"MANUAL PIN ENTRY", //EN
				L"MANUELLE EINGABE DER PIN", //DE
				L"РУЧНОЙ ВВОД ПАРОЛЯ", //RU
				L"手动密码输入", //ZH
				L"RUČNÍ ZADÁNÍ PINu", //CZ
				L"SAISIE MANUELLE DU NIP", //FR
				L"INTRO. MANUAL DEL CÓDIGO", //ES
				L"ENTRADA DO PIN MANUAL", //PT
				L"MANUEL PIN GİRİŞİ", //TU
				L"INSER. MANUALE DEL PIN"  //IT
		} ;
		const wchar_t udsBACK_line0[][9] = {
				L"BACK", //EN
				L"ZURÜCK", //DE
				L"НАЗАД", //RU
				L"退回", //ZH
				L"ZPĚT", //CZ
				L"RETOUR", //FR
				L"ATRÁS", //ES
				L"COSTAS", //PT
				L"GERİ", //TU
				L"INDIETRO"  //IT
		} ;
		const wchar_t udsGO_line0[][12] = {
				L"GO", //EN
				L"GEHEN", //DE
				L"ПОДТВЕРДИТЬ", //RU
				L"确认", //ZH
				L"DÁLE", //CZ
				L"CONTINUER", //FR
				L"IR", //ES
				L"IR", //PT
				L"İLERLE", //TU
				L"GO"  //IT
		} ;

		const wchar_t DESCRIBE_ADVANCED_SETUP_line0[][25] = {
				L"ADVANCED SETUP", //EN
				L"ERWEITERTE KONFIGURATION", //DE
				L"РАСШИРЕННАЯ НАСТРОЙКА", //RU
				L"高级设置", //ZH
				L"POKROČILÁ KONFIGURACE", //CZ
				L"CONFIGURATION AVANCÉE", //FR
				L"CONFIGURACIÓN AVANZADA", //ES
				L"CONFIGURAÇÃO AVANÇADA", //PT
				L"GELİŞMİŞ KURULUM", //TU
				L"IMPOSTAZIONI AVANZATE"  //IT
		} ;


		waitForNoButtonPress();

		initDisplay();
	    overlayBatteryStatus(BATT_VALUE_DISPLAY);
		writeEinkDrawUnicodeSingle((unsigned int*)DESCRIBE_ADVANCED_SETUP_line0[lang], wcslen(DESCRIBE_ADVANCED_SETUP_line0[lang]), COL_1_X, LINE_0_Y);
		writeUnderline(STRIPE_X_START, STRIPE_Y_START, STRIPE_X_END, STRIPE_Y_END);
		writeEinkDrawUnicodeSingle((unsigned int*)DESCRIBE_ADVANCED_SETUP_line1[lang], wcslen(DESCRIBE_ADVANCED_SETUP_line1[lang]), COL_1_X, LINE_1_Y);
		writeEinkDrawUnicodeSingle((unsigned int*)DESCRIBE_ADVANCED_SETUP_line2[lang], wcslen(DESCRIBE_ADVANCED_SETUP_line2[lang]), COL_1_X, LINE_2_Y);
		writeEinkDrawUnicodeSingle((unsigned int*)DESCRIBE_ADVANCED_SETUP_line3[lang], wcslen(DESCRIBE_ADVANCED_SETUP_line3[lang]), COL_1_X, LINE_3_Y);
		writeEinkDrawUnicodeSingle((unsigned int*)udsGO_line0[lang], wcslen(udsGO_line0[lang]), ACCEPT_X_START, LINE_4_Y);
		writeEinkDrawUnicodeSingle((unsigned int*)udsBACK_line0[lang], wcslen(udsBACK_line0[lang]), (DENY_X_START)-((zhSizer*wcslen(udsBACK_line0[lang]))*8), LINE_4_Y);


		drawX(draw_X_X,draw_X_Y);
		drawCheck(draw_check_X,draw_check_Y);
		display();

//		r = waitForButtonPress();
//		if (!r){
//			clearDisplay();
//		}else{
//			writeX_Screen();
//			showReady();
//		};
	}
	else if (command == ASKUSER_DESCRIBE_ADVANCED_SETUP_2)
	{
		const wchar_t udsBACK_line0[][9] = {
				L"BACK", //EN
				L"ZURÜCK", //DE
				L"НАЗАД", //RU
				L"退回", //ZH
				L"ZPĚT", //CZ
				L"RETOUR", //FR
				L"ATRÁS", //ES
				L"COSTAS", //PT
				L"GERİ", //TU
				L"INDIETRO"  //IT
		} ;
		const wchar_t udsGO_line0[][12] = {
				L"GO", //EN
				L"GEHEN", //DE
				L"ПОДТВЕРДИТЬ", //RU
				L"确认", //ZH
				L"DÁLE", //CZ
				L"CONTINUER", //FR
				L"IR", //ES
				L"IR", //PT
				L"İLERLE", //TU
				L"GO"  //IT
		} ;

		const wchar_t DESCRIBE_ADVANCED_SETUP_line0[][25] = {
				L"ADVANCED SETUP", //EN
				L"ERWEITERTE KONFIGURATION", //DE
				L"РАСШИРЕННАЯ НАСТРОЙКА", //RU
				L"高级设置", //ZH
				L"POKROČILÁ KONFIGURACE", //CZ
				L"CONFIGURATION AVANCÉE", //FR
				L"CONFIGURACIÓN AVANZADA", //ES
				L"CONFIGURAÇÃO AVANÇADA", //PT
				L"GELİŞMİŞ KURULUM", //TU
				L"IMPOSTAZIONI AVANZATE"  //IT
		} ;

		const wchar_t DESCRIBE_ADVANCED_SETUP_2_line1[][25] = {
				L"TO SET PIN", //EN
				L"UM PIN KONFIG", //DE
				L"УСТАНОВКА ПАРОЛЯ УСТ-ВА", //RU
				L"设置设具密码", //ZH
				L"NASTAVENÍ PINu", //CZ
				L"POUR RÉGLER LE NIP", //FR
				L"CREAR CÓDIGO DEL DISPOS.", //ES
				L"PARA DEFINIR PIN DEVICE", //PT
				L"KODUNU AYARLAMAK İÇİN", //TU
				L"PER IMPOSTARE IL PIN"  //IT
		} ;
		const wchar_t DESCRIBE_ADVANCED_SETUP_2_line2[][25] = {
				L"-PRESS AND HOLD TO CYCLE", //EN
				L"-DRÜCKEN UND ANHALTEN", //DE
				L"ДЕРЖИТЕ КНОПКУ: ПРОСМОТР", //RU
				L"推下不放浏览字符", //ZH
				L"-PODRŽENÍM ROTUJTE", //CZ
				L"-APPUYEZ ET MAINTENEZ", //FR
				L"-PULSAR Y MANTENER", //ES
				L"-PRESSIONE E SEGURE", //PT
				L"-DÖNGÜYE BASILI TUTUN", //TU
				L"TIENI PREMUTO LE LETTERE"  //IT
		} ;
		const wchar_t DESCRIBE_ADVANCED_SETUP_2_line3[][25] = {
				L"-RELEASE TO SELECT", //EN
				L"-LOSLASSEN ZUR AUSWAHL", //DE
				L"ОТПУСТИТЕ: ВЫБОР", //RU
				L"放开即可选择字符", //ZH
				L"-UVOLNĚNÍM VYBERTE", //CZ
				L"-RELÂCHEZ POUR SÉLECTION", //FR
				L"-SOLTAR PARA SELECCIONAR", //ES
				L"-SOLTE PARA SELECIONAR", //PT
				L"-SEÇMEK İÇİN BIRAKIN", //TU
				L"RILASCIA PER SELEZIONARE"  //IT
		} ;


		waitForNoButtonPress();

		initDisplay();
	    overlayBatteryStatus(BATT_VALUE_DISPLAY);
		writeEinkDrawUnicodeSingle((unsigned int*)DESCRIBE_ADVANCED_SETUP_line0[lang], wcslen(DESCRIBE_ADVANCED_SETUP_line0[lang]), COL_1_X, LINE_0_Y);
		writeUnderline(STRIPE_X_START, STRIPE_Y_START, STRIPE_X_END, STRIPE_Y_END);
		writeEinkDrawUnicodeSingle((unsigned int*)DESCRIBE_ADVANCED_SETUP_2_line1[lang], wcslen(DESCRIBE_ADVANCED_SETUP_2_line1[lang]), COL_1_X, LINE_1_Y);
		writeEinkDrawUnicodeSingle((unsigned int*)DESCRIBE_ADVANCED_SETUP_2_line2[lang], wcslen(DESCRIBE_ADVANCED_SETUP_2_line2[lang]), COL_1_X, LINE_2_Y);
		writeEinkDrawUnicodeSingle((unsigned int*)DESCRIBE_ADVANCED_SETUP_2_line3[lang], wcslen(DESCRIBE_ADVANCED_SETUP_2_line3[lang]), COL_1_X, LINE_3_Y);
		writeEinkDrawUnicodeSingle((unsigned int*)udsGO_line0[lang], wcslen(udsGO_line0[lang]), ACCEPT_X_START, LINE_4_Y);
		writeEinkDrawUnicodeSingle((unsigned int*)udsBACK_line0[lang], wcslen(udsBACK_line0[lang]), (DENY_X_START)-((zhSizer*wcslen(udsBACK_line0[lang]))*8), LINE_4_Y);


		drawX(draw_X_X,draw_X_Y);
		drawCheck(draw_check_X,draw_check_Y);
		display();

//		r = waitForButtonPress();
//		if (!r){
//			clearDisplay();
//		}else{
//			writeX_Screen();
//			showReady();
//		};
	}
	else if (command == ASKUSER_DESCRIBE_EXPERT_SETUP)
	{
		const wchar_t DESCRIBE_EXPERT_SETUP_line1[][25] = {
				L"24 WORD MNEMONIC", //EN
				L"24 WORT MNEMONIC", //DE
				L"24 МНЕМОНИЧЕСКИХ СЛОВ", //RU
				L"24个单词助记符", //ZH
				L"24 MNEMOTECHNICKÝCH SLOV", //CZ
				L"MNÉMONIQUE DE 24 MOTS", //FR
				L"24 PALABRAS MNEMÓNICAS", //ES
				L"24 PALAVRA MNEMÔNICA", //PT
				L"24 KELİME MNEMONIC", //TU
				L"24 PAROLE MNEMONICHE"  //IT
		} ;

		const wchar_t DESCRIBE_EXPERT_SETUP_line2[][25] = {
				L"ALPHANUMERIC PINS", //EN
				L"ALPHANUMERISCHE PINs", //DE
				L"БУКВЕННО-ЦИФРОВЫЕ ПАРОЛИ", //RU
				L"字母与数字混合密码", //ZH
				L"ALFANUMERICKÉ PINy", //CZ
				L"NIP ALPHANUMÉRIQUE", //FR
				L"CODIGOS ALFANUMÉRICOS", //ES
				L"PINs ALFANUMERICOS", //PT
				L"ALFANÜMERİK PIN", //TU
				L"PIN ALFANUMERICI"  //IT
		} ;

		const wchar_t DESCRIBE_EXPERT_SETUP_line3[][25] = {
				L"MANUAL PIN ENTRY", //EN
				L"MANUELLE EINGABE", //DE
				L"РУЧНОЙ ВВОД ПАРОЛЯ", //RU
				L"手动密码输入", //ZH
				L"RUČNÍ ZADÁNÍ PINu", //CZ
				L"SAISIE MANUELLE DU NIP", //FR
				L"INTRO. MANUAL DEL CÓDIGO", //ES
				L"ENTRADA DO PIN MANUAL", //PT
				L"MANUEL PIN GİRİŞİ", //TU
				L"INSERIMENTO MANUALE"  //IT
		} ;
		const wchar_t udsBACK_line0[][9] = {
				L"BACK", //EN
				L"ZURÜCK", //DE
				L"НАЗАД", //RU
				L"退回", //ZH
				L"ZPĚT", //CZ
				L"RETOUR", //FR
				L"ATRÁS", //ES
				L"COSTAS", //PT
				L"GERİ", //TU
				L"INDIETRO"  //IT
		} ;
		const wchar_t udsGO_line0[][12] = {
				L"GO", //EN
				L"GEHEN", //DE
				L"ПОДТВЕРДИТЬ", //RU
				L"确认", //ZH
				L"DÁLE", //CZ
				L"CONTINUER", //FR
				L"IR", //ES
				L"IR", //PT
				L"İLERLE", //TU
				L"GO"  //IT
		} ;
		const wchar_t DESCRIBE_EXPERT_SETUP_line0[][25] = {
				L"EXPERT SETUP", //EN
				L"EXPERT KONFIGURATION", //DE
				L"ЭКСПЕРТНЫЕ НАСТРОЙКИ", //RU
				L"专家设置", //ZH
				L"EXPERT KONFIGURACE", //CZ
				L"CONFIGURATION EXPERT", //FR
				L"CONFIGURACIÓN EXPERTO", //ES
				L"INSTALAÇÃO DE ESPECIAL.", //PT
				L"UZMAN KURULUMU", //TU
				L"IMPOSTAZIONIE ESPERTO"  //IT
		} ;




		waitForNoButtonPress();

		initDisplay();
	    overlayBatteryStatus(BATT_VALUE_DISPLAY);
		writeEinkDrawUnicodeSingle((unsigned int*)DESCRIBE_EXPERT_SETUP_line0[lang], wcslen(DESCRIBE_EXPERT_SETUP_line0[lang]), COL_1_X, LINE_0_Y);
		writeUnderline(STRIPE_X_START, STRIPE_Y_START, STRIPE_X_END, STRIPE_Y_END);
		writeEinkDrawUnicodeSingle((unsigned int*)DESCRIBE_EXPERT_SETUP_line1[lang], wcslen(DESCRIBE_EXPERT_SETUP_line1[lang]), COL_1_X, LINE_1_Y);
		writeEinkDrawUnicodeSingle((unsigned int*)DESCRIBE_EXPERT_SETUP_line2[lang], wcslen(DESCRIBE_EXPERT_SETUP_line2[lang]), COL_1_X, LINE_2_Y);
		writeEinkDrawUnicodeSingle((unsigned int*)DESCRIBE_EXPERT_SETUP_line3[lang], wcslen(DESCRIBE_EXPERT_SETUP_line3[lang]), COL_1_X, LINE_3_Y);
		writeEinkDrawUnicodeSingle((unsigned int*)udsGO_line0[lang], wcslen(udsGO_line0[lang]), ACCEPT_X_START, LINE_4_Y);
		writeEinkDrawUnicodeSingle((unsigned int*)udsBACK_line0[lang], wcslen(udsBACK_line0[lang]), (DENY_X_START)-((zhSizer*wcslen(udsBACK_line0[lang]))*8), LINE_4_Y);

		drawX(draw_X_X,draw_X_Y);
		drawCheck(draw_check_X,draw_check_Y);
		display();

//		r = waitForButtonPress();
//		if (!r){
//			clearDisplay();
//		}else{
//			writeX_Screen();
//			showReady();
//		};
	}
	else if (command == ASKUSER_DESCRIBE_EXPERT_SETUP_2)
	{
		const wchar_t udsBACK_line0[][9] = {
				L"BACK", //EN
				L"ZURÜCK", //DE
				L"НАЗАД", //RU
				L"退回", //ZH
				L"ZPĚT", //CZ
				L"RETOUR", //FR
				L"ATRÁS", //ES
				L"COSTAS", //PT
				L"GERİ", //TU
				L"INDIETRO"  //IT
		} ;

		const wchar_t udsGO_line0[][12] = {
				L"GO", //EN
				L"GEHEN", //DE
				L"ПОДТВЕРДИТЬ", //RU
				L"确认", //ZH
				L"DÁLE", //CZ
				L"CONTINUER", //FR
				L"IR", //ES
				L"IR", //PT
				L"İLERLE", //TU
				L"GO"  //IT
		} ;
		const wchar_t DESCRIBE_EXPERT_SETUP_line0[][25] = {
				L"EXPERT SETUP", //EN
				L"EXPERT KONFIGURATION", //DE
				L"ЭКСПЕРТНЫЕ НАСТРОЙКИ", //RU
				L"专家设置", //ZH
				L"EXPERT KONFIGURACE", //CZ
				L"CONFIGURATION EXPERT", //FR
				L"CONFIGURACIÓN EXPERTO", //ES
				L"INSTALAÇÃO DE ESPECIAL.", //PT
				L"UZMAN KURULUMU", //TU
				L"IMPOSTAZIONIE ESPERTO"  //IT
		} ;

		const wchar_t DESCRIBE_ADVANCED_SETUP_2_line1[][25] = {
				L"TO SET PIN", //EN
				L"UM PIN KONFIG", //DE
				L"УСТАНОВКА ПАРОЛЯ УСТ-ВА", //RU
				L"设置设具密码", //ZH
				L"NASTAVENÍ PINu", //CZ
				L"POUR RÉGLER LE NIP", //FR
				L"CREAR CÓDIGO DEL DISPOS.", //ES
				L"PARA DEFINIR PIN DEVICE", //PT
				L"KODUNU AYARLAMAK İÇİN", //TU
				L"PER IMPOSTARE IL PIN"  //IT
		} ;

		const wchar_t DESCRIBE_ADVANCED_SETUP_2_line2[][25] = {
				L"-PRESS AND HOLD TO CYCLE", //EN
				L"-DRÜCKEN UND ANHALTEN", //DE
				L"ДЕРЖИТЕ КНОПКУ: ПРОСМОТР", //RU
				L"推下不放浏览字符", //ZH
				L"-PODRŽENÍM ROTUJTE", //CZ
				L"-APPUYEZ ET MAINTENEZ", //FR
				L"-PULSAR Y MANTENER", //ES
				L"-PRESSIONE E SEGURE", //PT
				L"-DÖNGÜYE BASILI TUTUN", //TU
				L"TIENI PREMUTO LE LETTERE"  //IT
		} ;
		const wchar_t DESCRIBE_ADVANCED_SETUP_2_line3[][25] = {
				L"-RELEASE TO SELECT", //EN
				L"-LOSLASSEN ZUR AUSWAHL", //DE
				L"ОТПУСТИТЕ: ВЫБОР", //RU
				L"放开即可选择字符", //ZH
				L"-UVOLNĚNÍM VYBERTE", //CZ
				L"-RELÂCHEZ POUR SÉLECTION", //FR
				L"-SOLTAR PARA SELECCIONAR", //ES
				L"-SOLTE PARA SELECIONAR", //PT
				L"-SEÇMEK İÇİN BIRAKIN", //TU
				L"RILASCIA PER SELEZIONARE"  //IT
		} ;


		waitForNoButtonPress();


		initDisplay();
	    overlayBatteryStatus(BATT_VALUE_DISPLAY);
		writeEinkDrawUnicodeSingle((unsigned int*)DESCRIBE_EXPERT_SETUP_line0[lang], wcslen(DESCRIBE_EXPERT_SETUP_line0[lang]), COL_1_X, LINE_0_Y);
		writeUnderline(STRIPE_X_START, STRIPE_Y_START, STRIPE_X_END, STRIPE_Y_END);
		writeEinkDrawUnicodeSingle((unsigned int*)DESCRIBE_ADVANCED_SETUP_2_line1[lang], wcslen(DESCRIBE_ADVANCED_SETUP_2_line1[lang]), COL_1_X, LINE_1_Y);
		writeEinkDrawUnicodeSingle((unsigned int*)DESCRIBE_ADVANCED_SETUP_2_line2[lang], wcslen(DESCRIBE_ADVANCED_SETUP_2_line2[lang]), COL_1_X, LINE_2_Y);
		writeEinkDrawUnicodeSingle((unsigned int*)DESCRIBE_ADVANCED_SETUP_2_line3[lang], wcslen(DESCRIBE_ADVANCED_SETUP_2_line3[lang]), COL_1_X, LINE_3_Y);
		writeEinkDrawUnicodeSingle((unsigned int*)udsGO_line0[lang], wcslen(udsGO_line0[lang]), ACCEPT_X_START, LINE_4_Y);
		writeEinkDrawUnicodeSingle((unsigned int*)udsBACK_line0[lang], wcslen(udsBACK_line0[lang]), (DENY_X_START)-((zhSizer*wcslen(udsBACK_line0[lang]))*8), LINE_4_Y);

		drawX(draw_X_X,draw_X_Y);
		drawCheck(draw_check_X,draw_check_Y);
		display();

//		r = waitForButtonPress();
//		if (!r){
//			clearDisplay();
//		}else{
//			writeX_Screen();
//			showReady();
//		};
	}
	else if (command == ASKUSER_DESCRIBE_STANDARD_SETUP_2_WALLET)
	{
		const wchar_t DESCRIBE_STANDARD_SETUP_2_WALLET_line1[][25] = {
				L"DIGITS IN WALLET PIN", //EN
				L"NUMMERN IN WALLET PIN", //DE
				L"КОЛИЧЕСТВО ЦИФР", //RU
				L"钱袋数字密码位数", //ZH
				L"ČÍSLICE V PINU PENĚŽENKY", //CZ
				L"LES CHIFFRES DU NIP", //FR
				L"DIGITOS DEL CÓDIGO", //ES
				L"DÍGITOS PIN DA CARTEIRA", //PT
				L"CÜZDAN PIN NUMARASI", //TU
				L"CIFRE DEL PIN"  //IT
		} ;

		const wchar_t DESCRIBE_STANDARD_SETUP_2_WALLET_line2[][25] = {
				L"CHOOSE: 4, 5, 6, 7 OR 8", //EN
				L"WÄHLEN: 4, 5, 6, 7, 8", //DE
				L"В ПАРОЛЕ КОШЕЛЬКА", //RU
				L"选择：4，5，6，7或8位数密码", //ZH
				L"VYBER: 4, 5, 6, 7 NEBO 8", //CZ
				L"DE LE PORTEFEUILLE", //FR
				L"DE SU CARTERA", //ES
				L"ESCOLHA: 4, 5, 6, 7 ou 8", //PT
				L"SEÇİN:4, 5, 6, 7 YA DA 8", //TU
				L"PER IL PORTAFOGLIO"  //IT
		} ;

		const wchar_t DESCRIBE_STANDARD_SETUP_2_WALLET_line3[][25] = {
				L"", //EN
				L"", //DE
				L"ВЫБРАТЬ 4, 5, 6, 7 ИЛИ 8", //RU
				L"", //ZH
				L"", //CZ
				L"CHOISISSEZ 4, 5, 6, 7, 8", //FR
				L"ELEGIR: 4, 5, 6, 7 u 8", //ES
				L"", //PT
				L"", //TU
				L"SCELTA: 4, 5, 6, 7 o 8"  //IT
		} ;

		const wchar_t udsCANCEL_line0[][9] = {
				L"CANCEL", //EN
				L"ABSAGEN", //DE
				L"ОТМЕНА", //RU
				L"退回", //ZH
				L"ZRUŠIT", //CZ
				L"ANNULER", //FR
				L"CANCELAR", //ES
				L"CANCELAR", //PT
				L"İPTAL", //TU
				L"ANNULLA"  //IT
		} ;
		const wchar_t DESCRIBE_ADVANCED_SETUP_2_WALLET_line0[][25] = {
				L"WALLET SETUP", //EN
				L"WALLET KONFIGURATION", //DE
				L"СОЗДАНИЕ КОШЕЛЬКА", //RU
				L"创建钱袋", //ZH
				L"KONFIGURACE PENĚŽENKY", //CZ
				L"CONF. DE PORTEFEUILLE", //FR
				L"CONF. DE LA CARTERA", //ES
				L"CARTEIRA DE INSTALAÇÃO", //PT
				L"CÜZDAN KURULUMU", //TU
				L"IMPOSTAZIONI PORTAFOGLIO"  //IT
		} ;

		waitForNoButtonPress();

		initDisplay();
	    overlayBatteryStatus(BATT_VALUE_DISPLAY);
		writeEinkDrawUnicodeSingle((unsigned int*)DESCRIBE_ADVANCED_SETUP_2_WALLET_line0[lang], wcslen(DESCRIBE_ADVANCED_SETUP_2_WALLET_line0[lang]), COL_1_X, LINE_0_Y);
		writeUnderline(STRIPE_X_START, STRIPE_Y_START, STRIPE_X_END, STRIPE_Y_END);
		writeEinkDrawUnicodeSingle((unsigned int*)DESCRIBE_STANDARD_SETUP_2_WALLET_line1[lang], wcslen(DESCRIBE_STANDARD_SETUP_2_WALLET_line1[lang]), COL_1_X, LINE_1_Y);
		writeEinkDrawUnicodeSingle((unsigned int*)DESCRIBE_STANDARD_SETUP_2_WALLET_line2[lang], wcslen(DESCRIBE_STANDARD_SETUP_2_WALLET_line2[lang]), COL_1_X, LINE_2_Y);
		writeEinkDrawUnicodeSingle((unsigned int*)DESCRIBE_STANDARD_SETUP_2_WALLET_line3[lang], wcslen(DESCRIBE_STANDARD_SETUP_2_WALLET_line3[lang]), COL_1_X, LINE_3_Y);
//		writeEinkDrawUnicodeSingle((unsigned int*)GO_line0[lang], wcslen(GO_line0[lang]), ACCEPT_X_START, 80);
		writeEinkDrawUnicodeSingle((unsigned int*)udsCANCEL_line0[lang], wcslen(udsCANCEL_line0[lang]), (DENY_X_START)-((zhSizer*wcslen(udsCANCEL_line0[lang]))*8), LINE_4_Y);

		drawX(draw_X_X,draw_X_Y);
//		drawCheck(draw_check_X,draw_check_Y);
		display();

//		r = waitForButtonPress();
//		if (!r){
//			clearDisplay();
//		}else{
//			writeX_Screen();
//			showReady();
//		};
	}
	else if (command == ASKUSER_DESCRIBE_ADVANCED_SETUP_2_WALLET)
	{
		const wchar_t udsBACK_line0[][9] = {
				L"BACK", //EN
				L"ZURÜCK", //DE
				L"НАЗАД", //RU
				L"退回", //ZH
				L"ZPĚT", //CZ
				L"RETOUR", //FR
				L"ATRÁS", //ES
				L"COSTAS", //PT
				L"GERİ", //TU
				L"INDIETRO"  //IT
		} ;
		const wchar_t udsGO_line0[][12] = {
				L"GO", //EN
				L"GEHEN", //DE
				L"ПОДТВЕРДИТЬ", //RU
				L"确认", //ZH
				L"DÁLE", //CZ
				L"CONTINUER", //FR
				L"IR", //ES
				L"IR", //PT
				L"İLERLE", //TU
				L"GO"  //IT
		} ;

		const wchar_t DESCRIBE_ADVANCED_SETUP_2_line1[][25] = {
				L"TO SET PIN", //EN
				L"UM PIN KONFIG", //DE
				L"УСТАНОВКА ПАРОЛЯ УСТ-ВА", //RU
				L"设置设具密码", //ZH
				L"NASTAVENÍ PINu", //CZ
				L"POUR RÉGLER LE NIP", //FR
				L"CREAR CÓDIGO DEL DISPOS.", //ES
				L"PARA DEFINIR PIN DEVICE", //PT
				L"KODUNU AYARLAMAK İÇİN", //TU
				L"PER IMPOSTARE IL PIN"  //IT
		} ;
		const wchar_t DESCRIBE_ADVANCED_SETUP_2_line2[][25] = {
				L"-PRESS AND HOLD TO CYCLE", //EN
				L"-DRÜCKEN UND ANHALTEN", //DE
				L"ДЕРЖИТЕ КНОПКУ: ПРОСМОТР", //RU
				L"推下不放浏览字符", //ZH
				L"-PODRŽENÍM ROTUJTE", //CZ
				L"-APPUYEZ ET MAINTENEZ", //FR
				L"-PULSAR Y MANTENER", //ES
				L"-PRESSIONE E SEGURE", //PT
				L"-DÖNGÜYE BASILI TUTUN", //TU
				L"TIENI PREMUTO LE LETTERE"  //IT
		} ;
		const wchar_t DESCRIBE_ADVANCED_SETUP_2_line3[][25] = {
				L"-RELEASE TO SELECT", //EN
				L"-LOSLASSEN ZUR AUSWAHL", //DE
				L"ОТПУСТИТЕ: ВЫБОР", //RU
				L"放开即可选择字符", //ZH
				L"-UVOLNĚNÍM VYBERTE", //CZ
				L"-RELÂCHEZ POUR SÉLECTION", //FR
				L"-SOLTAR PARA SELECCIONAR", //ES
				L"-SOLTE PARA SELECIONAR", //PT
				L"-SEÇMEK İÇİN BIRAKIN", //TU
				L"RILASCIA PER SELEZIONARE"  //IT
		} ;

		const wchar_t DESCRIBE_ADVANCED_SETUP_2_WALLET_line0[][25] = {
				L"WALLET SETUP", //EN
				L"WALLET KONFIGURATION", //DE
				L"СОЗДАНИЕ КОШЕЛЬКА", //RU
				L"创建钱袋", //ZH
				L"KONFIGURACE PENĚŽENKY", //CZ
				L"CONF. DE PORTEFEUILLE", //FR
				L"CONF. DE LA CARTERA", //ES
				L"CARTEIRA DE INSTALAÇÃO", //PT
				L"CÜZDAN KURULUMU", //TU
				L"IMPOSTAZIONI PORTAFOGLIO"  //IT
		} ;

		waitForNoButtonPress();


		initDisplay();
	    overlayBatteryStatus(BATT_VALUE_DISPLAY);
		writeEinkDrawUnicodeSingle((unsigned int*)DESCRIBE_ADVANCED_SETUP_2_WALLET_line0[lang], wcslen(DESCRIBE_ADVANCED_SETUP_2_WALLET_line0[lang]), COL_1_X, LINE_0_Y);
		writeUnderline(STRIPE_X_START, STRIPE_Y_START, STRIPE_X_END, STRIPE_Y_END);
		writeEinkDrawUnicodeSingle((unsigned int*)DESCRIBE_ADVANCED_SETUP_2_line1[lang], wcslen(DESCRIBE_ADVANCED_SETUP_2_line1[lang]), COL_1_X, LINE_1_Y);
		writeEinkDrawUnicodeSingle((unsigned int*)DESCRIBE_ADVANCED_SETUP_2_line2[lang], wcslen(DESCRIBE_ADVANCED_SETUP_2_line2[lang]), COL_1_X, LINE_2_Y);
		writeEinkDrawUnicodeSingle((unsigned int*)DESCRIBE_ADVANCED_SETUP_2_line3[lang], wcslen(DESCRIBE_ADVANCED_SETUP_2_line3[lang]), COL_1_X, LINE_3_Y);
		writeEinkDrawUnicodeSingle((unsigned int*)udsGO_line0[lang], wcslen(udsGO_line0[lang]), ACCEPT_X_START, LINE_4_Y);
		writeEinkDrawUnicodeSingle((unsigned int*)udsBACK_line0[lang], wcslen(udsBACK_line0[lang]), (DENY_X_START)-((zhSizer*wcslen(udsBACK_line0[lang]))*8), LINE_4_Y);


		drawX(draw_X_X,draw_X_Y);
		drawCheck(draw_check_X,draw_check_Y);
		display();

//		r = waitForButtonPress();
//		if (!r){
//			clearDisplay();
//		}else{
//			writeX_Screen();
//			showReady();
//		};
	}
	else if (command == ASKUSER_DESCRIBE_EXPERT_SETUP_2_WALLET)
	{
		const wchar_t udsBACK_line0[][9] = {
				L"BACK", //EN
				L"ZURÜCK", //DE
				L"НАЗАД", //RU
				L"退回", //ZH
				L"ZPĚT", //CZ
				L"RETOUR", //FR
				L"ATRÁS", //ES
				L"COSTAS", //PT
				L"GERİ", //TU
				L"INDIETRO"  //IT
		} ;
		const wchar_t udsGO_line0[][12] = {
				L"GO", //EN
				L"GEHEN", //DE
				L"ПОДТВЕРДИТЬ", //RU
				L"确认", //ZH
				L"DÁLE", //CZ
				L"CONTINUER", //FR
				L"IR", //ES
				L"IR", //PT
				L"İLERLE", //TU
				L"GO"  //IT
		} ;

		const wchar_t DESCRIBE_ADVANCED_SETUP_2_line1[][25] = {
				L"TO SET PIN", //EN
				L"UM PIN KONFIG", //DE
				L"УСТАНОВКА ПАРОЛЯ УСТ-ВА", //RU
				L"设置设具密码", //ZH
				L"NASTAVENÍ PINu", //CZ
				L"POUR RÉGLER LE NIP", //FR
				L"CREAR CÓDIGO DEL DISPOS.", //ES
				L"PARA DEFINIR PIN DEVICE", //PT
				L"KODUNU AYARLAMAK İÇİN", //TU
				L"PER IMPOSTARE IL PIN"  //IT
		} ;
		const wchar_t DESCRIBE_ADVANCED_SETUP_2_line2[][25] = {
				L"-PRESS AND HOLD TO CYCLE", //EN
				L"-DRÜCKEN UND ANHALTEN", //DE
				L"ДЕРЖИТЕ КНОПКУ: ПРОСМОТР", //RU
				L"推下不放浏览字符", //ZH
				L"-PODRŽENÍM ROTUJTE", //CZ
				L"-APPUYEZ ET MAINTENEZ", //FR
				L"-PULSAR Y MANTENER", //ES
				L"-PRESSIONE E SEGURE", //PT
				L"-DÖNGÜYE BASILI TUTUN", //TU
				L"TIENI PREMUTO LE LETTERE"  //IT
		} ;
		const wchar_t DESCRIBE_ADVANCED_SETUP_2_line3[][25] = {
				L"-RELEASE TO SELECT", //EN
				L"-LOSLASSEN ZUR AUSWAHL", //DE
				L"ОТПУСТИТЕ: ВЫБОР", //RU
				L"放开即可选择字符", //ZH
				L"-UVOLNĚNÍM VYBERTE", //CZ
				L"-RELÂCHEZ POUR SÉLECTION", //FR
				L"-SOLTAR PARA SELECCIONAR", //ES
				L"-SOLTE PARA SELECIONAR", //PT
				L"-SEÇMEK İÇİN BIRAKIN", //TU
				L"RILASCIA PER SELEZIONARE"  //IT
		} ;

		const wchar_t DESCRIBE_ADVANCED_SETUP_2_WALLET_line0[][25] = {
				L"WALLET SETUP", //EN
				L"WALLET KONFIGURATION", //DE
				L"СОЗДАНИЕ КОШЕЛЬКА", //RU
				L"创建钱袋", //ZH
				L"KONFIGURACE PENĚŽENKY", //CZ
				L"CONF. DE PORTEFEUILLE", //FR
				L"CONF. DE LA CARTERA", //ES
				L"CARTEIRA DE INSTALAÇÃO", //PT
				L"CÜZDAN KURULUMU", //TU
				L"IMPOSTAZIONI PORTAFOGLIO"  //IT
		} ;

		waitForNoButtonPress();


		initDisplay();
	    overlayBatteryStatus(BATT_VALUE_DISPLAY);
		writeEinkDrawUnicodeSingle((unsigned int*)DESCRIBE_ADVANCED_SETUP_2_WALLET_line0[lang], wcslen(DESCRIBE_ADVANCED_SETUP_2_WALLET_line0[lang]), COL_1_X, LINE_0_Y);
//		writeEinkDrawUnicodeSingle(str_ASKUSER_DESCRIBE_ADVANCED_SETUP_2_WALLET_line0_UNICODE_SIZED[lang][0], line0length, COL_1_X, LINE_1_Y);
		writeUnderline(STRIPE_X_START, STRIPE_Y_START, STRIPE_X_END, STRIPE_Y_END);
		writeEinkDrawUnicodeSingle((unsigned int*)DESCRIBE_ADVANCED_SETUP_2_line1[lang], wcslen(DESCRIBE_ADVANCED_SETUP_2_line1[lang]), COL_1_X, LINE_1_Y);
		writeEinkDrawUnicodeSingle((unsigned int*)DESCRIBE_ADVANCED_SETUP_2_line2[lang], wcslen(DESCRIBE_ADVANCED_SETUP_2_line2[lang]), COL_1_X, LINE_2_Y);
		writeEinkDrawUnicodeSingle((unsigned int*)DESCRIBE_ADVANCED_SETUP_2_line3[lang], wcslen(DESCRIBE_ADVANCED_SETUP_2_line3[lang]), COL_1_X, LINE_3_Y);
		writeEinkDrawUnicodeSingle((unsigned int*)udsGO_line0[lang], wcslen(udsGO_line0[lang]), ACCEPT_X_START, LINE_4_Y);
		writeEinkDrawUnicodeSingle((unsigned int*)udsBACK_line0[lang], wcslen(udsBACK_line0[lang]), (DENY_X_START)-((zhSizer*wcslen(udsBACK_line0[lang]))*8), LINE_4_Y);

		drawX(draw_X_X,draw_X_Y);
		drawCheck(draw_check_X,draw_check_Y);
		display();

//		r = waitForButtonPress();
//		if (!r){
//			clearDisplay();
//		}else{
//			writeX_Screen();
//			showReady();
//		};
	}
	else if (command == ASKUSER_MNEMONIC_PREP)
	{
		const wchar_t MNEMONIC_PREP_line1[][25] = {
				L"WRITE DOWN THE NEXT", //EN
				L"NOTIEREN SIE DEN", //DE
				L"ЗАПИШИТЕ СЛЕДУЮЩИЙ", //RU
				L"请写下以下单词列表", //ZH
				L"ZAPIŠTE SI DALŠÍ", //CZ
				L"NOTEZ LES MOTS", //FR
				L"PORFAVOR TOME NOTA DE", //ES
				L"ANOTE A PRÓXIMA", //PT
				L"SONRAKİ EKRANDA GELEN", //TU
				L"ANNOTA LE PAROLE "  //IT
		} ;
		const wchar_t MNEMONIC_PREP_line2[][25] = {
				L"SCREEN OF WORDS.", //EN
				L"SCHIRMINHALT", //DE
				L"СПИСОК СЛОВ.", //RU
				L"这将是您钱袋的具份!!!", //ZH
				L"OBRAZOVKU SLOV.", //CZ
				L"QUI SUIVENT.", //FR
				L"LAS SIGUIENTES PALABRAS.", //ES
				L"TELA DE PALAVRAS.", //PT
				L"KELİMELERİ YAZINIZ", //TU
				L"DELLA PROSSIMA SCHERMATA"  //IT
		} ;
		const wchar_t MNEMONIC_PREP_line3[][25] = {
				L"THIS IS YOUR BACKUP!!!", //EN
				L"DAS IST IHREN BACKUP !!!", //DE
				L"ЭТО РЕЗЕРВНАЯ КОПИЯ !!!", //RU
				L"", //ZH
				L"TOHLE JE VAŠE ZÁLOHA!!!", //CZ
				L"CECI EST SAUVEGARDE", //FR
				L"ESTE ES SU CÓDIGO", //ES
				L"ESTE É SEU BACKUP !!!", //PT
				L"BU SİZİN YEDEĞİNİZ!!!", //TU
				L"QUESTO È IL"  //IT
		} ;
		const wchar_t MNEMONIC_PREP_line4[][25] = {
				L"", //EN
				L"", //DE
				L"", //RU
				L"", //ZH
				L"", //CZ
				L"(NE LA PERDEZ PAS) !!!", //FR
				L"DE RESPALDO !!!", //ES
				L"", //PT
				L"", //TU
				L"VOSTRO BACKUP !!!"  //IT
		} ;
		const wchar_t udsGO_line0[][12] = {
				L"GO", //EN
				L"GEHEN", //DE
				L"ПОДТВЕРДИТЬ", //RU
				L"确认", //ZH
				L"DÁLE", //CZ
				L"CONTINUER", //FR
				L"IR", //ES
				L"IR", //PT
				L"İLERLE", //TU
				L"GO"  //IT
		} ;

		const wchar_t MNEMONIC_PREP_line0[][25] = {
				L"BACKUP MNEMONIC", //EN
				L"SICHERUNGSKOPIE MNEMONIC", //DE
				L"СОХРАНЕНИЕ МНЕМОНИКИ", //RU
				L"具份助记符", //ZH
				L"MNEMOTECHNICKÁ ZÁLOHA", //CZ
				L"SAUVEG. DE LA MNÉMONIQUE", //FR
				L"RESPALDO MNEMÓNICO", //ES
				L"SEGURANÇA MNEMÓNICA", //PT
				L"YEDEK MNEMONIC", //TU
				L"BACKUP MNEMONICO"  //IT
		} ;


		waitForNoButtonPress();

		initDisplay();
	    overlayBatteryStatus(BATT_VALUE_DISPLAY);
		writeEinkDrawUnicodeSingle((unsigned int*)MNEMONIC_PREP_line0[lang], wcslen(MNEMONIC_PREP_line0[lang]), COL_1_X, LINE_0_Y);
		writeUnderline(STRIPE_X_START, STRIPE_Y_START, STRIPE_X_END, STRIPE_Y_END);
		writeEinkDrawUnicodeSingle((unsigned int*)MNEMONIC_PREP_line1[lang], wcslen(MNEMONIC_PREP_line1[lang]), COL_1_X, LINE_1_Y);
		writeEinkDrawUnicodeSingle((unsigned int*)MNEMONIC_PREP_line2[lang], wcslen(MNEMONIC_PREP_line2[lang]), COL_1_X, LINE_2_Y);
		writeEinkDrawUnicodeSingle((unsigned int*)MNEMONIC_PREP_line3[lang], wcslen(MNEMONIC_PREP_line3[lang]), COL_1_X, LINE_3_Y);
		writeEinkDrawUnicodeSingle((unsigned int*)udsGO_line0[lang], wcslen(udsGO_line0[lang]), ACCEPT_X_START, LINE_4_Y);
//		writeEinkDrawUnicodeSingle((unsigned int*)BACK_line0[lang], wcslen(BACK_line0[lang]), (DENY_X_START)-((zhSizer*wcslen(BACK_line0[lang]))*8), LINE_4_Y);

//		drawX(draw_X_X,draw_X_Y);
		drawCheck(draw_check_X,draw_check_Y);
		display();

//		r = waitForButtonPress();
//		if (!r){
//			clearDisplay();
//		}else{
//			writeX_Screen();
//			showReady();
//		};
	}
	else if (command == ASKUSER_MNEMONIC_PREP_2)
	{
		const wchar_t MNEMONIC_PREP_2_line1[][25] = {
				L"SHOW AGAIN?", //EN
				L"WIEDER ANZEIGEN?", //DE
				L"ПОКАЗАТЬ ЕЩЕ РАЗ?", //RU
				L"再次显示?", //ZH
				L"UKÁZAT ZNOVU?", //CZ
				L"MONTRER ENCORE UNE FOIS?", //FR
				L"MOSTRAR DE NUEVO?", //ES
				L"MOSTRAR NOVAMENTE?", //PT
				L"TEKRAR GÖSTER?", //TU
				L"MOSTRARE DI NUOVO?"  //IT
		} ;
		const wchar_t udsSHOW_line0[][9] = {
				L"SHOW", //EN
				L"ZEIGEN", //DE
				L"ПОКАЗАТЬ", //RU
				L"显示", //ZH
				L"UKÁZAT", //CZ
				L"MONTRER", //FR
				L"MOSTRAR", //ES
				L"MOSTRA", //PT
				L"GÖSTER", //TU
				L"MOSTRARE"  //IT
		} ;
		const wchar_t udsSKIP_line0[][13] = {
				L"SKIP", //EN
				L"ÜBERSPRINGEN", //DE
				L"ПРОПУСТИТЬ", //RU
				L"忽略", //ZH
				L"PŘESKOČIT", //CZ
				L"PASSER", //FR
				L"SALTAR", //ES
				L"PASSAR", //PT
				L"GEÇ", //TU
				L"SALTA"  //IT
		} ;
		const wchar_t MNEMONIC_PREP_line0[][25] = {
				L"BACKUP MNEMONIC", //EN
				L"SICHERUNGSKOPIE MNEMONIC", //DE
				L"СОХРАНЕНИЕ МНЕМОНИКИ", //RU
				L"具份助记符", //ZH
				L"MNEMOTECHNICKÁ ZÁLOHA", //CZ
				L"SAUVEG. DE LA MNÉMONIQUE", //FR
				L"RESPALDO MNEMÓNICO", //ES
				L"SEGURANÇA MNEMÓNICA", //PT
				L"YEDEK MNEMONIC", //TU
				L"BACKUP MNEMONICO"  //IT
		} ;

		waitForNoButtonPress();

		initDisplay();
	    overlayBatteryStatus(BATT_VALUE_DISPLAY);
		writeEinkDrawUnicodeSingle((unsigned int*)MNEMONIC_PREP_line0[lang], wcslen(MNEMONIC_PREP_line0[lang]), COL_1_X, LINE_0_Y);
		writeUnderline(STRIPE_X_START, STRIPE_Y_START, STRIPE_X_END, STRIPE_Y_END);
		writeEinkDrawUnicodeSingle((unsigned int*)MNEMONIC_PREP_2_line1[lang], wcslen(MNEMONIC_PREP_2_line1[lang]), COL_1_X, LINE_1_Y);
//		writeEinkDrawUnicodeSingle(str_ASKUSER_MNEMONIC_PREP_line2_UNICODE_SIZED[lang][0], line2length, COL_1_X, LINE_3_Y);
//		writeEinkDrawUnicodeSingle(str_ASKUSER_MNEMONIC_PREP_line3_UNICODE_SIZED[lang][0], line3length, COL_1_X, LINE_4_Y);
		writeEinkDrawUnicodeSingle((unsigned int*)udsSHOW_line0[lang], wcslen(udsSHOW_line0[lang]), ACCEPT_X_START, LINE_4_Y);
		writeEinkDrawUnicodeSingle((unsigned int*)udsSKIP_line0[lang], wcslen(udsSKIP_line0[lang]), (DENY_X_START)-((zhSizer*wcslen(udsSKIP_line0[lang]))*8), LINE_4_Y);

		drawX(draw_X_X,draw_X_Y);
		drawCheck(draw_check_X,draw_check_Y);
		display();

//		r = waitForButtonPress();
//		if (!r){
//			clearDisplay();
//		}else{
//			writeX_Screen();
//			showReady();
//		};
	}
	else if (command == ASKUSER_USE_AEM)
	{
		const wchar_t USE_AEM_line0[][25] = {
				L"AEM PROTECTION SETUP", //EN
				L"AEM SCHUTZKONFIGURATION", //DE
				L"УСТАНОВКА AEM ЗАЩИТЫ", //RU
				L"AEM防护设置", //ZH
				L"AEM KONFIGURACE", //CZ
				L"CONFIGURATION AEM", //FR
				L"CONFIGURACIÓN AEM", //ES
				L"AEM CONFIGURAÇÃO", //PT
				L"AEM KORUMA KURULUMU", //TU
				L"IMPOSTAZIONI AEM"  //IT
		} ;

		const wchar_t USE_AEM_line1[][25] = {
				L"GUARD AGAINST TAMPERING", //EN
				L"SCHUTZ VOR MANIPULATION", //DE
				L"ЗАЩИТИТЬ УСТРОЙСТВО ОТ", //RU
				L"防护设具篡改", //ZH
				L"OCHRANA PROTI PADĚLÁNÍ", //CZ
				L"PROTÉGEZ-VOUS CONTRE", //FR
				L"VIGILANCIA CONTRA", //ES
				L"PROTECCAO CONTRA", //PT
				L"GİZLİ KELİMELERLE", //TU
				L"SICUREZZA CONTRO"  //IT
		} ;

		const wchar_t USE_AEM_line2[][25] = {
				L"WITH A SECRET PHRASE", //EN
				L"MIT EINEM GEHEIM. PHRASE", //DE
				L"ВЗЛОМА ОТОБРАЖЕНИЕМ", //RU
				L"使用秘密短语", //ZH
				L"S VLASTNÍ FRÁZÍ", //CZ
				L"LA FALSIFICATION", //FR
				L"LA MANIPULACIÓN", //ES
				L"UTILIZACAO INDEVIDA", //PT
				L"KURCALAMAYA KARŞI", //TU
				L"LE MANOMISSIONI CON"  //IT
		} ;

		const wchar_t USE_AEM_line3[][25] = {
				L"", //EN
				L"", //DE
				L"СЕКРЕТНОЙ ФРАЗЫ", //RU
				L"", //ZH
				L"", //CZ
				L"AVEC UNE PHRASE SECRÈTE", //FR
				L"CON UNA FRASE SECRETA", //ES
				L"COM UMA FRASE SECRETA", //PT
				L"", //TU
				L"UNA FRASE PERSONALIZZATA"  //IT
		} ;
		const wchar_t udYES_line0[][5] = {
				L"YES", //EN
				L"JA", //DE
				L"ДА", //RU
				L"是", //ZH
				L"ANO", //CZ
				L"OUI", //FR
				L"SÍ", //ES
				L"SIM", //PT
				L"EVET", //TU
				L"SÌ"  //IT
		} ;
		const wchar_t udNO_line0[][6] = {
				L"NO", //EN
				L"NEIN", //DE
				L"НЕТ", //RU
				L"否", //ZH
				L"NE", //CZ
				L"NON", //FR
				L"NO", //ES
				L"NÃO", //PT
				L"HAYIR", //TU
				L"NO"  //IT
		};

		waitForNoButtonPress();

		initDisplay();
	    overlayBatteryStatus(BATT_VALUE_DISPLAY);
		writeEinkDrawUnicodeSingle((unsigned int*)USE_AEM_line0[lang], wcslen(USE_AEM_line0[lang]), COL_1_X, LINE_0_Y);
		writeUnderline(STRIPE_X_START, STRIPE_Y_START, STRIPE_X_END, STRIPE_Y_END);
		writeEinkDrawUnicodeSingle((unsigned int*)USE_AEM_line1[lang], wcslen(USE_AEM_line1[lang]), COL_1_X, LINE_1_Y);
		writeEinkDrawUnicodeSingle((unsigned int*)USE_AEM_line2[lang], wcslen(USE_AEM_line2[lang]), COL_1_X, LINE_2_Y);
		writeEinkDrawUnicodeSingle((unsigned int*)USE_AEM_line3[lang], wcslen(USE_AEM_line3[lang]), COL_1_X, LINE_3_Y);
		writeEinkDrawUnicodeSingle((unsigned int*)udYES_line0[lang], wcslen(udYES_line0[lang]), ACCEPT_X_START, LINE_4_Y);
		writeEinkDrawUnicodeSingle((unsigned int*)udNO_line0[lang], wcslen(udNO_line0[lang]), (DENY_X_START)-((zhSizer*wcslen(udNO_line0[lang]))*8), LINE_4_Y);

		drawX(draw_X_X,draw_X_Y);
		drawCheck(draw_check_X,draw_check_Y);
		display();

		r = waitForButtonPress();
		if (!r){
			clearDisplay();
		}else{
			clearDisplay();
//			writeX_Screen();
//			showReady();
		};
	}
	else if (command == ASKUSER_AEM_PASSPHRASE)
	{
		const wchar_t AEM_PASSPHRASE_line0[][25] = {
				L"AEM PROTECTION SETUP", //EN
				L"AEM SCHUTZKONFIGURATION", //DE
				L"УСТАНОВКА AEM ЗАЩИТЫ", //RU
				L"AEM防护设置", //ZH
				L"AEM KONFIGURACE", //CZ
				L"CONFIGURATION AEM", //FR
				L"CONFIGURACIÓN AEM", //ES
				L"AEM CONFIGURAÇÃO", //PT
				L"AEM KORUMA KURULUMU", //TU
				L"IMPOSTAZIONI AEM"  //IT
		} ;

		const wchar_t AEM_PASSPHRASE_line1[][25] = {
				L"SET YOUR CUSTOM", //EN
				L"STELLEN SIE IHREN", //DE
				L"УСТАНОВИТЕ СПЕЦИАЛЬНЫЙ", //RU
				L"请设置您的自定义", //ZH
				L"NASTAVTE SI VLASTNÍ", //CZ
				L"RÉGLEZ VOTRE CLÉ", //FR
				L"ESTABLEZCA SU", //ES
				L"CONJUNTO PERSONALIZADO", //PT
				L"ÖZEL ŞİFRENİZİ", //TU
				L"IMPOSTA LA TUA CHIAVE"  //IT
		} ;

		const wchar_t AEM_PASSPHRASE_line2[][25] = {
				L"UNLOCK KEY", //EN
				L"KUNDENSPEZIFISCHEN", //DE
				L"КЛЮЧ РАЗБЛОКИРОВАНИЯ", //RU
				L"解锁密码", //ZH
				L"KLÍČ K ODEMČENÍ", //CZ
				L"PERSONALISÉE", //FR
				L"LLAVE DE DESBLOQUEO", //ES
				L"CHAVE DE DESBLOQUEIO", //PT
				L"GİRİNİZ", //TU
				L"DI SBLOCCO"  //IT
		} ;

		const wchar_t AEM_PASSPHRASE_line3[][25] = {
				L"", //EN
				L"FREISCHALTSCHLÜSSEL", //DE
				L"", //RU
				L"", //ZH
				L"", //CZ
				L"DE DÉVEROUILLAGE", //FR
				L"", //ES
				L"", //PT
				L"", //TU
				L"PERSONALIZZATA"  //IT
		} ;
		const wchar_t udsCANCEL_line0[][9] = {
				L"CANCEL", //EN
				L"ABSAGEN", //DE
				L"ОТМЕНА", //RU
				L"退回", //ZH
				L"ZRUŠIT", //CZ
				L"ANNULER", //FR
				L"CANCELAR", //ES
				L"CANCELAR", //PT
				L"İPTAL", //TU
				L"ANNULLA"  //IT
		} ;
		const wchar_t udsGO_line0[][12] = {
				L"GO", //EN
				L"GEHEN", //DE
				L"ПОДТВЕРДИТЬ", //RU
				L"确认", //ZH
				L"DÁLE", //CZ
				L"CONTINUER", //FR
				L"IR", //ES
				L"IR", //PT
				L"İLERLE", //TU
				L"GO"  //IT
		} ;


		waitForNoButtonPress();

		initDisplay();
	    overlayBatteryStatus(BATT_VALUE_DISPLAY);
		writeEinkDrawUnicodeSingle((unsigned int*)AEM_PASSPHRASE_line0[lang], wcslen(AEM_PASSPHRASE_line0[lang]), COL_1_X, LINE_0_Y);
		writeUnderline(STRIPE_X_START, STRIPE_Y_START, STRIPE_X_END, STRIPE_Y_END);
		writeEinkDrawUnicodeSingle((unsigned int*)AEM_PASSPHRASE_line1[lang], wcslen(AEM_PASSPHRASE_line1[lang]), COL_1_X, LINE_1_Y);
		writeEinkDrawUnicodeSingle((unsigned int*)AEM_PASSPHRASE_line2[lang], wcslen(AEM_PASSPHRASE_line2[lang]), COL_1_X, LINE_2_Y);
		writeEinkDrawUnicodeSingle((unsigned int*)AEM_PASSPHRASE_line3[lang], wcslen(AEM_PASSPHRASE_line3[lang]), COL_1_X, LINE_3_Y);
		writeEinkDrawUnicodeSingle((unsigned int*)udsGO_line0[lang], wcslen(udsGO_line0[lang]), ACCEPT_X_START, LINE_4_Y);
		writeEinkDrawUnicodeSingle((unsigned int*)udsCANCEL_line0[lang], wcslen(udsCANCEL_line0[lang]), (DENY_X_START)-((zhSizer*wcslen(udsCANCEL_line0[lang]))*8), LINE_4_Y);

		drawX(draw_X_X,draw_X_Y);
		drawCheck(draw_check_X,draw_check_Y);
		display();

		r = waitForButtonPress();
		if (!r){
			clearDisplay();
		}else{
			writeX_Screen();
//			showReady();
		};
	}
	else if (command == ASKUSER_AEM_DISPLAYPHRASE)
	{
		const wchar_t AEM_DISPLAYPHRASE_line0[][25] = {
				L"AEM PROTECTION SETUP", //EN
				L"AEM SCHUTZKONFIGURATION", //DE
				L"УСТАНОВКА AEM ЗАЩИТЫ", //RU
				L"AEM防护设置", //ZH
				L"AEM KONFIGURACE", //CZ
				L"CONFIGURATION AEM", //FR
				L"CONFIGURACIÓN AEM", //ES
				L"AEM CONFIGURAÇÃO", //PT
				L"AEM KORUMA KURULUMU", //TU
				L"IMPOSTAZIONI AEM"  //IT
		} ;

		const wchar_t AEM_DISPLAYPHRASE_line1[][25] = {
				L"ENTER SECRET PHRASE THAT", //EN
				L"BITTE GEHEIMEN", //DE
				L"ВВЕДИТЕ СЕКРЕТНУЮ ФРАЗУ,", //RU
				L"请输入将显示的秘密短语", //ZH
				L"ZADEJTE TAJNOU FRÁZI,", //CZ
				L"SAISIR LA PHRASE SECRÈTE", //FR
				L"INTRODUZCA LA FRASE", //ES
				L"DIGITE A FRASE SECRETA", //PT
				L"BURADA GÖSTERİLECEK", //TU
				L"INSERISCI UNA FRASE CHE"  //IT
		} ;

		const wchar_t AEM_DISPLAYPHRASE_line2[][25] = {
				L"WILL BE DISPLAYED", //EN
				L"SATZGLIEDE EINGEBEN,", //DE
				L"ДЛЯ ОТОБРАЖЕНИЯ", //RU
				L"", //ZH
				L"KTERÁ BUDE ZOBRAZENA", //CZ
				L"QUI SERA AFFICHÉE", //FR
				L"SECRETA QUE SE MOSTRARÁ", //ES
				L"QUE SERÁ EXIBIDO", //PT
				L"GİZLİ KELİMELERİ YAZIN", //TU
				L"VERRÀ VISUALIZZATA"  //IT
		} ;

		const wchar_t AEM_DISPLAYPHRASE_line3[][25] = {
				L"", //EN
				L"DER ANGEZEIGT WIRD", //DE
				L"", //RU
				L"", //ZH
				L"", //CZ
				L"", //FR
				L"", //ES
				L"", //PT
				L"", //TU
				L""  //IT
		} ;
		const wchar_t udsCANCEL_line0[][9] = {
				L"CANCEL", //EN
				L"ABSAGEN", //DE
				L"ОТМЕНА", //RU
				L"退回", //ZH
				L"ZRUŠIT", //CZ
				L"ANNULER", //FR
				L"CANCELAR", //ES
				L"CANCELAR", //PT
				L"İPTAL", //TU
				L"ANNULLA"  //IT
		} ;
		const wchar_t udsGO_line0[][12] = {
				L"GO", //EN
				L"GEHEN", //DE
				L"ПОДТВЕРДИТЬ", //RU
				L"确认", //ZH
				L"DÁLE", //CZ
				L"CONTINUER", //FR
				L"IR", //ES
				L"IR", //PT
				L"İLERLE", //TU
				L"GO"  //IT
		} ;


		waitForNoButtonPress();

		initDisplay();
	    overlayBatteryStatus(BATT_VALUE_DISPLAY);
		writeEinkDrawUnicodeSingle((unsigned int*)AEM_DISPLAYPHRASE_line0[lang], wcslen(AEM_DISPLAYPHRASE_line0[lang]), COL_1_X, LINE_0_Y);
		writeUnderline(STRIPE_X_START, STRIPE_Y_START, STRIPE_X_END, STRIPE_Y_END);
		writeEinkDrawUnicodeSingle((unsigned int*)AEM_DISPLAYPHRASE_line1[lang], wcslen(AEM_DISPLAYPHRASE_line1[lang]), COL_1_X, LINE_1_Y);
		writeEinkDrawUnicodeSingle((unsigned int*)AEM_DISPLAYPHRASE_line2[lang], wcslen(AEM_DISPLAYPHRASE_line2[lang]), COL_1_X, LINE_2_Y);
		writeEinkDrawUnicodeSingle((unsigned int*)AEM_DISPLAYPHRASE_line3[lang], wcslen(AEM_DISPLAYPHRASE_line3[lang]), COL_1_X, LINE_3_Y);
		writeEinkDrawUnicodeSingle((unsigned int*)udsGO_line0[lang], wcslen(udsGO_line0[lang]), ACCEPT_X_START, LINE_4_Y);
		writeEinkDrawUnicodeSingle((unsigned int*)udsCANCEL_line0[lang], wcslen(udsCANCEL_line0[lang]), (DENY_X_START)-((zhSizer*wcslen(udsCANCEL_line0[lang]))*8), LINE_4_Y);

		drawX(draw_X_X,draw_X_Y);
		drawCheck(draw_check_X,draw_check_Y);
		display();

		r = waitForButtonPress();
		if (!r){
			clearDisplay();
		}else{
			writeX_Screen();
//			showReady();
		};
	}
	else if (command == ASKUSER_AEM_ENTRY)
	{
		const wchar_t AEM_ENTRY_line0[][25] = {
				L"DEVICE INTEGRITY CHECK", //EN
				L"GERÄT INTEGRITÄTSPRÜFUNG", //DE
				L"ОПОЗНАНИЕ УСТРОЙСТВА", //RU
				L"设具完整性检查", //ZH
				L"KONTROLA INTEGRITY", //CZ
				L"VÉRIFICATION", //FR
				L"COMPROBACIÓN", //ES
				L"VERIFICAÇÃO DE ", //PT
				L"CİHAZ ENTEGRASYON", //TU
				L"CONTROLLO INTEGRITA"  //IT
		} ;

		const wchar_t AEM_ENTRY_line1[][25] = {
				L"ENTER VERIFICATION CODE", //EN
				L"BESTÄTIGUNGSCODE", //DE
				L"ВВЕДИТЕ КОД", //RU
				L"输入防篡改验证码", //ZH
				L"ZAŘÍZENÍ", //CZ
				L"DE L'INTÉGRITÉ", //FR
				L"DEL DISPOSITIVO", //ES
				L"DISPOSITIVO", //PT
				L"KONTROLÜ", //TU
				L"DISPOSITIVO"  //IT
		} ;

		const wchar_t AEM_ENTRY_line2[][25] = {
				L"", //EN
				L"EINGEBEN", //DE
				L"ПОДТВЕРЖДЕНИЯ", //RU
				L"", //ZH
				L"VLOŽTE OVĚŘOVACÍ KÓD", //CZ
				L"ENTREZ LE CODE", //FR
				L"INTRODUZCA EL CÓDIGO", //ES
				L"DE INTEGRIDADE", //PT
				L"ONAY KODUNU GİRİN", //TU
				L"INSERISCI IL CODICE"  //IT
		} ;

		const wchar_t AEM_ENTRY_line3[][25] = {
				L"", //EN
				L"", //DE
				L"", //RU
				L"", //ZH
				L"", //CZ
				L"DE VÉRIFICATION", //FR
				L"DE VERIFICACIÓN", //ES
				L"DIGITE O CÓDIGO", //PT
				L"", //TU
				L"DI VERIFICA"  //IT
		} ;
		const wchar_t udsACCEPT_line0[][12] = {
				L"ACCEPT", //EN
				L"AKZEPTIEREN", //DE
				L"ПОДТВЕРДИТЬ", //RU
				L"确认", //ZH
				L"POTVRDIT", //CZ
				L"ACCEPTER", //FR
				L"ACEPTAR", //ES
				L"ACEITAR", //PT
				L"KABUL ET", //TU
				L"ACCETTA"  //IT
		} ;
		const wchar_t udsNUM_line0[][4] = {
				L"NUM", //EN
				L"NUM", //DE
				L"NUM", //RU
				L"字", //ZH
				L"NUM", //CZ
				L"NUM", //FR
				L"NUM", //ES
				L"NUM", //PT
				L"NUM", //TU
				L"NUM"  //IT
		} ;


		waitForNoButtonPress();

		initDisplay();
//	    overlayBatteryStatus(BATT_VALUE_DISPLAY);
		writeEinkDrawUnicodeSingle((unsigned int*)AEM_ENTRY_line0[lang], wcslen(AEM_ENTRY_line0[lang]), COL_1_X, LINE_0_Y);
		writeUnderline(STRIPE_X_START, STRIPE_Y_START, STRIPE_X_END, STRIPE_Y_END);
		writeEinkDrawUnicodeSingle((unsigned int*)AEM_ENTRY_line1[lang], wcslen(AEM_ENTRY_line1[lang]), COL_1_X, LINE_1_Y);
		writeEinkDrawUnicodeSingle((unsigned int*)AEM_ENTRY_line2[lang], wcslen(AEM_ENTRY_line2[lang]), COL_1_X, LINE_2_Y);
		writeEinkDrawUnicodeSingle((unsigned int*)AEM_ENTRY_line3[lang], wcslen(AEM_ENTRY_line3[lang]), COL_1_X, LINE_3_Y);
		writeEinkDrawUnicodeSingle((unsigned int*)udsACCEPT_line0[lang], wcslen(udsACCEPT_line0[lang]), ACCEPT_X_START, LINE_4_Y);
		writeEinkDrawUnicodeSingle((unsigned int*)udsNUM_line0[lang], wcslen(udsNUM_line0[lang]), (DENY_X_START)-((zhSizer*wcslen(udsNUM_line0[lang]))*8), LINE_4_Y);

		drawX(draw_X_X,draw_X_Y);
		drawCheck(draw_check_X,draw_check_Y);
		display();

//		r = waitForButtonPress();
//		if (!r){
//			clearDisplay();
//		}else{
//			writeX_Screen();
//			showReady();
//		};
	}
	else if (command == ASKUSER_AEM_ENTRY_ALPHA)
	{
		const wchar_t AEM_ENTRY_ALPHA_line0[][25] = {
				L"DEVICE INTEGRITY CHECK", //EN
				L"GERÄT INTEGRITÄTSPRÜFUNG", //DE
				L"ОПОЗНАНИЕ УСТРОЙСТВА", //RU
				L"设具完整性检查", //ZH
				L"KONTROLA INTEGRITY", //CZ
				L"VÉRIFICATION", //FR
				L"COMPROBACIÓN", //ES
				L"VERIFICAÇÃO DE ", //PT
				L"CİHAZ ENTEGRASYON", //TU
				L"CONTROLLO INTEGRITA"  //IT
		} ;

		const wchar_t AEM_ENTRY_ALPHA_line1[][25] = {
				L"ENTER VERIFICATION CODE", //EN
				L"BESTÄTIGUNGSCODE", //DE
				L"ВВЕДИТЕ КОД", //RU
				L"输入防篡改验证码", //ZH
				L"ZAŘÍZENÍ", //CZ
				L"DE L'INTÉGRITÉ", //FR
				L"DEL DISPOSITIVO", //ES
				L"DISPOSITIVO", //PT
				L"KONTROLÜ", //TU
				L"DISPOSITIVO"  //IT
		} ;

		const wchar_t AEM_ENTRY_ALPHA_line2[][25] = {
				L"", //EN
				L"EINGEBEN", //DE
				L"ПОДТВЕРЖДЕНИЯ", //RU
				L"", //ZH
				L"VLOŽTE OVĚŘOVACÍ KÓD", //CZ
				L"ENTREZ LE CODE", //FR
				L"INTRODUZCA EL CÓDIGO", //ES
				L"DE INTEGRIDADE", //PT
				L"ONAY KODUNU GİRİN", //TU
				L"INSERISCI IL CODICE"  //IT
		} ;

		const wchar_t AEM_ENTRY_ALPHA_line3[][25] = {
				L"", //EN
				L"", //DE
				L"", //RU
				L"", //ZH
				L"", //CZ
				L"DE VÉRIFICATION", //FR
				L"DE VERIFICACIÓN", //ES
				L"DIGITE O CÓDIGO", //PT
				L"", //TU
				L"DI VERIFICA"  //IT
		} ;
		const wchar_t udsACCEPT_line0[][12] = {
				L"ACCEPT", //EN
				L"AKZEPTIEREN", //DE
				L"ПОДТВЕРДИТЬ", //RU
				L"确认", //ZH
				L"POTVRDIT", //CZ
				L"ACCEPTER", //FR
				L"ACEPTAR", //ES
				L"ACEITAR", //PT
				L"KABUL ET", //TU
				L"ACCETTA"  //IT
		} ;
		const wchar_t udsALPHA_line0[][6] = {
				L"ALPHA", //EN
				L"ALPHA", //DE
				L"ALPHA", //RU
				L"拼音", //ZH
				L"ALPHA", //CZ
				L"ALPHA", //FR
				L"ALPHA", //ES
				L"ALPHA", //PT
				L"ALPHA", //TU
				L"ALPHA"  //IT
		} ;


		waitForNoButtonPress();

		initDisplay();
//	    overlayBatteryStatus(BATT_VALUE_DISPLAY);
		writeEinkDrawUnicodeSingle((unsigned int*)AEM_ENTRY_ALPHA_line0[lang], wcslen(AEM_ENTRY_ALPHA_line0[lang]), COL_1_X, LINE_0_Y);
		writeUnderline(STRIPE_X_START, STRIPE_Y_START, STRIPE_X_END, STRIPE_Y_END);
		writeEinkDrawUnicodeSingle((unsigned int*)AEM_ENTRY_ALPHA_line1[lang], wcslen(AEM_ENTRY_ALPHA_line1[lang]), COL_1_X, LINE_1_Y);
		writeEinkDrawUnicodeSingle((unsigned int*)AEM_ENTRY_ALPHA_line2[lang], wcslen(AEM_ENTRY_ALPHA_line2[lang]), COL_1_X, LINE_2_Y);
		writeEinkDrawUnicodeSingle((unsigned int*)AEM_ENTRY_ALPHA_line3[lang], wcslen(AEM_ENTRY_ALPHA_line3[lang]), COL_1_X, LINE_3_Y);
//		writeEinkDrawUnicodeSingle((unsigned int*)udsACCEPT_line0[lang], wcslen(udsACCEPT_line0[lang]), ACCEPT_X_START, LINE_4_Y);
		writeEinkDrawUnicodeSingle((unsigned int*)udsALPHA_line0[lang], wcslen(udsALPHA_line0[lang]), (DENY_X_START)-((zhSizer*wcslen(udsALPHA_line0[lang]))*8), LINE_4_Y);

		drawX(draw_X_X,draw_X_Y);
//		drawCheck(draw_check_X,draw_check_Y);
		display();

//		r = waitForButtonPress();
//		if (!r){
//			clearDisplay();
//		}else{
//			writeX_Screen();
//			showReady();
//		};
	}
	else if (command == ASKUSER_DELETE_ONLY_EX_DISPLAY)
	{
		const wchar_t udsDELETE_line0[][9] = {
				L"DELETE", //EN
				L"LÖSCHEN", //DE
				L"УДАЛИТЬ", //RU
				L"删除", //ZH
				L"SMAZAT", //CZ
				L"EFFACER", //FR
				L"BORRAR", //ES
				L"APAGAR", //PT
				L"SİL", //TU
				L"CANCELLA"  //IT
		} ;

//		initDisplay();
//	    overlayBatteryStatus(BATT_VALUE_DISPLAY);
//		writeEinkDrawUnicodeSingle(str_ASKUSER_ENTER_PIN_line0_UNICODE_SIZED[lang][0], line0length, COL_1_X, LINE_1_Y);
		writeUnderline(STRIPE_X_START, STRIPE_Y_START, STRIPE_X_END, STRIPE_Y_END);
//		writeEinkDrawUnicodeSingle(str_ASKUSER_DESCRIBE_STANDARD_SETUP_line1_UNICODE_SIZED[lang][0], line1length, COL_1_X, LINE_2_Y);
//		writeEinkDrawUnicodeSingle(str_ASKUSER_DESCRIBE_STANDARD_SETUP_line2_UNICODE_SIZED[lang][0], line2length, COL_1_X, LINE_3_Y);
//		writeEinkDrawUnicodeSingle(str_ASKUSER_DESCRIBE_STANDARD_SETUP_line3_UNICODE_SIZED[lang][0], line3length, COL_1_X, LINE_4_Y);
//		writeEinkDrawUnicodeSingle((unsigned int*)GO_line0[lang], wcslen(GO_line0[lang]), ACCEPT_X_START, 80);
		writeEinkDrawUnicodeSingle((unsigned int*)udsDELETE_line0[lang], wcslen(udsDELETE_line0[lang]), (DENY_X_START)-((zhSizer*wcslen(udsDELETE_line0[lang]))*8), LINE_4_Y);

		drawX(draw_X_X,draw_X_Y);
//		drawCheck(draw_check_X,draw_check_Y);
//		display();

//		r = waitForButtonPress();
//		if (!r){
//			clearDisplay();
//		}else{
//			writeX_Screen();
//			showReady();
//		};
	}
	else if (command == ASKUSER_ACCEPT_AND_DELETE_EX_DISPLAY)
	{
		const wchar_t udsDELETE_line0[][9] = {
				L"DELETE", //EN
				L"LÖSCHEN", //DE
				L"УДАЛИТЬ", //RU
				L"删除", //ZH
				L"SMAZAT", //CZ
				L"EFFACER", //FR
				L"BORRAR", //ES
				L"APAGAR", //PT
				L"SİL", //TU
				L"CANCELLA"  //IT
		} ;

		const wchar_t udsACCEPT_line0[][12] = {
				L"ACCEPT", //EN
				L"AKZEPTIEREN", //DE
				L"ПОДТВЕРДИТЬ", //RU
				L"确认", //ZH
				L"POTVRDIT", //CZ
				L"ACCEPTER", //FR
				L"ACEPTAR", //ES
				L"ACEITAR", //PT
				L"KABUL ET", //TU
				L"ACCETTA"  //IT
		} ;



//		initDisplay();
//	    overlayBatteryStatus(BATT_VALUE_DISPLAY);
//		writeEinkDrawUnicodeSingle(str_ASKUSER_ENTER_PIN_line0_UNICODE_SIZED[lang][0], line0length, COL_1_X, LINE_1_Y);
		writeUnderline(STRIPE_X_START, STRIPE_Y_START, STRIPE_X_END, STRIPE_Y_END);
//		writeEinkDrawUnicodeSingle(str_ASKUSER_DESCRIBE_STANDARD_SETUP_line1_UNICODE_SIZED[lang][0], line1length, COL_1_X, LINE_2_Y);
//		writeEinkDrawUnicodeSingle(str_ASKUSER_DESCRIBE_STANDARD_SETUP_line2_UNICODE_SIZED[lang][0], line2length, COL_1_X, LINE_3_Y);
//		writeEinkDrawUnicodeSingle(str_ASKUSER_DESCRIBE_STANDARD_SETUP_line3_UNICODE_SIZED[lang][0], line3length, COL_1_X, LINE_4_Y);

		writeEinkDrawUnicodeSingle((unsigned int*)udsACCEPT_line0[lang], wcslen(udsACCEPT_line0[lang]), ACCEPT_X_START, LINE_4_Y);
		writeEinkDrawUnicodeSingle((unsigned int*)udsDELETE_line0[lang], wcslen(udsDELETE_line0[lang]), (DENY_X_START)-((zhSizer*wcslen(udsDELETE_line0[lang]))*8), LINE_4_Y);

		drawX(draw_X_X,draw_X_Y);
		drawCheck(draw_check_X,draw_check_Y);
//		display();

//		r = waitForButtonPress();
//		if (!r){
//			clearDisplay();
//		}else{
//			writeX_Screen();
//			showReady();
//		};
	}
	else if (command == ASKUSER_ALPHA_INPUT_PREFACE)
	{
		const wchar_t udsALPHA_INPUT_PREFACE_A_line0[][25] = {
				L"MINIMUM 4 CHARACTERS", //EN
				L"MINIMUM 4 ZEICHEN", //DE
				L"НЕ МЕНЕЕ 4 СИМВОЛОВ", //RU
				L"最少4个字符", //ZH
				L"ALESPOŇ 4 ZNAKY", //CZ
				L"4 CARACTÈRES MINIMUM", //FR
				L"MÍNIMO 4 CARACTERES", //ES
				L"MÍNIMAS 4 CARACTERES", //PT
				L"MİNİMUM 4 KARAKTER", //TU
				L"MINIMO 4 CARATTERI"  //IT
		} ;

		const wchar_t udsALPHA_INPUT_PREFACE_B_line0[][25] = {
				L"PRESS/HOLD/RELEASE", //EN
				L"DRÜCKEN/HALTEN/LOSLASSEN", //DE
				L"НАЖАТЬ/ДЕРЖАТЬ/ОТПУСТИТЬ", //RU
				L"推下/推住/放开", //ZH
				L"STISKNOUT/PODRŽET/UVOLNT", //CZ
				L"APPUYER/MAINT./RELÂCHER", //FR
				L"PULSAR/MANTENER/SOLTAR", //ES
				L"PRESSIONE/AGUARDE/SOLTE", //PT
				L"BAS/TUT/BIRAK", //TU
				L"PREMI/TIENI/RILASCIA"  //IT
		} ;

		waitForNoButtonPress();

		initDisplay();
		writeEinkDrawUnicodeSingle((unsigned int*)udsALPHA_INPUT_PREFACE_A_line0[lang], wcslen(udsALPHA_INPUT_PREFACE_A_line0[lang]), COL_1_X, LINE_0_Y);
		writeUnderline(STRIPE_X_START, STRIPE_Y_START, STRIPE_X_END, STRIPE_Y_END);
		display();

		initDisplay();
		writeEinkDrawUnicodeSingle((unsigned int*)udsALPHA_INPUT_PREFACE_B_line0[lang], wcslen(udsALPHA_INPUT_PREFACE_B_line0[lang]), COL_1_X, LINE_0_Y);
		writeUnderline(STRIPE_X_START, STRIPE_Y_START, STRIPE_X_END, STRIPE_Y_END);
//		writeEinkDrawUnicodeSingle(str_ASKUSER_ALPHA_INPUT_PREFACE_line2_UNICODE_SIZED[lang][0], line2length, COL_1_X, LINE_2_Y);
		display();

//		r = waitForButtonPress();
//		if (!r){
//			clearDisplay();
//		}else{
//			writeX_Screen();
//			showReady();
//		};
	}
	else if (command == ASKUSER_SET_TRANSACTION_PIN)
	{
		const wchar_t SET_TRANSACTION_PIN_line1[][25] = {
				L"SET TRANSACTION PIN", //EN
				L"TRANSAKTION PIN STELLEN", //DE
				L"УСТАНОВИТЬ ПАРОЛЬ", //RU
				L"设置交易密码", //ZH
				L"NASTAVTE PIN TRANSAKCE", //CZ
				L"DÉFINIR LE NIP", //FR
				L"CREAR CÓDIGO", //ES
				L"DEFINIR O TRANSACT. PIN", //PT
				L"İŞLEM PIN KODUNU AYARLA", //TU
				L"IMPOSTA IL PIN"  //IT
		} ;

		const wchar_t SET_TRANSACTION_PIN_line2[][25] = {
				L"", //EN
				L"", //DE
				L"ДЛЯ ТРАНЗАКЦИЙ", //RU
				L"", //ZH
				L"", //CZ
				L"DE TRANSACTION", //FR
				L"DE TRANSACCIÓN", //ES
				L"", //PT
				L"", //TU
				L"DELLE TRANSAZIONI"  //IT
		} ;
		const wchar_t udsCANCEL_line0[][9] = {
				L"CANCEL", //EN
				L"ABSAGEN", //DE
				L"ОТМЕНА", //RU
				L"退回", //ZH
				L"ZRUŠIT", //CZ
				L"ANNULER", //FR
				L"CANCELAR", //ES
				L"CANCELAR", //PT
				L"İPTAL", //TU
				L"ANNULLA"  //IT
		} ;
		const wchar_t udsGO_line0[][12] = {
				L"GO", //EN
				L"GEHEN", //DE
				L"ПОДТВЕРДИТЬ", //RU
				L"确认", //ZH
				L"DÁLE", //CZ
				L"CONTINUER", //FR
				L"IR", //ES
				L"IR", //PT
				L"İLERLE", //TU
				L"GO"  //IT
		} ;
		const wchar_t DESCRIBE_EXPERT_SETUP_line0[][25] = {
				L"EXPERT SETUP", //EN
				L"EXPERT KONFIGURATION", //DE
				L"ЭКСПЕРТНЫЕ НАСТРОЙКИ", //RU
				L"专家设置", //ZH
				L"EXPERT KONFIGURACE", //CZ
				L"CONFIGURATION EXPERT", //FR
				L"CONFIGURACIÓN EXPERTO", //ES
				L"INSTALAÇÃO DE ESPECIAL.", //PT
				L"UZMAN KURULUMU", //TU
				L"IMPOSTAZIONIE ESPERTO"  //IT
		} ;



		waitForNoButtonPress();

		initDisplay();
//	    overlayBatteryStatus(BATT_VALUE_DISPLAY);
		writeEinkDrawUnicodeSingle((unsigned int*)DESCRIBE_EXPERT_SETUP_line0[lang], wcslen(DESCRIBE_EXPERT_SETUP_line0[lang]), COL_1_X, LINE_0_Y);
		writeUnderline(STRIPE_X_START, STRIPE_Y_START, STRIPE_X_END, STRIPE_Y_END);
		writeEinkDrawUnicodeSingle((unsigned int*)SET_TRANSACTION_PIN_line1[lang], wcslen(SET_TRANSACTION_PIN_line1[lang]), COL_1_X, LINE_1_Y);
		writeEinkDrawUnicodeSingle((unsigned int*)SET_TRANSACTION_PIN_line2[lang], wcslen(SET_TRANSACTION_PIN_line2[lang]), COL_1_X, LINE_2_Y);
		writeEinkDrawUnicodeSingle((unsigned int*)udsGO_line0[lang], wcslen(udsGO_line0[lang]), ACCEPT_X_START, LINE_4_Y);
		writeEinkDrawUnicodeSingle((unsigned int*)udsCANCEL_line0[lang], wcslen(udsCANCEL_line0[lang]), (DENY_X_START)-((zhSizer*wcslen(udsCANCEL_line0[lang]))*8), LINE_4_Y);

		drawX(draw_X_X,draw_X_Y);
		drawCheck(draw_check_X,draw_check_Y);
		display();

		r = waitForButtonPress();
		if (!r){
			clearDisplay();
		}else{
			clearDisplay();
//			writeX_Screen();
//			showReady();
		};
	}



	else
	{
		const wchar_t udsCANCEL_line0[][9] = {
				L"CANCEL", //EN
				L"ABSAGEN", //DE
				L"ОТМЕНА", //RU
				L"退回", //ZH
				L"ZRUŠIT", //CZ
				L"ANNULER", //FR
				L"CANCELAR", //ES
				L"CANCELAR", //PT
				L"İPTAL", //TU
				L"ANNULLA"  //IT
		} ;

		waitForNoButtonPress();


				initDisplay();
			    overlayBatteryStatus(BATT_VALUE_DISPLAY);
				writeUnderline(STRIPE_X_START, STRIPE_Y_START, STRIPE_X_END, STRIPE_Y_END);
				writeEinkDrawUnicodeSingle((unsigned int*)udsCANCEL_line0[lang], wcslen(udsCANCEL_line0[lang]), (DENY_X_START)-((zhSizer*wcslen(udsCANCEL_line0[lang]))*8), LINE_4_Y);

				drawX(draw_X_X,draw_X_Y);
				drawCheck(draw_check_X,draw_check_Y);
				display();
		waitForButtonPress();
		r = true; // unconditionally deny
	}

	return r;
}

//#####################################################################################
//#####################################################################################
//#####################################################################################

/** Convert 4 bit number into corresponding hexadecimal character. For
  * example, 0 is converted into '0' and 15 is converted into 'f'.
  * \param nibble The 4 bit number to look at. Only the least significant
  *               4 bits are considered.
  * \return The hexadecimal character.
  */
char nibbleToHex(uint8_t nibble)
{
	uint8_t temp;
	temp = (uint8_t)(nibble & 0xf);
	if (temp < 10)
	{
		return (char)('0' + temp);
	}
	else
	{
		return (char)('a' + (temp - 10));
	}
}


/** Notify user of stream error via. LCD. */
void streamError(void)
{
	writeEinkDisplay(str_stream_error, true, 10, 10, "",false,0,0, "",false,0,0, "",false,0,0, "",false,0,0);
}


/** Display a short (maximum 8 characters) one-time password for the user to
  * see. This one-time password is used to reduce the chance of a user
  * accidentally doing something stupid.
  * \param command The action to ask the user about. See #AskUserCommandEnum.
  * \param otp The one-time password to display. This will be a
  *            null-terminated string.
  */
void displayOTP(AskUserCommand command, char *otp)
{
	waitForNoButtonPress();
	writeEinkDisplay("OTP:", false, COL_1_X, LINE_1_Y, otp,false,COL_1_X,LINE_2_Y, "",false,0,0, "",false,0,0, "",false,0,0);
}


/** Clear the OTP (one-time password) shown by displayOTP() from the
  * display. */
void clearOTP(void)
{
	showWorking();
}


/** Clear the OTP (one-time password) shown by displayOTP() from the
 * display. */
void showWorking(void)
{
	writeEinkDisplay("COMPUTING...", false, COL_1_X, LINE_1_Y, "",false,10,30, "",false,0,0, "",false,0,0, "",false,0,0);
}


void showDenied(void)
{
	writeEinkDisplay("Canceled", false, COL_1_X, LINE_1_Y, "",false,10,30, "",false,0,0, "",false,0,0, "",false,0,0);
}


void clearDisplay(void)
{
	writeBlankScreen();
}


void clearDisplaySecure(void)
{
	writeEinkDisplay("", false, 0, 0, "",false,0,0, "",false,0,0, "",false,0,0, "",false,0,0);
	writeEinkDisplay("", false, 0, 0, "",false,0,0, "",false,0,0, "",false,0,0, "",false,0,0);
	writeEinkDisplay("", false, 0, 0, "",false,0,0, "",false,0,0, "",false,0,0, "",false,0,0);
}


void showBattery(void)
{
	int battery = batteryLevel();
	char batteryTxt[16];
	sprintf(batteryTxt, "%lu", battery);

	writeEinkDisplay("BATTERY", false, 5, 5, batteryTxt, false, 5, 25, "", false, 5, 45, "", false , 5, 60, "", false, 0, 0);
}


void showReady(void)
{
	writeSplashScreen();
}


void languageMenuInitially(void){
	uint8_t tempLangSet[1];
	nonVolatileRead(tempLangSet, DEVICE_LANG_SET_ADDRESS, 1);

	int lang;

	lang = (int)tempLangSet[0];
//	lang =0;// temp to force selection
//	nonVolatileRead(byteRead, DEVICE_LANG_SET_ADDRESS, 1);
//	uint8_t  byteRead = eeprom_read_byte((uint8_t*)148);
	if (lang != 123) {
		initDisplay();
		writeEinkDrawUnicode(str_lang_list_UNICODE[0],  24, 0, 5,
				str_lang_list_UNICODE[1],  24, 0, 22,
				str_lang_list_UNICODE[2],  24, 0, 39,
				str_lang_list_UNICODE[3],  24, 0, 56,
				str_lang_list_UNICODE[4],  24, 0, 73);
		display();
		setLangInitially();
	}else{
//		writeEink("set", false, 10, 10);
	}
	clearDisplay();
}


void languageMenu(void){
	uint8_t tempLangSet[1];
	nonVolatileRead(tempLangSet, DEVICE_LANG_SET_ADDRESS, 1);

	int lang;

	lang = (int)tempLangSet[0];
//	lang =0;// temp to force selection
//	nonVolatileRead(byteRead, DEVICE_LANG_SET_ADDRESS, 1);
//	uint8_t  byteRead = eeprom_read_byte((uint8_t*)148);
	if (lang != 123) {
//		writeWorking_Screen();
		writeEinkDisplayUnicode(str_lang_list_UNICODE[0], true, 24, 0, 5,
				str_lang_list_UNICODE[1], true, 24, 0, 22,
				str_lang_list_UNICODE[2], true, 24, 0, 39,
				str_lang_list_UNICODE[3], true, 24, 0, 56,
				str_lang_list_UNICODE[4], true, 24, 0, 73);
		setLang();
	}else{
//		writeEink("set", false, 10, 10);
	}
	clearDisplay();
}


void displayMnemonic(const char * mnemonicToDisplay, int length)
{
	bool showAgain;

	char * pch;
	pch = strtok (mnemonicToDisplay," ");
	const char *mnemPiece[length];
	int i = 0;
	while (pch != NULL)
	{
		mnemPiece[i] = pch;
		pch = strtok (NULL, " ");
		i++;
	}

	buttonInterjectionNoAckSetup(ASKUSER_MNEMONIC_PREP);

	waitForNoButtonPress();
	waitForNumberButtonPress();
	clearDisplay();
	int col1 = 20;
	int col2 = 120;
	int row1 = 0;
	int row2 = 16;
	int row3 = 32;
	int row4 = 48;
	int row5 = 64;
// the new descenders on the "g" cause the line to truncate if writing below 79 correction 78 check the word "luggage"!
	int row6 = 78;
	int offset = 12;

	writeEinkDisplayPrep("1", col1-offset, row1, "3",col1-offset,row2, "5",col1-offset,row3, "7",col1-offset,row4, "9", col1-offset,row5, "11", col1-offset-8,row6,
						"2", col2-offset, row1, "4",col2-offset,row2, "6",col2-offset,row3, "8",col2-offset,row4, "10",col2-offset-8,row5, "12",col2-offset-8,row6);

	writeEinkDisplayBig(mnemPiece[0], col1, row1, mnemPiece[2],col1,row2, mnemPiece[4],col1,row3, mnemPiece[6],col1,row4, mnemPiece[8], col1,row5, mnemPiece[10], col1,row6,
						mnemPiece[1], col2, row1, mnemPiece[3],col2,row2, mnemPiece[5],col2,row3, mnemPiece[7],col2,row4, mnemPiece[9],col2,row5, mnemPiece[11],col2,row6);
	waitForNoButtonPress();
	waitForNumberButtonPress();
	clearDisplay();

	if(length == 18)
	{
//		writeEinkDisplay("Next screen:", false, 5, 5, "",false,5,25, "",false,0,0, "",false,0,0, "",false,0,0);
		writeEinkDisplayPrep("13", col1-offset-8, row1, "14",col2-offset-8,row1, "15",col1-offset-8,row2, "16",col2-offset-8,row2, "17", col1-offset-8,row3, "18", col2-offset-8,row3,
							"", col2-offset-8, row1, "",col2-offset-8,row2, "",col2-offset-8,row3, "",col2-offset-8,row4, "",col2-offset-8,row5, "",col2-offset-8,row6);

		writeEinkDisplayBig(mnemPiece[12], col1, row1, mnemPiece[13],col2,row1, mnemPiece[14],col1,row2, mnemPiece[15],col2,row2, mnemPiece[16], col1,row3, mnemPiece[17], col2,row3,
							"", col2, row1, "",col2,row2, "",col2,row3, "",col2,row4, "",col2,row5, "",col2,row6);
		waitForNoButtonPress();
		waitForNumberButtonPress();
		clearDisplay();
	}
	else if(length == 24)
	{
//		writeEinkDisplay("Next screen:", false, 5, 5, "",false,5,25, "",false,0,0, "",false,0,0, "",false,0,0);
		writeEinkDisplayPrep("13", col1-offset-8, row1, "14",col2-offset-8,row1, "15",col1-offset-8,row2, "16",col2-offset-8,row2, "17", col1-offset-8,row3, "18", col2-offset-8,row3,
							"19", col1-offset-8, row4, "20",col2-offset-8,row4, "21",col1-offset-8,row5, "22",col2-offset-8,row5, "23",col1-offset-8,row6, "24",col2-offset-8,row6);

		writeEinkDisplayBig(mnemPiece[12], col1, row1, mnemPiece[13],col2,row1, mnemPiece[14],col1,row2, mnemPiece[15],col2,row2, mnemPiece[16], col1,row3, mnemPiece[17], col2,row3,
							mnemPiece[18], col1, row4, mnemPiece[19],col2,row4, mnemPiece[20],col1,row5, mnemPiece[21],col2,row5, mnemPiece[22],col1,row6, mnemPiece[23],col2,row6);
		waitForNoButtonPress();
		waitForNumberButtonPress();
		clearDisplay();
	}

	buttonInterjectionNoAckSetup(ASKUSER_MNEMONIC_PREP_2);

	showAgain = waitForButtonPress();
	if(!showAgain)
	{
		clearDisplay();
		writeEinkDisplayPrep("1", col1-offset, row1, "3",col1-offset,row2, "5",col1-offset,row3, "7",col1-offset,row4, "9", col1-offset,row5, "11", col1-offset-8,row6,
							"2", col2-offset, row1, "4",col2-offset,row2, "6",col2-offset,row3, "8",col2-offset,row4, "10",col2-offset-8,row5, "12",col2-offset-8,row6);

		writeEinkDisplayBig(mnemPiece[0], col1, row1, mnemPiece[2],col1,row2, mnemPiece[4],col1,row3, mnemPiece[6],col1,row4, mnemPiece[8], col1,row5, mnemPiece[10], col1,row6,
							mnemPiece[1], col2, row1, mnemPiece[3],col2,row2, mnemPiece[5],col2,row3, mnemPiece[7],col2,row4, mnemPiece[9],col2,row5, mnemPiece[11],col2,row6);
		waitForNoButtonPress();
		waitForNumberButtonPress();
		clearDisplay();

		if(length == 18)
		{
			writeEinkDisplayPrep("13", col1-offset-8, row1, "14",col2-offset-8,row1, "15",col1-offset-8,row2, "16",col2-offset-8,row2, "17", col1-offset-8,row3, "18", col2-offset-8,row3,
								"", col2-offset-8, row1, "",col2-offset-8,row2, "",col2-offset-8,row3, "",col2-offset-8,row4, "",col2-offset-8,row5, "",col2-offset-8,row6);

			writeEinkDisplayBig(mnemPiece[12], col1, row1, mnemPiece[13],col2,row1, mnemPiece[14],col1,row2, mnemPiece[15],col2,row2, mnemPiece[16], col1,row3, mnemPiece[17], col2,row3,
								"", col2, row1, "",col2,row2, "",col2,row3, "",col2,row4, "",col2,row5, "",col2,row6);
			waitForNoButtonPress();
			waitForNumberButtonPress();
			clearDisplay();
		}
		else if(length == 24)
		{
			writeEinkDisplayPrep("13", col1-offset-8, row1, "14",col2-offset-8,row1, "15",col1-offset-8,row2, "16",col2-offset-8,row2, "17", col1-offset-8,row3, "18", col2-offset-8,row3,
								"19", col1-offset-8, row4, "20",col2-offset-8,row4, "21",col1-offset-8,row5, "22",col2-offset-8,row5, "23",col1-offset-8,row6, "24",col2-offset-8,row6);

			writeEinkDisplayBig(mnemPiece[12], col1, row1, mnemPiece[13],col2,row1, mnemPiece[14],col1,row2, mnemPiece[15],col2,row2, mnemPiece[16], col1,row3, mnemPiece[17], col2,row3,
								mnemPiece[18], col1, row4, mnemPiece[19],col2,row4, mnemPiece[20],col1,row5, mnemPiece[21],col2,row5, mnemPiece[22],col1,row6, mnemPiece[23],col2,row6);
			waitForNoButtonPress();
			waitForNumberButtonPress();
			clearDisplay();
		}
	}
	clearDisplay();
};


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
bool displayHexStream(uint8_t *stream, uint8_t length)
{
	uint8_t i;
	uint8_t one_byte; // current byte of seed
	uint8_t byte_counter; // current byte on screen, 0 = first, 1 = second etc.

	char str[3];
	char lineTemp[64] = "";
	char line0[24] = "";
	char line1[24] = "";
	char line2[24] = "";
	char line3[24] = "";
	char line4[24] = "";
	char line5[24] = "";
	char line6[24] = "";
	char line7[24] = "";
	char line8[24] = "";
	char line9[24] = "";


	// Tell user whether seed is encrypted or not.
//	clearDisplay();
//	waitForNoButtonPress();
//	gotoStartOfLine(0);
//	writeString(str_seed_encrypted_or_not_line0, true);
//	gotoStartOfLine(1);

//	if (waitForButtonPress())
//	{
////		clearDisplay();
//		return true;
//	}
//	waitForNoButtonPress();

	// Output seed to LCD.
	// str is " xx", where xx are hexadecimal digits.
//	str[0] = ' ';
	str[2] = '\0';
	byte_counter = 0;
	int j = 0;
	strcpy(lineTemp, "");
	for (i = 0; i < length; i++)
		{
			one_byte = stream[i];
//			str[1] = nibbleToHex((uint8_t)(one_byte >> 4));
//			str[2] = nibbleToHex(one_byte);
			str[0] = nibbleToHex((uint8_t)(one_byte >> 4));
			str[1] = nibbleToHex(one_byte);
//			lineTemp[0+currentJ]= str[0];
//			lineTemp[1+currentJ]= str[1];
//			lineTemp[2+currentJ]= str[2];
//			lineTemp[3+currentJ]= str[3];

//			strcpy(lineTemp, "");
			strcat(lineTemp, str);
			//			if (byte_counter == 6)
//			{
//				gotoStartOfLine(1);
//			//	strcpy(line1, lineTemp);
//			}

			if (byte_counter == 1){
				strcat(lineTemp, " ");

			}
			if (byte_counter == 3){
				strcat(lineTemp, " ");
			}
			if (byte_counter == 5){
				strcat(lineTemp, " ");
			}



			if (byte_counter == 7 || byte_counter+1 == length )
			{
				if (j == 0){
					strcpy(line0, lineTemp);
				}
				if (j == 1){
					strcpy(line1, lineTemp);
				}
				if (j == 2){
					strcpy(line2, lineTemp);
				}
				if (j == 3){
					strcpy(line3, lineTemp);
				}
				if (j == 4){
					strcpy(line4, lineTemp);
				}
				if (j == 5){
					strcpy(line5, lineTemp);
				}
				if (j == 6){
					strcpy(line6, lineTemp);
				}
				if (j == 7){
					strcpy(line7, lineTemp);
				}
				if (j == 8){
					strcpy(line8, lineTemp);
				}
				if (j == 9){
					strcpy(line9, lineTemp);
				}
				j++;
//				writeEinkDisplay(lineTemp, false, 10, 10, "",false,10,30, "",false,10,50, "",false,0,0, "",false,0,0);
//				waitForNoButtonPress();
//				if (waitForButtonPress())
//				{
//					clearLcd();
//					return true;
//				}
//				clearLcd();
				byte_counter = 0;

				strcpy(lineTemp, "");
			}else{
				byte_counter++;
			}

			// The following code will output the seed in the format:
			// " xxxx xxxx xxxx"
			// " xxxx xxxx xxxx"
//			if ((byte_counter & 1) == 0)
//			{
//				writeString(str, false);
////				strcat(lineTemp, str);
//			}
//			else
//			{
//				// Don't include space.
//				writeString(&(str[1]), false);
////				strcat(lineTemp, str);
//
//			}
//
//			if (byte_counter == 0)
//			{
//				gotoStartOfLine(0);
////				strcpy(line0, lineTemp);
//
//			}
//			else if (byte_counter == 6)
//			{
//				gotoStartOfLine(1);
////				strcpy(line1, lineTemp);
//			}


		}

	char stream_bytes_display[16];
	sprintf(stream_bytes_display,"%lu", length);

	writeEinkDisplay("length", false, 10, 10, stream_bytes_display,false,10,30, "",false,10,50, "",false,0,0, "",false,0,0);

	writeEinkDisplay(line0, false, 10, 10, line1,false,10,25, line2,false,10,40, line3,false,10,55, line4,false,10,70);

	waitForNoButtonPress();
	if (waitForButtonPress())
	{
		return true;
	}
//	if(line5 != "")
//	{
//		writeEinkDisplay(line5, false, 10, 10, line6,false,10,25, line7,false,10,40, line8,false,10,55, line9,false,10,70);
//		waitForNoButtonPress();
//		if (waitForButtonPress())
//		{
//			return true;
//		}
//	}


	return false;
}


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
bool displayBigHexStream(uint8_t *stream, uint32_t length)
{
	uint32_t i;
	uint8_t one_byte; // current byte of seed
	uint8_t byte_counter; // current byte on screen, 0 = first, 1 = second etc.

	uint32_t totalCount;

	uint8_t innerLength = 40;
	char str[3];
	char lineTemp[64] = "";
	char line0[24] = "";
	char line1[24] = "";
	char line2[24] = "";
	char line3[24] = "";
	char line4[24] = "";
	int j = 0;

	char stream_bytes_display[16];
	sprintf(stream_bytes_display,"%lu", length);

	writeEinkDisplay("length", false, 10, 10, stream_bytes_display,false,10,30, "",false,10,50, "",false,0,0, "",false,0,0);
	byte_counter = 0;

	for (totalCount = 0; totalCount < length; totalCount = totalCount + 40)
	{
		str[2] = '\0';
		j = 0;
		strcpy(lineTemp, "");
		if(totalCount > length - 40)
		{
			innerLength = length - totalCount;
		}
		for (i = 0; i < innerLength-1; i++)
			{
				one_byte = stream[i+totalCount];
				str[0] = nibbleToHex((uint8_t)(one_byte >> 4));
				str[1] = nibbleToHex(one_byte);
				strcat(lineTemp, str);

				if (byte_counter == 1)
				{
					strcat(lineTemp, " ");
				}

				if (byte_counter == 3)
				{
					strcat(lineTemp, " ");
				}

				if (byte_counter == 5)
				{
					strcat(lineTemp, " ");
				}


				if (byte_counter == 7 || byte_counter+1 == length )
				{
					if (j == 0)
					{
						strcpy(line0, lineTemp);
					}

					if (j == 1)
					{
						strcpy(line1, lineTemp);
					}

					if (j == 2)
					{
						strcpy(line2, lineTemp);
					}

					if (j == 3)
					{
						strcpy(line3, lineTemp);
					}

					if (j == 4)
					{
						strcpy(line4, lineTemp);
					}

					j++;

					byte_counter = 0;

					strcpy(lineTemp, "");
				}else{
					byte_counter++;
				}
			}


		writeEinkDisplay(line0, false, 10, 10, line1,false,10,25, line2,false,10,40, line3,false,10,55, line4,false,10,70);

//		waitForNoButtonPress();
//		if (waitForButtonPress())
//		{
//			return true;
//		}
		strcpy(line0, " ");
		strcpy(line1, " ");
		strcpy(line2, " ");
		strcpy(line3, " ");
		strcpy(line4, " ");
	}
	return false;
}

















