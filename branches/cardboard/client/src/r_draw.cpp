// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id: r_draw.cpp 165 2007-03-08 18:43:19Z denis $
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
//		The actual span/column drawing functions.
//		Here find the main potential for optimization,
//		 e.g. inline assembly, different algorithms.
//
//-----------------------------------------------------------------------------

#include <stddef.h>

#include "m_alloc.h"
#include "doomdef.h"
#include "i_system.h"
#include "z_zone.h"
#include "w_wad.h"
#include "r_local.h"
#include "v_video.h"
#include "doomstat.h"
#include "st_stuff.h"

#include "gi.h"

#undef RANGECHECK

// status bar height at bottom of screen
// [RH] status bar position at bottom of screen
extern	int		ST_Y;

//
// All drawing to the view buffer is accomplished in this file.
// The other refresh files only know about ccordinates,
//	not the architecture of the frame buffer.
// Conveniently, the frame buffer is a linear one,
//	and we need only the base address,
//	and the total size == width*height*depth/8.,
//


byte*			viewimage;
extern "C" {
int 			viewwidth;
int 			viewheight;
}
int 			scaledviewwidth;
int 			scaledviewheight;
int 			viewwindowx;
int 			viewwindowy;
byte**			ylookup;
int* 			columnofs;

extern "C" {
int				realviewwidth;		// [RH] Physical width of view window
int				realviewheight;		// [RH] Physical height of view window
int				detailxshift;		// [RH] X shift for horizontal detail level
int				detailyshift;		// [RH] Y shift for vertical detail level
}
/*
// [RH] Pointers to the different column drawers.
//		These get changed depending on the current
//		screen depth.
void (*R_DrawColumnHoriz)(void);
void (*R_DrawColumn)(void);
void (*R_DrawFuzzColumn)(void);
void (*R_DrawTranslucentColumn)(void);
void (*R_DrawTranslatedColumn)(void);
void (*R_DrawSpan)(void);
void (*rt_map4cols)(int,int,int);
*/

// Color tables for different players,
//  translate a limited part to another
//  (color ramps used for  suit colors).
//
 
byte *tranmap;          // translucency filter maps 256x256   // phares 
byte *main_tranmap;     // killough 4/11/98

//
// R_DrawColumn
// Source is the top of the column to scale.
//
extern "C" {
int				dc_pitch=0x12345678;	// [RH] Distance between rows

lighttable_t*	dc_colormap; 
int 			dc_x; 
int 			dc_yl; 
int 			dc_yh; 
fixed_t 		dc_iscale; 
fixed_t 		dc_texturemid;
fixed_t			dc_textureheight;
//int			dc_color;				// [RH] Color for column filler

// first pixel in a column (possibly virtual) 
byte*			dc_source;				

// [RH] Tutti-Frutti fix
//unsigned int	dc_mask;

// just for profiling 
//int 			dccount;

// Fuzz stuffs

const int fuzzoffset[FUZZTABLE] = 
{
  1,0,1,0,1,1,0,
  1,1,0,1,1,1,0,
  1,1,1,0,0,0,0,
  1,0,0,1,1,1,1,0,
  1,0,1,1,0,0,1,
  1,0,0,0,0,1,1,
  1,1,0,1,1,0,1 
}; 

int fuzzpos = 0; 
}


/************************************/
/*									*/
/* Palettized drawers (C versions)	*/
/*									*/
/************************************/

#ifndef	USEASM
//
// A column is a vertical slice/span from a wall texture that,
//	given the DOOM style restrictions on the view orientation,
//	will always have constant z depth.
// Thus a special case loop for very fast rendering can
//	be used. It has also been used with Wolfenstein 3D.
// 
/*
void R_DrawColumnP_C (void)
{
	int 				count;
	byte*				dest;
	fixed_t 			frac;
	fixed_t 			fracstep;

	count = dc_yh - dc_yl;

	// Zero length, column does not exceed a pixel.
	if (count < 0)
		return;

	count++;

#ifdef RANGECHECK 
	if (dc_x >= screen->width
		|| dc_yl < 0
		|| dc_yh >= screen->height) {
		Printf (PRINT_HIGH, "R_DrawColumnP_C: %i to %i at %i\n", dc_yl, dc_yh, dc_x);
		return;
	}
#endif

	// Framebuffer destination address.
	// Use ylookup LUT to avoid multiply with ScreenWidth.
	// Use columnofs LUT for subwindows?
	dest = ylookup[dc_yl] + columnofs[dc_x];

	// Determine scaling,
	//	which is the only mapping to be done.
	fracstep = dc_iscale; 
	frac = dc_texturefrac;

	{
		// [RH] Get local copies of these variables so that the compiler
		//		has a better chance of optimizing this well.
		byte *colormap = dc_colormap;
		int mask = dc_mask;
		byte *source = dc_source;
		int pitch = dc_pitch;

		// Inner loop that does the actual texture mapping,
		//	e.g. a DDA-lile scaling.
		// This is as fast as it gets.
		do
		{
			// Re-map color indices from wall texture column
			//	using a lighting/special effects LUT.
			*dest = colormap[source[(frac>>FRACBITS)&mask]];

			dest += pitch;
			frac += fracstep;

		} while (--count);
	}
} 
*/

#endif	// USEASM

/*
// [RH] Same as R_DrawColumnP_C except that it doesn't do any colormapping.
//		Used by the sky drawer because the sky is always fullbright.
void R_StretchColumnP_C (void)
{
	int 				count;
	byte*				dest;
	fixed_t 			frac;
	fixed_t 			fracstep;

	count = dc_yh - dc_yl;

	if (count < 0)
		return;

	count++;

#ifdef RANGECHECK 
	if (dc_x >= screen->width
		|| dc_yl < 0
		|| dc_yh >= screen->height) {
		Printf (PRINT_HIGH, "R_StretchColumnP_C: %i to %i at %i\n", dc_yl, dc_yh, dc_x);
		return;
	}
#endif

	dest = ylookup[dc_yl] + columnofs[dc_x];
	fracstep = dc_iscale; 
	frac = dc_texturefrac;

	{
		int mask = dc_mask;
		byte *source = dc_source;
		int pitch = dc_pitch;

		do
		{
			*dest = source[(frac>>FRACBITS)&mask];
			dest += pitch;
			frac += fracstep;
		} while (--count);
	}
} 

// [RH] Just fills a column with a color
void R_FillColumnP (void)
{
	int 				count;
	byte*				dest;

	count = dc_yh - dc_yl;

	if (count < 0)
		return;

	count++;

#ifdef RANGECHECK 
	if (dc_x >= screen->width
		|| dc_yl < 0
		|| dc_yh >= screen->height) {
		Printf (PRINT_HIGH, "R_StretchColumnP_C: %i to %i at %i\n", dc_yl, dc_yh, dc_x);
		return;
	}
#endif

	dest = ylookup[dc_yl] + columnofs[dc_x];

	{
		int pitch = dc_pitch;
		byte color = dc_color;

		do
		{
			*dest = color;
			dest += pitch;
		} while (--count);
	}
} 
*/

