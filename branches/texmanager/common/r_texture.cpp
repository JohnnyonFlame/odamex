// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// $Id: r_texture.cpp 3945 2013-07-03 14:32:48Z dr_sean $
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
//	Texture manager to load and convert textures.	
//
//-----------------------------------------------------------------------------

#include "i_system.h"
#include "tables.h"
#include "r_defs.h"
#include "r_state.h"
#include "g_level.h"
#include "m_random.h"
#include "w_wad.h"
#include "sc_man.h"
#include "cmdlib.h"

#include <cstring>
#include <algorithm>
#include <math.h>

#include "r_texture.h"

TextureManager texturemanager;

void R_InitTextureManager()
{
	texturemanager.startup();
}

void R_ShutdownTextureManager()
{
	texturemanager.shutdown();
}

//
// R_DrawPatchIntoTexture
//
// Draws a lump in patch_t format into a Texture at the given offset.
//
static void R_DrawPatchIntoTexture(Texture* texture, const byte* lumpdata, int xoffs, int yoffs)
{
	int texwidth = texture->getWidth();
	int texheight = texture->getHeight();
	int patchwidth = LESHORT(*(short*)(lumpdata + 0));

	const int* colofs = (int*)(lumpdata + 8);

	int x1 = MAX(xoffs, 0);
	int x2 = MIN(xoffs + patchwidth - 1, texwidth - 1);

	for (int x = x1; x <= x2; x++)
	{
		int abstopdelta = 0;

		const byte* post = lumpdata + LELONG(colofs[x - xoffs]);
		while (*post != 0xFF)
		{
			int posttopdelta = *(post + 0);
			int postlength = *(post + 1);

			// handle DeePsea tall patches where topdelta is treated as a relative
			// offset instead of an absolute offset
			if (posttopdelta <= abstopdelta)
				abstopdelta += posttopdelta;
			else
				abstopdelta = posttopdelta;

			int topoffset = yoffs + abstopdelta;
			int y1 = MAX(topoffset, 0);
			int y2 = MIN(topoffset + postlength - 1, texheight - 1);

			if (y1 <= y2)
			{
				byte* dest = texture->getData() + texheight * x + y1;
				const byte* source = post + 3;
				memcpy(dest, source, y2 - y1 + 1);

				// set up the mask
				byte* mask = texture->getMaskData() + texheight * x + y1;
				memset(mask, 1, y2 - y1 + 1);	
			}
			
			post += postlength + 4;
		}
	}
}


//
// R_CopySubimage
//
// Copies a portion of source_texture and draws it into dest_texture.
// The source subimage is scaled to fit the dimensions of the destination
// texture.
//
// Note: no clipping is performed so it is possible to read past the
// end of the source texture.
//
void R_CopySubimage(Texture* dest_texture, const Texture* source_texture,
	int dx1, int dy1, int dx2, int dy2,
	int sx1, int sy1, int sx2, int sy2)
{
	const int destwidth = dx2 - dx1 + 1; 
	const int destheight = dy2 - dy1 + 1;

	const int sourcewidth = sx2 - sx1 + 1;
	const int sourceheight = sy2 - sy1 + 1;

	const fixed_t xstep = FixedDiv(sourcewidth << FRACBITS, destwidth << FRACBITS) + 1;
	const fixed_t ystep = FixedDiv(sourceheight << FRACBITS, destheight << FRACBITS) + 1;

	int dest_offset = dx1 * dest_texture->getHeight() + dy1;
	byte* dest = dest_texture->getData() + dest_offset; 
	byte* dest_mask = dest_texture->getMaskData() + dest_offset; 

	fixed_t xfrac = 0;
	for (int xcount = destwidth; xcount > 0; xcount--)
	{
		int source_offset = (sx1 + (xfrac >> FRACBITS)) * source_texture->getHeight() + sy1;
		const byte* source = source_texture->getData() + source_offset;
		const byte* source_mask = source_texture->getMaskData() + source_offset;

		fixed_t yfrac = 0;
		for (int ycount = destheight; ycount > 0; ycount--)
		{
			*dest++ = source[yfrac >> FRACBITS];	
			*dest_mask++ = source_mask[yfrac >> FRACBITS];	
			yfrac += ystep;
		}

		dest += dest_texture->getHeight() - destheight;
		dest_mask += dest_texture->getHeight() - destheight; 

		xfrac += xstep;
	}

	// copy the source texture's offset info
	int xoffs = FixedDiv(source_texture->getOffsetX() << FRACBITS, xstep) >> FRACBITS;
	int yoffs = FixedDiv(source_texture->getOffsetY() << FRACBITS, ystep) >> FRACBITS;
	dest_texture->setOffsetX(xoffs);
	dest_texture->setOffsetY(yoffs);
}

const Texture* R_LoadTexture(const char* name)
{
	texhandle_t texhandle = texturemanager.getHandle(name, Texture::TEX_PATCH);
	return texturemanager.getTexture(texhandle);
}

