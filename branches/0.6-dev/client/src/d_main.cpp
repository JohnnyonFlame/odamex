// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// $Id$
//
// Copyright (C) 1993-1996 by id Software, Inc.
// Copyright (C) 2006-2012 by The Odamex Team.
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
//		DOOM main program (D_DoomMain) and game loop (D_DoomLoop),
//		plus functions to determine game mode (shareware, registered),
//		parse command line parameters, configure game parameters (turbo),
//		and call the startup functions.
//
//-----------------------------------------------------------------------------

#include "version.h"

#include <sstream>
#include <string>
#include <vector>
#include <algorithm>

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#ifdef _XBOX
#include <xtl.h>
#else
#include <windows.h>
#endif // _XBOX
#else
#include <sys/stat.h>
#endif

#ifdef UNIX
#include <unistd.h>
#include <dirent.h>
#endif

#include <time.h>
#include <math.h>

#include "errors.h"

#include "m_alloc.h"
#include "m_random.h"
#include "minilzo.h"
#include "doomdef.h"
#include "doomstat.h"
#include "gstrings.h"
#include "z_zone.h"
#include "w_wad.h"
#include "s_sound.h"
#include "v_video.h"
#include "f_finale.h"
#include "f_wipe.h"
#include "m_argv.h"
#include "m_fileio.h"
#include "m_misc.h"
#include "m_menu.h"
#include "c_console.h"
#include "c_dispatch.h"
#include "i_system.h"
#include "i_sound.h"
#include "i_video.h"
#include "g_game.h"
#include "hu_stuff.h"
#include "wi_stuff.h"
#include "st_stuff.h"
#include "am_map.h"
#include "c_effect.h"
#include "p_setup.h"
#include "r_local.h"
#include "r_sky.h"
#include "d_main.h"
#include "d_dehacked.h"
#include "cmdlib.h"
#include "s_sound.h"
#include "m_swap.h"
#include "v_text.h"
#include "gi.h"
#include "stats.h"
#include "p_ctf.h"
#include "cl_main.h"

#ifdef GEKKO
#include "i_wii.h"
#endif

#ifdef _XBOX
#include "i_xbox.h"
#endif

extern size_t got_heapsize;

//extern void M_RestoreMode (void); // [Toke - Menu]
extern void R_ExecuteSetViewSize (void);

void D_CheckNetGame (void);
void D_ProcessEvents (void);
void D_DoAdvanceDemo (void);

void D_DoomLoop (void);


extern gameinfo_t SharewareGameInfo;
extern gameinfo_t RegisteredGameInfo;
extern gameinfo_t RetailGameInfo;
extern gameinfo_t CommercialGameInfo;

extern QWORD testingmode;
extern BOOL setsizeneeded;
extern BOOL setmodeneeded;
extern int NewWidth, NewHeight, NewBits, DisplayBits;
EXTERN_CVAR (st_scale)
extern BOOL gameisdead;
extern BOOL demorecording;
extern bool M_DemoNoPlay;	// [RH] if true, then skip any demos in the loop
extern DThinker ThinkerCap;
extern int NoWipe;			// [RH] Don't wipe when travelling in hubs

std::vector<std::string> wadfiles, wadhashes;		// [RH] remove limit on # of loaded wads
BOOL devparm;				// started game with -devparm
const char *D_DrawIcon;			// [RH] Patch name of icon to draw on next refresh
int NoWipe;					// [RH] Allow wipe? (Needs to be set each time)
char startmap[8];
BOOL autostart;
BOOL autorecord;
std::string demorecordfile;
BOOL advancedemo;
event_t events[MAXEVENTS];
int eventhead;
int eventtail;
gamestate_t wipegamestate = GS_DEMOSCREEN;	// can be -1 to force a wipe
DCanvas *page;
bool demotest;

static int demosequence;
static int pagetic;

EXTERN_CVAR (sv_allowexit)
EXTERN_CVAR (sv_nomonsters)
EXTERN_CVAR (sv_monstersrespawn)
EXTERN_CVAR (sv_fastmonsters)
EXTERN_CVAR (sv_freelook)
EXTERN_CVAR (sv_allowjump)
EXTERN_CVAR (sv_allowredscreen)
EXTERN_CVAR (waddirs)
EXTERN_CVAR (snd_sfxvolume)				// maximum volume for sound
EXTERN_CVAR (snd_musicvolume)			// maximum volume for music

const char *LOG_FILE;

void M_RestoreMode (void);
void M_ModeFlashTestText (void);

//
// D_ProcessEvents
// Send all the events of the given timestamp down the responder chain
//
void D_ProcessEvents (void)
{
	event_t *ev;

	// [RH] If testing mode, do not accept input until test is over
	if (testingmode)
	{
		if (testingmode <= I_GetTime())
		{
			M_RestoreMode ();
		}
		else
		{
			M_ModeFlashTestText();
		}

		return;
	}

	for (; eventtail != eventhead ; eventtail = ++eventtail<MAXEVENTS ? eventtail : 0)
	{
		ev = &events[eventtail];
		if (C_Responder (ev))
			continue;				// console ate the event
		if (M_Responder (ev))
			continue;				// menu ate the event
		G_Responder (ev);
	}
}

//
// D_PostEvent
// Called by the I/O functions when input is detected
//
void D_PostEvent (const event_t* ev)
{
	events[eventhead] = *ev;

	if(++eventhead >= MAXEVENTS)
		eventhead = 0;
}

