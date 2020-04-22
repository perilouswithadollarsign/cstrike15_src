//====== Copyright © 1996-2004, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#ifndef DMXHEADER_H
#define DMXHEADER_H
#ifdef _WIN32
#pragma once
#endif

//-----------------------------------------------------------------------------
// data file format info
//-----------------------------------------------------------------------------
#define DMX_LEGACY_VERSION_STARTING_TOKEN "<!-- DMXVersion"
#define DMX_LEGACY_VERSION_ENDING_TOKEN "-->"

#define DMX_VERSION_STARTING_TOKEN "<!-- dmx"
#define DMX_VERSION_ENDING_TOKEN "-->"

#define GENERIC_DMX_FORMAT "dmx"


enum
{
	DMX_MAX_FORMAT_NAME_MAX_LENGTH = 64,
	DMX_MAX_HEADER_LENGTH = 40 + 2 * DMX_MAX_FORMAT_NAME_MAX_LENGTH,
};

struct DmxHeader_t
{
	char encodingName[ DMX_MAX_FORMAT_NAME_MAX_LENGTH ];
	int nEncodingVersion;
	char formatName[ DMX_MAX_FORMAT_NAME_MAX_LENGTH ];
	int nFormatVersion;

	DmxHeader_t() : nEncodingVersion( -1 ), nFormatVersion( -1 )
	{
		encodingName[ 0 ] = formatName[ 0 ] = '\0';
	}
};

//-----------------------------------------------------------------------------
// file id - also used to refer to elements that don't have file associations
//-----------------------------------------------------------------------------
enum DmFileId_t
{
	DMFILEID_INVALID = 0xffffffff
};

#endif // DMXHEADER_H
