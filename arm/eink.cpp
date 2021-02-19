/*
 * eink.cpp
 *
 *  Created on: Jul 14, 2014
 *      Author: thesquid
 */
#include "Arduino.h"
#include "eink.h"
#include "due_ePaper_lib/due_ePaper.h"
#include <SPI.h>
#include <SD.h>
#include "due_ePaper_lib/due_GT20L16_drive.h"
//#include <avr/pgmspace.h>
#include "due_qrencode_lib/due_qrencode.h"
#include "lcd_and_input.h"
#include <string.h>
#include "avr2arm.h"

#include "picture.h"
#include "keypad_alpha.h"
#include "../hwinterface.h"


//#define progMemBuffer 128

#define SCREEN_SIZE 200                 // choose screen size: 144, 200, 270

#if (SCREEN_SIZE == 144)
#define EPD_SIZE    EPD_1_44

#elif (SCREEN_SIZE == 200)
#define EPD_SIZE    EPD_2_0
//#define IMAGEFILE   image_200_0
#define IMAGEFILE   bitlox_key_bits

#elif (SCREEN_SIZE == 270)
#define EPD_SIZE    EPD_2_7

#else
#error "Unknown EPB size: Change the #define SCREEN_SIZE to a supported value"
#endif

void initEink(){
    EPAPER.begin(EPD_SIZE);                             // setup epaper, size
    EPAPER.setDirection(DIRDOWN);                     // set display direction
    eSD.begin(EPD_SIZE);
    GT20L16.begin();
}

#define MAX_STRING 24
//char stringBufferSingle[MAX_STRING] = {0};
//char stringBuffer0[MAX_STRING] = {0};
//char stringBuffer1[MAX_STRING] = {0};
//char stringBuffer2[MAX_STRING] = {0};
//char stringBuffer3[MAX_STRING] = {0};
//char stringBuffer4[MAX_STRING] = {0};
//char stringBuffer5[MAX_STRING] = {0};
//char stringBuffer6[MAX_STRING] = {0};
//char stringBuffer7[MAX_STRING] = {0};
//
//char* getStringSingle(char* strSingle) {
//	strcpy_P(stringBufferSingle, (char*)strSingle);
//	return stringBufferSingle;
//}
//char* getString0(char* str0) {
//	strcpy_P(stringBuffer0, (char*)str0);
//	return stringBuffer0;
//}
//char* getString1(char* str1) {
//	strcpy_P(stringBuffer1, (char*)str1);
//	return stringBuffer1;
//}
//char* getString2(char* str2) {
//	strcpy_P(stringBuffer2, (char*)str2);
//	return stringBuffer2;
//}
//char* getString3(char* str3) {
//	strcpy_P(stringBuffer3, (char*)str3);
//	return stringBuffer3;
//}
//char* getString4(char* str4) {
//	strcpy_P(stringBuffer4, (char*)str4);
//	return stringBuffer4;
//}
//char* getString5(char* str5) {
//	strcpy_P(stringBuffer5, (char*)str5);
//	return stringBuffer5;
//}
//char* getString6(char* str6) {
//	strcpy_P(stringBuffer6, (char*)str6);
//	return stringBuffer6;
//}
//char* getString7(char* str7) {
//	strcpy_P(stringBuffer7, (char*)str7);
//	return stringBuffer7;
//}

void manualWriteDisplay(void)
{
	EPAPER.display();
}


void writeUnderline(int x0, int y0, int x1, int y1)
{
	EPAPER.drawLine(x0, y0, x1, y1);
}

