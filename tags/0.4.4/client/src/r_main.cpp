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
//	Rendering main loop and setup functions,
//	 utility functions (BSP, geometry, trigonometry).
//	See tables.c, too.
//
//-----------------------------------------------------------------------------

#include "m_alloc.h"
#include <stdlib.h>
#include <math.h>
#include "doomdef.h"
#include "d_net.h"
#include "doomstat.h"
#include "m_random.h"
#include "m_bbox.h"
#include "p_local.h"
#include "r_local.h"
#include "r_sky.h"
#include "st_stuff.h"
#include "c_cvars.h"
#include "v_video.h"
#include "stats.h"
#include "z_zone.h"
#include "i_video.h"

#define DISTMAP			2

void R_SpanInitData ();

extern int *walllights;

// [RH] Defined in d_main.cpp
extern BOOL DrawNewHUD;
extern BOOL DrawNewSpecHUD;
extern dyncolormap_t NormalLight;

EXTERN_CVAR (r_viewsize)


fixed_t			FocalLengthX;
fixed_t			FocalLengthY;
int 			viewangleoffset;

// increment every time a check is made
int 			validcount = 1;

// [RH] colormap currently drawing with
lighttable_t	*basecolormap;
int				fixedlightlev;
lighttable_t	*fixedcolormap;

int 			centerx;
extern "C" {int	centery; }

fixed_t 		centerxfrac;
fixed_t 		centeryfrac;
fixed_t			yaspectmul;

// just for profiling purposes
int 			framecount;
int 			linecount;
int 			loopcount;

fixed_t 		viewx;
fixed_t 		viewy;
fixed_t 		viewz;

angle_t 		viewangle;

fixed_t 		viewcos;
fixed_t 		viewsin;

AActor			*camera;	// [RH] camera to draw from. doesn't have to be a player

//
// precalculated math tables
//
angle_t 		clipangle;

// The viewangletox[viewangle + FINEANGLES/4] lookup
// maps the visible view angles to screen X coordinates,
// flattening the arc to a flat projection plane.
// There will be many angles mapped to the same X.
int 			viewangletox[FINEANGLES/2];

// The xtoviewangleangle[] table maps a screen pixel
// to the lowest viewangle that maps back to x ranges
// from clipangle to -clipangle.
angle_t 		*xtoviewangle;

fixed_t			*finecosine = &finesine[FINEANGLES/4];

int				scalelight[LIGHTLEVELS][MAXLIGHTSCALE];
int				scalelightfixed[MAXLIGHTSCALE];
int				zlight[LIGHTLEVELS][MAXLIGHTZ];

// [RH] used to keep hires modes dark enough
int				lightscalexmul;
int				lightscaleymul;

// bumped light from gun blasts
int 			extralight;

// [RH] ignore extralight and fullbright
BOOL			foggy;

BOOL			setsizeneeded;
int				setblocks, setdetail = -1;

fixed_t			freelookviewheight;

unsigned int	R_OldBlend = ~0;

void (*colfunc) (void);
void (*basecolfunc) (void);
void (*fuzzcolfunc) (void);
void (*lucentcolfunc) (void);
void (*transcolfunc) (void);
void (*tlatedlucentcolfunc) (void);
void (*spanfunc) (void);

void (*hcolfunc_pre) (void);
void (*hcolfunc_post1) (int hx, int sx, int yl, int yh);
void (*hcolfunc_post2) (int hx, int sx, int yl, int yh);
void (*hcolfunc_post4) (int sx, int yl, int yh);

static int lastcenteryfrac;
static int FieldOfView = 2048;	// Fineangles in the SCREENWIDTH wide window

//
//
// R_PointOnSide
//
// Traverse BSP (sub) tree, check point against partition plane.
// Returns side 0 (front) or 1 (back).
//
// killough 5/2/98: reformatted
//
//

int R_PointOnSide (fixed_t x, fixed_t y, node_t *node)
{
	if (!node->dx)
		return x <= node->x ? node->dy > 0 : node->dy < 0;

	if (!node->dy)
		return y <= node->y ? node->dx < 0 : node->dx > 0;

	x -= node->x;
	y -= node->y;

	// Try to quickly decide by looking at sign bits.
	if ((node->dy ^ node->dx ^ x ^ y) < 0)
		return (node->dy ^ x) < 0;  // (left is negative)
	return FixedMul (y, node->dx >> FRACBITS) >= FixedMul (node->dy >> FRACBITS, x);
}

