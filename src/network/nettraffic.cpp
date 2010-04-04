//-----------------------------------------------------------------------------
//
// Skulltag Source
// Copyright (C) 2010 Benjamin Berkels
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
// Filename: nettraffic.cpp
//
//-----------------------------------------------------------------------------

#include "nettraffic.h"
#include "network.h"
#include "c_dispatch.h"

#include <map>

//*****************************************************************************
//	VARIABLES

struct ltstr
{
  bool operator()(const char* s1, const char* s2) const
  {
    return strcmp(s1, s2) < 0;
  }
};

std::map<const char*, int, ltstr> g_actorTrafficMap;
std::map<int, int> g_ACSScriptTrafficMap;

CVAR( Bool, sv_measureoutboundtraffic, false, 0 )

//*****************************************************************************
//
void NETTRAFFIC_AddActorTraffic ( const AActor* pActor, const int BytesUsed )
{
	if ( ( pActor == NULL ) || ( BytesUsed == 0 ) )
		return;

	if ( ( NETWORK_GetState( ) != NETSTATE_SERVER ) || ( sv_measureoutboundtraffic == false ) )
		return;

	g_actorTrafficMap [ pActor->GetClass()->TypeName.GetChars() ] += BytesUsed;
}

//*****************************************************************************
//
void NETTRAFFIC_AddACSScriptTraffic ( const int ScriptNum, const int BytesUsed )
{
	if ( BytesUsed == 0 )
		return;

	if ( ( NETWORK_GetState( ) != NETSTATE_SERVER ) || ( sv_measureoutboundtraffic == false ) )
		return;

	g_ACSScriptTrafficMap [ ScriptNum ] += BytesUsed;
}

//*****************************************************************************
//
void NETTRAFFIC_Reset ( )
{
	g_actorTrafficMap.clear();
	g_ACSScriptTrafficMap.clear();
}

//*****************************************************************************
//
CCMD( dumptrafficmeasure )
{
	if ( NETWORK_GetState( ) != NETSTATE_SERVER )
		return;

	Printf ( "Network traffic (in bytes) caused by actor code pointers:\n" );

	for ( std::map<const char*, int, ltstr>::const_iterator it = g_actorTrafficMap.begin(); it != g_actorTrafficMap.end(); ++it )
		Printf ( "%s %d\n", (*it).first, (*it).second );

	Printf ( "\nNetwork traffic (in bytes) caused by ACS scripts:\n" );
	for ( std::map<int, int>::const_iterator it = g_ACSScriptTrafficMap.begin(); it != g_ACSScriptTrafficMap.end(); ++it )
		Printf ( "Script %d: %d\n", (*it).first, (*it).second );
}

//*****************************************************************************
//
CCMD( cleartrafficmeasure )
{
	NETTRAFFIC_Reset ();
}