void writeEinkDisplay(	char *toDisplayLine0, bool is_progmem0, int x0, int y0,
						char *toDisplayLine1, bool is_progmem1, int x1, int y1,
						char *toDisplayLine2, bool is_progmem2, int x2, int y2,
						char *toDisplayLine3, bool is_progmem3, int x3, int y3,
						char *toDisplayLine4, bool is_progmem4, int x4, int y4 )
{
	initEink();

	#if defined(__MSP430_CPU__) || defined(__SAM3X8E__)|| defined(__SAM3A8C__)
	is_progmem0 = false;
	is_progmem1 = false;
	is_progmem2 = false;
	is_progmem3 = false;
	is_progmem4 = false;
	#endif


	char *line0;
	char *line1;
	char *line2;
	char *line3;
	char *line4;

	line0 = toDisplayLine0;
	line1 = toDisplayLine1;
	line2 = toDisplayLine2;
	line3 = toDisplayLine3;
	line4 = toDisplayLine4;


//	overlayBatteryStatus(BATT_VALUE_DISPLAY);

    EPAPER.drawString(line0, x0, y0);
    EPAPER.drawString(line1, x1, y1);
    EPAPER.drawString(line2, x2, y2);
    EPAPER.drawString(line3, x3, y3);
    EPAPER.drawString(line4, x4, y4);


    EPAPER.display();                                   // use only once

}

void writeEinkNoDisplay(char *toDisplayLine0, int x0, int y0,
						char *toDisplayLine1, int x1, int y1,
						char *toDisplayLine2, int x2, int y2,
						char *toDisplayLine3, int x3, int y3,
						char *toDisplayLine4, int x4, int y4 )
{




    EPAPER.drawString(toDisplayLine0, x0, y0);
    EPAPER.drawString(toDisplayLine1, x1, y1);
    EPAPER.drawString(toDisplayLine2, x2, y2);
    EPAPER.drawString(toDisplayLine3, x3, y3);
    EPAPER.drawString(toDisplayLine4, x4, y4);



}
void writeEinkNoDisplaySingle(char *toDisplayLine0, int x0, int y0)
{
    EPAPER.drawString(toDisplayLine0, x0, y0);
}
void writeEinkNoDisplaySingleBig(char *toDisplayLine0, int x0, int y0)
{
    EPAPER.drawStringBig(toDisplayLine0, x0, y0);
}

void writeEinkDisplayNumberSingleBig(long theNumber, int x0, int y0)
{
    initDisplay();
	EPAPER.drawNumberBig(theNumber, x0, y0);
	display();
}

void writeEinkDrawNumberSingleBig(long theNumber, int x0, int y0)
{
 	EPAPER.drawNumberBig(theNumber, x0, y0);
}
void writeEinkDrawNumberSingle(long theNumber, int x0, int y0)
{
 	EPAPER.drawNumber(theNumber, x0, y0);
}


#define MAX_INT 24 // 24
#define DOUBLE_MAX_INT 48 // 48

unsigned int intBuffer0[MAX_INT] = {0};
unsigned int intBuffer1[MAX_INT] = {0};
unsigned int intBuffer2[MAX_INT] = {0};
unsigned int intBuffer3[MAX_INT] = {0};
unsigned int intBuffer4[MAX_INT] = {0};


//unsigned int* getInt0(unsigned int* int0) {
//	memcpy_P(intBuffer0, (unsigned int*)int0, DOUBLE_MAX_INT);
//	return intBuffer0;
//}
//unsigned int* getInt1(unsigned int* int1) {
//	memcpy_P(intBuffer1, (unsigned int*)int1, DOUBLE_MAX_INT);
//	return intBuffer1;
//}
//unsigned int* getInt2(unsigned int* int2) {
//	memcpy_P(intBuffer2, (unsigned int*)int2, DOUBLE_MAX_INT);
//	return intBuffer2;
//}
//unsigned int* getInt3(unsigned int* int3) {
//	memcpy_P(intBuffer3, (unsigned int*)int3, DOUBLE_MAX_INT);
//	return intBuffer3;
//}
//unsigned int* getInt4(unsigned int* int4) {
//	memcpy_P(intBuffer4, (unsigned int*)int4, DOUBLE_MAX_INT);
//	return intBuffer4;
//}