//
//
// R_PointOnSegSide
//
// Same, except takes a lineseg as input instead of a linedef
//
// killough 5/2/98: reformatted
//
//

int R_PointOnSegSide (fixed_t x, fixed_t y, seg_t *line)
{
	fixed_t lx = line->v1->x;
	fixed_t ly = line->v1->y;
	fixed_t ldx = line->v2->x - lx;
	fixed_t ldy = line->v2->y - ly;

	if (!ldx)
		return x <= lx ? ldy > 0 : ldy < 0;

	if (!ldy)
		return y <= ly ? ldx < 0 : ldx > 0;

	x -= lx;
	y -= ly;

	// Try to quickly decide by looking at sign bits.
	if ((ldy ^ ldx ^ x ^ y) < 0)
		return (ldy ^ x) < 0;          // (left is negative)
	return FixedMul (y, ldx >> FRACBITS) >= FixedMul (ldy >> FRACBITS, x);
}

//
// R_PointToAngle
//
// To get a global angle from cartesian coordinates, the coordinates are
// flipped until they are in the first octant of the coordinate system,
// then the y (<=x) is scaled and divided by x to get a tangent (slope)
// value which is looked up in the tantoangle[] table.
//

angle_t R_PointToAngle2 (fixed_t x1, fixed_t y1, fixed_t x, fixed_t y)
{
  return (y -= y1, (x -= x1) || y) ?
    x >= 0 ?
      y >= 0 ?
        (x > y) ? tantoangle[SlopeDiv(y,x)] :						// octant 0
                ANG90-1-tantoangle[SlopeDiv(x,y)] :					// octant 1
        x > (y = -y) ? (angle_t)-(SDWORD)tantoangle[SlopeDiv(y,x)] :	// octant 8
                       ANG270+tantoangle[SlopeDiv(x,y)] :			// octant 7
      y >= 0 ? (x = -x) > y ? ANG180-1-tantoangle[SlopeDiv(y,x)] :	// octant 3
                            ANG90 + tantoangle[SlopeDiv(x,y)] :		// octant 2
        (x = -x) > (y = -y) ? ANG180+tantoangle[SlopeDiv(y,x)] :	// octant 4
                              ANG270-1-tantoangle[SlopeDiv(x,y)] :	// octant 5
    0;
}

//
// R_PointToAngle - wrapper around R_PointToAngle2
//
angle_t
R_PointToAngle
( fixed_t	x,
  fixed_t	y )
{
    return R_PointToAngle2 (viewx, viewy, x, y);
}

//
// R_InitPointToAngle
//
void R_InitPointToAngle (void)
{
	int i;
	float f;
//
// slope (tangent) to angle lookup
//
	for (i = 0; i <= SLOPERANGE; i++)
	{
		f = atan((float)i/SLOPERANGE)/(3.141592657*2); // denis - vanilla
		tantoangle[i] = (angle_t)(0xffffffff*f);
	}
}

//
// R_PointToDist
//
// Returns the distance from the viewpoint to some other point. In a
// floating point environment, we'd probably be better off using the
// Pythagorean Theorem to determine the result.
//
// killough 5/2/98: simplified
// [RH] Simplified further [sin (t + 90 deg) == cos (t)]
//
fixed_t
R_PointToDist
( fixed_t	x,
  fixed_t	y )
{
	fixed_t dx = abs(x - viewx);
	fixed_t dy = abs(y - viewy);

	if (dy > dx)
	{
		fixed_t t = dx;
		dx = dy;
		dy = t;
	}

	return FixedDiv (dx, finecosine[tantoangle[FixedDiv (dy, dx) >> DBITS] >> ANGLETOFINESHIFT]);
}

//
//
// R_PointToDist2
//
//

fixed_t R_PointToDist2 (fixed_t dx, fixed_t dy)
{
	dx = abs (dx);
	dy = abs (dy);

	if (dy > dx)
	{
		fixed_t t = dx;
		dx = dy;
		dy = t;
	}

	return FixedDiv (dx, finecosine[tantoangle[FixedDiv (dy, dx) >> DBITS] >> ANGLETOFINESHIFT]);
}

//
//
// R_ScaleFromGlobalAngle
//
// Returns the texture mapping scale for the current line (horizontal span)
// at the given angle. rw_distance must be calculated first.
//
//

