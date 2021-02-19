/*
 * BLE.h
 *
 *  Created on: Sep 10, 2014
 *      Author: thesquid
 */

#ifndef BLE_H_
#define BLE_H_

#ifdef __cplusplus
     extern "C" {
#endif

extern void activateBLE(void);
extern void deactivateBLE(void);
extern int checkBLE(void);
extern int checkisFormatted(void);
extern int checkHasPIN(void);
extern int checkSetupType(void);
extern int checkUseAEM(void);
extern void toggleAEM(bool useAEM);

#ifdef __cplusplus
     }
#endif

#endif /* BLE_H_ */