//
// D_Display
//  draw current display, possibly wiping it from the previous
//
void D_Display (void)
{
	BOOL wipe;

	if (nodrawers)
		return; 				// for comparative timing / profiling

	BEGIN_STAT(D_Display);

	if (gamestate == GS_LEVEL && viewactive && consoleplayer().camera)
		R_SetFOV (consoleplayer().camera->player ?
			consoleplayer().camera->player->fov : 90.0f);

	// [RH] change the screen mode if needed
	if (setmodeneeded)
	{
		int oldwidth = screen->width;
		int oldheight = screen->height;
		int oldbits = DisplayBits;

		// Change screen mode.
		if (!V_SetResolution (NewWidth, NewHeight, NewBits))
			if (!V_SetResolution (oldwidth, oldheight, oldbits))
				I_FatalError ("Could not change screen mode");

		// Recalculate various view parameters.
		setsizeneeded = true;
		// Trick status bar into rethinking its position
		st_scale.Callback ();
		// Refresh the console.
		C_NewModeAdjust ();
	}

	// change the view size if needed
	if (setsizeneeded)
	{
		R_ExecuteSetViewSize ();
		setmodeneeded = false;
	}

	I_BeginUpdate ();

	// [RH] Allow temporarily disabling wipes
	if (NoWipe)
	{
		NoWipe--;
		wipe = false;
		wipegamestate = gamestate;
	}
	else if (gamestate != wipegamestate && gamestate != GS_FULLCONSOLE)
	{ // save the current screen if about to wipe
		wipe = true;
		wipe_StartScreen ();
		wipegamestate = gamestate;
	}
	else
	{
		wipe = false;
	}

	switch (gamestate)
	{
		case GS_FULLCONSOLE:
		case GS_DOWNLOAD:
		case GS_CONNECTING:
			C_DrawConsole ();
			M_Drawer ();
			I_FinishUpdate ();
			return;

		case GS_LEVEL:
			if (!gametic)
				break;

			// denis - freshen the borders (ffs..)
			R_DrawViewBorder ();    // erase old menu stuff

			if (viewactive)
				R_RenderPlayerView (&displayplayer());
			if (automapactive)
				AM_Drawer ();
			C_DrawMid ();
			CTF_DrawHud ();
			ST_Drawer ();
			HU_Drawer ();
			break;

		case GS_INTERMISSION:
			if (viewactive)
				R_RenderPlayerView (&displayplayer());
			C_DrawMid ();
			CTF_DrawHud ();
			WI_Drawer ();
			HU_Drawer ();
			break;

		case GS_FINALE:
			F_Drawer ();
			break;

		case GS_DEMOSCREEN:
			D_PageDrawer ();
			break;

	default:
	    break;
	}

	// draw pause pic
	if (paused && !menuactive)
	{
		patch_t *pause = W_CachePatch ("M_PAUSE");
		int y;

		y = (automapactive && !viewactive) ? 4 : viewwindowy + 4;
		screen->DrawPatchCleanNoMove (pause, (screen->width-(pause->width())*CleanXfac)/2, y);
	}

	// [RH] Draw icon, if any
	if (D_DrawIcon)
	{
		int lump = W_CheckNumForName (D_DrawIcon);

		D_DrawIcon = NULL;
		if (lump >= 0)
		{
			patch_t *p = W_CachePatch (lump);

			screen->DrawPatchIndirect (p, 160-p->width()/2, 100-p->height()/2);
		}
		NoWipe = 10;
	}

	static bool live_wiping = false;

	if (!wipe)
	{
		if(live_wiping)
		{
			// wipe update online (multiple calls, not just looping here)
			C_DrawConsole ();
			wipe_EndScreen();
			live_wiping = !wipe_ScreenWipe (1);
			M_Drawer ();			// menu is drawn even on top of wipes
			I_FinishUpdate ();		// page flip or blit buffer
		}
		else
		{
			// normal update
			C_DrawConsole ();	// draw console
			M_Drawer ();		// menu is drawn even on top of everything
			I_FinishUpdate ();	// page flip or blit buffer
		}
	}
	else
	{
		if(!connected)
		{
			// wipe update offline
			int wipestart, wipecont, nowtime, tics;
			BOOL done;

			C_DrawConsole ();
			wipe_EndScreen ();
			I_FinishUpdateNoBlit ();

			extern int canceltics;

			wipestart = I_GetTime ();
			wipecont = wipestart - 1;

			do
			{
				do
				{
					nowtime = I_GetTime ();
					tics = nowtime - wipecont;
				} while (!tics);
				wipecont = nowtime;
				I_BeginUpdate ();
				done = wipe_ScreenWipe (tics);
				M_Drawer ();			// menu is drawn even on top of wipes
				I_FinishUpdate ();		// page flip or blit buffer
			} while (!done);

			if(!connected)
				canceltics += I_GetTime () - wipestart;
		}
		else
		{
			// wipe update online
			live_wiping = true;

			// wipe update online (multiple calls, not just looping here)
			C_DrawConsole ();
			wipe_EndScreen();
			live_wiping = !wipe_ScreenWipe (1);
			M_Drawer ();			// menu is drawn even on top of wipes
			I_FinishUpdate ();		// page flip or blit buffer
		}
	}

	END_STAT(D_Display);
}

//
//  D_DoomLoop
//
void D_DoomLoop (void)
{
	while (1)
	{
		try
		{
			TryRunTics (); // will run at least one tic

			if (!connected)
				CL_RequestConnectInfo();

			// [RH] Use the consoleplayer's camera to update sounds
			S_UpdateSounds (listenplayer().camera);	// move positional sounds
			S_UpdateMusic();	// play another chunk of music

			// Update display, next frame, with current state.
			D_Display ();
		}
		catch (CRecoverableError &error)
		{
			Printf_Bold ("\n%s\n", error.GetMsg().c_str());

			CL_QuitNetGame ();

			G_ClearSnapshots ();

			DThinker::DestroyAllThinkers();

			players.clear();

			gameaction = ga_fullconsole;
		}
	}
}

