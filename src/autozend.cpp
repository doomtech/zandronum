/*
** autozend.cpp
** This file contains the tails of lists stored in special data segments
**
**---------------------------------------------------------------------------
** Copyright 1998-2006 Randy Heit
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
** See autostart.cpp for an explanation of why I do things like this.
*/

#include "autosegs.h"

#if defined(_MSC_VER)

#pragma data_seg(".areg$z")
void *ARegTail = 0;

#pragma data_seg(".creg$z")
void *CRegTail = 0;

#pragma data_seg(".greg$z")
void *GRegTail = 0;

#pragma data_seg(".sreg$z")
void *SRegTail = 0;

#pragma data_seg()



#elif defined(__GNUC__)

void *ARegTail __attribute__((section(AREG_SECTION))) = 0;
void *CRegTail __attribute__((section(CREG_SECTION))) = 0;
void *GRegTail __attribute__((section(GREG_SECTION))) = 0;
void *SRegTail __attribute__((section(SREG_SECTION))) = 0;

#elif

#error Please fix autozend.cpp for your compiler

#endif
