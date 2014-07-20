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
// Filename: sv_auth.cpp
//
//-----------------------------------------------------------------------------

#include "c_dispatch.h"
#include "i_system.h"
#include "network.h"
#include "cl_auth.h"
#include "sv_auth.h"
#include "sv_commands.h"
#include "srp.h"

#define	DEFAULT_AUTH_SERVER_PORT 15301

#define AUTH_PROTOCOL_VERSION 1

//*****************************************************************************
//	DEFINES

#define EMULATE_AUTH_SERVER 0

#if EMULATE_AUTH_SERVER
#include "srp.h"
#include <openssl/sha.h>
// [BB] This only works if only one client tries to log in simultaneously.
struct SRPVerifier * g_ver = NULL;
#endif

//*****************************************************************************
enum
{
	SERVER_AUTH_NEGOTIATE		= 0xD003CA01,
	SERVER_AUTH_SRP_STEP_ONE	= 0xD003CA02,
	SERVER_AUTH_SRP_STEP_THREE	= 0xD003CA03,
};

//*****************************************************************************
enum
{
	AUTH_SERVER_NEGOTIATE		= 0xD003CA10,
	AUTH_SERVER_SRP_STEP_TWO	= 0xD003CA20,
	AUTH_SERVER_SRP_STEP_FOUR	= 0xD003CA30,
	AUTH_SERVER_USER_ERROR		= 0xD003CAFF,
	AUTH_SERVER_SESSION_ERROR	= 0xD003CAEE,
};

//*****************************************************************************
enum
{
	USER_TRY_LATER			= 0,
	USER_NO_EXIST			= 1,
	USER_OUTDATED_PROTOCOL	= 2,
	USER_WILL_NOT_AUTH		= 3,
};


//*****************************************************************************
enum
{
	SESSION_TRY_LATER		= 0,
	SESSION_NO_EXIST		= 1,
	SESSION_VERIFIER_UNSAFE = 2,
	SESSION_AUTH_FAILED		= 3,
};

//*****************************************************************************
//	VARIABLES

static	NETADDRESS_s		g_AuthServerAddress;

static	NETBUFFER_s			g_AuthServerBuffer;

// [BB] Hostname of the authentication server.
CVAR( String, authhostname, "localhost", CVAR_ARCHIVE|CVAR_GLOBALCONFIG )

//*****************************************************************************
//	PROTOTYPES

//*****************************************************************************
//	FUNCTIONS

void NETWORK_AUTH_Construct( void )
{
	NETWORK_InitBuffer( &g_AuthServerBuffer, MAX_UDP_PACKET, BUFFERTYPE_WRITE );
	NETWORK_ClearBuffer( &g_AuthServerBuffer );

	g_AuthServerAddress = NETWORK_AUTH_GetServerAddress();

	atterm( NETWORK_AUTH_Destruct );
}

//*****************************************************************************
//
void NETWORK_AUTH_Destruct( void )
{
	NETWORK_FreeBuffer( &g_AuthServerBuffer );
}

//*****************************************************************************
//
NETADDRESS_s NETWORK_AUTH_GetServerAddress( void )
{
	NETADDRESS_s authServerAddress;
	// [BB] Initialize the port with 0. NETWORK_StringToAddress will change
	// it in case authhostname contains a port.
	authServerAddress.usPort = 0;

	UCVarValue Val = authhostname.GetGenericRep( CVAR_String );

	if ( NETWORK_StringToAddress( Val.String, &authServerAddress ) == false )
		Printf ( "Warning: Can't find authhostname %s!\n", Val.String );

	// [BB] If authhostname doesn't include the port, use the default port.
	if ( authServerAddress.usPort == 0 )
		NETWORK_SetAddressPort( authServerAddress, DEFAULT_AUTH_SERVER_PORT );

	return ( authServerAddress );
}

//*****************************************************************************
//
NETADDRESS_s NETWORK_AUTH_GetCachedServerAddress( void )
{
	return g_AuthServerAddress;
}

//*****************************************************************************
//
int SERVER_FindClientWithUsername ( const char *Username )
{
	for ( int i = 0; i < MAXPLAYERS; ++i )
	{
		if ( SERVER_IsValidClient ( i ) == false )
			continue;

		if ( SERVER_GetClient(i)->username.Compare ( Username ) == 0 )
			return i;
	}
	return MAXPLAYERS;
}

