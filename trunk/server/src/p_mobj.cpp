// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id:$
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// DESCRIPTION:
//	Moving object handling. Spawn functions.
//
//-----------------------------------------------------------------------------


#include "m_alloc.h"
#include "i_system.h"
#include "z_zone.h"
#include "m_random.h"
#include "doomdef.h"
#include "p_local.h"
#include "p_lnspec.h"
#include "s_sound.h"
#include "doomstat.h"
#include "doomtype.h"
#include "v_video.h"
#include "c_cvars.h"
#include "sv_main.h"
#include "vectors.h"
#include "p_mobj.h"
#include "sv_ctf.h"

mapthing2_t 		itemrespawnque[ITEMQUESIZE];
int 				itemrespawntime[ITEMQUESIZE];
int 				iquehead;
int 				iquetail;

CVAR	(sv_gravity,	"800",		CVAR_ARCHIVE)  // removeme (should always be fixed at this setting)
CVAR	(sv_friction,	"0.90625",	CVAR_ARCHIVE)  // removeme (should always be fixed at this setting)
CVAR	(infiniteheight, "1",		CVAR_ARCHIVE | CVAR_SERVERINFO | CVAR_LATCH)
CVAR	(weaponstay,		"1",		CVAR_ARCHIVE | CVAR_SERVERINFO | CVAR_LATCH)	// Initial weapons wont be removed after picked up when true. - does not work yet

EXTERN_CVAR (chasedemo) // removeme
EXTERN_CVAR (freelook)
EXTERN_CVAR (nomonsters)

IMPLEMENT_SERIAL(AActor, DThinker)

NetIDHandler ServerNetID;

AActor::~AActor ()
{
	// Please avoid calling the destructor directly (or through delete)!
	// Use Destroy() instead.
}

void AActor::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	if (arc.IsStoring ())
	{
		arc << x
			<< y
			<< z
			<< pitch
			<< angle
			<< roll
			<< sprite
			<< frame
			<< effects
			<< floorz
			<< ceilingz
			<< radius
			<< height
			<< momx
			<< momy
			<< momz
			<< type
			<< tics
			<< state
			<< flags
			<< health
			<< movedir
			<< visdir
			<< movecount
			<< target->netid
			<< lastenemy->netid
			<< reactiontime
			<< threshold
			<< player
			<< lastlook
			<< tracer->netid
			<< tid
			<< goal->netid
			<< (unsigned)0
			<< translucency
			<< waterlevel;
		spawnpoint.Serialize (arc);
	}
	else
	{
		unsigned dummy;
		arc >> x
			>> y
			>> z
			>> pitch
			>> angle
			>> roll
			>> sprite
			>> frame
			>> effects
			>> floorz
			>> ceilingz
			>> radius
			>> height
			>> momx
			>> momy
			>> momz
			>> type
			>> tics
			>> state
			>> flags
			>> health
			>> movedir
			>> visdir
			>> movecount
			>> target->netid
			>> lastenemy->netid
			>> reactiontime
			>> threshold
			>> player
			>> lastlook
			>> tracer->netid
			>> tid
			>> goal->netid
			>> dummy
			>> translucency
			>> waterlevel;

		DWORD trans;
		arc >> trans;
		spawnpoint.Serialize (arc);
		info = &mobjinfo[type];
		touching_sectorlist = NULL;
		LinkToWorld ();
		AddToHash ();
	}
}

void MapThing::Serialize (FArchive &arc)
{
	if (arc.IsStoring ())
	{
		arc << thingid << x << y << z << angle << type << flags;
	}
	else
	{
		arc >> thingid >> x >> y >> z >> angle >> type >> flags;
	}
}

AActor::AActor ()
{
	memset (&x, 0, (byte *)&this[1] - (byte *)&x);
	self.init(this);
}

AActor::AActor (const AActor &other)
{
	memcpy (&x, &other.x, (byte *)&this[1] - (byte *)&x);
	self.init(this);
}

AActor &AActor::operator= (const AActor &other)
{
	memcpy (&x, &other.x, (byte *)&this[1] - (byte *)&x);
	return *this;
}

//
//
// P_SetMobjState
//
// Returns true if the mobj is still present.
//
//
BOOL P_SetMobjState (AActor *mobj, statenum_t state)
{
    state_t*	st;
	
	// denis - prevent harmful state cycles
	static unsigned int callstack;
	if(callstack++ > 16)
	{
		callstack = 0;
		I_Error("P_SetMobjState: callstack depth exceeded bounds");
	}
	
    do
    {
		if (state == S_NULL)
		{
			mobj->state = (state_t *) S_NULL;
			mobj->Destroy();
			
			callstack--;
			return false;
		}
		
		st = &states[state];
		mobj->state = st;
		mobj->tics = st->tics;
		mobj->sprite = st->sprite;
		mobj->frame = st->frame;
		
		// Modified handling.
		// Call action functions when the state is set
		if (st->action.acp1)		
			st->action.acp1(mobj);	
		
		state = st->nextstate;
    } while (!mobj->tics);
	
	callstack--;
    return true;
}

//
// P_ExplodeMissile  
//
void P_ExplodeMissile (AActor* mo)
{
	for (size_t i = 0; i < players.size(); i++)
	{
		client_t *cl = &clients[i];

		if(!SV_IsPlayerAllowedToSee(players[i], mo))
			continue;

		MSG_WriteMarker (&cl->reliablebuf, svc_movemobj);
		MSG_WriteShort (&cl->reliablebuf, mo->netid);
		MSG_WriteByte (&cl->reliablebuf, mo->rndindex);
		MSG_WriteLong (&cl->reliablebuf, mo->x);
		MSG_WriteLong (&cl->reliablebuf, mo->y);
		MSG_WriteLong (&cl->reliablebuf, mo->z);

		MSG_WriteMarker(&cl->reliablebuf, svc_explodemissile);
		MSG_WriteShort(&cl->reliablebuf, mo->netid);
	}


	mo->momx = mo->momy = mo->momz = 0;

	P_SetMobjState (mo, mobjinfo[mo->type].deathstate);
	if (mobjinfo[mo->type].deathstate != S_NULL)
	{
		// [RH] If the object is already translucent, don't change it.
		// Otherwise, make it 66% translucent.
		if (mo->translucency == FRACUNIT)
			mo->translucency = TRANSLUC66;

		mo->tics -= P_Random (mo) & 3;

		if (mo->tics < 1)
			mo->tics = 1;

		mo->flags &= ~MF_MISSILE;

		if (mo->info->deathsound)
			S_Sound (mo, CHAN_VOICE, mo->info->deathsound, 1, ATTN_NORM);

		mo->effects = 0;				// [RH]
	}
}

