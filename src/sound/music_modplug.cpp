/*
** music_modplug.cpp
**
** MOD playback using libmodplug
**
**---------------------------------------------------------------------------
** Copyright 2007 Christoph Oelckers
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
**
*/

#include "i_musicinterns.h"
#include "templates.h"
#include "c_cvars.h"
#include "doomdef.h"
#include "modplug/modplug.h"

// 192 approximately replicates the volume of a WinAmp Wave export of the song.
CVAR(Int, snd_mod_mastervolume, 192, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)

ModPlugSong::ModPlugSong (FILE *iofile, char * musiccache, int len)
{
	char *buffer;

	Data = NULL;
	order = 0;
	if (iofile != NULL)
	{
		buffer = new char[len];
		fread(buffer, 1, len, iofile);
	}
	else buffer = musiccache;

	m_Stream = GSnd->CreateStream (FillStream, 16384, 0, 44100, this);
	if (m_Stream == NULL)
	{
		Printf (PRINT_BOLD, "Could not create music stream.\n");
		if (iofile != NULL) delete buffer;
		return;
	}

	Data = ModPlug_Load(buffer, len);
	if (Data == NULL)
	{
		if (iofile != NULL) delete buffer;
		delete m_Stream;
		m_Stream = NULL;
		return;
	}
	ModPlug_SetMasterVolume(Data, snd_mod_mastervolume);
}

ModPlugSong::~ModPlugSong ()
{
	Stop ();
	if (Data != NULL) ModPlug_Unload(Data);
	Data = NULL;
}

bool ModPlugSong::IsPlaying ()
{
	return m_Status == STATE_Playing;
}

void ModPlugSong::Play(bool looping)
{
	ModPlug_Settings settings;
	ModPlug_GetSettings(&settings);
	settings.mLoopCount=looping? -1:0;
	ModPlug_SetSettings(&settings);
	StreamSong::Play(looping);
}

bool ModPlugSong::SetPosition (int order)
{
	this->order = order;
	ModPlug_SeekOrder(Data, order);
	return true;
}

bool ModPlugSong::FillStream (SoundStream *stream, void *buf, int len, void *userdata)
{
	char *buff = (char *)buf;
	ModPlugSong *song = (ModPlugSong *)userdata;

	int read = ModPlug_Read(song->Data, buff, len);

	if (read < len)
	{
		buff += read;
		len -= read;
		if (song->m_Looping)
		{
			ModPlug_SeekOrder(song->Data, song->order);
			read = ModPlug_Read(song->Data, buff, len);
			if (read < len)
			{
				memset(buff+read, 0, len-read);
			}
			return true;
		}
		if (len > 0)
		{
			memset(buff+read, 0, len-read);
			return true;
		}
		else
		{
			song->Stop();
			return false;
		}
	}
	return true;
}