//
// R_WarpTexture
//
// Alters the image in source_texture with a warping effect and saves the
// result in dest_texture.
//
static void R_WarpTexture(Texture* dest_texture, const Texture* source_texture)
{
	const byte* source_buffer = source_texture->getData();
	byte* warped_buffer = dest_texture->getData();

	int widthbits = source_texture->getWidthBits();
	int width = (1 << widthbits);
	int widthmask = width - 1; 
	int heightbits = source_texture->getHeightBits();
	int height = (1 << heightbits);
	int heightmask = height - 1;

	byte temp_buffer[Texture::MAX_TEXTURE_HEIGHT];

	int step = level.time * 32;
	for (int y = height - 1; y >= 0; y--)
	{
		const byte* source = source_buffer + y;
		byte* dest = warped_buffer + y;

		int xf = (finesine[(step + y * 128) & FINEMASK] >> 13) & widthmask;
		for (int x = 0; x < width; x++)
		{
			*dest = source[xf << heightbits];
			dest += height;
			xf = (xf + 1) & widthmask;
		}
	}

	step = level.time * 23;
	for (int x = width - 1; x >= 0; x--)
	{
		const byte *source = warped_buffer + (x << heightbits);
		byte *dest = temp_buffer;

		int yf = (finesine[(step + 128 * (x + 17)) & FINEMASK] >> 13) & heightmask;
		for (int y = 0; y < height; y++)
		{
			*dest++ = source[yf];
			yf = (yf + 1) & heightmask;
		}
		memcpy(warped_buffer + (x << heightbits), temp_buffer, height);
	}
}

// ============================================================================
//
// Texture
//
// ============================================================================

Texture::Texture() :
	mHandle(TextureManager::NO_TEXTURE_HANDLE)
{
	init(0, 0);
}

size_t Texture::calculateSize(int width, int height)
{
	return sizeof(Texture)						// header
		+ width * height						// mData
		+ width * height;						// mMask
}

//
// Texture::init
//
// Sets up the member variable of Texture based on a supplied width and height.
//
void Texture::init(int width, int height)
{
	mWidth = width;
	mHeight = height;
	mWidthBits = Log2(width);
	mHeightBits = Log2(height);
	mOffsetX = 0;
	mOffsetY = 0;
	mScaleX = FRACUNIT;
	mScaleY = FRACUNIT;
	mHasMask = false;

	if (clientside)
	{
		// mData follows the header in memory
		mData = (byte*)((byte*)this + sizeof(*this));
		// mMask follows mData
		mMask = (byte*)(mData) + sizeof(byte) * width * height;
	}
	else
	{
		mData = NULL;
		mMask = NULL;
	}
}



// ============================================================================
//
// TextureManager
//
// ============================================================================

// define GARBAGE_TEXTURE_HANDLE to be the first wall texture (AASTINKY)
const texhandle_t TextureManager::GARBAGE_TEXTURE_HANDLE = TextureManager::WALLTEXTURE_HANDLE_MASK;

TextureManager::TextureManager() :
	mHandleMap(2048),
	mPNameLookup(NULL),
	mTextureNameTranslationMap(512),
	mFreeCustomHandlesHead(0),
	mFreeCustomHandlesTail(TextureManager::MAX_CUSTOM_HANDLES)
{
}

TextureManager::~TextureManager()
{
}


//
// TextureManager::clear
//
// Frees all memory used by TextureManager, freeing all Texture objects
// and the supporting lookup structures.
//
void TextureManager::clear()
{
	// free normal textures
	for (HandleMap::iterator it = mHandleMap.begin(); it != mHandleMap.end(); ++it)
		if (it->second)
			Z_Free((void*)it->second);

	mHandleMap.clear();

	// free all custom texture handles
	mFreeCustomHandlesHead = 0;
	mFreeCustomHandlesTail = TextureManager::MAX_CUSTOM_HANDLES - 1;
	for (unsigned int i = mFreeCustomHandlesHead; i <= mFreeCustomHandlesTail; i++)
		mFreeCustomHandles[i] = CUSTOM_HANDLE_MASK | i;

	delete [] mPNameLookup;
	mPNameLookup = NULL;

	for (size_t i = 0; i < mTextureDefinitions.size(); i++)
		delete [] (byte*)mTextureDefinitions[i];
	mTextureDefinitions.clear();

	mTextureNameTranslationMap.clear();

	mAnimDefs.clear();

	// free warping original texture (not stored in mHandleMap)
	for (size_t i = 0; i < mWarpDefs.size(); i++)
		if (mWarpDefs[i].original_texture)
			Z_Free((void*)mWarpDefs[i].original_texture);

	mWarpDefs.clear();
}


//
// TextureManager::startup
//
// Frees any existing textures and sets up the lookup structures for flats
// and wall textures. This should be called at the start of each map, prior
// to P_SetupLevel.
//
void TextureManager::startup()
{
	clear();

	// initialize the FLATS data
	mFirstFlatLumpNum = W_GetNumForName("F_START") + 1;
	mLastFlatLumpNum = W_GetNumForName("F_END") - 1;
	
	// initialize the PNAMES mapping to map an index in PNAMES to a WAD lump number
	readPNamesDirectory();

	// initialize the TEXTURE1 & TEXTURE2 data
	addTextureDirectory("TEXTURE1");
	addTextureDirectory("TEXTURE2");

	generateNotFoundTexture();

	if (clientside)
	{	
		readAnimDefLump();
		readAnimatedLump();
	}
}


