//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef NETMESSAGES_H
#define NETMESSAGES_H

#ifdef _WIN32
#pragma once
#pragma warning(disable : 4100)	// unreferenced formal parameter
#endif

#include <inetmessage.h>
#include <checksum_crc.h>
#include <checksum_md5.h>
#include <const.h>
#include <utlvector.h>
#include "qlimits.h"
#include "mathlib/vector.h"
#include <soundflags.h>
#include <bitbuf.h>
#include <inetchannel.h>
#include "protocol.h"
#include <inetmsghandler.h>
#include <igameevents.h>
#include <bitvec.h>
#include "networksystem/inetworksystem.h"
#include <engine/iserverplugin.h>
#include <color.h>
#include <tier0/tslist.h>
#include <tier1/utldelegate.h>
#include "tier1/utlstring.h"
#include "tier1/tokenset.h"
#include "netmessages_signon.h"

// eliminates a conflict with TYPE_BOOL in OSX
#ifdef TYPE_BOOL
#undef TYPE_BOOL
#endif

#include "netmessages.pb.h"

#if !defined( _X360 )
#include "xbox/xboxstubs.h"
#endif

template< int msgType, typename PB_OBJECT_TYPE, int groupType = INetChannelInfo::GENERIC, bool bReliable = true > 
class CNetMessagePB : public INetMessage, public PB_OBJECT_TYPE
{
public:
	typedef CNetMessagePB< msgType, PB_OBJECT_TYPE, groupType, bReliable > MyType_t;
	typedef PB_OBJECT_TYPE PBType_t;
	static const int sk_Type = msgType;

	CNetMessagePB() :
	m_bReliable( bReliable )
	{
	}

	virtual ~CNetMessagePB()
	{
	}
	
	virtual bool ReadFromBuffer( bf_read &buffer )
	{
		int size = buffer.ReadVarInt32();
		if ( size < 0 || size > NET_MAX_PAYLOAD )
		{
			return false;
		}

		// Check its valid
		if ( size > buffer.GetNumBytesLeft() )
		{
			return false;
		}

		// If the read buffer is byte aligned, we can parse right out of it
		if ( ( buffer.GetNumBitsRead() % 8 ) == 0 )
		{
			bool parseResult = PB_OBJECT_TYPE::ParseFromArray( buffer.GetBasePointer() + buffer.GetNumBytesRead(), size );
			buffer.SeekRelative( size * 8 );
			return parseResult;
		}

		// otherwise we have to do a temp allocation so we can read it all shifted
#ifdef NET_SHOW_UNALIGNED_MSGS
		DevMsg("Warning: unaligned read of protobuf message %s (%d bytes)\n", PB_OBJECT_TYPE::GetTypeName().c_str(), size );
#endif

		void *parseBuffer = stackalloc( size );
		if ( !buffer.ReadBytes( parseBuffer, size ) )
		{
			return false;
		}

		if ( ! PB_OBJECT_TYPE::ParseFromArray( parseBuffer, size ) )
		{
			return false;
		}

		return true;
	}

	virtual bool WriteToBuffer( bf_write &buffer ) const
	{
		if ( !PB_OBJECT_TYPE::IsInitialized() )
		{
			Msg("WriteToBuffer Message %s is not initialized! Probably missing required fields!\n", PB_OBJECT_TYPE::GetTypeName().c_str() );
		}

		int size = PB_OBJECT_TYPE::ByteSize();

		// If the write is byte aligned we can go direct
		if ( ( buffer.GetNumBitsWritten() % 8 ) == 0 )
		{
			int sizeWithHeader = size + 1 + buffer.ByteSizeVarInt32( GetType() ) + buffer.ByteSizeVarInt32( size );

			if ( buffer.GetNumBytesLeft() >= sizeWithHeader )
			{
				buffer.WriteVarInt32( GetType() );
				buffer.WriteVarInt32( size );

				if ( !PB_OBJECT_TYPE::SerializeWithCachedSizesToArray( ( google::protobuf::uint8 * )buffer.GetData() + buffer.GetNumBytesWritten() ) )
				{
					return false;
				}

				// Tell the buffer we just splatted into it
				buffer.SeekToBit( buffer.GetNumBitsWritten() + ( size * 8 ) );
				return true;
			}

			// Won't fit
			return false;
		}

		// otherwise we have to do a temp allocation so we can write it all shifted
#ifdef NET_SHOW_UNALIGNED_MSGS
		DevMsg("Warning: unaligned write of protobuf message %s (%d bytes)\n", PB_OBJECT_TYPE::GetTypeName().c_str(), size );
#endif

		void *serializeBuffer = stackalloc( size );

		if ( ! PB_OBJECT_TYPE::SerializeWithCachedSizesToArray( ( google::protobuf::uint8 * )serializeBuffer ) )
		{
			return false;
		}

		buffer.WriteVarInt32( GetType() );
		buffer.WriteVarInt32( size );
		return buffer.WriteBytes( serializeBuffer, size );
	}

