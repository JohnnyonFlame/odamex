// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// $Id$
//
// Copyright (C) 1993-1996 by id Software, Inc.
// Copyright (C) 2006-2013 by The Odamex Team.
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
//		Functions to draw textures directly to screen->
//		Functions to blit a block to the screen->
//
//-----------------------------------------------------------------------------



#include <stdio.h>
#include <assert.h>

#include "minilzo.h"
// [RH] Output buffer size for LZO compression.
//		Extra space in case uncompressable.
#define OUT_LEN(a)		((a) + (a) / 64 + 16 + 3)

#include "m_alloc.h"

#include "i_system.h"
#include "i_video.h"
#include "r_local.h"
#include "r_draw.h"
#include "r_plane.h"
#include "r_state.h"

#include "doomdef.h"
#include "doomdata.h"
#include "doomstat.h"
#include "d_main.h"

#include "c_console.h"
#include "hu_stuff.h"

#include "m_argv.h"
#include "m_bbox.h"
#include "m_swap.h"
#include "m_menu.h"

#include "v_video.h"
#include "v_text.h"

#include "w_wad.h"
#include "z_zone.h"

#include "c_cvars.h"
#include "c_dispatch.h"
#include "cmdlib.h"

IMPLEMENT_CLASS (DCanvas, DObject)

int DisplayWidth, DisplayHeight, DisplayBits;
int SquareWidth;

argb_t Col2RGB8[65][256];
palindex_t RGB32k[32][32][32];

void I_FlushInput();

// [RH] The framebuffer is no longer a mere byte array.
// There's also only one, not four.
DCanvas *screen;

DBoundingBox dirtybox;

EXTERN_CVAR (vid_defwidth)
EXTERN_CVAR (vid_defheight)
EXTERN_CVAR (vid_32bpp)
EXTERN_CVAR (vid_autoadjust)
EXTERN_CVAR (vid_overscan)

CVAR_FUNC_IMPL (vid_maxfps)
{
	if (var == 0)
	{
		capfps = false;
		maxfps = 99999.0f;
	}
	else
	{
		if (var < 35.0f)
			var.Set(35.0f);
		else
		{
			capfps = true;
			maxfps = var;
		}
	}
}

EXTERN_CVAR (ui_dimamount)
EXTERN_CVAR (ui_dimcolor)

// [RH] Set true when vid_setmode command has been executed
BOOL	setmodeneeded = false;
// [RH] Resolution to change to when setmodeneeded is true
int		NewWidth, NewHeight, NewBits;

CVAR_FUNC_IMPL (vid_fullscreen)
{
	setmodeneeded = true;
	NewWidth = I_GetVideoWidth();
	NewHeight = I_GetVideoHeight();
	NewBits = I_GetVideoBitDepth();
}

CVAR_FUNC_IMPL (vid_32bpp)
{
	setmodeneeded = true;
	NewBits = (int)vid_32bpp ? 32 : 8;
}


//
// V_MarkRect
//
void V_MarkRect (int x, int y, int width, int height)
{
	dirtybox.AddToBox (x, y);
	dirtybox.AddToBox (x+width-1, y+height-1);
}


void DCanvas::Dim(int x1, int y1, int w, int h, const char* color, float famount) const
{
	if (!buffer)
		return;

	if (x1 < 0 || x1 + w > width || y1 < 0 || y1 + h > height)
		return;

	if (famount <= 0.0f)
		return;
	else if (famount > 1.0f)
		famount = 1.0f;

	if (is8bit())
	{
		int bg;
		int x, y;

		fixed_t amount = (fixed_t)(famount * 64.0f);
		argb_t *fg2rgb = Col2RGB8[amount];
		argb_t *bg2rgb = Col2RGB8[64-amount];
		unsigned int fg = fg2rgb[V_GetColorFromString(V_GetDefaultPalette()->basecolors, color)];

		byte *dest = buffer + y1 * pitch + x1;
		int gap = pitch - w;

		int xcount = w / 4;
		int xcount_remainder = w % 4;

		for (y = h; y > 0; y--)
		{
			for (x = xcount; x > 0; x--)
			{
				// Unroll the loop for a speed improvement
				bg = bg2rgb[*dest];
				bg = (fg+bg) | 0x1f07c1f;
				*dest++ = RGB32k[0][0][bg&(bg>>15)];

				bg = bg2rgb[*dest];
				bg = (fg+bg) | 0x1f07c1f;
				*dest++ = RGB32k[0][0][bg&(bg>>15)];

				bg = bg2rgb[*dest];
				bg = (fg+bg) | 0x1f07c1f;
				*dest++ = RGB32k[0][0][bg&(bg>>15)];

				bg = bg2rgb[*dest];
				bg = (fg+bg) | 0x1f07c1f;
				*dest++ = RGB32k[0][0][bg&(bg>>15)];
			}
			for (x = xcount_remainder; x > 0; x--)
			{
				// account for widths that aren't multiples of 4
				bg = bg2rgb[*dest];
				bg = (fg+bg) | 0x1f07c1f;
				*dest++ = RGB32k[0][0][bg&(bg>>15)];
			}
			dest += gap;
		}
	}
	else
	{
		int fill = V_GetColorFromString (NULL, color);
		r_dimpatchD(this, fill, (int)(famount * 256.0f), x1, y1, w, h);
	}
}