//
// P_XYMovement
//
void P_XYMovement (AActor *mo)
{
//	angle_t angle;
	fixed_t ptryx, ptryy;
	player_t *player = NULL;
	fixed_t xmove, ymove;

	if (!mo->momx && !mo->momy)
	{
		if (mo->flags & MF_SKULLFLY)
		{
			// the skull slammed into something
			mo->flags &= ~MF_SKULLFLY;
			mo->momx = mo->momy = mo->momz = 0;

			P_SetMobjState (mo, mo->info->spawnstate);
		}
		return;
	}

	player = mo->player;

	if(!player || !player->mo)
		player = NULL;

	int maxmove = (mo->waterlevel < 2) || (mo->flags & MF_MISSILE) ? MAXMOVE : MAXMOVE/4;

	if (mo->momx > maxmove)
		mo->momx = maxmove;
	else if (mo->momx < -maxmove)
		mo->momx = -maxmove;

	if (mo->momy > maxmove)
		mo->momy = maxmove;
	else if (mo->momy < -maxmove)
		mo->momy = -maxmove;

	xmove = mo->momx;
	ymove = mo->momy;

	maxmove /= 2;

	do
	{
		if ( xmove > maxmove || ymove > maxmove )
		{
			ptryx = mo->x + xmove/2;
			ptryy = mo->y + ymove/2;
			xmove >>= 1;
			ymove >>= 1;
		}
		else
		{
			ptryx = mo->x + xmove;
			ptryy = mo->y + ymove;
			xmove = ymove = 0;
		}

		// killough 3/15/98: Allow objects to drop off
		if (!P_TryMove (mo, ptryx, ptryy))
		{
			// blocked move
			if (mo->player)
			{	// try to slide along it
				P_SlideMove (mo);
			}
			else if (mo->flags & MF_MISSILE)
			{
				// explode a missile
				if (ceilingline &&
					ceilingline->backsector &&
					ceilingline->backsector->ceilingpic == skyflatnum)
				{
					// Hack to prevent missiles exploding
					// against the sky.
					// Does not handle sky floors.
					mo->Destroy ();
					return;
				}
				P_ExplodeMissile (mo);
			}
			else
			{
				mo->momx = mo->momy = 0;
			}
		}
	} while (xmove || ymove);

    // slow down
	if (player && player->mo == mo && player->cheats & CF_NOMOMENTUM)
	{
		// debug option for no sliding at all
		mo->momx = mo->momy = 0;
		return;
	}

	if (mo->flags & (MF_MISSILE | MF_SKULLFLY))
		return; 	// no friction for missiles ever
	
	if (mo->z > mo->floorz && !mo->waterlevel)
		return;		// no friction when airborne
	
	if (mo->flags & MF_CORPSE)
	{
		// do not stop sliding
		//  if halfway off a step with some momentum
		if (mo->momx > FRACUNIT/4
			|| mo->momx < -FRACUNIT/4
			|| mo->momy > FRACUNIT/4
			|| mo->momy < -FRACUNIT/4)
		{
			if (mo->floorz != mo->subsector->sector->floorheight)
				return;
		}
	}

	// killough 11/98:
	// Stop voodoo dolls that have come to rest, despite any
	// moving corresponding player:
	if (mo->momx > -STOPSPEED && mo->momx < STOPSPEED
		&& mo->momy > -STOPSPEED && mo->momy < STOPSPEED
		&& (!player || (player->mo != mo)
		|| !(player->cmd.ucmd.forwardmove | player->cmd.ucmd.sidemove)))
	{
		// if in a walking frame, stop moving
		// killough 10/98:
		// Don't affect main player when voodoo dolls stop:
		if (player && (unsigned)((player->mo->state - states) - S_PLAY_RUN1) < 4
			&& (player->mo == mo))
		{
			P_SetMobjState (player->mo, S_PLAY);
		}

		mo->momx = mo->momy = 0;
	}
	else
	{
		// phares 3/17/98
		// Friction will have been adjusted by friction thinkers for icy
		// or muddy floors. Otherwise it was never touched and
		// remained set at ORIG_FRICTION
		//
		// killough 8/28/98: removed inefficient thinker algorithm,
		// instead using touching_sectorlist in P_GetFriction() to
		// determine friction (and thus only when it is needed).
		//
		// killough 10/98: changed to work with new bobbing method.
		// Reducing player momentum is no longer needed to reduce
		// bobbing, so ice works much better now.

		fixed_t friction = P_GetFriction (mo, NULL);

		mo->momx = FixedMul (mo->momx, friction);
		mo->momy = FixedMul (mo->momy, friction);
	}
}

