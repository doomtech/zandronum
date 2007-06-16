//-----------------------------------------------------------------------------
//
// Skulltag Source
// Copyright (C) 2007 Brad Carney, Benjamin Berkels
// Copyright (C) 2007-2012 Skulltag Development Team
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
//    this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
// 3. Neither the name of the Skulltag Development Team nor the names of its
//    contributors may be used to endorse or promote products derived from this
//    software without specific prior written permission.
// 4. Redistributions in any form must be accompanied by information on how to
//    obtain complete source code for the software and any accompanying
//    software that uses the software. The source code must either be included
//    in the distribution or be available for no more than the cost of
//    distribution plus a nominal fee, and must be freely redistributable
//    under reasonable conditions. For an executable file, complete source
//    code means the source code for all modules it contains. It does not
//    include source code for modules or files that typically accompany the
//    major components of the operating system on which the executable file
//    runs.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
//
//
// Filename: sv_banfuncs.h
//
// Description: Contains routines related banning shared between
// Skulltag and the master server.
//
//-----------------------------------------------------------------------------

#ifndef __SV_BANFUNCS_H__
#define __SV_BANFUNCS_H__

#define	MAX_SERVER_BANS			256

//*****************************************************************************
//
bool SERVERBAN_StringToBan( char *pszAddress, char *pszIP0, char *pszIP1, char *pszIP2, char *pszIP3 )
{
	char	szCopy[16];
	char	*pszCopy;
	char	szTemp[4];
	char	*pszTemp;
	ULONG	ulIdx;
	ULONG	ulNumPeriods;

	// Too long.
	if ( strlen( pszAddress ) > 15 )
		return ( false );

	// First, get rid of anything after the colon (if it exists).
	strcpy( szCopy, pszAddress );
	for ( pszCopy = szCopy; *pszCopy; pszCopy++ )
	{
		if ( *pszCopy == ':' )
		{
			*pszCopy = 0;
			break;
		}
	}

	// Next, make sure there's at least 3 periods.
	ulNumPeriods = 0;
	for ( pszCopy = szCopy; *pszCopy; pszCopy++ )
	{
		if ( *pszCopy == '.' )
			ulNumPeriods++;
	}

	// If there weren't 3 periods, then it's not a valid ban string.
	if ( ulNumPeriods != 3 )
		return ( false );

	ulIdx = 0;
	pszTemp = szTemp;
	*pszTemp = 0;
	for ( pszCopy = szCopy; *pszCopy; pszCopy++ )
	{
		if ( *pszCopy == '.' )
		{
			// Shouldn't happen.
			if ( ulIdx > 3 )
				return ( false );

			switch ( ulIdx )
			{
			case 0:

				strcpy( pszIP0, szTemp );
				break;
			case 1:

				strcpy( pszIP1, szTemp );
				break;
			case 2:

				strcpy( pszIP2, szTemp );
				break;
			case 3:

				strcpy( pszIP3, szTemp );
				break;
			}
			ulIdx++;
//			strcpy( szBan[ulIdx++], szTemp );
			pszTemp = szTemp;
		}
		else
		{
			*pszTemp++ = *pszCopy;
			*pszTemp = 0;
			if ( strlen( szTemp ) > 3 )
				return ( false );
		}
	}

	strcpy( pszIP3, szTemp );

	// Finally, make sure each entry of our string is valid.
	if ((( atoi( pszIP0 ) < 0 ) || ( atoi( pszIP0 ) > 255 )) && ( stricmp( "*", pszIP0 ) != 0 ))
		return ( false );
	if ((( atoi( pszIP1 ) < 0 ) || ( atoi( pszIP1 ) > 255 )) && ( stricmp( "*", pszIP1 ) != 0 ))
		return ( false );
	if ((( atoi( pszIP2 ) < 0 ) || ( atoi( pszIP2 ) > 255 )) && ( stricmp( "*", pszIP2 ) != 0 ))
		return ( false );
	if ((( atoi( pszIP3 ) < 0 ) || ( atoi( pszIP3 ) > 255 )) && ( stricmp( "*", pszIP3 ) != 0 ))
		return ( false );

    return ( true );
}

//*****************************************************************************
//*****************************************************************************
//
static char serverban_SkipWhitespace( FILE *pFile )
{
	char curChar = fgetc( pFile );
	while (( curChar == ' ' ) && ( curChar != -1 ))
		curChar = fgetc( pFile );

	return ( curChar );
}

//*****************************************************************************
//
static char serverban_SkipComment( FILE *pFile )
{
	char curChar = fgetc( pFile );
	while (( curChar != '\r' ) && ( curChar != '\n' ) && ( curChar != -1 ))
		curChar = fgetc( pFile );

	return ( curChar );
}

//*****************************************************************************
//
static bool serverban_ParseNextLine( FILE *pFile, BAN_t &Ban, LONG &BanIdx, char *ErrorMessage )
{
	netadr_t	BanAddress;
	char		szIP[256];
	char		lPosition;

	lPosition = 0;
	szIP[0] = 0;

	char curChar = fgetc( pFile );

	// Skip whitespace.
	if ( curChar == ' ' )
	{
		curChar = serverban_SkipWhitespace( pFile );

		if ( feof( pFile ))
		{
			fclose( pFile );
			return ( false );
		}
	}

	while ( 1 )
	{
		if ( curChar == '\r' || curChar == '\n' || curChar == ':' || curChar == '/' || curChar == -1 )
		{
			if ( lPosition > 0 )
			{
				if ( SERVERBAN_StringToBan( szIP, Ban.szBannedIP[0], Ban.szBannedIP[1], Ban.szBannedIP[2], Ban.szBannedIP[3] ))
				{
					if ( BanIdx == MAX_SERVER_BANS )
					{
						sprintf( ErrorMessage, "serverban_ParseNextLine: WARNING! Maximum number of bans (%d) exceeded!\n", MAX_SERVER_BANS );
						return ( false );
					}

					BanIdx++;
					return ( true );
				}
				else if ( NETWORK_StringToAddress( szIP, &BanAddress ))
				{
					if ( BanIdx == MAX_SERVER_BANS )
					{
						sprintf( ErrorMessage, "serverban_ParseNextLine: WARNING! Maximum number of bans (%d) exceeded!\n", MAX_SERVER_BANS );
						return ( false );
					}

					itoa( BanAddress.ip[0], Ban.szBannedIP[0], 10 );
					itoa( BanAddress.ip[1], Ban.szBannedIP[1], 10 );
					itoa( BanAddress.ip[2], Ban.szBannedIP[2], 10 );
					itoa( BanAddress.ip[3], Ban.szBannedIP[3], 10 );
					BanIdx++;
					return ( true );
				}
				else
				{
					Ban.szBannedIP[0][0] = 0;
					Ban.szBannedIP[1][0] = 0;
					Ban.szBannedIP[2][0] = 0;
					Ban.szBannedIP[3][0] = 0;
				}
			}

			if ( feof( pFile ))
			{
				fclose( pFile );
				return ( false );
			}
			// If we've hit a comment, skip until the end of the line (or the end of the file) and get out.
			else if ( curChar == ':' || curChar == '/' )
			{
				serverban_SkipComment( pFile );
				return ( true );
			}
			else
				return ( true );
		}

		szIP[lPosition++] = curChar;
		szIP[lPosition] = 0;

		if ( lPosition == 256 )
		{
			fclose( pFile );
			return ( false );
		}

		curChar = fgetc( pFile );
	}
}



#endif	// __SV_BANFUNCS_H__