// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id$
//
// Copyright (C) 1993-1996 by id Software, Inc.
// Copyright (C) 2006-2010 by The Odamex Team.
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


#ifndef __M_MISC__
#define __M_MISC__

#include <string>

#include "doomtype.h"

// [Russell] Simple function to check whether the given string is an iwad name
BOOL M_IsIWAD(std::string filename);

void M_LoadDefaults (void);

void STACK_ARGS M_SaveDefaults (void);

std::string GetConfigPath (void);

#endif