//
// P_ZMovement
// joek - from choco with love
//
void P_ZMovement (AActor *mo)
{
   fixed_t	dist;
   fixed_t	delta;
   fixed_t      gravity = (fixed_t)(sv_gravity * mo->subsector->sector->gravity * -163.84);
    
    // check for smooth step up
   if (mo->player && mo->z < mo->floorz)
   {
      mo->player->viewheight -= mo->floorz-mo->z;

      mo->player->deltaviewheight
            = (VIEWHEIGHT - mo->player->viewheight)>>3;
   }
    
    // adjust height
   mo->z += mo->momz;
	
   if ( mo->flags & MF_FLOAT
        && mo->target)
   {
	// float down towards target if too close
      if ( !(mo->flags & MF_SKULLFLY)
             && !(mo->flags & MF_INFLOAT) )
      {
         dist = P_AproxDistance (mo->x - mo->target->x,
                                 mo->y - mo->target->y);
	    
         delta =(mo->target->z + (mo->height>>1)) - mo->z;

         if (delta<0 && dist < -(delta*3) )
            mo->z -= FLOATSPEED;
         else if (delta>0 && dist < (delta*3) )
            mo->z += FLOATSPEED;			
      }
	
   }
    
    // clip movement
   if (mo->z <= mo->floorz)
   {
	// hit the floor

	// Note (id):
	//  somebody left this after the setting momz to 0,
	//  kinda useless there.
      //
	// cph - This was the a bug in the linuxdoom-1.10 source which
	//  caused it not to sync Doom 2 v1.9 demos. Someone
	//  added the above comment and moved up the following code. So
	//  demos would desync in close lost soul fights.
	// Note that this only applies to original Doom 1 or Doom2 demos - not
	//  Final Doom and Ultimate Doom.  So we test demo_compatibility *and*
	//  gamemission. (Note we assume that Doom1 is always Ult Doom, which
	//  seems to hold for most published demos.)
      //  
        //  fraggle - cph got the logic here slightly wrong.  There are three
        //  versions of Doom 1.9:
      //
        //  * The version used in registered doom 1.9 + doom2 - no bounce
        //  * The version used in ultimate doom - has bounce
        //  * The version used in final doom - has bounce
      //
        // So we need to check that this is either retail or commercial
        // (but not doom2)
	
      /* joek - we don't have any of this crap. Looks like souls will bounce wrongly :/
      int correct_lost_soul_bounce = gameversion >= exe_ultimate;

      if (correct_lost_soul_bounce && mo->flags & MF_SKULLFLY)
      {
	    // the skull slammed into something
      mo->momz = -mo->momz;
   }*/
	
      if (mo->momz < 0)
      {
         if (mo->player
             && mo->momz < gravity*8)	
         {
		// Squat down.
		// Decrease viewheight for a moment
		// after hitting the ground (hard),
		// and utter appropriate sound.
            mo->player->deltaviewheight = mo->momz>>3;
            S_Sound (mo, CHAN_AUTO, "*land1", 1, ATTN_NORM);
         }
         mo->momz = 0;
      }
      mo->z = mo->floorz;


	// cph 2001/05/26 -
	// See lost soul bouncing comment above. We need this here for bug
	// compatibility with original Doom2 v1.9 - if a soul is charging and
	// hit by a raising floor this incorrectly reverses its Y momentum.
      //

      //if (!correct_lost_soul_bounce && mo->flags & MF_SKULLFLY)
      //   mo->momz = -mo->momz;

      if ( (mo->flags & MF_MISSILE)
            && !(mo->flags & MF_NOCLIP) )
      {
         P_ExplodeMissile (mo);
         return;
      }
   }
   else if (! (mo->flags & MF_NOGRAVITY) )
   {
      if (mo->momz == 0)
         mo->momz = gravity;
      else
         mo->momz -= (gravity/-2);
   }
	
   if (mo->z + mo->height > mo->ceilingz)
   {
	// hit the ceiling
      if (mo->momz > 0)
         mo->momz = 0;
      {
         mo->z = mo->ceilingz - mo->height;
      }

      if (mo->flags & MF_SKULLFLY)
      {	// the skull slammed into something
         mo->momz = -mo->momz;
      }
	
      if ( (mo->flags & MF_MISSILE)
            && !(mo->flags & MF_NOCLIP) )
      {
         P_ExplodeMissile (mo);
         return;
      }
   }
} 

//
// P_NightmareRespawn
//
void P_NightmareRespawn (AActor *mobj)
{
    fixed_t		x;
    fixed_t		y;
    fixed_t		z; 
    subsector_t*	ss; 
    mapthing2_t*		mthing;
	
    x = mobj->spawnpoint.x << FRACBITS; 
    y = mobj->spawnpoint.y << FRACBITS; 
	
    // somthing is occupying it's position?
    if (!P_CheckPosition (mobj, x, y) ) 
		return;	// no respwan
	
    // spawn a teleport fog at old spot
    // because of removal of the body?
	new AActor (mobj->x,
					  mobj->y,
					  mobj->subsector->sector->floorheight , MT_TFOG); 
	
    // spawn a teleport fog at the new spot
    ss = R_PointInSubsector (x,y); 
	
    new AActor (x, y, ss->sector->floorheight , MT_TFOG); 
	
    // spawn the new monster
    mthing = &mobj->spawnpoint;
	
    // spawn it
    if (mobj->info->flags & MF_SPAWNCEILING)
		z = ONCEILINGZ;
    else
		z = ONFLOORZ;
	
    // inherit attributes from deceased one
	if(serverside)
	{
		AActor *mo = new AActor (x,y,z, mobj->type);
		mo->spawnpoint = mobj->spawnpoint;	
		mo->angle = ANG45 * (mthing->angle/45);
		
	    if (mthing->flags & MTF_AMBUSH)
			mo->flags |= MF_AMBUSH;
			
		SV_SpawnMobj(mo);
		
		mo->reactiontime = 18;
	}
	
    // remove the old monster,
	mobj->Destroy();
}

//
// [RH] Some new functions to work with Thing IDs. ------->
//
AActor *AActor::TIDHash[128];

//
// P_ClearTidHashes
//
// Clears the tid hashtable.
//

void AActor::ClearTIDHashes ()
{
	int i;

	for (i = 0; i < 128; i++)
		TIDHash[i] = NULL;
}

