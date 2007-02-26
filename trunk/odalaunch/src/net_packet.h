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
//	Launcher packet structure file
//  AUTHOR:	Russell Rice (russell at odamex dot net)
//
//-----------------------------------------------------------------------------


#ifndef NETPACKET_H
#define NETPACKET_H

#include <wx/mstream.h>
#include <wx/datstrm.h>

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
};

struct teamplay_t       // Teamplay score structure 
{
    wxInt32     scorelimit;
    wxInt8      blue, red, gold;
    wxInt32     bluescore, redscore, goldscore;
};

struct serverinfo_t     // Server information structure
{
    wxString        name;           // Server name
    wxUint8         numplayers;     // Number of players playing
    wxUint8         maxplayers;     // Maximum number of possible players
    wxString        map;            // Current map
    wxUint8         numpwads;       // Number of PWAD files
    wxString        iwad;           // The main game file
    wxString        iwad_hash;      // IWAD hash
    wxString        *pwads;         // Array of PWAD file names
    wxUint8         gametype;       // Gametype (0 = Coop, 1 = DM)
    wxUint8         gameskill;      // Gameskill
    wxUint8         teamplay;       // Teamplay enabled?
    player_t        *playerinfo;    // Player information array, use numplayers
    wxString        *wad_hashes;    // IWAD and PWAD hashes
    wxUint8         ctf;            // CTF enabled?
    wxString        webaddr;        // Website address of server
    teamplay_t      teamplayinfo;   // Teamplay information if enabled
    wxUint16        version;
    // added on settings for bond
    wxString        emailaddr;
    wxUint16        timelimit;
    wxUint16        timeleft;
    wxUint16        fraglimit;
    
    wxUint8         itemrespawn;
    wxUint8         weaponstay;
    wxUint8         friendlyfire;
    wxUint8         allowexit;
    wxUint8         infiniteammo;
    wxUint8         nomonsters;
    wxUint8         monstersrespawn;
    wxUint8         fastmonsters;
    wxUint8         allowjump;
    wxUint8         allowfreelook;
    wxUint8         waddownload;
    wxUint8         emptyreset;
    wxUint8         cleanmaps;
    wxUint8         fragonexit;
    
};

class ServerBase  // [Russell] - Defines an abstract class for all packets
{
    protected:      
        static BufferedSocket Socket;
        
        // Magic numbers
        wxInt32 challenge;
        wxInt32 response;
           
        // The time in milliseconds a packet was received
        wxInt32 Ping;
       
        wxIPV4address to_addr;
       
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
        
		void SetAddress(wxString Address, wxInt16 Port) { to_addr.Hostname(Address); to_addr.Service(Port); }
//        void SetAddress(wxString AddressAndPort) { Socket.SetAddress(AddressAndPort); }
        
		wxString GetAddress() { return to_addr.IPAddress() << _T(':') << to_addr.Service(); }
		wxInt32 GetPing() { return Ping; }
};

class MasterServer : public ServerBase  // [Russell] - A master server packet
{
    private:
        // Address format structure
        struct addr_t
        {
            wxUint8     ip[4];
            wxUint16    port;
        };

        wxUint16    server_count;   // Number of servers
        addr_t     *addresses;      // Server array
        
    public:
        MasterServer() 
        { 
            challenge = MASTER_CHALLENGE;
            response = MASTER_CHALLENGE;
                        
            server_count = 0;
            
            addresses = NULL; 
        }
        
        virtual ~MasterServer() 
        { 
            if (addresses != NULL) 
            {
                free(addresses); 
                addresses = NULL;
            }
        }
        
		wxInt32 GetServerCount() { return server_count; }
               
        addr_t GetServerAddress(wxInt32 index) 
        {  
            if ((addresses != NULL) && (server_count > 0))
            if ((index >= 0) && (index < server_count))
            {
                return addresses[index];
            }
        }
        
        void GetServerAddress(wxInt32 index, wxString &Address, wxInt16 &Port)
        {
            if ((addresses != NULL) && (server_count > 0))
            if ((index >= 0) && (index < server_count))
            {
                Address.Printf(_T("%d.%d.%d.%d"),addresses[index].ip[0],
                                                         addresses[index].ip[1],
                                                         addresses[index].ip[2],
                                                         addresses[index].ip[3]);
                                                         
                Port = addresses[index].port;
            }
        }
        
        wxInt32 Parse();
};

class Server : public ServerBase  // [Russell] - A single server
{           
   
    public:
       
        serverinfo_t info; // this could be better, but who cares
        
        Server();
        
        virtual  ~Server();
        
        wxInt32 Parse();
};

#endif // NETPACKET_H
