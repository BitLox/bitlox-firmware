/*
 * GB2312.c
 *
 *  Created on: Jan 6, 2016
 *      Author: thesquid
 */

#include "GB2312.h"

//unsigned int getGB(unsigned int unicodePoint)
//{
//	int i;
//	do
//	 i++;
//	while (mini_unicode_gb2312[0][i] != unicodePoint);
//	return mini_unicode_gb2312[1][i];
//}

unsigned int getGB(unsigned int unicodePoint)
{
	int i;
	do
	 i++;
	while (unicode_gb2312[0][i] != unicodePoint);
	return unicode_gb2312[1][i];
}