//
// D_PageTicker
// Handles timing for warped projection
//
void D_PageTicker (void)
{
    if (--pagetic < 0)
		D_AdvanceDemo ();
}

//
// D_PageDrawer
//
void D_PageDrawer (void)
{
	if (page)
	{
		page->Blit (0, 0, page->width, page->height,
			screen, 0, 0, screen->width, screen->height);
	}
	else
	{
		screen->Clear (0, 0, screen->width, screen->height, 0);
		//screen->PrintStr (0, 0, "Page graphic goes here", 22);
	}
}

//
// D_AdvanceDemo
// Called after each demo or intro demosequence finishes
//
void D_AdvanceDemo (void)
{
	advancedemo = true;
}

//
// This cycles through the demo sequences.
//
void D_DoAdvanceDemo (void)
{
	const char *pagename = NULL;

	consoleplayer().playerstate = PST_LIVE;	// not reborn
	advancedemo = false;
	usergame = false;				// no save / end game here
	paused = false;
	gameaction = ga_nothing;

    // [Russell] - Old demo sequence used in original games, zdoom's
    // dynamic one was too dynamic for its own good
    // [Nes] - Newer demo sequence with better flow.
    if (W_CheckNumForName("DEMO4") >= 0 && gamemode != retail_chex)
        demosequence = (demosequence+1)%8;
    else
        demosequence = (demosequence+1)%6;

    switch (demosequence)
    {
        case 0:
            if (gamemode == commercial)
                pagetic = TICRATE * 11;
            else
                pagetic = 170;

            gamestate = GS_DEMOSCREEN;
            pagename = "TITLEPIC";

            S_StartMusic(gameinfo.titleMusic);

            break;
        case 1:
            G_DeferedPlayDemo("DEMO1");

            break;
        case 2:
            pagetic = 200;
            gamestate = GS_DEMOSCREEN;
            pagename = "CREDIT";

            break;
        case 3:
            G_DeferedPlayDemo("DEMO2");

            break;
        case 4:
            gamestate = GS_DEMOSCREEN;

            if (gamemode == commercial || gamemode == retail)
            {
				if (gamemode == commercial)
					pagetic = TICRATE * 11;
				else
					pagetic = 170;
                pagename = "TITLEPIC";
                S_StartMusic(gameinfo.titleMusic);
            }
            else
            {
                pagetic = 200;
				if (gamemode == retail_chex)	// [ML] Chex mode just cycles this screen
					pagename = "CREDIT";
				else
					pagename = "HELP2";
            }

            break;
        case 5:
            G_DeferedPlayDemo("DEMO3");

            break;
        case 6:
            pagetic = 200;
            gamestate = GS_DEMOSCREEN;
            pagename = "CREDIT";

            break;
        case 7:
            G_DeferedPlayDemo("DEMO4");

            break;
    }

    // [Russell] - Still need this toilet humor for now unfortunately
	if (pagename)
	{
		int width, height;
		patch_t *data;

		if (gameinfo.flags & GI_PAGESARERAW)
		{
			data = W_CachePatch (pagename);
			width = 320;
			height = 200;
		}
		else
		{
			data = W_CachePatch (pagename);
			width = data->width();
			height = data->height();
		}

		if (page && (page->width != width || page->height != height))
		{
			I_FreeScreen(page);
			page = NULL;
		}

		if (page == NULL)
			page = I_AllocateScreen (width, height, 8);

		page->Lock ();
		if (gameinfo.flags & GI_PAGESARERAW)
			page->DrawBlock (0, 0, 320, 200, (byte *)data);
		else
			page->DrawPatch (data, 0, 0);
		page->Unlock ();
	}
}

//
// D_Close
//
void STACK_ARGS D_Close (void)
{
	if(page)
	{
		I_FreeScreen(page);
		page = NULL;
	}
}

//
// D_StartTitle
//
void D_StartTitle (void)
{
	// CL_QuitNetGame();
	bool firstTime = true;
	if(firstTime)
		atterm (D_Close);

	gameaction = ga_nothing;
	demosequence = -1;
	D_AdvanceDemo ();
}

bool HashOk(std::string &required, std::string &available)
{
	if(!required.length())
		return false;

	return required == available;
}

