//============ Copyright (c) Valve Corporation, All rights reserved. ============
//
// Tier1 logging helpers.
//
//===============================================================================

#ifndef TIER1_LOGGING_H
#define TIER1_LOGGING_H

#if defined( COMPILER_MSVC )
#pragma once
#endif

#include "logging.h"
#include "utlbuffer.h"
#include "color.h"

class CBufferedLoggingListener : public ILoggingListener
{
public:
	CBufferedLoggingListener();

	virtual void Log( const LoggingContext_t *pContext, const tchar *pMessage );

	void EmitBufferedSpew();

private:

	CUtlBuffer m_StoredSpew;
};

#endif // TIER1_LOGGING_H