void CB_DrawColumn_8(void)
{
   int count;
   register byte *dest;
   register fixed_t frac;
   fixed_t fracstep;

   count = column.y2 - column.y1 + 1;
   if(count <= 0) return;

#ifdef RANGECHECK 
   if(column.x < 0 || column.x >= v_width || column.y1 < 0 || column.y2 >= v_height)
      I_Error("CB_DrawColumn_8: %i to %i at %i", column.y1, column.y2, column.x);    
#endif 

   dest = ylookup[column.y1] + columnofs[column.x];
   fracstep = column.step;
   frac = column.texmid + (int)((column.y1 - view.ycenter + 1) * fracstep);

   {
      register byte *source = (byte *)(column.source);
      register lighttable_t *colormap = column.colormap;
      register int heightmask = column.texheight - 1;
      
      if(column.texheight & heightmask)
      {
         heightmask++;
         heightmask <<= FRACBITS;

         if(frac < 0)
            while((frac += heightmask) <  0);
         else
            while(frac >= heightmask)
               frac -= heightmask;

         do
         {
            *dest = colormap[source[frac>>FRACBITS]];
            dest += linesize;                     // killough 11/98
            if((frac += fracstep) >= heightmask)
               frac -= heightmask;
         } 
         while(--count);
      }
      else
      {
         while((count -= 2) >= 0) // texture height is a power of 2 -- killough
         {
            *dest = colormap[source[(frac>>FRACBITS) & heightmask]];
            dest += linesize;   // killough 11/98
            frac += fracstep;
            *dest = colormap[source[(frac>>FRACBITS) & heightmask]];
            dest += linesize;   // killough 11/98
            frac += fracstep;
         }
         if(count & 1)
            *dest = colormap[source[(frac>>FRACBITS) & heightmask]];
      }
   }
}
//
// Spectre/Invisibility.
//
/*
void R_InitFuzzTable (void)
{
	int i;
	int fuzzoff;

	screen->Lock ();
	fuzzoff = FUZZOFF << detailyshift;
	screen->Unlock ();

	for (i = 0; i < FUZZTABLE; i++)
		fuzzoffset[i] = fuzzinit[i] * fuzzoff;
}
*/
#ifndef USEASM
//
// Framebuffer postprocessing.
// Creates a fuzzy image by copying pixels
//	from adjacent ones to left and right.
// Used with an all black colormap, this
//	could create the SHADOW effect,
//	i.e. spectres and invisible players.
//
/*
void R_DrawFuzzColumnP_C (void)
{
	int count;
	byte *dest;

	// Adjust borders. Low...
	if (!dc_yl)
		dc_yl = 1;

	// .. and high.
	if (dc_yh == realviewheight-1)
		dc_yh = realviewheight - 2;

	count = dc_yh - dc_yl;

	// Zero length.
	if (count < 0)
		return;

	count++;

#ifdef RANGECHECK
	if (dc_x >= screen->width
		|| dc_yl < 0 || dc_yh >= screen->height)
	{
		I_Error ("R_DrawFuzzColumnP_C: %i to %i at %i",
				 dc_yl, dc_yh, dc_x);
	}
#endif


	dest = ylookup[dc_yl] + columnofs[dc_x];

	// Looks like an attempt at dithering,
	//	using the colormap #6 (of 0-31, a bit
	//	brighter than average).
	{
		// [RH] Make local copies of global vars to try and improve
		//		the optimizations made by the compiler.
		int pitch = dc_pitch;
		int fuzz = fuzzpos;
		byte *map = DefaultPalette->maps.colormaps + 6*256;

		do 
		{
			// Lookup framebuffer, and retrieve
			//	a pixel that is either one column
			//	left or right of the current one.
			// Add index from colormap to index.
			*dest = map[dest[fuzzoffset[fuzz]]]; 

			// Clamp table lookup index.
			fuzz = (fuzz + 1) & (FUZZTABLE - 1);
			
			dest += pitch;
		} while (--count);

		fuzzpos = (fuzz + 3) & (FUZZTABLE - 1);
	}
} 
*/
#endif	// USEASM

// sf: restored original fuzz effect (changed in mbf)
// sf: changed to use vis->colormap not fullcolormap
//     for coloured lighting and SHADOW now done with
//     flags not NULL colormap

void CB_DrawFuzzColumn_8(void)
{
   int count;
   register byte *dest;

   // Adjust borders. Low...
   if(!column.y1) 
      column.y1 = 1;
   
   // .. and high.
   if(column.y2 == viewheight - 1) 
      column.y2 = viewheight - 2; 

   count = column.y2 - column.y1 + 1;
   if(count <= 0) return;

#ifdef RANGECHECK 
   if(column.x < 0 || column.x >= v_width || column.y1 < 0 || column.y2 >= v_height)
      I_Error("CB_DrawFuzzColumn_8: %i to %i at %i", column.y1, column.y2, column.x);    
#endif 

   dest = ylookup[column.y1] + columnofs[column.x];

   {
      register lighttable_t *colormap = column.colormap;
      
      while((count -= 2) >= 0) // texture height is a power of 2 -- killough
      {
         *dest = colormap[6*256+dest[fuzzoffset[fuzzpos] ? linesize : -linesize]];
         if(++fuzzpos == FUZZTABLE) fuzzpos = 0;
         dest += linesize;   // killough 11/98

         *dest = colormap[6*256+dest[fuzzoffset[fuzzpos] ? linesize : -linesize]];
         if(++fuzzpos == FUZZTABLE) fuzzpos = 0;
         dest += linesize;   // killough 11/98
      }
      if(count & 1)
      {
         *dest = colormap[6*256+dest[fuzzoffset[fuzzpos] ? linesize : -linesize]];
         if(++fuzzpos == FUZZTABLE) fuzzpos = 0;
      }
   }
}
//
// R_DrawTranlucentColumn
//
//fixed_t dc_translevel;

/*
[RH] This translucency algorithm is based on DOSDoom 0.65's, but uses
a 32k RGB table instead of an 8k one. At least on my machine, it's
slightly faster (probably because it uses only one shift instead of
two), and it looks considerably less green at the ends of the
translucency range. The extra size doesn't appear to be an issue.

The following note is from DOSDoom 0.65:

New translucency algorithm, by Erik Sandberg:

Basically, we compute the red, green and blue values for each pixel, and
then use a RGB table to check which one of the palette colours that best
represents those RGB values. The RGB table is 8k big, with 4 R-bits,
5 G-bits and 4 B-bits. A 4k table gives a bit too bad precision, and a 32k
table takes up more memory and results in more cache misses, so an 8k
table seemed to be quite ultimate.

The computation of the RGB for each pixel is accelerated by using two
1k tables for each translucency level.
The xth element of one of these tables contains the r, g and b values for
the colour x, weighted for the current translucency level (for example,
the weighted rgb values for background colour at 75% translucency are 1/4
of the original rgb values). The rgb values are stored as three
low-precision fixed point values, packed into one long per colour:
Bit 0-4:   Frac part of blue  (5 bits)
Bit 5-8:   Int  part of blue  (4 bits)
Bit 9-13:  Frac part of red   (5 bits)
Bit 14-17: Int  part of red   (4 bits)
Bit 18-22: Frac part of green (5 bits)
Bit 23-27: Int  part of green (5 bits)
Bit 28-31: All zeros          (4 bits)

The point of this format is that the two colours now can be added, and
then be converted to a RGB table index very easily: First, we just set
all the frac bits and the four upper zero bits to 1. It's now possible
to get the RGB table index by anding the current value >> 5 with the
current value >> 19. When asm-optimised, this should be the fastest
algorithm that uses RGB tables.

*/
/*
void R_DrawTranslucentColumnP_C (void)
{
	int count;
	byte *dest;
	fixed_t frac;
	fixed_t fracstep;
	unsigned int *fg2rgb, *bg2rgb;

	count = dc_yh - dc_yl;
	if (count < 0)
		return;
	count++;

#ifdef RANGECHECK
	if (dc_x >= screen->width
		|| dc_yl < 0
		|| dc_yh >= screen->height)
	{
		I_Error ( "R_DrawTranslucentColumnP_C: %i to %i at %i",
				  dc_yl, dc_yh, dc_x);
	}
#endif 

	{
		fixed_t fglevel, bglevel;

		fglevel = dc_translevel & ~0x3ff;
		bglevel = FRACUNIT-fglevel;
		fg2rgb = Col2RGB8[fglevel>>10];
		bg2rgb = Col2RGB8[bglevel>>10];
	}

	dest = ylookup[dc_yl] + columnofs[dc_x];

	fracstep = dc_iscale;
	frac = dc_texturefrac;

	{
		byte *colormap = dc_colormap;
		byte *source = dc_source;
		int mask = dc_mask;
		int pitch = dc_pitch;

		do
		{
			unsigned int fg = colormap[source[(frac>>FRACBITS)&mask]];
			unsigned int bg = *dest;

			fg = fg2rgb[fg];
			bg = bg2rgb[bg];
			fg = (fg+bg) | 0x1f07c1f;
			*dest = RGB32k[0][0][fg & (fg>>15)];
			dest += pitch;
			frac += fracstep;
		} while (--count);
	}
}
*/
// Here is the version of R_DrawColumn that deals with translucent  // phares
// textures and sprites. It's identical to R_DrawColumn except      //    |
// for the spot where the color index is stuffed into *dest. At     //    V
// that point, the existing color index and the new color index
// are mapped through the TRANMAP lump filters to get a new color
// index whose RGB values are the average of the existing and new
// colors.
//
// Since we're concerned about performance, the 'translucent or
// opaque' decision is made outside this routine, not down where the
// actual code differences are.