fixed_t R_ScaleFromGlobalAngle (angle_t visangle)
{
	fixed_t scale;

	angle_t anglea = ANG90 + (visangle - viewangle);
	angle_t angleb = ANG90 + (visangle - rw_normalangle);
	// both sines are always positive
	fixed_t num = FixedMul (FocalLengthY, finesine[angleb>>ANGLETOFINESHIFT]);
	fixed_t den = FixedMul (rw_distance, finesine[anglea>>ANGLETOFINESHIFT]);

	if (den > num>>16)
	{
		scale = FixedDiv (num, den);

		if (scale > 64*FRACUNIT)
			scale = 64*FRACUNIT;
		else if (scale < 256)
			scale = 256;
	}
	else
		scale = 64*FRACUNIT;
	return scale;
}

//
//
// R_InitTables
//
//

void R_InitTables (void)
{
	int i;
	float a, fv;

	// viewangle tangent table
	for (i = 0; i < FINEANGLES/2; i++)
	{
		a = (i-FINEANGLES/4+0.5)*PI*2/FINEANGLES;
		fv = FRACUNIT*tan (a);
		finetangent[i] = (fixed_t)fv;
	}

	// finesine table
	for (i = 0; i < 5*FINEANGLES/4; i++)
	{
		// OPTIMIZE: mirror...
		a = (i+0.5)*PI*2/FINEANGLES;
		finesine[i] = (fixed_t)(FRACUNIT * sin (a));
	}
}


//
//
// R_InitTextureMapping
//
//

void R_InitTextureMapping (void)
{
	int i, t, x;

	// Use tangent table to generate viewangletox: viewangletox will give
	// the next greatest x after the view angle.

	const fixed_t hitan = finetangent[FINEANGLES/4+FieldOfView/2];
	const fixed_t lotan = finetangent[FINEANGLES/4-FieldOfView/2];
	const int highend = viewwidth + 1;

	// Calc focallength so FieldOfView angles covers viewwidth.
	FocalLengthX = FixedDiv (centerxfrac, hitan);
	FocalLengthY = FixedDiv (FixedMul (centerxfrac, yaspectmul), hitan);

	for (i = 0; i < FINEANGLES/2; i++)
	{
		fixed_t tangent = finetangent[i];

		if (tangent > hitan)
			t = -1;
		else if (tangent < lotan)
			t = highend;
		else
		{
			t = (centerxfrac - FixedMul (tangent, FocalLengthX) + FRACUNIT - 1) >> FRACBITS;

			if (t < -1)
				t = -1;
			else if (t > highend)
				t = highend;
		}
		viewangletox[i] = t;
	}

	// Scan viewangletox[] to generate xtoviewangle[]:
	//	xtoviewangle will give the smallest view angle
	//	that maps to x.
	for (x = 0; x <= viewwidth; x++)
	{
		i = 0;
		while (viewangletox[i] > x)
			i++;
		xtoviewangle[x] = (i<<ANGLETOFINESHIFT)-ANG90;
	}

	// Take out the fencepost cases from viewangletox.
	for (i = 0; i < FINEANGLES/2; i++)
	{
		t = FixedMul (finetangent[i], FocalLengthX);
		t = centerx - t;

		if (viewangletox[i] == -1)
			viewangletox[i] = 0;
		else if (viewangletox[i] == highend)
			viewangletox[i]--;
	}

	clipangle = xtoviewangle[0];
}

//
//
// R_SetFOV
//
// Changes the field of view
//
//

void R_SetFOV (float fov)
{
	static float lastfov = 90.0f;
	if (fov != lastfov)
	{
		if (fov < 1)
			fov = 1;
		else if (fov > 179)
			fov = 179;
		if (fov == lastfov)
			return;
		lastfov = fov;
		FieldOfView = (int)(fov * (float)FINEANGLES / 360.0f);
		setsizeneeded = true;
	}
}

//
//
// R_InitLightTables
//
// Only inits the zlight table, because the scalelight table changes with
// view size.
//
// [RH] This now sets up indices into a colormap rather than pointers into
// the colormap, because the colormap can vary by sector, but the indices
// into it don't.
//
//