	virtual const char *ToString() const
	{
		m_toString = PB_OBJECT_TYPE::DebugString();
		return m_toString.c_str();
	}

	virtual int GetType() const 
	{ 
		return msgType; 
	}

	virtual size_t GetSize() const
	{
		return sizeof( *this );
	}

	virtual const char *GetName() const 
	{ 
		if ( s_typeName.empty() )
		{
			s_typeName = PB_OBJECT_TYPE::GetTypeName(); 
		}
		return s_typeName.c_str();
	}

	virtual int GetGroup() const 
	{ 
		return groupType;
	}

	virtual void SetReliable( bool state )
	{
		m_bReliable = state;
	}

	virtual bool IsReliable() const 
	{ 
		return m_bReliable; 
	}

	virtual INetMessage *Clone() const
	{
		MyType_t *pClone = new MyType_t;
		pClone->CopyFrom( *this );
		pClone->m_bReliable = m_bReliable;

		return pClone;
	}

protected:
	bool m_bReliable; // true if message should be sent reliable
	mutable std::string	m_toString; // cached copy of ToString()
	static std::string s_typeName;
};

template< int msgType, typename PB_OBJECT_TYPE, int groupType , bool bReliable > 
std::string CNetMessagePB< msgType, PB_OBJECT_TYPE, groupType , bReliable >::s_typeName;

class CNetMessageBinder
{
public:
	CNetMessageBinder()
		: m_pBind( NULL )
	{
	}

	~CNetMessageBinder()
	{
		delete m_pBind;
	}

	template< class _N > 
	void Bind( INetChannel *pNetChannel, CUtlDelegate< bool ( const typename _N::PBType_t & obj ) > handler )
	{
		delete m_pBind;
		m_pBind = new BindParams<_N>( pNetChannel, handler );
	}

	void Unbind()
	{
		delete m_pBind;
		m_pBind = NULL;
	}

	bool IsBound() const
	{
		return m_pBind != NULL;
	}

private:
	template < class _N >
	struct BindParams : public INetMessageBinder
	{
		BindParams( INetChannel *pNetChannel, CUtlDelegate< bool ( const typename _N::PBType_t & obj ) > handler )
			: m_NetChannel( pNetChannel )
			, m_handler( handler )
		{
			if ( m_NetChannel )
			{
				m_NetChannel->RegisterMessage( this );
			}
		}

		virtual ~BindParams()
		{
			if ( m_NetChannel )
			{
				m_NetChannel->UnregisterMessage( this );
			}
		}

		virtual int	GetType( void ) const
		{
			return _N::sk_Type;
		}

		virtual void SetNetChannel(INetChannel * netchan) 
		{ 
			if( m_NetChannel != netchan )
			{
				if( m_NetChannel )
					m_NetChannel->UnregisterMessage( this );

				m_NetChannel = netchan;

				if( m_NetChannel )
					m_NetChannel->RegisterMessage( this );
			}
		}

		virtual INetMessage *CreateFromBuffer( bf_read &buffer )
		{
			INetMessage *pMsg = new typename _N::MyType_t;
			if ( !pMsg->ReadFromBuffer( buffer ) )
			{
				delete pMsg;
				return NULL;
			}
			return pMsg;
		}

		virtual bool Process( const INetMessage &src )
		{	
			const typename _N::MyType_t &typedSrc = static_cast< const typename _N::MyType_t & >( src );

			Assert( m_handler );
			if( m_handler )
			{
				return m_handler( static_cast< typename _N::PBType_t const & >( typedSrc ) );
			}
			return false;
		}

		INetChannel	*m_NetChannel;	// netchannel this message is from/for
		CUtlDelegate< bool ( const typename _N::PBType_t & obj ) > m_handler;
	};

	INetMessageBinder *m_pBind;
}; 

///////////////////////////////////////////////////////////////////////////////////////
// bidirectional net messages:
///////////////////////////////////////////////////////////////////////////////////////

class CNETMsg_Tick_t : public CNetMessagePB< net_Tick, CNETMsg_Tick >
{
public:
	static float FrametimeToFloat( uint32 frametime ) { return ( float )frametime / 1000000.0f; }