// haleyjd 04/10/04: FIXME -- ASM version of R_DrawTLColumn is out
// of sync currently.

//#ifndef USEASM                       // killough 2/21/98: converted to x86 asm

#define SRCPIXEL \
   tranmap[(*dest<<8)+colormap[source[(frac>>FRACBITS) & heightmask]]]
   
void CB_DrawTLColumn_8(void)
{
   int count;
   register byte *dest;
   register fixed_t frac;
   fixed_t fracstep;

   count = column.y2 - column.y1 + 1;
   if(count <= 0) return;

#ifdef RANGECHECK 
   if(column.x < 0 || column.x >= v_width || column.y1 < 0 || column.y2 >= v_height)
      I_Error("CB_DrawTLColumn_8: %i to %i at %i", column.y1, column.y2, column.x);    
#endif 

   dest = ylookup[column.y1] + columnofs[column.x];
   fracstep = column.step;
   frac = column.texmid + (int)((column.y1 - view.ycenter + 1) * fracstep);

   // killough 2/1/98, 2/21/98: more performance tuning
   {
      register byte *source = (byte *)(column.source);
      register lighttable_t *colormap = column.colormap;
      register int heightmask = column.texheight - 1;
      
      if(column.texheight & heightmask)
      {
         heightmask++;
         heightmask <<= FRACBITS;

         if(frac < 0)
            while((frac += heightmask) <  0);
         else
            while(frac >= heightmask)
               frac -= heightmask;

         do
         {
            *dest = tranmap[(*dest<<8) + colormap[source[frac>>FRACBITS]]]; // phares
            dest += linesize;          // killough 11/98
            if((frac += fracstep) >= heightmask)
               frac -= heightmask;
         } 
         while(--count);
      }
      else
      {
         while((count -= 2) >= 0) // texture height is a power of 2 -- killough
         {
            *dest = tranmap[(*dest<<8)+colormap[source[(frac>>FRACBITS) & heightmask]]]; // phares
            dest += linesize;   // killough 11/98
            frac += fracstep;
            *dest = tranmap[(*dest<<8)+colormap[source[(frac>>FRACBITS) & heightmask]]]; // phares
            dest += linesize;   // killough 11/98
            frac += fracstep;
         }
         if(count & 1)
            *dest = tranmap[(*dest<<8)+colormap[source[(frac>>FRACBITS) & heightmask]]]; // phares
      }
   }
}

#undef SRCPIXEL

#define SRCPIXEL \
   tranmap[(*dest<<8) + colormap[column.translation[source[frac>>FRACBITS]]]]

#define SRCPIXEL_MASK \
   tranmap[(*dest<<8) + \
      colormap[column.translation[source[(frac>>FRACBITS) & heightmask]]]]

void CB_DrawTLTRColumn_8(void)
{
   int count;
   register byte *dest;
   register fixed_t frac;
   fixed_t fracstep;

   count = column.y2 - column.y1 + 1;
   if(count <= 0) return;

#ifdef RANGECHECK 
   if(column.x < 0 || column.x >= v_width || column.y1 < 0 || column.y2 >= v_height)
      I_Error("CB_DrawTLTRColumn_8: %i to %i at %i", column.y1, column.y2, column.x);    
#endif 

   dest = ylookup[column.y1] + columnofs[column.x];
   fracstep = column.step;
   frac = column.texmid + (int)((column.y1 - view.ycenter + 1) * fracstep);

   {
      register byte *source = (byte *)(column.source);
      register lighttable_t *colormap = column.colormap;
      register int heightmask = column.texheight - 1;
      
      if(column.texheight & heightmask)
      {
         heightmask++;
         heightmask <<= FRACBITS;

         if(frac < 0)
            while((frac += heightmask) <  0);
         else
            while(frac >= heightmask)
               frac -= heightmask;

         do
         {
            *dest = tranmap[(*dest<<8) + colormap[column.translation[source[frac>>FRACBITS]]]]; // phares
            dest += linesize;          // killough 11/98
            if((frac += fracstep) >= heightmask)
               frac -= heightmask;
         } 
         while(--count);
      }
      else
      {
         while((count -= 2) >= 0) // texture height is a power of 2 -- killough
         {
            *dest = tranmap[(*dest<<8)+colormap[column.translation[source[(frac>>FRACBITS) & heightmask]]]]; // phares
            dest += linesize;   // killough 11/98
            frac += fracstep;
            *dest = tranmap[(*dest<<8)+colormap[column.translation[source[(frac>>FRACBITS) & heightmask]]]]; // phares
            dest += linesize;   // killough 11/98
            frac += fracstep;
         }
         if(count & 1)
            *dest = tranmap[(*dest<<8)+colormap[column.translation[source[(frac>>FRACBITS) & heightmask]]]]; // phares
      }
   }
}
#undef SRCPIXEL
#undef SRCPIXEL_MASK
//
// R_DrawTranslatedColumn
// Used to draw player sprites
//	with the green colorramp mapped to others.
// Could be used with different translation
//	tables, e.g. the lighter colored version
//	of the BaronOfHell, the HellKnight, uses
//	identical sprites, kinda brightened up.
//
//byte*	dc_translation;
byte **translationtables = NULL;

