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
//	User interface
//	AUTHOR:	Russell Rice, John D Corrado
//
//-----------------------------------------------------------------------------


#include "dlg_main.h"
#include "query_thread.h"

#include "md5.h"

#include <wx/settings.h>
#include <wx/menu.h>
#include <wx/statusbr.h>
#include <wx/msgdlg.h>
#include <wx/utils.h>
#include <wx/tipwin.h>
#include <wx/app.h>
#include <wx/imaglist.h>
#include <wx/artprov.h>
#include <wx/iconbndl.h>
#include <wx/aboutdlg.h>
#include <wx/generic/aboutdlgg.h>

// Control ID assignments for events
// application icon

// lists
static wxInt32 Id_LstCtrlServers = XRCID("Id_LstCtrlServers");
static wxInt32 Id_LstCtrlPlayers = XRCID("Id_LstCtrlPlayers");
static wxInt32 Id_LstCtrlServerDetails = XRCID("Id_LstCtrlServerDetails");

static wxInt32 Id_MnuItmLaunch = XRCID("Id_MnuItmLaunch");

static wxInt32 Id_MnuItmGetList = XRCID("Id_MnuItmGetList");

// custom events
DEFINE_EVENT_TYPE(wxEVT_THREAD_MONITOR_SIGNAL)
DEFINE_EVENT_TYPE(wxEVT_THREAD_WORKER_SIGNAL)

// Event handlers
BEGIN_EVENT_TABLE(dlgMain, wxFrame)
	EVT_MENU(wxID_EXIT, dlgMain::OnExit)
	
	// menu item events
    EVT_MENU(XRCID("Id_MnuItmCustomServers"), dlgMain::OnMenuServers)
    EVT_MENU(XRCID("Id_MnuItmManualConnect"), dlgMain::OnManualConnect)

	EVT_MENU(Id_MnuItmLaunch, dlgMain::OnLaunch)
	EVT_MENU(XRCID("Id_MnuItmRunOffline"), dlgMain::OnQuickLaunch)

	EVT_MENU(Id_MnuItmGetList, dlgMain::OnGetList)
	EVT_MENU(XRCID("Id_MnuItmRefreshServer"), dlgMain::OnRefreshServer)
	EVT_MENU(XRCID("Id_MnuItmRefreshAll"), dlgMain::OnRefreshAll)

    EVT_MENU(XRCID("Id_MnuItmDownloadWad"), dlgMain::OnOpenOdaGet)

	EVT_MENU(XRCID("Id_MnuItmSettings"), dlgMain::OnOpenSettingsDialog)

	EVT_MENU(XRCID("Id_MnuItmVisitWebsite"), dlgMain::OnOpenWebsite)
	EVT_MENU(XRCID("Id_MnuItmVisitForum"), dlgMain::OnOpenForum)
	EVT_MENU(XRCID("Id_MnuItmVisitWiki"), dlgMain::OnOpenWiki)
    EVT_MENU(XRCID("Id_MnuItmViewChangelog"), dlgMain::OnOpenChangeLog)
    EVT_MENU(XRCID("Id_MnuItmSubmitBugReport"), dlgMain::OnOpenReportBug)
	EVT_MENU(XRCID("Id_MnuItmAboutOdamex"), dlgMain::OnAbout)
	
	EVT_SHOW(dlgMain::OnShow)
	EVT_CLOSE(dlgMain::OnClose)
	
    // thread events
    EVT_COMMAND(-1, wxEVT_THREAD_MONITOR_SIGNAL, dlgMain::OnMonitorSignal)    
    EVT_COMMAND(-1, wxEVT_THREAD_WORKER_SIGNAL, dlgMain::OnWorkerSignal)  

    // misc events
    EVT_LIST_ITEM_SELECTED(Id_LstCtrlServers, dlgMain::OnServerListClick)
    EVT_LIST_ITEM_ACTIVATED(Id_LstCtrlServers, dlgMain::OnServerListDoubleClick)
END_EVENT_TABLE()

