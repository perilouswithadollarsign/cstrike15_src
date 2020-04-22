//========= Copyright © 1996-2007, Valve LLC, All rights reserved. ============
//
// Purpose: XBox VXConsole Common. Used for public remote access items.
//
//=============================================================================
#pragma once

// sent during connection, used to explicitly guarantee a binary compatibility
#define VXCONSOLE_PROTOCOL_VERSION	300

typedef struct
{
	char		labelString[128];
	COLORREF	color;
} xrProfile_t;

typedef struct
{
	char		messageString[256];
	float		time;
	float		deltaTime;
	size_t		memory;
	int			deltaMemory;
} xrTimeStamp_t;

typedef struct
{
	char		nameString[256];
	char		shaderString[256];
	int			refCount;
} xrMaterial_t;

enum cacheableState_e
{
	CS_STATIC = 0,	// texture is not lru managed
	CS_EVICTED,		// texture mip0 is not in memory
	CS_LOADING,		// texture mip0 is loading
	CS_VALID,		// texture mip0 is valid for rendering
	CS_MAX
};

typedef struct
{
	char		nameString[256];
	char		groupString[64];
	char		formatString[64];
	int			size;
	int			width;
	int			height;
	int			depth;
	int			numLevels;
	int			binds;
	int			refCount;
	int			sRGB;
	int			edram;
	int			procedural;
	int			cacheableState;
	int			cacheableSize : 31; // Packing avoids game-to-vxconsole protocol version conflict
	int			final : 1;
	int			failed;
	int			pwl;
	int			reduced;
} xrTexture_t;

typedef struct
{
	char		nameString[256];
	char		formatString[64];
	int			rate;
	int			bits;
	int			channels;
	int			looped;
	int			dataSize;
	int			numSamples;
	int			streamed;
	int			quality;
} xrSound_t;

typedef struct
{
	char		nameString[128];
	char		helpString[256];	
} xrCommand_t;

typedef struct
{
	float	position[3];
	float	angle[3];
	char	mapPath[256];
	char	savePath[256];
	int		build;
	int		skill;
	char	details[1024];
} xrMapInfo_t;

typedef struct
{
	int		BSPSize;
} xrBudgetInfo_t;

struct xrModel_t
{
	char		nameString[256];
	int			dataSize;
	int			numVertices;
	int			triCount;
	int			dataSizeLod0;
	int			numVerticesLod0;
	int			triCountLod0;
	int			numBones;
	int			numParts;
	int			numLODs;
	int			numMeshes;
};

struct xrDataCacheItem_t
{
	char			nameString[256];
	char			sectionString[64];
	int				size;
	int				lockCount;
	unsigned int	clientId;
	unsigned int	itemData;
	unsigned int	handle;	
};

struct xrVProfNodeItem_t
{
	char			nameString[128];
	char			budgetGroupString[128];
	unsigned int	budgetGroupColor;
	unsigned int	totalCalls;
	double			inclusiveTime;
	double			exclusiveTime;
};

// Types of action taken in response to an rc_Assert() message
enum AssertAction_t
{
	ASSERT_ACTION_BREAK = 0,		//	Break on this Assert
	ASSERT_ACTION_IGNORE_THIS,		//	Ignore this Assert once
	ASSERT_ACTION_IGNORE_ALWAYS,	//	Ignore this Assert from now on
	ASSERT_ACTION_IGNORE_FILE,		//	Ignore all Asserts from this file from now on
	ASSERT_ACTION_IGNORE_ALL,		//	Ignore all Asserts from now on
	ASSERT_ACTION_OTHER				//	A more complex response requiring additional data (e.g. "ignore this Assert 5 times")
};

//id's for dispatching binary notifications to proper handlers
enum XBX_DBGBinaryNotification_HandlerID_t
{
	XBX_DBG_BNH_STACKTRANSLATOR,

	XBX_DBG_BNH_END, 
};
