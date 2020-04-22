//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "netmessages.h"
#include "bitbuf.h"
#include "const.h"
#include "../engine/net_chan.h"
#include "mathlib/mathlib.h"
#include "networkstringtabledefs.h"
#include "../engine/audio/public/sound.h"
#include "../engine/event_system.h"
#include "../engine/dt.h"
#include "mathlib/IceKey.H"
#include "tier0/vprof.h"


static char s_text[1024];


const char *g_MostCommonPathIDs[] =
{
	"GAME",
	"MOD"
};

const char *g_MostCommonPrefixes[] =
{
	"materials",
	"models",
	"sounds",
	"scripts"
};

static int FindCommonPathID( const char *pPathID )
{
	for ( int i = 0; i < ARRAYSIZE( g_MostCommonPathIDs ); i++ )
	{
		if ( V_stricmp( pPathID, g_MostCommonPathIDs[i] ) == 0 )
			return i;
	}
	return -1;
}

static int FindCommonPrefix( const char *pStr )
{
	for ( int i = 0; i < ARRAYSIZE( g_MostCommonPrefixes ); i++ )
	{
		if ( V_stristr( pStr, g_MostCommonPrefixes[i] ) == pStr  )
		{
			int iNextChar = V_strlen( g_MostCommonPrefixes[i] );
			if ( pStr[iNextChar] == '/' || pStr[iNextChar] == '\\' )
				return i;
		}
	}
	return -1;
}

void CCLCMsg_FileCRCCheck_t::SetPath( CCLCMsg_FileCRCCheck& msg, const char *path )
{
	int iCode = FindCommonPathID( path );
	if ( iCode == -1 )
	{
		msg.set_code_path( -1 );
		msg.set_path( path );
	}
	else
	{
		msg.set_code_path( iCode );
	}
}

const char *CCLCMsg_FileCRCCheck_t::GetPath( const CCLCMsg_FileCRCCheck& msg )
{
	int iCode = msg.code_path();
	if( ( iCode >= 0 ) && ( iCode < ARRAYSIZE( g_MostCommonPathIDs ) ) )
	{
		return g_MostCommonPathIDs[ iCode ];
	}

	Assert( msg.code_path() == -1 );
	return msg.path().c_str();
}

void CCLCMsg_FileCRCCheck_t::SetFileName( CCLCMsg_FileCRCCheck& msg, const char *fileName )
{
	int iCode = FindCommonPrefix( fileName );
	if ( iCode == -1 )
	{
		msg.set_code_filename( -1 );
		msg.set_filename( fileName );
	}
	else
	{
		msg.set_code_filename( iCode );
		msg.set_filename( &fileName[ V_strlen( g_MostCommonPrefixes[ iCode ] ) + 1 ] );
	}
}

const char *CCLCMsg_FileCRCCheck_t::GetFileName( const CCLCMsg_FileCRCCheck& msg )
{
	int iCode = msg.code_filename();
	if( ( iCode >= 0 ) && ( iCode < ARRAYSIZE( g_MostCommonPrefixes ) ) )
	{
		return va( "%s%c%s", g_MostCommonPrefixes[ iCode ], CORRECT_PATH_SEPARATOR, msg.filename().c_str() );
	}

	Assert( msg.code_filename() == -1 );
	return msg.filename().c_str();
}

void CmdKeyValuesHelper::CLCMsg_SetKeyValues( CCLCMsg_CmdKeyValues& msg, const KeyValues *keyValues )
{
	CUtlBuffer bufData;
	keyValues->WriteAsBinary( bufData );
	int numBytes = bufData.TellPut();
	msg.set_keyvalues( bufData.Base(), numBytes );
}

KeyValues *CmdKeyValuesHelper::CLCMsg_GetKeyValues ( const CCLCMsg_CmdKeyValues& msg )
{
	KeyValues *pKeyValues = new KeyValues( "" );

	const std::string& msgStr = msg.keyvalues();
	int numBytes = msgStr.size();

	CUtlBuffer bufRead( msgStr.data(), numBytes, CUtlBuffer::READ_ONLY );
	if ( !pKeyValues->ReadAsBinary( bufRead, 90 ) )	// we are expecting very few nest levels of keyvalues here!
	{
		Assert( false );
	}

	return pKeyValues;
}

void CmdKeyValuesHelper::SVCMsg_SetKeyValues( CSVCMsg_CmdKeyValues& msg, const KeyValues *keyValues )
{
	CUtlBuffer bufData;
	keyValues->WriteAsBinary( bufData );
	int numBytes = bufData.TellPut();
	msg.set_keyvalues( bufData.Base(), numBytes );
}

KeyValues *CmdKeyValuesHelper::SVCMsg_GetKeyValues ( const CSVCMsg_CmdKeyValues& msg )
{
	KeyValues *pKeyValues = new KeyValues( "" );

	const std::string& msgStr = msg.keyvalues();
	int numBytes = msgStr.size();

	CUtlBuffer bufRead( msgStr.data(), numBytes, CUtlBuffer::READ_ONLY );
	if ( !pKeyValues->ReadAsBinary( bufRead ) )
	{
		Assert( false );
	}

	return pKeyValues;
}