	CNETMsg_Tick_t( int tick, float host_computationtime, float host_computationtime_stddeviation, float host_framestarttime_std_deviation )
	{
		SetReliable( false );
		set_tick( tick );
		set_host_computationtime( MIN( ( uint32 )( 1000000.0 * host_computationtime ), 1000000u ) );
		set_host_computationtime_std_deviation( MIN( ( uint32 )( 1000000.0 * host_computationtime_stddeviation ), 1000000u ) );
		set_host_framestarttime_std_deviation( MIN( ( uint32 )( 1000000.0 * host_framestarttime_std_deviation ), 1000000u ) );
	}
};

class CNETMsg_StringCmd_t : public CNetMessagePB< net_StringCmd, CNETMsg_StringCmd, INetChannelInfo::STRINGCMD >
{
public:
	CNETMsg_StringCmd_t( const char *command )
	{
		set_command( command );
	}
};

class CNETMsg_PlayerAvatarData_t : public CNetMessagePB< net_PlayerAvatarData, CNETMsg_PlayerAvatarData, INetChannelInfo::PAINTMAP >
{
	// 12 KB player avatar 64x64 rgb only no alpha
	// WARNING-WARNING-WARNING
	// This message is extremely large for our net channels
	// and must be pumped through special fragmented waiting list
	// via chunk-based ack mechanism!
	// See: INetChannel::EnqueueVeryLargeAsyncTransfer
	// WARNING-WARNING-WARNING
public:
	CNETMsg_PlayerAvatarData_t() {}
	CNETMsg_PlayerAvatarData_t( uint32 unAccountID, void const *pvData, uint32 cbData )
	{
		set_accountid( unAccountID );
		set_rgb( ( const char * ) pvData, cbData );
	}
};

class CNETMsg_SignonState_t : public CNetMessagePB< net_SignonState, CNETMsg_SignonState, INetChannelInfo::SIGNON >
{
public:
	CNETMsg_SignonState_t( int state, int spawncount )
	{
		set_signon_state( state );
		set_spawn_count( spawncount );
		set_num_server_players( 0 );
	}
};

inline void NetMsgSetCVarUsingDictionary( CMsg_CVars::CVar *convar, char const * name, char const * value )
{
	convar->set_value( value );
	
	if ( 0 ) ( void ) 0;
	/** Removed for partner depot **/
	else
	{
#ifdef _DEBUG
		DevWarning( "Missing dictionary entry for cvar '%s'\n", name );
#endif
		convar->set_name( name );
	}
}

inline void NetMsgExpandCVarUsingDictionary( CMsg_CVars::CVar *convar )
{
	if ( convar->has_name() )
		return;
	switch ( convar->dictionary_name() )
	{
	case 0: return;
	/** Removed for partner depot **/
	default:
		DevWarning( "Invalid dictionary entry for cvar # %d\n", convar->dictionary_name() );
		convar->set_name( "undefined" );
		break;
	}
}

inline const char * NetMsgGetCVarUsingDictionary( CMsg_CVars::CVar const &convar )
{
	if ( convar.has_name() )
		return convar.name().c_str();
	switch ( convar.dictionary_name() )
	{
	case 0: return "";
	/** Removed for partner depot **/
default:
	DevWarning( "Invalid dictionary entry for cvar # %d\n", convar.dictionary_name() );
	return "undefined";
	}
}

class CNETMsg_SetConVar_t : public CNetMessagePB< net_SetConVar, CNETMsg_SetConVar, INetChannelInfo::STRINGCMD >
{
public:
	CNETMsg_SetConVar_t() {}
	CNETMsg_SetConVar_t( const char * name, const char * value )
	{
		AddToTail( name, value );
	}
	void AddToTail( const char * name, const char * value )
	{
		NetMsgSetCVarUsingDictionary( mutable_convars()->add_cvars(), name, value );
	}
};

typedef CNetMessagePB< net_NOP, CNETMsg_NOP >											CNETMsg_NOP_t;
typedef CNetMessagePB< net_Disconnect, CNETMsg_Disconnect >								CNETMsg_Disconnect_t;
typedef CNetMessagePB< net_File, CNETMsg_File >											CNETMsg_File_t;
typedef CNetMessagePB< net_SplitScreenUser, CNETMsg_SplitScreenUser >					CNETMsg_SplitScreenUser_t;

///////////////////////////////////////////////////////////////////////////////////////
// Client messages: Sent from the client to the server
///////////////////////////////////////////////////////////////////////////////////////

