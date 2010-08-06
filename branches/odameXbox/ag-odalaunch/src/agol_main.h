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
//	User interface
//
// AUTHORS:
//	 Michael Wood (mwoodj at huntsvegas dot org)
//
//-----------------------------------------------------------------------------

#ifndef _AGOL_MAIN_H
#define _AGOL_MAIN_H

#include <vector>

#include "agol_settings.h"
#include "event_handler.h"
#include "oda_thread.h"
#include "net_packet.h"

typedef struct
{
	AG_Box    *buttonbox;
	AG_Button *launch;
	AG_Button *qlaunch;
	AG_Button *refresh;
	AG_Button *refreshall;
	AG_Button *mlist;
	AG_Button *settings;
	AG_Button *about;
	AG_Button *exit;
} ODA_ButtonBox;

typedef struct 
{
	AG_Box       *statbox;
	AG_Statusbar *tooltip;
	AG_Statusbar *mping;
	AG_Statusbar *queried;
	AG_Statusbar *players;
} ODA_Statusbar;

// AGOL_MainWindow - Class for the main window
class AGOL_MainWindow : private ODA_EventRegister, ODA_ThreadBase
{
public:
	AGOL_MainWindow();
	~AGOL_MainWindow();

private:
	// Event Handler Functions
	void OnWindowResize(AG_Event *event);
	void OnOpenSettingsDialog(AG_Event *event);
	void OnCloseSettingsDialog(AG_Event *event);
	void OnExit(AG_Event *event);
	void OnLaunch(AG_Event *event);
	void OnOfflineLaunch(AG_Event *event);
	void OnRefreshSelected(AG_Event *event);
	void OnRefreshAll(AG_Event *event);
	void OnGetMasterList(AG_Event *event);
	void OnGetWAD(AG_Event *event);
	void OnOdaGetConfig(AG_Event *event);
	void OnManualConnect(AG_Event *event);
	void OnCustomServer(AG_Event *event);
	void OnReportBug(AG_Event *event);
	void OnAbout(AG_Event *event);
	void OnMouseOverWidget(AG_Event *event);
	void UpdateServerList(AG_Event *event);
	void UpdatePlayerList(AG_Event*);
	void UpdateServInfoList(AG_Event *event);

	// Interface Interaction Functions
	void UpdateStatusbarTooltip(const char *tip);
	void ClearStatusbarTooltip();
	void UpdateStatusbarMasterPing(uint32_t ping);
	int  GetSelectedServerListIndex();
	int  GetSelectedServerArrayIndex();
	void ClearList(AG_Table *table);
	void CompleteRowSelection(AG_Table *table);

	// Interface Creation Functions
	AG_Menu       *CreateMainMenu(void *parent);
	ODA_ButtonBox *CreateMainButtonBox(void *parent);
	AG_Pane       *CreateMainListPane(void *parent);
	AG_Pane       *CreateBottomListPane(void *parent);
	AG_Table      *CreateServerList(void *parent);
	AG_Table      *CreatePlayerList(void *parent);
	AG_Table      *CreateServInfoList(void *parent);
	ODA_Statusbar *CreateMainStatusbar(void *parent);

	AG_Button     *CreateButton(void *parent, const char *label, const unsigned char *icon,
														int iconsize, EVENT_FUNC_PTR handler);

	// Query Functions
	void *GetMasterList(void *arg);
	void *QueryServer(void *arg);
	int   QuerySingleServer(Server *server);
	void *QueryAllServers(void *arg);
	void *QueryServerThrEntry(void *arg);

	static bool CvarCompare(const Cvar_t &a, const Cvar_t &b);

	// Interface Components
	AG_Window                *MainWindow;
	AG_Menu                  *MainMenu;
	ODA_ButtonBox            *MainButtonBox;
	AG_Pane                  *MainListPane;
    AG_Table                 *ServerList;
    AG_Pane                  *BottomListPane;
	AG_Table                 *PlayerList;
	AG_Table                 *ServInfoList;
	ODA_Statusbar            *MainStatusbar;

	AGOL_Settings            *SettingsDialog;
	EventHandler             *CloseSettingsHandler;

	// Servers
	MasterServer              MServer;
	Server                   *QServer;

	// Threads
	ODA_Thread                MasterThread;
	std::vector<ODA_Thread*>  QServerThread;
};

#endif