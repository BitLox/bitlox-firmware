// Only modify this file to include
// - function definitions (prototypes)
// - include files
// - extern variable definitions
// In the appropriate section

#ifndef _ARM_Lockbox_1_H_
#define _ARM_Lockbox_1_H_
#include "Arduino.h"
//add your includes for the project ARM_Lockbox_1 here


//end of add your includes here
#ifdef __cplusplus
extern "C" {
#endif
void loop();
void setup();
void startTimer1(void);
void stopTimer1(void);

//void Software_Reset();
#ifdef __cplusplus
} // extern "C"
#endif

//add your function definitions for the project ARM_Lockbox_1 here
extern void useWhatComms(void);
extern void testFunc(void);
extern void useWhatSetup(void);
extern void useWhatCommsDuress(void);
extern void enterBackupMode(void);
//extern void startTimer1(void);
//extern void stopTimer1(void);

//Do not add code below this line
#endif /* _ARM_Lockbox_1_H_ */