void R_InitLightTables (void)
{
	int i;
	int j;
	int level;
	int startmap;
	int scale;
	int lightmapsize = 8 + (screen->is8bit() ? 0 : 2);

	// Calculate the light levels to use
	//	for each level / distance combination.
	for (i = 0; i < LIGHTLEVELS; i++)
	{
		startmap = ((LIGHTLEVELS-1-i)*2)*NUMCOLORMAPS/LIGHTLEVELS;
		for (j=0 ; j<MAXLIGHTZ ; j++)
		{
			scale = FixedDiv (160*FRACUNIT, (j+1)<<LIGHTZSHIFT);
			scale >>= LIGHTSCALESHIFT-LIGHTSCALEMULBITS;
			level = startmap - scale/DISTMAP;

			if (level < 0)
				level = 0;
			else if (level >= NUMCOLORMAPS)
				level = NUMCOLORMAPS-1;

			zlight[i][j] = level << lightmapsize;
		}
	}

	lightscalexmul = ((320<<detailyshift) * (1<<LIGHTSCALEMULBITS)) / screen->width;
}

//
//
// R_SetViewSize
//
// Do not really change anything here, because it might be in the middle
// of a refresh. The change will take effect next refresh.
//
//

void R_SetViewSize (int blocks)
{
	setsizeneeded = true;
	setblocks = blocks;
}

//
//
// CVAR r_detail
//
// Selects a pixel doubling mode
//
//

CVAR_FUNC_IMPL (r_detail)
{
	static BOOL badrecovery = false;

	if (badrecovery)
	{
		badrecovery = false;
		return;
	}

	if (var < 0.0 || var > 3.0)
	{
		Printf (PRINT_HIGH, "Bad detail mode. (Use 0-3)\n");
		badrecovery = true;
		var.Set ((float)((detailyshift << 1)|detailxshift));
		return;
	}

	setdetail = (int)var;
	setsizeneeded = true;
}

//
//
// R_ExecuteSetViewSize
//
//

void R_ExecuteSetViewSize (void)
{
	int i, j;
	int level;
	int startmap;
	int virtheight, virtwidth;
	int lightmapsize = 8 + (screen->is8bit() ? 0 : 2);

	setsizeneeded = false;

	if (setdetail >= 0)
	{
		if (!r_columnmethod)
		{
			R_RenderSegLoop = R_RenderSegLoop1;
			// [RH] x-doubling only works with the standard column drawer
			detailxshift = setdetail & 1;
		}
		else
		{
			R_RenderSegLoop = R_RenderSegLoop2;
			detailxshift = 0;
		}
		detailyshift = (setdetail >> 1) & 1;
		setdetail = -1;
	}

	if (setblocks == 11 || setblocks == 12)
	{
		realviewwidth = screen->width;
		freelookviewheight = realviewheight = screen->height;
	}
	else if (setblocks == 10) {
		realviewwidth = screen->width;
		realviewheight = ST_Y;
		freelookviewheight = screen->height;
	}
	else
	{
		realviewwidth = ((setblocks*screen->width)/10) & (~(15>>(screen->is8bit() ? 0 : 2)));
		realviewheight = ((setblocks*ST_Y)/10)&~7;
		freelookviewheight = ((setblocks*screen->height)/10)&~7;
	}

	if ((&consoleplayer())->spectator && setblocks != 12) {
		DrawNewHUD = false;
		DrawNewSpecHUD = true;
	} else if (setblocks == 11) {
		DrawNewHUD = true;
		DrawNewSpecHUD = false;
	} else {
		DrawNewHUD = false;
		DrawNewSpecHUD = false;
	}

	viewwidth = realviewwidth >> detailxshift;
	viewheight = realviewheight >> detailyshift;
	freelookviewheight >>= detailyshift;

	{
		char temp[16];

		sprintf (temp, "%d x %d", viewwidth, viewheight);
		r_viewsize.ForceSet (temp);
	}

	lastcenteryfrac = 1<<30;
	centery = viewheight/2;
	centerx = viewwidth/2;
	centerxfrac = centerx<<FRACBITS;
	centeryfrac = centery<<FRACBITS;

	virtwidth = screen->width >> detailxshift;
	virtheight = screen->height >> detailyshift;

	yaspectmul = (fixed_t)(65536.0f*(320.0f*(float)virtheight/(200.0f*(float)virtwidth)));

	colfunc = basecolfunc = R_DrawColumn;
	lucentcolfunc = R_DrawTranslucentColumn;
	fuzzcolfunc = R_DrawFuzzColumn;
	transcolfunc = R_DrawTranslatedColumn;
	tlatedlucentcolfunc = R_DrawTlatedLucentColumn;
	spanfunc = R_DrawSpan;

	// [RH] Horizontal column drawers
	hcolfunc_pre = R_DrawColumnHoriz;
	hcolfunc_post1 = rt_map1col;
	hcolfunc_post2 = rt_map2cols;
	hcolfunc_post4 = rt_map4cols;

	R_InitBuffer (viewwidth, viewheight);
	R_InitTextureMapping ();

	// psprite scales
	pspritexscale = centerxfrac / 160;
	pspriteyscale = FixedMul (pspritexscale, yaspectmul);
	pspritexiscale = FixedDiv (FRACUNIT, pspritexscale);

	// [RH] Sky height fix for screens not 200 (or 240) pixels tall
	R_InitSkyMap ();

	// thing clipping
	for (i = 0; i < viewwidth; i++)
		screenheightarray[i] = (short)viewheight;

	// planes
	for (i = 0; i < viewwidth; i++)
	{
		fixed_t cosadj = abs (finecosine[xtoviewangle[i]>>ANGLETOFINESHIFT]);
		distscale[i] = FixedDiv (FRACUNIT, cosadj);
	}

	// Calculate the light levels to use for each level / scale combination.
	// [RH] This just stores indices into the colormap rather than pointers to a specific one.
	for (i = 0; i < LIGHTLEVELS; i++)
	{
		startmap = ((LIGHTLEVELS-1-i)*2)*NUMCOLORMAPS/LIGHTLEVELS;
		for (j = 0; j < MAXLIGHTSCALE; j++)
		{
			level = startmap - (j*(screen->width>>detailxshift))/((viewwidth*DISTMAP));
			if (level < 0)
				level = 0;
			else if (level >= NUMCOLORMAPS)
				level = NUMCOLORMAPS-1;

			scalelight[i][j] = level << lightmapsize;
		}
	}

	// [RH] Initialize z-light tables here
	R_InitLightTables ();
}

