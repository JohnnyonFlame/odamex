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
//  Internally used data structures for virtually everything,
//   key definitions, lots of other stuff.
//
//-----------------------------------------------------------------------------


#ifndef __DOOMDEF_H__
#define __DOOMDEF_H__

#include <stdio.h>
#include <string.h>

// GhostlyDeath -- Can't stand the warnings
#if _MSC_VER == 1200
#pragma warning(disable:4786)
#endif

// GhostlyDeath -- MSVC++ 8+, remove "deprecated" warnings
#if _MSC_VER >= 1400
#pragma warning(disable : 4996)
#endif

//
// The packed attribute forces structures to be packed into the minimum
// space necessary.  If this is not done, the compiler may align structure
// fields differently to optimise memory access, inflating the overall
// structure size.  It is important to use the packed attribute on certain
// structures where alignment is important, particularly data read/written
// to disk.
//

#ifdef __GNUC__
#define PACKEDATTR __attribute__((packed))
#else
#define PACKEDATTR
#endif


#include "farchive.h"

//
// denis
//
// if(clientside) { do visual effects, sounds, etc }
// if(serverside) { spawn/destroy things, change maps, etc }
//
extern bool clientside, serverside;

// [Nes] - Determines which program the user is running.
enum baseapp_t
{
	client,		// Odamex.exe
	server		// Odasrv.exe
};

extern baseapp_t baseapp;

//
// Global parameters/defines.
//

// Game mode handling - identify IWAD version
//	to handle IWAD dependend animations etc.
enum GameMode_t
{
  shareware,	// DOOM 1 shareware, E1, M9
  registered,	// DOOM 1 registered, E3, M27
  commercial,	// DOOM 2 retail, E1 M34
  // DOOM 2 german edition not handled
  retail,		// DOOM 1 retail, E4, M36
  retail_chex,	// Chex Quest
  undetermined	// Well, no IWAD found.

};


// Mission packs - might be useful for TC stuff?
enum GameMission_t
{
  doom, 		// DOOM 1
  doom2,		// DOOM 2
  pack_tnt, 	// TNT mission pack
  pack_plut,	// Plutonia pack
  chex,			// Chex Quest
  none
};


// Identify language to use, software localization.
enum Language_t
{
  english,
  french,
  german,
  unknown
};


// If rangecheck is undefined,
// most parameter validation debugging code will not be compiled
#define RANGECHECK

// The maximum number of players, multiplayer/networking.
#define MAXPLAYERS				255
#define MAXPLAYERS_VANILLA		4

// State updates, number of tics / second.
#define TICRATE 		35

// The current state of the game: whether we are
// playing, gazing at the intermission screen,
// the game final animation, or a demo.
enum gamestate_t
{
	GS_LEVEL,
	GS_INTERMISSION,
	GS_FINALE,
	GS_DEMOSCREEN,
	GS_FULLCONSOLE,		// [RH]	Fullscreen console
	GS_HIDECONSOLE,		// [RH] The menu just did something that should hide fs console
	GS_STARTUP,			// [RH] Console is fullscreen, and game is just starting
	GS_DOWNLOAD,		// denis - wad downloading
	GS_CONNECTING,		// denis - replace the old global "tryingtoconnect"

	GS_FORCEWIPE = -1
};

//
// Difficulty/skill settings/filters.
//

// Skill flags.
#define MTF_EASY				1
#define MTF_NORMAL				2
#define MTF_HARD				4

// Deaf monsters/do not react to sound.
#define MTF_AMBUSH				8

// Gravity
#define GRAVITY		FRACUNIT

enum skill_t
{
	sk_baby = 1,
	sk_easy,
	sk_medium,
	sk_hard,
	sk_nightmare
};

//
// Key cards.
//
enum card_t
{
	it_bluecard,
	it_yellowcard,
	it_redcard,
	it_blueskull,
	it_yellowskull,
	it_redskull,

	NUMCARDS,

		// GhostlyDeath <August 31, 2008> -- Before this was not = 0 and when
		// the map is loaded the value is just bitshifted so that the values
		// that were here were incorrect, keyed generalized doors work now
        NoKey = 0,
        RCard,
        BCard,
        YCard,
        RSkull,
        BSkull,
        YSkull,

        AnyKey = 100,
        AllKeys = 101,

        CardIsSkull = 128
};

//
//	[Toke - CTF] CTF Flags
//
enum flag_t
{
	it_blueflag,
	it_redflag,
	it_goldflag, // Remove me in 0.5

	NUMFLAGS
};

inline FArchive &operator<< (FArchive &arc, card_t i)
{
	return arc << (BYTE)i;
}
inline FArchive &operator>> (FArchive &arc, card_t &i)
{
	BYTE in; arc >> in; i = (card_t)in; return arc;
}


// The defined weapons,
//	including a marker indicating
//	user has not changed weapon.
enum weapontype_t
{
	wp_fist,
	wp_pistol,
	wp_shotgun,
	wp_chaingun,
	wp_missile,
	wp_plasma,
	wp_bfg,
	wp_chainsaw,
	wp_supershotgun,

	NUMWEAPONS,

	// No pending weapon change.
	wp_nochange

};

inline FArchive &operator<< (FArchive &arc, weapontype_t i)
{
	return arc << (BYTE)i;
}
inline FArchive &operator>> (FArchive &arc, weapontype_t &i)
{
	BYTE in; arc >> in; i = (weapontype_t)in; return arc;
}


// Ammunition types defined.
enum ammotype_t
{
	am_clip,	// Pistol / chaingun ammo.
	am_shell,	// Shotgun / double barreled shotgun.
	am_cell,	// Plasma rifle, BFG.
	am_misl,	// Missile launcher.
	NUMAMMO,
	am_noammo	// Unlimited for chainsaw / fist.

};

inline FArchive &operator<< (FArchive &arc, ammotype_t i)
{
	return arc << (BYTE)i;
}
inline FArchive &operator>> (FArchive &arc, ammotype_t &i)
{
	BYTE in; arc >> in; i = (ammotype_t)in; return arc;
}


// Power up artifacts.
enum powertype_t
{
	pw_invulnerability,
	pw_strength,
	pw_invisibility,
	pw_ironfeet,
	pw_allmap,
	pw_infrared,
	NUMPOWERS
};

inline FArchive &operator<< (FArchive &arc, powertype_t i)
{
	return arc << (BYTE)i;
}
inline FArchive &operator>> (FArchive &arc, powertype_t &i)
{
	BYTE in; arc >> in; i = (powertype_t)in; return arc;
}


//
// Power up durations, how many tics till expiration.
//
#define INVULNTICS	(30*TICRATE)
#define INVISTICS	(60*TICRATE)
#define INFRATICS	(120*TICRATE)
#define IRONTICS	(60*TICRATE)

// [ML] 1/5/10: Move input defs to doomkeys.h
#include "doomkeys.h"

// phares 3/20/98:
//
// Player friction is variable, based on controlling
// linedefs. More friction can create mud, sludge,
// magnetized floors, etc. Less friction can create ice.

#define MORE_FRICTION_MOMENTUM	15000	// mud factor based on momentum
#define ORIG_FRICTION			0xE800	// original value
#define ORIG_FRICTION_FACTOR	2048	// original value

#endif	// __DOOMDEF_H__