//
// P_AddMobjToHash
//
// Inserts an mobj into the correct chain based on its tid.
// If its tid is 0, this function does nothing.
//
void AActor::AddToHash ()
{
	if (tid == 0)
	{
		inext = iprev = NULL;
		return;
	}
	else
	{
		int hash = TIDHASH (tid);

		inext = TIDHash[hash];
		iprev = NULL;
		TIDHash[hash] = this;
	}
}

//
// P_RemoveMobjFromHash
//
// Removes an mobj from its hash chain.
//
void AActor::RemoveFromHash ()
{
	if (tid == 0)
		return;
	else
	{
		if (iprev == NULL)
		{
			// First mobj in the chain (probably)
			int hash = TIDHASH(tid);

			if (TIDHash[hash] == this)
				TIDHash[hash] = inext;
			if (inext)
			{
				inext->iprev = NULL;
				inext = NULL;
			}
		}
		else
		{
			// Not the first mobj in the chain
			iprev->inext = inext;
			if (inext)
			{
				inext->iprev = iprev;
				inext = NULL;
			}
			iprev = NULL;
		}
	}
}

//
// P_FindMobjByTid
//
// Returns the next mobj with the tid after the one given,
// or the first with that tid if no mobj is passed. Returns
// NULL if there are no more.
//
AActor *AActor::FindByTID (int tid) const
{
	return FindByTID (this, tid);
}

AActor *AActor::FindByTID (const AActor *actor, int tid)
{
	// Mobjs without tid are never stored.
	if (tid == 0)
		return NULL;

	if (!actor)
		actor = TIDHash[TIDHASH(tid)];
	else
		actor = actor->inext;

	while (actor && actor->tid != tid)
		actor = actor->inext;

	return const_cast<AActor *>(actor);
}

//
// P_FindGoal
//
// Like FindByTID except it also matches on type.
//
AActor *AActor::FindGoal (int tid, int kind) const
{
	return FindGoal (this, tid, kind);
}

AActor *AActor::FindGoal (const AActor *actor, int tid, int kind)
{
	do
	{
		actor = FindByTID (actor, tid);
	} while (actor && actor->type != kind);

	return const_cast<AActor *>(actor);
}

// <------- [RH] End new functions

EXTERN_CVAR(sv_speedhackfix)

//
// P_MobjThinker
//
void AActor::RunThink ()
{
	if (type == MT_PLAYER && health <= 0)
		deadtic++;

	// remove dead players but don't tell clients about it
	if (type == MT_PLAYER && !player && deadtic >= REMOVECOPRSESTIC)
	{
		this->Destroy();
		return;
	}
	
	// [RH] Fade a stealth monster in and out of visibility
	if (visdir > 0)
	{
		translucency += 2*FRACUNIT/TICRATE;
		if (translucency > FRACUNIT)
		{
			translucency = FRACUNIT;
			visdir = 0;
		}
	}
	else if (visdir < 0)
	{
		translucency -= 3*FRACUNIT/TICRATE/2;
		if (translucency < 0)
		{
			translucency = 0;
			visdir = 0;
		}
	}


	// Handle X and Y momemtums
	if (momx || momy || (flags & MF_SKULLFLY))
	{
		P_XYMovement (this);

		if (ObjectFlags & OF_MassDestruction)
			return; 		// actor was destroyed
	}

	if ((z != floorz) || momz)
	{
		P_ZMovement (this);

		if (ObjectFlags & OF_MassDestruction)
			return; 		// actor was destroyed
	}

	if(subsector)
	{
		//byte lastwaterlevel = waterlevel;
		waterlevel = 0;
		if (subsector->sector->waterzone)
			waterlevel = 3;
		sector_t *hsec;
		if ( (hsec = subsector->sector->heightsec) )
		{
			if (hsec->waterzone && !subsector->sector->waterzone)
			{
				if (z < hsec->floorheight)
				{
					waterlevel = 1;
					if (z + height/2 < hsec->floorheight)
					{
						waterlevel = 2;
						if (z + height <= hsec->floorheight)
							waterlevel = 3;
					}
				}
				else if (z + height > hsec->ceilingheight)
				{
					waterlevel = 3;
				}
			}
		}
	}

    // cycle through states,
    // calling action functions at transitions
	if (tics != -1)
	{
		tics--;

		// you can cycle through multiple states in a tic
		if (!tics)
			if (!P_SetMobjState (this, state->nextstate) )
				return; 				// freed itself
	}
	else
	{
		// check for nightmare respawn
		if (!(flags & MF_COUNTKILL) || !respawnmonsters)
			return;

		movecount++;

		if (movecount < 12*TICRATE)
			return;

		if (level.time & 31)
			return;

		if (P_Random (this) > 4)
			return;

		P_NightmareRespawn (this);
	}
}

AActor::AActor (fixed_t ix, fixed_t iy, fixed_t iz, mobjtype_t itype)
{
	state_t *st;

	// Fly!!! fix it in P_RespawnSpecial
	if ((unsigned int)itype >= NUMMOBJTYPES)
	{
		I_Error ("Tried to spawn actor type %d\n", itype);
	}

	memset (&x, 0, (byte *)&this[1] - (byte *)&x);
	self.init(this);
	info = &mobjinfo[itype];
	type = itype;
	x = ix;
	y = iy;
	radius = info->radius;
	height = info->height;
	flags = info->flags;
	health = info->spawnhealth;
	translucency = info->translucency;
	rndindex = M_Random();

	netid = ServerNetID.ObtainNetID();

	if (skill != sk_nightmare)
		reactiontime = info->reactiontime;

	lastlook = P_Random () % MAXPLAYERS;

    // do not set the state with P_SetMobjState,
    // because action routines can not be called yet
	st = &states[info->spawnstate];
	state = st;
	tics = st->tics;
	sprite = st->sprite;
	frame = st->frame;
	touching_sectorlist = NULL; 	// NULL head of sector list // phares 3/13/98

	// set subsector and/or block links
	LinkToWorld ();
	floorz = subsector->sector->floorheight;
	ceilingz = subsector->sector->ceilingheight;

	if (iz == ONFLOORZ)
	{
		z = floorz;
	}
	else if (iz == ONCEILINGZ)
	{
		z = ceilingz - height;
	}
	else
	{
		z = iz;
	}
}