//
// denis - BaseFileSearchDir
// Check single paths for a given file with a possible extension
// Case insensitive, but returns actual file name
//
std::string BaseFileSearchDir(std::string dir, std::string file, std::string ext, std::string hash = "")
{
	std::string found;

	if(dir[dir.length() - 1] != PATHSEPCHAR)
		dir += PATHSEP;

	std::transform(hash.begin(), hash.end(), hash.begin(), toupper);
	std::string dothash = ".";
	if(hash.length())
		dothash += hash;
	else
		dothash = "";

	// denis - list files in the directory of interest, case-desensitize
	// then see if wanted wad is listed
#ifdef UNIX
	struct dirent **namelist = 0;
	int n = scandir(dir.c_str(), &namelist, 0, alphasort);

	for(int i = 0; i < n && namelist[i]; i++)
	{
		std::string d_name = namelist[i]->d_name;

		M_Free(namelist[i]);

		if(!found.length())
		{
			if(d_name == "." || d_name == "..")
				continue;

			std::string tmp = d_name;
			std::transform(tmp.begin(), tmp.end(), tmp.begin(), toupper);

			if(file == tmp || (file + ext) == tmp || (file + dothash) == tmp || (file + ext + dothash) == tmp)
			{
				std::string local_file = (dir + d_name).c_str();
				std::string local_hash = W_MD5(local_file.c_str());

				if(!hash.length() || hash == local_hash)
				{
					found = d_name;
				}
				else if(hash.length())
				{
					Printf (PRINT_HIGH, "WAD at %s does not match required copy\n", local_file.c_str());
					Printf (PRINT_HIGH, "Local MD5: %s\n", local_hash.c_str());
					Printf (PRINT_HIGH, "Required MD5: %s\n\n", hash.c_str());
				}
			}
		}
	}

	M_Free(namelist);
#else
	std::string all_ext = dir + "*";
	//all_ext += ext;

	WIN32_FIND_DATA FindFileData;
	HANDLE hFind = FindFirstFile(all_ext.c_str(), &FindFileData);
	DWORD dwError = GetLastError();

	if (hFind == INVALID_HANDLE_VALUE)
	{
		Printf (PRINT_HIGH, "FindFirstFile failed for %s\n", all_ext.c_str());
		Printf (PRINT_HIGH, "GetLastError: %d\n", dwError);
		return "";
	}

	do
	{
		if(FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			continue;

		std::string tmp = FindFileData.cFileName;
		std::transform(tmp.begin(), tmp.end(), tmp.begin(), toupper);

		if(file == tmp || (file + ext) == tmp || (file + dothash) == tmp || (file + ext + dothash) == tmp)
		{
			std::string local_file = (dir + FindFileData.cFileName).c_str();
			std::string local_hash = W_MD5(local_file.c_str());

			if(!hash.length() || hash == local_hash)
			{
				found = FindFileData.cFileName;
				break;
			}
			else if(hash.length())
			{
				Printf (PRINT_HIGH, "WAD at %s does not match required copy\n", local_file.c_str());
				Printf (PRINT_HIGH, "Local MD5: %s\n", local_hash.c_str());
				Printf (PRINT_HIGH, "Required MD5: %s\n\n", hash.c_str());
			}
		}
	} while(FindNextFile(hFind, &FindFileData));

	dwError = GetLastError();
	
	// Note: As documented, FindNextFile sets ERROR_NO_MORE_FILES as the error 
	// code, but when this function "fails" it does not set it we have to assume 
	// that it completed successfully (this is actually  bad practice, because 
    // it says in the docs that it does not set ERROR_SUCCESS, even though 
    // GetLastError returns 0) WTF DO WE DO?!
	if(dwError != ERROR_SUCCESS && dwError != ERROR_NO_MORE_FILES)
		Printf (PRINT_HIGH, "FindNextFile failed. GetLastError: %d\n", dwError);

	FindClose(hFind);
#endif

	return found;
}

//
// denis - D_AddSearchDir
// Split a new directory string using the separator and append results to the output
//
void D_AddSearchDir(std::vector<std::string> &dirs, const char *dir, const char separator)
{
	if(!dir)
		return;

	// search through dwd
	std::stringstream ss(dir);
	std::string segment;

	while(!ss.eof())
	{
		std::getline(ss, segment, separator);

		if(!segment.length())
			continue;

		FixPathSeparator(segment);
		I_ExpandHomeDir(segment);

		if(segment[segment.length() - 1] != PATHSEPCHAR)
			segment += PATHSEP;

		dirs.push_back(segment);
	}
}

//
// denis - BaseFileSearch
// Check all paths of interest for a given file with a possible extension
//
std::string BaseFileSearch(std::string file, std::string ext, std::string hash)
{
	#ifdef WIN32
		// absolute path?
		if(file.find(':') != std::string::npos)
			return file;

		const char separator = ';';
	#else
		// absolute path?
		if(file[0] == PATHSEPCHAR || file[0] == '~')
			return file;

		const char separator = ':';
	#endif

    // [Russell] - Bit of a hack. (since BaseFileSearchDir should handle this)
    // return file if it contains a path already
    if (M_FileExists(file))
        return file;

	std::transform(file.begin(), file.end(), file.begin(), toupper);
	std::transform(ext.begin(), ext.end(), ext.begin(), toupper);
	std::vector<std::string> dirs;

	dirs.push_back(startdir);
	dirs.push_back(progdir);

	D_AddSearchDir(dirs, Args.CheckValue("-waddir"), separator);
	D_AddSearchDir(dirs, getenv("DOOMWADDIR"), separator);
	D_AddSearchDir(dirs, getenv("DOOMWADPATH"), separator);
	D_AddSearchDir(dirs, getenv("HOME"), separator);
	D_AddSearchDir(dirs, waddirs.cstring(), separator);

    dirs.erase(std::unique(dirs.begin(), dirs.end()), dirs.end());

	for(size_t i = 0; i < dirs.size(); i++)
	{
		std::string found = BaseFileSearchDir(dirs[i], file, ext, hash);

		if(found.length())
		{
			std::string &dir = dirs[i];

			if(dir[dir.length() - 1] != PATHSEPCHAR)
				dir += PATHSEP;

			return dir + found;
		}
	}

	// Not found
	return "";
}

//
// CheckIWAD
//
// Tries to find an IWAD from a set of know IWAD names, and checks the first
// one found's contents to determine whether registered/commercial features
// should be executed (notably loading PWAD's).
//
static bool CheckIWAD (std::string suggestion, std::string &titlestring)
{
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
		"freedm.wad",
		"chex.wad",		// [ML] 1/7/10: Hello Chex Quest!
		NULL
	};

	std::string iwad;
	std::string iwad_file;
	int i;

	if(suggestion.length())
	{
		std::string found = BaseFileSearch(suggestion, ".WAD");

		if(found.length())
			iwad = found;
		else
		{
			if(M_FileExists(suggestion.c_str()))
				iwad = suggestion;
		}
		/*	[ML] Removed 1/13/10: we can trust the user to provide an iwad
		if(iwad.length())
		{
			FILE *f;

			if ( (f = fopen (iwad.c_str(), "rb")) )
			{
				wadinfo_t header;
				fread (&header, sizeof(header), 1, f);
				header.identification = LONG(header.identification);
				if (header.identification != IWAD_ID)
				{
					if(header.identification == PWAD_ID)
					{
						Printf(PRINT_HIGH, "Suggested file is a PWAD, not an IWAD: %s \n", iwad.c_str());
					}
					else
					{
						Printf(PRINT_HIGH, "Suggested file is not an IWAD: %s \n", iwad.c_str());
					}
					iwad = "";
				}
				fclose(f);
			}
		}
		*/
	}

	if(!iwad.length())
	{
		// Search for a pre-defined IWAD from the list above
		for (i = 0; doomwadnames[i]; i++)
		{
			std::string found = BaseFileSearch(doomwadnames[i]);

			if(found.length())
			{
				iwad = found;
				break;
			}
		}
	}

	// Now scan the contents of the IWAD to determine which one it is
	if (iwad.length())
	{
#define NUM_CHECKLUMPS 9
		static const char checklumps[NUM_CHECKLUMPS][8] = {
			"E1M1", "E2M1", "E4M1", "MAP01",
			{ 'A','N','I','M','D','E','F','S'},
			"FINAL2", "REDTNT2", "CAMO1",
			{ 'E','X','T','E','N','D','E','D'}
		};
		int lumpsfound[NUM_CHECKLUMPS];
		wadinfo_t header;
		FILE *f;
		M_ExtractFileName(iwad,iwad_file);

		memset (lumpsfound, 0, sizeof(lumpsfound));
		if ( (f = fopen (iwad.c_str(), "rb")) )
		{
			fread (&header, sizeof(header), 1, f);
			header.identification = LONG(header.identification);
			if (header.identification == IWAD_ID ||
				header.identification == PWAD_ID)
			{
				header.numlumps = LONG(header.numlumps);
				if (0 == fseek (f, LONG(header.infotableofs), SEEK_SET))
				{
					for (i = 0; i < header.numlumps; i++)
					{
						filelump_t lump;
						int j;

						if (0 == fread (&lump, sizeof(lump), 1, f))
							break;
						for (j = 0; j < NUM_CHECKLUMPS; j++)
							if (!strnicmp (lump.name, checklumps[j], 8))
								lumpsfound[j]++;
					}
				}
			}
			fclose (f);
		}

		gamemode = undetermined;

		if (lumpsfound[3])
		{
			gamemode = commercial;
			gameinfo = CommercialGameInfo;
			if (lumpsfound[6])
			{
				gamemission = pack_tnt;
				titlestring = "DOOM 2: TNT - Evilution";
			}
			else if (lumpsfound[7])
			{
				gamemission = pack_plut;
				titlestring = "DOOM 2: Plutonia Experiment";
			}
			else
			{
				gamemission = doom2;
				titlestring = "DOOM 2: Hell on Earth";
			}
		}
		else if (lumpsfound[0])
		{
			gamemission = doom;
			if (lumpsfound[1])
			{
				if (lumpsfound[2])
				{
					if (!StdStringCompare(iwad_file,"chex.wad",true))	// [ML] 1/7/10: HACK - There's no unique lumps in the chex quest
					{													// iwad.  It's ultimate doom with their stuff replacing most things.
						gamemission = chex;
						gamemode = retail_chex;
						gameinfo = RetailGameInfo;
						titlestring = "Chex Quest";
					}
					else
					{
						gamemode = retail;
						gameinfo = RetailGameInfo;
						titlestring = "The Ultimate DOOM";
					}
				}
				else
				{
					gamemode = registered;
					gameinfo = RegisteredGameInfo;
					titlestring = "DOOM Registered";
				}
			}
			else
			{
				gamemode = shareware;
				gameinfo = SharewareGameInfo;
				titlestring = "DOOM Shareware";
			}
		}
	}

	if (gamemode == undetermined)
	{
		gameinfo = SharewareGameInfo;
	}
	if (iwad.length())
		wadfiles.push_back(iwad);
	else
		I_Error("Cannot find IWAD (try -waddir)");

	return iwad.length() ? true : false;
}