/*
void R_DrawTranslatedColumnP_C (void)
{ 
	int 				count;
	byte*				dest;
	fixed_t 			frac;
	fixed_t 			fracstep;

	count = dc_yh - dc_yl;
	if (count < 0) 
		return;
	count++;

#ifdef RANGECHECK 
	if (dc_x >= screen->width
		|| dc_yl < 0
		|| dc_yh >= screen->height)
	{
		I_Error ( "R_DrawTranslatedColumnP_C: %i to %i at %i",
				  dc_yl, dc_yh, dc_x);
	}
	
#endif 

	dest = ylookup[dc_yl] + columnofs[dc_x];

	fracstep = dc_iscale;
	frac = dc_texturefrac;

	{
		// [RH] Local copies of global vars to improve compiler optimizations
		byte *colormap = dc_colormap;
		byte *translation = dc_translation;
		byte *source = dc_source;
		int pitch = dc_pitch;
		int mask = dc_mask;

		do
		{
			*dest = colormap[translation[source[(frac>>FRACBITS) & mask]]];
			dest += pitch;

			frac += fracstep;
		} while (--count);
	}
}
*/

// haleyjd: new stuff
int firsttranslationlump, lasttranslationlump;
int numtranslations = 0;

#define SRCPIXEL \
   colormap[column.translation[source[(frac>>FRACBITS) & heightmask]]]

void CB_DrawTRColumn_8(void)
{
   int count;
   register byte *dest;
   register fixed_t frac;
   fixed_t fracstep;

   count = column.y2 - column.y1 + 1;
   if(count <= 0) return;

#ifdef RANGECHECK 
   if(column.x  < 0 || column.x  >= video.width || 
      column.y1 < 0 || column.y2 >= video.height)
      I_Error("CB_DrawTRColumn_8: %i to %i at %i", column.y1, column.y2, column.x);    
#endif 

   dest = ylookup[column.y1] + columnofs[column.x];
   fracstep = column.step;
   frac = column.texmid + (int)((column.y1 - view.ycenter + 1) * fracstep);

   {
      register byte *source = (byte *)(column.source);
      register lighttable_t *colormap = column.colormap;
      register int heightmask = column.texheight - 1;
      
      if(column.texheight & heightmask)
      {
         heightmask++;
         heightmask <<= FRACBITS;

         if(frac < 0)
            while((frac += heightmask) <  0);
         else
            while(frac >= heightmask)
               frac -= heightmask;

         do
         {
            *dest = colormap[column.translation[source[frac>>FRACBITS]]]; // phares
            dest += linesize;                     // killough 11/98
            if((frac += fracstep) >= heightmask)
               frac -= heightmask;
         } 
         while(--count);
      }
      else
      {
         while((count -= 2) >= 0) // texture height is a power of 2 -- killough
         {
            *dest = SRCPIXEL; // phares
            dest += linesize;   // killough 11/98
            frac += fracstep;
            *dest = SRCPIXEL;
            dest += linesize;   // killough 11/98
            frac += fracstep;
         }
         if(count & 1)
            *dest = SRCPIXEL;
      }
   }
}

#undef SRCPIXEL

//
// R_DrawFlexColumn
//
// haleyjd 09/01/02: zdoom-style translucency
//
void CB_DrawFlexColumn_8(void)
{
   int count;
   register byte *dest;
   register fixed_t frac;
   fixed_t fracstep;
   unsigned int *fg2rgb, *bg2rgb;
   register unsigned int fg, bg;

   count = column.y2 - column.y1 + 1;
   if(count <= 0) return;

#ifdef RANGECHECK 
   if(column.x  < 0 || column.x  >= video.width || 
      column.y1 < 0 || column.y2 >= video.height)
      I_Error("CB_DrawFlexColumn_8: %i to %i at %i", column.y1, column.y2, column.x);    
#endif 

   {
      fixed_t fglevel, bglevel;
      
      fglevel = column.translevel & ~0x3ff;
      bglevel = FRACUNIT - fglevel;
      fg2rgb  = Col2RGB[fglevel >> 10];
      bg2rgb  = Col2RGB[bglevel >> 10];
   }

   dest = ylookup[column.y1] + columnofs[column.x];
   fracstep = column.step;
   frac = column.texmid + (int)((column.y1 - view.ycenter + 1) * fracstep);

   {
      register byte *source = (byte *)(column.source);
      register lighttable_t *colormap = column.colormap;
      register int heightmask = column.texheight - 1;
      
      if (column.texheight & heightmask)   // not a power of 2 -- killough
      {
         heightmask++;
         heightmask <<= FRACBITS;
          
         if (frac < 0)
            while ((frac += heightmask) <  0);
         else
            while (frac >= (int)heightmask)
               frac -= heightmask;          

         do
         {
            fg = colormap[source[frac>>FRACBITS]];
            bg = *dest;

            fg = fg2rgb[fg];
            bg = bg2rgb[bg];
            fg = (fg+bg) | 0xf07c3e1f;
            *dest = RGB8k[0][0][(fg>>5) & (fg>>19)];
            
            dest += linesize;          // killough 11/98
            if((frac += fracstep) >= heightmask)
               frac -= heightmask;
         } 
         while(--count);
      }
      else
      {
         while((count -= 2) >= 0)   // texture height is a power of 2 -- killough
         {
            fg = colormap[source[(frac>>FRACBITS) & heightmask]];
            bg = *dest;
            fg = fg2rgb[fg];
            bg = bg2rgb[bg];
            fg = (fg+bg) | 0xf07c3e1f;

            *dest = RGB8k[0][0][(fg>>5) & (fg>>19)];
            dest += linesize;   // killough 11/98
            frac += fracstep;
            
            fg = colormap[source[(frac>>FRACBITS) & heightmask]];
            bg = *dest;
            fg = fg2rgb[fg];
            bg = bg2rgb[bg];
            fg = (fg+bg) | 0xf07c3e1f;

            *dest = RGB8k[0][0][(fg>>5) & (fg>>19)];
            dest += linesize;   // killough 11/98
            frac += fracstep;
         }
         if(count & 1)
         {
            fg = colormap[source[(frac>>FRACBITS) & heightmask]];
            bg = *dest;
            fg = fg2rgb[fg];
            bg = bg2rgb[bg];
            fg = (fg+bg) | 0xf07c3e1f;

            *dest = RGB8k[0][0][(fg>>5) & (fg>>19)];
         }
      }
   }
}

#define SRCPIXEL \
   colormap[column.translation[source[(frac>>FRACBITS) & heightmask]]]

