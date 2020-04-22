//==== Copyright (c) 1996-2010, Valve Corporation, All rights reserved. =====//
//
// Purpose: net_chan.cpp: implementation of the CNetChan_t struct.
//
//===========================================================================//

#include "../utils/bzip2/bzlib.h"
#include "net_chan.h"
#include "tier1/strtools.h"
#include "filesystem_engine.h"
#include "demo.h"
#include "convar.h"
#include "mathlib/mathlib.h"
#include "protocol.h"
#include "inetmsghandler.h"
#include "host.h"
#include "netmessages.h"
#include "replay.h"
#include "datacache/imdlcache.h"
#include "tier0/vprof.h"
#if defined( _X360 )
#include "xbox/xbox_console.h"
#endif
#ifdef POSIX
#include "net_ws_headers.h"									// need SOCKET
#endif
#include "net_ws_queued_packet_sender.h"
#include "download.h"
#include "filesystem_init.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

ConVar net_showudp( "net_showudp", "0", FCVAR_RELEASE, "Dump UDP packets summary to console" );
ConVar net_showudp_oob( "net_showudp_oob", "0", FCVAR_RELEASE, "Dump OOB UDP packets summary to console" );
ConVar net_showudp_remoteonly( "net_showudp_remoteonly", "0", FCVAR_RELEASE, "Dump non-loopback udp only" );

ConVar net_showtcp( "net_showtcp", "0", 0, "Dump TCP stream summary to console" );
ConVar net_blocksize( "net_maxfragments", NETSTRING( MAX_ROUTABLE_PAYLOAD ), 0, "Max fragment bytes per packet", true, FRAGMENT_SIZE, true, MAX_ROUTABLE_PAYLOAD );

static ConVar net_showmsg( "net_showmsg", "0", FCVAR_DEVELOPMENTONLY, "Show incoming message: <0|1|name>" );
static ConVar net_showfragments( "net_showfragments", "0", 0, "Show netchannel fragments" );
static ConVar net_showpeaks( "net_showpeaks", "0", 0, "Show messages for large packets only: <size>" );
static ConVar net_blockmsg( "net_blockmsg", "0", FCVAR_CHEAT, "Discards incoming message: <0|1|name>" ); // "none" here is bad, causes superfluous strcmp on every net message
static ConVar net_showdrop( "net_showdrop", "0", FCVAR_DEVELOPMENTONLY, "Show dropped packets in console" );
static ConVar net_drawslider( "net_drawslider", "0", 0, "Draw completion slider during signon" );
static ConVar net_chokeloopback( "net_chokeloop", "0", FCVAR_DEVELOPMENTONLY, "Apply bandwidth choke to loopback packets" );
static ConVar net_maxfilesize( "net_maxfilesize", "16", 0, "Maximum allowed file size for uploading in MB", true, 0, true, 64 );
static ConVar net_compresspackets( "net_compresspackets", "1", FCVAR_DEVELOPMENTONLY, "Use lz compression on game packets." );
static ConVar net_compresspackets_minsize( "net_compresspackets_minsize", "1000", FCVAR_DEVELOPMENTONLY, "Don't bother compressing packets below this size." );
static ConVar net_maxcleartime( "net_maxcleartime", "4.0", 0, "Max # of seconds we can wait for next packets to be sent based on rate setting (0 == no limit)." );
static ConVar net_droponsendoverflow( "net_droponsendoverflow", "0", FCVAR_RELEASE, "If enabled, channel will drop client when sending too much data causes buffer overrun" );

extern ConVar net_maxroutable;

extern int  NET_ConnectSocket( int nSock, const netadr_t &addr );
extern void NET_CloseSocket( int hSocket, int sock = -1 );
extern int  NET_SendStream( int nSock, const char * buf, int len, int flags );
extern int  NET_ReceiveStream( int nSock, char * buf, int len, int flags );

// If the network connection hasn't been active in this many seconds, display some warning text.
#define CONNECTION_PROBLEM_TIME		4.0f	// assume network problem after this time

#define BYTES2FRAGMENTS(i) ((i+FRAGMENT_SIZE-1)/FRAGMENT_SIZE)

#define FLIPBIT(v,b) if (v&b) v &= ~b; else v |= b;

static const char HEX[] = "0123456789abcdef";

void LogMultiline(bool input, char const *label, const char * data, size_t len) {
	const char * direction = (input ? " << " : " >> ");

	const size_t LINE_SIZE = 24;
	char hex_line[LINE_SIZE * 9 / 4 + 2], asc_line[LINE_SIZE + 1];
	while (len > 0) 
	{
		memset(asc_line, ' ', sizeof(asc_line));
		memset(hex_line, ' ', sizeof(hex_line));
		size_t line_len = MIN(len, LINE_SIZE);
		for (size_t i=0; i<line_len; ++i) {
			unsigned char ch = static_cast<unsigned char>(data[i]);
			asc_line[i] = ( V_isprint(ch) && !V_iscntrl(ch) ) ? data[i] : '.';
			hex_line[i*2 + i/4] = HEX[ch >> 4];
			hex_line[i*2 + i/4 + 1] = HEX[ch & 0xf];
		}
		asc_line[sizeof(asc_line)-1] = 0;
		hex_line[sizeof(hex_line)-1] = 0;
		Msg( "%s %s %s %s \n", label, direction, asc_line, hex_line );
		data += line_len;
		len -= line_len;
	}
}

// Want this on PC and non-cert builds
#if !defined( _GAMECONSOLE ) || !defined( _CERT )
#define NET_PARANOID_DUMPS
#endif

#if defined( NET_PARANOID_DUMPS )
class CNetchanParanoidMode
{
public:
	CNetchanParanoidMode();
	~CNetchanParanoidMode();

	void Init();

	void StartPacket( bf_read &fullbuf );
	void NoteHeaderSize( bf_read &buf, int flags );
	void AddMessage( INetMessage *msg, int nStartBit );
	// Message has been parsed, update raw data
	void UpdateMessage( INetMessage *msg, int nEndBit );
	void AddControlMessage( int m_nType, int nStartBit, char const *name, char const *desc );

	// Called if processing failes for some reason
	void Dump( CNetChan *channel,  char const *pchReason );

private:

	bool	IsEnabled();

	bool	m_bInitialized;

	int		m_nRawDataSize;
	byte	*m_pRawData;
	int		m_nHeaderBits;
	int		m_nFlags;

	struct MessageItem_t
	{
		MessageItem_t() :
			m_pMsg( NULL ),
			m_nMessageSize( 0 ),
			m_nType( 0 ),
			m_nGroup( 0 ),
			m_nStartBit( 0 ),
			m_nEndBit( -1 )
		{
			m_szName[ 0 ] = 0;
			m_szDesc[ 0 ] = 0;
			Q_memset( m_Message, 0, sizeof( m_Message ) );
		}

		void Init( INetMessage *msg, int nStartBit )
		{
			m_pMsg = msg;
			m_nMessageSize = msg->GetSize();
			m_nType = msg->GetType();
			m_nGroup = msg->GetGroup();
			Q_strncpy( m_szName, msg->GetName(), sizeof( m_szName ) );
			//Q_strncpy( m_szDesc, msg->ToString(), sizeof( m_szDesc ) );
			m_szDesc[ 0 ] = 0; // Don't call ToString until after message parses
			m_nStartBit = nStartBit;
			m_nEndBit = -1;
		}

		void InitControlMessage( int nType, int nStartBit, char const *pchName, char const *pchDesc )
		{
			m_pMsg = NULL;
			m_nMessageSize = 0;
			m_nType = nType;
			m_nGroup = 0;
			Q_strncpy( m_szName, pchName, sizeof( m_szName ) );
			Q_strncpy( m_szDesc, pchDesc, sizeof( m_szDesc ) );
			m_nStartBit = nStartBit;
			m_nEndBit = -1;
		}

		void UpdateRaw( INetMessage *msg, int nEndBit )
		{
			Q_memcpy( m_Message, (const byte *)msg, MIN( msg->GetSize(), sizeof( m_Message ) ) );
			m_nEndBit = nEndBit;
			Q_strncpy( m_szDesc, msg->ToString(), sizeof( m_szDesc ) );
		}

		void Dump()
		{
			if ( m_pMsg && m_nEndBit == -1 )
			{
				Msg( "---> DIDN'T FINISH PARSING %s\n", m_szName );
			}
			Msg( "%s: type( %d ) group ( %d ) size ( %d bytes ), startbit %d end bit %d\n", m_szName, m_nType, m_nGroup, m_nMessageSize, m_nStartBit, m_nEndBit );
			Msg( "   %s\n", m_szDesc );
			// Raw contents
			if ( m_pMsg )
			{
				Msg( "RAW(%s) start\n", m_szName );
				LogMultiline( false, m_szName, (const char *)m_pMsg, m_nMessageSize );
				Msg( "RAW(%s) end\n", m_szName );
			}
			else
			{
				Msg( " %s Control message\n", m_szName );
			}
		}

		INetMessage *m_pMsg;
		size_t		m_nMessageSize;
		int			m_nType;
		int			m_nGroup;
		int			m_nStartBit;
		int			m_nEndBit;
		byte		m_Message[ 128 ];;
		char		m_szName[ 32 ];
		char		m_szDesc[ 128 ];  // m_pMsg->ToString()
	};

	enum
	{
		MAX_MESSAGES_IN_PACKET = 256,
	};

	MessageItem_t	*m_pPacketMessages;
	int				m_nPacketMessageCount;
};

static ConVar net_dumptest( "net_dumptest", "0" );
static ConVar net_paranoid( "net_paranoid", "1" );

static CNetchanParanoidMode g_NetParanoid;

void NET_InitParanoidMode()
{
	g_NetParanoid.Init();
}

CNetchanParanoidMode::CNetchanParanoidMode() :
	m_bInitialized( false ),
	m_nRawDataSize( 0 ),
	m_pRawData( NULL ),
	m_nHeaderBits( 0 ),
	m_nFlags( 0 ),
	m_pPacketMessages( NULL ),
	m_nPacketMessageCount( 0 )
{
}

CNetchanParanoidMode::~CNetchanParanoidMode()
{
	delete[] m_pPacketMessages;
	delete[] m_pRawData;
}

bool CNetchanParanoidMode::IsEnabled()
{
	if ( !m_bInitialized )
		return false;
	return net_paranoid.GetBool();
}

void CNetchanParanoidMode::Init()
{
	m_bInitialized = true;

	m_pRawData = new byte[ NET_MAX_PAYLOAD ];
	m_pPacketMessages = new MessageItem_t[ MAX_MESSAGES_IN_PACKET ];
}

void CNetchanParanoidMode::StartPacket( bf_read &fullbuf )
{
	if ( !IsEnabled() )
		return;

	m_nRawDataSize = MIN( fullbuf.TotalBytesAvailable(), NET_MAX_PAYLOAD );
	Q_memcpy( m_pRawData, fullbuf.GetBasePointer(), m_nRawDataSize );

	// Reset counter
	m_nPacketMessageCount = 0;
	m_nHeaderBits = 0;
	m_nFlags = 0;
}

void CNetchanParanoidMode::NoteHeaderSize( bf_read &msg, int flags )
{
	if ( !IsEnabled() )
		return;

	m_nHeaderBits = msg.GetNumBitsRead();
	m_nFlags = flags;
}

void CNetchanParanoidMode::AddMessage( INetMessage *msg, int nStartBit )
{
	if ( !IsEnabled() )
		return;

	if ( m_nPacketMessageCount >= MAX_MESSAGES_IN_PACKET - 1 )
		return;

	MessageItem_t &item = m_pPacketMessages[ m_nPacketMessageCount++ ];
	item.Init( msg, nStartBit );
}

void CNetchanParanoidMode::UpdateMessage( INetMessage *msg, int nEndBit )
{
	if ( !IsEnabled() )
		return;
	if ( m_nPacketMessageCount <= 0 )
		return;

	MessageItem_t &item = m_pPacketMessages[ m_nPacketMessageCount - 1 ];
	if ( item.m_pMsg != msg )
		return;

	item.UpdateRaw( msg, nEndBit );
}

void CNetchanParanoidMode::AddControlMessage( int m_nType, int nStartBit, char const *name, char const *desc )
{
	if ( !IsEnabled() )
		return;

	if ( m_nPacketMessageCount >= MAX_MESSAGES_IN_PACKET - 1 )
		return;

	MessageItem_t &item = m_pPacketMessages[ m_nPacketMessageCount++ ];
	item.InitControlMessage( m_nType, nStartBit, name, desc );
}

void CNetchanParanoidMode::Dump( CNetChan *channel, char const *pchReason )
{
	if ( !IsEnabled() )
		return;

	Msg( "%s\n ", pchReason );
	Msg( "Dumping messages for channel %s(%s) 0x%p\n",
		channel->GetName(), channel->GetAddress(), channel );

	Msg( "Header bits %d, flags == %d\n", m_nHeaderBits, m_nFlags );

	Msg( "%d messages\n", m_nPacketMessageCount );

	for ( int i = 0; i < m_nPacketMessageCount; ++i )
	{
		MessageItem_t &item = m_pPacketMessages[ i ];
		Msg( "%d -----------------------\n", i );
		item.Dump();
	}

	// Dump raw memory, too
	Msg( "Raw\n" );
	LogMultiline( false, "PKT", (const char *)m_pRawData, m_nRawDataSize );
}
#else

void NET_InitParanoidMode()
{
	// NOTHING!!!
}

#endif

// We only need to checksum packets on the PC and only when we're actually sending them over the network.
bool ShouldChecksumPackets()
{
	return NET_IsMultiplayer();
}

bool CNetChan::IsLoopback() const
{
	return remote_address.IsLoopback();		
}

bool CNetChan::IsNull() const
{
	return remote_address.IsNull();
}

/*
==============================
CNetChan::Clear

==============================
*/
void CNetChan::Clear()
{
	int i;

	// clear waiting lists

	for ( i=0; i<MAX_STREAMS; i++ )
	{
		while ( m_WaitingList[i].Count() )
			RemoveHeadInWaitingList( i );	

		if ( m_ReceiveList[i].buffer )
		{
			delete[] m_ReceiveList[i].buffer;
			m_ReceiveList[i].buffer = NULL;
		}
	}

	for( i=0; i<MAX_SUBCHANNELS; i++ )
	{
		if ( m_SubChannels[i].state == SUBCHANNEL_TOSEND )
		{
			int bit = 1<<i; // flip bit back since data was send yet
			
			FLIPBIT(m_nOutReliableState, bit);

			m_SubChannels[i].Free(); 
		}
		else if ( m_SubChannels[i].state == SUBCHANNEL_WAITING )
		{
			// data is already out, mark channel as dirty
			m_SubChannels[i].state = SUBCHANNEL_DIRTY;
		}
	}

	m_bStopProcessing = true;

	Reset();
}


