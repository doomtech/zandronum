//-----------------------------------------------------------------------------
//
// Skulltag Source
// Copyright (C) 2003-2005 Brad Carney
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
// Date created:  9/27/05
//
//
// Filename: statistics.h
//
// Description: 
//
//-----------------------------------------------------------------------------

#ifndef __STATISTICS_H__
#define __STATISTICS_H__

//*****************************************************************************
//	DEFINES

//*****************************************************************************
//	STRUCTURES

//*****************************************************************************
//	PROTOTYPES

void	STATISTICS_Construct( void );

void	STATISTICS_GetNode( char *pszName, ULONG ulPlayer );
void	INVASION_StartFirstCountdown( ULONG ulTicks );
void	INVASION_StartCountdown( ULONG ulTicks );
void	INVASION_BeginWave( ULONG ulWave );
void	INVASION_DoWaveComplete( void );
void	INVASION_WriteSaveInfo( FILE *pFile );
void	INVASION_ReadSaveInfo( PNGHandle *pPng );

ULONG	INVASION_GetCountdownTicks( void );
void	INVASION_SetCountdownTicks( ULONG ulTicks );	

INVASIONSTATE_e		INVASION_GetState( void );
void				INVASION_SetState( INVASIONSTATE_e State );

ULONG	INVASION_GetNumMonstersLeft( void );
void	INVASION_SetNumMonstersLeft( ULONG ulLeft );

ULONG	INVASION_GetNumArchVilesLeft( void );
void	INVASION_SetNumArchVilesLeft( ULONG ulLeft );

ULONG	INVASION_GetCurrentWave( void );

//*****************************************************************************
//  EXTERNAL CONSOLE VARIABLES

EXTERN_CVAR( Int, sv_invasioncountdowntime )
EXTERN_CVAR( Int, wavelimit )

#endif	// __STATISTICS_H__