//
// R_DrawFlexTlatedColumn
//
// haleyjd 11/05/02: zdoom-style translucency w/translation, for
// player sprites
//
void CB_DrawFlexTRColumn_8(void)
{
   int count;
   register byte *dest;
   register fixed_t frac;
   fixed_t fracstep;
   unsigned int *fg2rgb, *bg2rgb;
   register unsigned int fg, bg;

   count = column.y2 - column.y1 + 1;
   if(count <= 0) return;

#ifdef RANGECHECK 
   if(column.x  < 0 || column.x  >= video.width || 
      column.y1 < 0 || column.y2 >= video.height)
      I_Error("CB_DrawFlexTRColumn_8: %i to %i at %i", column.y1, column.y2, column.x);    
#endif 

   {
      fixed_t fglevel, bglevel;
      
      fglevel = column.translevel & ~0x3ff;
      bglevel = FRACUNIT - fglevel;
      fg2rgb  = Col2RGB[fglevel >> 10];
      bg2rgb  = Col2RGB[bglevel >> 10];
   }

   dest = ylookup[column.y1] + columnofs[column.x];
   fracstep = column.step;
   frac = column.texmid + (int)((column.y1 - view.ycenter + 1) * fracstep);

   {
      register byte *source = (byte *)(column.source);
      register lighttable_t *colormap = column.colormap;
      register int heightmask = column.texheight - 1;
      
      if(column.texheight & heightmask)
      {
         heightmask++;
         heightmask <<= FRACBITS;
          
         if (frac < 0)
            while ((frac += heightmask) <  0);
         else
            while (frac >= (int)heightmask)
               frac -= heightmask;          

         do
         {
            fg = colormap[column.translation[source[frac>>FRACBITS]]];
            bg = *dest;

            fg = fg2rgb[fg];
            bg = bg2rgb[bg];
            fg = (fg+bg) | 0xf07c3e1f;
            *dest = RGB8k[0][0][(fg>>5) & (fg>>19)];
            
            dest += linesize;          // killough 11/98
            if((frac += fracstep) >= heightmask)
               frac -= heightmask;
         } 
         while(--count);
      }
      else
      {
         while((count -= 2) >= 0) // texture height is a power of 2 -- killough
         {
            fg = SRCPIXEL;
            bg = *dest;
            fg = fg2rgb[fg];
            bg = bg2rgb[bg];
            fg = (fg+bg) | 0xf07c3e1f;

            *dest = RGB8k[0][0][(fg>>5) & (fg>>19)];
            dest += linesize;   // killough 11/98
            frac += fracstep;
            
            fg = SRCPIXEL;
            bg = *dest;
            fg = fg2rgb[fg];
            bg = bg2rgb[bg];
            fg = (fg+bg) | 0xf07c3e1f;

            *dest = RGB8k[0][0][(fg>>5) & (fg>>19)];
            dest += linesize;   // killough 11/98
            frac += fracstep;
         }
         if(count & 1)
         {
            fg = SRCPIXEL;
            bg = *dest;
            fg = fg2rgb[fg];
            bg = bg2rgb[bg];
            fg = (fg+bg) | 0xf07c3e1f;

            *dest = RGB8k[0][0][(fg>>5) & (fg>>19)];
         }
      }
   }
}

#undef SRCPIXEL

#define SRCPIXEL \
   fg2rgb[colormap[source[(frac>>FRACBITS) & heightmask]]] & 0xFFBFDFF

//
// R_DrawAddColumn
//
// haleyjd 02/08/05: additive translucency
//
void CB_DrawAddColumn_8(void)
{
   int count;
   register byte *dest;
   register fixed_t frac;
   fixed_t fracstep;
   unsigned int *fg2rgb, *bg2rgb;
   unsigned int a, b;

   count = column.y2 - column.y1 + 1;
   if(count <= 0) return;

#ifdef RANGECHECK 
   if(column.x  < 0 || column.x  >= video.width || 
      column.y1 < 0 || column.y2 >= video.height)
      I_Error("CB_DrawAddColumn_8: %i to %i at %i", column.y1, column.y2, column.x);    
#endif 

   {
      fixed_t fglevel, bglevel;
      
      fglevel = column.translevel & ~0x3ff;
      bglevel = FRACUNIT;
      fg2rgb  = Col2RGB[fglevel >> 10];
      bg2rgb  = Col2RGB[bglevel >> 10];
   }

   dest = ylookup[column.y1] + columnofs[column.x];
   fracstep = column.step;
   frac = column.texmid + (int)((column.y1 - view.ycenter + 1) * fracstep);

   {
      register byte *source = (byte *)(column.source);
      register lighttable_t *colormap = column.colormap;
      register int heightmask = column.texheight - 1;
      
      if(column.texheight & heightmask)
      {
         heightmask++;
         heightmask <<= FRACBITS;
          
         if (frac < 0)
            while ((frac += heightmask) <  0);
         else
            while (frac >= (int)heightmask)
               frac -= heightmask;          

         do
         {
            // mask out LSBs in green and red to allow overflow
            a = fg2rgb[colormap[source[frac>>FRACBITS]]] & 0xFFBFDFF;
            b = bg2rgb[*dest] & 0xFFBFDFF;
            
            a  = a + b;                      // add with overflow
            b  = a & 0x10040200;             // isolate LSBs
            b  = (b - (b >> 5)) & 0xF83C1E0; // convert to clamped values
            a |= 0xF07C3E1F;                 // apply normal tl mask
            a |= b;                          // mask in clamped values
            
            *dest = RGB8k[0][0][(a >> 5) & (a >> 19)];
            
            dest += linesize;          // killough 11/98
            if((frac += fracstep) >= heightmask)
               frac -= heightmask;
         } 
         while(--count);
      }
      else
      {
         while((count -= 2) >= 0)   // texture height is a power of 2 -- killough
         {
            a = SRCPIXEL;
            b = bg2rgb[*dest] & 0xFFBFDFF;
            
            a  = a + b;                      // add with overflow
            b  = a & 0x10040200;             // isolate LSBs
            b  = (b - (b >> 5)) & 0xF83C1E0; // convert to clamped values
            a |= 0xF07C3E1F;                 // apply normal tl mask
            a |= b;                          // mask in clamped values
            
            *dest = RGB8k[0][0][(a >> 5) & (a >> 19)];
            dest += linesize;   // killough 11/98
            frac += fracstep;

            a = SRCPIXEL;
            b = bg2rgb[*dest] & 0xFFBFDFF;
            
            a  = a + b;                      // add with overflow
            b  = a & 0x10040200;             // isolate LSBs
            b  = (b - (b >> 5)) & 0xF83C1E0; // convert to clamped values
            a |= 0xF07C3E1F;                 // apply normal tl mask
            a |= b;                          // mask in clamped values
            
            *dest = RGB8k[0][0][(a >> 5) & (a >> 19)];
            dest += linesize;   // killough 11/98
            frac += fracstep;            
         }
         if(count & 1)
         {
            a = SRCPIXEL;
            b = bg2rgb[*dest] & 0xFFBFDFF;
            
            a  = a + b;                      // add with overflow
            b  = a & 0x10040200;             // isolate LSBs
            b  = (b - (b >> 5)) & 0xF83C1E0; // convert to clamped values
            a |= 0xF07C3E1F;                 // apply normal tl mask
            a |= b;                          // mask in clamped values
            
            *dest = RGB8k[0][0][(a >> 5) & (a >> 19)];
         }
      }
   }
}

#undef SRCPIXEL

// Draw a column that is both translated and translucent
/*
void R_DrawTlatedLucentColumnP_C (void)
{
	int count;
	byte *dest;
	fixed_t frac;
	fixed_t fracstep;
	unsigned int *fg2rgb, *bg2rgb;

	count = dc_yh - dc_yl;
	if (count < 0)
		return;
	count++;

#ifdef RANGECHECK
	if (dc_x >= screen->width
		|| dc_yl < 0
		|| dc_yh >= screen->height)
	{
		I_Error ( "R_DrawTlatedLucentColumnP_C: %i to %i at %i",
				  dc_yl, dc_yh, dc_x);
	}
	
#endif 

	{
		fixed_t fglevel, bglevel;

		fglevel = dc_translevel & ~0x3ff;
		bglevel = FRACUNIT-fglevel;
		fg2rgb = Col2RGB8[fglevel>>10];
		bg2rgb = Col2RGB8[bglevel>>10];
	}

	dest = ylookup[dc_yl] + columnofs[dc_x];

	fracstep = dc_iscale;
	frac = dc_texturefrac;

	{
		byte *translation = dc_translation;
		byte *colormap = dc_colormap;
		byte *source = dc_source;
		int mask = dc_mask;
		int pitch = dc_pitch;

		do
		{
			unsigned int fg = colormap[translation[source[(frac>>FRACBITS)&mask]]];
			unsigned int bg = *dest;

			fg = fg2rgb[fg];
			bg = bg2rgb[bg];
			fg = (fg+bg) | 0x1f07c1f;
			*dest = RGB32k[0][0][fg & (fg>>15)];
			dest += pitch;
			frac += fracstep;
		} while (--count);
	}
}
*/