/*
bool CmdEncryptedDataMessageCodec::SVCMsg_EncryptedData_EncryptMessage( CSVCMsg_EncryptedData_t &msgEncryptedResult, const ::google::protobuf::Message &msgPlaintextInput )
{
	static char const *szEncryptedTag = "[[ENCRYPTED_DATA]]";
	static size_t nEncryptedLen = Q_strlen( szEncryptedTag );
	int32 const numBytesWritten = msgPlaintextInput.ByteSize();
	msgEncryptedResult.mutable_encrypted()->resize( nEncryptedLen + sizeof( int32 ) + numBytesWritten );

	Q_memcpy( &msgEncryptedResult.mutable_encrypted()->at(0), szEncryptedTag, nEncryptedLen );
	int32 const numBytesWrittenWire = BigLong( numBytesWritten );	// byteswap for the wire
	Q_memcpy( &msgEncryptedResult.mutable_encrypted()->at( nEncryptedLen ), &numBytesWrittenWire, sizeof( numBytesWrittenWire ) );
	return msgPlaintextInput.SerializeWithCachedSizesToArray( ( uint8 * ) &msgEncryptedResult.mutable_encrypted()->at( nEncryptedLen + sizeof( int32 ) ) );
}
*/

bool CmdEncryptedDataMessageCodec::SVCMsg_EncryptedData_EncryptMessage( CSVCMsg_EncryptedData_t &msgEncryptedResult, INetMessage *pMsgPlaintextInput, char const *key )
{
	// Prepare encryption key
	IceKey iceKey( 2 );
	if ( iceKey.keySize() != Q_strlen( key ) )
	{
		Warning( "CmdEncryptedDataMessageCodec key size is %d, but %d is expected!\n", Q_strlen( key ), iceKey.keySize() );
		return false; // we cannot encrypt with the supplied key
	}
	iceKey.set( ( const unsigned char * ) key );

	// supporting smaller stack
	net_scratchbuffer_t scratch;
	bf_write msg( "SVCMsg_EncryptedData_EncryptMessage", scratch.GetBuffer(), scratch.Size() );
	if ( !pMsgPlaintextInput->WriteToBuffer( msg ) )
		return false;
	int32 const numBytesWritten = msg.GetNumBytesWritten();
	
	// Generate some random fudge, ICE operates on 64-bit blocks, so make sure our total size is a multiple of 8 bytes
	int numRandomFudgeBytes = RandomInt( 16, 72 );
	int numTotalEncryptedBytes = 1 + numRandomFudgeBytes + sizeof( int32 ) + numBytesWritten;
	numRandomFudgeBytes += iceKey.blockSize() - ( numTotalEncryptedBytes % iceKey.blockSize() );
	numTotalEncryptedBytes = 1 + numRandomFudgeBytes + sizeof( int32 ) + numBytesWritten;

	char *pchRandomFudgeBytes = ( char * ) stackalloc( numRandomFudgeBytes );
	for ( int k = 0; k < numRandomFudgeBytes; ++ k )
		pchRandomFudgeBytes[k] = RandomInt( 16, 250 );
	
	msgEncryptedResult.mutable_encrypted()->resize( numTotalEncryptedBytes );

	msgEncryptedResult.mutable_encrypted()->at(0) = numRandomFudgeBytes;
	Q_memcpy( &msgEncryptedResult.mutable_encrypted()->at(1), pchRandomFudgeBytes, numRandomFudgeBytes );
	
	int32 const numBytesWrittenWire = BigLong( numBytesWritten );	// byteswap for the wire
	Q_memcpy( &msgEncryptedResult.mutable_encrypted()->at( 1 + numRandomFudgeBytes ), &numBytesWrittenWire, sizeof( numBytesWrittenWire ) );
	Q_memcpy( &msgEncryptedResult.mutable_encrypted()->at( 1 + numRandomFudgeBytes + sizeof( int32 ) ), msg.GetBasePointer(), numBytesWritten );

	// Encrypt the message
	unsigned char *pchCryptoBuffer = ( unsigned char * ) stackalloc( iceKey.blockSize() );
	for ( int k = 0; k < numTotalEncryptedBytes; k += iceKey.blockSize() )
	{
		iceKey.encrypt( ( const unsigned char * ) &msgEncryptedResult.mutable_encrypted()->at(k), pchCryptoBuffer );
		Q_memcpy( &msgEncryptedResult.mutable_encrypted()->at(k), pchCryptoBuffer, iceKey.blockSize() );
	}
	return true;
}