// Main window creation
dlgMain::dlgMain(wxWindow* parent, wxWindowID id)
{
    wxFileConfig ConfigInfo;   
    wxInt32 WindowWidth, WindowHeight;
    wxString Version;

    // Loads the frame from the xml resource file
	wxXmlResource::Get()->LoadFrame(this, parent, wxT("dlgMain")); 
    
    // Sets the title of the application with a version string to boot
    Version = wxString::Format(
        wxT("The Odamex Launcher v%d.%d.%d - PV%d"), 
        VERSIONMAJOR(VERSION), VERSIONMINOR(VERSION), VERSIONPATCH(VERSION), 
        PROTOCOL_VERSION);
    
    SetLabel(Version);
    
    // Sets the window size
    ConfigInfo.Read(wxT("MainWindowWidth"), 
                    &WindowWidth, 
                    GetClientSize().GetWidth());
                    
    ConfigInfo.Read(wxT("MainWindowHeight"), 
                    &WindowHeight, 
                    GetClientSize().GetHeight());
    
    SetClientSize(WindowWidth, WindowHeight);
    
    launchercfg_s.get_list_on_start = 1;
    launchercfg_s.show_blocked_servers = 1;
    launchercfg_s.wad_paths = wxGetCwd();
    launchercfg_s.odamex_directory = wxGetCwd();

    // Set up icons, this is a hack because wxwidgets does not have an xml
    // handler for wxIconBundle :(
    wxIconBundle IconBundle;
    
    IconBundle.AddIcon(wxXmlResource::Get()->LoadIcon(wxT("icon16x16x32")));
    IconBundle.AddIcon(wxXmlResource::Get()->LoadIcon(wxT("icon32x32x32")));
    IconBundle.AddIcon(wxXmlResource::Get()->LoadIcon(wxT("icon48x48x32")));
    IconBundle.AddIcon(wxXmlResource::Get()->LoadIcon(wxT("icon16x16x8")));
    IconBundle.AddIcon(wxXmlResource::Get()->LoadIcon(wxT("icon32x32x8")));
    
    SetIcons(IconBundle);
    
    m_LstCtrlServers = wxDynamicCast(FindWindow(Id_LstCtrlServers), LstOdaServerList);
    m_LstCtrlPlayers = wxDynamicCast(FindWindow(Id_LstCtrlPlayers), LstOdaPlayerList);
    m_LstOdaSrvDetails = wxDynamicCast(FindWindow(Id_LstCtrlServerDetails), LstOdaSrvDetails);

    m_LstCtrlServers->SetupServerListColumns();
    m_LstCtrlPlayers->SetupPlayerListColumns();

	// set up the master server information
	MServer = new MasterServer;
    
    MServer->AddMaster(_T("master1.odamex.net"), 15000);
    MServer->AddMaster(_T("voxelsoft.com"), 15000);
    
    /* Init sub dialogs and load settings */
    config_dlg = new dlgConfig(&launchercfg_s, this);
    server_dlg = new dlgServers(MServer, this);
    
    /* Get the first directory for wad downloading */
    wxInt32 Pos = launchercfg_s.wad_paths.Find(wxT(PATH_DELIMITER), false);
    wxString FirstDirectory = launchercfg_s.wad_paths.Mid(0, Pos);
    
    OdaGet = new frmOdaGet(this, -1, FirstDirectory);
    
    QServer = NULL;

    // get master list on application start
    if (launchercfg_s.get_list_on_start)
    {
        wxCommandEvent event(wxEVT_COMMAND_TOOL_CLICKED, Id_MnuItmGetList);
    
        wxPostEvent(this, event);
    }
}

// Window Destructor
dlgMain::~dlgMain()
{
    // Cleanup
    delete MServer;
    
    MServer = NULL;
    
    delete[] QServer;
    
    QServer = NULL;
    
    if (config_dlg != NULL)
        config_dlg->Destroy();

    if (server_dlg != NULL)
        server_dlg->Destroy();

    if (OdaGet != NULL)
        OdaGet->Destroy();
}

// Called when the menu exit item or exit button is clicked
void dlgMain::OnExit(wxCommandEvent& event)
{
    Close();
}