//
//
// CVAR screenblocks
//
// Selects the size of the visible window
//
//

CVAR_FUNC_IMPL (screenblocks)
{
	if (var > 12.0)
		var.Set (12.0);
	else if (var < 3.0)
		var.Set (3.0);
	else
		R_SetViewSize ((int)var);
}

//
//
// CVAR r_columnmethod
//
// Selects which version of the seg renderers to use.
//
//

// [ML] Disabled 16/3/06, now always 0 (Original)
// [Russell] Reenabled 14/3/07, fixes smudging of graphics
//BEGIN_CUSTOM_CVAR (r_columnmethod, "1", CVAR_ARCHIVE)
//{
    /*
	if (var != 0 && var != 1)
		var.Set (1);
	else
		// Trigger the change
		r_detail.Callback ();
    */
//}
//END_CUSTOM_CVAR (r_columnmethod)
EXTERN_CVAR (r_columnmethod)
//
//
// R_Init
//
//

void R_Init (void)
{
	R_InitData ();
	R_InitPointToAngle ();
	R_InitTables ();
	// viewwidth / viewheight are set by the defaults

	R_SetViewSize ((int)screenblocks);
	R_InitPlanes ();
	R_InitLightTables ();
	R_InitTranslationTables ();

	framecount = 0;
}

// R_Shutdown
void R_Shutdown (void)
{
    R_FreeTranslationTables();
}

//
//
// R_PointInSubsector
//
//

subsector_t *R_PointInSubsector (fixed_t x, fixed_t y)
{
	node_t *node;
	int side;
	int nodenum;

	// single subsector is a special case
	if (!numnodes)
		return subsectors;

	nodenum = numnodes-1;

	while (! (nodenum & NF_SUBSECTOR) )
	{
		node = &nodes[nodenum];
		side = R_PointOnSide (x, y, node);
		nodenum = node->children[side];
	}

	return &subsectors[nodenum & ~NF_SUBSECTOR];
}

//
//
// R_SetupFrame
//
//

