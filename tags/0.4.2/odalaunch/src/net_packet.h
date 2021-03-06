// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id$
//
// Copyright (C) 2006-2008 by The Odamex Team.
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
//	Launcher packet structure file
//  AUTHOR:	Russell Rice (russell at odamex dot net)
//
//-----------------------------------------------------------------------------


#ifndef NET_PACKET_H
#define NET_PACKET_H

#include <wx/mstream.h>
#include <wx/datstrm.h>

#include <vector>

#include "net_io.h"

#define MASTER_CHALLENGE    777123
#define MASTER_RESPONSE     777123
#define SERVER_CHALLENGE    777123
#define SERVER_RESPONSE     5560020

struct player_t         // Player info structure
{
    wxString    name;
    wxInt16     frags;
    wxInt32     ping;
    wxInt8      team;
    wxInt16     killcount;
    wxInt16     deathcount;
    wxUint16    timeingame;
    bool        spectator;
};

struct teamplay_t       // Teamplay score structure 
{
    wxInt32     scorelimit;
    bool        blue, red, gold;
    wxInt32     bluescore, redscore, goldscore;
};

struct serverinfo_t     // Server information structure
{
    wxUint32        response;
    wxString        name;           // Server name
    wxUint8         numplayers;     // Number of players playing
    wxUint8         maxplayers;     // Maximum number of possible players
    wxString        map;            // Current map
    wxUint8         numpwads;       // Number of PWAD files
    wxString        iwad;           // The main game file
    wxString        iwad_hash;      // IWAD hash
    wxString        *pwads;         // Array of PWAD file names
    wxInt8          gametype;       // Gametype (0 = Coop, 1 = DM)
    wxUint8         gameskill;      // Gameskill
    bool            teamplay;       // Teamplay enabled?
    player_t        *playerinfo;    // Player information array, use numplayers
    wxString        *wad_hashes;    // IWAD and PWAD hashes
    bool            ctf;            // CTF enabled?
    wxString        webaddr;        // Website address of server
    teamplay_t      teamplayinfo;   // Teamplay information if enabled
    wxUint16        version;
    // added on settings for bond
    wxString        emailaddr;
    wxUint16        timelimit;
    wxUint16        timeleft;
    wxUint16        fraglimit;
    
    bool         itemrespawn;
    bool         weaponstay;
    bool         friendlyfire;
    bool         allowexit;
    bool         infiniteammo;
    bool         nomonsters;
    bool         monstersrespawn;
    bool         fastmonsters;
    bool         allowjump;
    bool         sv_freelook;
    bool         waddownload;
    bool         emptyreset;
    bool         cleanmaps;
    bool         fragonexit;
    
    wxUint32        spectating;
    wxUint16        maxactiveplayers;
    
    wxUint32        extrainfo;
    bool            passworded;
};

class ServerBase  // [Russell] - Defines an abstract class for all packets
{
    protected:      
        BufferedSocket Socket;
        
        // Magic numbers
        wxUint32 challenge;
        wxUint32 response;
           
        // The time in milliseconds a packet was received
        wxUint32 Ping;
    public:
        // Constructor
        ServerBase() 
        {
            Ping = 0;           
        }
        
        // Destructor
        virtual ~ServerBase()
        {
        }
        
        // Parse a packet, the parameter is the packet
        virtual wxInt32 Parse() { return -1; }
        
        // Query the server
        wxInt32 Query(wxInt32 Timeout);
        
		void SetAddress(const wxString &Address, const wxInt16 &Port) 
		{ 
		    Socket.SetRemoteAddress(Address, Port);
        }
        
		wxString GetAddress() { return Socket.GetRemoteAddress(); }
		wxUint32 GetPing() const { return Ping; }
};

class MasterServer : public ServerBase  // [Russell] - A master server packet
{
    private:
        // Address format structure
        typedef struct
        {
            wxString    ip;
            wxUint16    port;
            bool        custom;
        } addr_t;

        std::vector<addr_t> addresses;
        std::vector<addr_t> masteraddresses;
    public:
        MasterServer() 
        { 
            challenge = MASTER_CHALLENGE;
            response = MASTER_CHALLENGE;
        }
        
        virtual ~MasterServer() 
        { 

        }
        
		size_t GetServerCount() { return addresses.size(); }
                      
        bool GetServerAddress(const size_t &Index, 
                              wxString &Address, 
                              wxUint16 &Port)
        {
            if (Index < addresses.size())
            {
                Address = addresses[Index].ip;
                Port = addresses[Index].port;
                
                return addresses[Index].custom;
            }
            
            return false;
        }
        
        void AddMaster(const wxString &Address, const wxUint16 &Port)
        {
            addr_t Master = { Address, Port, true };
            
            if ((Master.ip != _T("")) && (Master.port != 0))
                masteraddresses.push_back(Master);
        }
        
        void QueryMasters(const wxUint32 &Timeout)
        {           
            DeleteAllNormalServers();
            
            for (size_t i = 0; i < masteraddresses.size(); ++i)
            {
                Socket.SetRemoteAddress(masteraddresses[i].ip, masteraddresses[i].port);
                
                Query(Timeout);
            }
        }
        
        size_t GetMasterCount() { return masteraddresses.size(); }
        
        void DeleteAllNormalServers()
        {
            size_t i = 0;
            
            // don't delete our custom servers!
            while (i < addresses.size())
            {       
                if (addresses[i].custom == false)
                {
                    addresses.erase(addresses.begin() + i);
                    continue;
                }
                
                ++i;
            }            
        }
        
        void AddCustomServer(const wxString &Address, const wxUint16 &Port)
        {
            addr_t cs;
                    
            cs.ip = Address;
            cs.port = Port;
            cs.custom = true;
            
            // Don't add the same address more than once.
            for (wxUint32 i = 0; i < addresses.size(); ++i)
            {
                if (addresses[i].ip == cs.ip && 
                    addresses[i].port == cs.port &&
                    addresses[i].custom == cs.custom)
                {
                    return;
                }
            }
            
            addresses.push_back(cs);
        }
               
        bool DeleteCustomServer(const size_t &Index)
        {
            if (Index < addresses.size())
            {
                if (addresses[Index].custom)
                {
                    addresses.erase(addresses.begin() + Index);
                    
                    return true;
                }
                else
                    return false;
            }
            
            return false;
        }

        void DeleteAllCustomServers()
        {
            size_t i = 0;
            
            while (i < addresses.size())
            {       
                if (DeleteCustomServer(i))
                    continue;
                
                ++i;
            }       
        }
        
        wxInt32 Parse();
};

class Server : public ServerBase  // [Russell] - A single server
{           
   
    public:
       
        serverinfo_t info; // this could be better, but who cares
        
        Server();
        
        void ResetData();
        
        virtual  ~Server();
        
        wxInt32 Parse();
};

#endif // NETPACKET_H