void writeEinkDisplayUnicode(	unsigned int *toDisplayLine0, bool is_progmem0, int l0, int x0, int y0,
								unsigned int *toDisplayLine1, bool is_progmem1, int l1, int x1, int y1,
								unsigned int *toDisplayLine2, bool is_progmem2, int l2, int x2, int y2,
								unsigned int *toDisplayLine3, bool is_progmem3, int l3, int x3, int y3,
								unsigned int *toDisplayLine4, bool is_progmem4, int l4, int x4, int y4  )
{
	initEink();

	#if defined(__MSP430_CPU__) || defined(__SAM3X8E__)|| defined(__SAM3A8C__)
	is_progmem0 = false;
	is_progmem1 = false;
	is_progmem2 = false;
	is_progmem3 = false;
	is_progmem4 = false;
	#endif

	unsigned int *line0;
	unsigned int *line1;
	unsigned int *line2;
	unsigned int *line3;
	unsigned int *line4;

		line0 = toDisplayLine0;
		line1 = toDisplayLine1;
		line2 = toDisplayLine2;
		line3 = toDisplayLine3;
		line4 = toDisplayLine4;

//    int timer1 = millis();
    EPAPER.drawUnicodeString(line0, l0, x0, y0);
    EPAPER.drawUnicodeString(line1, l1, x1, y1);
    EPAPER.drawUnicodeString(line2, l2, x2, y2);
    EPAPER.drawUnicodeString(line3, l3, x3, y3);
    EPAPER.drawUnicodeString(line4, l4, x4, y4);

	overlayBatteryStatus(BATT_VALUE_DISPLAY);

	EPAPER.display();                                   // use only once
}

void writeEinkDrawUnicode(	unsigned int *toDisplayLine0, int l0, int x0, int y0,
								unsigned int *toDisplayLine1, int l1, int x1, int y1,
								unsigned int *toDisplayLine2, int l2, int x2, int y2,
								unsigned int *toDisplayLine3, int l3, int x3, int y3,
								unsigned int *toDisplayLine4, int l4, int x4, int y4  )
{
	unsigned int *line0;
	unsigned int *line1;
	unsigned int *line2;
	unsigned int *line3;
	unsigned int *line4;

	line0 = toDisplayLine0;
	line1 = toDisplayLine1;
	line2 = toDisplayLine2;
	line3 = toDisplayLine3;
	line4 = toDisplayLine4;

	EPAPER.drawUnicodeString(line0, l0, x0, y0);
    EPAPER.drawUnicodeString(line1, l1, x1, y1);
    EPAPER.drawUnicodeString(line2, l2, x2, y2);
    EPAPER.drawUnicodeString(line3, l3, x3, y3);
    EPAPER.drawUnicodeString(line4, l4, x4, y4);
}


void writeEinkDrawUnicodeSingle(unsigned int *toDisplayLine0, int l0, int x0, int y0)
{
	unsigned int *line0;

	line0 = toDisplayLine0;

//	unsigned int tempLine[] = {0x7801};
//
//	line0 = tempLine;

	EPAPER.drawUnicodeString(line0, l0, x0, y0);
}

