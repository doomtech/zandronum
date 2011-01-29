/*
** m_random.h
** Random number generators
**
**---------------------------------------------------------------------------
** Copyright 2002-2009 Randy Heit
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

#ifndef __M_RANDOM__
#define __M_RANDOM__

#include <stdio.h>
#include "basictypes.h"
#include "sfmt/SFMT.h"

struct PNGHandle;

class FRandom
{
public:
	FRandom ();
	FRandom (const char *name);
	~FRandom ();

	// Returns a random number in the range [0,255]
	// [BB] Moved the implementation to m_random.cpp.
	int operator()();

	// Returns a random number in the range [0,mod)
	int operator() (int mod)
	{
		return GenRand32() % mod;
	}

	// Returns rand# - rand#
	// [BB] Moved the implementation to m_random.cpp.
	int Random2();

// Returns (rand# & mask) - (rand# & mask)
	int Random2(int mask)
	{
		int t = GenRand32() & mask & 255;
		return t - (GenRand32() & mask & 255);
	}

	// HITDICE macro used in Heretic and Hexen
	int HitDice(int count)
	{
		return (1 + (GenRand32() & 7)) * count;
	}

	int Random()				// synonym for ()
	{
		return operator()();
	}

	void Init(DWORD seed);

	// SFMT interface
	unsigned int GenRand32();
	QWORD GenRand64();
	void FillArray32(DWORD *array, int size);
	void FillArray64(QWORD *array, int size);
	void InitGenRand(DWORD seed);
	void InitByArray(DWORD *init_key, int key_length);
	int GetMinArraySize32();
	int GetMinArraySize64();

	/* These real versions are due to Isaku Wada */
	/** generates a random number on [0,1]-real-interval */
	static inline double ToReal1(DWORD v)
	{
		return v * (1.0/4294967295.0); 
		/* divided by 2^32-1 */ 
	}

	/** generates a random number on [0,1]-real-interval */
	inline double GenRand_Real1()
	{
		return ToReal1(GenRand32());
	}

	/** generates a random number on [0,1)-real-interval */
	static inline double ToReal2(DWORD v)
	{
		return v * (1.0/4294967296.0); 
		/* divided by 2^32 */
	}

	/** generates a random number on [0,1)-real-interval */
	inline double GenRand_Real2()
	{
		return ToReal2(GenRand32());
	}

	/** generates a random number on (0,1)-real-interval */
	static inline double ToReal3(DWORD v)
	{
		return (((double)v) + 0.5)*(1.0/4294967296.0); 
		/* divided by 2^32 */
	}

	/** generates a random number on (0,1)-real-interval */
	inline double GenRand_Real3(void)
	{
		return ToReal3(GenRand32());
	}
	/** These real versions are due to Isaku Wada */

	/** generates a random number on [0,1) with 53-bit resolution*/
	static inline double ToRes53(QWORD v) 
	{ 
		return v * (1.0/18446744073709551616.0L);
	}

	/** generates a random number on [0,1) with 53-bit resolution from two
	 * 32 bit integers */
	static inline double ToRes53Mix(DWORD x, DWORD y) 
	{ 
		return ToRes53(x | ((QWORD)y << 32));
	}

	/** generates a random number on [0,1) with 53-bit resolution
	 */
	inline double GenRand_Res53(void) 
	{ 
		return ToRes53(GenRand64());
	} 

	/** generates a random number on [0,1) with 53-bit resolution
		using 32bit integer.
	 */
	inline double GenRand_Res53_Mix() 
	{ 
		DWORD x, y;

		x = GenRand32();
		y = GenRand32();
		return ToRes53Mix(x, y);
	}

	// Static interface
	static void StaticClearRandom ();
	static DWORD StaticSumSeeds ();
	static void StaticReadRNGState (PNGHandle *png);
	static void StaticWriteRNGState (FILE *file);
	static FRandom *StaticFindRNG(const char *name);

#ifndef NDEBUG
	static void StaticPrintSeeds ();
#endif

private:
#ifndef NDEBUG
	const char *Name;
#endif
	FRandom *Next;
	DWORD NameCRC;

	static FRandom *RNGList;

	/*-------------------------------------------
	  SFMT internal state, index counter and flag 
	  -------------------------------------------*/

	void GenRandAll();
	void GenRandArray(w128_t *array, int size);
	void PeriodCertification();

	/** the 128-bit internal state array */
	union
	{
		w128_t w128[SFMT::N];
		unsigned int u[SFMT::N32];
		QWORD u64[SFMT::N64];
	} sfmt;
	/** index counter to the 32-bit internal state array */
	int idx;
	/** a flag: it is 0 if and only if the internal state is not yet
	 * initialized. */
#ifndef NDEBUG
	bool initialized;
#endif
};

extern DWORD rngseed;			// The starting seed (not part of state)

// M_Random can be used for numbers that do not affect gameplay
extern FRandom M_Random;

#endif
