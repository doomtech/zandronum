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

// [BB] Number of packets that are stored to recover from packet loss.
#define PACKET_BUFFER_SIZE		1024

// [RC] A security debug feature to catch malicious packets.
// #define CREATE_PACKET_LOG

//*****************************************************************************
typedef enum
{
	BUFFERTYPE_READ,
	BUFFERTYPE_WRITE,

} BUFFERTYPE_e;

enum
{
	// Server is letting master server of its existance.
	SERVER_MASTER_CHALLENGE = 5660020,

	// Server is letting master server of its existance, along with sending an IP the master server
	// should use for this server.
	SERVER_MASTER_CHALLENGE_OVERRIDE,

	// Server is sending some statistics to the master server.
	SERVER_MASTER_STATISTICS,

	// Server is sending its info to the launcher.
	SERVER_LAUNCHER_CHALLENGE,

	// Server is telling a launcher that it's ignoring it.
	SERVER_LAUNCHER_IGNORING,

	// Server is telling a launcher that his IP is banned from the server.
	SERVER_LAUNCHER_BANNED,

	// Client is trying to create a new account with the master server.
	CLIENT_MASTER_NEWACCOUNT,

	// Client is trying to log in with the master server.
	CLIENT_MASTER_LOGIN,

};

// Launcher is querying the server, or master server.
#define	LAUNCHER_SERVER_CHALLENGE	199

#define	DEFAULT_SERVER_PORT		10666
#define	DEFAULT_CLIENT_PORT		10667
#define	DEFAULT_MASTER_PORT		15300
#define	DEFAULT_BROADCAST_PORT	15101
#define	DEFAULT_STATS_PORT		15201

// This is the longest possible string we can pass over the network.
#define	MAX_NETWORK_STRING			2048

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

#ifdef CREATE_PACKET_LOG
	// [RC] Pointer to the start of the stream.
	BYTE		*pbStreamBeginning;

	// [RC] Whether or not we've logged this.
	bool		bPacketAlreadyLogged;
#endif

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

void			NETWORK_InitBuffer( NETBUFFER_s *pBuffer, ULONG ulLength, BUFFERTYPE_e BufferType );
void			NETWORK_FreeBuffer( NETBUFFER_s *pBuffer );
void			NETWORK_ClearBuffer( NETBUFFER_s *pBuffer );
LONG			NETWORK_CalcBufferSize( NETBUFFER_s *pBuffer );

int				NETWORK_ReadByte( BYTESTREAM_s *pByteStream );
void			NETWORK_WriteByte( BYTESTREAM_s *pByteStream, int Byte );

int				NETWORK_ReadShort( BYTESTREAM_s *pByteStream );
void			NETWORK_WriteShort( BYTESTREAM_s *pByteStream, int Short );

int				NETWORK_ReadLong( BYTESTREAM_s *pByteStream );
void			NETWORK_WriteLong( BYTESTREAM_s *pByteStream, int Long );

float			NETWORK_ReadFloat( BYTESTREAM_s *pByteStream );
void			NETWORK_WriteFloat( BYTESTREAM_s *pByteStream, float Float );

const char		*NETWORK_ReadString( BYTESTREAM_s *pByteStream );
void			NETWORK_WriteString( BYTESTREAM_s *pByteStream, const char *pszString );

void			NETWORK_WriteBuffer( BYTESTREAM_s *pByteStream, const void *pvBuffer, int nLength );

// Debugging function.
void			NETWORK_WriteHeader( BYTESTREAM_s *pByteStream, int Byte );

bool			NETWORK_CompareAddress( NETADDRESS_s Address1, NETADDRESS_s Address2, bool bIgnorePort );
bool			NETWORK_StringToAddress( const char *pszString, NETADDRESS_s *pAddress );
void			NETWORK_SocketAddressToNetAddress( struct sockaddr_in *s, NETADDRESS_s *a );
bool			NETWORK_StringToIP( const char *pszAddress, char *pszIP0, char *pszIP1, char *pszIP2, char *pszIP3 );

std::string GenerateCouldNotOpenFileErrorString( const char *pszFunctionHeader, const char *pszFileName, LONG lErrorCode );

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

/**
 * Class to store a list of IPs, supports wildcards.
 *
 * \author BB
 */
