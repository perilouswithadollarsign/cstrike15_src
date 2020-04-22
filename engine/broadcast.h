//========= Copyright © Valve Corporation, All rights reserved. ============//

#ifndef ENGINE_BROADCAST_HDR
#define ENGINE_BROADCAST_HDR

enum BroadcastChunkEnum
{
	BROADCAST_FULLFRAME, // a single keyframe message
	BROADCAST_SIGNON,
	BROADCAST_NETWORK_DATA_TABLES,
	BROADCAST_STRING_TABLES,

	BROADCAST_FRAMES, // a number of incremental frames (384 frames for a 3-second pack on a 128-tick server)
	BROADCAST_DELTAFRAME,
	BROADCAST_CONSOLE_COMMAND,
	BROADCAST_STOP,
	BROADCAST_TOC,
	BROADCAST_TOC_SIZE
};

struct BroadcastChunk_t
{
	uint32 nChunkId;
	uint32 nChunkSize; // size in bytes of the chunk that follows; 0 means it's empty
};

struct BroadcastTocKeyframe_t
{
	uint32 nKeyframeTick;
	uint32 nBroadcastIndex;
};




#endif // ENGINE_BROADCAST_HDR