typedef CNetMessagePB< clc_SplitPlayerConnect, CCLCMsg_SplitPlayerConnect >							CCLCMsg_SplitPlayerConnect_t;
typedef CNetMessagePB< clc_Move, CCLCMsg_Move, INetChannelInfo::MOVE, false >						CCLCMsg_Move_t;
typedef CNetMessagePB< clc_ClientInfo, CCLCMsg_ClientInfo > 										CCLCMsg_ClientInfo_t;
typedef CNetMessagePB< clc_VoiceData, CCLCMsg_VoiceData, INetChannelInfo::VOICE, false >			CCLCMsg_VoiceData_t;
typedef CNetMessagePB< clc_BaselineAck, CCLCMsg_BaselineAck >										CCLCMsg_BaselineAck_t;
typedef CNetMessagePB< clc_ListenEvents, CCLCMsg_ListenEvents >										CCLCMsg_ListenEvents_t;
typedef CNetMessagePB< clc_RespondCvarValue, CCLCMsg_RespondCvarValue >								CCLCMsg_RespondCvarValue_t;
typedef CNetMessagePB< clc_LoadingProgress, CCLCMsg_LoadingProgress >								CCLCMsg_LoadingProgress_t;
typedef CNetMessagePB< clc_CmdKeyValues, CCLCMsg_CmdKeyValues >										CCLCMsg_CmdKeyValues_t;
typedef CNetMessagePB< clc_HltvReplay, CCLCMsg_HltvReplay >											CCLCMsg_HltvReplay_t;

class CCLCMsg_FileCRCCheck_t : public CNetMessagePB< clc_FileCRCCheck, CCLCMsg_FileCRCCheck >
{
public:
	// Warning: These routines may use the va() function...
	static void SetPath( CCLCMsg_FileCRCCheck& msg, const char *path );
	static const char *GetPath( const CCLCMsg_FileCRCCheck& msg );
	static void SetFileName( CCLCMsg_FileCRCCheck& msg, const char *fileName );	
	static const char *GetFileName( const CCLCMsg_FileCRCCheck& msg );
};

///////////////////////////////////////////////////////////////////////////////////////
// Server messages: Sent from the server to the client
///////////////////////////////////////////////////////////////////////////////////////

typedef CNetMessagePB< svc_ServerInfo, CSVCMsg_ServerInfo, INetChannelInfo::SIGNON >					CSVCMsg_ServerInfo_t;
typedef CNetMessagePB< svc_ClassInfo, CSVCMsg_ClassInfo, INetChannelInfo::SIGNON >						CSVCMsg_ClassInfo_t;
typedef CNetMessagePB< svc_SendTable, CSVCMsg_SendTable, INetChannelInfo::SIGNON >					    CSVCMsg_SendTable_t;
typedef CNetMessagePB< svc_Print, CSVCMsg_Print, INetChannelInfo::GENERIC, false >						CSVCMsg_Print_t;
typedef CNetMessagePB< svc_SetPause, CSVCMsg_SetPause >													CSVCMsg_SetPause_t;
typedef CNetMessagePB< svc_SetView, CSVCMsg_SetView >												    CSVCMsg_SetView_t;
typedef CNetMessagePB< svc_CreateStringTable, CSVCMsg_CreateStringTable, INetChannelInfo::SIGNON >	    CSVCMsg_CreateStringTable_t;
typedef CNetMessagePB< svc_UpdateStringTable, CSVCMsg_UpdateStringTable, INetChannelInfo::STRINGTABLE >	CSVCMsg_UpdateStringTable_t;
typedef CNetMessagePB< svc_VoiceInit, CSVCMsg_VoiceInit, INetChannelInfo::SIGNON >						CSVCMsg_VoiceInit_t;
typedef CNetMessagePB< svc_VoiceData, CSVCMsg_VoiceData, INetChannelInfo::VOICE, false >				CSVCMsg_VoiceData_t;
typedef CNetMessagePB< svc_FixAngle, CSVCMsg_FixAngle, INetChannelInfo::GENERIC, false >			    CSVCMsg_FixAngle_t;
typedef CNetMessagePB< svc_Prefetch, CSVCMsg_Prefetch, INetChannelInfo::SOUNDS >					    CSVCMsg_Prefetch_t;
typedef CNetMessagePB< svc_CrosshairAngle, CSVCMsg_CrosshairAngle >									    CSVCMsg_CrosshairAngle_t;
typedef CNetMessagePB< svc_BSPDecal, CSVCMsg_BSPDecal >												    CSVCMsg_BSPDecal_t;
typedef CNetMessagePB< svc_SplitScreen, CSVCMsg_SplitScreen >										    CSVCMsg_SplitScreen_t;
typedef CNetMessagePB< svc_GetCvarValue, CSVCMsg_GetCvarValue >										    CSVCMsg_GetCvarValue_t;
typedef CNetMessagePB< svc_Menu, CSVCMsg_Menu, INetChannelInfo::GENERIC, false >					    CSVCMsg_Menu_t;
typedef CNetMessagePB< svc_UserMessage, CSVCMsg_UserMessage, INetChannelInfo::USERMESSAGES, false >		CSVCMsg_UserMessage_t;
typedef CNetMessagePB< svc_PaintmapData, CSVCMsg_PaintmapData, INetChannelInfo::PAINTMAP >			    CSVCMsg_PaintmapData_t;
typedef CNetMessagePB< svc_GameEvent, CSVCMsg_GameEvent, INetChannelInfo::EVENTS >					    CSVCMsg_GameEvent_t;
typedef CNetMessagePB< svc_GameEventList, CSVCMsg_GameEventList >									    CSVCMsg_GameEventList_t;
typedef CNetMessagePB< svc_TempEntities, CSVCMsg_TempEntities, INetChannelInfo::TEMPENTS, false >	    CSVCMsg_TempEntities_t;
typedef CNetMessagePB< svc_PacketEntities, CSVCMsg_PacketEntities, INetChannelInfo::ENTITIES >		    CSVCMsg_PacketEntities_t;
typedef CNetMessagePB< svc_Sounds, CSVCMsg_Sounds, INetChannelInfo::SOUNDS >							CSVCMsg_Sounds_t;
typedef CNetMessagePB< svc_EntityMessage, CSVCMsg_EntityMsg, INetChannelInfo::ENTMESSAGES, false >		CSVCMsg_EntityMsg_t;
typedef CNetMessagePB< svc_CmdKeyValues, CSVCMsg_CmdKeyValues >											CSVCMsg_CmdKeyValues_t;
typedef CNetMessagePB< svc_EncryptedData, CSVCMsg_EncryptedData, INetChannelInfo::ENCRYPTED >			CSVCMsg_EncryptedData_t;
typedef CNetMessagePB< svc_HltvReplay, CSVCMsg_HltvReplay, INetChannelInfo::ENTITIES >					CSVCMsg_HltvReplay_t;
typedef CNetMessagePB< svc_Broadcast_Command, CSVCMsg_Broadcast_Command, INetChannelInfo::STRINGCMD >	CSVCMsg_Broadcast_Command_t;


