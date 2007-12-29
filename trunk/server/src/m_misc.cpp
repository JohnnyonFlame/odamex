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
//	Main loop menu stuff.
//	Default Config File.
//
//-----------------------------------------------------------------------------


#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>

#include "doomtype.h"
#include "version.h"

#if defined(_WIN32)
#include <io.h>
#else
#include <unistd.h>
#endif

#include "m_alloc.h"

#include "doomdef.h"

#include "z_zone.h"

#include "m_swap.h"
#include "m_argv.h"

#include "w_wad.h"

#include "c_cvars.h"
#include "c_dispatch.h"

#include "i_system.h"
#include "v_video.h"

// State.
#include "doomstat.h"

// Data.
#include "dstrings.h"

#include "m_misc.h"

#include "cmdlib.h"

#include "g_game.h"
#include "sv_master.h"


//
// M_WriteFile
//
#ifndef O_BINARY
#define O_BINARY 0
#endif

// Used to identify the version of the game that saved
// a config file to compensate for new features that get
// put into newer configfiles.
static CVAR (configver, CONFIGVERSIONSTR, CVAR_ARCHIVE | CVAR_NOENABLEDISABLE)

//
// M_WriteFile
//
BOOL M_WriteFile(char const *name, void *source, QWORD length)
{
    FILE *handle;
    int	count;
	
    handle = fopen(name, "wb");

    if (handle == NULL)
	{
		Printf(PRINT_HIGH, "Could not open file %s for writing\n", name);
		return false;
	}

    count = fwrite(source, 1, length, handle);
    fclose(handle);
	
	if (count < length)
	{
		Printf(PRINT_HIGH, "Failed while writing to file %s\n", name);
		return false;
	}
		
    return true;
}


//
// M_ReadFile
//
QWORD M_ReadFile(char const *name, BYTE **buffer)
{
    FILE *handle;
    QWORD count, length;
    BYTE *buf;
	
    handle = fopen(name, "rb");
    
	if (handle == NULL)
	{
		Printf(PRINT_HIGH, "Could not open file %s for reading\n", name);
		return false;
	}

    // find the size of the file by seeking to the end and
    // reading the current position

    fseek(handle, 0, SEEK_END);
    length = ftell(handle);
    fseek(handle, 0, SEEK_SET);
    
    buf = (BYTE *)Z_Malloc (length, PU_STATIC, NULL);
    count = fread(buf, 1, length, handle);
    fclose (handle);
	
    if (count < length)
	{
		Printf(PRINT_HIGH, "Failed while reading from file %s\n", name);
		return false;
	}
		
    *buffer = buf;
    return length;
}


// [Russell] Simple function to check whether the given string is an iwad name
//           it also removes the extension for short-hand comparison
BOOL M_IsIWAD(std::string filename)
{
    // Valid IWAD file names
    static const char *doomwadnames[] =
    {
        "doom2f.wad",
        "doom2.wad",
        "plutonia.wad",
        "tnt.wad",
        "doomu.wad", // Hack from original Linux version. Not necessary, but I threw it in anyway.
        "doom.wad",
        "doom1.wad",
        "freedoom.wad",
        "freedm.wad"
    };
    
    std::vector<std::string> iwad_names(doomwadnames, 
                                        doomwadnames + sizeof(doomwadnames)/sizeof(*doomwadnames));
    
    if (!filename.length())
        return false;
        
    // lowercase our comparison string
    std::transform(filename.begin(), filename.end(), filename.begin(), tolower);
    
    // find our match if there is one
    for (QWORD i = 0; i < iwad_names.size(); i++)
    {
        std::string no_ext;
        
        // create an extensionless version, for short-hand comparison
        QWORD first_dot = iwad_names[i].find_last_of('.', 
                                                     iwad_names[i].length());
        
        if (first_dot != std::string::npos)
            no_ext = iwad_names[i].substr(0, first_dot);    
        
        if ((iwad_names[i] == filename) || (no_ext == filename))
            return true;
    }
    
    return false;
}

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