#define SRCPIXEL \
   fg2rgb[colormap[column.translation[source[frac>>FRACBITS]]]] & 0xFFBFDFF

#define SRCPIXEL_MASK \
   fg2rgb[colormap[column.translation[source[(frac>>FRACBITS) & heightmask]]]] & 0xFFBFDFF

//
// R_DrawAddTlatedColumn
//
// haleyjd 02/08/05: additive translucency + translation
// The slowest of all column drawers!
//
void CB_DrawAddTRColumn_8(void)
{
   int count;
   register byte *dest;
   register fixed_t frac;
   fixed_t fracstep;
   unsigned int *fg2rgb, *bg2rgb;
   unsigned int a, b;

   count = column.y2 - column.y1 + 1;
   if(count <= 0) return;

#ifdef RANGECHECK 
   if(column.x  < 0 || column.x  >= video.width || 
      column.y1 < 0 || column.y2 >= video.height)
      I_Error("CB_DrawAddTRColumn_8: %i to %i at %i", column.y1, column.y2, column.x);    
#endif 

   {
      fixed_t fglevel, bglevel;
      
      fglevel = column.translevel & ~0x3ff;
      bglevel = FRACUNIT;
      fg2rgb  = Col2RGB[fglevel >> 10];
      bg2rgb  = Col2RGB[bglevel >> 10];
   }

   dest = ylookup[column.y1] + columnofs[column.x];
   fracstep = column.step;
   frac = column.texmid + (int)((column.y1 - view.ycenter + 1) * fracstep);

   {
      register byte *source = (byte *)(column.source);
      register lighttable_t *colormap = column.colormap;
      register int heightmask = column.texheight - 1;
      
      if(column.texheight & heightmask)
      {
         heightmask++;
         heightmask <<= FRACBITS;
          
         if (frac < 0)
            while ((frac += heightmask) <  0);
         else
            while (frac >= (int)heightmask)
               frac -= heightmask;          

         do
         {
            // mask out LSBs in green and red to allow overflow
            a = SRCPIXEL;
            b = bg2rgb[*dest] & 0xFFBFDFF;
            
            a  = a + b;                      // add with overflow
            b  = a & 0x10040200;             // isolate LSBs
            b  = (b - (b >> 5)) & 0xF83C1E0; // convert to clamped values
            a |= 0xF07C3E1F;                 // apply normal tl mask
            a |= b;                          // mask in clamped values
            
            *dest = RGB8k[0][0][(a >> 5) & (a >> 19)];
            
            dest += linesize;          // killough 11/98
            if((frac += fracstep) >= heightmask)
               frac -= heightmask;
         } 
         while(--count);
      }
      else
      {
         while((count -= 2) >= 0) // texture height is a power of 2 -- killough
         {
            a = SRCPIXEL_MASK;
            b = bg2rgb[*dest] & 0xFFBFDFF;
            
            a  = a + b;                      // add with overflow
            b  = a & 0x10040200;             // isolate LSBs
            b  = (b - (b >> 5)) & 0xF83C1E0; // convert to clamped values
            a |= 0xF07C3E1F;                 // apply normal tl mask
            a |= b;                          // mask in clamped values
            
            *dest = RGB8k[0][0][(a >> 5) & (a >> 19)];
            dest += linesize;   // killough 11/98
            frac += fracstep;

            a = SRCPIXEL_MASK;
            b = bg2rgb[*dest] & 0xFFBFDFF;
            
            a  = a + b;                      // add with overflow
            b  = a & 0x10040200;             // isolate LSBs
            b  = (b - (b >> 5)) & 0xF83C1E0; // convert to clamped values
            a |= 0xF07C3E1F;                 // apply normal tl mask
            a |= b;                          // mask in clamped values
            
            *dest = RGB8k[0][0][(a >> 5) & (a >> 19)];
            dest += linesize;   // killough 11/98
            frac += fracstep;            
         }
         if(count & 1)
         {
            a = SRCPIXEL_MASK;
            b = bg2rgb[*dest] & 0xFFBFDFF;
            
            a  = a + b;                      // add with overflow
            b  = a & 0x10040200;             // isolate LSBs
            b  = (b - (b >> 5)) & 0xF83C1E0; // convert to clamped values
            a |= 0xF07C3E1F;                 // apply normal tl mask
            a |= b;                          // mask in clamped values
            
            *dest = RGB8k[0][0][(a >> 5) & (a >> 19)];
         }
      }
   }
}

#undef SRCPIXEL
#undef SRCPIXEL_MASK

//
// Normal Column Drawer Object
// haleyjd 09/04/06
//
columndrawer_t r_normal_drawer =
{
   CB_DrawColumn_8,
   CB_DrawTLColumn_8,
   CB_DrawTRColumn_8,
   CB_DrawTLTRColumn_8,
   CB_DrawFuzzColumn_8,
   CB_DrawFlexColumn_8,
   CB_DrawFlexTRColumn_8,
   CB_DrawAddColumn_8,
   CB_DrawAddTRColumn_8,

   NULL
};


/****************************************************/
/****************************************************/

//
// R_InitTranslationTables
// Creates the translation tables to map
//	the green color ramp to gray, brown, red.
// Assumes a given structure of the PLAYPAL.
// Could be read from a lump instead.
//
//byte *Ranges;

typedef struct
{
  int start;      // start of the sequence of colours
  int number;     // number of colours
} translat_t;

