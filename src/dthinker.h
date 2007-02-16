/*
** dthinker.h
**
**---------------------------------------------------------------------------
** Copyright 1998-2006 Randy Heit
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

#ifndef __DTHINKER_H__
#define __DTHINKER_H__

#include <stdlib.h>
#include "dobject.h"
#include "lists.h"

class AActor;
class player_s;
struct pspdef_s;

typedef void (*actionf_p)( AActor* );

class FThinkerIterator;

enum { MAX_STATNUM = 127 };

// Doubly linked list of thinkers
class DThinker : public DObject, protected Node
{
	DECLARE_CLASS (DThinker, DObject)

public:
	DThinker (int statnum = MAX_STATNUM) throw();
	void Destroy ();
	virtual ~DThinker ();
	virtual void Tick ();
	virtual void PostBeginPlay ();	// Called just before the first tick

	void ChangeStatNum (int statnum);

	static void RunThinkers ();
	static void RunThinkers (int statnum);
	static void DestroyAllThinkers ();
	static void DestroyMostThinkers ();
	static void SerializeAll (FArchive &arc, bool keepPlayers);

	static DThinker *FirstThinker (int statnum);

private:
	static void DestroyThinkersInList (Node *first);
	static void DestroyMostThinkersInList (List &list, int stat);
	static int TickThinkers (List *list, List *dest);	// Returns: # of thinkers ticked


	static List Thinkers[MAX_STATNUM+1];		// Current thinkers
	static List FreshThinkers[MAX_STATNUM+1];	// Newly created thinkers
	static bool bSerialOverride;

	friend class FThinkerIterator;
	friend class DObject;
};

class FThinkerIterator
{
private:
	const PClass *m_ParentType;
	Node *m_CurrThinker;
	BYTE m_Stat;
	bool m_SearchStats;
	bool m_SearchingFresh;

public:
	FThinkerIterator (const PClass *type, int statnum=MAX_STATNUM+1);
	FThinkerIterator (const PClass *type, int statnum, DThinker *prev);
	DThinker *Next ();
	void Reinit ();
};

template <class T> class TThinkerIterator : public FThinkerIterator
{
public:
	TThinkerIterator (int statnum=MAX_STATNUM+1) : FThinkerIterator (RUNTIME_CLASS(T), statnum)
	{
	}
	TThinkerIterator (int statnum, DThinker *prev) : FThinkerIterator (RUNTIME_CLASS(T), statnum, prev)
	{
	}
	T *Next ()
	{
		return static_cast<T *>(FThinkerIterator::Next ());
	}
};

#endif //__DTHINKER_H__