// Called when the window X button or Close(); function is called
void dlgMain::OnClose(wxCloseEvent &event)
{
    if (GetThread() && GetThread()->IsRunning())
        GetThread()->Wait();

    // Save GUI layout
    wxFileConfig ConfigInfo;

    ConfigInfo.Write(wxT("MainWindowWidth"), GetClientSize().GetWidth());
    ConfigInfo.Write(wxT("MainWindowHeight"), GetClientSize().GetHeight());
    
    event.Skip();
}

// Called when the window is shown
void dlgMain::OnShow(wxShowEvent &event)
{

}

// manually connect to a server
void dlgMain::OnManualConnect(wxCommandEvent &event)
{
    wxString ted_result = _T("");
    wxTextEntryDialog ted(this, 
                            _T("Please enter IP Address and Port"), 
                            _T("Please enter IP Address and Port"),
                            _T("0.0.0.0:0"));
                            
    // show it
    ted.ShowModal();
    ted_result = ted.GetValue();
    
    wxString ped_result = _T("");
    wxPasswordEntryDialog ped(this, 
                            _T("Enter an optional password"), 
                            _T("Enter an optional password"),
                            _T(""));
                            
    ped.ShowModal();
    
    if (!ted_result.IsEmpty() && ted_result != _T("0.0.0.0:0"))
        LaunchGame(ted_result, 
                    launchercfg_s.odamex_directory, 
                    launchercfg_s.wad_paths,
                    ped.GetValue());
}


// Posts a thread message to the main thread
void dlgMain::ThreadPostEvent(wxEventType EventType, int win_id, mtrs_t Signal, 
    wxInt32 Index, wxInt32 ListIndex)
{
    static wxCommandEvent event(EventType, win_id);
    
    mtrs_struct_t *Result = new mtrs_struct_t;
    
    Result->Signal = Signal;                
    Result->Index = Index;
    Result->ServerListIndex = ListIndex;
    
    event.SetClientData(Result);
    
    wxPostEvent(this, event);
}