void DCanvas::Dim(int x1, int y1, int w, int h) const
{
	if (ui_dimamount < 0.0f)
		ui_dimamount.Set (0.0f);
	else if (ui_dimamount > 1.0f)
		ui_dimamount.Set (1.0f);

	if (ui_dimamount == 0.0f)
		return;

	Dim(x1, y1, w, h, ui_dimcolor.cstring(), ui_dimamount);
}

std::string V_GetColorStringByName (const char *name)
{
	/* Note: The X11R6RGB lump used by this function *MUST* end
	 * with a NULL byte. This is so that COM_Parse is able to
	 * detect the end of the lump.
	 */
	char *rgbNames, *data, descr[5*3];
	int c[3], step;

	if (!(rgbNames = (char *)W_CacheLumpName ("X11R6RGB", PU_CACHE))) {
		Printf (PRINT_HIGH, "X11R6RGB lump not found\n");
		return NULL;
	}

	// skip past the header line
	data = strchr (rgbNames, '\n');
	step = 0;

	while ( (data = COM_Parse (data)) ) {
		if (step < 3) {
			c[step++] = atoi (com_token);
		} else {
			step = 0;
			if (*data >= ' ') {		// In case this name contains a space...
				char *newchar = com_token + strlen(com_token);

				while (*data >= ' ') {
					*newchar++ = *data++;
				}
				*newchar = 0;
			}

			if (!stricmp (com_token, name)) {
				sprintf (descr, "%04x %04x %04x",
						 (c[0] << 8) | c[0],
						 (c[1] << 8) | c[1],
						 (c[2] << 8) | c[2]);
				return descr;
			}
		}
	}
	return "";
}

BEGIN_COMMAND (setcolor)
{
	if (argc < 3)
	{
		Printf (PRINT_HIGH, "Usage: setcolor <cvar> <color>\n");
		return;
	}

	std::string name = C_ArgCombine(argc - 2, (const char **)(argv + 2));
	if (name.length())
	{
		std::string desc = V_GetColorStringByName (name.c_str());

		if (desc.length())
		{
			std::string setcmd = "set ";
			setcmd += argv[1];
			setcmd += " \"";
			setcmd += desc;
			setcmd += "\"";
			AddCommandString (setcmd);
		}
	}
}
END_COMMAND (setcolor)

// Build the tables necessary for translucency
static void BuildTransTable (argb_t *palette)
{
	// create the small RGB table
	for (int r = 0; r < 32; r++)
		for (int g = 0; g < 32; g++)
			for (int b = 0; b < 32; b++)
				RGB32k[r][g][b] = V_BestColor(palette, (r<<3)|(r>>2), (g<<3)|(g>>2), (b<<3)|(b>>2), 256);

	for (int x = 0; x < 65; x++)
		for (int y = 0; y < 256; y++)
			Col2RGB8[x][y] = (((RPART(palette[y])*x)>>4)<<20)  |
							  ((GPART(palette[y])*x)>>4) |
							 (((BPART(palette[y])*x)>>4)<<10);
}

void DCanvas::Lock ()
{
	m_LockCount++;
	if (m_LockCount == 1)
	{
		I_LockScreen (this);

		if (this == screen)
		{
			int bytesperpixel = bits >> 3;
			if (dcol.pitch != pitch / bytesperpixel)
			{
				dcol.pitch = pitch / bytesperpixel;
				dcol.bytesperpixel = bytesperpixel;
				R_InitFuzzTable();
			}
		}
	}
}

void DCanvas::Unlock ()
{
	if (m_LockCount)
		if (--m_LockCount == 0)
			I_UnlockScreen (this);
}

void DCanvas::Blit (int srcx, int srcy, int srcwidth, int srcheight,
			 DCanvas *dest, int destx, int desty, int destwidth, int destheight)
{
	I_Blit (this, srcx, srcy, srcwidth, srcheight, dest, destx, desty, destwidth, destheight);
}

CVAR_FUNC_IMPL (vid_widescreen)
{
	static bool last_value = !var;	// force setmodeneeded when loading cvar
	if (last_value != var)
		setmodeneeded = true;
	last_value = var;
}

