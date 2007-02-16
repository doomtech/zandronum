/*
** a_spark.cpp
** Actor that makes a particle spark when activated
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

#include "actor.h"
#include "info.h"
#include "m_random.h"
#include "p_effect.h"
#include "s_sound.h"

class ASpark : public AActor
{
	DECLARE_STATELESS_ACTOR (ASpark, AActor)
public:
	angle_t AngleIncrements ();
	void Activate (AActor *activator);
};

IMPLEMENT_STATELESS_ACTOR (ASpark, Any, 9026, 0)
	PROP_Flags (MF_NOSECTOR|MF_NOBLOCKMAP|MF_NOGRAVITY)
END_DEFAULTS

angle_t ASpark::AngleIncrements ()
{
	return ANGLE_1;
}

void ASpark::Activate (AActor *activator)
{
	Super::Activate (activator);
	P_DrawSplash (args[0] ? args[0] : 32, x, y, z, angle, 1);
	S_Sound (this, CHAN_AUTO, "world/spark", 1, ATTN_STATIC);
}
