//============ Copyright (c) Valve Corporation, All rights reserved. ============
//
// Tier1 logging helpers.
//
//===============================================================================

#include "tier1_logging.h"

CBufferedLoggingListener::CBufferedLoggingListener() :
m_StoredSpew( 0, 512, 0 )
{
}

void CBufferedLoggingListener::Log( const LoggingContext_t *pContext, const tchar *pMessage )
{
	m_StoredSpew.PutInt( pContext->m_ChannelID );
	m_StoredSpew.PutInt( pContext->m_Severity );
	m_StoredSpew.PutChar( pContext->m_Color.r() );
	m_StoredSpew.PutChar( pContext->m_Color.g() );
	m_StoredSpew.PutChar( pContext->m_Color.b() );
	m_StoredSpew.PutChar( pContext->m_Color.a() );
	m_StoredSpew.PutString( pMessage );
}

void CBufferedLoggingListener::EmitBufferedSpew()
{
	while ( m_StoredSpew.GetBytesRemaining() > 0 )
	{
		LoggingChannelID_t channelID = m_StoredSpew.GetInt();
		LoggingSeverity_t severity = ( LoggingSeverity_t )m_StoredSpew.GetInt();
		unsigned char r, g, b, a;
		r = m_StoredSpew.GetChar();
		g = m_StoredSpew.GetChar();
		b = m_StoredSpew.GetChar();
		a = m_StoredSpew.GetChar();
		Color color( r, g, b, a );

		int nLen = m_StoredSpew.PeekStringLength();
		if ( nLen )
		{
			char *pMessage = ( char * )stackalloc( nLen );
			m_StoredSpew.GetString( pMessage, nLen );
			LoggingSystem_LogDirect( channelID, severity, color, pMessage );
		}
	}

	m_StoredSpew.Clear();
}