CVAR_FUNC_IMPL (sv_allowwidescreen)
{
	// change setmodeneeded when the value of sv_allowwidescreen
	// changes our ability to use wide-fov
	bool wide_fov = V_UseWidescreen() || V_UseLetterBox();
	static bool last_value = !wide_fov; 

	if (last_value != wide_fov)
		setmodeneeded = true;
	last_value = wide_fov;
}

//
// V_UsePillarBox
//
// Determines if the display should use pillarboxing. If the resolution is a
// widescreen mode and either the user or the server doesn't allow
// widescreen usage, use pillarboxing.
//
bool V_UsePillarBox()
{
	if (I_GetVideoWidth() == 0 || I_GetVideoHeight() == 0)
		return false;

	if (I_GetVideoWidth() == 320 && I_GetVideoHeight() == 200)
		return false;
	if (I_GetVideoWidth() == 640 && I_GetVideoHeight() == 400)
		return false;
	
	float ratio = float(I_GetVideoWidth()) / float(I_GetVideoHeight());
	return (!vid_widescreen || (!serverside && !sv_allowwidescreen)) && ratio > (4.0f / 3.0f);
}

//
// V_UseLetterBox
//
// Determines if the display should use letterboxing. If the resolution is a
// standard 4:3 mode and both the user and the server allow widescreen
// usage, use letterboxing.
//
bool V_UseLetterBox()
{
	if (I_GetVideoWidth() == 0 || I_GetVideoHeight() == 0)
		return false;

	if (I_GetVideoWidth() == 320 && I_GetVideoHeight() == 200)
		return false;
	if (I_GetVideoWidth() == 640 && I_GetVideoHeight() == 400)
		return false;
	
	float ratio = float(I_GetVideoWidth()) / float(I_GetVideoHeight());
	return (vid_widescreen && (serverside || sv_allowwidescreen)) && ratio <= (4.0f / 3.0f);
}

//
// V_UseWidescreen
//
//
bool V_UseWidescreen()
{
	if (I_GetVideoWidth() == 0 || I_GetVideoHeight() == 0)
		return false;

	if (I_GetVideoWidth() == 320 && I_GetVideoHeight() == 200)
		return false;
	if (I_GetVideoWidth() == 640 && I_GetVideoHeight() == 400)
		return false;
	
	float ratio = float(I_GetVideoWidth()) / float(I_GetVideoHeight());
	return (vid_widescreen && (serverside || sv_allowwidescreen)) && ratio > (4.0f / 3.0f);
}

//
// V_SetResolution
//
static bool V_DoModeSetup(int width, int height, int bits)
{
	int basew = 320, baseh = 200;

	// Free the virtual framebuffer
	if (screen)
	{
		I_FreeScreen(screen);
		screen = NULL;
	}

	if (!I_SetMode(width, height, bits))
		return false;

	I_SetOverscan (vid_overscan);

	if (V_UsePillarBox())
		width = 4.0f * height / 3.0f;
	else if (V_UseLetterBox())
		height = 9.0f * width / 16.0f;

	// This uses the smaller of the two results. It's still not ideal but at least
	// this allows con_scaletext to have some purpose...

    CleanXfac = width / basew;
    CleanYfac = height / baseh;

	if (CleanXfac == 0 || CleanYfac == 0)
		CleanXfac = CleanYfac = 1;
	else
	{
		if (CleanXfac < CleanYfac)
			CleanYfac = CleanXfac;
		else
			CleanXfac = CleanYfac;
	}

	DisplayWidth = width;
	DisplayHeight = height;
	DisplayBits = bits;

	SquareWidth = 4.0f * DisplayHeight / 3.0f;
	
	if (SquareWidth > DisplayWidth)
        SquareWidth = DisplayWidth;

	// Allocate a new virtual framebuffer
	bool primary = (vid_fullscreen == 0);

	// [SL] Add a bit to the screen width if it's a power-of-two to avoid
	// cache thrashing
	int cache_fudge = (width % 256) == 0 ? 4 : 0;
	
	screen = I_AllocateScreen(width + cache_fudge, height, bits, primary);

	V_ForceBlend (0,0,0,0);
	V_GammaAdjustPalettes();
	V_RefreshPalettes();
	V_ReinitColormap();

	R_InitColumnDrawers();
	R_MultiresInit();

	if (V_FontsLoaded())
		V_LoadFonts();

	// [SL] 2011-11-30 - Prevent the player's view angle from moving
	I_FlushInput();

	return true;
}