void R_SetupFrame (player_t *player)
{
	unsigned int newblend;

	camera = player->camera;	// [RH] Use camera instead of viewplayer

	if(!camera || !camera->subsector)
		return;

	if (player->cheats & CF_CHASECAM)
	{
		// [RH] Use chasecam view
		P_AimCamera (camera);
		viewx = CameraX;
		viewy = CameraY;
		viewz = CameraZ;
	}
	else
	{
		viewx = camera->x;
		viewy = camera->y;
		viewz = camera->player ? camera->player->viewz : camera->z;
	}

	viewangle = camera->angle + viewangleoffset;
	extralight = camera == player->mo ? player->extralight : 0;

	viewsin = finesine[viewangle>>ANGLETOFINESHIFT];
	viewcos = finecosine[viewangle>>ANGLETOFINESHIFT];

	// killough 3/20/98, 4/4/98: select colormap based on player status
	// [RH] Can also select a blend

	if (camera->subsector->sector->heightsec)
	{
		const sector_t *s = camera->subsector->sector->heightsec;
		newblend = viewz < s->floorheight ? s->bottommap : viewz > s->ceilingheight ?
				   s->topmap : s->midmap;
		if (!screen->is8bit())
			newblend = R_BlendForColormap (newblend);
		else if (APART(newblend) == 0 && newblend >= numfakecmaps)
			newblend = 0;
	}
	else
	{
		newblend = 0;
	}

	// [RH] Don't override testblend unless entering a sector with a
	//		blend different from the previous sector's. Same goes with
	//		NormalLight's maps pointer.
	if (R_OldBlend != newblend)
	{
		R_OldBlend = newblend;
		if (APART(newblend))
		{
			BaseBlendR = RPART(newblend);
			BaseBlendG = GPART(newblend);
			BaseBlendB = BPART(newblend);
			BaseBlendA = APART(newblend) / 255.0f;
			NormalLight.maps = realcolormaps;
		}
		else
		{
			NormalLight.maps = realcolormaps + (NUMCOLORMAPS+1)*256*newblend;
			BaseBlendR = BaseBlendG = BaseBlendB = 0;
			BaseBlendA = 0.0f;
		}
	}

	fixedcolormap = NULL;
	fixedlightlev = 0;

	if (camera == player->mo && player->fixedcolormap)
	{
		if (player->fixedcolormap < NUMCOLORMAPS)
		{
			fixedlightlev = player->fixedcolormap*256;
			fixedcolormap = DefaultPalette->maps.colormaps;
		}
		else
		{
			if (screen->is8bit())
				fixedcolormap =
					DefaultPalette->maps.colormaps
					+ player->fixedcolormap*256;
			else
				fixedcolormap = (lighttable_t *)
					(DefaultPalette->maps.shades
					+ player->fixedcolormap*256);
		}

		walllights = scalelightfixed;

		memset (scalelightfixed, 0, MAXLIGHTSCALE*sizeof(*scalelightfixed));
	}

	// [RH] freelook stuff
	{
		fixed_t dy = FixedMul (FocalLengthY, finetangent[(ANG90-camera->pitch)>>ANGLETOFINESHIFT]);

		centeryfrac = (viewheight << (FRACBITS-1)) + dy;
		centery = centeryfrac >> FRACBITS;

		//centeryfrac &= 0xffff0000;
		int i = lastcenteryfrac - centeryfrac;
		if (i != 0)
		{
			int e;

			if (i & (FRACUNIT-1) == 0)	// Unlikely, but possible
			{
				i >>= FRACBITS;
				if (abs (i) < viewheight)
				{
					fixed_t *from, *to;
					if (i > 0)
					{
//						memmove (yslope, yslope + i, (viewheight - i) * sizeof(fixed_t));
						int index = 0;
						from = yslope + i;
						to = yslope;
						i = e = viewheight - i;
						do
						{
							*(to + index) = *(from + index);
							index++;
						} while (--e);
						e = viewheight;
					}
					else
					{
//						memmove (yslope - i, yslope, (viewheight + i) * sizeof(fixed_t));
						from = yslope;
						to = yslope - i;
						e = viewheight + i - 1;
						do
						{
							*(to + e) = *(from + e);
						} while (--e >= 0);
						e = -i;
						i = 0;
					}
				}
				else
				{
					i = 0;
					e = viewheight;
				}
			}
			else
			{
				i = 0;
				e = viewheight;
			}

			{
				fixed_t focus = FocalLengthY;
				fixed_t den;
				if (i < centery)
				{
					den = centeryfrac - (i << FRACBITS) - FRACUNIT/2;
					if (e <= centery)
					{
						do {
							yslope[i] = FixedDiv (focus, den);
							den -= FRACUNIT;
						} while (++i < e);
					}
					else
					{
						do {
							yslope[i] = FixedDiv (focus, den);
							den -= FRACUNIT;
						} while (++i < centery);
						den = (i << FRACBITS) - centeryfrac + FRACUNIT/2;
						do {
							yslope[i] = FixedDiv (focus, den);
							den += FRACUNIT;
						} while (++i < e);
					}
				}
				else
				{
					den = (i << FRACBITS) - centeryfrac + FRACUNIT/2;
					do {
						yslope[i] = FixedDiv (focus, den);
						den += FRACUNIT;
					} while (++i < e);
				}
			}
		}
		lastcenteryfrac = centeryfrac;
	}

	framecount++;
	validcount++;
}

