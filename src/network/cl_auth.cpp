//-----------------------------------------------------------------------------
//
// Zandronum Source
// Copyright (C) 2014 Benjamin Berkels
// Copyright (C) 2014 Zandronum Development Team
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
// 3. Neither the name of the Zandronum Development Team nor the names of its
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
// Filename: cl_auth.cpp
//
//-----------------------------------------------------------------------------

#include "c_dispatch.h"
#include "cl_main.h"
#include "network.h"
#include "cl_auth.h"
#include "srp.h"

//*****************************************************************************
//	VARIABLES

static struct SRPUser* g_usr = NULL;

//*****************************************************************************
//	PROTOTYPES

//*****************************************************************************
//	FUNCTIONS

//*****************************************************************************
//
void client_RequestLogin ( const char* Username, const char* Password )
{
    const unsigned char * bytesA = 0;
    int lenA = 0;
    const char * authUsername = NULL; 

	if ( g_usr != NULL )
		srp_user_delete( g_usr );
	g_usr = srp_user_new( SRP_SHA256, SRP_NG_2048, Username, reinterpret_cast<const unsigned char*> ( Password ), strlen ( Password ), NULL, NULL, 1 );
	srp_user_start_authentication( g_usr, &authUsername, &bytesA, &lenA );
	if ( strcmp( authUsername, Username ) != 0 )
		Printf ( "Username problem when calling srp_user_start_authentication.\n" );

	NETWORK_WriteByte( &CLIENT_GetLocalBuffer( )->ByteStream, CLC_SRP_USER_START_AUTHENTICATION );
	NETWORK_WriteString( &CLIENT_GetLocalBuffer( )->ByteStream, Username );
	NETWORK_WriteLong( &CLIENT_GetLocalBuffer( )->ByteStream, lenA );
	for ( int i = 0; i < lenA; ++i )
		NETWORK_WriteByte( &CLIENT_GetLocalBuffer( )->ByteStream, bytesA[i] );
}

//*****************************************************************************
//
void client_SRPUserProcessChallenge ( TArray<unsigned char> &Salt, TArray<unsigned char> &B )
{
    const unsigned char * bytesM = 0;
    int lenM = 0;

	srp_user_process_challenge( g_usr, &(Salt[0]), Salt.Size(), &(B[0]), B.Size(), &bytesM, &lenM );
	if ( ( bytesM == NULL ) || ( lenM == 0 ) )
		Printf ( "User SRP-6a safety check violation!\n" );
	else
	{
		NETWORK_WriteByte( &CLIENT_GetLocalBuffer( )->ByteStream, CLC_SRP_USER_PROCESS_CHALLENGE );
		NETWORK_WriteLong( &CLIENT_GetLocalBuffer( )->ByteStream, lenM );
		for ( int i = 0; i < lenM; ++i )
			NETWORK_WriteByte( &CLIENT_GetLocalBuffer( )->ByteStream, bytesM[i] );
	}
}

//*****************************************************************************
//
void CLIENT_ProcessSRPServerCommand( LONG lCommand, BYTESTREAM_s *pByteStream )
{
	switch ( lCommand )
	{
	case SVC2_SRP_USER_PROCESS_CHALLENGE:
		{
			const int lenSalt = NETWORK_ReadByte( pByteStream );
			TArray<unsigned char> salt;
			salt.Resize ( lenSalt ); 
			for ( int i = 0; i < lenSalt; ++i )
				salt[i] = NETWORK_ReadByte( pByteStream );
			const int lenB = NETWORK_ReadLong( pByteStream );
			TArray<unsigned char> bytesB;
			bytesB.Resize ( lenB ); 
			for ( int i = 0; i < lenB; ++i )
				bytesB[i] = NETWORK_ReadByte( pByteStream );

			client_SRPUserProcessChallenge ( salt, bytesB );
		}
		break;
	case SVC2_SRP_USER_VERIFY_SESSION:
		{
			const int lenHAMK = NETWORK_ReadLong( pByteStream );
			TArray<unsigned char> bytesHAMK;
			// [BB] We may need to allocate more then lenHAMK!
			bytesHAMK.Resize ( srp_user_get_session_key_length( g_usr ) ); 
			for ( int i = 0; i < lenHAMK; ++i )
				bytesHAMK[i] = NETWORK_ReadByte( pByteStream );

			srp_user_verify_session( g_usr, &(bytesHAMK[0]) );

			if ( !srp_user_is_authenticated ( g_usr ) )
				Printf( "Server authentication failed!\n" );
			else
				Printf ( "Login successful\n" );
		}
		break;

	default:
		Printf ( "Error: Received unknown SRP command '%d' from client %d.\n", lCommand, SERVER_GetCurrentClient() );
		break;
	}
}

//*****************************************************************************
//	CONSOLE COMMANDS

CCMD( login )
{
	if ( NETWORK_GetState( ) != NETSTATE_CLIENT )
		return;

	if ( argv.argc() == 3 )
		client_RequestLogin ( argv[1], argv[2] );
	else
		Printf ("Usage: login <username> <password>\n");
}