//*****************************************************************************
//
int SERVER_FindClientWithSessionID ( const int SessionID )
{
	for ( int i = 0; i < MAXPLAYERS; ++i )
	{
		if ( SERVER_IsValidClient ( i ) == false )
			continue;

		if ( SERVER_GetClient(i)->SRPsessionID == SessionID )
			return i;
	}
	return MAXPLAYERS;
}

//*****************************************************************************
//
void SERVER_InitClientSRPData ( const ULONG ulClient )
{
	if ( ulClient >= MAXPLAYERS )
		return;

	SERVER_GetClient(ulClient)->username = "";
	SERVER_GetClient(ulClient)->SRPsessionID = -1;
	SERVER_GetClient(ulClient)->loggedIn = false;
	SERVER_GetClient(ulClient)->bytesA.Clear();
	SERVER_GetClient(ulClient)->bytesB.Clear();
	SERVER_GetClient(ulClient)->bytesM.Clear();
	SERVER_GetClient(ulClient)->bytesHAMK.Clear();
	SERVER_GetClient(ulClient)->salt.Clear();
}

//*****************************************************************************
//
void SERVER_AUTH_Negotiate ( const char *Username )
{
	NETWORK_ClearBuffer( &g_AuthServerBuffer );
	NETWORK_WriteLong( &g_AuthServerBuffer.ByteStream, SERVER_AUTH_NEGOTIATE );
	NETWORK_WriteByte( &g_AuthServerBuffer.ByteStream, AUTH_PROTOCOL_VERSION );
	NETWORK_WriteString( &g_AuthServerBuffer.ByteStream, Username );
	NETWORK_LaunchPacket( &g_AuthServerBuffer, g_AuthServerAddress );
}

//*****************************************************************************
//
void SERVER_AUTH_SRPMessage ( const int MagicNumber, const int SessionID, const TArray<unsigned char> &Bytes )
{
	NETWORK_ClearBuffer( &g_AuthServerBuffer );
	NETWORK_WriteLong( &g_AuthServerBuffer.ByteStream, MagicNumber );
	NETWORK_WriteLong( &g_AuthServerBuffer.ByteStream, SessionID );
	NETWORK_WriteShort( &g_AuthServerBuffer.ByteStream, Bytes.Size() );
	for ( unsigned int i = 0; i < Bytes.Size(); ++i )
		NETWORK_WriteByte( &g_AuthServerBuffer.ByteStream, Bytes[i] );
	NETWORK_LaunchPacket( &g_AuthServerBuffer, g_AuthServerAddress );
}

