//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// protocol.h -- communications protocols
#ifndef PROTOCOL_H
#define PROTOCOL_H

#ifdef _WIN32
#pragma once
#endif


#define INSTANCE_BASELINE_TABLENAME	"instancebaseline"
#define LIGHT_STYLES_TABLENAME		"lightstyles"
#define USER_INFO_TABLENAME			"userinfo"
#define SERVER_STARTUP_DATA_TABLENAME	"server_query_info"	// the name is a remnant...
#define DYNAMIC_MODEL_TABLENAME		"dynamicmodel"


//#define CURRENT_PROTOCOL    1


#define DELTA_OFFSET_BITS 5
#define DELTA_OFFSET_MAX ( ( 1 << DELTA_OFFSET_BITS ) - 1 )

#define DELTASIZE_BITS		20	// must be: 2^DELTASIZE_BITS > (NET_MAX_PAYLOAD * 8)

// Largest # of commands to send in a packet
#define NUM_NEW_COMMAND_BITS		4
#define MAX_NEW_COMMANDS			((1 << NUM_NEW_COMMAND_BITS)-1)

// Max number of history commands to send ( 2 by default ) in case of dropped packets
#define NUM_BACKUP_COMMAND_BITS		3
#define MAX_BACKUP_COMMANDS			((1 << NUM_BACKUP_COMMAND_BITS)-1)


#define PROTOCOL_AUTHCERTIFICATE 0x01   // Connection from client is using a WON authenticated certificate
#define PROTOCOL_HASHEDCDKEY     0x02	// Connection from client is using hashed CD key because WON comm. channel was unreachable
#define PROTOCOL_STEAM			 0x03	// Steam certificates
#define PROTOCOL_LASTVALID       0x03    // Last valid protocol

#define CONNECTIONLESS_HEADER			0xFFFFFFFF	// all OOB packet start with this sequence
#define STEAM_KEYSIZE				2048  // max size needed to contain a steam authentication key (both server and client)

// each channel packet has 1 byte of FLAG bits
#define PACKET_FLAG_RELIABLE			(1<<0)	// packet contains subchannel stream data
#define PACKET_FLAG_COMPRESSED			(1<<1)	// packet is compressed
#define PACKET_FLAG_ENCRYPTED			(1<<2)  // packet is encrypted
#define PACKET_FLAG_SPLIT				(1<<3)  // packet is split
#define PACKET_FLAG_CHOKED				(1<<4)  // packet was choked by sender

// NOTE:  Bits 5, 6, and 7 are used to specify the # of padding bits at the end of the packet!!!
#define ENCODE_PAD_BITS( x ) ( ( x << 5 ) & 0xff )
#define DECODE_PAD_BITS( x ) ( ( x >> 5 ) & 0xff )

//
// client to server
//

#define RES_FATALIFMISSING	(1<<0)   // Disconnect if we can't get this file.
#define RES_PRELOAD			(1<<1)  // Load on client rather than just reserving name

// Some day we may want to integrate Zoid's CL 1295766 - Rewrite of Source networking to be protobuf based.

#endif // PROTOCOL_H