//
// TextureManager::shutdown
//
void TextureManager::shutdown()
{
	clear();
}

//
// TextureManager::precache
//
// Loads all of a level's textures into Zone memory.
// Requires that init() and P_SetupLevel be called first.
//
void TextureManager::precache()
{
	// precache all the wall textures
	for (int i = 0; i < numsides; i++)
	{
		if (sides[i].toptexture)
			getTexture(sides[i].toptexture);
		if (sides[i].midtexture)
			getTexture(sides[i].midtexture);
		if (sides[i].bottomtexture)
			getTexture(sides[i].bottomtexture);
	}

	// precache all the floor/ceiling textures
	for (int i = 0; i < numsectors; i++)
	{
		getTexture(sectors[i].ceiling_texhandle);
		getTexture(sectors[i].floor_texhandle);
	}
}


//
// TextureManager::readPNamesDirectory
//
void TextureManager::readPNamesDirectory()
{
	int lumpnum = W_GetNumForName("PNAMES");
	size_t lumplen = W_LumpLength(lumpnum);

	byte* lumpdata = new byte[lumplen];
	W_ReadLump(lumpnum, lumpdata);

	int num_pname_mappings = LELONG(*((int*)(lumpdata + 0)));
	mPNameLookup = new int[num_pname_mappings];

	for (int i = 0; i < num_pname_mappings; i++)
	{
		const char* lumpname = (const char*)(lumpdata + 4 + 8 * i);
		mPNameLookup[i] = W_CheckNumForName(lumpname);

		// killough 4/17/98:
		// Some wads use sprites as wall patches, so repeat check and
		// look for sprites this time, but only if there were no wall
		// patches found. This is the same as allowing for both, except
		// that wall patches always win over sprites, even when they
		// appear first in a wad. This is a kludgy solution to the wad
		// lump namespace problem.

		if (mPNameLookup[i] == -1)
			mPNameLookup[i] = W_CheckNumForName(lumpname, ns_sprites);
	}

	delete [] lumpdata;
}


//
// TextureManager::readAnimDefLump
//
// [RH] This uses a Hexen ANIMDEFS lump to define the animation sequences.
//
void TextureManager::readAnimDefLump()
{
	int lump = -1;
	
	while ((lump = W_FindLump("ANIMDEFS", lump)) != -1)
	{
		SC_OpenLumpNum(lump, "ANIMDEFS");

		while (SC_GetString())
		{
			if (SC_Compare("flat") || SC_Compare("texture"))
			{
				anim_t anim;

				Texture::TextureSourceType texture_type = Texture::TEX_WALLTEXTURE;
				if (SC_Compare("flat"))
					texture_type = Texture::TEX_FLAT;

				SC_MustGetString();
				anim.basepic = texturemanager.getHandle(sc_String, texture_type);

				anim.curframe = 0;
				anim.numframes = 0;
				memset(anim.speedmin, 1, anim_t::MAX_ANIM_FRAMES * sizeof(*anim.speedmin));
				memset(anim.speedmax, 1, anim_t::MAX_ANIM_FRAMES * sizeof(*anim.speedmax));

				while (SC_GetString())
				{
					if (!SC_Compare("pic"))
					{
						SC_UnGet();
						break;
					}

					if ((unsigned)anim.numframes == anim_t::MAX_ANIM_FRAMES)
						SC_ScriptError ("Animation has too many frames");

					byte min = 1, max = 1;
					
					SC_MustGetNumber();
					int frame = sc_Number;
					SC_MustGetString();
					if (SC_Compare("tics"))
					{
						SC_MustGetNumber();
						sc_Number = clamp(sc_Number, 0, 255);
						min = max = sc_Number;
					}
					else if (SC_Compare("rand"))
					{
						SC_MustGetNumber();
						min = MAX(sc_Number, 0);
						SC_MustGetNumber();
						max = MIN(sc_Number, 255);
						if (min > max)
							min = max = 1;
					}
					else
					{
						SC_ScriptError ("Must specify a duration for animation frame");
					}

					anim.speedmin[anim.numframes] = min;
					anim.speedmax[anim.numframes] = max;
					anim.framepic[anim.numframes] = frame + anim.basepic - 1;
					anim.numframes++;
				}

				anim.countdown = anim.speedmin[0];

				if (anim.basepic != TextureManager::NOT_FOUND_TEXTURE_HANDLE &&
					anim.basepic != TextureManager::NO_TEXTURE_HANDLE)
					mAnimDefs.push_back(anim);
			}
			else if (SC_Compare ("switch"))   // Don't support switchdef yet...
			{
				//P_ProcessSwitchDef ();
				SC_ScriptError("switchdef not supported.");
			}
			else if (SC_Compare("warp"))
			{
				SC_MustGetString();
				if (SC_Compare("flat") || SC_Compare("texture"))
				{

					Texture::TextureSourceType texture_type = Texture::TEX_WALLTEXTURE;
					if (SC_Compare("flat"))
						texture_type = Texture::TEX_FLAT;

					SC_MustGetString();

					texhandle_t texhandle = texturemanager.getHandle(sc_String, texture_type);
					if (texhandle == TextureManager::NOT_FOUND_TEXTURE_HANDLE ||
						texhandle == TextureManager::NO_TEXTURE_HANDLE)
						continue;

					warp_t warp;

					// backup the original texture
					warp.original_texture = getTexture(texhandle);

					int width = 1 << warp.original_texture->getWidthBits();
					int height = 1 << warp.original_texture->getHeightBits();

					// create a new texture of the same size for the warped image
					warp.warped_texture = createTexture(texhandle, width, height);

					mWarpDefs.push_back(warp);
				}
				else
				{
					SC_ScriptError(NULL, NULL);
				}
			}
		}
		SC_Close ();
	}
}