//
// P_RemoveMobj
//
void AActor::Destroy ()
{
	if (netid && type != MT_PUFF)
	{
		for (size_t i = 0; i < players_aware.size(); i++)
		{
			client_t *cl = &idplayer(players_aware[i]).client;

			MSG_WriteMarker(&cl->reliablebuf, svc_removemobj); // denis - todo - need a queue for destroyed (lost awareness) objects, as a flood of destroyed things could easily overflow a buffer
			MSG_WriteShort(&cl->reliablebuf, netid);
		}
	}

	// AActor no longer active. NetID released.
	if(netid)
		ServerNetID.ReleaseNetID( netid );

	if ((flags & MF_SPECIAL) && !(flags & MF_DROPPED))
	{
		if (type != MT_INV && type != MT_INS)
		{
			itemrespawnque[iquehead] = spawnpoint;
			itemrespawntime[iquehead] = level.time;
			iquehead = (iquehead+1)&(ITEMQUESIZE-1);
		}

		// lose one off the end?
		if (iquehead == iquetail)
			iquetail = (iquetail+1)&(ITEMQUESIZE-1);
	}

	// [RH] Unlink from tid chain
	RemoveFromHash ();

	// unlink from sector and block lists
	UnlinkFromWorld ();

	// Delete all nodes on the current sector_list					phares 3/16/98
	if (sector_list)
	{
		P_DelSeclist (sector_list);
		sector_list = NULL;
	}

	// stop any playing sound
	//	S_RelinkSound (this, NULL);
	
	// Zero all pointers generated by this->ptr()
	self.update_all(NULL);

	Super::Destroy ();
}

EXTERN_CVAR (itemsrespawn)


//
// P_RespawnSpecials
//
void P_RespawnSpecials (void)
{
	fixed_t 						x;
	fixed_t 						y;
	fixed_t 						z;

	subsector_t*			ss;
	AActor* 						mo;
	mapthing2_t*			mthing;

	int 							i;

	if(!serverside)
		return;

	// only respawn items in deathmatch
	if (!itemsrespawn)
		return;

	// nothing left to respawn?
	if (iquehead == iquetail)
		return;

	// wait at least 30 seconds
	if (level.time - itemrespawntime[iquetail] < 30*TICRATE)
		return;

	mthing = &itemrespawnque[iquetail];

	x = mthing->x << FRACBITS;
	y = mthing->y << FRACBITS;

	// find which type to spawn
	for (i=0 ; i< NUMMOBJTYPES ; i++)
	{
		if (mthing->type == mobjinfo[i].doomednum)
			break;
	}

	// [Fly] crashes sometimes without it
	if (i >= NUMMOBJTYPES)
	{
		// pull it from the que
		iquetail = (iquetail+1)&(ITEMQUESIZE-1);
		return;
	}

	if (mobjinfo[i].flags & MF_SPAWNCEILING)
		z = ONCEILINGZ;
	else
		z = ONFLOORZ;

	// spawn a teleport fog at the new spot
	ss = R_PointInSubsector (x, y);
	mo = new AActor (x, y, z, MT_IFOG);
	SV_SpawnMobj(mo);
	S_Sound (mo, CHAN_VOICE, "misc/spawn", 1, ATTN_IDLE);

	// spawn it
	mo = new AActor (x, y, z, (mobjtype_t)i);
	mo->spawnpoint = *mthing;
	mo->angle = ANG45 * (mthing->angle/45);

	if (z == ONFLOORZ)
		mo->z += mthing->z << FRACBITS;
	else if (z == ONCEILINGZ)
		mo->z -= mthing->z << FRACBITS;

	// pull it from the que
	iquetail = (iquetail+1)&(ITEMQUESIZE-1);

	SV_SpawnMobj(mo);
}

//
// P_SpawnPlayer
// Called when a player is spawned on the level.
// Most of the player structure stays unchanged
//		between levels.
//
void P_SpawnPlayer (player_t &player, mapthing2_t *mthing)
{
	// denis - clients should not control spawning
	if(!serverside)
		return;

	player_t *p = &player;

	if (p->playerstate == PST_REBORN)
		G_PlayerReborn (*p);

	AActor *mobj = new AActor (mthing->x << FRACBITS, mthing->y << FRACBITS, ONFLOORZ, MT_PLAYER);
	// set color translations for player sprites
	// [RH] Different now: MF_TRANSLATION is not used.
	//		  mobj->translation = translationtables + 256*playernum;

	mobj->angle = ANG45 * (mthing->angle/45);
	mobj->pitch = mobj->roll = 0;
	mobj->player = p;
	mobj->health = p->health;

	// [RH] Set player sprite based on skin
	if(p->userinfo.skin >= numskins)
		p->userinfo.skin = 0;

	mobj->sprite = skins[p->userinfo.skin].sprite;

	p->fov = 90.0f;
	p->camera = p->mo = mobj->ptr();
	p->playerstate = PST_LIVE;
	p->refire = 0;
	p->damagecount = 0;
	p->bonuscount = 0;
	p->extralight = 0;
	p->fixedcolormap = 0;
	p->viewheight = VIEWHEIGHT;
	p->attacker = AActor::AActorPtr();

	// [RH] Allow chasecam for demo watching
	if ((demoplayback || demonew) && chasedemo)
		p->cheats = CF_CHASECAM;

	// setup gun psprite
	P_SetupPsprites (p);

	// give all cards in death match mode
	if (deathmatch)
	{
		for (int i = 0; i < NUMCARDS; i++)
			p->cards[i] = true;
	}

	// [RH] If someone is in the way, kill them
	P_TeleportMove (mobj, mobj->x, mobj->y, mobj->z, true);

	// send new player object
	SV_SpawnMobj(mobj);

	// send teleport fog
	AActor *tfog = new AActor (mobj->x, mobj->y, mobj->z, MT_TFOG);
	SV_SpawnMobj(tfog);

	SV_Sound (mobj->x, mobj->y, CHAN_VOICE, "misc/teleport", ATTN_NORM);
}


