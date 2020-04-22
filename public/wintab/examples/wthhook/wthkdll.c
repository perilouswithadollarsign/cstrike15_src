//wthkdll.c

/* Here, we just record a bunch of packets with a WTH_RECORD hook, and put them
    in a big list. If there is more than one context giving us packets, then
	we are in trouble; in real life, we would have to sort the packets by
	context. We should probably also check to see that the lcPktData
	and lcPktMode of each context doesn't change. */


#include <malloc.h>
#include <stdio.h>
#include <string.h>

#include <windows.h>
#include <wintab.h>

#define PACKETDATA PK_CONTEXT | PK_STATUS | PK_TIME | PK_CHANGED | PK_SERIAL_NUMBER | PK_CURSOR | PK_BUTTONS | PK_X | PK_Y | PK_Z | PK_NORMAL_PRESSURE | PK_TANGENT_PRESSURE | PK_ROTATION
#define PACKETMODE 0
#include <pktdef.h>


/* 16-bit does not have a 64-bit integer type */
#ifndef hyper
#define hyper long
#endif


#define NCONTEXTS 32


/* Shared memory */
static HANDLE shmem;
static unsigned hyper shmem_size;
static unsigned char *shmem_view = 0;

static unsigned hyper shmem_buf_size;
static unsigned char *shmem_buf = 0; /* Store everyone's packets in this buffer */

static HCTX *shmem_ctx = 0; /* One for each possible context (NCONTEXTS) */
static unsigned hyper *shmem_buf_pos = 0; 
static unsigned hyper *shmem_buf_play_pos = 0;
static unsigned long *shmem_pkts_recorded = 0;
static unsigned long *shmem_pkts_played = 0;
static unsigned long *shmem_time = 0;

static HWTHOOK *shmem_hHook = 0;



#ifdef _WIN32

void
shutdown_shared_mem( void )
{
	if( shmem_view )
		UnmapViewOfFile( shmem_view );
	if( shmem )
		CloseHandle( shmem );
}

BOOL
init_shared_mem( void )
{
	BOOL fInit;

	shmem_size = 0x100000;
	shmem_buf_size = shmem_size - (NCONTEXTS + 1)*sizeof(hyper) - (NCONTEXTS+2)*sizeof(long) - NCONTEXTS*sizeof(HCTX);

	shmem = CreateFileMapping( 
                (HANDLE) 0xFFFFFFFF,                              // use paging file
                0,	                                              // no security attributes
                PAGE_READWRITE,                                   // read/write access
                (unsigned long)((shmem_size >> 32) & 0xffffffff), // size: high 32-bits
                (unsigned long)(shmem_size & 0xffffffff),         // size: low 32-bits
                "wthkdll.dll: shared memory");                    // name of map object
	if (shmem == NULL)
		return FALSE;

	fInit = (GetLastError() != ERROR_ALREADY_EXISTS);

	shmem_view = MapViewOfFile( 
                shmem,                   // object to map view of
                FILE_MAP_WRITE,          // read/write access
                0,                       // high offset:  map from
                0,                       // low offset:   beginning
                0);                      // default: map entire file

	if( !shmem_view ) {
		shutdown_shared_mem();
		return FALSE;
	}

	shmem_buf_pos = (unsigned hyper*)shmem_view;
	shmem_buf_play_pos = shmem_buf_pos + 1;
	shmem_pkts_recorded = (long *)(shmem_buf_play_pos + NCONTEXTS);
	shmem_pkts_played = shmem_pkts_recorded + 1;
	shmem_hHook = (HWTHOOK)(shmem_pkts_played + 1);
	shmem_ctx = (HCTX *)(shmem_hHook + 1);
	shmem_time = (unsigned long *)(shmem_ctx + NCONTEXTS);
	shmem_buf = (unsigned char *)(shmem_time + NCONTEXTS);
	
	if( fInit ) {
		unsigned i;
		for( i = 0; i < NCONTEXTS; i++ ) {
			shmem_ctx[i] = 0;
			shmem_buf_pos[i] = 0;
			shmem_buf_play_pos[i] = 0;
			shmem_pkts_recorded[i] = 0;
			shmem_pkts_played[i] = 0;
			shmem_time[i] = 0;
		}
		*shmem_hHook = 0;
	}

	return TRUE;
}




/******************************************************************/
/* Windows DLL entry/exit points */