//
// TextureManager::readAnimatedLump
//
// Reads animation definitions from the ANIMATED lump.
//
// Load the table of animation definitions, checking for existence of
// the start and end of each frame. If the start doesn't exist the sequence
// is skipped, if the last doesn't exist, BOOM exits.
//
// Wall/Flat animation sequences, defined by name of first and last frame,
// The full animation sequence is given using all lumps between the start
// and end entry, in the order found in the WAD file.
//
// This routine modified to read its data from a predefined lump or
// PWAD lump called ANIMATED rather than a static table in this module to
// allow wad designers to insert or modify animation sequences.
//
// Lump format is an array of byte packed animdef_t structures, terminated
// by a structure with istexture == -1. The lump can be generated from a
// text source file using SWANTBLS.EXE, distributed with the BOOM utils.
// The standard list of switches and animations is contained in the example
// source text file DEFSWANI.DAT also in the BOOM util distribution.
//
// [RH] Rewritten to support BOOM ANIMATED lump but also make absolutely
// no assumptions about how the compiler packs the animdefs array.
//
void TextureManager::readAnimatedLump()
{
	int lumpnum = W_CheckNumForName("ANIMATED");
	if (lumpnum == -1)
		return;

	size_t lumplen = W_LumpLength(lumpnum);
	if (lumplen == 0)
		return;

	byte* lumpdata = new byte[lumplen];
	W_ReadLump(lumpnum, lumpdata);

	for (byte* ptr = lumpdata; *ptr != 255; ptr += 23)
	{
		anim_t anim;

		Texture::TextureSourceType texture_type = *(ptr + 0) == 1 ?
					Texture::TEX_WALLTEXTURE : Texture::TEX_FLAT;

		const char* startname = (const char*)(ptr + 10);
		const char* endname = (const char*)(ptr + 1);

		texhandle_t start_texhandle =
				texturemanager.getHandle(startname, texture_type);
		texhandle_t end_texhandle =
				texturemanager.getHandle(endname, texture_type);

		if (start_texhandle == TextureManager::NOT_FOUND_TEXTURE_HANDLE ||
			start_texhandle == TextureManager::NO_TEXTURE_HANDLE ||
			end_texhandle == TextureManager::NOT_FOUND_TEXTURE_HANDLE ||
			end_texhandle == TextureManager::NO_TEXTURE_HANDLE)
			continue;

		anim.basepic = start_texhandle;
		anim.numframes = end_texhandle - start_texhandle + 1;

		if (anim.numframes <= 0)
			continue;
		anim.curframe = 0;
			
		int speed = LELONG(*(int*)(ptr + 19));
		anim.countdown = speed - 1;

		for (int i = 0; i < anim.numframes; i++)
		{
			anim.framepic[i] = anim.basepic + i;
			anim.speedmin[i] = anim.speedmax[i] = speed;
		}

		mAnimDefs.push_back(anim);
	}

	delete [] lumpdata;
}


//
// TextureManager::updateAnimatedTextures
//
// Handles ticking the animated textures and cyles the Textures within an
// animation definition.
//
void TextureManager::updateAnimatedTextures()
{
	if (!clientside)
		return;

	// cycle the animdef textures
	for (size_t i = 0; i < mAnimDefs.size(); i++)
	{
		anim_t* anim = &mAnimDefs[i];
		if (--anim->countdown == 0)
		{
			anim->curframe = (anim->curframe + 1) % anim->numframes;

			if (anim->speedmin[anim->curframe] == anim->speedmax[anim->curframe])
				anim->countdown = anim->speedmin[anim->curframe];
			else
				anim->countdown = M_Random() %
					(anim->speedmax[anim->curframe] - anim->speedmin[anim->curframe]) +
					anim->speedmin[anim->curframe];

			// cycle the Textures
			getTexture(anim->framepic[0]);	// ensure Texture is still cached
			Texture* first_texture = mHandleMap[anim->framepic[0]];

			for (int frame1 = 0; frame1 < anim->numframes - 1; frame1++)
			{
				int frame2 = (frame1 + 1) % anim->numframes;
				getTexture(anim->framepic[frame2]);	// ensure Texture is still cached
				mHandleMap[anim->framepic[frame1]] = mHandleMap[anim->framepic[frame2]]; 
			}
			
			mHandleMap[anim->framepic[anim->numframes - 1]] = first_texture;
		}
	}

	// warp textures
	for (size_t i = 0; i < mWarpDefs.size(); i++)
	{
		const Texture* original_texture = mWarpDefs[i].original_texture;
		Texture* warped_texture = mWarpDefs[i].warped_texture;
		R_WarpTexture(warped_texture, original_texture);
	}
}

