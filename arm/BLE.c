/*
 * BLE.c
 *
 *  Created on: Sep 10, 2014
 *      Author: thesquid
 */

#include "Arduino.h"
#include "BLE.h"
#include "main.h"
#include "hwinit.h"
#include "../storage_common.h"

#if defined(__SAM3X8E__)
#define ENABLE_PIN  28
#define BRTS_PIN  25
#define BCTS_PIN  24
#endif

#if defined(__SAM3A8C__)
#define ENABLE_PIN  4
#define BRTS_PIN  3
#define BCTS_PIN  10
#endif

int ENABLE = ENABLE_PIN;
int BRTS = BRTS_PIN;
int BCTS = BCTS_PIN;

//const int BLE_EN_PIN = 28;//nee 23

void toggleAEM(bool useAEM)
{
	int s = 0;
	uint8_t temp1[1] = {};

	if(useAEM)
	{
		s = 127;
		temp1[0] = s;
		nonVolatileWrite(temp1, AEM_USE_ADDRESS, 1);
	}else
	{
		s = 128;
		temp1[0] = s;
		nonVolatileWrite(temp1, AEM_USE_ADDRESS, 1);
	}
}

int checkBLE(void)
{
	int useBLE;
	uint8_t tempComms[1];
	nonVolatileRead(tempComms, DEVICE_COMMS_SET_ADDRESS, 1);

	useBLE = (int)tempComms[0];

	return useBLE;
}


void activateBLE(void)
{
    pinMode( ENABLE, OUTPUT );
    pinMode(BRTS, OUTPUT);
    pinMode(BCTS, OUTPUT);
    digitalWrite( ENABLE, LOW );
}

void deactivateBLE(void)
{
    pinMode( ENABLE, OUTPUT );
    digitalWrite( ENABLE, HIGH );
}


int checkisFormatted(void)
{
	int isForm;
	uint8_t tempComms[1];
	nonVolatileRead(tempComms, IS_FORMATTED_ADDRESS, 1);

	isForm = (int)tempComms[0];

	return isForm;
}

int checkHasPIN(void)
{
	int isForm;
	uint8_t tempComms[1];
	nonVolatileRead(tempComms, HAS_PIN_ADDRESS, 1);

	isForm = (int)tempComms[0];

	return isForm;
}

int checkUseAEM(void)
{
	int isForm;
	uint8_t tempComms[1];
	nonVolatileRead(tempComms, AEM_USE_ADDRESS, 1);

	isForm = (int)tempComms[0];

	return isForm;
}

int checkSetupType(void)
{
	int isForm;
	uint8_t tempComms[1];
	nonVolatileRead(tempComms, SETUP_TYPE_ADDRESS, 1);

	isForm = (int)tempComms[0];

	return isForm;
}


