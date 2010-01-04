// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// $Id$
//
// Copyright (C) 2006-2009 by The Odamex Team.
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
//   List control for handling extra server details 
//
//-----------------------------------------------------------------------------

#include "lst_srvdetails.h"

IMPLEMENT_DYNAMIC_CLASS(LstOdaSrvDetails, wxListCtrl)

typedef enum
{
     srvdetails_field_name
    ,srvdetails_field_value
    
    ,max_srvdetails_fields
} srvdetails_fields_t;

void LstOdaSrvDetails::InsertHeader(const wxString &Name, 
                                    const wxColor *NameColor,
                                    const wxColor *NameBGColor)
{
    wxListItem ListItem;
    
    ListItem.SetMask(wxLIST_MASK_TEXT);
    
    // Name Column
    ListItem.SetText(Name);
    ListItem.SetColumn(srvdetails_field_name);
    ListItem.SetId(InsertItem(GetItemCount(), ListItem.GetText()));

    ListItem.SetBackgroundColour(*NameBGColor);
    ListItem.SetTextColour(*NameColor);
    SetItem(ListItem);
}

void LstOdaSrvDetails::InsertLine(const wxString &Name, const wxString &Value)
{
    wxListItem ListItem;
    
    ListItem.SetMask(wxLIST_MASK_TEXT);
    
    // Name Column
    ListItem.SetText(Name);
    ListItem.SetColumn(srvdetails_field_name);
    ListItem.SetId(InsertItem(GetItemCount(), ListItem.GetText()));
    
    if (BGItemAlternator == *wxWHITE)
    {
        BGItemAlternator.Set(wxUint8(245), wxUint8(245), wxUint8(245));
    }
    else
        BGItemAlternator = *wxWHITE;
    
    ListItem.SetBackgroundColour(BGItemAlternator);
    
    SetItem(ListItem);
    
    // Value Column
    ListItem.SetText(Value);    
    ListItem.SetColumn(srvdetails_field_value);
    SetItem(ListItem);
}

void LstOdaSrvDetails::LoadDetailsFromServer(const Server &In)
{   
    DeleteAllItems();
    DeleteAllColumns();
    
    // Begin adding data to the control
    
    // Set the initial background colour
    BGItemAlternator = *wxWHITE;
    
    InsertColumn(srvdetails_field_name, wxT(""), wxLIST_FORMAT_LEFT, 150);
    InsertColumn(srvdetails_field_value, wxT(""), wxLIST_FORMAT_LEFT, 150);
    
    // Version
    InsertLine(wxT("Version"), wxString::Format(wxT("%u.%u.%u"), 
                                In.Info.VersionMajor, 
                                In.Info.VersionMinor, 
                                In.Info.VersionPatch));
    
    
    // Patch (BEX/DEH) files
    InsertLine(wxT(""), wxT(""));                            
    InsertHeader(wxT("BEX/DEH Files"), wxRED, wxWHITE);
    
    if (In.Info.Patches.size() == 0)
    {
        InsertLine(wxT("None"), wxT(""));
    }
    else
    {
        for (size_t i = 0; i < In.Info.Patches.size(); i += 2)
            InsertLine(In.Info.Patches[i], In.Info.Patches[i + 1]);
    }
    
    // Gameplay variables (Cvars, others)
    InsertLine(wxT(""), wxT(""));                            
    InsertHeader(wxT("Game Settings"), wxRED, wxWHITE);
    
    InsertLine(wxT("Time left"), wxString::Format(wxT("%u"), In.Info.TimeLeft));
    
    for (size_t i = 0; i < In.Info.Cvars.size(); ++i)
        InsertLine(In.Info.Cvars[i].Name, In.Info.Cvars[i].Value);
}