//
// TextureManager::generateNotFoundTexture
//
// Generates a checkerboard texture with 32x32 squares. This texture will be
// used whenever a texture is requested but not found in the WAD file.
//
void TextureManager::generateNotFoundTexture()
{
	const int width = 64, height = 64;

	const texhandle_t handle = NOT_FOUND_TEXTURE_HANDLE;
	Texture* texture = createTexture(handle, width, height);

	if (clientside)
	{
		const byte color1 = 0x40, color2 = 0x80;

		for (int y = 0; y < height / 2; y++)
		{
			memset(texture->mData + y * width + 0, color1, width / 2);
			memset(texture->mData + y * width + width / 2, color2, width / 2);
		}
		for (int y = height / 2; y < height; y++)
		{
			memset(texture->mData + y * width + 0, color2, width / 2);
			memset(texture->mData + y * width + width / 2, color1, width / 2);
		}
	}
}


//
// TextureManager::addTextureDirectory
//
// Requires that the PNAMES lump has been read and processed.
//
void TextureManager::addTextureDirectory(const char* lumpname)
{
	//
	// Texture definition.
	// Each texture is composed of one or more patches,
	// with patches being lumps stored in the WAD.
	// The lumps are referenced by number, and patched
	// into the rectangular texture space using origin
	// and possibly other attributes.
	//
	struct mappatch_t
	{
		short	originx;
		short	originy;
		short	patch;
		short	stepdir;
		short	colormap;
	};

	//
	// Texture definition.
	// A DOOM wall texture is a list of patches
	// which are to be combined in a predefined order.
	//
	struct maptexture_t
	{
		char		name[8];
		WORD		masked;				// [RH] Unused
		BYTE		scalex;				// [RH] Scaling (8 is normal)
		BYTE		scaley;				// [RH] Same as above
		short		width;
		short		height;
		byte		columndirectory[4];	// OBSOLETE
		short		patchcount;
		mappatch_t	patches[1];
	};

	int lumpnum = W_CheckNumForName(lumpname);
	if (lumpnum == -1)
	{
		if (iequals("TEXTURE1", lumpname))
			I_Error("R_InitTextures: TEXTURE1 lump not found");
		return;
	}

	size_t lumplen = W_LumpLength(lumpnum);
	if (lumplen == 0)
		return;

	byte* lumpdata = new byte[lumplen];
	W_ReadLump(lumpnum, lumpdata);

	int* texoffs = (int*)(lumpdata + 4);

	// keep track of the number of texture errors
	int errors = 0;

	int count = LELONG(*((int*)(lumpdata + 0)));
	for (int i = 0; i < count; i++)
	{
		maptexture_t* mtexdef = (maptexture_t*)((byte*)lumpdata + LELONG(texoffs[i]));
		
		size_t texdefsize = sizeof(texdef_t) + sizeof(texdefpatch_t) * (SAFESHORT(mtexdef->patchcount) - 1);
		texdef_t* texdef = (texdef_t*)(new byte[texdefsize]); 	

		texdef->width = SAFESHORT(mtexdef->width);
		texdef->height = SAFESHORT(mtexdef->height);
		texdef->patchcount = SAFESHORT(mtexdef->patchcount);
		texdef->scalex = mtexdef->scalex;
		texdef->scaley = mtexdef->scaley;

		char uname[9];
		for (int c = 0; c < 8; c++)
			uname[c] = toupper(mtexdef->name[c]);
		uname[8] = 0;

		mappatch_t* mpatch = &mtexdef->patches[0];
		texdefpatch_t* patch = &texdef->patches[0];

		for (int j = 0; j < texdef->patchcount; j++, mpatch++, patch++)
		{
			patch->originx = LESHORT(mpatch->originx);
			patch->originy = LESHORT(mpatch->originy);
			patch->patch = mPNameLookup[LESHORT(mpatch->patch)];
			if (patch->patch == -1)
			{
				Printf(PRINT_HIGH, "R_InitTextures: Missing patch in texture %s\n", uname);
				errors++;
			}
		}

		mTextureDefinitions.push_back(texdef);
		mTextureNameTranslationMap[uname] = mTextureDefinitions.size() - 1;
	}

	delete [] lumpdata;
}


