#include "actor.h"
#include "info.h"
#include "p_local.h"
#include "p_spec.h"
#include "p_enemy.h"
#include "a_action.h"
#include "thingdef/thingdef.h"

//
// A_KeenDie
// DOOM II special, map 32.
// Uses special tag 666.
//
DEFINE_ACTION_FUNCTION(AActor, A_KeenDie)
{
	CALL_ACTION(A_NoBlocking, self);
	
	// scan the remaining thinkers to see if all Keens are dead
	AActor *other;
	TThinkerIterator<AActor> iterator;
	const PClass *matchClass = self->GetClass ();

	while ( (other = iterator.Next ()) )
	{
		if (other != self && other->health > 0 && other->IsA (matchClass))
		{
			// other Keen not dead
			return;
		}
	}

	EV_DoDoor (DDoor::doorOpen, NULL, NULL, 666, 2*FRACUNIT, 0, 0, 0);
}