/* 32-bit entrypoint */
BOOL   WINAPI   DllMain (HANDLE hInst, 
                        unsigned long ul_reason_for_call,
                        void far * lpReserved)
{
	BOOL retval;

	switch( ul_reason_for_call ) {
	case DLL_PROCESS_ATTACH:
		retval = init_shared_mem();
		break;

	case DLL_THREAD_ATTACH:
		retval = TRUE;
		break;

	case DLL_THREAD_DETACH:
		retval = TRUE;
		break;

	case DLL_PROCESS_DETACH:
		shutdown_shared_mem();
		retval = TRUE;
		break;

	default:
		retval = FALSE;
	}

	return retval;
}

#endif /* WIN32 */


/* 16-bit dll entrypoint */  
BOOL WINAPI LibMain(HANDLE hModule, WORD wDataSeg, WORD cbHeapSize,
						LPSTR lpCmdLine)
{
	unsigned i;

	if( !shmem_buf_pos ) {
		shmem_buf_pos = calloc( 1, sizeof(hyper) );
		if( !shmem_buf_pos )
			return FALSE;
	}

	if( !shmem_buf_play_pos ) {
		shmem_buf_play_pos = calloc( 32, sizeof(hyper) );
		if( !shmem_buf_play_pos ) {
			free( shmem_buf_pos );
			shmem_buf_pos = 0;
			return FALSE;
		}
	}

	if( !shmem_pkts_recorded ) {
		shmem_pkts_recorded = calloc( 1, sizeof(hyper) );
		if( !shmem_pkts_recorded ) {
			free( shmem_buf_play_pos );
			free( shmem_buf_pos );
			shmem_buf_play_pos = 0;
			shmem_buf_pos = 0;
			return FALSE;
		}
	}

	if( !shmem_pkts_played ) {
		shmem_pkts_played = calloc( 1, sizeof(hyper) );
		if( !shmem_pkts_played ) {
			free( shmem_buf_play_pos );
			free( shmem_buf_pos );
			free( shmem_pkts_recorded );
			shmem_buf_play_pos = 0;
			shmem_buf_pos = 0;
			shmem_pkts_recorded = 0;
			return FALSE;
		}
	}

	if( !shmem_hHook ) {
		shmem_hHook = malloc( sizeof(HWTHOOK) );
		if( !shmem_hHook ) {
			free( shmem_buf_pos );
			shmem_buf_pos = 0;
			free( shmem_buf_play_pos );
			shmem_buf_play_pos = 0;
			free( shmem_pkts_recorded );
			free( shmem_pkts_played );
			shmem_pkts_recorded = 0;
			shmem_pkts_played = 0;
			return FALSE;
		}
		*shmem_hHook = 0;
	}

	if( !shmem_buf ) {
		shmem_buf_size = 0xff00;
		shmem_buf = calloc( 32, (size_t)shmem_buf_size );
		if( !shmem_buf ) {
			free( shmem_buf_pos );
			shmem_buf_pos = 0;
			free( shmem_buf_play_pos );
			shmem_buf_play_pos = 0;
			free( shmem_pkts_recorded );
			free( shmem_pkts_played );
			shmem_pkts_recorded = 0;
			shmem_pkts_played = 0;
			free( shmem_hHook );
			shmem_hHook = 0;
			return FALSE;
		}
	}

	if( !shmem_ctx ) {
		shmem_ctx = calloc( 32, sizeof(HCTX) );
		if( !shmem_ctx ) {
			free( shmem_buf_pos );
			shmem_buf_pos = 0;
			free( shmem_buf_play_pos );
			shmem_buf_play_pos = 0;
			free( shmem_pkts_recorded );
			free( shmem_pkts_played );
			shmem_pkts_recorded = 0;
			shmem_pkts_played = 0;
			free( shmem_hHook );
			shmem_hHook = 0;
			free( shmem_buf );
			shmem_buf = 0;
			shmem_buf_size = 0;
			return FALSE;
		}
	}

	if( !shmem_time ) {
		shmem_time = calloc( 32, sizeof(long) );
		if( !shmem_time ) {
			free( shmem_buf_pos );
			shmem_buf_pos = 0;
			free( shmem_buf_play_pos );
			shmem_buf_play_pos = 0;
			free( shmem_pkts_recorded );
			free( shmem_pkts_played );
			shmem_pkts_recorded = 0;
			shmem_pkts_played = 0;
			free( shmem_hHook );
			shmem_hHook = 0;
			free( shmem_buf );
			shmem_buf = 0;
			shmem_buf_size = 0;
			free( shmem_ctx );
			shmem_ctx = 0;
			return FALSE;
		}
	}

	for( i = 0; i < NCONTEXTS; i++ ) {
		shmem_ctx[i] = 0;
		shmem_buf_pos[i] = 0;
		shmem_buf_play_pos[i] = 0;
		shmem_pkts_recorded[i] = 0;
		shmem_pkts_played[i] = 0;
		shmem_time[i] = 0;
	}

	return TRUE;
}