//
// TextureManager::createCustomHandle
//
// Generates a valid handle that can be used by the engine to denote certain
// properties for a wall or ceiling or floor. For instance, a special use
// handle can be used to denote that a ceiling should be rendered with SKY2.
//
texhandle_t TextureManager::createCustomHandle()
{
	if (mFreeCustomHandlesTail <= mFreeCustomHandlesHead)
		return TextureManager::NOT_FOUND_TEXTURE_HANDLE;
	return mFreeCustomHandles[mFreeCustomHandlesHead++ % MAX_CUSTOM_HANDLES];
}

//
// TextureManager::freeCustomHandle
//
void TextureManager::freeCustomHandle(texhandle_t texhandle)
{
	mFreeCustomHandles[mFreeCustomHandlesTail++ % MAX_CUSTOM_HANDLES] = texhandle;
}

//
// TextureManager::createTexture
//
// Allocates memory for a new texture and returns a pointer to it. The texture
// is inserted into mHandlesMap for future retrieval.
//
Texture* TextureManager::createTexture(texhandle_t texhandle, int width, int height)
{
	width = std::min<int>(width, Texture::MAX_TEXTURE_WIDTH);
	height = std::min<int>(height, Texture::MAX_TEXTURE_HEIGHT);

	// server shouldn't allocate memory for texture data, only the header	
	size_t texture_size = clientside ?
			Texture::calculateSize(width, height) : sizeof(Texture);
	
	Texture* texture = (Texture*)Z_Malloc(texture_size, PU_STATIC, NULL);
	texture->init(width, height);

	texture->mHandle = texhandle;

	mHandleMap.insert(HandleMapPair(texhandle, texture));

	return texture;
}

//
// TextureManager::freeTexture
//
// Frees the memory used by the specified texture and removes it
// from mHandlesMap.
//
void TextureManager::freeTexture(texhandle_t texhandle)
{
	if (texhandle == TextureManager::NOT_FOUND_TEXTURE_HANDLE ||
		texhandle == TextureManager::NO_TEXTURE_HANDLE)
		return;

	HandleMap::iterator it = mHandleMap.find(texhandle);
	if (it != mHandleMap.end())
	{
		const Texture* texture = it->second;
		if (texture != NULL)
		{
			Z_Free((void*)texture);
			if (texhandle & CUSTOM_HANDLE_MASK)
				freeCustomHandle(texhandle);
		}
		
		mHandleMap.erase(it);
	}
}


//
// TextureManager::getPatchHandle
//
// Returns the handle for the patch with the given WAD lump number.
//
texhandle_t TextureManager::getPatchHandle(unsigned int lumpnum)
{
	if (lumpnum >= numlumps)
		return NOT_FOUND_TEXTURE_HANDLE;

	if (W_LumpLength(lumpnum) == 0)
		return NOT_FOUND_TEXTURE_HANDLE;

	return (texhandle_t)lumpnum | PATCH_HANDLE_MASK;
}

texhandle_t TextureManager::getPatchHandle(const char* name)
{
	int lumpnum = W_CheckNumForName(name);
	if (lumpnum >= 0)
		return getPatchHandle(lumpnum);
	return NOT_FOUND_TEXTURE_HANDLE;
}


//
// TextureManger::cachePatch
//
void TextureManager::cachePatch(texhandle_t handle)
{
	unsigned int lumpnum = handle & ~(PATCH_HANDLE_MASK | SPRITE_HANDLE_MASK);

	unsigned int lumplen = W_LumpLength(lumpnum);
	byte* lumpdata = new byte[lumplen];
	W_ReadLump(lumpnum, lumpdata);

	int width = LESHORT(*(short*)(lumpdata + 0));
	int height = LESHORT(*(short*)(lumpdata + 2));
	int offsetx = LESHORT(*(short*)(lumpdata + 4));
	int offsety = LESHORT(*(short*)(lumpdata + 6));

	Texture* texture = createTexture(handle, width, height);
	texture->mOffsetX = offsetx;
	texture->mOffsetY = offsety;

	if (clientside)
	{
		// TODO: remove this once proper masking is in place
		memset(texture->mData, 0, width * height);

		// initialize the mask to entirely transparent 
		memset(texture->mMask, 0, width * height);

		R_DrawPatchIntoTexture(texture, lumpdata, 0, 0);
		texture->mHasMask = (memchr(texture->mMask, 0, width * height) != NULL);
	}

	delete [] lumpdata;

}


//
// TextureManager::getSpriteHandle
//
// Returns the handle for the sprite with the given WAD lump number.
//
texhandle_t TextureManager::getSpriteHandle(unsigned int lumpnum)
{
	if (lumpnum >= numlumps)
		return NOT_FOUND_TEXTURE_HANDLE;

	if (W_LumpLength(lumpnum) == 0)
		return NOT_FOUND_TEXTURE_HANDLE;

	return (texhandle_t)lumpnum | SPRITE_HANDLE_MASK;
}

texhandle_t TextureManager::getSpriteHandle(const char* name)
{
	int lumpnum = W_CheckNumForName(name, ns_sprites);
	if (lumpnum >= 0)
		return getSpriteHandle(lumpnum);
	lumpnum = W_CheckNumForName(name);
	if (lumpnum >= 0)
		return getSpriteHandle(lumpnum);
	return NOT_FOUND_TEXTURE_HANDLE;
}


