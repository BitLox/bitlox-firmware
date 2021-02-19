/*
 * usart.h
 *
 *  Created on: Jan 27, 2015
 *      Author: thesquid
 */

#ifndef ARM_BITLOX_54_ARM_USART_H_
#define ARM_BITLOX_54_ARM_USART_H_

#ifdef __cplusplus
     extern "C" {
#endif
     void initFormatting(void);
//     extern uint8_t bluetooth_on;
//     extern uint8_t is_formatted;
//     extern uint8_t message_slice_counter;
     void checkFlush(void);
     void moof(void);
     void startTimer1(void);
     void stopTimer1(void);
     void startTimer2(void);
     void stopTimer2(void);
//     void serialToggle(void);

#ifdef __cplusplus
     }
#endif


#endif /* ARM_BITLOX_54_ARM_USART_H_ */