bool V_SetResolution(int width, int height, int bits)
{
	int oldwidth, oldheight, oldbits;

	if (screen)
	{
		oldwidth = I_GetVideoWidth(); 
		oldheight = I_GetVideoHeight(); 
		oldbits = I_GetVideoBitDepth(); 
	}
	else
	{
		// Harmless if screen wasn't allocated
		oldwidth = width;
		oldheight = height;
		oldbits = bits;
	}

	// Make sure we don't set the resolution smaller than Doom's original 320x200
	// resolution. Bad things might happen. 
	width = clamp(width, 320, MAXWIDTH);
	height = clamp(height, 200, MAXHEIGHT);

	if ((int)(vid_autoadjust))
	{
		if (vid_fullscreen)
		{
			// Fullscreen needs to check for a valid resolution.
			I_ClosestResolution(&width, &height);
		}

		// Try specified resolution
		if (!I_CheckResolution(width, height))
		{
			// Try the previous resolution (if any)
			if (!I_CheckResolution(oldwidth, oldheight))
		   		return false;

			width = oldwidth;
			height = oldheight;
			bits = oldbits;
		}
	}

	return V_DoModeSetup(width, height, bits);
}

BEGIN_COMMAND (vid_setmode)
{
	int		width = 0, height = 0;
	int		bits = DisplayBits;

	// No arguments
	if (argc == 1) {
		Printf(PRINT_HIGH, "Usage: vid_setmode <width> <height>\n");
		return;
	}
	// Width
	if (argc > 1) 
		width = atoi(argv[1]);
	
	// Height (optional)
	if (argc > 2)
		height = atoi(argv[2]);
	if (height == 0)
		height = I_GetVideoHeight(); 

	// Bits
	bits = (int)vid_32bpp ? 32 : 8;

	if (width < 320 || height < 200) 
		Printf(PRINT_HIGH, "%dx%d is too small.  Minimum resolution is 320x200.\n", width, height);

	if (width > MAXWIDTH || height > MAXHEIGHT)
		Printf(PRINT_HIGH, "%dx%d is too large.  Maximum resolution is %dx%d.\n", width, height, MAXWIDTH, MAXHEIGHT);

	if (I_CheckResolution(width, height))
	{
		// The actual change of resolution will take place
		// near the beginning of D_Display().
		if (gamestate != GS_STARTUP)
		{
			setmodeneeded = true;
			NewWidth = width;
			NewHeight = height;
			NewBits = bits;
		}
	}
	else
	{
		Printf(PRINT_HIGH, "Unknown resolution %dx%d\n", width, height);
	}
}
END_COMMAND (vid_setmode)

BEGIN_COMMAND (checkres)
{
    Printf (PRINT_HIGH, "Resolution: %d x %d x %d (%s)\n",
			I_GetVideoWidth(), I_GetVideoHeight(), I_GetVideoBitDepth(),
        	(vid_fullscreen ? "FULLSCREEN" : "WINDOWED")); // NES - Simple resolution checker.
}
END_COMMAND (checkres)

//
// V_InitPalette
//

void V_InitPalette (void)
{
	// [RH] Initialize palette subsystem
	if (!(V_InitPalettes("PLAYPAL")))
		I_FatalError ("Could not initialize palette");

	BuildTransTable(V_GetDefaultPalette()->basecolors);

	V_ForceBlend(0, 0, 0, 0);

	V_RefreshPalettes();

	assert(V_GetDefaultPalette()->maps.colormap != NULL);
	assert(V_GetDefaultPalette()->maps.shademap != NULL);
	V_Palette = shaderef_t(&V_GetDefaultPalette()->maps, 0);
}

//
// V_Close
//
//
void STACK_ARGS V_Close()
{
	if(screen)
	{
		I_FreeScreen(screen);
		screen = NULL;
	}
}

//
// V_Init
//

void V_Init (void)
{
	int width = 0, height = 0, bits = 0;
	const char* value;

	if ( (value = Args.CheckValue("-width")) )
		width = atoi(value);

	if ( (value = Args.CheckValue("-height")) )
		height = atoi(value);

	if ( (value = Args.CheckValue("-bits")) )
		bits = atoi(value);

	if (width == 0)
	{
		if (height == 0)
		{
			width = (int)(vid_defwidth);
			height = (int)(vid_defheight);
		}
		else
		{
			width = (height * 8) / 6;
		}
	}
	else if (height == 0)
	{
		height = (width * 6) / 8;
	}

	if (bits == 0)
	{
		bits = (int)vid_32bpp ? 32 : 8;
	}

    if ((int)(vid_autoadjust))
        I_ClosestResolution (&width, &height);

	if (!V_SetResolution (width, height, bits))
		I_FatalError ("Could not set resolution to %d x %d x %d %s\n", width, height, bits,
            (vid_fullscreen ? "FULLSCREEN" : "WINDOWED"));
	else
        AddCommandString("checkres");
}

VERSION_CONTROL (v_video_cpp, "$Id$")