//
// TextureManger::cacheSprite
//
void TextureManager::cacheSprite(texhandle_t handle)
{
	cachePatch(handle);
}


//
// TextureManager::getFlatHandle
//
// Returns the handle for the flat with the given WAD lump number.
//
texhandle_t TextureManager::getFlatHandle(unsigned int lumpnum)
{
	const unsigned int flatcount = mLastFlatLumpNum - mFirstFlatLumpNum + 1;
	const unsigned int flatnum = lumpnum - mFirstFlatLumpNum;

	// flatnum > number of flats in the WAD file?
	if (flatnum >= flatcount)
		return NOT_FOUND_TEXTURE_HANDLE;

	if (W_LumpLength(lumpnum) == 0)
		return NOT_FOUND_TEXTURE_HANDLE;

	return (texhandle_t)flatnum | FLAT_HANDLE_MASK;
}

texhandle_t TextureManager::getFlatHandle(const char* name)
{
	int lumpnum = W_CheckNumForName(name, ns_flats);
	if (lumpnum >= 0)
		return getFlatHandle(lumpnum);
	return NOT_FOUND_TEXTURE_HANDLE;
}

//
// TextureManager::cacheFlat
//
// Loads a flat with the specified handle from the WAD file and composes
// a Texture object.
//
void TextureManager::cacheFlat(texhandle_t handle)
{
	// should we check that the handle is valid for a flat?

	unsigned int lumpnum = (handle & ~FLAT_HANDLE_MASK) + mFirstFlatLumpNum;
	unsigned int lumplen = W_LumpLength(lumpnum);

	int width, height;	

	if (lumplen == 64 * 64)
		width = height = 64;
	else if (lumplen == 128 * 128)
		width = height = 128;
	else if (lumplen == 256 * 256)
		width = height = 256;
	else
		width = height = Log2(sqrt(lumplen));	// probably not pretty... 

	Texture* texture = createTexture(handle, width, height);

	if (clientside)
	{
		byte *lumpdata = new byte[lumplen];
		W_ReadLump(lumpnum, lumpdata);

		// convert the row-major flat lump to into column-major
		byte* dest = texture->mData;

		for (int x = 0; x < width; x++)
		{
			const byte* source = lumpdata + x;
			
			for (int y = 0; y < height; y++)
			{
				*dest = *source;
				source += width;
				dest++;
			}
		}
		
		delete [] lumpdata;
	}
}


//
// TextureManager::getWallTextureHandle
//
// Returns the handle for the wall texture with the given WAD lump number.
//
texhandle_t TextureManager::getWallTextureHandle(unsigned int texdef_handle)
{
	// texdef_handle > number of wall textures in the WAD file?
	if (texdef_handle >= mTextureDefinitions.size())
		return NOT_FOUND_TEXTURE_HANDLE;

	return (texhandle_t)texdef_handle | WALLTEXTURE_HANDLE_MASK;
}

texhandle_t TextureManager::getWallTextureHandle(const char* name)
{
	char uname[9];
	for (int i = 0; i < 8; i++)
		uname[i] = toupper(name[i]);
	uname[8] = 0;

	TextureNameTranslationMap::const_iterator it = mTextureNameTranslationMap.find(uname);
	if (it != mTextureNameTranslationMap.end())
		return getWallTextureHandle(it->second);
	return NOT_FOUND_TEXTURE_HANDLE;
}

//
// TextureManager::cacheWallTexture
//
// Composes a wall texture from a set of patches loaded from the WAD file.
//
void TextureManager::cacheWallTexture(texhandle_t handle)
{
	// should we check that the handle is valid for a wall texture?

	texdef_t* texdef = mTextureDefinitions[handle & ~WALLTEXTURE_HANDLE_MASK];

	int width = texdef->width;
	int height = texdef->height;

	Texture* texture = createTexture(handle, width, height);
	if (texdef->scalex)
		texture->mScaleX = texdef->scalex << (FRACBITS - 3);
	if (texdef->scaley)
		texture->mScaleY = texdef->scaley << (FRACBITS - 3);

	if (clientside)
	{
		// TODO: remove this once proper masking is in place
		memset(texture->mData, 0, width * height);

		// initialize the mask to entirely transparent 
		memset(texture->mMask, 0, width * height);

		// compose the texture out of a set of patches
		for (int i = 0; i < texdef->patchcount; i++)
		{
			texdefpatch_t* texdefpatch = &texdef->patches[i];
			
			if (texdefpatch->patch == -1)		// not found ?
				continue;

			unsigned int lumplen = W_LumpLength(texdefpatch->patch);
			byte* lumpdata = new byte[lumplen];
			W_ReadLump(texdefpatch->patch, lumpdata);
			R_DrawPatchIntoTexture(texture, lumpdata, texdefpatch->originx, texdefpatch->originy);

			delete [] lumpdata;
		}

		texture->mHasMask = (memchr(texture->mMask, 0, width * height) != NULL);
	}
}