//
// P_SpawnMapThing
// The fields of the mapthing should
// already be in host byte order.
//
// [RH] position is used to weed out unwanted start spots
bool unnatural_level_progression;
void P_SpawnMapThing (mapthing2_t *mthing, int position)
{
	int i;
	int bit;
	AActor *mobj;
	fixed_t x, y, z;

	if (mthing->type == 0 || mthing->type == -1)
		return;

	// count deathmatch start positions
	if (mthing->type == 11)
	{
		if (deathmatch_p == &deathmatchstarts[MaxDeathmatchStarts])
		{
			// [RH] Get more deathmatchstarts
			int offset = MaxDeathmatchStarts;
			MaxDeathmatchStarts *= 2;
			deathmatchstarts = (mapthing2_t *)Realloc (deathmatchstarts, MaxDeathmatchStarts * sizeof(mapthing2_t));
			deathmatch_p = &deathmatchstarts[offset];
		}
		memcpy (deathmatch_p, mthing, sizeof(*mthing));
		deathmatch_p++;
		return;
	}

	// [Toke - CTF - starts] CTF starts - count Blue team start positions
	if (mthing->type == 5080)
	{
		if (blueteam_p == &blueteamstarts[MaxBlueTeamStarts])
		{
			int offset = MaxBlueTeamStarts;
			MaxBlueTeamStarts *= 2;
			blueteamstarts = (mapthing2_t *)Realloc (blueteamstarts, MaxBlueTeamStarts * sizeof(mapthing2_t));
			blueteam_p = &blueteamstarts[offset];
		}
		memcpy (blueteam_p, mthing, sizeof(*mthing));
		blueteam_p++;
		return;
	}

	// [Toke - CTF - starts] CTF starts - count Red team start positions
	if (mthing->type == 5081)
	{
		if (redteam_p == &redteamstarts[MaxRedTeamStarts])
		{
			int offset = MaxRedTeamStarts;
			MaxRedTeamStarts *= 2;
			redteamstarts = (mapthing2_t *)Realloc (redteamstarts, MaxRedTeamStarts * sizeof(mapthing2_t));
			redteam_p = &redteamstarts[offset];
		}
		memcpy (redteam_p, mthing, sizeof(*mthing));
		redteam_p++;
		return;
	}

	// [Toke - CTF - starts] CTF starts - count Gold team start positions
	if (mthing->type == 5082)
	{
		if (goldteam_p == &goldteamstarts[MaxGoldTeamStarts])
		{
			int offset = MaxGoldTeamStarts;
			MaxGoldTeamStarts *= 2;
			goldteamstarts = (mapthing2_t *)Realloc (goldteamstarts, MaxGoldTeamStarts * sizeof(mapthing2_t));
			goldteam_p = &goldteamstarts[offset];
		}
		memcpy (goldteam_p, mthing, sizeof(*mthing));
		goldteam_p++;
		return;
	}

	// check for players specially
	if ((mthing->type <= 4 && mthing->type > 0)
		|| (mthing->type >= 4001 && mthing->type <= 4001 + MAXPLAYERSTARTS - 4))
	{
		// [RH] Only spawn spots that match position.
		if (mthing->args[0] != position)
			return;

		// save spots for respawning in network games
		size_t playernum = mthing->type <= 4 ? mthing->type-1 : (mthing->type - 4001 + 4)%MAXPLAYERSTARTS;
		playerstarts.push_back(*mthing);
		player_t &p = idplayer(playernum+1);

		if (!deathmatch &&
			(validplayer(p) && p.ingame()))
		{
			if(unnatural_level_progression)
			{
				P_SpawnPlayer (p, mthing);
				return;
			}

			// denis - carry weapons and keys over to next level
			int			weaponowned[NUMWEAPONS];
			int			ammo[NUMAMMO];
			int			maxammo[NUMAMMO];
			weapontype_t readyweapon;
			bool		backpack;

			memcpy(weaponowned, p.weaponowned, sizeof(weaponowned));
			memcpy(ammo, p.ammo, sizeof(ammo));
			memcpy(maxammo, p.maxammo, sizeof(maxammo));
			readyweapon = p.readyweapon;
			backpack = p.backpack;

			P_SpawnPlayer (p, mthing);

			memcpy(p.weaponowned, weaponowned, sizeof(weaponowned));
			memcpy(p.ammo, ammo, sizeof(ammo));
			memcpy(p.maxammo, maxammo, sizeof(maxammo));
			p.readyweapon = readyweapon;
			p.backpack = backpack;

			// inform client
			{
				size_t j;
				client_t *cl = &p.client;

				MSG_WriteMarker (&cl->reliablebuf, svc_playerinfo);

				for(j = 0; j < NUMWEAPONS; j++)
					MSG_WriteByte (&cl->reliablebuf, p.weaponowned[j]);

				for(j = 0; j < NUMAMMO; j++)
				{
					MSG_WriteShort (&cl->reliablebuf, p.maxammo[j]);
					MSG_WriteShort (&cl->reliablebuf, p.ammo[j]);
				}
				
				MSG_WriteByte (&cl->reliablebuf, p.health);
				MSG_WriteByte (&cl->reliablebuf, p.armorpoints);
				MSG_WriteByte (&cl->reliablebuf, p.armortype);
				MSG_WriteByte (&cl->reliablebuf, p.readyweapon);
				MSG_WriteByte (&cl->reliablebuf, p.backpack);
			}
		}

		return;
	}
	
	// [Toke - CTF] Setup flag sockets
	if (mthing->type >= ID_BLUE_FLAG && mthing->type <= ID_GOLD_FLAG) // Check for items with flag socks ID's
	{
		if (mthing->type == ID_BLUE_FLAG)
		{
			Printf (PRINT_HIGH, "Map contains BLUE FLAG, enabling TEAM BLUE \n");
			
			CTF_Load ();
			TEAMenabled[it_blueflag] = true;
			
			CTF_RememberFlagPos (mthing);
		}
		
		if (mthing->type == ID_RED_FLAG)
		{
			Printf (PRINT_HIGH, "Map contains RED FLAG, enabling TEAM RED \n");
			
			CTF_Load ();
			TEAMenabled[it_redflag] = true;
			
			CTF_RememberFlagPos (mthing);
		}
		
		if (mthing->type == ID_GOLD_FLAG)
		{
			Printf (PRINT_HIGH, "Map contains GOLD FLAG, enabling TEAM GOLD \n");
			
			CTF_Load ();
			TEAMenabled[it_goldflag] = true;
			
			CTF_RememberFlagPos (mthing);
		}
	}

	// [RH] sound sequence overrides
	if (mthing->type >= 1400 && mthing->type < 1410)
	{
		return;
	}
	else if (mthing->type == 1411)
	{
		int type;

		if (mthing->args[0] == 255)
			type = -1;
		else
			type = mthing->args[0];

		if (type > 63)
		{
			Printf (PRINT_HIGH, "Sound sequence %d out of range\n", type);
		}
		else
		{
		}
		return;
	}

	if (deathmatch)
	{
		if (!(mthing->flags & MTF_DEATHMATCH))
			return;
	}
	else if (multiplayer)
	{
		if (!(mthing->flags & MTF_COOPERATIVE))
			return;
	}
	else
	{
		if (!(mthing->flags & MTF_SINGLE))
			return;
	}

	// check for apropriate skill level
	if (skill == sk_baby)
		bit = 1;
	else if (skill == sk_nightmare)
		bit = 4;
	else
		bit = 1 << ((int)skill - 2);

	if (!(mthing->flags & bit))
		return;

	// [RH] Determine if it is an old ambient thing, and if so,
	//				map it to MT_AMBIENT with the proper parameter.
	if (mthing->type >= 14001 && mthing->type <= 14064)
	{
		mthing->args[0] = mthing->type - 14000;
		mthing->type = 14065;
		i = MT_AMBIENT;
	}
	// [RH] Check if it's a particle fountain
	else if (mthing->type >= 9027 && mthing->type <= 9033)
	{
		mthing->args[0] = mthing->type - 9026;
		i = MT_FOUNTAIN;
	}
	else
	{
		// find which type to spawn
		for (i = 0; i < NUMMOBJTYPES; i++)
			if (mthing->type == mobjinfo[i].doomednum)
				break;
	}

	if (i >= NUMMOBJTYPES)
	{
		// [RH] Don't die if the map tries to spawn an unknown thing
		Printf (PRINT_HIGH, "Unknown type %i at (%i, %i)\n",
			mthing->type,
			mthing->x, mthing->y);
		i = MT_UNKNOWNTHING;
	}
	// [RH] If the thing's corresponding sprite has no frames, also map
	//				it to the unknown thing.
	else if (sprites[states[mobjinfo[i].spawnstate].sprite].numframes == 0)
	{
		Printf (PRINT_HIGH, "Type %i at (%i, %i) has no frames\n",
			mthing->type, mthing->x, mthing->y);
		i = MT_UNKNOWNTHING;
	}

	// don't spawn keycards and players in deathmatch
	if (deathmatch && mobjinfo[i].flags & MF_NOTDMATCH)
		return;

	// don't spawn deathmatch weapons in offline single player mode
	if (!multiplayer)
	{
		switch (i)
		{
		case MT_CHAINGUN:
		case MT_SHOTGUN:
		case MT_SUPERSHOTGUN:
		case MT_MISC25: 		// BFG
		case MT_MISC26: 		// chainsaw
		case MT_MISC27: 		// rocket launcher
		case MT_MISC28: 		// plasma gun
			if ((mthing->flags & (MTF_DEATHMATCH|MTF_SINGLE)) == MTF_DEATHMATCH)
				return;
			break;
		default:
			break;
		}
	}

	if (nomonsters)
		if (i == MT_SKULL || (mobjinfo[i].flags & MF_COUNTKILL) )
			return;

	// spawn it
	x = mthing->x << FRACBITS;
	y = mthing->y << FRACBITS;

	if (i == MT_WATERZONE)
	{
		sector_t *sec = R_PointInSubsector (x, y)->sector;
		sec->waterzone = 1;
		return;
	}
	else if (i == MT_SECRETTRIGGER)
	{
		level.total_secrets++;
	}

	if (mobjinfo[i].flags & MF_SPAWNCEILING)
		z = ONCEILINGZ;
	else
		z = ONFLOORZ;

	mobj = new AActor (x, y, z, (mobjtype_t)i);

	if (z == ONFLOORZ)
		mobj->z += mthing->z << FRACBITS;
	else if (z == ONCEILINGZ)
		mobj->z -= mthing->z << FRACBITS;
	mobj->spawnpoint = *mthing;

	if (mobj->tics > 0)
		mobj->tics = 1 + (P_Random () % mobj->tics);
	if (mobj->flags & MF_COUNTKILL)
		level.total_monsters++;
	if (mobj->flags & MF_COUNTITEM)
		level.total_items++;

	if (i != MT_SPARK)
		mobj->angle = ANG45 * (mthing->angle/45);

	if (mthing->flags & MTF_AMBUSH)
		mobj->flags |= MF_AMBUSH;

	// [RH] Add ThingID to mobj and link it in with the others
	mobj->tid = mthing->thingid;
	mobj->AddToHash ();

	SV_SpawnMobj(mobj);

	// [RH] Go dormant as needed
//	if (mthing->flags & MTF_DORMANT)
//		P_DeactivateMobj (mobj);
}