void CNetChan::CompressFragments()
{
	// We don't want this to go in the VCR file, because the compressed size can be different. The reason is 
	// that the bf_writes that contributed to this message may have uninitialized bits at the end of the buffer
	// (for example if it uses only the first couple bits of the last byte in the message). If the
	// last few bits are different, it can produce a different compressed size.
	if ( !m_bUseCompression || !net_compresspackets.GetBool() )
		return;

	// write fragemnts for both streams
	for ( int i=0; i<MAX_STREAMS; i++ )
	{
		if ( m_WaitingList[i].Count() == 0 )
			continue;

		// get the first fragments block which is send next
		dataFragments_t *data = m_WaitingList[i][0];

		// if data is already compressed or too small, skip it
		if ( data->isCompressed || data->bytes < 512 )
			continue;

		// if we already started sending this block, we can't compress it anymore
		if ( data->ackedFragments > 0 || data->pendingFragments > 0 )
			continue;

		//ok, compress it.

		if ( data->buffer )	
		{
			// fragments data is in memory
			unsigned int compressedSize = data->bytes;
			char * compressedData = new char[data->bytes];

			if ( NET_BufferToBufferCompress( compressedData , &compressedSize, data->buffer, data->bytes ) )
			{
				const char *name = GetName();
				const char *address = GetAddress();
				DevMsg("Compressing fragments for %s(%s) (%d -> %d bytes)\n", name, address, data->bytes, compressedSize );

				// copy compressed data but dont reallocate memory
				Q_memcpy( data->buffer, compressedData, compressedSize );

				data->nUncompressedSize = data->bytes;
				data->bytes = compressedSize;
				data->numFragments = BYTES2FRAGMENTS(data->bytes);
				data->isCompressed = true;				
			}

			delete [] compressedData; // free temp buffer
		}
		else // it's a file
		{
			Assert( data->file != FILESYSTEM_INVALID_HANDLE );

			char compressedfilename[ MAX_OSPATH ];
			int compressedFileSize = -1;
			FileHandle_t hZipFile = FILESYSTEM_INVALID_HANDLE;

			// check to see if there is a compressed version of the file
			Q_snprintf( compressedfilename, sizeof(compressedfilename), "%s.ztmp", data->filename);

			// check the timestamps 
			int compressedFileTime = g_pFileSystem->GetFileTime( compressedfilename );
			int fileTime = g_pFileSystem->GetFileTime( data->filename );

			if ( compressedFileTime >= fileTime )
			{
				// compressed file is newer than uncompressed file, use this one
				hZipFile = g_pFileSystem->Open( compressedfilename, "rb", NULL );
			}

			if ( hZipFile != FILESYSTEM_INVALID_HANDLE )
			{
				// use the existing compressed file
				compressedFileSize = g_pFileSystem->Size( hZipFile );
			}
			else
			{
				// create compressed version of source file
				char *uncompressed = new char[data->bytes];
				char *compressed = new char[data->bytes];
				unsigned int compressedSize = data->bytes;
				unsigned int uncompressedSize = data->bytes;
					
				// read in source file
				g_pFileSystem->Read( uncompressed, data->bytes, data->file );

				// compress into buffer
				if ( NET_BufferToBufferCompress( compressed, &compressedSize, uncompressed, uncompressedSize ) )
				{
					// write out to disk compressed version
					hZipFile = g_pFileSystem->Open( compressedfilename, "wb", NULL );

					if ( hZipFile != FILESYSTEM_INVALID_HANDLE )
					{
						DevMsg("Creating compressed version of file %s (%d -> %d)\n", data->filename, data->bytes, compressedSize);
						g_pFileSystem->Write( compressed, compressedSize, hZipFile );
						g_pFileSystem->Close( hZipFile );

						// and open zip file it again for reading
						hZipFile = g_pFileSystem->Open( compressedfilename, "rb", NULL );

						if ( hZipFile != FILESYSTEM_INVALID_HANDLE )
						{
							// ok, now everything if fine
							compressedFileSize = compressedSize;
						}
					}
				}
				
				delete [] uncompressed;
				delete [] compressed;
			}

			if ( compressedFileSize > 0 )
			{
				// use compressed file handle instead of original file
				g_pFileSystem->Close( data->file );
				data->file = hZipFile;
				data->nUncompressedSize = data->bytes;
				data->bytes = compressedFileSize;
				data->numFragments = BYTES2FRAGMENTS(data->bytes);
				data->isCompressed = true;
			}
		}
	}
}

void CNetChan::UncompressFragments( dataFragments_t *data )
{
	if ( !data->isCompressed )
		return;

	VPROF( "UncompressFragments" );

	// allocate buffer for uncompressed data, align to 4 bytes boundary
	char *newbuffer = new char[PAD_NUMBER( data->nUncompressedSize, 4 )];
	unsigned int uncompressedSize = data->nUncompressedSize;

	// uncompress data
	NET_BufferToBufferDecompress( newbuffer, &uncompressedSize, data->buffer, data->bytes );

	Assert( uncompressedSize == data->nUncompressedSize );

	// free old buffer and set new buffer
	delete [] data->buffer;
	data->buffer = newbuffer;
	data->bytes = uncompressedSize;
	data->isCompressed = false;
}

unsigned int CNetChan::RequestFile(const char *filename, bool bIsReplayDemoFile	)
{
	m_FileRequestCounter++;

	if ( net_showfragments.GetInt() == 2 )
	{
		DevMsg("RequestFile: %s (ID %i)\n", filename, m_FileRequestCounter );
	}

	CNETMsg_File_t file;
	file.set_transfer_id( m_FileRequestCounter );
	file.set_file_name( filename );
	file.set_is_replay_demo_file( bIsReplayDemoFile );
	file.set_deny( 0 );
	file.WriteToBuffer( m_StreamReliable );

	return m_FileRequestCounter;
}

void CNetChan::RequestFile_OLD(const char *filename, unsigned int transferID)
{
	Error( "Called RequestFile_OLD" );
}

void CNetChan::DenyFile(const char *filename, unsigned int transferID, bool bIsReplayDemoFile )
{
	if ( net_showfragments.GetInt() == 2 )
	{
		DevMsg("DenyFile: %s (ID %i)\n", filename, transferID );
	}

	CNETMsg_File_t file;
	file.set_transfer_id( transferID );
	file.set_file_name( filename );
	file.set_is_replay_demo_file( bIsReplayDemoFile );
	file.set_deny( 1 );
	file.WriteToBuffer( m_StreamReliable );
}

bool CNetChan::SendFile(const char *filename, unsigned int transferID, bool bIsReplayDemoFile )
{
	// add file to waiting list
	if ( IsNull() )
		return true;

	if ( !filename )
		return false;

	const char *sendfile = filename;
	while( sendfile[0] && PATHSEPARATOR( sendfile[0] ) )
	{
		sendfile = sendfile + 1;
	}

	// Don't transfer exe, vbs, com, bat-type files.
	if ( !IsValidFileForTransfer( sendfile ) )
		return false;

	if ( !CreateFragmentsFromFile( sendfile, FRAG_FILE_STREAM, transferID, bIsReplayDemoFile ) )
	{
		DenyFile( sendfile, transferID, bIsReplayDemoFile ); // send host a deny message
		return false;
	}

	if ( net_showfragments.GetInt() == 2 )
	{
		DevMsg("SendFile: %s (ID %i)\n", sendfile, transferID );
	}

	return true;
}

bool CNetChan::EnqueueVeryLargeAsyncTransfer( INetMessage &msg )
{
	if ( IsNull() )
		return true;

	char chStackBuffer[ NET_MAX_PAYLOAD ];
	bf_write bufWrite;
	bufWrite.StartWriting( chStackBuffer, NET_MAX_PAYLOAD );
	bool bResult = msg.WriteToBuffer( bufWrite );
	if ( !bResult )
	{
		Warning( "EnqueueVeryLargeAsyncTransfer %s: buffer overflow (maxsize = %d)!\n", GetAddress(), NET_MAX_PAYLOAD );
		Assert( 0 );

		if ( net_droponsendoverflow.GetBool() )
		{
			m_MessageHandler->ConnectionCrashed( "Buffer overflow in EnqueueVeryLargeAsyncTransfer" );
		}

		return false;
	}

	if ( !bufWrite.GetNumBytesWritten() )
		return false;
	
	//////////////////////////////////////////////////////////////////////////

	dataFragments_t *data = new dataFragments_t;
	data->bytes = bufWrite.GetNumBytesWritten();
	data->bits = data->bytes * 8;
	data->buffer = new char[ bufWrite.GetNumBytesWritten() ];
	V_memcpy( data->buffer, chStackBuffer, bufWrite.GetNumBytesWritten() );

	data->isCompressed = false;
	data->isReplayDemo = false;
	data->nUncompressedSize = 0;
	data->file = FILESYSTEM_INVALID_HANDLE;

	m_FileRequestCounter++;
	data->transferID = m_FileRequestCounter;
	V_memset( data->filename, 0, sizeof( data->filename ) );

	m_WaitingList[ FRAG_FILE_STREAM ].AddToTail( data );	// that's it for now

	// check if send as stream or with snapshot
	data->asTCP = false; // m_StreamActive && ( Bits2Bytes(data->length) > m_MaxReliablePayloadSize );

	// calc number of fragments needed
	data->numFragments = BYTES2FRAGMENTS( data->bytes );
	data->ackedFragments = 0;
	data->pendingFragments = 0;

	if ( net_showfragments.GetInt() == 2 )
	{
		DevMsg( "EnqueueVeryLargeAsyncTransfer: %s (%d bytes, ID %i)\n", msg.GetName(), data->bytes, data->transferID );
	}

	return true;
}

void CNetChan::Shutdown(const char *pReason)
{
	// send discconect

	if ( m_Socket < 0 )
		return;

	Clear(); // free all buffers (reliable & unreliable)

	if ( pReason )
	{
		// send disconnect message
		CNETMsg_Disconnect_t disconnect;
		disconnect.set_text( pReason );
		disconnect.WriteToBuffer( m_StreamUnreliable );
		Transmit();	// push message out
	}

	if ( m_StreamSocket )
	{
		NET_CloseSocket( m_StreamSocket, m_Socket );
		m_StreamSocket = 0;
		m_StreamActive = false;
	}

	NET_TerminateConnection( m_Socket, remote_address );

	// signals that netchannel isn't valid anymore
	m_Socket = -1; 
	remote_address.Clear();
	m_szRemoteAddressName[0] = '\0';

	if ( m_MessageHandler )
	{
		m_MessageHandler->ConnectionClosing( pReason );
		m_MessageHandler = NULL;
	}

	// free net messages
	for ( int i = 0; i < m_NetMessages.Count(); i++ )
	{
		CUtlVector< INetMessageBinder * >& Messages = m_NetMessages[ i ];

		for( int j = Messages.Count() - 1; j >= 0; j-- )
		{
			Messages[ j ]->SetNetChannel( NULL );
		}
	}

	m_NetMessages.Purge();


	m_DemoRecorder = NULL;

	if ( m_bProcessingMessages )
	{
		NET_RemoveNetChannel( this, false );	// Delay the deletion or it'll crash in the message-processing loop.
		m_bShouldDelete = true;
	}
	else
	{
		NET_RemoveNetChannel( this, true );
	}
}

CNetChan::CNetChan() : m_SplitPlayers( 0, 0, SplitPlayer_t::Less )
{
	m_pActiveChannel = this;
	m_nSplitPacketSequence = 1;
	m_nMaxRoutablePayloadSize = MAX_ROUTABLE_PAYLOAD;
	m_bProcessingMessages = false;
	m_bShouldDelete = false;
	m_bStopProcessing = false;
	m_Socket = -1; // invalid
	remote_address.Clear();
	m_szRemoteAddressName[0] = '\0';
	last_received = 0;
	connect_time = 0;
	
	Q_strncpy( m_Name, "", sizeof(m_Name) ); 

	m_MessageHandler = NULL;
	m_DemoRecorder = NULL;

	m_StreamUnreliable.SetDebugName( "netchan_t::unreliabledata" );
	m_StreamReliable.SetDebugName( "netchan_t::reliabledata" );

	m_Rate		= DEFAULT_RATE;
	m_Timeout	= SIGNON_TIME_OUT;

	// Prevent the first message from getting dropped after connection is set up.

	m_nOutSequenceNr = 1;	// otherwise it looks like a 	
	m_nInSequenceNr = 0;
	m_nOutSequenceNrAck = 0;
	m_nOutReliableState = 0; // our current reliable state
	m_nInReliableState = 0;	// last remote reliable state
	// m_nLostPackets = 0;

	m_ChallengeNr = 0;

	m_StreamSocket = 0;
	m_StreamActive = false;

	ResetStreaming();

	m_MaxReliablePayloadSize = 	NET_MAX_PAYLOAD;

	m_FileRequestCounter = 0;
	m_bFileBackgroundTranmission = true;
	m_bUseCompression = false;
	m_nQueuedPackets = 0;

	m_flRemoteFrameTime = 0;
	m_flRemoteFrameTimeStdDeviation = 0;
	m_flRemoteFrameStartTimeStdDeviation = 0;

	m_bWasLastMessageReliable = false;

	FlowReset();
}

CNetChan::~CNetChan()
{
	Shutdown("NetChannel removed.");
}

/*
==============
CNetChan::Setup

called to open a channel to a remote system
==============
*/
void CNetChan::Setup( int sock, const ns_address &adr, const char * name, INetChannelHandler * handler, const byte *pbEncryptionKey )
{
	Assert( name ); 
	Assert ( handler );

	m_Socket = sock;

	if ( m_StreamSocket )
	{
		NET_CloseSocket( m_StreamSocket );
		m_StreamSocket = 0;
	}

	remote_address = adr;
	V_strcpy_safe( m_szRemoteAddressName, ns_address_render( remote_address ).String() );

	last_received		= net_time;
	connect_time		= net_time;
	
	Q_strncpy( m_Name, name, sizeof(m_Name) ); 

	m_MessageHandler = handler;

	m_DemoRecorder = NULL;

	MEM_ALLOC_CREDIT();

	SetMaxBufferSize( false, NET_MAX_DATAGRAM_PAYLOAD );

	SetMaxBufferSize( false, NET_MAX_DATAGRAM_PAYLOAD, true ); //Set up voice buffer

	SetMaxBufferSize( true, NET_MAX_PAYLOAD );

	m_Rate		= DEFAULT_RATE;
	m_Timeout	= SIGNON_TIME_OUT;

	// Prevent the first message from getting dropped after connection is set up.

	m_nOutSequenceNr = 1;	// otherwise it looks like a 	
	m_nInSequenceNr = 0;
	m_nOutSequenceNrAck = 0;
	m_nOutReliableState = 0; // our current reliable state
	m_nInReliableState = 0;	// last remote reliable state
	m_nChokedPackets = 0;
	m_fClearTime = 0.0;
	
	m_ChallengeNr = 0;

	m_StreamSocket = 0;
	m_StreamActive = false;

	m_ReceiveList[FRAG_NORMAL_STREAM].buffer = NULL;
	m_ReceiveList[FRAG_FILE_STREAM].buffer = NULL;

	// init 8 subchannels
	for ( int i=0; i<MAX_SUBCHANNELS; i++ )
	{
		m_SubChannels[i].index = i; // set index once
		m_SubChannels[i].Free();
	}
	
	ResetStreaming();

	if ( NET_IsMultiplayer() )
	{
		m_MaxReliablePayloadSize = 	net_blocksize.GetInt();
	}
	else
	{
		m_MaxReliablePayloadSize = 	NET_MAX_PAYLOAD;
	}

	FlowReset();

	// Set our encryption key
	m_EncryptionKey.Purge();
	if ( pbEncryptionKey )
	{
		m_EncryptionKey.EnsureCapacity( NET_CRYPT_KEY_LENGTH );
		Q_memcpy( m_EncryptionKey.Base(), pbEncryptionKey, NET_CRYPT_KEY_LENGTH );
	}

	m_NETMsgNOP.Bind< CNETMsg_NOP_t >( this, UtlMakeDelegate( this, &CNetChan::NETMsg_NOP ) );
	m_NETMsgDisconnect.Bind< CNETMsg_Disconnect_t >( this, UtlMakeDelegate( this, &CNetChan::NETMsg_Disconnect ) );
	m_NETMsgFile.Bind< CNETMsg_File_t >( this, UtlMakeDelegate( this, &CNetChan::NETMsg_File ) );
	m_NETMsgSplitScreenUser.Bind< CNETMsg_SplitScreenUser_t >( this, UtlMakeDelegate( this, &CNetChan::NETMsg_SplitScreenUser ) );

	// tell message handler to register known netmessages
	m_MessageHandler->ConnectionStart( this );
}