//
// TextureManager::getRawTextureHandle
//
// Returns the handle for the raw image with the given WAD lump number.
//
texhandle_t TextureManager::getRawTextureHandle(unsigned int lumpnum)
{
	if (lumpnum >= numlumps)
		return NOT_FOUND_TEXTURE_HANDLE;

	if (W_LumpLength(lumpnum) == 0)
		return NOT_FOUND_TEXTURE_HANDLE;
	return (texhandle_t)lumpnum | RAW_HANDLE_MASK;
}

texhandle_t TextureManager::getRawTextureHandle(const char* name)
{
	int lumpnum = W_CheckNumForName(name);
	if (lumpnum >= 0)
		return getRawTextureHandle(lumpnum);
	return NOT_FOUND_TEXTURE_HANDLE;
}

//
// TextureManager::cacheRawTexture
//
// Converts a linear 320x200 block of pixels into a Texture 
//
void TextureManager::cacheRawTexture(texhandle_t handle)
{
	const int width = 320;
	const int height = 200;

	Texture* texture = createTexture(handle, width, height);

	if (clientside)
	{
		unsigned int lumpnum = (handle & ~RAW_HANDLE_MASK);
		unsigned int lumplen = W_LumpLength(lumpnum);

		byte *lumpdata = new byte[lumplen];
		W_ReadLump(lumpnum, lumpdata);

		// convert the row-major flat lump to into column-major
		byte* dest = texture->mData;

		for (int x = 0; x < width; x++)
		{
			const byte* source = lumpdata + x;
			
			for (int y = 0; y < height; y++)
			{
				*dest = *source;
				source += width;
				dest++;
			}
		}
		
		delete [] lumpdata;
	}
}
	

//
// TextureManager::getHandle
//
// Returns the handle for the texture that matches the supplied name.
//
texhandle_t TextureManager::getHandle(const char* name, Texture::TextureSourceType type)
{
	// sidedefs with the '-' texture indicate there should be no texture used
	if (name[0] == '-' && type == Texture::TEX_WALLTEXTURE)
		return NO_TEXTURE_HANDLE;

	texhandle_t handle = NOT_FOUND_TEXTURE_HANDLE;

	// check for the texture in the default location specified by type
	if (type == Texture::TEX_FLAT)
		handle = getFlatHandle(name);
	else if (type == Texture::TEX_WALLTEXTURE)
		handle = getWallTextureHandle(name);
	else if (type == Texture::TEX_PATCH)
		handle = getPatchHandle(name);
	else if (type == Texture::TEX_SPRITE)
		handle = getSpriteHandle(name);
	else if (type == Texture::TEX_RAW)
		handle = getRawTextureHandle(name);

	// not found? check elsewhere
	if (handle == NOT_FOUND_TEXTURE_HANDLE && type != Texture::TEX_FLAT)
		handle = getFlatHandle(name);
	if (handle == NOT_FOUND_TEXTURE_HANDLE && type != Texture::TEX_WALLTEXTURE)
		handle = getWallTextureHandle(name);

	return handle;
}

texhandle_t TextureManager::getHandle(unsigned int lumpnum, Texture::TextureSourceType type)
{
	texhandle_t handle = NOT_FOUND_TEXTURE_HANDLE;

	if (type == Texture::TEX_FLAT)
		handle = getFlatHandle(lumpnum);
	else if (type == Texture::TEX_PATCH)
		handle = getPatchHandle(lumpnum);
	else if (type == Texture::TEX_SPRITE)
		handle = getSpriteHandle(lumpnum);
	else if (type == Texture::TEX_RAW)
		handle = getRawTextureHandle(lumpnum);

	// not found? check elsewhere
	if (handle == NOT_FOUND_TEXTURE_HANDLE && type != Texture::TEX_FLAT)
		handle = getFlatHandle(lumpnum);
	if (handle == NOT_FOUND_TEXTURE_HANDLE && type != Texture::TEX_WALLTEXTURE)
		handle = getWallTextureHandle(lumpnum);

	return handle;
}

//
// TextureManager::getTexture
//
// Returns the Texture for the appropriate handle. If the Texture is not
// currently cached, it will be loaded from the disk and cached.
//
const Texture* TextureManager::getTexture(texhandle_t handle) 
{
	Texture* texture = mHandleMap[handle];
	if (!texture)
	{
		if (handle & FLAT_HANDLE_MASK)
			cacheFlat(handle);
		else if (handle & WALLTEXTURE_HANDLE_MASK)
			cacheWallTexture(handle);
		else if (handle & PATCH_HANDLE_MASK)
			cachePatch(handle);
		else if (handle & SPRITE_HANDLE_MASK)
			cacheSprite(handle);
		else if (handle & RAW_HANDLE_MASK)
			cacheRawTexture(handle);

		texture = mHandleMap[handle];
	}

	return texture;
}

VERSION_CONTROL (r_texture_cpp, "$Id: r_texture.cpp 3945 2013-07-03 14:32:48Z dr_sean $")



