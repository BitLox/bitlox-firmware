/*
 * This file is part of the TREZOR project.
 *
 * Copyright (C) 2014 Pavol Rusnak <stick@satoshilabs.com>
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library.  If not, see <http://www.gnu.org/licenses/>.
 *
 * @Thes Quid: modified for Bitlox
 *
 */


//#if UINT32_MAX == UINTPTR_MAX
//#define STACK_CHK_GUARD 0xe2dee396
//#else
//#define STACK_CHK_GUARD 0x595e9fbd94fda766
//#endif
//
//uintptr_t __stack_chk_guard = STACK_CHK_GUARD;
//
//__attribute__((noreturn))
//void __stack_chk_fail(void)
//{
//	for (;;) {} // loop forever
//}


#include "Arduino.h"

#include "ssp.h"
#include "arm/eink.h"
#include "hwinterface.h"
#include "common.h"

void *__stack_chk_guard = 0;

void __stack_chk_guard_setup(void)
{
	unsigned char * p;
	p = (unsigned char *) &__stack_chk_guard;
//	hardwareRandom32Bytes(p);
	p[0] = 0;
	p[1] = 0;
	p[2] = '\n';
	p[3] = 0xFF; // hardwareRandom32Bytes & 0xFF;
//	p[3] =  & 0xFF;
}

void __attribute__((noreturn)) __stack_chk_fail(void)
{
	writeEinkDisplay("Stack smashing", false, 5, 5, "detected.",false,5,25, "Reset device",false,45,0, "",false,0,0, "",false,0,0);
	for (;;) {} // loop forever
}
