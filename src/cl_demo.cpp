//-----------------------------------------------------------------------------
//
// Skulltag Source
// Copyright (C) 2003 Brad Carney
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
// Date created:  7/2/07
//
//
// Filename: cl_demo.cpp
//
// Description: 
//
//-----------------------------------------------------------------------------

#include "c_dispatch.h"
#include "cl_demo.h"
#include "cmdlib.h"
#include "d_netinf.h"
#include "d_protocol.h"
#include "doomtype.h"
#include "i_system.h"
#include "m_random.h"
#include "version.h"

//*****************************************************************************
//	VARIABLES

// Are we recording a demo?
static	bool				g_bDemoRecording;

// Buffer for our demo.
static	BYTE				*g_pbDemoBuffer;

// Name of our demo.
static	char				g_szDemoName[8];

// Length of the demo.
static	LONG				g_lDemoLength;

//*****************************************************************************
//	FUNCTIONS

void CLIENTDEMO_BeginRecording( char *pszDemoName )
{
	if ( pszDemoName == NULL )
		return;

	// First, setup the demo name.
	strcpy( g_szDemoName, pszDemoName );
	FixPathSeperator( g_szDemoName );
	DefaultExtension( g_szDemoName, ".cld" );

	// Allocate memory for the demo buffer.
	g_bDemoRecording = true;
	g_pbDemoBuffer = (BYTE *)malloc( 0x20000 );

	// Write our header.
	StartChunk( CLD_DEMOSTART, &g_pbDemoBuffer );
	WriteLong( 12345678, &g_pbDemoBuffer );
	FinishChunk( &g_pbDemoBuffer );

	// Write the length of the demo. Of course, we can't complete this quite yet!
	StartChunk( CLD_DEMOLENGTH, &g_pbDemoBuffer );
	g_pbDemoBuffer += 4;
	FinishChunk( &g_pbDemoBuffer );

	// Write version information helpful for this demo.
	StartChunk( CLD_DEMOVERSION, &g_pbDemoBuffer );
	WriteWord( DEMOGAMEVERSION, &g_pbDemoBuffer );
	WriteString( DOTVERSIONSTR, &g_pbDemoBuffer );
	WriteLong( rngseed, &g_pbDemoBuffer );
	FinishChunk( &g_pbDemoBuffer );

	// Write cvars chunk.
	StartChunk( CLD_CVARS, &g_pbDemoBuffer );
	C_WriteCVars( &g_pbDemoBuffer, CVAR_SERVERINFO|CVAR_DEMOSAVE );
	FinishChunk( &g_pbDemoBuffer );

	// Write the console player's userinfo.
	StartChunk( CLD_USERINFO, &g_pbDemoBuffer );
	WriteByte( (BYTE)consoleplayer, &g_pbDemoBuffer );
	D_WriteUserInfoStrings( consoleplayer, &g_pbDemoBuffer );
	FinishChunk( &g_pbDemoBuffer );

	// Indicate that we're done with header information, and are ready
	// to move onto the body of the demo.
	StartChunk( CLD_BODYSTART, &g_pbDemoBuffer );
}

//*****************************************************************************
//
bool CLIENTDEMO_ProcessDemoHeader( void )
{
	bool	bBodyStart;
	LONG	lDemoVersion;
	LONG	lCommand;
	BYTE	*pDemoEnd;

	if (( ReadLong( &g_pbDemoBuffer ) != CLD_DEMOSTART ) ||
		( ReadLong( &g_pbDemoBuffer ) != 12345678 ))
	{
		I_Error( "CLIENTDEMO_ProcessDemoHeader: Expected CLD_DEMOSTART.\n" );
		return ( false );
	}

	if ( ReadLong( &g_pbDemoBuffer ) != CLD_DEMOLENGTH )
	{
		I_Error( "CLIENTDEMO_ProcessDemoHeader: Expected CLD_DEMOLENGTH.\n" );
		return ( false );
	}

	g_lDemoLength = ReadLong( &g_pbDemoBuffer );
	pDemoEnd = g_pbDemoBuffer + g_lDemoLength + ( g_lDemoLength & 1 );

	// Continue to read header commands until we reach the body of the demo.
	bBodyStart = false;
	while (( g_pbDemoBuffer < pDemoEnd ) && ( bBodyStart == false ))
	{  
		lCommand = ReadLong( &g_pbDemoBuffer );

		switch ( lCommand )
		{
		case CLD_DEMOVERSION:

			// Read in the DEMOGAMEVERSION the demo was recorded with.
			lDemoVersion = ReadWord( &g_pbDemoBuffer );
			if ( lDemoVersion < MINDEMOVERSION )
				I_Error( "Demo requires an older version of Skulltag!\n" );

			// Read in the DOTVERSIONSTR the demo was recorded with.
			Printf( "Version %s demo\n", ReadString( &g_pbDemoBuffer ));

			// Read in the random number generator seed.
			rngseed = ReadLong( &g_pbDemoBuffer );
			FRandom::StaticClearRandom( );
			break;
		case CLD_CVARS:

			C_ReadCVars( &g_pbDemoBuffer );
			break;
		case CLD_USERINFO:

			consoleplayer = ReadByte( &g_pbDemoBuffer );
			D_ReadUserInfoStrings( consoleplayer, &g_pbDemoBuffer, false );
			break;
		case CLD_BODYSTART:

			bBodyStart = true;
			break;
		}
	}

	return ( bBodyStart );
}

//*****************************************************************************
//
bool CLIENTDEMO_IsRecording( void )
{
	return ( g_bDemoRecording );
}

//*****************************************************************************
//
bool CLIENTDEMO_IsPlaying( void )
{
	return ( false );
}
