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
//		Default Config File.
//
//-----------------------------------------------------------------------------


#include <stdio.h>
#include <stdlib.h>

#include "c_cvars.h"
#include "c_dispatch.h"
#include "doomdef.h"
#include "doomtype.h"
#include "m_argv.h"
#include "i_system.h"
#include "version.h"
#include "sv_master.h"


// Used to identify the version of the game that saved
// a config file to compensate for new features that get
// put into newer configfiles.
static CVAR (configver, CONFIGVERSIONSTR, CVAR_ARCHIVE | CVAR_NOENABLEDISABLE)

// [RH] Get configfile path.
// This file contains commands to set all
// archived cvars, bind commands to keys,
// and set other general game information.
std::string GetConfigPath (void)
{
	const char *p = Args.CheckValue ("-config");

	if(p)
		return p;

	return I_GetUserFileName ("odasrv.cfg");
}

//
// M_SaveDefaults
//

// [RH] Don't write a config file if M_LoadDefaults hasn't been called.
static BOOL DefaultsLoaded;

void STACK_ARGS M_SaveDefaults (void)
{
	FILE *f;

	if (!DefaultsLoaded)
		return;

	std::string configfile = GetConfigPath();

	// Make sure the user hasn't changed configver
	configver.Set(CONFIGVERSIONSTR);

	if ( (f = fopen (configfile.c_str(), "w")) )
	{
		fprintf (f, "// Generated by Odamex " DOTVERSIONSTR " - don't hurt anything\n");

		// Archive all cvars marked as CVAR_ARCHIVE
		cvar_t::C_ArchiveCVars (f);

		// Archive all active key bindings
		//C_ArchiveBindings (f);

		// Archive all aliases
		DConsoleAlias::C_ArchiveAliases (f);

		// Archive master list
		SV_ArchiveMasters (f);

		fclose (f);
	}
}


//
// M_LoadDefaults
//
extern int cvar_defflags;
EXTERN_CVAR (dimamount)

void M_LoadDefaults (void)
{
	// Set default key bindings. These will be overridden
	// by the bindings in the config file if it exists.
	//AddCommandString (DefBindings);

	std::string cmd = "exec \"";
	cmd += GetConfigPath();
	cmd += "\"";

	cvar_defflags = CVAR_ARCHIVE;
	AddCommandString (cmd.c_str());
	cvar_defflags = 0;

	if (configver < 118.0f)
	{
		AddCommandString ("alias idclev map");		
		AddCommandString ("alias changemap map");	
	}

	DefaultsLoaded = true;
	atterm (M_SaveDefaults);
}

VERSION_CONTROL (m_misc_cpp, "$Id$")