void writeEinkDisplayUnicode_transaction(	unsigned int *toDisplayLine0, bool is_progmem0, int l0, int x0, int y0,
											unsigned int *toDisplayLine1, bool is_progmem1, int l1, int x1, int y1,
											unsigned int *toDisplayLine2, bool is_progmem2, int l2, int x2, int y2,
											unsigned int *toDisplayLine3, bool is_progmem3, int l3, int x3, int y3,
											unsigned int *toDisplayLine4, bool is_progmem4, int l4, int x4, int y4,
											char *toDisplayLine5, bool is_progmem5, int x5, int y5,
											char *toDisplayLine6, bool is_progmem6, int x6, int y6,
											char *toDisplayLine7, bool is_progmem7, int x7, int y7)
{
	initEink();

	#if defined(__MSP430_CPU__) || defined(__SAM3X8E__)|| defined(__SAM3A8C__)
	is_progmem0 = false;
	is_progmem1 = false;
	is_progmem2 = false;
	is_progmem3 = false;
	is_progmem4 = false;
	is_progmem5 = false;
	is_progmem6 = false;
	is_progmem7 = false;
	#endif


	char *line5;
	char *line6;
	char *line7;
		line5 = toDisplayLine5;
		line6 = toDisplayLine6;
		line7 = toDisplayLine7;


	unsigned int *line0;
	unsigned int *line1;
	unsigned int *line2;
	unsigned int *line3;
	unsigned int *line4;

		line0 = toDisplayLine0;
		line1 = toDisplayLine1;
		line2 = toDisplayLine2;
		line3 = toDisplayLine3;
		line4 = toDisplayLine4;

//    int timer1 = millis();
    EPAPER.drawUnicodeString(line0, l0, x0, y0);
    EPAPER.drawUnicodeString(line1, l1, x1, y1);
    EPAPER.drawUnicodeString(line2, l2, x2, y2);
    EPAPER.drawUnicodeString(line3, l3, x3, y3);
    EPAPER.drawUnicodeString(line4, l4, x4, y4);

    EPAPER.drawString(line5, x5, y5);
    EPAPER.drawString(line6, x6, y6);
    EPAPER.drawString(line7, x7, y7);

	overlayBatteryStatus(BATT_VALUE_DISPLAY);

    EPAPER.display();                                   // use only once

}

void writeEinkDisplayUnicode_transactionND(	unsigned int *toDisplayLine0, bool is_progmem0, int l0, int x0, int y0,
											unsigned int *toDisplayLine1, bool is_progmem1, int l1, int x1, int y1,
											unsigned int *toDisplayLine2, bool is_progmem2, int l2, int x2, int y2,
											unsigned int *toDisplayLine3, bool is_progmem3, int l3, int x3, int y3,
											unsigned int *toDisplayLine4, bool is_progmem4, int l4, int x4, int y4,
											char *toDisplayLine5, bool is_progmem5, int x5, int y5,
											char *toDisplayLine6, bool is_progmem6, int x6, int y6,
											char *toDisplayLine7, bool is_progmem7, int x7, int y7)
{

	#if defined(__MSP430_CPU__) || defined(__SAM3X8E__)|| defined(__SAM3A8C__)
	is_progmem0 = false;
	is_progmem1 = false;
	is_progmem2 = false;
	is_progmem3 = false;
	is_progmem4 = false;
	is_progmem5 = false;
	is_progmem6 = false;
	is_progmem7 = false;
	#endif


	char *line5;
	char *line6;
	char *line7;
		line5 = toDisplayLine5;
		line6 = toDisplayLine6;
		line7 = toDisplayLine7;


	unsigned int *line0;
	unsigned int *line1;
	unsigned int *line2;
	unsigned int *line3;
	unsigned int *line4;

		line0 = toDisplayLine0;
		line1 = toDisplayLine1;
		line2 = toDisplayLine2;
		line3 = toDisplayLine3;
		line4 = toDisplayLine4;

//    int timer1 = millis();
    EPAPER.drawUnicodeString(line0, l0, x0, y0);
    EPAPER.drawUnicodeString(line1, l1, x1, y1);
    EPAPER.drawUnicodeString(line2, l2, x2, y2);
    EPAPER.drawUnicodeString(line3, l3, x3, y3);
    EPAPER.drawUnicodeString(line4, l4, x4, y4);

    EPAPER.drawString(line5, x5, y5);
    EPAPER.drawString(line6, x6, y6);
    EPAPER.drawString(line7, x7, y7);



}

void writeEinkChar(char toDisplay, bool runDisplay, int x, int y)
{
	initEink();
	EPAPER.drawChar(toDisplay, x, y);
	if(runDisplay)
	{
		EPAPER.display();
	}
}