void CNetChan::ResetStreaming( void )
{
	m_SteamType = STREAM_CMD_NONE;
	m_StreamLength = 0;	
	m_StreamReceived = 0;
	m_StreamSeqNr = 0;
	m_SteamFile[0] = 0;	
}

bool CNetChan::StartStreaming( unsigned int challengeNr )
{
	// reset stream state machine
	ResetStreaming();

	m_ChallengeNr = challengeNr;
	
	if ( !NET_IsMultiplayer() )
	{
		m_StreamSocket = 0;
		return true;	// streaming is done via loopback buffers in SP mode
	}

	// Stream sockets only supported to ordinary IP address
	if ( !remote_address.IsType<netadr_t>() )
	{
		extern bool net_notcp;
		if ( !net_notcp )
			Warning( "Attempt to open streaming socket to remote host '%s'.  Stream sockets only supported to ordinary IPv4 addresses\n", ns_address_render( remote_address ).String() );
		return false;
	}

	MEM_ALLOC_CREDIT();

	m_StreamSocket = NET_ConnectSocket( m_Socket, remote_address.AsType<netadr_t>() );
	m_StreamData.EnsureCapacity( NET_MAX_PAYLOAD );

	return (m_StreamSocket != 0);
}

void CNetChan::SetChallengeNr(unsigned int chnr)
{
	m_ChallengeNr = chnr;
}

unsigned int CNetChan::GetChallengeNr( void )  const
{
	return m_ChallengeNr;
}

void CNetChan::GetSequenceData( int &nOutSequenceNr, int &nInSequenceNr, int &nOutSequenceNrAck )
{
	nOutSequenceNr = m_nOutSequenceNr;
	nInSequenceNr = m_nInSequenceNr;
	nOutSequenceNrAck = m_nOutSequenceNrAck;
}

void CNetChan::SetSequenceData( int nOutSequenceNr, int nInSequenceNr, int nOutSequenceNrAck )
{
	Assert( IsPlayback() );

	m_nOutSequenceNr = nOutSequenceNr;
	m_nInSequenceNr = nInSequenceNr;
	m_nOutSequenceNrAck = nOutSequenceNrAck;
}

void CNetChan::SetDemoRecorder(IDemoRecorder * recorder)
{
	m_DemoRecorder = recorder;
}

void CNetChan::SetTimeout(float seconds, bool bForceExact /*= false*/)
{
	m_Timeout = seconds;

	// Exact setting doesn't require validation
	if ( bForceExact )
		return;

	if ( m_Timeout > 3600.0f )
	{
		m_Timeout = 3600.0f; // 1 hour maximum
	} 
	else if ( m_Timeout <= 0.0f )
	{
		m_Timeout = -1.0f;	// never time out (demo files)
	}
	else if ( m_Timeout < CONNECTION_PROBLEM_TIME )
	{
		m_Timeout = CONNECTION_PROBLEM_TIME; // allow at least this minimum
	}
}

void CNetChan::SetMaxBufferSize(bool bReliable, int nBytes, bool bVoice )
{
	// force min/max sizes 4-96kB
	nBytes = clamp( nBytes, NET_MAX_DATAGRAM_PAYLOAD, NET_MAX_PAYLOAD );

	bf_write *stream;
	CUtlMemory<byte> *buffer;

	if ( bReliable )
	{
		stream = &m_StreamReliable;
		buffer = &m_ReliableDataBuffer;
	}
	else if ( bVoice == true )
	{
		stream = &m_StreamVoice;
		buffer = &m_VoiceDataBuffer;
	}
	else
	{
		stream = &m_StreamUnreliable;
		buffer = &m_UnreliableDataBuffer;
	}

	if ( buffer->Count() == nBytes )
		return;

	byte	*copybuf = NULL;
	int		copybits = stream->GetNumBitsWritten();
	int		copybytes = Bits2Bytes( copybits );

	if ( copybytes >= nBytes )
	{
		ConMsg("CNetChan::SetMaxBufferSize: cant preserve exiting data %i>%i.\n", copybytes, nBytes );
		return;
	}

	if ( copybits > 0 )
	{
		copybuf = new byte[ copybytes ];
		Q_memcpy( copybuf, buffer->Base(), copybytes );
	}

	buffer->Purge();

	MEM_ALLOC_CREDIT();
	buffer->EnsureCapacity( nBytes );

	if ( copybits > 0 )
	{
		Q_memcpy( buffer->Base(), copybuf, copybytes );
		delete [] copybuf;
		copybuf = NULL;
	}

	stream->StartWriting( buffer->Base(), nBytes, copybits );
	
}

void CNetChan::SetFileTransmissionMode( bool bBackgroundMode )
{
	m_bFileBackgroundTranmission = bBackgroundMode;
}

void CNetChan::SetCompressionMode( bool bUseCompression )
{
	m_bUseCompression = bUseCompression;
}

void CNetChan::SetDataRate(float rate)
{
	m_Rate = clamp( rate, MIN_RATE, MAX_RATE );
}

const char * CNetChan::GetName() const
{
	return m_Name;
}

const char * CNetChan::GetAddress() const
{
	return m_szRemoteAddressName;
}


int CNetChan::GetDropNumber() const
{
	return m_PacketDrop;
}

/*
===============
CNetChan::CanPacket

Returns true if the bandwidth choke isn't active
================
*/
bool CNetChan::CanPacket () const
{
	// Never choke loopback packets.
	if ( !net_chokeloopback.GetInt() && remote_address.IsLoopback() )
	{
		return true;
	}

	if ( HasQueuedPackets() )
	{
		return false;
	}

	return m_fClearTime < net_time;
}

bool CNetChan::IsPlayback( void ) const
{
#if !defined(DEDICATED)
	return demoplayer->IsPlayingBack();
#else
	return false;
#endif
}

void CNetChan::FlowReset( void )
{
	Q_memset( m_DataFlow, 0, sizeof( m_DataFlow ) );
	Q_memset( m_MsgStats, 0, sizeof( m_MsgStats ) );
}

void CNetChan::FlowNewPacket(int flow, int seqnr, int acknr, int nChoked, int nDropped, int nSize )
{
	netflow_t * pflow = &m_DataFlow[ flow ];
	
	// if frame_number != ( current + 1 ) mark frames between as invalid

	netframe_header_t *pframeheader = NULL;
	netframe_t *pframe = NULL;

	if ( seqnr > pflow->currentindex )
	{
		//
		// The following loop must execute no more than NET_FRAMES_BACKUP times
		// since that's the amount of storage in frame_headers & frames arrays,
		// a malformed client packet pushing "seqnr" by 1,000,000 can cause this
		// loop to watchdog.
		//
		for ( int i = pflow->currentindex + 1, numPacketFramesOverflow = 0;
			( i <= seqnr ) && ( numPacketFramesOverflow < NET_FRAMES_BACKUP );
			++ i, ++ numPacketFramesOverflow )
		{
			int nBackTrack = seqnr - i;
			
			pframeheader = &pflow->frame_headers[ i & NET_FRAMES_MASK ];
			pframe = &pflow->frames[ i & NET_FRAMES_MASK ];

			pframeheader->time = net_time;	// now
			pframeheader->valid = false;
			pframeheader->size = 0;
			pframeheader->latency = -1.0f; // not acknowledged yet
			pframe->avg_latency = GetAvgLatency( FLOW_OUTGOING );
			pframeheader->choked = 0; // not acknowledged yet
			pframe->dropped = 0;
			pframe->m_flInterpolationAmount = 0.0f;
			Q_memset( &pframe->msggroups, 0, sizeof(pframe->msggroups) );

			if ( nBackTrack < ( nChoked + nDropped ) )
			{
				if ( nBackTrack < nChoked )
				{
					pframeheader->choked = 1;
				}
				else
				{
					pframe->dropped = 1;
				}
			}
		}

		pframe->dropped = nDropped;
		pframeheader->choked = nChoked;
		pframeheader->size = nSize;
		pframeheader->valid = true;
		pframe->avg_latency = GetAvgLatency( FLOW_OUTGOING );
		pframe->m_flInterpolationAmount = m_flInterpolationAmount;
	}
	else
	{
#ifdef DEDICATED
		Assert( seqnr > pflow->currentindex );
#else
		Assert( demoplayer->IsPlayingBack() || seqnr > pflow->currentindex );
#endif
	}

	pflow->totalpackets++;
	pflow->currentindex = seqnr;
	pflow->currentframe = pframe;

	// updated ping for acknowledged packet

	int aflow = (flow==FLOW_OUTGOING) ? FLOW_INCOMING : FLOW_OUTGOING;

	if ( acknr <= (m_DataFlow[aflow].currentindex - NET_FRAMES_BACKUP) )
		return;	// acknowledged packet isn't in backup buffer anymore
	
	netframe_header_t * aframe = &m_DataFlow[aflow].frame_headers[ acknr & NET_FRAMES_MASK ];

	if ( aframe->valid && aframe->latency == -1.0f )
	{
		// update ping for acknowledged packet, if not already acknowledged before
		
		aframe->latency = net_time - aframe->time;

		if ( aframe->latency < 0.0f )
			aframe->latency = 0.0f;
	}
	
}

void CNetChan::FlowUpdate(int flow, int addbytes)
{
	netflow_t * pflow = &m_DataFlow[ flow ];
	pflow->totalbytes += addbytes;

	if ( pflow->nextcompute > net_time )
		return;

	pflow->nextcompute = net_time + FLOW_INTERVAL;

	int		totalvalid = 0;
	int		totalinvalid = 0;
	int		totalbytes = 0;
	float	totallatency = 0.0f;
	int		totallatencycount = 0;
	int		totalchoked = 0;

	float   starttime = FLT_MAX;
	float	endtime = 0.0f;
	netframe_header_t * pcurr;

	for ( int i = 0; i < NET_FRAMES_BACKUP; i++ )
	{
		// Most recent message then backward from there
		pcurr = &pflow->frame_headers[ i ];

		if ( pcurr->valid )
		{
			if ( pcurr->time < starttime )
				starttime = pcurr->time;

			if ( pcurr->time > endtime )
				endtime = pcurr->time;
		
			totalvalid++;
			totalchoked += pcurr->choked;
			totalbytes += pcurr->size;

			if ( pcurr->latency > -1.0f  )
			{
				totallatency += pcurr->latency; 
				totallatencycount++;
			}
		}
		else
		{
			totalinvalid++;
		}
	}

	float totaltime = endtime - starttime;

	if ( totaltime > 0.0f )
	{
		pflow->avgbytespersec *= FLOW_AVG;
		pflow->avgbytespersec += ( 1.0f - FLOW_AVG ) * ((float)totalbytes / totaltime);

		pflow->avgpacketspersec *= FLOW_AVG;
		pflow->avgpacketspersec += ( 1.0f - FLOW_AVG ) * ((float)totalvalid / totaltime);
	}

	int totalPackets = totalvalid + totalinvalid;
			
	if ( totalPackets > 0 )
	{
		pflow->avgloss *= FLOW_AVG;
		pflow->avgloss += ( 1.0f - FLOW_AVG ) * ((float)(totalinvalid-totalchoked)/totalPackets);
		if ( totalinvalid-totalchoked <= 0 )
			pflow->avgloss = 0; // snap loss to zero if nothing lost over last 128 ticks
		
		pflow->avgchoke *= FLOW_AVG;
		pflow->avgchoke += ( 1.0f - FLOW_AVG ) * ((float)totalchoked/totalPackets);
		if ( totalchoked <= 0 )
			pflow->avgchoke = 0; // snap choke to zero if nothing lost over last 128 ticks
	}
	else
	{
		pflow->avgloss = 0;
		pflow->avgchoke = 0;
	}
	
	if ( totallatencycount>0 )
	{
		float newping = totallatency / totallatencycount ;
		pflow->latency = newping;
		pflow->avglatency*= FLOW_AVG;
		pflow->avglatency += ( 1.0f - FLOW_AVG ) * newping;
	}
}

void CNetChan::SetChoked( void )
{
	m_nOutSequenceNr++;	// sends to be done since move command use sequence number
	m_nChokedPackets++;
}

bool CNetChan::Transmit(bool onlyReliable )
{
	if ( onlyReliable )
		m_StreamUnreliable.Reset();

	return (SendDatagram( NULL ) != 0);
}

bool CNetChan::IsFileInWaitingList( const char *filename )
{
	if ( !filename || !filename[0] )
		return true;

	for ( int stream=0; stream<MAX_STREAMS; stream++)
	{
		for ( int i = 0; i < m_WaitingList[stream].Count(); i++ )
		{
			dataFragments_t * data = m_WaitingList[stream][i]; 

			if ( !Q_strcmp( data->filename, filename ) )
				return true; // alread in list
		}
	}

	return false; // file not found
}

void CNetChan::RemoveHeadInWaitingList( int nList )
{
	Assert( m_WaitingList[nList].Count() );

	dataFragments_t * data = m_WaitingList[nList][0]; // get head

	if ( data->buffer )
		delete [] data->buffer;	// free data buffer

	if ( data->file	!= FILESYSTEM_INVALID_HANDLE )
	{
		g_pFileSystem->Close( data->file );
		data->file = FILESYSTEM_INVALID_HANDLE;
	}

	// data->fragments.Purge();

	m_WaitingList[nList].FindAndRemove( data );	// remove from list

	delete data;	//free structure itself
}


