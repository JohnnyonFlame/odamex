// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// $Id$
//
// Copyright (C) 2012 by Alex Mayfield.
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
//   HUD drawing functions.
//
//-----------------------------------------------------------------------------

#include "hu_drawers.h"
#include "v_video.h"
#include "v_text.h"

namespace hud {

// Calculate the starting x and y coordinate and proper scaling factors for
// each of the HUD drawers.
void calculateOrigin(int& x, int& y,
                     const unsigned short w, const unsigned short h,
                     const float scale, int& x_scale, int& y_scale,
                     const x_align_t x_align, const y_align_t y_align,
                     const x_align_t x_origin, const y_align_t y_origin) {
	if (x_origin == X_ABSOLUTE || y_origin == Y_ABSOLUTE) {
		// No such thing as "absolute origin".
		return;
	}

	// Since Doom's assets are so low-resolution, scaling is done by simple
	// doubling/tripling/etc. of the pixels with no alising.
	x_scale = scale * CleanXfac;
	if (x_scale < 1) {
		x_scale = 1;
	}

	y_scale = scale * CleanYfac;
	if (y_scale < 1) {
		y_scale = 1;
	}

	// "Alignment" is the side of the screen that the passed x and y values
	// are relative to.  Note that for X_RIGHT and Y_BOTTOM, the coordinate
	// system is flippxed.
	switch (x_align) {
	case X_LEFT:
		x = x * x_scale;
		break;
	case X_CENTER:
		x = (screen->width >> 1) + (x * x_scale);
		break;
	case X_RIGHT:
		x = screen->width - (x * x_scale);
		break;
	case X_ABSOLUTE:
		x = (x * screen->width) / (320 * x_scale);
		break;
	}

	switch (y_align) {
	case Y_TOP:
		y = y * y_scale;
		break;
	case Y_MIDDLE:
		y = (screen->height >> 1) + (y * y_scale);
		break;
	case Y_BOTTOM:
		y = screen->height - (y * y_scale);
		break;
	case Y_ABSOLUTE:
		y = (y * screen->height) / (200 * y_scale);
		break;
	}

	// "Origin" is the corner of the texture/whatever that the drawing function
	// should appear to begin drawing from.  All DCanvas texture drawers begin
	// at the top left, so for other cases we need to offset our x and y.
	switch (x_origin) {
	case X_CENTER:
		x = x - ((w * x_scale) >> 1);
		break;
	case X_RIGHT:
		x = x - (w * x_scale);
		break;
	default:
		break;
	}

	switch (y_origin) {
	case Y_MIDDLE:
		y = y - ((h * y_scale) >> 1);
		break;
	case Y_BOTTOM:
		y = y - (h * y_scale);
		break;
	default:
		break;
	}
}

// Round float to short integer.  Used by the scaling function.
short roundToShort(float f) {
	if (f >= 0.0f) {
		return (short)(f + 0.5f);
	} else {
		return (short)(f - 0.5f);
	}
}

// Return the number of scaled available horizontal pixels to draw on.
int XSize(const float scale) {
	int x_scale = scale * CleanXfac;
	if (x_scale < 1) {
		x_scale = 1;
	}
	return screen->width / x_scale;
}

// Return the number of scaled available horizontal pixels to draw on.
int YSize(const float scale) {
	int y_scale = scale * CleanYfac;
	if (y_scale < 1) {
		y_scale = 1;
	}
	return screen->height / y_scale;
}

// Fill an area with a solid color.
void Clear(int x, int y,
           const unsigned short w, const unsigned short h,
           const float scale,
           const x_align_t x_align, const y_align_t y_align,
           const x_align_t x_origin, const y_align_t y_origin,
           const int color) {
	// Turn our scaled coordinates into real coordinates.
	int x_scale, y_scale;
	calculateOrigin(x, y, w, h, scale, x_scale, y_scale,
	                x_align, y_align, x_origin, y_origin);
	screen->Clear(x, y, x + (w * x_scale), y + (h * y_scale), color);
}

// Fill an area with a dimmed box.
void Dim(int x, int y,
         const unsigned short w, const unsigned short h,
         const float scale,
         const x_align_t x_align, const y_align_t y_align,
         const x_align_t x_origin, const y_align_t y_origin) {
	// Turn our scaled coordinates into real coordinates.
	int x_scale, y_scale;
	calculateOrigin(x, y, w, h, scale, x_scale, y_scale,
	                x_align, y_align, x_origin, y_origin);
	screen->Dim(x, y, w * x_scale, h * y_scale);
}

// Draw HUD text using hud_font.
void DrawText(int x, int y, const float scale,
              const x_align_t x_align, const y_align_t y_align,
              const x_align_t x_origin, const y_align_t y_origin,
              const char* str, const int color,
              const bool force_opaque) {
	// No string?  Don't bother with this function.
	if (!str) {
		return;
	}

	// Calculate width and height of string
	unsigned short w = hud_font->getTextWidth(str);
	unsigned short h = hud_font->getHeight();

	// Turn our scaled coordinates into real coordinates.
	int x_scale, y_scale;
	calculateOrigin(x, y, w, h, scale, x_scale, y_scale,
	                x_align, y_align, x_origin, y_origin);

	if (force_opaque) {
		screen->DrawTextStretched(color, x, y, str, x_scale, y_scale);
	} else {
		screen->DrawTextStretchedLuc(color, x, y, str, x_scale, y_scale);
	}
}

void DrawTexture(	int x, int y, float scale,
					x_align_t x_align, y_align_t y_align,
					x_align_t x_origin, y_align_t y_origin,
					const Texture* texture,
					bool force_opaque,
					bool use_texture_offsets)
{
	unsigned short w = texture->getWidth();
	unsigned short h = texture->getHeight();

	// Turn our scaled coordinates into real coordinates.
	int x_scale, y_scale;
	calculateOrigin(x, y, w, h, scale, x_scale, y_scale,
	                x_align, y_align, x_origin, y_origin);

	if (!use_texture_offsets)
	{
		// Negate scaled texture offsets.
		x += texture->getOffsetX() * x_scale;
		y += texture->getOffsetY() * y_scale;
	}

	if (force_opaque)
		screen->DrawTextureStretched(texture, x, y, w * x_scale, h * y_scale);
	else
		screen->DrawLucentTextureStretched(texture, x, y, w * x_scale, h * y_scale);
}


void DrawTranslatedTexture(	int x, int y, float scale,
							x_align_t x_align, y_align_t y_align,
							x_align_t x_origin, y_align_t y_origin,
							const Texture* texture, const byte* translation,
							bool force_opaque,
							bool use_texture_offsets)
{
	unsigned short w = texture->getWidth();
	unsigned short h = texture->getHeight();

	// Turn our scaled coordinates into real coordinates.
	int x_scale, y_scale;
	calculateOrigin(x, y, w, h, scale, x_scale, y_scale,
	                x_align, y_align, x_origin, y_origin);

	if (!use_texture_offsets)
	{
		// Negate scaled texture offsets.
		x += texture->getOffsetX() * x_scale;
		y += texture->getOffsetY() * y_scale;
	}

	V_ColorMap = translation;

	if (force_opaque)
		screen->DrawTranslatedTextureStretched(texture, x, y, w * x_scale, h * y_scale);
	else 
		screen->DrawTranslatedLucentTextureStretched(texture, x, y, w * x_scale, h * y_scale);
}

void DrawTextureStretched(	int x, int y,
							short w, short h,
							float scale,
							x_align_t x_align, y_align_t y_align,
							x_align_t x_origin, y_align_t y_origin,
							const Texture* texture,
							bool force_opaque,
							bool use_texture_offsets)
{
	// Turn our scaled coordinates into real coordinates.
	int x_scale, y_scale;
	calculateOrigin(x, y, w, h, scale, x_scale, y_scale,
	                x_align, y_align, x_origin, y_origin);

	if (!use_texture_offsets)
	{
		// Negate scaled texture offsets.
		x += (roundToShort(texture->getOffsetX() * ((float)w / texture->getWidth()))) * x_scale;
		y += (roundToShort(texture->getOffsetY() * ((float)h / texture->getHeight()))) * y_scale;
	}

	if (force_opaque)
		screen->DrawTextureStretched(texture, x, y, w * x_scale, h * y_scale);
	else
		screen->DrawLucentTextureStretched(texture, x, y, w * x_scale, h * y_scale);
}

void DrawTextureScaled(	int x, int y,
						short w, short h,
						float scale,
						x_align_t x_align, y_align_t y_align,
						x_align_t x_origin, y_align_t y_origin,
						const Texture* texture,
						bool force_opaque,
						bool use_texture_offsets)
{
	// Calculate aspect ratios of texture and destination.
	float texture_aspect = (float)texture->getWidth() / texture->getHeight();
	float dest_aspect = (float)w / h;

	if (texture_aspect < dest_aspect)
	{
		// Destination is wider than texture.  Keep height, calculate width.
		w = (texture->getWidth() * h) / texture->getHeight();
	}
	else if (texture_aspect > dest_aspect)
	{
		// Destination is taller than texture.  Keep width, calculate height.
		h = (texture->getHeight() * w) / texture->getWidth();
	}

	// Call the 'stretched' drawer with our new dest. width and height.
	DrawTextureStretched(x, y, w, h, scale, x_align, y_align, x_origin, y_origin,
			texture, force_opaque, use_texture_offsets);
}

}