// [Russell] - Monitor thread entry point
void *dlgMain::Entry()
{
    wxFileConfig ConfigInfo;
    wxInt32 MasterTimeout, ServerTimeout;
    
    // Can I order a master server request with a list of server addresses
    // to go with that kthx?
    if (mtcs_Request.Signal == mtcs_getmaster)
    {           
        mtcs_Request.Signal = mtcs_getservers;
            
        ConfigInfo.Read(wxT(MASTERTIMEOUT), &MasterTimeout, 500);
            
        MServer->QueryMasters(MasterTimeout);
            
        if (!MServer->GetServerCount())
        {          
            ThreadPostEvent(wxEVT_THREAD_MONITOR_SIGNAL, -1, 
                mtrs_master_timeout, -1, -1);
        }
        else
        {                 
            ThreadPostEvent(wxEVT_THREAD_MONITOR_SIGNAL, -1, 
                mtrs_master_success, -1, -1);             
        }

        if (QServer != NULL && MServer->GetServerCount())
        {
            delete[] QServer;
            
            QServer = new Server [MServer->GetServerCount()];
        }
        else
            QServer = new Server [MServer->GetServerCount()];

    }
    
    // get a new list of servers
    if (mtcs_Request.Signal == mtcs_getservers)
    {
        size_t count = 0;
        size_t serverNum = 0;
        wxString Address = _T("");
        wxUint16 Port = 0;
   
        mtcs_Request.Signal = mtcs_none;

        // [Russell] - This includes custom servers.
        if (!MServer->GetServerCount())
        {
            ThreadPostEvent(wxEVT_THREAD_MONITOR_SIGNAL, -1, 
                mtrs_server_noservers, -1, -1);               
        }

        ConfigInfo.Read(wxT(SERVERTIMEOUT), &ServerTimeout, 500);
    
        /* 
            Thread pool manager:
            Executes a number of threads that contain the same amount of
            servers, when a thread finishes, it gets deleted and another
            gets executed with a different server, eventually all the way
            down to 0 servers.
        */
        while(count < MServer->GetServerCount())
        {
            for(size_t i = 0; i < NUM_THREADS; i++)
            {
                if((threadVector.size() != 0) && ((threadVector.size() - 1) >= i))
                {
                    // monitor our thread vector, delete ONLY if the thread is
                    // finished
                    if(threadVector[i]->IsRunning())
                        continue;
                    else
                    {
                        threadVector[i]->Wait();
                        delete threadVector[i];
                        threadVector.erase(threadVector.begin() + i);
                        count++;
                    }
                }
                if(serverNum < MServer->GetServerCount())
                {
                    MServer->GetServerAddress(serverNum, Address, Port);
                    QServer[serverNum].SetAddress(Address, Port);

                    // add the thread to the vector
                    threadVector.push_back(new QueryThread(this, &QServer[serverNum], serverNum, ServerTimeout));

                    // create and run the thread
                    if(threadVector[threadVector.size() - 1]->Create() == wxTHREAD_NO_ERROR)
                        threadVector[threadVector.size() - 1]->Run();

                    // DUMB: our next server will be this incremented value
                    serverNum++;
                }          
            }
        }

        ThreadPostEvent(wxEVT_THREAD_MONITOR_SIGNAL, -1, 
            mtrs_servers_querydone, -1, -1);              
    }
        
    // User requested single server to be refreshed
    if (mtcs_Request.Signal == mtcs_getsingleserver)
    {            
        mtcs_Request.Signal = mtcs_none;

        ConfigInfo.Read(wxT(SERVERTIMEOUT), &ServerTimeout, 500);

        if (MServer->GetServerCount())
        if (QServer[mtcs_Request.Index].Query(ServerTimeout))
        {
            ThreadPostEvent(wxEVT_THREAD_MONITOR_SIGNAL, -1, 
                mtrs_server_singlesuccess, mtcs_Request.Index, 
                mtcs_Request.ServerListIndex);     
        }
        else
        {
            ThreadPostEvent(wxEVT_THREAD_MONITOR_SIGNAL, 
                mtrs_server_singletimeout, mtrs_server_singletimeout, 
                mtcs_Request.Index, mtcs_Request.ServerListIndex);
        }                     
    }
    
    return NULL;
}

void dlgMain::OnMonitorSignal(wxCommandEvent& event)
{
    mtrs_struct_t *Result = (mtrs_struct_t *)event.GetClientData();
    wxInt32 i;
    
    switch (Result->Signal)
    {
        case mtrs_master_timeout:
        {
            // We use multiple masters you see, if one fails and the others are
            // working, atleast we can get some useful data
            if (!MServer->GetServerCount())
            {
                wxMessageBox(wxT("No master servers could be contacted"), 
                    wxT("Error"), wxOK | wxICON_ERROR);
                
                break;
            }
        }
        
        case mtrs_master_success:
            break;
        case mtrs_server_noservers:
        {
            wxMessageBox(wxT("There are no servers to query"), 
                wxT("Error"), wxOK | wxICON_ERROR);
        }
        break;
        
        case mtrs_server_singletimeout:
        {
            i = FindServerInList(QServer[Result->Index].GetAddress());

            m_LstOdaSrvDetails->LoadDetailsFromServer(NullServer);
            
            QServer[Result->Index].ResetData();
            
            if (launchercfg_s.show_blocked_servers)
            if (i == -1)
                m_LstCtrlServers->AddServerToList(QServer[Result->Index], Result->Index);
            else
                m_LstCtrlServers->AddServerToList(QServer[Result->Index], i, false);
        }
        break;
        
        case mtrs_server_singlesuccess:           
        {
            m_LstCtrlServers->AddServerToList(QServer[Result->Index], Result->ServerListIndex, false);
            
            m_LstCtrlPlayers->AddPlayersToList(QServer[Result->Index]);
            
            m_LstOdaSrvDetails->LoadDetailsFromServer(QServer[Result->Index]);
            
            TotalPlayers += QServer[Result->Index].Info.Players.size();
        }
        break;

        case mtrs_servers_querydone:
        {            
            // Sort server list after everything has been queried
            m_LstCtrlServers->Sort();
        }
        break;
        
        default:
            break;
    }

    GetStatusBar()->SetStatusText(wxString::Format(_T("Master Ping: %u"), MServer->GetPing()), 1);
    GetStatusBar()->SetStatusText(wxString::Format(_T("Total Players: %d"), TotalPlayers), 3);

    delete Result;
}