translat_t translations[TRANSLATIONCOLOURS] =
{
    {96,  16},     // indigo
    {64,  16},     // brown
    {32,  16},     // red
  
  /////////////////////////
  // New colours
  
    {176, 16},     // tomato
    {128, 16},     // dirt
    {200, 8},      // blue
    {160, 8},      // gold
    {152, 8},      // felt?
    {0,   1},      // bleeacckk!!
    {250, 5},      // purple
  //  {168, 8}, // bright pink, kinda
    {216, 8},      // vomit yellow
    {16,  16},     // pink
    {56,  8},      // cream
    {88,  8},      // white
};
/*
void R_InitTranslationTables (void)
{
	static const char ranges[11][8] = {
		"CRBRICK",
		"CRTAN",
		"CRGRAY",
		"CRGREEN",
		"CRBROWN",
		"CRGOLD",
		"CRRED",
		"CRBLUE2",
		{ 'C','R','O','R','A','N','G','E' },
		{ 'C','R','Y','E','L','L','O','W' },
		"CRBLUE"
	};
	int i;
	
	static byte *translationtablesmem = 0;
	
	if(translationtablesmem)
		delete[] translationtablesmem;
	
	translationtablesmem = new byte[256*(MAXPLAYERS+3+11)+255]; // denis - fixme - magic numbers?

	// [Toke - fix13]
	// denis - cleaned this up somewhat
	translationtables = (byte *)(((ptrdiff_t)translationtablesmem + 255) & ~255);
	
	// [RH] Each player now gets their own translation table
	//		(soon to be palettes). These are set up during
	//		netgame arbitration and as-needed rather than
	//		in here. We do, however load some text translation
	//		tables from our PWAD (ala BOOM).

	for (i = 0; i < 256; i++)
		translationtables[i] = i;

	for (i = 1; i < MAXPLAYERS+3; i++)
		memcpy (translationtables + i*256, translationtables, 256);

	// create translation tables for dehacked patches that expect them
	for (i = 0x70; i < 0x80; i++) {
		// map green ramp to gray, brown, red
		translationtables[i+(MAXPLAYERS+0)*256] = 0x60 + (i&0xf);
		translationtables[i+(MAXPLAYERS+1)*256] = 0x40 + (i&0xf);
		translationtables[i+(MAXPLAYERS+2)*256] = 0x20 + (i&0xf);
	}

	Ranges = translationtables + (MAXPLAYERS+3)*256;
	for (i = 0; i < 11; i++)
		W_ReadLump (W_GetNumForName (ranges[i]), Ranges + 256 * i);
}
*/
// 
// R_InitTranslationTables
//
// haleyjd 01/12/04: rewritten to support translation lumps
//
void R_InitTranslationTables(void)
{
   int numlumps, i, c;
   
   // don't leak the allocation
   if(translationtables)
   {
      for(i = 0; i < numtranslations; ++i)
         Z_Free(translationtables[i]);

      Z_Free(translationtables);

      // SoM: let's try... this.
      translationtables = NULL;
   }

   // count number of lumps
   firsttranslationlump = W_CheckNumForName("T_START");
   lasttranslationlump  = W_CheckNumForName("T_END");

   if(firsttranslationlump == -1 || lasttranslationlump == -1)
      numlumps = 0;
   else
      numlumps = (lasttranslationlump - firsttranslationlump) - 1;

   // set numtranslations
   numtranslations = TRANSLATIONCOLOURS + numlumps;

   // allocate the array of pointers
   translationtables = (byte **)Z_Malloc(sizeof(byte *) * numtranslations, PU_STATIC, 0);
   
   // build the internal player translations
   for(i = 0; i < TRANSLATIONCOLOURS; ++i)
   {
      byte *transtbl;

      transtbl = translationtables[i] =  (byte *)Z_Malloc(256, PU_STATIC, 0);

      for(c = 0; c < 256; ++c)
      {
         transtbl[c] =
            (c < 0x70 || c > 0x7f) ? c : translations[i].start +
             ((c & 0xf) * (translations[i].number-1))/15;
      }
   }

   // read in the lumps, if any
   for(i = TRANSLATIONCOLOURS; i < numtranslations; ++i)
   {
      int lumpnum = (i - TRANSLATIONCOLOURS) + firsttranslationlump + 1;

      translationtables[i] =  (byte *)W_CacheLumpNum(lumpnum, PU_STATIC);
   }
}

// [RH] Create a player's translation table based on
//		a given mid-range color.
void R_BuildPlayerTranslation (int player, int color)
{
	palette_t *pal = GetDefaultPalette();
	byte **table = &translationtables[player * 256];
	int i;
	float r = (float)RPART(color) / 255.0f;
	float g = (float)GPART(color) / 255.0f;
	float b = (float)BPART(color) / 255.0f;
	float h, s, v;
	float sdelta, vdelta;

	RGBtoHSV (r, g, b, &h, &s, &v);

	s -= 0.23f;
	if (s < 0.0f)
		s = 0.0f;
	sdelta = 0.014375f;

	v += 0.1f;
	if (v > 1.0f)
		v = 1.0f;
	vdelta = -0.05882f;

	for (i = 0x70; i < 0x80; i++) {
		HSVtoRGB (&r, &g, &b, h, s, v);
		table[i] = (byte *)BestColor (pal->basecolors,
							  (int)(r * 255.0f),
							  (int)(g * 255.0f),
							  (int)(b * 255.0f),
							  pal->numcolors);
		s += sdelta;
		if (s > 1.0f) {
			s = 1.0f;
			sdelta = 0.0f;
		}

		v += vdelta;
		if (v < 0.0f) {
			v = 0.0f;
			vdelta = 0.0f;
		}
	}
}


//
// R_InitBuffer 
// Creats lookup tables that avoid
//  multiplies and other hazzles
//  for getting the framebuffer address
//  of a pixel to draw.
//
void
R_InitBuffer
( int		width,
  int		height ) 
{ 
	int 		i;
	byte		*buffer;
	int			pitch;
	int			xshift;

	// Handle resize,
	//	e.g. smaller view windows
	//	with border and/or status bar.
	viewwindowx = (screen->width-(width<<detailxshift))>>1;

	// [RH] Adjust column offset according to bytes per pixel
	//		and detail mode
	xshift = (screen->is8bit) ? 0 : 2;
	xshift += detailxshift;

	// Column offset. For windows
	for (i = 0; i < width; i++)
		columnofs[i] = viewwindowx + (i << xshift);

	// Same with base row offset.
	if ((width<<detailxshift) == screen->width)
		viewwindowy = 0;
	else
		viewwindowy = (ST_Y-(height<<detailyshift)) >> 1;

	screen->Lock ();
	buffer = screen->buffer;
	pitch = screen->pitch;
	screen->Unlock ();

	// Precalculate all row offsets.
	for (i=0 ; i<height ; i++)
		ylookup[i] = buffer + ((i<<detailyshift)+viewwindowy)*pitch;
}


void R_DrawBorder (int x1, int y1, int x2, int y2)
{
	int lump;

	lump = W_CheckNumForName (gameinfo.borderFlat, ns_flats);
	if (lump >= 0)
	{
		screen->FlatFill (x1 & ~63, y1, x2, y2,
			(byte *)W_CacheLumpNum (lump, PU_CACHE));
	}
	else
	{
		screen->Clear (x1, y1, x2, y2, 0);
	}
}


//
// R_DrawViewBorder
// Draws the border around the view
//  for different size windows?
//
void V_MarkRect (int x, int y, int width, int height);

void R_DrawViewBorder (void)
{
	int x, y;
	int offset, size;
	gameborder_t *border;

	if (realviewwidth == screen->width) {
		return;
	}

	border = gameinfo.border;
	offset = border->offset;
	size = border->size;

	R_DrawBorder (0, 0, screen->width, viewwindowy);
	R_DrawBorder (0, viewwindowy, viewwindowx, realviewheight + viewwindowy);
	R_DrawBorder (viewwindowx + realviewwidth, viewwindowy, screen->width, realviewheight + viewwindowy);
	R_DrawBorder (0, viewwindowy + realviewheight, screen->width, ST_Y);

	for (x = viewwindowx; x < viewwindowx + realviewwidth; x += size)
	{
		screen->DrawPatch (W_CachePatch (border->t),
			x, viewwindowy - offset);
		screen->DrawPatch (W_CachePatch (border->b),
			x, viewwindowy + realviewheight);
	}
	for (y = viewwindowy; y < viewwindowy + realviewheight; y += size)
	{
		screen->DrawPatch (W_CachePatch (border->l),
			viewwindowx - offset, y);
		screen->DrawPatch (W_CachePatch (border->r),
			viewwindowx + realviewwidth, y);
	}
	// Draw beveled edge.
	screen->DrawPatch (W_CachePatch (border->tl),
		viewwindowx-offset, viewwindowy-offset);
	
	screen->DrawPatch (W_CachePatch (border->tr),
		viewwindowx+realviewwidth, viewwindowy-offset);
	
	screen->DrawPatch (W_CachePatch (border->bl),
		viewwindowx-offset, viewwindowy+realviewheight);
	
	screen->DrawPatch (W_CachePatch (border->br),
		viewwindowx+realviewwidth, viewwindowy+realviewheight);

	V_MarkRect (0, 0, screen->width, ST_Y);
}

