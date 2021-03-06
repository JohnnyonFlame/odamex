// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// $Id: r_main.h 1856 2010-09-05 03:14:13Z ladna $
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
//	System specific interface stuff.
//
//-----------------------------------------------------------------------------


#ifndef __R_MAIN_H__
#define __R_MAIN_H__

#include "d_player.h"
#include "r_draw.h"
#include "r_data.h"
#include "v_palette.h"
#include "m_vectors.h"
#include "v_video.h"

// killough 10/98: special mask indicates sky flat comes from sidedef
#define PL_SKYTRANSFERLINE_MASK (0x80000000)
#define PL_SKYTRANSFERLINE_USESKY2 (0x00000002)

BOOL R_AlignFlat (int linenum, int side, int fc);

extern int negonearray[MAXWIDTH];
extern int viewheightarray[MAXWIDTH];

//
// POV related.
//
extern int32_t			viewcos;
extern int32_t			viewsin;

extern int				viewwidth;
extern int				viewheight;
extern int				viewwindowx;
extern int				viewwindowy;

extern bool				r_fakingunderwater;
extern bool				r_underwater;

extern fixed_t			fovtan;

extern int				centerx;
extern "C" int			centery;

extern fixed_t			centerxfrac;
extern fixed_t			centeryfrac;
extern fixed_t			yaspectmul;

extern shaderef_t		basecolormap;	// [RH] Colormap for sector currently being drawn

extern int				validcount;

extern int				linecount;
extern int				loopcount;

extern fixed_t			render_lerp_amount;

//
// Lighting LUT.
// Used for z-depth cuing per column/row,
//	and other lighting effects (sector ambient, flash).
//

// Lighting constants.
// Now why not 32 levels here?
#define LIGHTLEVELS 			16
#define LIGHTSEGSHIFT			 4

#define MAXLIGHTSCALE			48
#define LIGHTSCALEMULBITS		 8	// [RH] for hires lighting fix
#define LIGHTSCALESHIFT 		(12+LIGHTSCALEMULBITS)
#define MAXLIGHTZ			   128
#define LIGHTZSHIFT 			20

// [RH] Changed from shaderef_t* to int.
extern int				scalelight[LIGHTLEVELS][MAXLIGHTSCALE];
extern int				scalelightfixed[MAXLIGHTSCALE];
extern int				zlight[LIGHTLEVELS][MAXLIGHTZ];

extern int				extralight;
extern BOOL				foggy;
extern int				fixedlightlev;
extern shaderef_t		fixedcolormap;

extern shaderef_t		fixed_light_colormap_table[MAXWIDTH];
extern shaderef_t		fixed_colormap_table[MAXWIDTH];

extern int				lightscalexmul;	// [RH] for hires lighting fix
extern int				lightscaleymul;

//
// Function pointers to switch refresh/drawing functions.
//
extern void (*colfunc)(drawcolumn_t&);
extern void (*maskedcolfunc)(drawcolumn_t&);
extern void (*spanfunc)(drawspan_t&);
extern void (*spanslopefunc)(drawspan_t&);

//
// Utility functions.

int R_PointOnSide(fixed_t x, fixed_t y, const node_t* node);
int R_PointOnSide(fixed_t x, fixed_t y, fixed_t xl, fixed_t yl, fixed_t xh, fixed_t yh);
int R_PointOnSegSide(fixed_t x, fixed_t y, const seg_t* line);
bool R_PointOnLine(fixed_t x, fixed_t y, fixed_t xl, fixed_t yl, fixed_t xh, fixed_t yh);

angle_t R_PointToAngle(fixed_t x, fixed_t y);

// 2/1/10: Updated (from EE) to restore vanilla style, with tweak for overflow tolerance
angle_t R_PointToAngle2(fixed_t viewx, fixed_t viewy, fixed_t x, fixed_t y);

fixed_t R_PointToDist(fixed_t x, fixed_t y);

int R_ProjectPointX(fixed_t x, fixed_t y);
int R_ProjectPointY(fixed_t z, fixed_t y);
bool R_CheckProjectionX(int &x1, int &x2);
bool R_CheckProjectionY(int &y1, int &y2);

void R_RotatePoint(fixed_t x, fixed_t y, fixed_t &tx, fixed_t &ty);
void R_RotatePoint(fixed_t x, fixed_t y, angle_t ang, fixed_t &tx, fixed_t &ty);

bool R_ClipLineToFrustum(
		const v2fixed_t* v1, const v2fixed_t* v2,
		fixed_t clipdist, fixed_t slope,
		int32_t& clip1, int32_t& clip2);

void R_ClipLine(const v2fixed_t* in1, const v2fixed_t* in2, 
				int32_t lclip, int32_t rclip,
				v2fixed_t* out1, v2fixed_t* out2);
void R_ClipLine(const vertex_t* in1, const vertex_t* in2,
				int32_t lclip, int32_t rclip,
				v2fixed_t* out1, v2fixed_t* out2);

bool R_ProjectSeg(const seg_t* segline, drawseg_t* ds, wall_t* wall, fixed_t clipdist);

subsector_t* R_PointInSubsector(fixed_t x, fixed_t y);

void R_AddPointToBox(int x, int y, fixed_t* box);

fixed_t R_PointToDist2 (fixed_t dx, fixed_t dy);
void R_SetFOV(float fov, bool force);
float R_GetFOV (void);

#define WIDE_STRETCH 0
#define WIDE_ZOOM 1
#define WIDE_TRUE 2

int R_GetWidescreen(void);

//
// REFRESH - the actual rendering functions.
//

// Called by G_Drawer.
void R_RenderPlayerView (player_t *player);

// Called by startup code.
void R_Init (void);

// Called by exit code.
void STACK_ARGS R_Shutdown (void);

// Called by M_Responder.
void R_SetViewSize (int blocks);

// [RH] Initialize multires stuff for renderer
void R_MultiresInit (void);

void R_ResetDrawFuncs();
void R_SetFuzzDrawFuncs();
void R_SetLucentDrawFuncs();
void R_SetTranslatedDrawFuncs();
void R_SetTranslatedLucentDrawFuncs();

extern argb_t translationRGB[MAXPLAYERS+1][16];

void R_DrawLine(const v3fixed_t* inpt1, const v3fixed_t* inpt2, byte color);

#endif // __R_MAIN_H__