// worker threads post to this callback
void dlgMain::OnWorkerSignal(wxCommandEvent& event)
{
    wxInt32 i;
    switch (event.GetId())
    {
        case 0: // server query timed out
        {
            i = FindServerInList(QServer[event.GetInt()].GetAddress());

            m_LstCtrlPlayers->DeleteAllItems();
            
            QServer[event.GetInt()].ResetData();
            
            if (launchercfg_s.show_blocked_servers)
            if (i == -1)
                m_LstCtrlServers->AddServerToList(QServer[event.GetInt()], event.GetInt());
            else
                m_LstCtrlServers->AddServerToList(QServer[event.GetInt()], i, false);
            
            break;                 
        }
        case 1: // server queried successfully
        {
            m_LstCtrlServers->AddServerToList(QServer[event.GetInt()], event.GetInt());
            
            TotalPlayers += QServer[event.GetInt()].Info.Players.size();
            
            break;      
        }
    }

    ++QueriedServers;
    
    GetStatusBar()->SetStatusText(wxString::Format(_T("Queried Server %d of %d"), 
                                                   QueriedServers, 
                                                   MServer->GetServerCount()), 
                                                   2);
                                                   
    GetStatusBar()->SetStatusText(wxString::Format(_T("Total Players: %d"), 
                                                   TotalPlayers), 
                                                   3);   
}

// Custom Servers menu item
void dlgMain::OnMenuServers(wxCommandEvent &event)
{
    if (server_dlg)
        server_dlg->Show();
}


void dlgMain::OnOpenSettingsDialog(wxCommandEvent &event)
{
    if (config_dlg)
        config_dlg->Show();
}

void dlgMain::OnOpenOdaGet(wxCommandEvent &event)
{
    if (OdaGet)
        OdaGet->Show();
}

// Quick-Launch button click
void dlgMain::OnQuickLaunch(wxCommandEvent &event)
{
	LaunchGame(_T(""), 
				launchercfg_s.odamex_directory, 
				launchercfg_s.wad_paths);
}

// Launch button click
void dlgMain::OnLaunch(wxCommandEvent &event)
{
    wxString Password;
    wxInt32 i;
    
    i = GetSelectedServerArrayIndex();
    
    if (i == -1)
        return;

    // If the server is passworded, pop up a password entry dialog for them to
    // specify one before going any further
    if (QServer[i].Info.PasswordHash.IsEmpty() == false)
    {                           
        wxPasswordEntryDialog ped(this, wxT("Please enter a password"),
            wxT("This server is passworded"), wxT(""));
        
        while (1)
        {          
            // Show the dialog box and get the resulting value
            ped.ShowModal();
        
            Password = ped.GetValue();
        
            // User possibly hit cancel or did not enter anything, just exit
            if (Password.IsEmpty())
                return;
            
            // Do an MD5 comparison of the password with the servers one, if it
            // fails, keep asking the user to enter a valid password, otherwise 
            // dive out and connect to the server
            if (QServer[i].Info.PasswordHash != MD5SUM(Password.c_str()))
            {
                wxMessageDialog Message(this, wxT("Incorrect password"), 
                    wxT("Incorrect password"), wxOK | wxICON_HAND);
                
                Message.ShowModal();
                
                // Reset the text so weird things don't happen
                ped.SetValue("");
            }
            else
                break;
        }
    }
    
    LaunchGame(QServer[i].GetAddress(), launchercfg_s.odamex_directory,
        launchercfg_s.wad_paths, Password);
}