//
// IdentifyVersion
//
static std::string IdentifyVersion (std::string custwad)
{
	std::string titlestring = "Public DOOM - ";

	Printf(PRINT_HIGH, "\n\35\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36"
                       "\36\36\36\36\36\36\36\36\36\36\36\36\37\n");

	if(!CheckIWAD(custwad, titlestring))
		Printf_Bold ("Game mode indeterminate, no standard wad found.\n\n");
	else
		Printf_Bold ("%s\n\n", titlestring.c_str());

	return titlestring;
}

//
// D_AddDefWads
//
void D_AddDefWads (std::string iwad)
{
	{
		// [RH] Make sure zdoom.wad is always loaded,
		// as it contains magic stuff we need.
		std::string wad = BaseFileSearch ("odamex.wad");
		if (wad.length())
			wadfiles.push_back(wad);
		else
			I_FatalError ("Cannot find odamex.wad");
	}

	I_SetTitleString(IdentifyVersion(iwad).c_str());
}

//
// D_AddDefSkins
//
/*void D_AddDefSkins (void)
{
	// [RH] Add any .wad files in the skins directory
#ifndef UNIX // denis - fixme - 1) _findnext not implemented on linux or osx, use opendir 2) server does not need skins, does it?
	{
		char curdir[256];

		if (getcwd (curdir, 256))
		{
			char skindir[256];
			findstate_t findstate; // denis - fixme - win32 dependency == BAD!!! this is solved in later csdooms with BaseFileSearch - that could be implemented better with posix opendir stuff
			long handle;
			int stuffstart;

			std::string pd = progdir;
			if(pd[pd.length() - 1] != PATHSEPCHAR)
				pd += PATHSEPCHAR;

			stuffstart = sprintf (skindir, "%sskins", pd.c_str());

			if (!chdir (skindir))
			{
				skindir[stuffstart++] = PATHSEPCHAR;
				if ((handle = I_FindFirst ("*.wad", &findstate)) != -1)
				{
					do
					{
						if (!(I_FindAttr (&findstate) & FA_DIREC))
						{
							strcpy (skindir + stuffstart,
									I_FindName (&findstate));
							wadfiles.push_back(skindir);
						}
					} while (I_FindNext (handle, &findstate) == 0);
					I_FindClose (handle);
				}
			}

			const char *home = getenv ("HOME");
			if (home)
			{
				stuffstart = sprintf (skindir, "%s%s.odamex/skins", home,
									  home[strlen(home)-1] == PATHSEPCHAR ? "" : PATHSEP);
				if (!chdir (skindir))
				{
					skindir[stuffstart++] = PATHSEPCHAR;
					if ((handle = I_FindFirst ("*.wad", &findstate)) != -1)
					{
						do
						{
							if (!(I_FindAttr (&findstate) & FA_DIREC))
							{
								strcpy (skindir + stuffstart,
										I_FindName (&findstate));
								wadfiles.push_back(skindir);
							}
						} while (I_FindNext (handle, &findstate) == 0);
						I_FindClose (handle);
					}
				}
			}
			chdir (curdir);
		}
	}
#endif
}*/