void writeEink(char *toDisplay, bool is_progmem, int x, int y )
{
	initEink();

	#if defined(__MSP430_CPU__) || defined(__SAM3X8E__)|| defined(__SAM3A8C__)
	is_progmem = false;
	#endif

    EPAPER.drawString(toDisplay, x, y);

	overlayBatteryStatus(BATT_VALUE_DISPLAY);

    EPAPER.display();                                   // use only once

}

/*
 *      Remove given section from string. Negative len means remove
 *      everything up to the end.
 */
int str_cut_e(char *str, int begin, int len)
{
    int l = strlen(str);

    if (len < 0) len = l - begin;
    if (begin + len > l) len = l - begin;
    memmove(str + begin, str + begin + len, l - len + 1);

    return len;
}


void writeQRcode(char toEncode[35])
{
//	char toEncode[35] ="1EPKg1FhN9HDJ87Cprs2V83X7dd74v9PrE";
	initDisplay();
	qrcontext qr;
	qr.qrencode(toEncode);

	for(int y = 0 ;y<WD;y++){
		for(int x = 0 ;x<WD;x++){
			if( qr.getQRBit(x,y) ){
				EPAPER.fillRectangle((x*2)+10, (y*2)+6, 1, 2);
			}
		}
	}

	char add1[35];
	strcpy (add1, toEncode);
	str_cut_e(add1, 11, -1);
	char add2[35];
	strcpy (add2, toEncode);
	str_cut_e(add2, 0, 11);
	str_cut_e(add2, 11, -1);
	char add3[35];
	strcpy (add3, toEncode);
	str_cut_e(add3, 0, 22);


	EPAPER.drawString(add1, 100, 25);
	EPAPER.drawString(add2, 100, 43);
	EPAPER.drawString(add3, 100, 61);

	overlayBatteryStatus(BATT_VALUE_DISPLAY);

	display();
}


void writeEinkDisplayBig(	char *toDisplayLine0, int x0, int y0,
						char *toDisplayLine1, int x1, int y1,
						char *toDisplayLine2, int x2, int y2,
						char *toDisplayLine3, int x3, int y3,
						char *toDisplayLine4, int x4, int y4,
						char *toDisplayLine5, int x5, int y5,
						char *toDisplayLine6, int x6, int y6,
						char *toDisplayLine7, int x7, int y7,
						char *toDisplayLine8, int x8, int y8,
						char *toDisplayLine9, int x9, int y9,
						char *toDisplayLine10, int x10, int y10,
						char *toDisplayLine11, int x11, int y11 )
{
//	initEink();
    EPAPER.drawString(toDisplayLine0, x0, y0);
    EPAPER.drawString(toDisplayLine1, x1, y1);
    EPAPER.drawString(toDisplayLine2, x2, y2);
    EPAPER.drawString(toDisplayLine3, x3, y3);
    EPAPER.drawString(toDisplayLine4, x4, y4);
    EPAPER.drawString(toDisplayLine5, x5, y5);
    EPAPER.drawString(toDisplayLine6, x6, y6);
    EPAPER.drawString(toDisplayLine7, x7, y7);
    EPAPER.drawString(toDisplayLine8, x8, y8);
    EPAPER.drawString(toDisplayLine9, x9, y9);
    EPAPER.drawString(toDisplayLine10, x10, y10);
    EPAPER.drawString(toDisplayLine11, x11, y11);

    EPAPER.display();                                   // use only once

}

