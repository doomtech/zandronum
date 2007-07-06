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
// Filename: networkshared.h
//
// Description: Contains network related code shared between
// Skulltag and the master server.
//
//-----------------------------------------------------------------------------

#ifndef __NETWORKSHARED_H__
#define __NETWORKSHARED_H__

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <iostream>

#include <ctype.h>
#include <math.h>

//*****************************************************************************
//	DEFINES

// Maximum size of the packets sent out by the server.
#define	MAX_UDP_PACKET				8192

//*****************************************************************************
//	STRUCTURES

typedef struct
{
	// Four digit IP address.
	BYTE		abIP[4];

	// The IP address's port extension.
	USHORT		usPort;

	// What's this for?
	USHORT		usPad;

} NETADDRESS_s;

//*****************************************************************************
typedef struct
{
	// This is the data in our packet.
	BYTE		*pbData;

	// The maximum amount of data this packet can hold.
	ULONG		ulMaxSize;

	// How much data is currently in this packet?
	ULONG		ulCurrentSize;

	// TEMPORARY
	// How far along are we in reading this buffer?
	ULONG		ulCurrentPosition;

} NETBUFFER_s;

//*****************************************************************************
typedef struct
{
	// The IP address that is banned in char form. Can be a number or a wildcard.
	char		szIP[4][4];

	// Comment regarding the banned address.
	char		szComment[128];

} IPADDRESSBAN_s;

//*****************************************************************************
//	PROTOTYPES

bool	NETWORK_StringToAddress( char *pszString, NETADDRESS_s *pAddress );
void	NETWORK_SocketAddressToNetAddress( struct sockaddr_in *s, NETADDRESS_s *a );
bool	NETWORK_StringToIP( char *pszAddress, char *pszIP0, char *pszIP1, char *pszIP2, char *pszIP3 );

class IPFileParser{
	const unsigned int _listLength;
	char _errorMessage[1024];
public:
	IPFileParser ( const int IPListLength )
		: _listLength(IPListLength)
	{
		_errorMessage[0] = '\0';
	}
		
	const char* getErrorMessage()
	{
		return _errorMessage;
	}
	bool parseIPList( const char* FileName, IPADDRESSBAN_s* IPArray ){
		FILE			*pFile;
		unsigned long	ulIdx;

		for ( ulIdx = 0; ulIdx < _listLength; ulIdx++ )
		{
			sprintf( IPArray[ulIdx].szIP[0], "0" );
			sprintf( IPArray[ulIdx].szIP[1], "0" );
			sprintf( IPArray[ulIdx].szIP[2], "0" );
			sprintf( IPArray[ulIdx].szIP[3], "0" );

			IPArray[ulIdx].szComment[0] = 0;
		}

		char curChar = 0;
		ULONG curIPIdx = 0;
		if (( pFile = fopen( FileName, "r" )) != NULL )
		{
			while ( true )
			{
				bool parsingDone = !parseNextLine( pFile, IPArray[curIPIdx], curIPIdx );

				if ( _errorMessage[0] != '\0' )
				{
					fclose( pFile );
					return false;
				}

				if ( parsingDone == true )
					break;
			}
		}
		else
		{
			sprintf( _errorMessage, "WARNING! Could not open %s!\n", FileName );
			return false;
		}

		fclose( pFile );
		return true;
	}
private:
	char skipWhitespace( FILE *pFile )
	{
		char curChar = fgetc( pFile );
		while (( curChar == ' ' ) && ( curChar != -1 ))
			curChar = fgetc( pFile );

		return ( curChar );
	}
	char skipComment( FILE *pFile )
	{
		char curChar = fgetc( pFile );
		while (( curChar != '\r' ) && ( curChar != '\n' ) && ( curChar != -1 ))
			curChar = fgetc( pFile );

		return ( curChar );
	}
	bool parseNextLine( FILE *pFile, IPADDRESSBAN_s &IP, ULONG &BanIdx )
	{
		NETADDRESS_s	IPAddress;
		char			szIP[257];
		int				lPosition;

		lPosition = 0;
		szIP[0] = 0;

		char curChar = fgetc( pFile );

		// Skip whitespace.
		if ( curChar == ' ' )
		{
			curChar = skipWhitespace( pFile );

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
					if ( NETWORK_StringToIP( szIP, IP.szIP[0], IP.szIP[1], IP.szIP[2], IP.szIP[3] ))
					{
						if ( BanIdx == _listLength )
						{
							sprintf( _errorMessage, "parseNextLine: WARNING! Maximum number of IPs (%d) exceeded!\n", _listLength );
							return ( false );
						}

						BanIdx++;
						return ( true );
					}
					else if ( NETWORK_StringToAddress( szIP, &IPAddress ))
					{
						if ( BanIdx == _listLength )
						{
							sprintf( _errorMessage, "parseNextLine: WARNING! Maximum number of IPs (%d) exceeded!\n", _listLength );
							return ( false );
						}

						_itoa( IPAddress.abIP[0], IP.szIP[0], 10 );
						_itoa( IPAddress.abIP[1], IP.szIP[1], 10 );
						_itoa( IPAddress.abIP[2], IP.szIP[2], 10 );
						_itoa( IPAddress.abIP[3], IP.szIP[3], 10 );
						BanIdx++;
						return ( true );
					}
					else
					{
						IP.szIP[0][0] = 0;
						IP.szIP[1][0] = 0;
						IP.szIP[2][0] = 0;
						IP.szIP[3][0] = 0;
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
					skipComment( pFile );
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
};

#endif	// __NETWORKSHARED_H__
