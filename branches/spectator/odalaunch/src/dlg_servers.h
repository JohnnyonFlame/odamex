// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id:$
//
// Copyright (C) 2006-2007 by The Odamex Team.
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
//	Custom Servers dialog
//  AUTHOR:	Russell Rice, John D Corrado
//
//-----------------------------------------------------------------------------


#ifndef DLG_SERVERS_H
#define DLG_SERVERS_H

#include <wx/dialog.h>
#include <wx/intl.h>
#include <wx/settings.h>
#include <wx/stattext.h>
#include <wx/xrc/xmlres.h>
#include <wx/listctrl.h>
#include <wx/fileconf.h>
#include <wx/checkbox.h>
#include <wx/listbox.h>
#include <wx/sizer.h>
#include <wx/textctrl.h>
#include <wx/checklst.h>
#include <wx/textdlg.h>

class dlgServers: public wxDialog
{
	public:

		dlgServers(wxWindow* parent, wxWindowID id = -1);
		virtual ~dlgServers();

	protected:

        void OnCheckServerListClick(wxCommandEvent &event);

        void OnButtonOK(wxCommandEvent &event);
        void OnButtonClose(wxCommandEvent &event);
        
        void OnButtonAddServer(wxCommandEvent &event);
        void OnButtonDelServer(wxCommandEvent &event);
        
        void OnButtonMoveServerUp(wxCommandEvent &event);
        void OnButtonMoveServerDown(wxCommandEvent &event);

/*        
        void OnGetEnvClick(wxCommandEvent &event);
        
        void OnCheckedBox(wxCommandEvent &event);
*/               
        wxFileConfig ConfigInfo;

        wxCheckListBox *SERVER_LIST;
        
        wxTextCtrl *SERVER_IPPORT_BOX;
                
		wxButton *ADD_SERVER_BUTTON;
		wxButton *DEL_SERVER_BUTTON;
        wxButton *UP_SERVER_BUTTON;
        wxButton *DOWN_SERVER_BUTTON;

		wxButton *CLOSE_BUTTON;
		wxButton *OK_BUTTON;

        wxInt32 UserChangedSetting;

	private:

		DECLARE_EVENT_TABLE()
};

#endif