void writeEinkDisplayPrep(	char *toDisplayLine0, int x0, int y0,
						char *toDisplayLine1, int x1, int y1,
						char *toDisplayLine2, int x2, int y2,
						char *toDisplayLine3, int x3, int y3,
						char *toDisplayLine4, int x4, int y4,
						char *toDisplayLine5, int x5, int y5,
						char *toDisplayLine6, int x6, int y6,
						char *toDisplayLine7, int x7, int y7,
						char *toDisplayLine8, int x8, int y8,
						char *toDisplayLine9, int x9, int y9,
						char *toDisplayLine10, int x10, int y10,
						char *toDisplayLine11, int x11, int y11 )
{
	initEink();

    EPAPER.drawString(toDisplayLine0, x0, y0);
    EPAPER.drawString(toDisplayLine1, x1, y1);
    EPAPER.drawString(toDisplayLine2, x2, y2);
    EPAPER.drawString(toDisplayLine3, x3, y3);
    EPAPER.drawString(toDisplayLine4, x4, y4);
    EPAPER.drawString(toDisplayLine5, x5, y5);
    EPAPER.drawString(toDisplayLine6, x6, y6);
    EPAPER.drawString(toDisplayLine7, x7, y7);
    EPAPER.drawString(toDisplayLine8, x8, y8);
    EPAPER.drawString(toDisplayLine9, x9, y9);
    EPAPER.drawString(toDisplayLine10, x10, y10);
    EPAPER.drawString(toDisplayLine11, x11, y11);

}

void overlayBatteryStatus(bool displayValue)
{
	int battery = batteryLevel();
	int battLevel;

	char batteryTxt[16];
	sprintf(batteryTxt, "%lu", battery);

if(battery > 700){
		battLevel = 15;
}else if(battery <= 700 && battery > 695){
		battLevel = 14;
}else if(battery <= 695 && battery > 690){
		battLevel = 13;
}else if(battery <= 690 && battery > 685){
		battLevel = 12;
}else if(battery <= 685 && battery > 680){
		battLevel = 11;
}else if(battery <= 680 && battery > 675){
		battLevel = 10;
}else if(battery <= 675 && battery > 670){
		battLevel = 9;
}else if(battery <= 670 && battery > 665){
		battLevel = 8;
}else if(battery <= 665 && battery > 660){
		battLevel = 7;
}else if(battery <= 660 && battery > 655){
		battLevel = 6;
}else if(battery <= 655 && battery > 650){
		battLevel = 5;
}else if(battery <= 650 && battery > 645){
		battLevel = 4;
}else if(battery <= 645 && battery > 640){
		battLevel = 3;
}else if(battery <= 640 && battery > 635){
		battLevel = 2;
}else if(battery <= 635 && battery > 630){
		battLevel = 1;
}else if(battery <= 630){
		battLevel = 0;
	}

if(battery > 705){
//		DRAW CHARGING ICON
}

	EPAPER.drawRectangle(179, 5, 15, 7);
    EPAPER.drawRectangle(194, 7, 2, 3);
    EPAPER.fillRectangle(179, 5, battLevel, 7);
    if(displayValue){
    	EPAPER.drawString(batteryTxt, 153, LINE_1_Y);
    }

}

void drawCheck(int x, int y)
{
	x = x + 2;

	EPAPER.drawLine(x+1,y-1,x+6,y+4);
	EPAPER.drawLine(x+1,y,x+6,y+5);
	EPAPER.drawLine(x,y,x+5,y+5);
	EPAPER.drawLine(x,y+1,x+5,y+6);
	EPAPER.drawLine(x-1,y+1,x+4,y+6);

	EPAPER.drawLine(x+7,y+4,x+14,y-3);
	EPAPER.drawLine(x+7,y+3,x+13,y-3);
	EPAPER.drawLine(x+6,y+3,x+13,y-4);
	EPAPER.drawLine(x+6,y+2,x+12,y-4);
	EPAPER.drawLine(x+5,y+2,x+12,y-5);
}

void drawX(int x, int y)
{
	x = x - 1;
	EPAPER.drawLine(x+5,y+6,x+14,y-3);
	EPAPER.drawLine(x+5,y+5,x+13,y-3);
	EPAPER.drawLine(x+4,y+5,x+13,y-4);
	EPAPER.drawLine(x+4,y+4,x+12,y-4);
	EPAPER.drawLine(x+3,y+4,x+12,y-5);

	EPAPER.drawLine(x+5,y-5,x+14,y+4);
	EPAPER.drawLine(x+5,y-4,x+13,y+4);
	EPAPER.drawLine(x+4,y-4,x+13,y+5);
	EPAPER.drawLine(x+4,y-3,x+12,y+5);
	EPAPER.drawLine(x+3,y-3,x+12,y+6);
}