//
//
// R_RenderView
//
//

void R_RenderPlayerView (player_t *player)
{
	R_SetupFrame (player);

	// Clear buffers.
	R_ClearClipSegs ();
	R_ClearDrawSegs ();
	R_ClearPlanes ();
	R_ClearSprites ();

	// [RH] Show off segs if r_drawflat is 1
	if (r_drawflat)
	{
		hcolfunc_pre = R_FillColumnHorizP;
		hcolfunc_post1 = rt_copy1col;
		hcolfunc_post2 = rt_copy2cols;
		hcolfunc_post4 = rt_copy4cols;
		colfunc = R_FillColumnP;
		spanfunc = R_FillSpan;
	}
	else
	{
		hcolfunc_pre = R_DrawColumnHoriz;
		colfunc = basecolfunc;
		hcolfunc_post1 = rt_map1col;
		hcolfunc_post2 = rt_map2cols;
		hcolfunc_post4 = rt_map4cols;
	}

	// Never draw the player unless in chasecam mode
	if(camera)
	if (camera->player && !(player->cheats & CF_CHASECAM))
	{
		R_RenderBSPNode (numnodes - 1);
	}
	else
		R_RenderBSPNode (numnodes-1);	// The head node is the last node output.

	R_DrawPlanes ();

	spanfunc = R_DrawSpan;

	R_DrawMasked ();

	// [RH] Apply detail mode doubling
	//R_DetailDouble ();
}

//
//
// R_MultiresInit
//
// Called from V_SetResolution()
//
//

void R_MultiresInit (void)
{
	int i;

	// in r_things.c
	extern short *r_dscliptop, *r_dsclipbot;
	// in r_draw.c
	extern byte **ylookup;
	extern int *columnofs;

	// [Russell] - Possible bug, ylookup is 2 star.
    M_Free(ylookup);
    M_Free(columnofs);
    M_Free(r_dscliptop);
    M_Free(r_dsclipbot);
    M_Free(negonearray);
    M_Free(screenheightarray);
    M_Free(xtoviewangle);
    
	ylookup = (byte **)M_Malloc (screen->height * sizeof(byte *));
	columnofs = (int *)M_Malloc (screen->width * sizeof(int));
	r_dscliptop = (short *)M_Malloc (screen->width * sizeof(short));
	r_dsclipbot = (short *)M_Malloc (screen->width * sizeof(short));
	
	// Moved from R_InitSprites()
	negonearray = (short *)M_Malloc (sizeof(short) * screen->width);

	// These get set in R_ExecuteSetViewSize()
	screenheightarray = (short *)M_Malloc (sizeof(short) * screen->width);
	xtoviewangle = (angle_t *)M_Malloc (sizeof(angle_t) * (screen->width + 1));
	
	// GhostlyDeath -- Clean up the buffers
	memset(ylookup, 0, screen->height * sizeof(byte*));
	memset(columnofs, 0, screen->width * sizeof(int));
	memset(r_dscliptop, 0, screen->width * sizeof(short));
	memset(r_dsclipbot, 0, screen->width * sizeof(short));

	for(i = 0; i < screen->width; i++)
	{
		negonearray[i] = -1;
	}
    memset(screenheightarray, 0, screen->width * sizeof(short));
    memset(xtoviewangle, 0, screen->width * sizeof(angle_t) + 1);

	R_InitFuzzTable ();
	R_PlaneInitData ();
	R_OldBlend = ~0;
}

VERSION_CONTROL (r_main_cpp, "$Id$")