//*****************************************************************************
//
void SERVER_AUTH_ParsePacket( BYTESTREAM_s *pByteStream )
{
	while ( 1 )	 
	{  
		const int commandNum = NETWORK_ReadLong( pByteStream );

		if ( commandNum == -1 )
			break;

		switch ( commandNum )
		{
		case AUTH_SERVER_NEGOTIATE:
			{
				const int protovolVersion = NETWORK_ReadByte( pByteStream );
				const int sessionID = NETWORK_ReadLong( pByteStream );
				const int lenSalt = NETWORK_ReadByte( pByteStream );
				TArray<unsigned char> bytesSalt;
				bytesSalt.Resize( lenSalt );
				for ( int i = 0; i < lenSalt; ++i )
					bytesSalt[i] = NETWORK_ReadByte( pByteStream );
				const FString username = NETWORK_ReadString( pByteStream );
				const int clientID = SERVER_FindClientWithUsername ( username.GetChars() );
				if ( clientID < MAXPLAYERS )
				{
					SERVER_GetClient(clientID)->SRPsessionID = sessionID;
					SERVER_GetClient(clientID)->salt = bytesSalt;

					SERVER_AUTH_SRPMessage ( SERVER_AUTH_SRP_STEP_ONE, sessionID, SERVER_GetClient(clientID)->bytesA );
				}
				else
					Printf ( "AUTH_SERVER_NEGOTIATE: Can't find client with username '%s'.\n", username.GetChars() );
			}
			break;
		case AUTH_SERVER_SRP_STEP_TWO:
			{
				const int sessionID = NETWORK_ReadLong( pByteStream );
				const int lenB = NETWORK_ReadShort( pByteStream );
				TArray<unsigned char> bytesB;
				bytesB.Resize( lenB );
				for ( int i = 0; i < lenB; ++i )
					bytesB[i] = NETWORK_ReadByte( pByteStream );
				const int clientID = SERVER_FindClientWithSessionID ( sessionID );
				if ( clientID < MAXPLAYERS )
				{
					SERVER_GetClient(clientID)->bytesB = bytesB;
					SERVERCOMMANDS_SRPUserProcessChallenge ( clientID );
				}
			}
			break;
		case AUTH_SERVER_SRP_STEP_FOUR:
			{
				const int sessionID = NETWORK_ReadLong( pByteStream );
				const int lenHAMK = NETWORK_ReadShort( pByteStream );
				TArray<unsigned char> bytesHAMK;
				bytesHAMK.Resize( lenHAMK );
				for ( int i = 0; i < lenHAMK; ++i )
					bytesHAMK[i] = NETWORK_ReadByte( pByteStream );
				const int clientID = SERVER_FindClientWithSessionID ( sessionID );
				if ( clientID < MAXPLAYERS )
				{
					// [BB] The user has logged in in successfully.
					Printf ( "Client %d has logged in successfully as '%s'.\n", clientID, SERVER_GetClient(clientID)->username.GetChars() );
					SERVER_GetClient(clientID)->loggedIn = true;
					// [BB] Thus the session is invalid.
					SERVER_GetClient(clientID)->SRPsessionID = -1;
					// [BB] Send H(A,M,K) back to the client. This allows the client to
					// check that the communication with the auth server was legit.
					SERVER_GetClient(clientID)->bytesHAMK = bytesHAMK;
					SERVERCOMMANDS_SRPUserVerifySession ( clientID );
				}
			}
			break;
		case AUTH_SERVER_USER_ERROR:
			{
				const int errorCode = NETWORK_ReadByte( pByteStream );
				const FString username = NETWORK_ReadString( pByteStream );
				Printf ( "Error: Error authenticating user '%s':.\n", username.GetChars() );
				FString errorMessage = "";
				switch ( errorCode )
				{
				case USER_TRY_LATER:
					errorMessage = "The auth server is having issues. Try again later.\n";
					break;
				case USER_NO_EXIST:
					errorMessage = "User does not exist.\n";
					break;
				case USER_OUTDATED_PROTOCOL:
					errorMessage = "User requires a newer version of the protocol to authenticate.\n";
					break;
				case USER_WILL_NOT_AUTH:
					errorMessage = "User exists, but will not be authenticated.\n";
					break;
				default:
					errorMessage.AppendFormat ( "Unknown error code '%d'.\n", errorCode );
					break;
				}
				Printf ( "%s", errorMessage.GetChars() );

				const int clientID = SERVER_FindClientWithUsername ( username.GetChars() );
				if ( clientID < MAXPLAYERS )
				{
					// [BB] Since the authentication failed, clear all authentication related data of this client.
					SERVER_InitClientSRPData ( clientID );
					SERVER_PrintfPlayer( PRINT_HIGH, clientID, "User authentication failed! %s", errorMessage.GetChars() );
				}
				else
					Printf ( "AUTH_SERVER_USER_ERROR: Can't find client with username '%s'.\n", username.GetChars() );
			}
			break;
		case AUTH_SERVER_SESSION_ERROR:
			{
				const int errorCode = NETWORK_ReadByte( pByteStream );
				const int sessionID = NETWORK_ReadLong( pByteStream );
				Printf ( "Session error: " );
				FString errorMessage = "";
				switch ( errorCode )
				{
				case SESSION_TRY_LATER:
					errorMessage = "The auth server is having issues. Try again later.\n";
					break;
				case SESSION_NO_EXIST:
					errorMessage = "Session ID does not exist or timed out.\n";
					break;
				case SESSION_VERIFIER_UNSAFE:
					errorMessage = "Verifier safety check has been violated.\n";
					break;
				case SESSION_AUTH_FAILED:
					errorMessage = "User could not be authenticated.\n";
					break;
				default:
					errorMessage.AppendFormat ( "Unknown error code '%d'.\n", errorCode );
					break;
				}
				Printf ( "%s", errorMessage.GetChars() );

				const int clientID = SERVER_FindClientWithSessionID ( sessionID );
				if ( clientID < MAXPLAYERS )
				{
					// [BB] Since the authentication failed, clear all authentication related data of this client.
					SERVER_InitClientSRPData ( clientID );
					SERVER_PrintfPlayer( PRINT_HIGH, clientID, "Session error: %s", errorMessage.GetChars() );
				}
			}
			break;
		default:
			Printf ( "Error: Received unknown command '%d' from authentication server.\n", commandNum );
			break;
		}
	}
}

