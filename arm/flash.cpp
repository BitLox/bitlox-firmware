/*
 * flash.cpp
 *
 *  Created on: Oct 11, 2014
 *      Author: thesquid
 */




/** \file eeprom.c
  *
  * \brief Reads and writes to the AVR's EEPROM.
  *
  * This contains functions which implement non-volatile storage using the
  * AVR's EEPROM. Compared to contemporary mass storage devices, the size of
  * the storage space is not much (only 1024 bytes on the ATmega328), but it's
  * enough to fit a couple of wallets.
  *
  * This file is licensed as described by the file LICENCE.
  */

//#include "flash.h"

#include "../common.h"
#include "../hwinterface.h"
#include "DueFlashStorage_lib/DueFlashStorage.h"

//#include "eink.h"

DueFlashStorage dueFlashStorage;

struct Configuration {
  uint8_t b;
};

/** Size of EEPROM, in number of bytes. */
//moved to hwinterface.h

/** Write to non-volatile storage.
  * \param data A pointer to the data to be written.
  * \param address Byte offset specifying where in non-volatile storage to
  *                start writing to.
  * \param length The number of bytes to write.
  * \return See #NonVolatileReturnEnum for return values.
  * \warning Writes may be buffered; use nonVolatileFlush() to be sure that
  *          data is actually written to non-volatile storage.
  */
NonVolatileReturn nonVolatileWrite(uint8_t *data, uint32_t address, uint32_t length)
{
	if ((address > EEPROM_SIZE) || (length > EEPROM_SIZE)
		|| ((address + length) > EEPROM_SIZE))
	{
		return NV_INVALID_ADDRESS;
	}
//	eeprom_busy_wait();
	// The (void *)(int) is there because pointers on AVR are 16 bit, so
	// just doing (void *) would result in a "cast to pointer from integer
	// of different size" warning.
//	cli();
	dueFlashStorage.write(address, data, (size_t)length);
//	eeprom_write_block(data, (void *)(int)address, (size_t)length);
//	sei();
	return NV_NO_ERROR;
}

/** Read from non-volatile storage.
  * \param data A pointer to the buffer which will receive the data.
  * \param address Byte offset specifying where in non-volatile storage to
  *                start reading from.
  * \param length The number of bytes to read.
  * \return See #NonVolatileReturnEnum for return values.
  */
NonVolatileReturn nonVolatileReadNoPtr(uint8_t data, uint32_t address, uint32_t length)
{
	if ((address > EEPROM_SIZE) || (length > EEPROM_SIZE)
		|| ((address + (uint32_t)length) > EEPROM_SIZE))
	{
		return NV_INVALID_ADDRESS;
	}
	data = dueFlashStorage.read(address);

	return NV_NO_ERROR;
}

NonVolatileReturn nonVolatileRead(uint8_t *data, uint32_t address, uint32_t length)
{
	if ((address > EEPROM_SIZE) || (length > EEPROM_SIZE)
		|| ((address + (uint32_t)length) > EEPROM_SIZE))
	{
		return NV_INVALID_ADDRESS;
	}
//	uint8_t beingRead[length];
	int i;

	for(i=0; i<length; i++){
		data[i] = dueFlashStorage.read(address+i);
	}

//	data = beingRead;

//	writeEink(, false, 10, 10);

	return NV_NO_ERROR;
}

/** Ensure that all buffered writes are committed to non-volatile storage.
  * \return See #NonVolatileReturnEnum for return values.
  */
NonVolatileReturn nonVolatileFlush(void)
{
	// Nothing to do; writes are never buffered.
	return NV_NO_ERROR;
}