bool CmdEncryptedDataMessageCodec::SVCMsg_EncryptedData_Process( CSVCMsg_EncryptedData const &msgEncryptedInput, INetChannel *pProcessingChannelInterface, char const *key )
{
	CNetChan *pProcessingChannel = ( CNetChan * ) pProcessingChannelInterface;
	if ( !pProcessingChannel )
		return false;

	if ( !msgEncryptedInput.has_encrypted() )
		return true;

	if ( !key || !*key )
		return true; // key is not supplied, so ignore the message

	// Decrypt the message
	IceKey iceKey( 2 );
	if ( iceKey.keySize() != Q_strlen( key ) )
		return true; // we cannot decrypt with the supplied key
	iceKey.set( ( const unsigned char * ) key );
	if ( msgEncryptedInput.encrypted().size() % iceKey.blockSize() )
		return true; // message malformed, cannot decrypt
	if ( msgEncryptedInput.encrypted().size() > NET_MAX_PAYLOAD )
		return true; // size too large, cannot decrypt

	net_scratchbuffer_t scratch;
	byte *buffer = scratch.GetBuffer();
	unsigned char *pchCryptoBuffer = ( unsigned char * ) stackalloc( iceKey.blockSize() );
	for ( int k = 0; k < ( int ) msgEncryptedInput.encrypted().size(); k += iceKey.blockSize() )
	{
		iceKey.decrypt( ( const unsigned char * ) &msgEncryptedInput.encrypted().at(k), pchCryptoBuffer );
		Q_memcpy( &buffer[k], pchCryptoBuffer, iceKey.blockSize() );
	}

	// Check how much random fudge we have
	int numRandomFudgeBytes = *buffer;
	if ( ( numRandomFudgeBytes > 0 ) && ( numRandomFudgeBytes + 1 + sizeof( int32 ) < msgEncryptedInput.encrypted().size() ) )
	{
		// Fetch the size of the encrypted message
		int32 numBytesWrittenWire = 0;
		Q_memcpy( &numBytesWrittenWire, &buffer[ 1 + numRandomFudgeBytes ], sizeof( int32 ) );
		int32 const numBytesWritten = BigLong( numBytesWrittenWire );	// byteswap from the wire
		
		// Make sure the total size of the message matches the expectations
		if ( numRandomFudgeBytes + 1 + sizeof( int32 ) + numBytesWritten == msgEncryptedInput.encrypted().size() )
		{
			bf_read bufRead( &buffer[ 1 + numRandomFudgeBytes + sizeof( int32 ) ], numBytesWritten );
			unsigned char cmd = bufRead.ReadVarInt32();

			// See if the netchan has the required binder registered
			int iMsgHandler = 0;
			INetMessageBinder *pMsgBind = pProcessingChannel->FindMessageBinder( cmd, iMsgHandler++ );
			if ( pMsgBind )
			{
				int startbit = bufRead.GetNumBitsRead();

				INetMessage *pEmbeddedMessage = pMsgBind->CreateFromBuffer( bufRead );
				if ( !pEmbeddedMessage )
				{
					Msg( "CmdEncryptedDataMessageCodec failed to parse embedded message" );
					Assert( 0 );
					return false;
				}
				pEmbeddedMessage->SetReliable( pProcessingChannel->WasLastMessageReliable() );

				pProcessingChannel->UpdateMessageStats( pEmbeddedMessage->GetGroup(), bufRead.GetNumBitsRead() - startbit );

				do 
				{
					bool bRet = pMsgBind->Process( *pEmbeddedMessage );
					if ( !bRet )
					{
						ConDMsg( "CmdEncryptedDataMessageCodec: netchannel failed processing embedded message %s.\n", pEmbeddedMessage->GetName() );
						Assert ( 0 );
						delete pEmbeddedMessage;
						return false;
					}

					// Move to another binder
					pMsgBind = pProcessingChannel->FindMessageBinder( cmd, iMsgHandler++ );
				} while ( pMsgBind );

				delete pEmbeddedMessage;
			}
		}
	}

	return true;
}

CTSPool< net_scratchbuffer_t::buffer_t > net_scratchbuffer_t::sm_NetScratchBuffers;

#if defined( REPLAY_ENABLED )
bool CLC_SaveReplay::WriteToBuffer( bf_write &buffer ) const
{
	buffer.WriteUBitLong( GetType(), NETMSG_TYPE_BITS );
	buffer.WriteString( m_szFilename );
	buffer.WriteUBitLong( m_nStartSendByte, sizeof( m_nStartSendByte ) );
	buffer.WriteFloat( m_flPostDeathRecordTime );
	return !buffer.IsOverflowed();
}

bool CLC_SaveReplay::ReadFromBuffer( bf_read &buffer )
{
	buffer.ReadString( m_szFilename, sizeof( m_szFilename ) );
	m_nStartSendByte = buffer.ReadUBitLong( sizeof( m_nStartSendByte ) );
	m_flPostDeathRecordTime = buffer.ReadFloat();
	return !buffer.IsOverflowed();
}

const char *CLC_SaveReplay::ToString() const
{
	V_snprintf( s_text, sizeof( s_text ), "%s: filename: %s, start byte: %i, post death record time: %f", GetName(), m_szFilename, m_nStartSendByte, m_flPostDeathRecordTime );
	return s_text;
}
#endif