///////////////////////////////////////////////////////////////////////////////////////
// Utility classes
///////////////////////////////////////////////////////////////////////////////////////

class CmdKeyValuesHelper
{
public:

	static void CLCMsg_SetKeyValues( CCLCMsg_CmdKeyValues& msg, const KeyValues *keyValues );
	static KeyValues* CLCMsg_GetKeyValues ( const CCLCMsg_CmdKeyValues& msg );

	static void SVCMsg_SetKeyValues( CSVCMsg_CmdKeyValues& msg, const KeyValues *keyValues );
	static KeyValues *SVCMsg_GetKeyValues ( const CSVCMsg_CmdKeyValues& msg );
};

class INetChannel;
class CmdEncryptedDataMessageCodec
{
public:
	static bool SVCMsg_EncryptedData_EncryptMessage( CSVCMsg_EncryptedData_t &msgEncryptedResult, INetMessage *pMsgPlaintextInput, char const *key );
	static bool SVCMsg_EncryptedData_Process( CSVCMsg_EncryptedData const &msgEncryptedInput, INetChannel *pProcessingChannel, char const *key );
};

//////////////////////////////////////////////////////////////////////////
// Helper class to share network buffers in the entire process
//////////////////////////////////////////////////////////////////////////

class net_scratchbuffer_t
{
public:
	net_scratchbuffer_t()
	{
		m_pBufferNetMaxMessage = sm_NetScratchBuffers.Get();
		if ( !m_pBufferNetMaxMessage )
			m_pBufferNetMaxMessage = new buffer_t;
	}
	~net_scratchbuffer_t()
	{
		sm_NetScratchBuffers.PutObject( m_pBufferNetMaxMessage );
	}
	byte * GetBuffer() const
	{
		return m_pBufferNetMaxMessage->buf;
	}
	int Size() const
	{
		return NET_MAX_MESSAGE;
	}

private:
	struct buffer_t { byte buf[ NET_MAX_MESSAGE ]; };
	buffer_t *m_pBufferNetMaxMessage;	// buffer that is allocated and returned to shared pool

private:
	net_scratchbuffer_t( const net_scratchbuffer_t& );				// FORBID
	net_scratchbuffer_t& operator=( const net_scratchbuffer_t& );	// FORBID
	static CTSPool< buffer_t > sm_NetScratchBuffers;
};

#endif // NETMESSAGES_H