// Get Master List button click
void dlgMain::OnGetList(wxCommandEvent &event)
{
    if (GetThread() && GetThread()->IsRunning())
        return;

    // Create monitor thread
    if (this->wxThreadHelper::Create() != wxTHREAD_NO_ERROR)
    {
        wxMessageBox(_T("Could not create monitor thread!"), 
                     _T("Error"), 
                     wxOK | wxICON_ERROR);
                     
        wxExit();
    }

    m_LstCtrlServers->DeleteAllItems();
    m_LstCtrlPlayers->DeleteAllItems();
        
    QueriedServers = 0;
    TotalPlayers = 0;
	
	mtcs_Request.Signal = mtcs_getmaster;

    GetThread()->Run();    
}

void dlgMain::OnRefreshServer(wxCommandEvent &event)
{   
    wxInt32 li, ai;

    if (GetThread() && GetThread()->IsRunning())
        return;

    // Create monitor thread
    if (this->wxThreadHelper::Create() != wxTHREAD_NO_ERROR)
    {
        wxMessageBox(_T("Could not create monitor thread!"), 
                     _T("Error"), 
                     wxOK | wxICON_ERROR);
                     
        wxExit();
    }

    li = GetSelectedServerListIndex();    
    ai = GetSelectedServerArrayIndex();
    
    if (li == -1 || ai == -1)
        return;
    
    m_LstCtrlPlayers->DeleteAllItems();
                
    TotalPlayers -= QServer[ai].Info.Players.size();
    
    mtcs_Request.Signal = mtcs_getsingleserver;
    mtcs_Request.ServerListIndex = li;
    mtcs_Request.Index = ai;

    GetThread()->Run();   
}

void dlgMain::OnRefreshAll(wxCommandEvent &event)
{
    if (GetThread() && GetThread()->IsRunning())
        return;

    if (!MServer->GetServerCount())
        return;

    // Create monitor thread
    if (this->wxThreadHelper::Create() != wxTHREAD_NO_ERROR)
    {
        wxMessageBox(_T("Could not create monitor thread!"), 
                     _T("Error"), 
                     wxOK | wxICON_ERROR);
                     
        wxExit();
    }
       
    m_LstCtrlServers->DeleteAllItems();
    m_LstCtrlPlayers->DeleteAllItems();
    
    QueriedServers = 0;
    TotalPlayers = 0;
    
    mtcs_Request.Signal = mtcs_getservers;
    mtcs_Request.ServerListIndex = -1;
    mtcs_Request.Index = -1;

    GetThread()->Run();   
}

// when the user clicks on the server list
void dlgMain::OnServerListClick(wxListEvent& event)
{   
    wxInt32 i;
    
    i = GetSelectedServerArrayIndex();
    
    if (i == -1)
        return;
    
    m_LstCtrlPlayers->DeleteAllItems();
        
    m_LstCtrlPlayers->AddPlayersToList(QServer[i]);
            
    if (QServer[i].GotResponse() == false)
        m_LstOdaSrvDetails->LoadDetailsFromServer(NullServer);
    else
        m_LstOdaSrvDetails->LoadDetailsFromServer(QServer[i]);
}

void dlgMain::LaunchGame(const wxString &Address, const wxString &ODX_Path, 
    const wxString &waddirs, const wxString &Password)
{
    wxFileConfig ConfigInfo;
    wxString ExtraCmdLineArgs;
    
    if (ODX_Path.IsEmpty())
    {
        wxMessageBox(wxT("Your Odamex path is empty!"));
        
        return;
    }
    
    #ifdef __WXMSW__
      wxString binname = ODX_Path + wxT('\\') + _T("odamex");
    #elif __WXMAC__
      wxString binname = ODX_Path + wxT("/odamex.app/Contents/MacOS/odamex");
    #else
      wxString binname = ODX_Path + wxT("/odamex");
    #endif

    wxString cmdline = wxT("");

    wxString dirs = waddirs.Mid(0, waddirs.Length());
    
    cmdline += wxString::Format(wxT("%s"), binname.c_str());
    
    if (!Address.IsEmpty())
		cmdline += wxString::Format(wxT(" -connect %s"),
									Address.c_str());
	
	if (!Password.IsEmpty())
        cmdline += wxString::Format(wxT(" %s"),
									Password.c_str());
	
	// this is so the client won't mess up parsing
	if (!dirs.IsEmpty())
        cmdline += wxString::Format(wxT(" -waddir \"%s\""), 
                                    dirs.c_str());

    // Check for any user command line arguments
    ConfigInfo.Read(wxT(EXTRACMDLINEARGS), &ExtraCmdLineArgs, wxT(""));
    
    if (!ExtraCmdLineArgs.IsEmpty())
        cmdline += wxString::Format(_T(" %s"), 
                                    ExtraCmdLineArgs.c_str());

	if (wxExecute(cmdline, wxEXEC_ASYNC, NULL) == -1)
        wxMessageBox(wxString::Format(wxT("Could not start %s!"), 
                                        binname.c_str()));
	
}