//
// GAME SPAWN FUNCTIONS
//

//
// P_SpawnPuff
//
void P_SpawnPuff (fixed_t x, fixed_t y, fixed_t z, angle_t dir, int updown)
{
	// denis - not clientside
	if(!serverside)
		return;

        AActor *th;

	z += (P_Random () - P_Random ()) << 10;

        th = new AActor (x, y, z, MT_PUFF);
        th->momz = FRACUNIT;
        th->tics -= P_Random (th) & 3;

        if (th->tics < 1)
               th->tics = 1;

	// don't make punches spark on the wall
	if (attackrange == MELEERANGE)
           P_SetMobjState (th, S_PUFF3);

        SV_SpawnMobj(th);
}

//
// P_SpawnBlood
//
void P_SpawnBlood (fixed_t x, fixed_t y, fixed_t z, angle_t dir, int damage)
{
	// denis - not clientside
	if(!serverside)
		return;

	AActor *th;

	z += (P_Random () - P_Random ()) << 10;
	th = new AActor (x, y, z, MT_BLOOD);
	th->momz = FRACUNIT*2;
	th->tics -= P_Random (th) & 3;

	if (th->tics < 1)
		th->tics = 1;

	if (damage <= 12 && damage >= 9)
		P_SetMobjState (th, S_BLOOD2);
	else if (damage < 9)
		P_SetMobjState (th, S_BLOOD3);

	SV_SpawnMobj(th);
}

