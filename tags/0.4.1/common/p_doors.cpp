// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id$
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
//	Door animation code (opening/closing)
//	[RH] Removed sliding door code and simplified for Hexen-ish specials
//
//-----------------------------------------------------------------------------


#include "z_zone.h"
#include "doomdef.h"
#include "p_local.h"
#include "s_sound.h"
#include "doomstat.h"
#include "r_state.h"
#include "dstrings.h"
#include "c_console.h"

IMPLEMENT_SERIAL (DDoor, DMovingCeiling)

DDoor::DDoor ()
{
}

void DDoor::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	if (arc.IsStoring ())
	{
		arc << m_Type
			<< m_TopHeight
			<< m_Speed
			<< m_Direction
			<< m_TopWait
			<< m_TopCountdown;
	}
	else
	{
		arc >> m_Type
			>> m_TopHeight
			>> m_Speed
			>> m_Direction
			>> m_TopWait
			>> m_TopCountdown;
	}
}


//
// VERTICAL DOORS
//

//
// T_VerticalDoor
//
void DDoor::RunThink ()
{
	EResult res;
		
	switch (m_Direction)
	{
	case 0:
		// WAITING
		if (!--m_TopCountdown)
		{
			switch (m_Type)
			{
			case doorRaise:
				m_Direction = -1; // time to go back down
				DoorSound (false);
				break;
				
			case doorCloseWaitOpen:
				m_Direction = 1;
				DoorSound (true);
				break;
				
			default:
				break;
			}
		}
		break;
		
	case 2:
		//	INITIAL WAIT
		if (!--m_TopCountdown)
		{
			switch (m_Type)
			{
			case doorRaiseIn5Mins:
				m_Direction = 1;
				m_Type = doorRaise;
				DoorSound (true);
				break;
				
			default:
				break;
			}
		}
		break;
		
	case -1:
		// DOWN
		res = MoveCeiling (m_Speed, m_Sector->floorheight, false, m_Direction);
		if (res == pastdest)
		{
			//S_StopSound (m_Sector->soundorg);
			switch (m_Type)
			{
			case doorRaise:
			case doorClose:
				m_Sector->ceilingdata = NULL;	//jff 2/22/98
				Destroy ();						// unlink and free
				break;
				
			case doorCloseWaitOpen:
				m_Direction = 0;
				m_TopCountdown = m_TopWait;
				break;
				
			default:
				break;
			}
		}
		else if (res == crushed)
		{
			switch (m_Type)
			{
			case doorClose:				// DO NOT GO BACK UP!
				break;
				
			default:
				m_Direction = 1;
				DoorSound (true);
				break;
			}
		}
		break;
		
	case 1:
		// UP
		res = MoveCeiling (m_Speed, m_TopHeight, false, m_Direction);
		
		if (res == pastdest)
		{
			//S_StopSound (m_Sector->soundorg);
			switch (m_Type)
			{
			case doorRaise:
				m_Direction = 0; // wait at top
				m_TopCountdown = m_TopWait;
				break;
				
			case doorCloseWaitOpen:
			case doorOpen:
				m_Sector->ceilingdata = NULL;	//jff 2/22/98
				Destroy ();						// unlink and free
				break;
				
			default:
				break;
			}
		}
		break;
	}
}

// [RH] DoorSound: Plays door sound depending on direction and speed
void DDoor::DoorSound (bool raise) const
{
	const char *snd;

	if (raise)
	{
		if((m_Speed >= FRACUNIT*8))
			snd = "doors/dr2_open";
		else
			snd = "doors/dr1_open";
	}
	else
	{
		if((m_Speed >= FRACUNIT*8))
			snd = "doors/dr2_clos";
		else
			snd = "doors/dr1_clos";
	}
	
	S_Sound (m_Sector->soundorg, CHAN_BODY, snd, 1, ATTN_NORM);
}

DDoor::DDoor (sector_t *sector)
	: DMovingCeiling (sector)
{
}

// [RH] Merged EV_VerticalDoor and EV_DoLockedDoor into EV_DoDoor
//		and made them more general to support the new specials.

