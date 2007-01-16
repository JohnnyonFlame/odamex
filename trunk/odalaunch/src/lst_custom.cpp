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
//	Custom list control, featuring sorting
//	AUTHOR:	Russell Rice
//
//-----------------------------------------------------------------------------


#include "lst_custom.h"

IMPLEMENT_DYNAMIC_CLASS(wxAdvancedListCtrl, wxListCtrl)

BEGIN_EVENT_TABLE(wxAdvancedListCtrl, wxListCtrl)
     EVT_LIST_COL_CLICK(-1, wxAdvancedListCtrl::OnHeaderColumnButtonClick)
END_EVENT_TABLE()

struct sortinfo_t
{
    wxInt32 SortOrder;
    wxInt32 SortCol;
    wxAdvancedListCtrl *ctrl;
};

int wxCALLBACK SortRoutine(long item1, long item2, long sortData) 
{    
    sortinfo_t *sortinfo = (sortinfo_t *)sortData;
    
    wxListItem lstitem1, lstitem2;

    lstitem1.SetId(item1);
    lstitem1.SetColumn(sortinfo->SortCol);
    lstitem1.SetMask(wxLIST_MASK_TEXT);
    sortinfo->ctrl->GetItem(lstitem1);

    lstitem2.SetId(item2);
    lstitem2.SetColumn(sortinfo->SortCol);
    lstitem2.SetMask(wxLIST_MASK_TEXT);
    sortinfo->ctrl->GetItem(lstitem2);
    
    wxString item1str = lstitem1.GetText();
    wxString item2str = lstitem2.GetText();
    
    if (sortinfo->SortOrder == 1) 
        return item1str.CmpNoCase(item2str);
    
    return item2str.CmpNoCase(item1str);
}

void wxAdvancedListCtrl::OnHeaderColumnButtonClick(wxListEvent &event)
{
    // invert sort order if need be (ascending/descending)
    if (SortCol != event.GetColumn())
        SortOrder = 1;
    else
        SortOrder = !SortOrder;
    
    // column that needs to be sorted, so the rest of the list
    // can be sorted by it
    SortCol = event.GetColumn();
    
    // prime 'er up
    long item = this->GetNextItem(-1);
      
    while(item != -1) 
    {                    
        this->SetItemData(item, item); 
 
        item = this->GetNextItem(item);
    }

    // information for sorting routine callback
    sortinfo_t sortinfo;
    
    sortinfo.ctrl = (wxAdvancedListCtrl *)event.GetEventObject();
    sortinfo.SortCol = SortCol;
    sortinfo.SortOrder = SortOrder;
    
    // sort the list by column
    this->SortItems(SortRoutine, (long)&sortinfo);

    // recolour the list
    ColourList();
}

void wxAdvancedListCtrl::ColourListItem(wxInt32 item)
{
    // reset colours back
    wxColour col;
     
    // light grey coolness
    if ((item % 2) == 0)
        col.Set(colRed, colGreen, colBlue);
    else
        col.Set(255, 255, 255);

    // apply to index.
    this->SetItemBackgroundColour(item, col); 
}

void wxAdvancedListCtrl::ColourList()
{
    // reset colours back
    wxColour col;
    
    // iterate through, changing background colour for each row
    for (wxInt32 i = 0; i < this->GetItemCount(); i++)
    {
        // light grey coolness
        if ((i % 2) == 0)
            col.Set(colRed, colGreen, colBlue);
        else
            col.Set(255, 255, 255);

        // apply to index.
        this->SetItemBackgroundColour(i, col); 
    }    
}

// get an index location of the text field in the list
wxInt32 wxAdvancedListCtrl::GetIndex(wxString str)
{
	wxInt32 i = 0, list_index = -1;
	wxString addr = _T("");

    wxListItem listitem;

    if (!(str.IsEmpty()) && (this->GetItemCount() > 0))
	for (i = 0; i < this->GetItemCount(); i++)
	{
        listitem.SetId(i);
        listitem.SetColumn(7);
        listitem.SetMask(wxLIST_MASK_TEXT);

        this->GetItem(listitem);
        
        if (listitem.GetText().IsSameAs(str))
        {
            list_index = i;
            break;
        }
    }
	
	return list_index;
}
