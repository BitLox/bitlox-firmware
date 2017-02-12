/*
 * keypad_arm.cpp
 *
 *  Created on: Oct 12, 2014
 *      Author: thesquid
 */

#include "keypad_arm.h"
#include <Keypad.h>
#include "Arduino.h"

#if defined(__SAM3X8E__)
#define ROW_PIN_1      69
#define ROW_PIN_2      68
#define ROW_PIN_3      67
#define COL_PIN_1      66
#define COL_PIN_2      65
#define COL_PIN_3      64
#define COL_PIN_4      63

const byte ROWS = 3; //three rows
const byte COLS = 4; //four columns
char keys[ROWS][COLS] = {
  {'3','2','1','0'},
  {'7','6','5','4'},
  {'N','Y','9','8'}
};
byte rowPins[ROWS] = {ROW_PIN_1, ROW_PIN_2, ROW_PIN_3}; //connect to the row pinouts of the keypad A15 A14 A13
byte colPins[COLS] = {COL_PIN_1, COL_PIN_2, COL_PIN_3, COL_PIN_4}; //connect to the column pinouts of the keypad A12 A11 A10 A9

Keypad keypad = Keypad( makeKeymap(keys), rowPins, colPins, ROWS, COLS );



#endif

#if defined(__SAM3A8C__)
#define ROW_PIN_1      22
#define ROW_PIN_2      23
#define ROW_PIN_3      24
#define COL_PIN_1      18
#define COL_PIN_2      19
#define COL_PIN_3      20
#define COL_PIN_4      21
#endif




char getFullKeys(void)
{
#if defined(__SAM3X8E__)
	char key = keypad.getKey();
#endif
#if defined(__SAM3A8C__)
	char key = NewKeyRoutine();
#endif

	if (key){
		return(key);
	}

	return ' '; // have to change this to something not normally returned

}

char getAcceptCancelKeys(void)
{
#if defined(__SAM3X8E__)
	char key = keypad.getKey();
#endif
#if defined(__SAM3A8C__)
	char key = NewKeyRoutine();
#endif

	if (key=='Y' || key=='N'){
		return(key);
	}


	return ' ';
}

char NewKeyRoutine(void)
{
	const char* ptr_pressed;
	char pressed;

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


//	do
//	{
		  int buttonState;
		  int pushButton;
		  int pushOutput;

		  pushButton = ROW_PIN_3;
		  pushOutput = COL_PIN_1;
		  digitalWrite(pushOutput, LOW);
		  buttonState = digitalRead(pushButton);
		  if (buttonState == 0) {
//		    Serial.println("Button Pressed: 8");
		    return '9';// '8'; //1 is 8; button 8 is 8
		  }
		  delay(1);        // delay in between reads for stability
		  digitalWrite(pushOutput, HIGH);

		  pushButton = ROW_PIN_2;
		  pushOutput = COL_PIN_1;
		  digitalWrite(pushOutput, LOW);
		  buttonState = digitalRead(pushButton);
		  if (buttonState == 0) {
//		    Serial.println("Button Pressed: 7");
		    return '8'; // '7'; //7
		  }
		  delay(1);        // delay in between reads for stability
		  digitalWrite(pushOutput, HIGH);

		  pushButton = ROW_PIN_1;
		  pushOutput = COL_PIN_1;
		  digitalWrite(pushOutput, LOW);
		  buttonState = digitalRead(pushButton);
		  if (buttonState == 0) {
//		    Serial.println("Button Pressed: 3");
		    return '3'; //3
		  }
		  delay(1);        // delay in between reads for stability
		  digitalWrite(pushOutput, HIGH);




		  pushButton = ROW_PIN_3;
		  pushOutput = COL_PIN_2;
		  digitalWrite(pushOutput, LOW);
		  buttonState = digitalRead(pushButton);
		  if (buttonState == 0) {
//		    Serial.println("Button Pressed: N");  // 8 due
		    return 'Y'; // 'N'; //N
		  }
		  delay(1);        // delay in between reads for stability
		  digitalWrite(pushOutput, HIGH);

		  pushButton = ROW_PIN_2;
		  pushOutput = COL_PIN_2;
		  digitalWrite(pushOutput, LOW);
		  buttonState = digitalRead(pushButton);
		  if (buttonState == 0) {
//		    Serial.println("Button Pressed: 6");
		    return '7'; // '6'; //6
		  }
		  delay(1);        // delay in between reads for stability
		  digitalWrite(pushOutput, HIGH);

		  pushButton = ROW_PIN_1;
		  pushOutput = COL_PIN_2;
		  digitalWrite(pushOutput, LOW);
		  buttonState = digitalRead(pushButton);
		  if (buttonState == 0) {
//		    Serial.println("Button Pressed: 2");
		    return '2'; //2
		  }
		  delay(1);        // delay in between reads for stability
		  digitalWrite(pushOutput, HIGH);




		  pushButton = ROW_PIN_3;
		  pushOutput = COL_PIN_3;
		  digitalWrite(pushOutput, LOW);
		  buttonState = digitalRead(pushButton);
		  if (buttonState == 0) {
//		    Serial.println("Button Pressed: Y");
		    return '6'; //'Y'; // Y
		  }
		  delay(1);        // delay in between reads for stability
		  digitalWrite(pushOutput, HIGH);

		  pushButton = ROW_PIN_2;
		  pushOutput = COL_PIN_3;
		  digitalWrite(pushOutput, LOW);
		  buttonState = digitalRead(pushButton);
		  if (buttonState == 0) {
//		    Serial.println("Button Pressed: 5");
		    return '5'; //5
		  }
		  delay(1);        // delay in between reads for stability
		  digitalWrite(pushOutput, HIGH);

		  pushButton = ROW_PIN_1;
		  pushOutput = COL_PIN_3;
		  digitalWrite(pushOutput, LOW);
		  buttonState = digitalRead(pushButton);
		  if (buttonState == 0) {
//		    Serial.println("Button Pressed: 1");
		    return '1';
		  }
		  delay(1);        // delay in between reads for stability
		  digitalWrite(pushOutput, HIGH);




		  pushButton = ROW_PIN_3;
		  pushOutput = COL_PIN_4;
		  digitalWrite(pushOutput, LOW);
		  buttonState = digitalRead(pushButton);
		  if (buttonState == 0) {
//		    Serial.println("Button Pressed: 9");
		    return '0';// '9';
		  }
		  delay(1);        // delay in between reads for stability
		  digitalWrite(pushOutput, HIGH);

		  pushButton = ROW_PIN_2;
		  pushOutput = COL_PIN_4;
		  digitalWrite(pushOutput, LOW);
		  buttonState = digitalRead(pushButton);
		  if (buttonState == 0) {
//		    Serial.println("Button Pressed: 4");
		    return '4'; //4
		  }
		  delay(1);        // delay in between reads for stability
		  digitalWrite(pushOutput, HIGH);

		  pushButton = ROW_PIN_1;
		  pushOutput = COL_PIN_4;
		  digitalWrite(pushOutput, LOW);
		  buttonState = digitalRead(pushButton);
		  if (buttonState == 0) {
//		    Serial.println("Button Pressed: 0");
		    return 'N'; //'0';
		  }
		  delay(1);        // delay in between reads for stability
		  digitalWrite(pushOutput, HIGH);


//	} while (1);

	return ' ';

}