// [RH] SpawnDoor: Helper function for EV_DoDoor
DDoor::DDoor (sector_t *sec, EVlDoor type, fixed_t speed, int delay)
	: DMovingCeiling (sec)
{
	m_Type = type;
	m_TopWait = delay;
	m_Speed = speed;

	switch (type)
	{
	case doorClose:
		m_TopHeight = P_FindLowestCeilingSurrounding (sec) - 4*FRACUNIT;
		m_TopHeight -= 4*FRACUNIT;
		m_Direction = -1;
		DoorSound (false);
		break;

	case doorOpen:
	case doorRaise:
		m_Direction = 1;
		m_TopHeight = P_FindLowestCeilingSurrounding (sec) - 4*FRACUNIT;
		if (m_TopHeight != sec->ceilingheight)
			DoorSound (true);
		break;

	case doorCloseWaitOpen:
		m_TopHeight = sec->ceilingheight;
		m_Direction = -1;
		DoorSound (false);
		break;

	case doorRaiseIn5Mins: // denis - fixme - does this need code?
		break;
	}
}

BOOL EV_DoDoor (DDoor::EVlDoor type, line_t *line, AActor *thing,
                int tag, int speed, int delay, card_t lock)
{
	BOOL		rtn = false;
	int 		secnum;
	sector_t*	sec;

	if (lock && thing && !P_CheckKeys (thing->player, lock, tag))
		return false;

	if (tag == 0)
	{		// [RH] manual door
		if (!line)
			return false;

		// if the wrong side of door is pushed, give oof sound
		if (line->sidenum[1]==-1)				// killough
		{
			S_Sound (thing->x, thing->y, CHAN_VOICE, "*grunt1", 1, ATTN_NORM);
			return false;
		}

		// get the sector on the second side of activating linedef
		sec = sides[line->sidenum[1]].sector;
		secnum = sec-sectors;

		// if door already has a thinker, use it
		if (sec->ceilingdata && sec->ceilingdata->IsKindOf (RUNTIME_CLASS(DDoor)))
		{
			DDoor *door = static_cast<DDoor *>(sec->ceilingdata);

			// ONLY FOR "RAISE" DOORS, NOT "OPEN"s
			if (door->m_Type == DDoor::doorRaise && type == DDoor::doorRaise)
			{
				if (door->m_Direction == -1)
				{
					door->m_Direction = 1;	// go back up
				}
				else if (line->flags & ML_SPECIAL_USE)
					// [RH] activate push doors don't go back down when you
					//		run into them (otherwise opening them would be
					//		a real pain).
				{
					if (thing && !thing->player)
						return false;	// JDC: bad guys never close doors

					door->m_Direction = -1;	// start going down immediately
				}
				return true;
			}
		}
		if (new DDoor (sec, type, speed, delay))
			rtn = true;
	}
	else
	{	// [RH] Remote door

		secnum = -1;
		while ((secnum = P_FindSectorFromTag (tag,secnum)) >= 0)
		{
			sec = &sectors[secnum];
			// if the ceiling already moving, don't start the door action
			if (sec->ceilingdata)
				continue;

			if (new DDoor (sec, type, speed, delay))
				rtn = true;
		}
				
	}
	return rtn;
}


//
// Spawn a door that closes after 30 seconds
//
void P_SpawnDoorCloseIn30 (sector_t *sec)
{
	DDoor *door = new DDoor (sec);

	sec->special = 0;

	door->m_Sector = sec;
	door->m_Direction = 0;
	door->m_Type = DDoor::doorRaise;
	door->m_Speed = FRACUNIT*2;
	door->m_TopCountdown = 30 * TICRATE;
}

//
// Spawn a door that opens after 5 minutes
//
void P_SpawnDoorRaiseIn5Mins (sector_t *sec)
{
	DDoor *door = new DDoor (sec);

	sec->special = 0;

	door->m_Direction = 2;
	door->m_Type = DDoor::doorRaiseIn5Mins;
	door->m_Speed = FRACUNIT * 2;
	door->m_TopHeight = P_FindLowestCeilingSurrounding (sec);
	door->m_TopHeight -= 4*FRACUNIT;
	door->m_TopWait = (150*TICRATE)/35;
	door->m_TopCountdown = 5 * 60 * TICRATE;
}

VERSION_CONTROL (p_doors_cpp, "$Id$")

