/*
** music_mus_midiout.cpp
** Code to let ZDoom play MUS music through the MIDI streaming API.
**
**---------------------------------------------------------------------------
** Copyright 1998-2008 Randy Heit
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
*/

// HEADER FILES ------------------------------------------------------------

#if !defined(__FreeBSD__) && !defined(__APPLE__)
#include <malloc.h>
#else
#include <stdlib.h>
#endif
#include "i_musicinterns.h"
#include "templates.h"
#include "doomdef.h"
#include "m_swap.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static const BYTE CtrlTranslate[15] =
{
	0,	// program change
	0,	// bank select
	1,	// modulation pot
	7,	// volume
	10, // pan pot
	11, // expression pot
	91, // reverb depth
	93, // chorus depth
	64, // sustain pedal
	67, // soft pedal
	120, // all sounds off
	123, // all notes off
	126, // mono
	127, // poly
	121, // reset all controllers
};

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// CODE --------------------------------------------------------------------

//==========================================================================
//
// MUSSong2 Constructor
//
// Performs some validity checks on the MUS file, buffers it, and creates
// the playback thread control events.
//
//==========================================================================

MUSSong2::MUSSong2 (FILE *file, char *musiccache, int len, EMIDIDevice type)
: MIDIStreamer(type), MusHeader(0), MusBuffer(0)
{
#ifdef _WIN32
	if (ExitEvent == NULL)
	{
		return;
	}
#endif

	MusHeader = (MUSHeader *)new BYTE[len];
	if (file != NULL)
	{
		if (fread(MusHeader, 1, len, file) != (size_t)len)
		{
			return;
		}
	}
	else
	{
		memcpy(MusHeader, musiccache, len);
	}

	// Do some validation of the MUS file
	if (MusHeader->Magic != MAKE_ID('M','U','S','\x1a'))
	{
		return;
	}
	
	if (LittleShort(MusHeader->NumChans) > 15)
	{
		return;
	}

	MusBuffer = (BYTE *)MusHeader + LittleShort(MusHeader->SongStart);
	MaxMusP = MIN<int> (LittleShort(MusHeader->SongLen), len - LittleShort(MusHeader->SongStart));
	Division = 140;
	InitialTempo = 1000000;
}

//==========================================================================
//
// MUSSong2 Destructor
//
//==========================================================================

MUSSong2::~MUSSong2 ()
{
	if (MusHeader != NULL)
	{
		delete[] (BYTE *)MusHeader;
	}
}

//==========================================================================
//
// MUSSong2 :: DoInitialSetup
//
// Sets up initial velocities and channel volumes.
//
//==========================================================================

void MUSSong2::DoInitialSetup()
{
	for (int i = 0; i < 16; ++i)
	{
		LastVelocity[i] = 100;
		ChannelVolumes[i] = 127;
	}
}

//==========================================================================
//
// MUSSong2 :: DoRestart
//
// Rewinds the song.
//
//==========================================================================

void MUSSong2::DoRestart()
{
	MusP = 0;
}

//==========================================================================
//
// MUSSong2 :: CheckDone
//
//==========================================================================

bool MUSSong2::CheckDone()
{
	return MusP >= MaxMusP;
}

//==========================================================================
//
// MUSSong2 :: Precache
//
// MUS songs contain information in their header for exactly this purpose.
//
//==========================================================================

void MUSSong2::Precache()
{
	WORD *work = (WORD *)alloca(MusHeader->NumInstruments * sizeof(WORD));
	const WORD *used = (WORD *)MusHeader + sizeof(MUSHeader) / sizeof(WORD);
	int i, j;

	for (i = j = 0; i < MusHeader->NumInstruments; ++i)
	{
		WORD instr = LittleShort(used[i]);
		if (instr < 128)
		{
			work[j++] = instr;
		}
		else if (used[i] >= 135 && used[i] <= 181)
		{ // Percussions are 100-based, not 128-based, eh?
			work[j++] = instr - 100 + (1 << 14);
		}
	}
	MIDI->PrecacheInstruments(&work[0], j);
}