/* Win16 dll exit point. */
int WINAPI WEP(int nSystemExit)
{
	if( shmem_buf )
		free( shmem_buf );
	if( shmem_buf_play_pos )
		free( shmem_buf_play_pos );
	if( shmem_buf_pos )
		free( shmem_buf_pos );
	if( shmem_pkts_recorded )
		free( shmem_pkts_recorded );
	if( shmem_pkts_played )
		free( shmem_pkts_played );
	if( shmem_hHook )
		free( shmem_hHook );

	return TRUE;
}




/******************************************************************/
/* Packet Decode Functions */

static const struct {
	WTPKT tag;				size_t size;			int nargs;	char *str;
} pkt_data_info[] = {
	{PK_CONTEXT,			sizeof(HCTX),		 1,			"Context: %u"},
	{PK_STATUS,				sizeof(UINT),		 1,			"Status: %u"},
	{PK_TIME,				sizeof(LONG),		 1,			"Time: %li"},
	{PK_CHANGED,			sizeof(WTPKT),		 1,			"Changed: %u"},
	{PK_SERIAL_NUMBER,		sizeof(UINT),		 1,			"Serial Number: %u"},
	{PK_CURSOR,				sizeof(UINT),		 1,			"Cursor: %u"},
	{PK_BUTTONS,			sizeof(DWORD),		 1,			"Buttons: %lu"},
	{PK_X,					sizeof(DWORD),		 1,			"X: %lu"},
	{PK_Y,					sizeof(DWORD),		 1,			"Y: %lu"},
	{PK_Z,					sizeof(DWORD),		 1,			"Z: %lu"},
	{PK_NORMAL_PRESSURE,	sizeof(UINT),		 1,			"Normal Pressure: %u"},
	{PK_TANGENT_PRESSURE,	sizeof(UINT),		 1,			"Tangent Pressure: %u"},
	{PK_ORIENTATION,		sizeof(ORIENTATION), 3,			"Orientation: %i, %i, %i"},
	{PK_ROTATION,			sizeof(ROTATION),	 3,			"Rotation: %i, %i, %i"},
	{0, 0, -1, 0}
};

size_t
compute_packet_size( WTPKT lcPktData )
{
	unsigned i = 0;
	size_t size = 0;

	while( pkt_data_info[i].nargs > 0 ) {
		if( lcPktData & pkt_data_info[i].tag )
			size += pkt_data_info[i].size;
		i++;
	}

	/* Check for extension data */
	i = 0;
	while( WTInfo( WTI_EXTENSIONS + i, EXT_NAME, 0 ) ) {
		UINT ext_size[2];
		WTPKT mask;

		if( WTInfo( WTI_EXTENSIONS + i, EXT_MASK, &mask ) && (mask & lcPktData) ) {
			WTInfo( WTI_EXTENSIONS + i, EXT_SIZE, ext_size );
			size += ext_size[0];
		}
		i++;
	}

	return size;
}

long
get_packet_offset( WTPKT lcPktData, WTPKT field )
{
	unsigned i = 0;
	long offset = 0;

	while( pkt_data_info[i].nargs > 0 && pkt_data_info[i].tag != field ) {
		if( lcPktData & pkt_data_info[i].tag  )
			offset += pkt_data_info[i].size;
		i++;
	}

	if( pkt_data_info[i].nargs == -1 )
		offset = -1;

	return offset;
}