void drawCAPSLOCK(int x, int y)
{
	x = x + 5;
	EPAPER.drawLine(x,y,x+5,y+5);
	EPAPER.drawLine(x,y,x-5,y+5);
	EPAPER.drawLine(x-5,y+5,x+5,y+5);
	EPAPER.drawLine(x-3,y+4,x+3,y+4);
	EPAPER.drawLine(x-2,y+3,x+2,y+3);
	EPAPER.drawLine(x-1,y+2,x+1,y+2);
	EPAPER.drawLine(x,y+1,x,y+1);

	EPAPER.fillRectangle(x-2,y+5, 4, 4);

	EPAPER.fillRectangle(x-2,y+10, 4, 2);

}


void initDisplay(void)
{
	initEink();
}

//void drawCheckX(void)
//{
//	drawCheck(5,85);
//	drawX(180,85);
//}

void display(void)
{
    EPAPER.display();
}

void writeBlankScreen(void)
{
    EPAPER.begin(EPD_SIZE);                             // setup epaper, size
    EPAPER.setDirection(DIRDOWN);                     // set display direction
    EPAPER.image_flash(white_bits);
}
void writeSplashScreen(void)
{
    EPAPER.begin(EPD_SIZE);                             // setup epaper, size
    EPAPER.setDirection(DIRDOWN);                     // set display direction
    EPAPER.image_flash(bitlox_key_bits);
}
void writeUSB_BLE_Screen(void)
{
    EPAPER.begin(EPD_SIZE);                             // setup epaper, size
    EPAPER.setDirection(DIRDOWN);                     // set display direction
    EPAPER.image_flash(USB_BLE_bits);
}
void writeUSB_Screen(void)
{
    EPAPER.begin(EPD_SIZE);                             // setup epaper, size
    EPAPER.setDirection(DIRDOWN);                     // set display direction
    EPAPER.image_flash(USB_bits);
}
void writeBLE_Screen(void)
{
    EPAPER.begin(EPD_SIZE);                             // setup epaper, size
    EPAPER.setDirection(DIRDOWN);                     // set display direction
    EPAPER.image_flash(BLE_bits);
}

void writeCheck_Screen(void)
{
    EPAPER.begin(EPD_SIZE);                             // setup epaper, size
    EPAPER.setDirection(DIRDOWN);                     // set display direction
    EPAPER.image_flash(check_bits);
}

void writeX_Screen(void)
{
    EPAPER.begin(EPD_SIZE);                             // setup epaper, size
    EPAPER.setDirection(DIRDOWN);                     // set display direction
    EPAPER.image_flash(x_mark_bits);
}

//void writeNotEqual_Screen(void)
//{
//    EPAPER.begin(EPD_SIZE);                             // setup epaper, size
//    EPAPER.setDirection(DIRDOWN);                     // set display direction
//    EPAPER.image_flash(not_equal_bits);
//}

//void writeWorking_Screen(void)
//{
//    EPAPER.begin(EPD_SIZE);                             // setup epaper, size
//    EPAPER.setDirection(DIRDOWN);                     // set display direction
//    EPAPER.image_flash(working_01_bits);
//}

//void write_moof(void)
//{
//    EPAPER.begin(EPD_SIZE);                             // setup epaper, size
//    EPAPER.setDirection(DIRDOWN);                     // set display direction
//    EPAPER.image_flash(moof_bits);
//}
void writeSleepScreen(void)
{
    EPAPER.begin(EPD_SIZE);                             // setup epaper, size
    EPAPER.setDirection(DIRDOWN);                     // set display direction
    EPAPER.image_flash(sleep_mode_bits);
}














