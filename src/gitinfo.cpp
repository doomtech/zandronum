/*
** gitinfo.cpp
** Returns strings from gitinfo.h.
**
**---------------------------------------------------------------------------
** Copyright 2013 Randy Heit
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
** This file is just here so that when gitinfo.h changes, only one source
** file needs to be recompiled.
*/

#include "gitinfo.h"
#include "version.h"
// [BB]
#include "zstring.h"

const char *GetGitDescription()
{
	// [BB]
	return HG_REVISION_HASH_STRING; // GIT_DESCRIPTION;
}

const char *GetGitHash()
{
	// [BB]
	return HG_REVISION_HASH_STRING; // GIT_HASH;
}

const char *GetGitTime()
{
	// [BB]
	return SVN_REVISION_STRING; // GIT_TIME;
}

const char *GetVersionString()
{
	// [BB]
	//if (GetGitDescription()[0] == '\0')
	{
		return VERSIONSTR;
	}
	/*
	else
	{
		return GIT_DESCRIPTION;
	}
	*/
}

// [BB]
const char *GetVersionStringRev()
{
	//FString s = DOTVERSIONSTR "-r" SVN_REVISION_STRING;
	//return s.GetChars();
	return DOTVERSIONSTR_REV;
}

// [BB]
unsigned int GetRevisionNumber()
{
	return SVN_REVISION_NUMBER;
}