bool CNetChan::CreateFragmentsFromBuffer( bf_write *buffer, int stream )
{
	bf_write bfwrite;
	dataFragments_t *data = NULL;

	// if we have more than one item in the waiting list, try to add the 
	// reliable data to the last item. that doesn't work with the first item
	// since it may have been already send and is waiting for acknowledge

	int count = m_WaitingList[stream].Count();

	if ( count > 1 )
	{
		// get last item in waiting list
		data = m_WaitingList[stream][count-1];

		int totalBytes = Bits2Bytes( data->bits + buffer->GetNumBitsWritten() );

		totalBytes = PAD_NUMBER( totalBytes, 4 ); // align to 4 bytes boundary

		if ( totalBytes < NET_MAX_PAYLOAD && data->buffer )
		{
			// we have enough space for it, create new larger mem buffer
			char *newBuf = new char[totalBytes];

			Q_memcpy( newBuf, data->buffer, data->bytes );

			delete [] data->buffer; // free old buffer

			data->buffer = newBuf; // set new buffer

			bfwrite.StartWriting( newBuf, totalBytes, data->bits );
		}
		else
		{
			data = NULL; // reset to NULL
		}
	}

	// if not added to existing item, create a new reliable data waiting buffer
	if ( !data )
	{
		int totalBytes = Bits2Bytes( buffer->GetNumBitsWritten());

		totalBytes = PAD_NUMBER( totalBytes, 4 ); // align to 4 bytes boundary

		data = new dataFragments_t;
		data->bytes = 0;	// not filled yet
		data->bits = 0;
		data->buffer = new char[ totalBytes ];
		data->isCompressed = false;
		data->isReplayDemo = false;
		data->nUncompressedSize = 0;
		data->file = FILESYSTEM_INVALID_HANDLE;
		data->filename[0] = 0;
		
		bfwrite.StartWriting( data->buffer, totalBytes );

		m_WaitingList[stream].AddToTail( data );	// that's it for now
	}

	// write new reliable data to buffer
	bfwrite.WriteBits( buffer->GetData(), buffer->GetNumBitsWritten() );

	// fill last bits in last byte with NOP if necessary
	int nRemainingBits = bfwrite.GetNumBitsWritten() % 8;
	if ( nRemainingBits > 0 &&  nRemainingBits <= (8-NETMSG_TYPE_BITS) )
	{
		CNETMsg_NOP_t nop;
		nop.WriteToBuffer( bfwrite );
	}

	// update bit length
	data->bits += buffer->GetNumBitsWritten();
	data->bytes = Bits2Bytes(data->bits);
	
	// check if send as stream or with snapshot
	data->asTCP = m_StreamActive && ( data->bytes > m_MaxReliablePayloadSize );

	// calc number of fragments needed
	data->numFragments = BYTES2FRAGMENTS(data->bytes);
	data->ackedFragments = 0;
	data->pendingFragments = 0;

	return true;
}

bool CNetChan::CreateFragmentsFromFile( const char *filename, int stream, unsigned int transferID, bool bIsReplayDemoFile )
{
	if ( IsFileInWaitingList( filename ) )
		return true; // already scheduled for upload
	
	const char *pPathID = "GAME";

	if ( !g_pFileSystem->FileExists( filename, pPathID ) )
	{
		ConMsg( "CreateFragmentsFromFile: '%s' doesn't exist.\n", filename );
		return false;
	}

	int totalBytes = g_pFileSystem->Size( filename, pPathID );

	if ( totalBytes >= (net_maxfilesize.GetInt()*1024*1024) )
	{
		ConMsg( "CreateFragmentsFromFile: '%s' size exceeds net_maxfilesize limit (%i MB).\n", filename, net_maxfilesize.GetInt() );
		return false;
	}
	
	if ( totalBytes >= MAX_FILE_SIZE )
	{
		ConMsg( "CreateFragmentsFromFile: '%s' too big (max %i bytes).\n", filename, MAX_FILE_SIZE );
		return false;
	}

	dataFragments_t *data = new dataFragments_t;
	data->bytes = totalBytes;
	data->bits = data->bytes * 8;
	data->buffer = NULL;
	data->isCompressed = false;
	data->isReplayDemo = bIsReplayDemoFile;
	data->nUncompressedSize = 0;
	data->file = g_pFileSystem->Open( filename, "rb", pPathID );

	if ( data->file == FILESYSTEM_INVALID_HANDLE )
	{
		ConMsg( "CreateFragmentsFromFile: couldn't open '%s'.\n", filename );
		delete data;
		return false;
	}

	data->transferID = transferID;
	Q_strncpy( data->filename, filename, sizeof(data->filename) );
		
	m_WaitingList[stream].AddToTail( data );	// that's it for now

	// check if send as stream or with snapshot
	data->asTCP = false; // m_StreamActive && ( Bits2Bytes(data->length) > m_MaxReliablePayloadSize );

	// calc number of fragments needed
	data->numFragments = BYTES2FRAGMENTS(data->bytes);
	data->ackedFragments = 0;
	data->pendingFragments = 0;

	return true;
}

void CNetChan::SendTCPData( void )
{
	if ( m_WaitingList[FRAG_NORMAL_STREAM].Count() == 0 )
		return; // nothing in line

	dataFragments_t *data = m_WaitingList[FRAG_NORMAL_STREAM][0];

	if ( !data->asTCP )
		return; // not as TCP

	if ( data->pendingFragments > 0 )
		return; // already send, wait for ACK

	// OK send it now
	SendReliableViaStream( data );
}

bool CNetChan::SendSubChannelData( bf_write &buf )
{
	subChannel_s *subChan = NULL;
	int i;

	CompressFragments();

	SendTCPData();

	UpdateSubChannels();

	// find subchannl with data to send/resend:
	for ( i=0; i<MAX_SUBCHANNELS; i++ )
	{
		subChan = &m_SubChannels[i];

		if ( subChan->state == SUBCHANNEL_TOSEND )
			break;
	}

	if ( i == MAX_SUBCHANNELS )
		return false; // no data to send in any subchannel

	// first write subchannel index
	buf.WriteUBitLong( i, 3 );

	// write fragemnts for both streams
	for ( i=0; i<MAX_STREAMS; i++ )
	{
		if ( subChan->numFragments[i] == 0 )
		{
			buf.WriteOneBit( 0 ); // no data for this stream
			continue;
		}

		dataFragments_t *data = m_WaitingList[i][0];

		buf.WriteOneBit( 1 ); // data follows:

		unsigned int offset = subChan->startFraggment[i]*FRAGMENT_SIZE;
		unsigned int length = subChan->numFragments[i]*FRAGMENT_SIZE;

		if ( (subChan->startFraggment[i]+subChan->numFragments[i]) == data->numFragments )
		{
			// we are sending the last fragment, adjust length
			int rest = FRAGMENT_SIZE - ( data->bytes % FRAGMENT_SIZE );
			if ( rest < FRAGMENT_SIZE )
				length -= rest;
		}

		// if all fragments can be send within a single packet, avoid overhead (if not a file)
		bool bSingleBlock = (subChan->numFragments[i] == data->numFragments) &&
							 ( data->file == FILESYSTEM_INVALID_HANDLE );

		if ( bSingleBlock )
		{	
			Assert( length == data->bytes );
			Assert( length < NET_MAX_PAYLOAD );
			Assert( offset == 0 );

			buf.WriteOneBit( 0 );	// single block bit

			// data compressed ?
			if ( data->isCompressed )
			{
				buf.WriteOneBit( 1 );
				buf.WriteUBitLong( data->nUncompressedSize, MAX_FILE_SIZE_BITS );
			}
			else
			{
				buf.WriteOneBit( 0 ); 
			}

			buf.WriteUBitLong( data->bytes, NET_MAX_PAYLOAD_BITS );
		}
		else
		{
			buf.WriteOneBit( 1 ); // uses fragments with start fragment offset byte
			buf.WriteUBitLong( subChan->startFraggment[i], (MAX_FILE_SIZE_BITS-FRAGMENT_BITS) ); 
			buf.WriteUBitLong( subChan->numFragments[i], 3 ); 
		
			if ( offset == 0 )
			{
				// this is the first fragment, write header info
				
				if ( data->file != FILESYSTEM_INVALID_HANDLE )
				{
					buf.WriteOneBit( 1 ); // file transmission net message stream
					buf.WriteUBitLong( data->transferID, 32 );
					buf.WriteString( data->filename );

					if ( data->isReplayDemo )
					{
						buf.WriteOneBit( 1 );
					}
					else
					{
						buf.WriteOneBit( 0 );
					}
				}
				else
				{
					buf.WriteOneBit( 0 ); // normal net message stream
				}

				// data compressed ?
				if ( data->isCompressed )
				{
					buf.WriteOneBit( 1 );
					buf.WriteUBitLong( data->nUncompressedSize, MAX_FILE_SIZE_BITS );
				}
				else
				{
					buf.WriteOneBit( 0 ); 
				}

				buf.WriteUBitLong( data->bytes, MAX_FILE_SIZE_BITS ); // 4MB max for files
			}
		}

		// write fragments to buffer
		if ( data->buffer )
		{
			Assert( data->file == FILESYSTEM_INVALID_HANDLE );
			// send from memory block
			buf.WriteBytes( data->buffer+offset, length );
		}
		else // if ( data->file != FILESYSTEM_INVALID_HANDLE )
		{
			// send from file
			Assert( data->file != FILESYSTEM_INVALID_HANDLE );
			char *tmpbuf = new char[ MAX( length, 1 ) ]; // alloc on stack
			g_pFileSystem->Seek( data->file, offset, FILESYSTEM_SEEK_HEAD );
			g_pFileSystem->Read( tmpbuf, length, data->file );
			buf.WriteBytes( tmpbuf, length );
			delete[] tmpbuf;
		}

		if ( net_showfragments.GetBool() )
		{
			ConMsg("Sending subchan %i: start %i, num %i\n", subChan->index, subChan->startFraggment[i], subChan->numFragments[i] );
		}

		subChan->sendSeqNr = m_nOutSequenceNr;
		subChan->state = SUBCHANNEL_WAITING;
	}

	return true;
}


bool CNetChan::ReadSubChannelData( bf_read &buf, int stream  )
{
	dataFragments_t * data = &m_ReceiveList[stream]; // get list
	int startFragment = 0;
	int numFragments = 0;
	unsigned int offset = 0;
	unsigned int length = 0;
	
	bool bSingleBlock = buf.ReadOneBit() == 0; // is single block ?

	if ( !bSingleBlock )
	{
		startFragment = buf.ReadUBitLong( MAX_FILE_SIZE_BITS-FRAGMENT_BITS ); // 16 MB max
		numFragments = buf.ReadUBitLong( 3 );  // 8 fragments per packet max
		offset = startFragment * FRAGMENT_SIZE;
		length = numFragments * FRAGMENT_SIZE;
	}

	if ( offset == 0 ) // first fragment, read header info
	{
		data->filename[0] = 0;
		data->isCompressed = false;
		data->isReplayDemo = false;
		data->transferID = 0;

		if ( bSingleBlock )
		{
			// data compressed ?
			if ( buf.ReadOneBit() )
			{
				data->isCompressed = true;
				data->nUncompressedSize = buf.ReadUBitLong( MAX_FILE_SIZE_BITS );
			}
			else
			{
				data->isCompressed = false;
			}

			data->bytes = buf.ReadUBitLong( NET_MAX_PAYLOAD_BITS );
		}
		else
		{
		
			if ( buf.ReadOneBit() ) // is it a file ?
			{
				data->transferID = buf.ReadUBitLong( 32 );
				buf.ReadString( data->filename, MAX_OSPATH );

				// replay demo?
				if ( buf.ReadOneBit() )
				{
					data->isReplayDemo = true;
				}
			}

			// data compressed ?
			if ( buf.ReadOneBit() )
			{
				data->isCompressed = true;
				data->nUncompressedSize = buf.ReadUBitLong( MAX_FILE_SIZE_BITS );
			}
			else
			{
				data->isCompressed = false;
			}
				
			data->bytes = buf.ReadUBitLong( MAX_FILE_SIZE_BITS );
		}

		if ( data->buffer )
		{
			// last transmission was aborted, free data
			delete [] data->buffer;
			data->buffer = NULL;
			ConDMsg("Fragment transmission aborted at %i/%i from %s.\n", data->ackedFragments, data->numFragments, GetAddress() );
		}

		data->bits = data->bytes * 8; 
		data->asTCP = false;
		data->numFragments = BYTES2FRAGMENTS(data->bytes);
		data->ackedFragments = 0;
		data->file = FILESYSTEM_INVALID_HANDLE;

		if ( bSingleBlock )
		{
			numFragments = data->numFragments;
			length = numFragments * FRAGMENT_SIZE;
		}

		if ( data->bytes > MAX_FILE_SIZE )
		{
			// This can happen with the compressed path above, which uses VarInt32 rather than MAX_FILE_SIZE_BITS
			Warning( "Net message exceeds max size (%u / %u)\n", MAX_FILE_SIZE, data->bytes );
			// Subsequent packets for this transfer will treated as invalid since we never setup a buffer.
			return false;
		}

		if ( data->isCompressed && data->nUncompressedSize > MAX_FILE_SIZE )
		{
			// This can happen with the compressed path above, which uses VarInt32 rather than MAX_FILE_SIZE_BITS
			Warning( "Net message uncompressed size exceeds max size (%u / compressed %u / uncompressed %u)\n", MAX_FILE_SIZE, data->bytes, data->nUncompressedSize );
			// Subsequent packets for this transfer will treated as invalid since we never setup a buffer.
			return false;
		}

		data->buffer = new char[ PAD_NUMBER( data->bytes, 4 ) ];
	}
	else
	{
		if ( data->buffer == NULL )
		{
			// This can occur if the packet containing the "header" (offset == 0) is dropped.  Since we need the header to arrive we'll just wait
			//  for a retry
			// ConDMsg("Received fragment out of order: %i/%i\n", startFragment, numFragments );
			return false;
		}
	}
	
	if ( (startFragment+numFragments) == data->numFragments )
	{
		// we are receiving the last fragment, adjust length
		int rest = FRAGMENT_SIZE - ( data->bytes % FRAGMENT_SIZE );
		if ( rest < FRAGMENT_SIZE )
			length -= rest;
	}
	else if ( (startFragment+numFragments) > data->numFragments )
	{
		// a malicious client can send a fragment beyond what was arranged in fragment#0 header
		// old code will overrun the allocated buffer and likely cause a server crash
		// it could also cause a client memory overrun because the offset can be anywhere from 0 to 16MB range
		// drop the packet and wait for client to retry
		ConDMsg("Received fragment chunk out of bounds: %i+%i>%i from %s\n", startFragment, numFragments, data->numFragments, GetAddress() );
		return false;
	}

	Assert ( (offset + length) <= data->bytes );

	buf.ReadBytes( data->buffer + offset, length ); // read data

	data->ackedFragments+= numFragments;

	if ( net_showfragments.GetBool() )
		ConMsg("Received fragments: start %i, num %i\n", startFragment, numFragments );

	return true;
}