//
// D_AddCmdParameterFiles
// Add the files specified with -file, do this only when it first loads
//
void D_AddCmdParameterFiles(void)
{
    modifiedgame = false;

	DArgs files = Args.GatherFiles ("-file", ".wad", true);
	if (files.NumArgs() > 0)
	{
		// the files gathered are wadfile/lump names
		modifiedgame = true;			// homebrew levels
		for (size_t i = 0; i < files.NumArgs(); i++)
		{
			std::string file = BaseFileSearch (files.GetArg (i), ".WAD");
			if (file.length())
				wadfiles.push_back(file);
		}
	}
}

//
// D_DoDefDehackedPatch
//
// [Russell] - Change the meaning, this will load multiple patch files if
//             specified
void D_DoDefDehackedPatch (const std::vector<std::string> patch_files = std::vector<std::string>())
{
    DArgs files;
    BOOL noDef = false;
    BOOL chexLoaded = false;
    QWORD i;

    if (!patch_files.empty())
    {
        std::string f;
        std::string ext;

        // we want the extension of the file
        for (i = 0; i < patch_files.size(); i++)
        {
            if (M_ExtractFileExtension(patch_files[i], ext))
            {
                f = BaseFileSearch (patch_files[i], ext);

                if (f.length())
                {
                    DoDehPatch (f.c_str(), false);

                    noDef = true;
                }
            }
        }
    }
    else // [Russell] - Only load if patch_files is empty
    {
        // try .deh files on command line

        files = Args.GatherFiles ("-deh", ".deh", false);

        if (files.NumArgs())
        {
            for (i = 0; i < files.NumArgs(); i++)
            {
                std::string f = BaseFileSearch (files.GetArg (i), ".DEH");

                if (f.length()) {
                    DoDehPatch (f.c_str(), false);
                    if (!strncmp(files.GetArg (i),"chex.deh",8))
						chexLoaded = true;
                }
            }
            noDef = true;
        }

        if (gamemode == retail_chex && !multiplayer && !chexLoaded)
			Printf(PRINT_HIGH,"Warning: chex.deh not loaded, experience may differ from the original!\n");

        // remove the old arguments
        files.FlushArgs();

        // try .bex files on command line
        files = Args.GatherFiles ("-bex", ".bex", false);

        if (files.NumArgs())
        {
            for (i = 0; i < files.NumArgs(); i++)
            {
                std::string f = BaseFileSearch (files.GetArg (i), ".BEX");

                if (f.length())
                    DoDehPatch (f.c_str(), false);
            }
            noDef = true;
        }
    }

    // try default patches
    if (!noDef)
        DoDehPatch (NULL, true);	// See if there's a patch in a PWAD
}

//
// [denis] D_DoomWadReboot
// change wads at runtime
// checks hashes if provided
// on 404 or bad hash, returns a vector of bad files
//
void V_InitPalette (void);

bool lastWadRebootSuccess = true;

