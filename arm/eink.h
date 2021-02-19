/*
 * blinker.h
 *
 *  Created on: Jul 14, 2014
 *      Author: thesquid
 */

#ifndef EINK_H_
#define EINK_H_

#ifdef __cplusplus
     extern "C" {
#endif
     void initEink();
     void writeEinkNoDisplaySingleBig(char *toDisplayLine0, int x0, int y0);
     void writeEinkDrawNumberSingleBig(long theNumber, int x0, int y0);
     void writeEinkDrawNumberSingle(long theNumber, int x0, int y0);
     void writeEinkDisplayNumberSingleBig(long theNumber, int x0, int y0);
     void manualWriteDisplay(void);
     void writeEinkChar(char toDisplay, bool runDisplay, int x, int y);
     void writeEink(char *, bool is_progmem, int x, int y);
     void writeEinkDisplay(	char *toDisplayLine0, bool is_progmem0, int x0, int y0,
     						char *toDisplayLine1, bool is_progmem1, int x1, int y1,
     						char *toDisplayLine2, bool is_progmem2, int x2, int y2,
     						char *toDisplayLine3, bool is_progmem3, int x3, int y3,
     						char *toDisplayLine4, bool is_progmem4, int x4, int y4);

     void writeEinkDrawUnicode(	unsigned int *toDisplayLine0, int l0, int x0, int y0,
     								unsigned int *toDisplayLine1, int l1, int x1, int y1,
     								unsigned int *toDisplayLine2, int l2, int x2, int y2,
     								unsigned int *toDisplayLine3, int l3, int x3, int y3,
     								unsigned int *toDisplayLine4, int l4, int x4, int y4  );

     void writeEinkDrawUnicodeSingle(unsigned int *toDisplayLine0, int l0, int x0, int y0);

     void writeEinkDisplayUnicode(	unsigned int *toDisplayLine0, bool is_progmem0, int l0, int x0, int y0,
     								unsigned int *toDisplayLine1, bool is_progmem1, int l1, int x1, int y1,
     								unsigned int *toDisplayLine2, bool is_progmem2, int l2, int x2, int y2,
     								unsigned int *toDisplayLine3, bool is_progmem3, int l3, int x3, int y3,
     								unsigned int *toDisplayLine4, bool is_progmem4, int l4, int x4, int y4  );

     void writeEinkDisplayUnicode_transaction(	unsigned int *toDisplayLine0, bool is_progmem0, int l0, int x0, int y0,
     											unsigned int *toDisplayLine1, bool is_progmem1, int l1, int x1, int y1,
     											unsigned int *toDisplayLine2, bool is_progmem2, int l2, int x2, int y2,
     											unsigned int *toDisplayLine3, bool is_progmem3, int l3, int x3, int y3,
     											unsigned int *toDisplayLine4, bool is_progmem4, int l4, int x4, int y4,
     											char *toDisplayLine5, bool is_progmem5, int x5, int y5,
     											char *toDisplayLine6, bool is_progmem6, int x6, int y6,
     											char *toDisplayLine7, bool is_progmem7, int x7, int y7);

     void writeEinkDisplayUnicode_transactionND(	unsigned int *toDisplayLine0, bool is_progmem0, int l0, int x0, int y0,
     											unsigned int *toDisplayLine1, bool is_progmem1, int l1, int x1, int y1,
     											unsigned int *toDisplayLine2, bool is_progmem2, int l2, int x2, int y2,
     											unsigned int *toDisplayLine3, bool is_progmem3, int l3, int x3, int y3,
     											unsigned int *toDisplayLine4, bool is_progmem4, int l4, int x4, int y4,
     											char *toDisplayLine5, bool is_progmem5, int x5, int y5,
     											char *toDisplayLine6, bool is_progmem6, int x6, int y6,
     											char *toDisplayLine7, bool is_progmem7, int x7, int y7);

     void writeQRcode(char toEncode[35]);
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
     						char *toDisplayLine11, int x11, int y11 );
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
     						char *toDisplayLine11, int x11, int y11 );
     void writeBlankScreen(void);
     void writeSleepScreen(void);
     void writeSplashScreen(void);
     void writeUSB_BLE_Screen(void);
     void writeUSB_Screen(void);
     void writeBLE_Screen(void);
     void writeCheck_Screen(void);
     void writeX_Screen(void);
//     void writeNotEqual_Screen(void);
//     void writeWorking_Screen(void);
     void overlayBatteryStatus(bool displayValue);
     void drawCheck(int x, int y);
     void drawX(int x, int y);
     void drawCheckX(void);
     void initDisplay(void);
     void display(void);
     void writeUnderline(int x0, int y0, int x1, int y1);
     void drawCAPSLOCK(int x, int y);



     void writeEinkNoDisplay(char *toDisplayLine0, int x0, int y0,
     						char *toDisplayLine1, int x1, int y1,
     						char *toDisplayLine2, int x2, int y2,
     						char *toDisplayLine3, int x3, int y3,
     						char *toDisplayLine4, int x4, int y4 );

     void writeEinkNoDisplaySingle(	char *toDisplayLine0, int x0, int y0);

     void write_moof(void);


#ifdef __cplusplus
     }
#endif

#endif /* EINK_H_ */