void CNetChan::UpdateSubChannels()
{
	// first check if there is a free subchannel
	subChannel_s * freeSubChan = GetFreeSubChannel();

	if ( freeSubChan == NULL )
		return; //all subchannels in use right now

	int i, nSendMaxFragments = m_MaxReliablePayloadSize / FRAGMENT_SIZE;

	bool bSendData = false;

	for ( i = 0; i < MAX_STREAMS; i++ )
	{
		if ( m_WaitingList[i].Count() <= 0 )
			continue;

		dataFragments_s *data = m_WaitingList[i][0]; // get head

		if ( data->asTCP )
			continue;

		int nSentFragments = data->ackedFragments + data->pendingFragments;

		Assert( nSentFragments <= data->numFragments );

		if ( nSentFragments == data->numFragments )
			continue; // all fragments already send

		// how many fragments can we send ?

		int numFragments = MIN( nSendMaxFragments, data->numFragments - nSentFragments );

		// if we are in file background transmission mode, just send one fragment per packet
		if ( i == FRAG_FILE_STREAM && m_bFileBackgroundTranmission )
			numFragments = MIN( 1, numFragments );

		// copy fragment data into subchannel

		freeSubChan->startFraggment[i] = nSentFragments;
		freeSubChan->numFragments[i] = numFragments;
		
		data->pendingFragments += numFragments;

		bSendData = true;

		nSendMaxFragments -= numFragments;

		if ( nSendMaxFragments <= 0 )
			break;
	}

	if ( bSendData )
	{
		// flip channel bit 
		int bit = 1<<freeSubChan->index;

		FLIPBIT(m_nOutReliableState, bit);

		freeSubChan->state = SUBCHANNEL_TOSEND;
		freeSubChan->sendSeqNr = 0;
	}
}

#if 1

unsigned short BufferToShortChecksum( const void *pvData, size_t nLength )
{
	CRC32_t crc = CRC32_ProcessSingleBuffer( pvData, nLength );

	unsigned short lowpart = ( crc & 0xffff );
	unsigned short highpart = ( ( crc >> 16 ) & 0xffff );

	return (unsigned short)( lowpart ^ highpart );
}

#else

// If the CRC version ever is deemed to expensive, here's a quick xor version.
//  It's probably not super robust.
inline unsigned short BufferToShortChecksum( const void *pvData, size_t nSize )
{
	const uint32 *pData = (const uint32 *)pvData;

	unsigned short us = 0;
	while ( nSize >= sizeof( uint32 ) )
	{
		us ^= ( *pData & 0xffff );
		us ^= ( ( *pData >> 16 ) & 0xffff );

		nSize -= sizeof( uint32 );
		pData += sizeof( uint32 );
	}

	const byte *pbData = (const byte *)pData;

	while ( nSize > 0 )
	{
		us ^= *pbData;
		++pbData;
		--nSize;
	}

	return us;
}

#endif

#define MIN_ROUTABLE_TESTING

#if defined( _DEBUG ) || defined( MIN_ROUTABLE_TESTING )
static ConVar net_minroutable( "net_minroutable", "16", FCVAR_DEVELOPMENTONLY, "Forces larger payloads." );
#endif

// XBox live requires appending voice data in the clear after game payload
static bool ShouldSendVoiceInTheClear()
{
	if ( IsX360() )
		return true;
	if ( NET_IsDedicatedForXbox() )
		return true;
	return false;
}

/*
===============
CNetChan::TransmitBits

tries to send an unreliable message to a connection, and handles the
transmition / retransmition of the reliable messages.

A 0 length will still generate a packet and deal with the reliable messages.
================
*/
int CNetChan::SendDatagram(bf_write *datagram)
{
	VPROF( "CNetChan::SendDatagram" );
	byte		send_buf[ NET_MAX_MESSAGE ];

	// Make sure for the client that the max routable payload size is up to date
	if ( m_Socket == NS_CLIENT )
	{
		if ( net_maxroutable.GetInt() != GetMaxRoutablePayloadSize() )
		{
			SetMaxRoutablePayloadSize( net_maxroutable.GetInt() );
		}
	}

	// first increase out sequence number
	
	// check, if fake client, then fake send also
	if ( remote_address.IsNull() )	
	{
		// this is a demo channel, fake sending all data
		m_fClearTime = 0.0;		// no bandwidth delay
		m_nChokedPackets = 0;	// Reset choke state
		m_StreamReliable.Reset();		// clear current reliable buffer
		m_StreamUnreliable.Reset();		// clear current unrelaible buffer
		m_nOutSequenceNr++;
		return m_nOutSequenceNr-1;
	}
	// If we have "split" users hanging off of us, we need to merge their data in now
	SplitUserCombineForSending();

	// process all new and pending reliable data, return true if reliable data should
	// been send with this packet

	if ( m_StreamReliable.IsOverflowed() )
	{
		ConMsg ("%s:send reliable stream overflow\n" , GetAddress() );
		return 0;
	}
	else if ( m_StreamReliable.GetNumBitsWritten() > 0 )
	{
		CreateFragmentsFromBuffer( &m_StreamReliable, FRAG_NORMAL_STREAM );
		m_StreamReliable.Reset();
	}

	bf_write send( "CNetChan_TransmitBits->send", send_buf, sizeof(send_buf) );

	// Prepare the packet header
	// build packet flags
	unsigned char flags = 0;

	// start writing packet

	send.WriteLong ( m_nOutSequenceNr );
	send.WriteLong ( m_nInSequenceNr );

	bf_write flagsPos = send; // remember flags byte position

	send.WriteByte ( 0 ); // write correct flags value later
	if ( ShouldChecksumPackets() )
	{
		send.WriteShort( 0 );  // write correct checksum later
		Assert( !(send.GetNumBitsWritten() % 8 ) );
	}

	// Note, this only matters on the PC
	int nCheckSumStart = send.GetNumBytesWritten();

	send.WriteByte ( m_nInReliableState );

	if ( m_nChokedPackets > 0 )
	{
		flags |= PACKET_FLAG_CHOKED;
		send.WriteByte ( m_nChokedPackets & 0xFF );	// send number of choked packets
	}

	if ( SendSubChannelData( send ) )
	{
		flags |= PACKET_FLAG_RELIABLE;
	}


	// Is there room for given datagram data. the datagram data 
	// is somewhat more important than the normal unreliable data
	// this is done to allow some kind of snapshot behavior
	// weather all data in datagram is transmitted or none.
	if ( datagram )
	{
		if( datagram->GetNumBitsWritten() < send.GetNumBitsLeft() )
		{
			send.WriteBits( datagram->GetData(), datagram->GetNumBitsWritten() );
		}
		else
		{
			ConDMsg("CNetChan::SendDatagram:  data would overfow, ignoring\n");
		}
	}

	// Is there room for the unreliable payload?
	if ( m_StreamUnreliable.GetNumBitsWritten() < send.GetNumBitsLeft() )
	{
		send.WriteBits(m_StreamUnreliable.GetData(), m_StreamUnreliable.GetNumBitsWritten() );
	}
	else
	{
		ConDMsg("CNetChan::SendDatagram:  Unreliable would overfow, ignoring\n");
	}

	m_StreamUnreliable.Reset();	// clear unreliable data buffer

	// On the PC the voice data is in the main packet
	if ( !ShouldSendVoiceInTheClear() && 
		m_StreamVoice.GetNumBitsWritten() > 0 && m_StreamVoice.GetNumBitsWritten() < send.GetNumBitsLeft() )
	{
		send.WriteBits(m_StreamVoice.GetData(), m_StreamVoice.GetNumBitsWritten() );
		m_StreamVoice.Reset();
	}

	int nMinRoutablePayload = MIN_ROUTABLE_PAYLOAD;

#if defined( _DEBUG ) || defined( MIN_ROUTABLE_TESTING )
	if ( m_Socket == NS_SERVER )
	{
		nMinRoutablePayload = net_minroutable.GetInt();
	}
#endif

	// Deal with packets that are too small for some networks
	while ( send.GetNumBytesWritten() < nMinRoutablePayload )		
	{
		// Go ahead and pad some bits as long as needed
		CNETMsg_NOP_t nop;
		nop.WriteToBuffer( send );
	}

	// Make sure we have enough bits to read a final net_NOP opcode before compressing 
	int nRemainingBits = send.GetNumBitsWritten() % 8;
	if ( nRemainingBits > 0 &&  nRemainingBits <= (8-NETMSG_TYPE_BITS) )
	{
		CNETMsg_NOP_t nop;
		nop.WriteToBuffer( send );
	}

	// if ( IsX360() )
	{
		// Now round up to byte boundary
		nRemainingBits = send.GetNumBitsWritten() % 8;
		if ( nRemainingBits > 0 )
		{
			int nPadBits = 8 - nRemainingBits;

			flags |= ENCODE_PAD_BITS( nPadBits );
	
			// Pad with ones
			if ( nPadBits > 0 )
			{
				unsigned int unOnes = GetBitForBitnum( nPadBits ) - 1;
				send.WriteUBitLong( unOnes, nPadBits );
			}
		}
	}


	int nBitsPerPayload = net_blocksize.GetInt() * 8;

	// FIXME:  This isn't actually correct since compression might make the main payload usage a bit smaller
	// On 360, only add voice data if the packet isn't going to be split
	bool bSendVoice = ShouldSendVoiceInTheClear() && ( m_StreamVoice.GetNumBitsWritten() > 0 && m_StreamVoice.GetNumBitsWritten() + send.GetNumBitsWritten() < nBitsPerPayload );
	bool bClearVoice = ( bSendVoice || m_StreamVoice.GetNumBitsWritten() >= nBitsPerPayload );
		
	bool bCompress = false;
	if ( net_compresspackets.GetBool() )
	{
		if ( send.GetNumBytesWritten() >= net_compresspackets_minsize.GetInt() )
		{
			bCompress = true;
		}
	}

	// write correct flags value and the checksum
	flagsPos.WriteByte( flags ); 

	// Compute checksum (must be aligned to a byte boundary!!)
	if ( ShouldChecksumPackets() )
	{
		const void *pvData = send.GetData() + nCheckSumStart;
		Assert( !(send.GetNumBitsWritten() % 8 ) );
		int nCheckSumBytes = send.GetNumBytesWritten() - nCheckSumStart;
		unsigned short usCheckSum = BufferToShortChecksum( pvData, nCheckSumBytes );

		flagsPos.WriteUBitLong( usCheckSum, 16 );
	}

	// Send the datagram
	int	bytesSent = NET_SendPacket ( this, m_Socket, remote_address, send.GetData(), send.GetNumBytesWritten(), bSendVoice ? &m_StreamVoice : 0, bCompress );

	if ( bClearVoice || !ShouldSendVoiceInTheClear() )
	{
		m_StreamVoice.Reset();
	}

	if ( net_showudp.GetInt() && net_showudp.GetInt() != 2 &&
		( !net_showudp_remoteonly.GetBool() || !( remote_address.IsLocalhost() || remote_address.IsLoopback() ) ) )
	{
		int mask = 63;
		char comp[ 64 ] = { 0 };
		if ( net_compresspackets.GetBool() && 
			bytesSent && 
			( bytesSent < send.GetNumBytesWritten() ) )
		{
			Q_snprintf( comp, sizeof( comp ), " compression=%5u [%5.2f %%]", bytesSent, 100.0f * float( bytesSent ) / float( send.GetNumBytesWritten() ) );
		}
	
		char desc[ 128 ];
		uint64 steamID = g_pSteamSocketMgr->GetSteamIDForRemote( remote_address );
		Color clr( 0, 200, 255, 255 );
		if ( steamID != 0ull )
		{
			clr = Color( 255, 255, 100, 255 );
			Q_snprintf( desc, sizeof( desc ), "%12.12s %21.21s s(%llx)", GetName(), GetAddress(), steamID );
		}
		else
		{
			Q_snprintf( desc, sizeof( desc ), "%12.12s %21.21s", GetName(), GetAddress() );
		}

		ConColorMsg ( clr, "UDP -> %s: sz=%5i seq=%5i ack=%5i rel=%1i tm=%8.3f%s\n"
			, desc
			, send.GetNumBytesWritten()
			, ( m_nOutSequenceNr ) & mask
			, m_nInSequenceNr & mask
			, (flags & PACKET_FLAG_RELIABLE) ? 1 : 0
			, (float)net_time
			, comp );
	}

	// update stats

	int nTotalSize = bytesSent + UDP_HEADER_SIZE;

	FlowNewPacket( FLOW_OUTGOING, m_nOutSequenceNr, m_nInSequenceNr, m_nChokedPackets, 0, nTotalSize );

	FlowUpdate( FLOW_OUTGOING, nTotalSize );
	
	if ( m_fClearTime < net_time )
	{
		m_fClearTime = net_time;
	}

	// calculate net_time when channel will be ready for next packet (throttling)
	// TODO:  This doesn't exactly match size sent when packet is a "split" packet (actual bytes sent is higher, etc.)
	double fAddTime = (float)nTotalSize / (float)m_Rate;

	m_fClearTime += fAddTime;

	if ( net_maxcleartime.GetFloat() > 0.0f )
	{
		double m_flLatestClearTime = net_time + net_maxcleartime.GetFloat();
		if ( m_fClearTime > m_flLatestClearTime )
		{
			m_fClearTime = m_flLatestClearTime;
		}
	}
	
	m_nChokedPackets = 0;
	m_nOutSequenceNr++;

	return m_nOutSequenceNr-1; // return send seq nr
}

bool CNetChan::NETMsg_NOP( const CNETMsg_NOP& msg )
{
	return true;
}

bool CNetChan::NETMsg_Disconnect( const CNETMsg_Disconnect& msg )
{
#ifdef DEDICATED
	m_MessageHandler->ConnectionClosing( "Disconnect" );
#else
	m_MessageHandler->ConnectionClosing( msg.text().c_str() );
#endif
	return false;
}

bool CNetChan::NETMsg_File( const CNETMsg_File& msg )
{
	const char *string = msg.file_name().c_str();

	if ( !msg.deny() && IsValidFileForTransfer( string ) )
	{
		m_MessageHandler->FileRequested( string, msg.transfer_id(), msg.is_replay_demo_file() );
	}
	else
	{
		m_MessageHandler->FileDenied( string, msg.transfer_id(), msg.is_replay_demo_file() );
	}

	return true;
}

bool CNetChan::NETMsg_SplitScreenUser( const CNETMsg_SplitScreenUser& msg )
{
	return m_MessageHandler->ChangeSplitscreenUser( msg.slot() );
}

bool CNetChan::WasLastMessageReliable() const
{
	return m_bWasLastMessageReliable;
}

