/*
** version.h
**
**---------------------------------------------------------------------------
** Copyright 1998-2007 Randy Heit
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

#ifndef __VERSION_H__
#define __VERSION_H__

// The svnrevision.h is automatically updated to grab the revision of
// of the current source tree so that it can be included with version numbers.
#include "svnrevision.h"

/** Lots of different version numbers **/

#define DOTVERSIONSTR "0.97d"
#define DOTVERSIONSTR_NOREV DOTVERSIONSTR

// [BB] The version string that includes revision / compatibility data.
#define DOTVERSIONSTR_REV DOTVERSIONSTR "-r" SVN_REVISION_STRING

// [BC] What version of ZDoom is this based off of?
#define	ZDOOMVERSIONSTR		"2.1.8-552"

/** Release code stuff */

// [RC] Release code ID for this build.
#define BUILD_ID			BUILD_INTERNAL;
#define BUILD_ID_STR		"Internal" // Used in the exe's metadata.

// Please maintain the existing structure as much as possible, because it's
// used in communicating between servers and clients of different versions.
#define BUILD_OTHER			0;
#define BUILD_RELEASE		1;
#define BUILD_INTERNAL		2;
#define BUILD_PRIVATE		3;

// Version identifier for network games.
// Bump it every time you do a release unless you're certain you
// didn't change anything that will affect network protocol.
// 003 = 0.97c2
// 004 = 0.97c3
// 005 = 0.97d-beta4
// 006 = 0.97d-beta4.2
#define NETGAMEVERSION 007

// Version stored in the ini's [LastRun] section.
// Bump it if you made some configuration change that you want to
// be able to migrate in FGameConfigFile::DoGlobalSetup().
#define LASTRUNVERSION "205"

// Protocol version used in demos.
// Bump it if you change existing DEM_ commands or add new ones.
// Otherwise, it should be safe to leave it alone.
#define DEMOGAMEVERSION 0x209

// Minimum demo version we can play.
// Bump it whenever you change or remove existing DEM_ commands.
#define MINDEMOVERSION 0x207

// [BC] This is what's displayed as the title for server windows.
#define	SERVERCONSOLE_TITLESTRING	"Skulltag v" DOTVERSIONSTR " Server"

// SAVEVER is the version of the information stored in level snapshots.
// Note that SAVEVER is not directly comparable to VERSION.
// SAVESIG should match SAVEVER.
#define SAVEVER 237
#define SAVESIG "ZDOOMSAVE237"

// This is so that derivates can use the same savegame versions without worrying about engine compatibility
#define GAMESIG "SKULLTAG"
#define BASEWAD "skulltag.pk3"

// More stuff that needs to be different for derivatives.
#define GAMENAME "Skulltag"
#define GAMENAMELOWERCASE "skulltag"
#define FORUM_URL "http://www.skulltag.com/forum/"

#ifdef unix
#define HOME_DIR "~/." GAMENAMELOWERCASE
#endif

// MINSAVEVER is the minimum level snapshot version that can be loaded.
#define MINSAVEVER 237	// Used by 2.1.8

// The maximum length of one save game description for the menus.
#define SAVESTRINGSIZE		24

#endif //__VERSION_H__
