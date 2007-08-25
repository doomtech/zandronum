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
#include <vector>

#include <ctype.h>
#include <math.h>

//*****************************************************************************
//	DEFINES

// Maximum size of the packets sent out by the server.
#define	MAX_UDP_PACKET				8192

//*****************************************************************************
typedef enum
{
	BUFFERTYPE_READ,
	BUFFERTYPE_WRITE,

} BUFFERTYPE_e;

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
	// Pointer to our stream of data.
	BYTE		*pbStream;

	// Pointer to the end of the stream. When pbStream > pbStreamEnd, the
	// entire stream has been read.
	BYTE		*pbStreamEnd;

} BYTESTREAM_s;

//*****************************************************************************
typedef struct
{
	// This is the data in our packet.
	BYTE			*pbData;

	// The maximum amount of data this packet can hold.
	ULONG			ulMaxSize;

	// How much data is currently in this packet?
	ULONG			ulCurrentSize;

	// Byte stream for this buffer for managing our current position and where
	// the end of the stream is.
	BYTESTREAM_s	ByteStream;

	// Is this a buffer that we write to, or read from?
	BUFFERTYPE_e	BufferType;

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

/**
 * Class to read IP file lists from files, supports wildcards.
 *
 * Based on the ban file parsing C-code written by BC.
 *
 * \author BB
 */
class IPFileParser{
	const unsigned int _listLength;
	ULONG _numberOfEntries;
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

	ULONG getNumberOfEntries ()
	{
		return _numberOfEntries;
	}

	bool parseIPList( const char* FileName, std::vector<IPADDRESSBAN_s> &IPArray );
private:
	char skipWhitespace( FILE *pFile );
	char skipComment( FILE *pFile );
	void readReason( FILE *pFile, char *Reason, const int MaxReasonLength );
	bool parseNextLine( FILE *pFile, IPADDRESSBAN_s &IP, ULONG &BanIdx );
};

#endif	// __NETWORKSHARED_H__