const unsigned char * CNetChan::GetChannelEncryptionKey() const
{
	extern int32 GetHostVersion();
	static const uint32 unHostVersion = GetHostVersion();
	static const byte ubDefaultEncryptionKey[ NET_CRYPT_KEY_LENGTH ] = {
		'C', 'S', 'G', 'O',
		byte( ( unHostVersion >> 0 ) & 0xFF ), byte( ( unHostVersion >>  8 ) & 0xFF ), byte( ( unHostVersion >> 16 ) & 0xFF ), byte( ( unHostVersion >> 24 ) & 0xFF ),
		byte( ( unHostVersion >> 2 ) & 0xFF ), byte( ( unHostVersion >> 10 ) & 0xFF ), byte( ( unHostVersion >> 18 ) & 0xFF ), byte( ( unHostVersion >> 26 ) & 0xFF ),
		byte( ( unHostVersion >> 4 ) & 0xFF ), byte( ( unHostVersion >> 12 ) & 0xFF ), byte( ( unHostVersion >> 20 ) & 0xFF ), byte( ( unHostVersion >> 28 ) & 0xFF ),
	};
	return m_EncryptionKey.NumAllocated() ? m_EncryptionKey.Base() : ubDefaultEncryptionKey;
}

bool CNetChan::ProcessMessages( bf_read &buf, bool wasReliable  )
{
	MDLCACHE_CRITICAL_SECTION( );

	// For split screen support
	m_pActiveChannel = this;
	return _ProcessMessages( buf, wasReliable );
	// Can't safely put code here because delete this could have occurred!!!
}

bool CNetChan::_ProcessMessages( bf_read &buf, bool wasReliable  )
{
	VPROF( "CNetChan::ProcessMessages" );
#if defined( NET_PARANOID_DUMPS )
	CNetchanParanoidMode *pParanoid = &g_NetParanoid;
#endif
	m_bStopProcessing = false;

	const char * showmsgname = net_showmsg.GetString();
	const char * blockmsgname = net_blockmsg.GetString();

	if ( showmsgname[ 0 ] == '0' )
	{
		showmsgname = NULL;	// dont do strcmp all the time
	}

	if ( blockmsgname[ 0 ] == '0' )
	{
		blockmsgname = NULL;	// dont do strcmp all the time
	}

	if ( net_showpeaks.GetInt() > 0 && net_showpeaks.GetInt() < buf.GetNumBytesLeft() )
	{
		showmsgname = "1";	// show messages for this packet only
	}

	bf_read democopy = buf; // create a copy of reading buffer state for demo recording
	
	m_bWasLastMessageReliable = wasReliable;

	int startbit = buf.GetNumBitsRead();
	
	while ( true )
	{
		if ( buf.IsOverflowed() )
		{
			Warning( "ProcessMessages %s: incoming buffer overflow!\n", GetAddress() );
			m_MessageHandler->ConnectionCrashed( "Buffer overflow in net message" );
			return false;
		}	

		// Are we at the end?
		if ( buf.GetNumBitsLeft() < 8 ) // Minimum bits for message header encoded using VarInt32
		{
			break;
		}

		unsigned char cmd = buf.ReadVarInt32();

		// see if we have a registered message object for this type
		INetMessageBinder *pMsgBind = ((CNetChan *)m_pActiveChannel)->FindMessageBinder( cmd, 0 );
		if ( pMsgBind )
		{
			int startbit = buf.GetNumBitsRead();
				
			INetMessage	* netmsg = pMsgBind->CreateFromBuffer( buf );
			if ( !netmsg )
			{
				Msg( "Netchannel: failed reading message %d from %s.\n", cmd, GetAddress() );
				Assert ( 0 );
				return false;
			}

			netmsg->SetReliable( wasReliable );

			UpdateMessageStats( netmsg->GetGroup(), buf.GetNumBitsRead() - startbit );

			if ( showmsgname )
			{
				if ( (*showmsgname == '1') || !Q_stricmp(showmsgname, netmsg->GetName() ) )
				{
					Msg("Msg from %s: %s\n", GetAddress(), netmsg->GetName() );
					Msg("%s\n", netmsg->ToString() );
				}
			}

			if ( blockmsgname )
			{
				if ( (*blockmsgname== '1') || !Q_stricmp(blockmsgname, netmsg->GetName() ) )
				{
					Msg("Blocking message %s\n", netmsg->ToString() );
					continue;
				}
			}

			int iMsgHandler = 1;
			do
			{
				// netmessage calls the Process function that was registered by it's MessageHandler
				m_bProcessingMessages = true;
				bool bRet = pMsgBind->Process( *netmsg );
				m_bProcessingMessages = false;

				// This means we were deleted during the processing of that message.
				if ( m_bShouldDelete )
				{
					delete netmsg;
					delete this;
					return false;
				}

				// This means our message buffer was freed or invalidated during the processing of that message.
				if ( m_bStopProcessing )
				{
					delete netmsg;
					return false;
				}

				if ( !bRet )
				{
					ConDMsg( "Netchannel: failed processing message %s.\n", netmsg->GetName() );
					Assert ( 0 );
					delete netmsg;
					return false;
				}

				if ( IsOverflowed() )
				{
					delete netmsg;
					return false;
				}

				// Because we are moving to another net message, we have to clone it
				pMsgBind = ( ( CNetChan * )m_pActiveChannel )->FindMessageBinder( cmd, iMsgHandler++ );
			} while( pMsgBind );

			delete netmsg;
		}
		else
		{
			ConMsg( "Netchannel: unknown net message (%i) from %s.\n", cmd, GetAddress() );
			Assert ( 0 );
#if defined( NET_PARANOID_DUMPS )
			pParanoid->Dump( this, "unknown message" );
#endif
			return false;
		}
	}

#if !defined(DEDICATED)
	// all messages could be parsed, write packet to demo file
	if ( m_DemoRecorder && !demoplayer->IsPlayingBack() )
	{
		// only record if any message was paresd
		m_DemoRecorder->RecordMessages( democopy, buf.GetNumBitsRead() - startbit );
	}
#endif

#if defined( NET_PARANOID_DUMPS )

	int level = net_dumptest.GetInt();
	if ( level > 0 )
	{
		if (level == 2 )
		{
			// Crash
#if defined( _X360 )
			XBX_CrashDump( false );
#else
			byte *p = 0;
			*p = 0x1;
#endif
		}
		else
		{
			net_dumptest.SetValue( 0 );
			pParanoid->Dump( this, "requested from console" );
		}
	}
#endif

	return true; // ok fine
}

bool CNetChan::SetActiveChannel( INetChannel *pNewChannel )
{
	m_pActiveChannel = static_cast< CNetChan * >( pNewChannel );
	Assert( m_pActiveChannel );
	return m_pActiveChannel ? true : false;
}

void CNetChan::ProcessPlayback( void )
{
#if !defined(DEDICATED)
	netpacket_t * packet;

	while ( ( packet = demoplayer->ReadPacket() ) != NULL )
	{
		// Update data flow stats
		FlowNewPacket( FLOW_INCOMING, m_nInSequenceNr, m_nOutSequenceNrAck, 0, 0, packet->wiresize );

		last_received = net_time;

		m_MessageHandler->PacketStart( m_nInSequenceNr, m_nOutSequenceNrAck );
		
		if ( ProcessMessages( packet->message, true ) )
		{
			m_MessageHandler->PacketEnd();
		}
		else
		{
			break;
		}
	}
#endif
}

CNetChan::subChannel_s *CNetChan::GetFreeSubChannel()
{
	for ( int i=0; i<MAX_SUBCHANNELS; i++ )
	{
		if ( m_SubChannels[i].state == SUBCHANNEL_FREE )
			return &m_SubChannels[i];
	}

	return NULL;
}

void CNetChan::CheckWaitingList(int nList)
{
	// go thru waiting lists and mark fragments send with this seqnr packet
	if ( m_WaitingList[nList].Count() == 0 || m_nOutSequenceNrAck <= 0 )
		return; // no data in list

	dataFragments_t *data = m_WaitingList[nList][0]; // get head

	if ( data->ackedFragments == data->numFragments )
	{
		// all fragmenst were send successfully
		if ( net_showfragments.GetBool() )
			ConMsg("Sending complete: %i fragments, %i bytes.\n", data->numFragments, data->bytes );

#if defined( REPLAY_ENABLED )
		// if it's a replay demo, notify the replay system
		if ( data->isReplayDemo )
		{
			Replay_OnFileSendComplete( data->filename, data->bytes );
		}
#endif

		RemoveHeadInWaitingList( nList );

		return;
	}
	else if ( data->ackedFragments > data->numFragments )
	{
		//ConMsg("CheckWaitingList: invalid acknowledge fragments %i/%i.\n", data->ackedFragments, data->numFragments );
	}
	// else: still pending fragments
}

bool CNetChan::CheckReceivingList(int nList)
{
	dataFragments_t * data = &m_ReceiveList[nList]; // get list
	
	if ( data->buffer == NULL )
		return true;

	if ( data->ackedFragments < data->numFragments )
		return true;

	if ( data->ackedFragments > data->numFragments )
	{
		ConMsg("Receiving failed: too many fragments %i/%i from %s\n", data->ackedFragments, data->numFragments, GetAddress() );
		return false;
	}

	// got all fragments

	if ( net_showfragments.GetBool() )
		ConMsg("Receiving complete: %i fragments, %i bytes\n", data->numFragments, data->bytes );

	if ( data->isCompressed )
	{
		UncompressFragments( data );
	}

	if ( !data->filename[0] )
	{
		bf_read buffer( data->buffer, data->bytes );

		if ( !ProcessMessages( buffer, true ) ) // parse net message
		{
			return false; // stop reading any further
		}
	}
	else
	{
		static ConVar *s_pAllowUpload = NULL;

		if ( !s_pAllowUpload )
		{
			s_pAllowUpload = g_pCVar->FindVar( "sv_allowupload" );
		}

		if ( s_pAllowUpload && s_pAllowUpload->GetBool() )
		{

			// Make sure that this file is not being written to a location above the current directory, isn't in 
			// writing to any locations we don't want, isn't an unsupported 
			if ( CNetChan::IsValidFileForTransfer( data->filename ) )
			{
				// CSGO: FileExists will fail with an invalid write path, but writing files will default to the game dir, which 
				// lets servers stomp existing files. Use default write path for the below calls (this is the same as the fastdl path done in
				// download.h/cpp
				const char *pszPathID = NULL;

				// we received a file, write it to disk and notify host
				if ( !g_pFileSystem->FileExists( data->filename, pszPathID ) )
				{

					// Make sure path exists
					char szParentDir[ MAX_PATH ];
					if ( !V_ExtractFilePath( data->filename, szParentDir, sizeof(szParentDir) ) )
					{
						szParentDir[0] = '\0';
					}
					g_pFileSystem->CreateDirHierarchy( szParentDir, pszPathID );

					// open new file for write binary
					data->file = g_pFileSystem->Open( data->filename, "wb", pszPathID );

					if ( FILESYSTEM_INVALID_HANDLE != data->file )
					{
						g_pFileSystem->Write( data->buffer, data->bytes, data->file );
						g_pFileSystem->Close( data->file );

						if ( net_showfragments.GetInt() == 2 )
						{
							DevMsg("FileReceived: %s, %i bytes (ID %i)\n", data->filename, data->bytes, data->transferID );
						}

						m_MessageHandler->FileReceived( data->filename, data->transferID, data->isReplayDemo );
					}
					else
					{
						ConMsg("Failed to write received file '%s'!\n", data->filename );
					}
				}
				else
				{
					// don't overwrite existing files
					ConMsg("Download file '%s' already exists!\n", data->filename );
				}
			}
			else
			{
				// Something about the path or extension was unnaceptable
				ConMsg("Download file '%s' has invalid path or extension!\n", data->filename );
			}
		}
		else
		{
			// We won't create the file if 'sv_allowupload' is not enabled
			ConMsg("Download file '%s' ignored. File uploads are disabled!\n", data->filename );
		}
	}

	// clear receiveList
	if ( data->buffer )
	{
		delete [] data->buffer; 
		data->buffer = NULL;
	}

	return true;

}

int CNetChan::ProcessPacketHeader( netpacket_t * packet )
{
	// get sequence numbers		
	int sequence	= packet->message.ReadLong();
	int sequence_ack= packet->message.ReadLong();
	int flags		= packet->message.ReadByte();

	if ( ShouldChecksumPackets() )
	{
		unsigned short usCheckSum = (unsigned short)packet->message.ReadUBitLong( 16 );

		// Checksum applies to rest of packet
		Assert( !( packet->message.GetNumBitsRead() % 8 ) );
		int nOffset = packet->message.GetNumBitsRead() >> 3;
		int nCheckSumBytes = packet->message.TotalBytesAvailable() - nOffset;
	
		const void *pvData = packet->message.GetBasePointer() + nOffset;
		unsigned short usDataCheckSum = BufferToShortChecksum( pvData, nCheckSumBytes );
	
		if ( usDataCheckSum != usCheckSum )
		{
			ConMsg ("%s:corrupted packet %i at %i\n"
				, GetAddress()
				, sequence
				, m_nInSequenceNr);
			return -1;
		}
	}

	int relState	= packet->message.ReadByte();	// reliable state of 8 subchannels
	int nChoked		= 0;	// read later if choked flag is set
	int i,j;

	if ( flags & PACKET_FLAG_CHOKED )
		nChoked = packet->message.ReadByte(); 

	// discard stale or duplicated packets
	if (sequence <= m_nInSequenceNr )
	{
		if ( net_showdrop.GetInt() )
		{
			if ( sequence == m_nInSequenceNr )
			{
				ConMsg ("%s:duplicate packet %i at %i\n"
					, GetAddress()
					, sequence
					, m_nInSequenceNr);
			}
			else
			{
				ConMsg ("%s:out of order packet %i at %i\n"
					, GetAddress()
					, sequence
					, m_nInSequenceNr);
			}
		}
		
		return -1;
	}

//
// dropped packets don't keep the message from being used
//
	m_PacketDrop = sequence - (m_nInSequenceNr + nChoked + 1);

	if ( m_PacketDrop > 0 )
	{
		if ( net_showdrop.GetInt() )
		{
			ConMsg ("%s:Dropped %i packets at %i\n"
			,GetAddress(), m_PacketDrop, sequence );
		}
	}

	for ( i = 0; i<MAX_SUBCHANNELS; i++ )
	{
		int bitmask = (1<<i);

		// data of channel i has been acknowledged
		subChannel_s * subchan = &m_SubChannels[i];

		Assert( subchan->index == i);

		if ( (m_nOutReliableState & bitmask) == (relState & bitmask) )
		{
			if ( subchan->state == SUBCHANNEL_DIRTY )
			{
				// subchannel was marked dirty during changelevel, waiting list is already cleared
				subchan->Free();
			}
			else if ( subchan->sendSeqNr > sequence_ack )
			{
				ConMsg ("%s:reliable state invalid (%i).\n"	,GetAddress(), i );
				Assert( 0 );
				return -1;
			}
			else if ( subchan->state == SUBCHANNEL_WAITING )
			{
				for ( j=0; j<MAX_STREAMS; j++ )
				{
					if ( subchan->numFragments[j] == 0 )
						continue;

					Assert( m_WaitingList[j].Count() > 0 );
					
					dataFragments_t * data = m_WaitingList[j][0];

					// tell waiting list, that we received the acknowledge
					data->ackedFragments += subchan->numFragments[j]; 
					data->pendingFragments -= subchan->numFragments[j];
				}

				subchan->Free(); // mark subchannel as free again
			}
		}
		else // subchannel doesn't match
		{
			if ( subchan->sendSeqNr <= sequence_ack )
			{
				Assert( subchan->state != SUBCHANNEL_FREE );

				if ( subchan->state == SUBCHANNEL_WAITING )
				{
					if ( net_showfragments.GetBool() )
					{	
						ConMsg("Resending subchan %i: start %i, num %i\n", subchan->index, subchan->startFraggment[0], subchan->numFragments[0] );
					}

					subchan->state = SUBCHANNEL_TOSEND; // schedule for resend
				}
				else if ( subchan->state == SUBCHANNEL_DIRTY )
				{
					// remote host lost dirty channel data, flip bit back
					int bit = 1<<subchan->index; // flip bit back since data was send yet
			
					FLIPBIT(m_nOutReliableState, bit);

					subchan->Free(); 
				}
			}
		}
	}

	m_nInSequenceNr = sequence;
	m_nOutSequenceNrAck = sequence_ack;

// Update waiting list status
	
	for( i=0; i<MAX_STREAMS;i++)
		CheckWaitingList( i ); 

// Update data flow stats (use wiresize (compressed))
	FlowNewPacket( FLOW_INCOMING, m_nInSequenceNr, m_nOutSequenceNrAck, nChoked, m_PacketDrop, packet->wiresize + UDP_HEADER_SIZE );

	return flags;
}