size_t
display_packet( WTPKT lcPktData, unsigned char *packet )
{
	unsigned i = 0;
	size_t size = 0;
	char outstring[2048] = "";
	char str[128];

	while( lcPktData && pkt_data_info[i].nargs > 0 ) {
		if( lcPktData & pkt_data_info[i].tag ) {

			if( pkt_data_info[i].nargs == 1 )
				if( pkt_data_info[i].size == 2 )
					sprintf( str, pkt_data_info[i].str, *((WORD *)packet) );
				else
					sprintf( str, pkt_data_info[i].str, *((DWORD *)packet) );
			if( pkt_data_info[i].nargs == 3 )
				sprintf( str, pkt_data_info[i].str, packet, *((WORD *)packet + 1), *((WORD *)packet + 2) );

			strcat( outstring, str );
			strcat( outstring, "\n" );

			packet += pkt_data_info[i].size;
			size += pkt_data_info[i].size;
		}
		i++;
	}
	/* Display extension data fields */
	i = 0;
	while( WTInfo( WTI_EXTENSIONS + i, EXT_NAME, str ) ) {
		WTPKT mask;

		if( WTInfo( WTI_EXTENSIONS + i, EXT_MASK, &mask ) && (mask & lcPktData) ) {
			UINT ext_size[2];
			UINT j;

			WTInfo( WTI_EXTENSIONS + i, EXT_SIZE, ext_size );

			strcat( outstring, str );
			strcat( outstring, ": " );

			for( j = 0; j < ext_size[0] / sizeof(int); j++ ) {
				sprintf( str, "%8x", *((int *)packet + j) );
				strcat( outstring, str );
				strcat( outstring, " " );
			}
			strcat( outstring, "\n" );

			packet += ext_size[0];
			size += ext_size[0];
		}
		i++;
	}
	MessageBox( 0, outstring, "wthook", MB_OK );

	return size;
}

int
hCtx_index( HCTX hCtx )
{
	unsigned i = 0;

	while( i < NCONTEXTS && shmem_ctx[i] && shmem_ctx[i] != hCtx )
		i++;

	if( i == NCONTEXTS )
		return -1;

	if( !shmem_ctx[i] )
		shmem_ctx[i] = hCtx;

	return i;
}

void *
find_next_ctx_packet( HCTX hCtx, WTPKT *lcPktData, DWORD * time_change, size_t *size )
{
	HCTX packet_hCtx;
	void *retval;
	int index = hCtx_index(hCtx);

	if( index > -1 ) {
		/* Let shmem_buf_play_pos[index] end up pointing to the packet which we return */
		*size = 0;
		do {
			if( *size )
				shmem_buf_play_pos[index] += sizeof(WTPKT) + sizeof(HCTX) + sizeof(DWORD) + *size;

			retval = shmem_buf + shmem_buf_play_pos[index];

			*lcPktData = *((WTPKT *)(shmem_buf + shmem_buf_play_pos[index]));
			packet_hCtx = *((HCTX *)(shmem_buf + sizeof(WTPKT) + shmem_buf_play_pos[index]));
			*time_change = *((DWORD *)(shmem_buf + sizeof(WTPKT) + sizeof(HCTX) + shmem_buf_play_pos[index]));

			*size = compute_packet_size( *lcPktData );
		} while( shmem_buf_play_pos[index] < *shmem_buf_pos && packet_hCtx != hCtx );

		if( packet_hCtx != hCtx )
			retval = 0;
	} else
		retval = 0;

	return retval;
}




/******************************************************************/
/* Wintab hook functions */

LRESULT
WINAPI RecordHook(int nCode,WPARAM wParam,LPARAM lParam) 
{
	DWORD curtime;

	if( nCode >= 0 ) {
		LOGCONTEXT lc;
		size_t packet_size;

		/* We need to find the packet data size */
		WTGet((HCTX)wParam, &lc);
		packet_size = compute_packet_size( lc.lcPktData );
		
		/* If we have space left in our shared memory buffer, */
		if( *shmem_buf_pos < shmem_buf_size - packet_size - sizeof(WTPKT) ) {
			int index = hCtx_index( (HCTX)wParam );

			/* There's no garauntee that the contexts will be giving us the
			    data that we need in their packets, so explicitly calculate 
				everything we need here: */

			/* Write lc.lcPktData */
			*((WTPKT *)(shmem_buf + *shmem_buf_pos)) = lc.lcPktData;
			(*shmem_buf_pos) += sizeof(WTPKT);

			/* Write hCtx */
			*((HCTX *)(shmem_buf + *shmem_buf_pos)) = (HCTX)wParam;
			(*shmem_buf_pos) += sizeof(HCTX);

			/* Write time change (since previous packet of this context) */
			curtime = timeGetTime();
			if( shmem_time[index] == 0 ) {
				shmem_time[index] = curtime;
				*((DWORD *)(shmem_buf + *shmem_buf_pos)) = 0;
			} else {
				*((DWORD *)(shmem_buf + *shmem_buf_pos)) = curtime - shmem_time[index];
				shmem_time[index] = curtime;
			}
			(*shmem_buf_pos) += sizeof(DWORD);

			/* Write packet data */
			memcpy( shmem_buf + *shmem_buf_pos, (LPVOID)lParam, packet_size );
			(*shmem_buf_pos) += packet_size;

			(*shmem_pkts_recorded)++;
		}
		return TRUE;
	} else /* Let Wintab continue processing the packet */
		return WTMgrPacketHookNext(*shmem_hHook,nCode,wParam,lParam);
}


