/** \file lcd_and_input.h
  *
  * \brief Describes functions exported by lcd_and_input.c
  *
  * This file is licensed as described by the file LICENCE.
  */

#ifndef LCD_AND_INPUT_H_INCLUDED
#define LCD_AND_INPUT_H_INCLUDED



#ifdef __cplusplus
     extern "C" {
#endif

//extern void initLcdAndInput(void);
extern void initLang(void);
extern void streamError(void);
//extern void writeString(const char *str, bool is_progmem); //put in to test LCD

//extern const unsigned int str_lang_list_UNICODE[][24];
//extern const unsigned int str_test_list_UNICODE[][24];
extern void showReady(void);
extern int str_cut(char *str, int begin, int len);
//extern void languageEEPROM(void);
extern void setLang(void);
extern void languageMenu(void);
extern void languageMenuInitially(void);

extern void resetLang(void);
extern void displayMnemonic(const char * mnemonicToDisplay, int length);
extern void displayMnemonicCheck(const char * mnemonicToDisplay);
//extern void inputMnemonic(char *tempMnemToSet);
extern char nibbleToHex(uint8_t nibble);
extern char waitForNumberButtonPress(void);
extern char waitForNumberButtonPress4to8(void);
extern bool displayHexStream(uint8_t *stream, uint8_t length);
extern bool displayBigHexStream(uint8_t *stream, uint32_t length);
extern void clearDisplay(void);
extern void clearDisplaySecure(void);
extern void showBattery(void);
extern bool waitForButtonPress(void);
extern void showWorking(void);





#ifdef __cplusplus
     }
#endif

#endif // #ifndef LCD_AND_INPUT_H_INCLUDED