// [RH] Double pixels in the view window horizontally
//		and/or vertically (or not at all).
/*
void R_DetailDouble (void)
{
	switch ((detailxshift << 1) | detailyshift)
	{
		case 1:		// y-double
		{
			int rowsize = realviewwidth << ((screen->is8bit) ? 0 : 2);
			int pitch = screen->pitch;
			int y;
			byte *line;

			line = screen->buffer + viewwindowy*pitch + viewwindowx;
			for (y = 0; y < viewheight; y++, line += pitch<<1)
			{
				memcpy (line+pitch, line, rowsize);
			}
		}
		break;

		case 2:		// x-double
		{
			int rowsize = realviewwidth >> 2;
			int pitch = screen->pitch >> (2-detailyshift);
			int y,x;
			unsigned *line,a,b;

			line = (unsigned *)(screen->buffer + viewwindowy*screen->pitch + viewwindowx);
			for (y = 0; y < viewheight; y++, line += pitch)
			{
				for (x = 0; x < rowsize; x += 2)
				{
					a = line[x+0];
					b = line[x+1];
					a &= 0x00ff00ff;
					b &= 0x00ff00ff;
					line[x+0] = a | (a << 8);
					line[x+1] = b | (b << 8);
				}
			}
		}
		break;

		case 3:		// x- and y-double
		{
			int rowsize = realviewwidth >> 2;
			int pitch = screen->pitch >> (2-detailyshift);
			int realpitch = screen->pitch >> 2;
			int y,x;
			unsigned *line,a,b;

			line = (unsigned *)(screen->buffer + viewwindowy*screen->pitch + viewwindowx);
			for (y = 0; y < viewheight; y++, line += pitch)
			{
				for (x = 0; x < rowsize; x += 2)
				{
					a = line[x+0];
					b = line[x+1];
					a &= 0x00ff00ff;
					b &= 0x00ff00ff;
					line[x+0] = a | (a << 8);
					line[x+0+realpitch] = a | (a << 8);
					line[x+1] = b | (b << 8);
					line[x+1+realpitch] = b | (b << 8);
				}
			}
		}
		break;
	}
}
*/

// [RH] Initialize the column drawer pointers
/*
void R_InitColumnDrawers (BOOL is8bit)
{
	if (is8bit)
	{
#ifdef USEASM
		if (screen->height <= 240)
			R_DrawColumn		= R_DrawColumnP_Unrolled;
		else
			R_DrawColumn		= R_DrawColumnP_ASM;
		R_DrawColumnHoriz		= R_DrawColumnHorizP_ASM;
		R_DrawFuzzColumn		= R_DrawFuzzColumnP_ASM;
		R_DrawTranslucentColumn = R_DrawTranslucentColumnP_C;
		R_DrawTranslatedColumn	= R_DrawTranslatedColumnP_C;
		R_DrawSpan				= R_DrawSpanP_ASM;
		if (CPUFamily <= 5)
			rt_map4cols			= rt_map4cols_asm2;
		else
			rt_map4cols			= rt_map4cols_asm1;
#else
		R_DrawColumnHoriz		= R_DrawColumnHorizP_C;
		R_DrawColumn			= R_DrawColumnP_C;
		R_DrawFuzzColumn		= R_DrawFuzzColumnP_C;
		R_DrawTranslucentColumn = R_DrawTranslucentColumnP_C;
		R_DrawTranslatedColumn	= R_DrawTranslatedColumnP_C;
		R_DrawSpan				= R_DrawSpanP_C;
		rt_map4cols				= rt_map4cols_c;
#endif
	} else {
#ifdef USEASM
		R_DrawColumnHoriz		= R_DrawColumnHorizP_ASM;
		R_DrawColumn			= R_DrawColumnD_C;
		R_DrawFuzzColumn		= R_DrawFuzzColumnD_C;
		R_DrawTranslucentColumn = R_DrawTranslucentColumnD_C;
		R_DrawTranslatedColumn	= R_DrawTranslatedColumnD_C;
#else
		R_DrawColumnHoriz		= R_DrawColumnHorizP_C;
		R_DrawColumn			= R_DrawColumnD_C;
		R_DrawFuzzColumn		= R_DrawFuzzColumnD_C;
		R_DrawTranslucentColumn = R_DrawTranslucentColumnD_C;
		R_DrawTranslatedColumn	= R_DrawTranslatedColumnD_C;
#endif
		R_DrawSpan				= R_DrawSpanD;
	}
}
*/

// haleyjd: experimental column drawer for masked sky textures
void R_DrawNewSkyColumn(void) 
{ 
  int              count; 
  register byte    *dest;            // killough
  register fixed_t frac;            // killough
  fixed_t          fracstep;     

  count = column.y2 - column.y1 + 1; 

  if (count <= 0)    // Zero length, column does not exceed a pixel.
    return; 
                                 
#ifdef RANGECHECK 
  if ((unsigned)column.x >= MAX_SCREENWIDTH
      || column.y1 < 0
      || column.y2 >= MAX_SCREENHEIGHT) 
    I_Error ("R_DrawNewSkyColumn: %i to %i at %i", column.y1, column.y2, column.x); 
#endif 

  // Framebuffer destination address.
  // Use ylookup LUT to avoid multiply with ScreenWidth.
  // Use columnofs LUT for subwindows? 

  dest = ylookup[column.y1] + columnofs[column.x];  

  // Determine scaling, which is the only mapping to be done.

  fracstep = column.step; 
  frac = column.texmid + (int)((column.y1 - view.ycenter + 1) * fracstep);

  // Inner loop that does the actual texture mapping,
  //  e.g. a DDA-lile scaling.
  // This is as fast as it gets.       (Yeah, right!!! -- killough)
  //
  // killough 2/1/98: more performance tuning

  {
    register const byte *source = (byte *)(column.source);            
    register const lighttable_t *colormap = column.colormap; 
    register int heightmask = column.texheight-1;
    if (column.texheight & heightmask)   // not a power of 2 -- killough
      {
        heightmask++;
        heightmask <<= FRACBITS;
          
        if (frac < 0)
          while ((frac += heightmask) <  0);
        else
          while (frac >= heightmask)
            frac -= heightmask;
          
        do
          {
            // Re-map color indices from wall texture column
            //  using a lighting/special effects LUT.
            
            // heightmask is the Tutti-Frutti fix -- killough

            // haleyjd
            if(source[frac>>FRACBITS])
              *dest = colormap[source[frac>>FRACBITS]];
            dest += linesize;                     // killough 11/98
            if ((frac += fracstep) >= heightmask)
              frac -= heightmask;
          } 
        while (--count);
      }
    else
      {
        while ((count-=2)>=0)   // texture height is a power of 2 -- killough
          {
            if(source[(frac>>FRACBITS) & heightmask])
              *dest = colormap[source[(frac>>FRACBITS) & heightmask]];
            dest += linesize;   // killough 11/98
            frac += fracstep;
            if(source[(frac>>FRACBITS) & heightmask])
              *dest = colormap[source[(frac>>FRACBITS) & heightmask]];
            dest += linesize;   // killough 11/98
            frac += fracstep;
          }
        if ((count & 1) && source[(frac>>FRACBITS) & heightmask])
          *dest = colormap[source[(frac>>FRACBITS) & heightmask]];
      }
  }
} 

VERSION_CONTROL (r_draw_cpp, "$Id: r_draw.cpp 165 2007-03-08 18:43:19Z denis $")