LRESULT WINAPI PlayHook(int nCode, WPARAM wParam, LPARAM lParam)
{
	DWORD delay;
	void *packet;
	size_t size;
	WTPKT lcPktData;
	LRESULT retval;
	int index;

	/* Find the next packet corrisponding to this context */
	packet = find_next_ctx_packet( (HCTX)wParam, &size, &lcPktData, &delay );

	if( packet ) {
		switch( nCode ) {
		case WTHC_GETNEXT:
			/* Copy the packet to the buffer */
			memcpy( (LPVOID)lParam, packet, size );
			retval = delay;
			break;

		case WTHC_SKIP:
			index = hCtx_index( (HCTX)wParam );
			shmem_buf_play_pos[index] += 
				size + sizeof(WTPKT) + sizeof(DWORD) + sizeof(HCTX);
			retval = 0;

			(*shmem_pkts_played)++;
			break;

		default:
			break;
		}
	} else
		retval = WTMgrPacketHookNext(*shmem_hHook, nCode, wParam, lParam);

	return retval;
}




/******************************************************************/
/* Exported user functions */

long WINAPI get_num_pkts_recorded( void )
{
	return *shmem_pkts_recorded;
}

long WINAPI get_num_pkts_played( void )
{
	return *shmem_pkts_played;
}


BOOL WINAPI Record(BOOL fEnable, HMGR hMgr)
{
	BOOL result = FALSE;

	if (fEnable) {
		if (!*shmem_hHook)
			*shmem_hHook = WTMgrPacketHookEx(hMgr, WTH_RECORD,
										"wthkdll.dll", "RecordHook");
		result = !!*shmem_hHook;
	} else {
		if (*shmem_hHook) {
			WTMgrPacketUnhook(*shmem_hHook);
            *shmem_hHook = 0;
		}
		result = TRUE;
	}
	return result;
}

void
WINAPI display_record(void)
{
	unsigned char *ptr = shmem_buf;

	while( ptr < shmem_buf + *shmem_buf_pos ) {
		WTPKT lcPktData;
		HCTX hCtx;
		DWORD time;

		lcPktData = *((WTPKT *)ptr);
		ptr += sizeof(WTPKT);

		hCtx = *((HCTX *)ptr);
		ptr += sizeof(HCTX);

		time = *((DWORD *)ptr);
		ptr += sizeof(DWORD);

		ptr += display_packet( lcPktData, ptr );
	}
}

BOOL WINAPI Playback(BOOL fEnable, HMGR hMgr)
{
	BOOL result = FALSE;

	if( fEnable ) {
		if( !*shmem_hHook )
			*shmem_hHook = WTMgrPacketHookEx( hMgr, WTH_PLAYBACK,
				"wthkdll.dll", "PlayHook" );
		result = !!*shmem_hHook;
	} else {
		if( *shmem_hHook ) {
			WTMgrPacketUnhook( *shmem_hHook );
            *shmem_hHook = 0;
		}
		result = TRUE;
	}
	return result;
}

/* Even when wthook.exe ends, the shared memory of the dll is still hanging
around, since the dll is loaded by wintab32 into other people's address spaces.
We have to reset the shared memory, so that the next time wthook runs, it doesn't
pick up the old shared memory contents. */
void
WINAPI reset( void )
{
	/* Just set the counters back to zero */
	*shmem_buf_pos = 0;
	*shmem_buf_play_pos = 0;
	*shmem_pkts_recorded = 0;
	*shmem_pkts_played = 0;
}