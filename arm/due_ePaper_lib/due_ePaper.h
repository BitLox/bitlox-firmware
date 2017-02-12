/*
  ePaper.h
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

#ifndef __DUE_EPAPER_H__
#define __DUE_EPAPER_H__

#include <SD.h>
#include <SPI.h>

#include "due_EPD.h"
#include "due_sd_epaper.h"
#include "due_ePaperDfs.h"

#define EP_DEBUG            1

#if EP_DEBUG
#define print_ep(X)         Serial.print(X)
#define println_ep(X)       Serial.println(X)
#else
#define print_ep(X)
#define println_ep(X)
#endif

#if defined (__GNUC__) && defined (__AVR__)
#include <avr/pgmspace.h>
#else
#include <stdint.h>
#define PROGMEM /* empty */
#define pgm_read_byte(x) (*(x))
#define pgm_read_word(x) (*(x))
#define pgm_read_float(x) (*(x))
#define __LPM(x) (*(x))

#define strtok_P(a,b) strtok((a),(b))
#define memcmp_P(dest, src, num) memcmp((dest), (src), (num)) 
#define memcpy_P(a, b, c) memcpy((a), (b), (c))
#define strcasecmp_P(a,b,num) strcasecmp((a),(b),(num)) 
#define strcat_P(a,b) strcat((a),(b))
#define strcmp_P(a,b) strcmp((a),(b))
#define strcpy_P(a,b) strcpy((a),(b))
#define strlcat_P(a,b,c) strlcat((a),(b),(c))
#define strlcpy_P(a,b,c) strlcpy((a),(b),(c))
#define strlen_P(a) strlen((a))
#define strncasecmp_P(a,b,num) strncasecmp((a),(b),(num)) 
#define strncat_P(a,b,c) strncat((a),(b),(c))
#define strncmp_P(a,b,c) strncmp((a),(b),(c))
#define strncpy_P(a,b,c) strncpy((a),(b),(c))
#define strnlen_P(a,b) strnlen((a),(b))
#define strstr_P(a,b) strstr((a),(b))

#define strcasestr_P(a,b) strcasestr((a),(b))

#endif


class ePaper
{

private:

    int getTemperature();                   // get temperature
    unsigned char tMatrix[32];
    
    int SIZE_LEN;
    int SIZE_WIDTH;
    
    int DISP_LEN;
    int DISP_WIDTH;
    
    EPD_DIR direction;
    
public:

    EPD_size size;
    
    void begin(EPD_size sz);
    
    void setDirection(EPD_DIR dir);
    
    void start();
    
    void end();
    
    void init_io();
    
    unsigned char display();                // refresh 
    
    void image_flash(PROGMEM const unsigned char *image)           // read image from flash
    {
        start();
        EPD.image(image);
        end();
    } 
 
    void clear()                             // clear display
    {
        start();
        EPD.clear();
        end();
    } 
    
    void clear_sd();                         // clear sd card 

    

   
    void spi_detach();
#if defined(__AVR_ATmega1280__) || defined(__AVR_ATmega2560__)|| defined(__SAM3X8E__)|| defined(__SAM3A8C__)
    void image_sram(unsigned char *image)
    {
        start();
        EPD.image_sram(image);
        end();
    }
#endif
    
    inline void drawPixel(int x, int y, unsigned char color)
    {
		eSD.putPixel(x, y, color);
        
	}
    
    int drawChar(char c, int x, int y);
    int drawString(char *string, int poX, int poY);
    int drawNumber(long long_num,int poX, int poY);
    int drawFloat(float floatNumber,int decimal,int poX, int poY);
    
    int drawUnicode(unsigned int uniCode, int x, int y);
    int drawUnicode(unsigned char *matrix, int x, int y);
    
    int drawUnicodeString(unsigned int *uniCode, int len, int x, int y);

    int drawCharBig(char c, int x, int y);
    int drawStringBig(char *string, int poX, int poY);
    int drawNumberBig(long long_num,int poX, int poY);
    //int drawFloatBig(float floatNumber,int decimal,int poX, int poY);
    
    int drawUnicodeBig(unsigned int uniCode, int x, int y);
    int drawUnicodeBig(unsigned char *matrix, int x, int y);

    int drawUnicodeStringBig(unsigned int *uniCode, int len, int x, int y);

    void drawLine(int x0, int y0, int x1, int y1);
    void drawCircle(int poX, int poY, int r);
    void drawHorizontalLine( int poX, int poY, int len);
    void drawVerticalLine( int poX, int poY, int len);
    void drawRectangle(int poX, int poY, int len, int width);
    void fillRectangle(int poX, int poY, int len, int width);
    void fillCircle(int poX, int poY, int r);
    void drawTraingle( int poX1, int poY1, int poX2, int poY2, int poX3, int poY3);
};

extern ePaper EPAPER;

#endif

/*********************************************************************************************************
  END FILE
*********************************************************************************************************/