std::vector<size_t> D_DoomWadReboot(
	const std::vector<std::string> &wadnames,
    const std::vector<std::string> &patch_files,
    std::vector<std::string> needhashes
)
{
	std::vector<size_t> fails;
	size_t i;


	// already loaded these?
 	if (lastWadRebootSuccess &&
		!wadhashes.empty() &&
			needhashes ==
				std::vector<std::string>(wadhashes.begin()+1, wadhashes.end()))
	{
		// fast track if files have not been changed // denis - todo - actually check the file timestamps
		Printf (PRINT_HIGH, "Currently loaded WADs match server checksum\n\n");
		return std::vector<size_t>();
	}

	// assume failure
	lastWadRebootSuccess = false;

	if (modifiedgame && (gameinfo.flags & GI_SHAREWARE))
		I_Error ("\nYou cannot switch WAD with the shareware version. Register!");

	if(gamestate == GS_LEVEL)
		G_ExitLevel(0, 0);

	AM_Stop();
	S_Stop();

	DThinker::DestroyAllThinkers();

	// Close all open WAD files
	W_Close();

	// [ML] 9/11/10: Reset custom wad level information from MAPINFO et al.
    // I have never used memset, I hope I am not invoking satan by doing this :(
	if (wadlevelinfos)
    {
		for (i = 0; i < numwadlevelinfos; i++)
			if (wadlevelinfos[i].snapshot)
			{
				delete wadlevelinfos[i].snapshot;
				wadlevelinfos[i].snapshot = NULL;
			}
        memset(wadlevelinfos,0,sizeof(wadlevelinfos));
        numwadlevelinfos = 0;
    }

    if (wadclusterinfos)
    {
        memset(wadclusterinfos,0,sizeof(wadclusterinfos));
        numwadclusterinfos = 0;
    }

	// Restart the memory manager
	Z_Init();

	gamestate_t oldgamestate = gamestate;
	gamestate = GS_STARTUP; // prevent console from trying to use nonexistant font

	wadfiles.clear();
	modifiedgame = false;

	std::string custwad;
	if(wadnames.empty() == false)
		custwad = wadnames[0];

	D_AddDefWads(custwad);

	for(i = 0; i < wadnames.size(); i++)
	{
		std::string tmp = wadnames[i];

		// strip absolute paths, as they present a security risk
		FixPathSeparator(tmp);
		size_t slash = tmp.find_last_of(PATHSEPCHAR);
		if(slash != std::string::npos)
			tmp = tmp.substr(slash + 1, tmp.length() - slash);

        // [Russell] - Generate a hash if it doesn't exist already
        if (needhashes[i].empty())
            needhashes[i] = W_MD5(tmp);

		std::string file = BaseFileSearch(tmp, ".wad", needhashes[i]);

		if(file.length())
			wadfiles.push_back(file);
		else
		{
			Printf (PRINT_HIGH, "could not find WAD: %s\n", tmp.c_str());
			fails.push_back(i);
		}
	}

	if(wadnames.size() > 1)
		modifiedgame = true;

	wadhashes = W_InitMultipleFiles (wadfiles);

    UndoDehPatch();

	// [RH] Initialize localizable strings.
	GStrings.ResetStrings ();
	GStrings.Compact ();

	D_DoDefDehackedPatch(patch_files);

	//gotconback = false;
	//C_InitConsole(DisplayWidth, DisplayHeight, true);

	HU_Init ();

	if(!(DefaultPalette = InitPalettes("PLAYPAL")))
		I_Error("Could not reinitialize palette");
	V_InitPalette();

	G_SetLevelStrings ();
	G_ParseMapInfo ();
	G_ParseMusInfo ();
	S_ParseSndInfo();

	M_Init();
	R_Init();
	P_InitEffects();	// [ML] Do this here so we don't have to put particle crap in server
	P_Init();

	S_Init (snd_sfxvolume, snd_musicvolume);
	ST_Init();

	// preserve state
	lastWadRebootSuccess = fails.empty();

	gamestate = oldgamestate; // GS_STARTUP would prevent netcode connecting properly

	return fails;
}

void CL_NetDemoRecord(const std::string &filename);
void CL_NetDemoPlay(const std::string &filename);