//
// P_CheckMissileSpawn
// Moves the missile forward a bit
//		and possibly explodes it right there.
//
BOOL P_CheckMissileSpawn (AActor* th)
{
	th->tics -= P_Random (th) & 3;
	if (th->tics < 1)
		th->tics = 1;

	// move a little forward so an angle can
	// be computed if it immediately explodes
	th->x += th->momx>>1;
	th->y += th->momy>>1;
	th->z += th->momz>>1;

	// killough 3/15/98: no dropoff (really = don't care for missiles)

	if (!P_TryMove (th, th->x, th->y))
	{
		P_ExplodeMissile (th);
		return false;
	}
	return true;
}

//
// P_SpawnMissile
//
AActor *P_SpawnMissile (AActor *source, AActor *dest, mobjtype_t type)
{
	if(!dest)
		return 0;

	AActor *th = new AActor (source->x, source->y, source->z + 4*8*FRACUNIT, type);

	if (th->info->seesound)
		S_Sound (th, CHAN_VOICE, th->info->seesound, 1, ATTN_NORM);

	th->target = source->ptr();			// where it came from

	vec3_t velocity;
	float speed = FIXED2FLOAT (th->info->speed);

	velocity[0] = FIXED2FLOAT(dest->x - source->x);
	velocity[1] = FIXED2FLOAT(dest->y - source->y);
	velocity[2] = FIXED2FLOAT(dest->z - source->z);
	VectorNormalize (velocity);
	th->momx = FLOAT2FIXED(velocity[0] * speed);
	th->momy = FLOAT2FIXED(velocity[1] * speed);
	th->momz = FLOAT2FIXED(velocity[2] * speed);

	// fuzzy player: rotate velocity vector in 2D
	if (dest->flags & MF_SHADOW)
	{
		angle_t an = (P_Random () - P_Random ()) << 20;
		an >>= ANGLETOFINESHIFT;

		fixed_t newx = FixedMul (th->momx, finecosine[an]) - FixedMul (th->momy, finesine[an]);
		fixed_t newy = FixedMul (th->momx, finesine[an]) + FixedMul (th->momy, finecosine[an]);
		th->momx = newx;
		th->momy = newy;
	}

	th->angle = R_PointToAngle2 (0, 0, th->momx, th->momy);

	P_CheckMissileSpawn (th);

	SV_SpawnMobj(th);

	return th;
}

//
// P_SpawnPlayerMissile
// Tries to aim at a nearby monster
//
void P_SpawnPlayerMissile (AActor *source, mobjtype_t type)
{
	if(!serverside)
		return;

	angle_t an;
	fixed_t slope;
	fixed_t pitchslope = finetangent[FINEANGLES/4-(source->pitch>>ANGLETOFINESHIFT)];

	// see which target is to be aimed at
	an = source->angle;

	if (source->player &&
		source->player->userinfo.aimdist == 0 &&
		freelook)
	{
		slope = pitchslope;
	}
	else
	{
		slope = P_AimLineAttack (source, an, 16*64*FRACUNIT);

		if (!linetarget)
		{
			an += 1<<26;
			slope = P_AimLineAttack (source, an, 16*64*FRACUNIT);

			if (!linetarget)
			{
				an -= 2<<26;
				slope = P_AimLineAttack (source, an, 16*64*FRACUNIT);
			}

			if (!linetarget)
			{
				an = source->angle;
				
				if(freelook)
					slope = pitchslope;
				else
					slope = 0;
			}
		}

		if (linetarget && source->player)
		{
			if (freelook && abs(slope - pitchslope) > source->player->userinfo.aimdist)
			{
				an = source->angle;
				slope = pitchslope;
			}
		}
	}

	AActor *th = new AActor (source->x, source->y, source->z + 4*8*FRACUNIT, type);

	fixed_t speed = th->info->speed;

	th->target = source->ptr();
	th->angle = an;
    th->momx = FixedMul(speed, finecosine[an>>ANGLETOFINESHIFT]);
    th->momy = FixedMul(speed, finesine[an>>ANGLETOFINESHIFT]);
    th->momz = FixedMul(speed, slope);
	
	SV_SpawnMobj(th);

	if (th->info->seesound)
		S_Sound (th, CHAN_VOICE, th->info->seesound, 1, ATTN_NORM);

	P_CheckMissileSpawn (th);
}