/*
=================
CNetChan::ProcessPacket

called when a new packet has arrived for this netchannel
sequence numbers are extracted, fragments/file streams stripped 
and then the netmessages processed
=================
*/
void CNetChan::ProcessPacket( netpacket_t * packet, bool bHasHeader )
{
	VPROF( "CNetChan::ProcessPacket" );

	Assert( packet );

	bf_read &msg = packet->message;	// handy shortcut

	msg.Seek( 0 );

	if ( remote_address.IsValid() && !packet->from.CompareAdr ( remote_address ) )
	{
		return;
	}
#if defined( NET_PARANOID_DUMPS )
	g_NetParanoid.StartPacket( msg );
#endif
	// Update data flow stats
	FlowUpdate( FLOW_INCOMING, packet->wiresize + UDP_HEADER_SIZE );

	int flags = 0;

	if ( bHasHeader	)
	{
		flags = ProcessPacketHeader( packet );
	}

	if ( flags == -1 )
		return; // invalid header/packet
#if defined( NET_PARANOID_DUMPS )
	g_NetParanoid.NoteHeaderSize( msg, flags );
#endif
	if ( net_showudp.GetInt() && net_showudp.GetInt() != 3 &&
		( !net_showudp_remoteonly.GetBool() || !( remote_address.IsLocalhost() || remote_address.IsLoopback() ) ) )
	{
		char desc[ 128 ];
		uint64 steamID = g_pSteamSocketMgr->GetSteamIDForRemote( remote_address );
		Color clr( 0, 200, 255, 255 );
		if ( steamID != 0ull )
		{
			clr = Color( 255, 255, 100, 255 );
			Q_snprintf( desc, sizeof( desc ), "%12.12s %21.21s s(%llx)", GetName(), GetAddress(), steamID );
		}
		else
		{
			Q_snprintf( desc, sizeof( desc ), "%12.12s %21.21s", GetName(), GetAddress() );
		}
		ConColorMsg( clr, "UDP <- %s: sz=%5i seq=%5i ack=%5i rel=%1i tm=%8.3f wire=%i\n"
			, desc
			, packet->size
			, m_nInSequenceNr & 63
			, m_nOutSequenceNrAck & 63 
			, flags & PACKET_FLAG_RELIABLE ? 1 : 0
			, net_time
			, packet->wiresize );
	}
	
	last_received = net_time;

// tell message handler that a new packet has arrived
	m_MessageHandler->PacketStart( m_nInSequenceNr, m_nOutSequenceNrAck );

	if ( flags & PACKET_FLAG_RELIABLE )
	{
		int i, bit = 1<<msg.ReadUBitLong( 3 );

		for ( i=0; i<MAX_STREAMS; i++ )
		{
			if ( msg.ReadOneBit() != 0 )
			{
				if ( !ReadSubChannelData( msg, i ) )
					return; // error while reading fragments, drop whole packet
			}
		}

		// flip subChannel bit to signal successfull receiving
		FLIPBIT(m_nInReliableState, bit);
		
		for ( i=0; i<MAX_STREAMS; i++ )
		{
			if ( !CheckReceivingList( i ) )
				return; // error while processing 
		}
	}

// Is there anything left to process?
	if ( msg.GetNumBitsLeft() > 0 )
	{
		// parse and handle all messeges 
		if ( !ProcessMessages( msg, false ) )
		{
			return;	// disconnect or error
		}
	}
	
// tell message handler that packet is completely parsed
	m_MessageHandler->PacketEnd();

#if !defined(DEDICATED)
// tell demo system that packet is completely parsed
	if ( m_DemoRecorder && !demoplayer->IsPlayingBack() )
	{
		m_DemoRecorder->RecordPacket();
	}
#endif
}

int CNetChan::GetNumBitsWritten( bool bReliable )
{
	bf_write *pStream = &m_StreamUnreliable;
	if ( bReliable )
	{
		pStream = &m_StreamReliable;
	}
	return pStream->GetNumBitsWritten();
}

bool CNetChan::SendNetMsg( INetMessage &msg, bool bForceReliable, bool bVoice )
{
	if ( remote_address.IsNull() )
		return true;

	bf_write *pStream = &m_StreamUnreliable;
	
	if ( msg.IsReliable() || bForceReliable )
	{
		pStream = &m_StreamReliable;
	}

	if ( bVoice )
	{
		pStream = &m_StreamVoice;
	}

	bool bResult = msg.WriteToBuffer( *pStream );
	if ( !bResult )
	{
		Warning( "SendNetMsg %s: stream[%s] buffer overflow (maxsize = %d)!\n", GetAddress(), pStream->GetDebugName(), (pStream->GetMaxNumBits()+7)/8 );
		Assert( 0 );

		if ( net_droponsendoverflow.GetBool() )
		{
			m_MessageHandler->ConnectionCrashed( "Buffer overflow in send net message" );
			return false;
		}
	}
	return bResult;
}

INetMessageBinder *CNetChan::FindMessageBinder( int type, int index )
{
	if ( !m_NetMessages.IsValidIndex( type )  )
	{
		return NULL;
	}

	if( index < m_NetMessages[ type ].Count() )
	{
		return m_NetMessages[ type ][ index ];
	}
	return NULL;
}

bool CNetChan::RegisterMessage(INetMessageBinder *msg)
{
	int Type = msg->GetType();

	Assert( Type <= SVC_Messages_MAX || Type <= CLC_Messages_MAX || Type <= NET_Messages_MAX );

	m_NetMessages.EnsureCount( Type + 1 );

	m_NetMessages[ Type ].AddToTail( msg );
	msg->SetNetChannel( this );
	return true;
}

bool CNetChan::UnregisterMessage(INetMessageBinder *msg)
{
	CUtlVector< INetMessageBinder * >& Messages = m_NetMessages[ msg->GetType() ];
	int numtypes = Messages.Count();

	for( int i = 0; i < numtypes; i++ )
	{
		if ( Messages[ i ] == msg )
		{
			Messages.Remove( i );
			return true;
		}
	}

	return false;
}

bool CNetChan::SendData( bf_write &msg, bool bReliable )
{
	// Always queue any pending reliable data ahead of the fragmentation buffer

	if ( remote_address.IsNull() )
		return true;

	if ( msg.GetNumBitsWritten() <= 0 )
		return true;

	if ( msg.IsOverflowed() && !bReliable )
		return true;

	bf_write * buf = bReliable ? &m_StreamReliable : &m_StreamUnreliable;


	if ( msg.GetNumBitsWritten() > buf->GetNumBitsLeft() )
	{
		if (  bReliable )
		{
			ConMsg( "ERROR! SendData reliabe data too big (%i)", msg.GetNumBytesWritten() );
		}

		return false;
	}

	return buf->WriteBits( msg.GetData(), msg.GetNumBitsWritten() );
}

bool CNetChan::SendReliableViaStream( dataFragments_t *data)
{
	// Always queue any pending reliable data ahead of the fragmentation buffer

	char		headerBuf[32];
	bf_write	header( "outDataHeader", headerBuf, sizeof(headerBuf) );

	
	data->transferID = m_nOutSequenceNr; // used for acknowledging
	data->pendingFragments = data->numFragments; // send, but not ACKed yet

	header.WriteByte( STREAM_CMD_DATA );
	header.WriteWord( data->bytes );			// bytes
	header.WriteLong( data->transferID );	

	if ( net_showtcp.GetInt() )
	{
		ConMsg ("TCP -> %s: sz=%i seq=%i\n", GetAddress(), data->bytes, m_nOutSequenceNr );
	}
	
	NET_SendStream( m_StreamSocket, (char*)header.GetData(), header.GetNumBytesWritten(), 0	);

	return NET_SendStream( m_StreamSocket, data->buffer, data->bytes, 0 ) != -1;
}

bool CNetChan::SendReliableAcknowledge(int seqnr)
{
	// Always queue any pending reliable data ahead of the fragmentation buffer

	char		headerBuf[32];
	bf_write	header( "outAcknHeader", headerBuf, sizeof(headerBuf) );

	header.WriteByte( STREAM_CMD_ACKN );
	header.WriteLong( seqnr );	// used for acknowledging

	if ( net_showtcp.GetInt() )
	{
		ConMsg ("TCP -> %s: ACKN seq=%i\n", GetAddress(), seqnr );
	}

	return NET_SendStream( m_StreamSocket, (char*)header.GetData(), header.GetNumBytesWritten(), 0 ) > 0;
}

bool CNetChan::ProcessStream( void )
{
	char		cmd;
	ALIGN4 char	headerBuf[512] ALIGN4_POST;
	
	if ( !m_StreamSocket )
		return true;

	if ( m_SteamType == STREAM_CMD_NONE )
	{
		// read command byte
		int ret = NET_ReceiveStream( m_StreamSocket, &cmd, 1, 0 );

		if ( ret == 0)
		{
			// nothing received, but ok
			return true;
		}
		else if ( ret == -1 )
		{
			// something failed with the TCP connection
			return false;
		}
		
		ResetStreaming();	// clear all state values

		m_SteamType = cmd;

	}

	bf_read		header( "inDataHeader", headerBuf, sizeof(headerBuf) );

	// now check command type

	if ( m_SteamType==STREAM_CMD_AUTH )
	{
		// server accpeted connection, send challenge nr
		m_StreamActive = true;
		
		ResetStreaming();

		return SendReliableAcknowledge( m_ChallengeNr );
	}

	if ( (m_SteamType==STREAM_CMD_DATA) && (m_StreamLength==0) )
	{
		int ret = NET_ReceiveStream( m_StreamSocket, (char*)&headerBuf, 6, 0 ) ;
		
		if ( ret == 0)
		{
			// nothing received, but ok
			return true;
		}
		else if ( ret == -1 )
		{
			// something failed with the TCP connection
			return false;
		}

		m_StreamLength = header.ReadWord();
		m_StreamSeqNr = header.ReadLong();

		if ( m_StreamLength	> NET_MAX_PAYLOAD )
		{
			ConMsg( "ERROR! Stream indata too big (%i)", m_StreamLength );
			return false;
		}
	}

	if ( (m_SteamType==STREAM_CMD_FILE) && (m_SteamFile[0]==0) )
	{
		Assert ( 0 );
		return false;
	}

	if ( (m_SteamType==STREAM_CMD_ACKN) && (m_StreamSeqNr==0) )
	{
		int ret = NET_ReceiveStream( m_StreamSocket, (char*)&headerBuf, 4, 0 );
		
		if ( ret == 0)
		{
			// nothing received, but ok
			return true;
		}
		else if ( ret == -1 )
		{
			// something failed with the TCP connection
			return false;
		}

		m_StreamSeqNr = header.ReadLong();

		dataFragments_t * data = m_WaitingList[FRAG_NORMAL_STREAM][0]; 

		if ( data->transferID == (unsigned)m_StreamSeqNr )
		{
			if ( net_showtcp.GetInt() )
			{
				ConMsg ("TCP <- %s: ACKN seq=%i\n", GetAddress(), m_StreamSeqNr );
			}

			Assert( data->pendingFragments == data->numFragments );

			RemoveHeadInWaitingList( FRAG_NORMAL_STREAM );
		}
		else
		{
			ConMsg ("TCP <- %s: invalid ACKN streamlen %d seqnr %i\n", GetAddress(), m_StreamLength, m_StreamSeqNr );
		}

		ResetStreaming();
		return true;
	}
	

	if ( m_StreamReceived < m_StreamLength )
	{
		// read in 4kB chuncks
		int bytesLeft = ( m_StreamLength - m_StreamReceived );	

		int bytesRecv = NET_ReceiveStream( m_StreamSocket, (char*)m_StreamData.Base() + m_StreamReceived, bytesLeft, 0 );
		
		if ( bytesRecv == 0 )
		{
			return true;
		}
		else if ( bytesRecv == -1 )
		{
			return false;
		}

		m_StreamReceived+= bytesRecv;

		if ( m_StreamReceived > m_StreamLength )
		{
			ConMsg( "ERROR! Stream indata oversize." );
			return false;
		}

		if ( m_StreamReceived == m_StreamLength	)
		{
			int ackseqnr =m_StreamSeqNr;

			bf_read buffer( m_StreamData.Base(), m_StreamLength );

			ProcessMessages( buffer, true );

			// reset stream state
			ResetStreaming();

			return SendReliableAcknowledge( ackseqnr );	// tell sender that we have it
		}
	}

	return true;
}

int CNetChan::GetDataRate() const
{
	return m_Rate;
}

bool CNetChan::HasPendingReliableData( void )
{
	return (m_StreamReliable.GetNumBitsWritten() > 0) || 
		   (m_WaitingList[FRAG_NORMAL_STREAM].Count() > 0) ||
		   (m_WaitingList[FRAG_FILE_STREAM].Count() > 0);
}

float CNetChan::GetTimeConnected() const
{
	float t = net_time - connect_time;
	return (t>0.0f) ? t : 0.0f ;
}

const ns_address &CNetChan::GetRemoteAddress() const
{
	return remote_address;
}

INetChannelHandler * CNetChan::GetMsgHandler( void ) const
{
	return  m_MessageHandler;
}

bool CNetChan::IsTimedOut() const
{
	if ( m_Timeout == -1.0f )
		return false;
	else
		return (last_received + m_Timeout) < net_time;
}

bool CNetChan::IsTimingOut() const
{
	if ( m_Timeout == -1.0f )
		return false;
	else
		return (last_received + CONNECTION_PROBLEM_TIME) < net_time;
}