//*****************************************************************************
//
bool SERVER_ProcessSRPClientCommand( LONG lCommand, BYTESTREAM_s *pByteStream )
{
	switch ( lCommand )
	{
	case CLC_SRP_USER_START_AUTHENTICATION:
		{
			CLIENT_s *pClient = SERVER_GetClient(SERVER_GetCurrentClient());

			pClient->username = NETWORK_ReadString( pByteStream );
			const int lenA = NETWORK_ReadShort( pByteStream );
			pClient->bytesA.Resize ( lenA ); 
			for ( int i = 0; i < lenA; ++i )
				pClient->bytesA[i] = NETWORK_ReadByte( pByteStream );

#if EMULATE_AUTH_SERVER
			const unsigned char * bytesS = NULL;
			const unsigned char * bytesV = NULL;
			int lenS   = 0;
			int lenV   = 0;
			const char *password = "password";
			srp_create_salted_verification_key( SRP_SHA256, SRP_NG_2048, pClient->username.GetChars(), reinterpret_cast<const unsigned char *>(password), strlen(password), 
                &bytesS, &lenS, &bytesV, &lenV, NULL, NULL );

			pClient->salt.Resize ( lenS ); 
			for ( int i = 0; i < lenS; ++i )
				pClient->salt[i] = bytesS[i];
			
			const unsigned char * bytesB = NULL;
			int lenB = 0;
			if ( g_ver != NULL )
				 srp_verifier_delete( g_ver );
			g_ver = srp_verifier_new( SRP_SHA256, SRP_NG_2048, pClient->username.GetChars(), bytesS, lenS, bytesV, lenV, &(pClient->bytesA[0]), lenA, &bytesB, &lenB, NULL, NULL, 1 );

			if ( ( bytesB == NULL ) || ( lenB == 0 ) )
			{
		        Printf ( "Verifier SRP-6a safety check violated!\n" );
				pClient->bytesB.Clear(); 
			}
			else
			{
		        Printf ( "Verifier SRP-6a safety check passed.\n" );
				pClient->bytesB.Resize ( lenB ); 
				for ( int i = 0; i < lenB; ++i )
					pClient->bytesB[i] = bytesB[i];

				SERVERCOMMANDS_SRPUserProcessChallenge ( SERVER_GetCurrentClient() );
			}

			free( (char *)bytesS );
			free( (char *)bytesV );
#else
			// [BB] The client wants to log in, so start negotiating with the auth server.
			SERVER_AUTH_Negotiate ( pClient->username.GetChars() );
#endif

			return ( false );
		}
		break;
	case CLC_SRP_USER_PROCESS_CHALLENGE:
		{
			CLIENT_s *pClient = SERVER_GetClient(SERVER_GetCurrentClient());

			const int lenM = NETWORK_ReadShort( pByteStream );
			pClient->bytesM.Resize ( lenM ); 
			for ( int i = 0; i < lenM; ++i )
				pClient->bytesM[i] = NETWORK_ReadByte( pByteStream );

#if EMULATE_AUTH_SERVER
			unsigned char bytesM[SHA512_DIGEST_LENGTH];
			for ( int i = 0; i < lenM; ++i )
				bytesM[i] = pClient->bytesM[i];

			if ( g_ver == NULL )
				Printf ( "Error: Verifier pointer is NULL.\n" );
			else
			{
				const unsigned char * bytesHAMK = NULL;
				srp_verifier_verify_session( g_ver, bytesM, &bytesHAMK );

				if ( bytesHAMK == NULL )
				{
					SERVER_InitClientSRPData ( SERVER_GetCurrentClient() );
					Printf ( "User authentication failed!\n" );
					SERVER_PrintfPlayer( PRINT_HIGH, SERVER_GetCurrentClient(), "User authentication failed!\n" );
				}
				else
				{
					Printf ( "User authentication successfully.\n" );
					// [BB] The user has logged in in successfully.
					SERVER_GetClient(SERVER_GetCurrentClient())->loggedIn = true;

					pClient->bytesHAMK.Resize ( srp_verifier_get_session_key_length( g_ver ) );
					for ( unsigned int i = 0; i < pClient->bytesHAMK.Size(); ++i )
						pClient->bytesHAMK[i] = bytesHAMK[i];

					SERVERCOMMANDS_SRPUserVerifySession ( SERVER_GetCurrentClient() );
				}
			}
#else
			if ( pClient->SRPsessionID != -1 )
				SERVER_AUTH_SRPMessage ( SERVER_AUTH_SRP_STEP_THREE, pClient->SRPsessionID, pClient->bytesM );
#endif

			return ( false );
		}
		break;

	default:
		Printf ( "Error: Received unknown SRP command '%d' from client %d.\n", static_cast<int>(lCommand), static_cast<int>(SERVER_GetCurrentClient()) );
		break;
	}
	return ( false );
}