class IPList{
	std::vector<IPADDRESSBAN_s> _ipVector;
	std::string _filename;
	std::string _error;
public:
	bool clearAndLoadFromFile( const char *Filename );
	bool isIPInList( const char *pszIP0, const char *pszIP1, const char *pszIP2, const char *pszIP3 ) const;
	bool isIPInList( const NETADDRESS_s &Address ) const;
	ULONG doesEntryExist( const char *pszIP0, const char *pszIP1, const char *pszIP2, const char *pszIP3 ) const;
	IPADDRESSBAN_s getEntry( const ULONG ulIdx ) const;
	std::string getEntryAsString( const ULONG ulIdx ) const;
	ULONG getEntryIndex( const NETADDRESS_s &Address ) const; // [RC]
	const char *getEntryComment( const NETADDRESS_s &Address ) const; // [RC]
	void addEntry( const char *pszIP0, const char *pszIP1, const char *pszIP2, const char *pszIP3, const char *pszPlayerName, const char *pszComment, std::string &Message );
	void addEntry( const char *pszIPAddress, const char *pszPlayerName, const char *pszComment, std::string &Message );
	void removeEntry( const char *pszIP0, const char *pszIP1, const char *pszIP2, const char *pszIP3, std::string &Message );
	void removeEntry( const char *pszIPAddress, std::string &Message );

	unsigned int size() const { return static_cast<unsigned int>(_ipVector.size()); }

	void clear() { _ipVector.clear(); }

	void push_back ( IPADDRESSBAN_s &IP ) { _ipVector.push_back(IP); }

	std::vector<IPADDRESSBAN_s>& getVector() { return _ipVector; }

	const char* getErrorMessage() const { return _error.c_str(); }

private:
	bool rewriteListToFile ();
};

/**
 * Class to store a list of IP address that this server has been queried by recently.
 *
 * \author BB
 */
class QueryIPQueue {

	typedef struct
	{
		// The IP address of someone who just queried the master server.
		NETADDRESS_s	Address;

		// This is the next time we're allowed to respond to a query from this IP address.
		long			lNextAllowedTime;

	} STORED_QUERY_IP_t;

	//! This is the maximum number of IPs that we can store in our query list.
	static const int maxStoredQueryIPs	= 512;

	STORED_QUERY_IP_t _StoredQueryIPs[maxStoredQueryIPs];
	LONG _storedQueryIPHead;
	LONG _storedQueryIPTail;
public:
	QueryIPQueue ()
		: _storedQueryIPHead ( 0 ),
		  _storedQueryIPTail ( 0 )
	{
	}

	void adjustHead ( const LONG CurrentTime ) {
		while (( _storedQueryIPHead != _storedQueryIPTail ) && ( CurrentTime >= _StoredQueryIPs[_storedQueryIPHead].lNextAllowedTime ) )
		{
			_storedQueryIPHead++;
			_storedQueryIPHead = _storedQueryIPHead % maxStoredQueryIPs;
		}
	}

	bool addressInQueue ( const NETADDRESS_s AddressFrom ) const {
		// If head == tail, the 
		if ( _storedQueryIPHead != _storedQueryIPTail )
		{
			ULONG ulIdx = _storedQueryIPHead;
			while ( ulIdx != static_cast<ULONG>(_storedQueryIPTail) )
			{
				if ( NETWORK_CompareAddress( AddressFrom, _StoredQueryIPs[ulIdx].Address, true ))
					return true;

				ulIdx++;
				ulIdx = ulIdx % maxStoredQueryIPs;
			}
		}
		return false;
	}
	void addAddress ( const NETADDRESS_s AddressFrom, const LONG CurrentTime, std::ostream *errorOut = NULL ) {
		_StoredQueryIPs[_storedQueryIPTail].Address = AddressFrom;
		_StoredQueryIPs[_storedQueryIPTail].lNextAllowedTime = CurrentTime + 10;

		_storedQueryIPTail++;
		_storedQueryIPTail = _storedQueryIPTail % maxStoredQueryIPs;

		if ( (_storedQueryIPTail == _storedQueryIPHead) && errorOut )
			*errorOut << "WARNING! _storedQueryIPTail == _storedQueryIPHead\n";
	}
};

#endif	// __NETWORKSHARED_H__