float CNetChan::GetTimeoutSeconds() const
{
	return m_Timeout;
}

float CNetChan::GetTimeSinceLastReceived() const
{
	float t = net_time - last_received;
	return (t>0.0f) ? t : 0.0f ;
}

bool CNetChan::IsOverflowed() const
{
	return m_StreamReliable.IsOverflowed();
}

void CNetChan::Reset()
{
	// FlowReset();
	m_StreamUnreliable.Reset();  // clear any pending unreliable data messages
	m_StreamReliable.Reset();	 // clear any pending reliable data messages
	m_fClearTime = 0.0;			 // ready to send
	m_nChokedPackets = 0;

	m_nSplitPacketSequence = 1;
}

int CNetChan::GetSocket() const
{
	return m_Socket;
}

float CNetChan::GetAvgData( int flow ) const
{
	return m_DataFlow[flow].avgbytespersec;
}

float CNetChan::GetAvgPackets( int flow ) const
{
	return m_DataFlow[flow].avgpacketspersec;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *chan - 
//-----------------------------------------------------------------------------
int CNetChan::GetTotalData(int flow ) const
{
	return m_DataFlow[flow].totalbytes;
}

int	CNetChan::GetTotalPackets( int flow ) const
{
	return m_DataFlow[flow].totalpackets;
}

int	CNetChan::GetSequenceNr( int flow ) const
{
	if ( flow == FLOW_OUTGOING )
	{
		return m_nOutSequenceNr;
	}
	else if ( flow == FLOW_INCOMING )
	{
		return m_nInSequenceNr;
	}
	
	return 0;
}

int	CNetChan::GetBufferSize( void ) const
{
	return NET_FRAMES_BACKUP;
}

bool CNetChan::IsValidPacket( int flow, int frame_number ) const 
{
	return m_DataFlow[flow].frame_headers[ frame_number & NET_FRAMES_MASK ].valid;
}

float CNetChan::GetPacketTime( int flow, int frame_number ) const
{
	return m_DataFlow[flow].frame_headers[ frame_number & NET_FRAMES_MASK ].time;
}

void CNetChan::GetPacketResponseLatency( int flow, int frame_number, int *pnLatencyMsecs, int *pnChoke ) const
{
	const netframe_header_t &nfh = m_DataFlow[flow].frame_headers[ frame_number & NET_FRAMES_MASK ];
	const netframe_t &nf = m_DataFlow[flow].frames[ frame_number & NET_FRAMES_MASK ];
	if ( pnLatencyMsecs )
	{
		if ( nf.dropped )
		{
			*pnLatencyMsecs = 9999;
		}
		else 
		{
			*pnLatencyMsecs = (int)( 1000.0f * nf.avg_latency );
		}
	}
	if ( pnChoke )
	{
		*pnChoke = nfh.choked;
	}
}

void CNetChan::GetRemoteFramerate( float *pflFrameTime, float *pflRemoteFrameTimeStdDeviation, float *pflFrameStartTimeStdDeviation ) const
{
	if ( pflFrameTime )
	{
		*pflFrameTime = m_flRemoteFrameTime;
	}
	if ( pflRemoteFrameTimeStdDeviation )
	{
		*pflRemoteFrameTimeStdDeviation = m_flRemoteFrameTimeStdDeviation;
	}
	if ( pflFrameStartTimeStdDeviation )
	{
		*pflFrameStartTimeStdDeviation = m_flRemoteFrameStartTimeStdDeviation;
	}
}


float CNetChan::GetLatency( int flow ) const
{
	return m_DataFlow[flow].latency;
}

float CNetChan::GetAvgChoke( int flow ) const
{
	return m_DataFlow[flow].avgchoke;
}

float CNetChan::GetAvgLatency( int flow ) const
{
	return m_DataFlow[flow].avglatency;	
}

float CNetChan::GetAvgLoss( int flow ) const
{
	return m_DataFlow[flow].avgloss;
}

float CNetChan::GetTime( void ) const
{
	return net_time;
}

bool CNetChan::GetStreamProgress( int flow, int *received, int *total ) const
{
	(*total) = 0;
	(*received) = 0;
	
	if ( flow == FLOW_INCOMING )
	{
		for ( int i = 0; i<MAX_STREAMS; i++ )
		{
			if ( m_ReceiveList[i].buffer != NULL )
			{
				(*total) += m_ReceiveList[i].numFragments * FRAGMENT_SIZE;
				(*received) += m_ReceiveList[i].ackedFragments * FRAGMENT_SIZE;
			}
		}

		return ((*total)>0);
	}

	if ( flow == FLOW_OUTGOING )
	{
		for ( int i = 0; i<MAX_STREAMS; i++ )
		{
			if ( m_WaitingList[i].Count() > 0 )
			{
				(*total) += m_WaitingList[i][0]->numFragments * FRAGMENT_SIZE;
				(*received) += m_WaitingList[i][0]->ackedFragments * FRAGMENT_SIZE;
			}
		}

		return ((*total)>0);
	}
	
	return false; // TODO TCP progress
}

float CNetChan::GetCommandInterpolationAmount( int flow, int frame_number ) const
{
	return m_DataFlow[ flow ].frames[ frame_number & NET_FRAMES_MASK ].m_flInterpolationAmount;
}

int	CNetChan::GetPacketBytes( int flow, int frame_number, int group ) const
{
	if ( group >= INetChannelInfo::TOTAL )
	{
		return m_DataFlow[flow].frame_headers[ frame_number & NET_FRAMES_MASK ].size;
	}
	else
	{
		return Bits2Bytes( m_DataFlow[flow].frames[ frame_number & NET_FRAMES_MASK ].msggroups[group] );
	}
}

void CNetChan::UpdateMessageStats( int msggroup, int bits)
{
	netflow_t * pflow = &m_DataFlow[ FLOW_INCOMING ];
	netframe_t *pframe = pflow->currentframe;

	Assert( (msggroup >= INetChannelInfo::GENERIC) && (msggroup < INetChannelInfo::TOTAL) );

	m_MsgStats[msggroup] += bits;

	if ( pframe )
		pframe->msggroups[msggroup] +=bits;
		
}

void CNetChan::IncrementQueuedPackets()
{
	++m_nQueuedPackets;
}

void CNetChan::DecrementQueuedPackets()
{
	--m_nQueuedPackets;
	Assert( m_nQueuedPackets >= 0 );
	if ( m_nQueuedPackets < 0 )
		m_nQueuedPackets = 0;
}

bool CNetChan::HasQueuedPackets() const
{
	if ( g_pQueuedPackedSender->HasQueuedPackets( this ) )
	{
		return true;
	}

	return m_nQueuedPackets > 0;
}

void CNetChan::SetInterpolationAmount( float flInterpolationAmount )
{
	m_flInterpolationAmount = flInterpolationAmount;
}

void CNetChan::SetRemoteFramerate( float flFrameTime, float flFrameTimeStdDeviation, float flFrameStartTimeStdDeviation )
{
	m_flRemoteFrameTime = flFrameTime;
	m_flRemoteFrameTimeStdDeviation = flFrameTimeStdDeviation;
	m_flRemoteFrameStartTimeStdDeviation = flFrameStartTimeStdDeviation;
}

// Max # of payload bytes before we must split/fragment the packet
void CNetChan::SetMaxRoutablePayloadSize( int nSplitSize )
{
	if ( m_nMaxRoutablePayloadSize != nSplitSize )
	{
		DevMsg( "Setting max routable payload size from %d to %d for %s\n",
			m_nMaxRoutablePayloadSize, nSplitSize, GetName() );
	}
	m_nMaxRoutablePayloadSize = nSplitSize;
}

int CNetChan::GetMaxRoutablePayloadSize()
{
	return m_nMaxRoutablePayloadSize;
}

int CNetChan::IncrementSplitPacketSequence()
{
	return ++m_nSplitPacketSequence;
}

bool CNetChan::IsValidFileForTransfer( const char *pszFilename )
{
	if ( !pszFilename || !pszFilename[0] )
		return false;

	// No absolute paths or weaseling up the tree with ".." allowed.
	if ( !COM_IsValidPath( pszFilename ) || V_IsAbsolutePath( pszFilename ) )
		return false;

	char szTemp[MAX_PATH];
	int l = V_strlen( pszFilename );
	if ( l >= sizeof(szTemp) )
		return false;
	V_strcpy_safe( szTemp, pszFilename );
	V_FixSlashes( szTemp, '/' );
	if ( szTemp[l-1] == '/' )
		return false;

	if (
		V_stristr( pszFilename, "lua/" )
		|| V_stristr( pszFilename, "gamemodes/" )
		|| V_stristr( pszFilename, "scripts/" )
		|| V_stristr( pszFilename, "addons/" )
		|| V_stristr( pszFilename, "cfg/" )
		|| V_stristr( pszFilename, "~/" )
		|| V_stristr( pszFilename, "gamemodes.txt" )
		)
		return false;

	// Allow only bsp and nav file transfers to not overwrite any assets in maps directory
	if ( V_stristr( pszFilename, "maps/" ) &&
		!V_stristr( pszFilename, ".bsp" ) &&
		!V_stristr( pszFilename, ".ain" ) &&
		!V_stristr( pszFilename, ".nav" ) )
		return false;

	const char *extension = V_strrchr( pszFilename, '.' );
	if ( !extension )
		return false;

	int baseLen = V_strlen( extension );
	if ( baseLen > 4 || baseLen < 3 )
		return false;

	// are there any spaces in the extension? (windows exploit)
	const char *pChar = extension;
	while ( *pChar )
	{
		if ( V_isspace( *pChar ) )
		{
			return false;
		}

		++pChar;
	}

	if ( !Q_strcasecmp( extension, ".cfg" ) ||
		!Q_strcasecmp( extension, ".lst" ) ||
		!Q_strcasecmp( extension, ".lmp" ) ||
		!Q_strcasecmp( extension, ".exe" ) ||
		!Q_strcasecmp( extension, ".vbs" ) ||
		!Q_strcasecmp( extension, ".com" ) ||
		!Q_strcasecmp( extension, ".bat" ) ||
		!Q_strcasecmp( extension, ".dll" ) ||
		!Q_strcasecmp( extension, ".ini" ) ||
		!Q_strcasecmp( extension, ".log" ) ||
		!Q_strcasecmp( extension, ".lua" ) ||
		!Q_strcasecmp( extension, ".nut" ) ||
		!Q_strcasecmp( extension, ".vdf" ) ||
		!Q_strcasecmp( extension, ".smx" ) ||
		!Q_strcasecmp( extension, ".gcf" ) ||
		!Q_strcasecmp( extension, ".sys" ) )
	{
		return false;
	}

	return true;
}


void CNetChan::AttachSplitPlayer( int nSplitPlayerSlot, INetChannel *pChannel )
{
	SplitPlayer_t search;
	search.m_nSlot = nSplitPlayerSlot;
	search.m_pChannel = pChannel;
	int idx = m_SplitPlayers.Find( search );
	if ( idx == m_SplitPlayers.InvalidIndex() )
	{
		m_SplitPlayers.Insert( search );
	}
}

void CNetChan::DetachSplitPlayer( int nSplitPlayerSlot )
{
	SplitPlayer_t search;
	search.m_nSlot = nSplitPlayerSlot;
	int idx = m_SplitPlayers.Find( search );
	if ( idx != m_SplitPlayers.InvalidIndex() )
	{
		m_SplitPlayers.RemoveAt( idx );
	}
}

void CNetChan::ChangeSplitUser( bf_write &out, int slot )
{
	// Msg( "Changing to slot %d on %s\n", slot, GetName() );
	CNETMsg_SplitScreenUser_t SplitScreenUser;
	SplitScreenUser.set_slot( slot );
	SplitScreenUser.WriteToBuffer( out );
}

void CNetChan::MaybeAppendBuffer( EBufType eBufType, bf_write &out, SplitPlayer_t &sp, bf_write &src, int *pnCurrentSlot )
{
	if ( src.GetNumBitsWritten() <= 0 )
		return;

	if ( sp.m_nSlot != *pnCurrentSlot )
	{
		*pnCurrentSlot = sp.m_nSlot;
		ChangeSplitUser( out, sp.m_nSlot );
	}

	out.WriteBits( src.GetData(), src.GetNumBitsWritten() );

	//Msg( "Wrote %d [%.3f] bits to buffer %s\n",
	//	src.GetNumBitsWritten(), (float)src.GetNumBitsWritten() / 8.0f, GetBufferDebugName( eBufType ) );

	src.Reset();
}

bf_write &CNetChan::GetBuffer( EBufType eBufType )
{
	switch ( eBufType )
	{
	default:
		break;
	case BUF_RELIABLE:
		return m_StreamReliable;
	case BUF_UNRELIABLE:
		return m_StreamUnreliable;
	case BUF_VOICE:
		return m_StreamVoice;
	}

	Assert( 0 );
	return m_StreamReliable;
}

char const *CNetChan::GetBufferDebugName( EBufType eBufType )
{
	switch ( eBufType )
	{
	default:
		break;
	case BUF_RELIABLE:
		return "m_StreamReliable";
	case BUF_UNRELIABLE:
		return "m_StreamUnreliable";
	case BUF_VOICE:
		return "m_StreamVoice";
	}

	Assert( 0 );
	return "???";
}

void CNetChan::MergeSplitUserBuffers( EBufType eBufType, bf_write &outbuf )
{
	int nCurrentSlot = 0;

	for ( int user = m_SplitPlayers.FirstInorder(); user != m_SplitPlayers.InvalidIndex(); user = m_SplitPlayers.NextInorder( user ) )
	{
		SplitPlayer_t &sp = m_SplitPlayers[ user ];
		CNetChan *chan = static_cast< CNetChan * >( sp.m_pChannel );
		if ( !chan )
			continue;

		bf_write &src = chan->GetBuffer( eBufType );
		// This might change the user
		MaybeAppendBuffer( eBufType, outbuf, sp, src, &nCurrentSlot );
	}

	if ( nCurrentSlot != 0 )
	{
		ChangeSplitUser( outbuf, 0 );
	}
}

void CNetChan::SplitUserCombineForSending()
{
	// Nothing to do?
	if ( m_SplitPlayers.Count() == 0 )
		return;

	MergeSplitUserBuffers( BUF_RELIABLE, m_StreamReliable );
	// FIXME:  datagram payload?
	MergeSplitUserBuffers( BUF_UNRELIABLE, m_StreamUnreliable );
	MergeSplitUserBuffers( BUF_VOICE, m_StreamVoice );
}

bool CNetChan::IsRemoteDisconnected() const
{
	// See if we think remote side is using Steam sockets
	if ( remote_address.IsValid() &&  // was connected
		 remote_address.GetPort() == (unsigned short)ISteamSocketMgr::STEAM_CNX_PORT && // was using the "steam" pseudo port #
		 g_pSteamSocketMgr->GetSteamIDForRemote( remote_address ) == 0ull ) // no longer have an association for the remote side by steamid (socket died)
	{
		return true;
	}

	// Regular case never signals this way.
	return false;
}
