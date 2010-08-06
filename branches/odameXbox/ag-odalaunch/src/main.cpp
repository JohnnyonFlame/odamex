// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id$
//
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
//	Main
//
// AUTHORS:
//  Michael Wood (mwoodj at huntsvegas dot org)
//
//-----------------------------------------------------------------------------

#include <iostream>

#include <agar/core.h>
#include <agar/gui.h>

#include "agol_main.h"
#include "net_io.h"

using namespace std;

int main(int argc, char *argv[])
{
	AGOL_MainWindow *mainWindow;
	char            *drivers = NULL;
	char            *optArg;
	int              c;

	/* Initialize Agar-Core. */
	if (AG_InitCore("ag-odalaunch", AG_VERBOSE) == -1) 
	{
		cerr << AG_GetError() << endl;
		return (-1);
	}

	while ((c = AG_Getopt(argc, argv, "?d:f", &optArg, NULL)) != -1) 
	{
		switch (c) 
		{
			case 'd':
				drivers = optArg;
				break;
			case 'f':
				/* Force full screen */
				AG_SetBool(agConfig, "view.full-screen", 1);
				break;
			case '?':
			default:
				cout << agProgName << "[-df] [-d driver] " << endl;
				exit(0);
		}
	}

	if(!drivers || !strstr(drivers, "sdl"))
	{
		/* Initialize Agar-GUI. */
		if (AG_InitGraphics(drivers) == -1) 
		{
			cerr << AG_GetError() << endl;
			return (-1);
		}
		if(agDriverSw)
			AG_ResizeDisplay(640, 480);
	}
	else // Alternative initialization. This will only initialize single-window display.
	{
		if(!strcmp(drivers, "sdlfb"))
		{
			if (AG_InitVideo(640, 480, 32, AG_VIDEO_SDL | AG_VIDEO_RESIZABLE) == -1) 
			{
				cerr << AG_GetError() << endl;
				return (-1);
			}
		}
		else
		{
			if (AG_InitVideo(640, 480, 32, AG_VIDEO_OPENGL_OR_SDL | AG_VIDEO_RESIZABLE) == -1) 
			{
				cerr << AG_GetError() << endl;
				return (-1);
			}
		}
	}

	// Initialize socket API
	BufferedSocket::InitializeSocketAPI();

	// Create the main window
	mainWindow = new AGOL_MainWindow();

	// Set key bindings
	AG_BindGlobalKey(AG_KEY_ESCAPE, AG_KEYMOD_ANY, AG_QuitGUI);
	AG_BindGlobalKey(AG_KEY_F8, AG_KEYMOD_ANY, AG_ViewCapture);

	// Event (main) Loop
	AG_EventLoop();

	delete mainWindow;

	AG_Destroy();

	// Shutdown socket API
	BufferedSocket::ShutdownSocketAPI();

	cout << "Exiting..." << endl;

	return 0;
}