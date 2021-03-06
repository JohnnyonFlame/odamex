// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id: v_font.h $
//
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
//
// Bitmapped font routines
//
//-----------------------------------------------------------------------------

#ifndef __V_FONT_H__
#define __V_FONT_H__

#include "i_system.h"
#include "v_video.h"
#include "r_texture.h"
#include "z_zone.h"

class OFont
{
public:
	virtual ~OFont() { }

	virtual int getHeight() const = 0;
	int getTextWidth(char c) const;
	int getTextWidth(const char* str) const;
	int getTextHeight(char c) const;
	int getTextHeight(const char* str) const;
	void printText(const DCanvas* canvas, int x, int y, int color, const char* str) const;

protected:
	texhandle_t		mCharacterHandles[256];
	const Texture*	mCharacters[256];

private:
	void printCharacter(const DCanvas* canvas, int& x, int& y, char c) const;
};


class ConCharsFont : public OFont
{
public:
	ConCharsFont(fixed_t scale);
	virtual ~ConCharsFont();

	virtual int getHeight() const
	{
		return mHeight;
	}

private:
	static const int charwidth = 8;
	static const int charheight = 8;

	int				mHeight;
}; 

class SmallDoomFont : public OFont
{
public:
	SmallDoomFont(fixed_t scale);
	virtual ~SmallDoomFont();

	virtual int getHeight() const
	{
		return mHeight;
	}

private:
	int				mHeight;
};

class LargeDoomFont : public OFont
{
public:
	LargeDoomFont(fixed_t scale);
	virtual ~LargeDoomFont();

	virtual int getHeight() const
	{
		return mHeight;
	}

private:
	int				mHeight;
};

class TrueTypeFont : public OFont
{
public:
	enum
	{
		TTF_GRADIENT		= 0x01,
		TTF_TEXTURE			= 0x02,
		TTF_OUTLINE			= 0x04,
		TTF_SHADOW			= 0x08
	};

	TrueTypeFont(const char* filename, int size, unsigned int stylemask);
	virtual ~TrueTypeFont();

	virtual int getHeight() const
	{
		return mHeight;
	}

private:
	int				mHeight;
};

#endif	// __V_FONT_H__