// when the user double clicks on the server list
void dlgMain::OnServerListDoubleClick(wxListEvent& event)
{
    wxCommandEvent LaunchEvent(wxEVT_COMMAND_TOOL_CLICKED, Id_MnuItmLaunch);
    
    wxPostEvent(this, LaunchEvent);
}

// returns a index of the server address as the internal array index
wxInt32 dlgMain::FindServer(wxString Address)
{
    for (size_t i = 0; i < MServer->GetServerCount(); i++)
        if (QServer[i].GetAddress().IsSameAs(Address))
            return i;
    
    return -1;
}

// Finds an index in the server list, via Address
wxInt32 dlgMain::FindServerInList(wxString Address)
{
    if (!m_LstCtrlServers->GetItemCount())
        return -1;
    
    for (wxInt32 i = 0; i < m_LstCtrlServers->GetItemCount(); i++)
    {
        wxListItem item;
        item.SetId(i);
        item.SetColumn(7);
        item.SetMask(wxLIST_MASK_TEXT);
        
        m_LstCtrlServers->GetItem(item);
        
        if (item.GetText().IsSameAs(Address))
            return i;
    }
    
    return -1;
}

// Retrieves the currently selected server in list index form
wxInt32 dlgMain::GetSelectedServerListIndex()
{
    wxInt32 i;

    if (!m_LstCtrlServers->GetItemCount() || 
        !m_LstCtrlServers->GetSelectedItemCount())
    {
        return -1;
    }
        
    i = m_LstCtrlServers->GetNextItem(-1, wxLIST_NEXT_ALL, 
        wxLIST_STATE_SELECTED);
        
    return i;
}

// Retrieves the currently selected server in array index form
wxInt32 dlgMain::GetSelectedServerArrayIndex()
{
    wxListItem item;
    wxInt32 i;

    i = GetSelectedServerListIndex();

    item.SetId(i);
    item.SetColumn(7);
    item.SetMask(wxLIST_MASK_TEXT);
        
    m_LstCtrlServers->GetItem(item);
        
    i = FindServer(item.GetText()); 
    
    return i;
}

// About information
void dlgMain::OnAbout(wxCommandEvent& event)
{
    wxAboutDialogInfo AboutDialogInfo;

    AboutDialogInfo.SetName(GetLabel());
    
    AboutDialogInfo.AddDeveloper(wxT("The Odamex Team"));
    AboutDialogInfo.SetWebSite(wxT("http://www.odamex.net"));
        
    wxAboutBox(AboutDialogInfo);
}

void dlgMain::OnOpenWebsite(wxCommandEvent &event)
{
    wxLaunchDefaultBrowser(_T("http://odamex.net"));
}

void dlgMain::OnOpenForum(wxCommandEvent &event)
{
    wxLaunchDefaultBrowser(_T("http://odamex.net/boards"));
}

void dlgMain::OnOpenWiki(wxCommandEvent &event)
{
    wxLaunchDefaultBrowser(_T("http://odamex.net/wiki"));
}

void dlgMain::OnOpenChangeLog(wxCommandEvent &event)
{
    wxLaunchDefaultBrowser(_T("http://odamex.net/changelog"));
}

void dlgMain::OnOpenReportBug(wxCommandEvent &event)
{
    wxLaunchDefaultBrowser(_T("http://odamex.net/bugs/enter_bug.cgi"));
}
