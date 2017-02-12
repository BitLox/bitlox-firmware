/*
  ePaperDfs.h
  2013 Copyright (c) Seeed Technology Inc.  All right reserved.

  Modified by Loovee
  www.seeedstudio.com
  2013-7-2

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef __DUE_EPAPERDFS_H__
#define __DUE_EPAPERDFS_H__

// pin define

typedef enum {
	DIRNORMAL,
	DIRLEFT,
	DIRRIGHT,
    DIRDOWN
} EPD_DIR;

#if defined(__SAM3X8E__)

#define SPI_CLOCK_DIVIDER 21
#define Pin_TEMPERATURE   A0
#define Pin_PANEL_ON      2
#define Pin_BORDER        3
#define Pin_DISCHARGE     8
#define Pin_PWM           5
#define Pin_RESET         6
#define Pin_BUSY          7

#define Pin_EPD_CS        10

#define Pin_SD_CS         4

#define Pin_OE123         A1
#define Pin_STV_IN        A3
#endif

#if defined(__SAM3A8C__)

#define SPI_CLOCK_DIVIDER 21
#define Pin_TEMPERATURE   A7
#define Pin_PANEL_ON      27
#define Pin_BORDER        26
#define Pin_DISCHARGE     28
#define Pin_PWM           2
#define Pin_RESET         14
#define Pin_BUSY          15

#define Pin_EPD_CS        40

#define Pin_SD_CS         9 //4

#define Pin_OE123         A1
#define Pin_STV_IN        A3
#endif


// spi cs

#define EPD_SELECT()        digitalWrite(Pin_EPD_CS, LOW)
#define EPD_UNSELECT()      digitalWrite(Pin_EPD_CS, HIGH)
#define SD_SELECT()         digitalWrite(Pin_SD_CS, LOW) 
#define SD_UNSELECT()       digitalWrite(Pin_SD_CS, HIGH)
#define FONT_SELECT()       digitalWrite(Pin_Font_CS, LOW) 
#define FONT_UNSELECT()     digitalWrite(Pin_Font_CS, HIGH)


#endif

/*********************************************************************************************************
  END FILE
*********************************************************************************************************/