//==========================================================================
//
// MUSSong2 :: MakeEvents
//
// Translates MUS events into MIDI events and puts them into a MIDI stream
// buffer. Returns the new position in the buffer.
//
//==========================================================================

DWORD *MUSSong2::MakeEvents(DWORD *events, DWORD *max_event_p, DWORD max_time)
{
	DWORD tot_time = 0;
	DWORD time = 0;

	max_time = max_time * Division / Tempo;

	while (events < max_event_p && tot_time <= max_time)
	{
		BYTE mid1, mid2;
		BYTE channel;
		BYTE t = 0, status;
		BYTE event = MusBuffer[MusP++];
		
		if ((event & 0x70) != MUS_SCOREEND)
		{
			t = MusBuffer[MusP++];
		}
		channel = event & 15;

		// Map MUS channels to MIDI channels
		if (channel == 15)
		{
			channel = 9;
		}
		else if (channel >= 9)
		{
			channel = channel + 1;
		}

		status = channel;

		switch (event & 0x70)
		{
		case MUS_NOTEOFF:
			status |= MIDI_NOTEOFF;
			mid1 = t;
			mid2 = 64;
			break;
			
		case MUS_NOTEON:
			status |= MIDI_NOTEON;
			mid1 = t & 127;
			if (t & 128)
			{
				LastVelocity[channel] = MusBuffer[MusP++];
			}
			mid2 = LastVelocity[channel];
			break;
			
		case MUS_PITCHBEND:
			status |= MIDI_PITCHBEND;
			mid1 = (t & 1) << 6;
			mid2 = (t >> 1) & 127;
			break;
			
		case MUS_SYSEVENT:
			status |= MIDI_CTRLCHANGE;
			mid1 = CtrlTranslate[t];
			mid2 = t == 12 ? LittleShort(MusHeader->NumChans) : 0;
			break;
			
		case MUS_CTRLCHANGE:
			if (t == 0)
			{ // program change
				status |= MIDI_PRGMCHANGE;
				mid1 = MusBuffer[MusP++];
				mid2 = 0;
			}
			else
			{
				status |= MIDI_CTRLCHANGE;
				mid1 = CtrlTranslate[t];
				mid2 = MusBuffer[MusP++];
				if (mid1 == 7)
				{
					mid2 = VolumeControllerChange(channel, mid2);
				}
			}
			break;
			
		case MUS_SCOREEND:
		default:
			MusP = MaxMusP;
			goto end;
		}

		events[0] = time;			// dwDeltaTime
		events[1] = 0;				// dwStreamID
		events[2] = status | (mid1 << 8) | (mid2 << 16);
		events += 3;

		time = 0;
		if (event & 128)
		{
			do
			{
				t = MusBuffer[MusP++];
				time = (time << 7) | (t & 127);
			}
			while (t & 128);
		}
		tot_time += time;
	}
end:
	if (time != 0)
	{
		events[0] = time;			// dwDeltaTime
		events[1] = 0;				// dwStreamID
		events[2] = MEVT_NOP << 24;	// dwEvent
		events += 3;
	}
	return events;
}

//==========================================================================
//
// MUSSong2 :: GetOPLDumper
//
//==========================================================================

MusInfo *MUSSong2::GetOPLDumper(const char *filename)
{
	return new MUSSong2(this, filename, MIDI_OPL);
}

//==========================================================================
//
// MUSSong2 :: GetWaveDumper
//
//==========================================================================

MusInfo *MUSSong2::GetWaveDumper(const char *filename, int rate)
{
	return new MUSSong2(this, filename, MIDI_Timidity);
}

//==========================================================================
//
// MUSSong2 OPL Dumping Constructor
//
//==========================================================================

MUSSong2::MUSSong2(const MUSSong2 *original, const char *filename, EMIDIDevice type)
: MIDIStreamer(filename, type)
{
	int songstart = LittleShort(original->MusHeader->SongStart);
	MaxMusP = original->MaxMusP;
	MusHeader = (MUSHeader *)new BYTE[songstart + MaxMusP];
	memcpy(MusHeader, original->MusHeader, songstart + MaxMusP);
	MusBuffer = (BYTE *)MusHeader + songstart;
	Division = 140;
	InitialTempo = 1000000;
}