//
// D_DoomMain
//
void D_DoomMain (void)
{
	unsigned p;
	const char *iwad;
	extern std::string defdemoname;

	M_ClearRandom();

	gamestate = GS_STARTUP;
	SetLanguageIDs ();
	M_FindResponseFile();		// [ML] 23/1/07 - Add Response file support back in

	if (lzo_init () != LZO_E_OK)	// [RH] Initialize the minilzo package.
		I_FatalError ("Could not initialize LZO routines");

    C_ExecCmdLineParams (false, true);	// [Nes] test for +logfile command

	Printf (PRINT_HIGH, "Heapsize: %u megabytes\n", got_heapsize);

	M_LoadDefaults ();					// load before initing other systems
	C_ExecCmdLineParams (true, false);	// [RH] do all +set commands on the command line

	iwad = Args.CheckValue("-iwad");
	if(!iwad)
		iwad = "";

	D_AddDefWads(iwad);
	D_AddCmdParameterFiles();

	wadhashes = W_InitMultipleFiles (wadfiles);

	// [RH] Initialize localizable strings.
	GStrings.LoadStrings (W_GetNumForName ("LANGUAGE"), STRING_TABLE_SIZE, false);
	GStrings.Compact ();

	// [RH] Initialize configurable strings.
	//D_InitStrings ();
	D_DoDefDehackedPatch ();

	// [RH] Moved these up here so that we can do most of our
	//		startup output in a fullscreen console.

	HU_Init ();
	I_Init ();
	V_Init ();

	// Base systems have been inited; enable cvar callbacks
	cvar_t::EnableCallbacks ();

	// [RH] User-configurable startup strings. Because BOOM does.
	if (GStrings(STARTUP1)[0])	Printf (PRINT_HIGH, "%s\n", GStrings(STARTUP1));
	if (GStrings(STARTUP2)[0])	Printf (PRINT_HIGH, "%s\n", GStrings(STARTUP2));
	if (GStrings(STARTUP3)[0])	Printf (PRINT_HIGH, "%s\n", GStrings(STARTUP3));
	if (GStrings(STARTUP4)[0])	Printf (PRINT_HIGH, "%s\n", GStrings(STARTUP4));
	if (GStrings(STARTUP5)[0])	Printf (PRINT_HIGH, "%s\n", GStrings(STARTUP5));

	// Nomonsters
	sv_nomonsters = Args.CheckParm("-nomonsters");

	// Respawn
	sv_monstersrespawn = Args.CheckParm("-respawn");

	// Fast
	sv_fastmonsters = Args.CheckParm("-fast");

    // developer mode
	devparm = Args.CheckParm ("-devparm");

	// Record a vanilla demo
	p = Args.CheckParm ("-record");
	if (p)
	{
		autorecord = true;
		autostart = true;
		demorecordfile = Args.GetArg (p+1);
	}

	// get skill / episode / map from parms
	strcpy (startmap, (gameinfo.flags & GI_MAPxx) ? "MAP01" : "E1M1");

	// Check for -playdemo, play a single demo then quit.
	p = Args.CheckParm ("-playdemo");
	// Hack to check for +playdemo command, since if you just add it normally
	// it won't run because it's attempting to run a demo and still set up the
	// first map as normal.
	if (!p)
		p = Args.CheckParm ("+playdemo");
	if (p && p < Args.NumArgs()-1)
	{
		Printf (PRINT_HIGH, "Playdemo parameter found on command line.\n");
		singledemo = true;
		defdemoname = Args.GetArg (p+1);
	}

	const char *val = Args.CheckValue ("-skill");
	if (val)
	{
		sv_skill.Set (val[0]-'0');
	}

	p = Args.CheckParm ("-warp");
	if (p && p < Args.NumArgs() - (1+(gameinfo.flags & GI_MAPxx ? 0 : 1)))
	{
		int ep, map;

		if (gameinfo.flags & GI_MAPxx)
		{
			ep = 1;
			map = atoi (Args.GetArg(p+1));
		}
		else
		{
			ep = Args.GetArg(p+1)[0]-'0';
			map = Args.GetArg(p+2)[0]-'0';
		}

		strncpy (startmap, CalcMapName (ep, map), 8);
		autostart = true;
	}

	// [RH] Hack to handle +map
	p = Args.CheckParm ("+map");
	if (p && p < Args.NumArgs()-1)
	{
		strncpy (startmap, Args.GetArg (p+1), 8);
		((char *)Args.GetArg (p))[0] = '-';
		autostart = true;
	}
	if (devparm)
		Printf (PRINT_HIGH, "%s", GStrings(D_DEVSTR));        // D_DEVSTR

	// [RH] Now that all text strings are set up,
	// insert them into the level and cluster data.
	G_SetLevelStrings ();

	// [RH] Parse through all loaded mapinfo lumps
	G_ParseMapInfo ();
	
	// [ML] Parse musinfo lump
	G_ParseMusInfo ();

	// [RH] Parse any SNDINFO lumps
	S_ParseSndInfo();

	// Check for -file in shareware
	if (modifiedgame && (gameinfo.flags & GI_SHAREWARE))
		I_Error ("You cannot -file with the shareware version. Register!");

#ifdef WIN32
	const char *sdlv = getenv("SDL_VIDEODRIVER");
	Printf (PRINT_HIGH, "Using %s video driver.\n",sdlv);
#endif

	Printf (PRINT_HIGH, "M_Init: Init miscellaneous info.\n");
	M_Init ();

	Printf (PRINT_HIGH, "R_Init: Init DOOM refresh daemon.\n");
	R_Init ();

	Printf (PRINT_HIGH, "P_Init: Init Playloop state.\n");
	P_InitEffects();	// [ML] Do this here so we don't have to put particle crap in server
	P_Init ();

	Printf (PRINT_HIGH, "S_Init: Setting up sound.\n");
	Printf (PRINT_HIGH, "S_Init: default sfx volume is %g\n", (float)snd_sfxvolume);
	Printf (PRINT_HIGH, "S_Init: default music volume is %g\n", (float)snd_musicvolume);
	S_Init (snd_sfxvolume, snd_musicvolume);

	I_FinishClockCalibration ();

	Printf (PRINT_HIGH, "D_CheckNetGame: Checking network game status.\n");
	D_CheckNetGame ();

	Printf (PRINT_HIGH, "ST_Init: Init status bar.\n");
	ST_Init ();

	// [RH] Initialize items. Still only used for the give command. :-(
	InitItems ();

	// [RH] Lock any cvars that should be locked now that we're
	// about to begin the game.
	cvar_t::EnableNoSet ();

	// [RH] Now that all game subsystems have been initialized,
	// do all commands on the command line other than +set
	C_ExecCmdLineParams (false, false);

	Printf_Bold("\n\35\36\36\36\36 Odamex Client Initialized \36\36\36\36\37\n");
	if(gamestate != GS_CONNECTING)
		Printf(PRINT_HIGH, "Type connect <address> or use the Odamex Launcher to connect to a game.\n");
    Printf(PRINT_HIGH, "\n");

	setmodeneeded = false; // [Fly] we don't need to set a video mode here!
    //gamestate = GS_FULLCONSOLE;

	p = Args.CheckParm("-netplay");
	if (p)
	{
		if (Args.GetArg(p + 1))
		{
			std::string filename = Args.GetArg(p + 1);
			CL_NetDemoPlay(filename);
		}
		else
		{
			Printf(PRINT_HIGH, "No netdemo filename specified.\n");
		}
	}

	// denis - bring back the demos
    if ( gameaction != ga_loadgame )
    {
		if (autostart || netgame || singledemo)
		{
			if (singledemo)
				G_DoPlayDemo();
			else
			{
				if(autostart)
				{
					// single player warp (like in g_level)
					serverside = true;
                    sv_allowexit = "1";
                    sv_freelook = "1";
                    sv_allowjump = "1";
                    sv_allowredscreen = "1";
                    sv_gametype = GM_COOP;

					players.clear();
					players.push_back(player_t());
					players.back().playerstate = PST_REBORN;
					consoleplayer_id = displayplayer_id = players.back().id = 1;
				}

				G_InitNew (startmap);
				if (autorecord)
					if (G_RecordDemo(demorecordfile.c_str()))
						G_BeginRecording();
			}
		}
        else
		{
            if (gamestate != GS_CONNECTING)
                gamestate = GS_HIDECONSOLE;

			C_ToggleConsole();

			D_StartTitle (); // start up intro loop
		}
    }

	// denis - this will run a demo and quit
	p = Args.CheckParm ("+demotest");
	if (p && p < Args.NumArgs()-1)
	{
		demotest = 1;
		defdemoname = Args.GetArg (p+1);
		G_DoPlayDemo();

		while(demoplayback)
		{
			DObject::BeginFrame ();
			G_Ticker();
			DObject::EndFrame ();
			gametic++;
		}
	}
	else
	{
		demotest = 0;
		D_DoomLoop ();		// never returns
	}
}

VERSION_CONTROL (d_main_cpp, "$Id$")





