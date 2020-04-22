//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "engine/iblackbox.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern IBlackBox *blackboxrecorder;

void BlackBox_Record( const char *type, PRINTF_FORMAT_STRING const char *pFormat, ... )
{
	static ConVarRef blackbox( "blackbox" );

	if ( IsX360() )
		return;

	if ( !blackbox.IsValid() || !blackbox.GetBool() )
		 return;

	int type_num;
	for ( type_num = 0; type_num < blackboxrecorder->GetTypeCount(); type_num++ )
	{
		if ( !V_strcasecmp( blackboxrecorder->GetTypeName( type_num ), type ) )
			break;
	}

	if ( type_num >= blackboxrecorder->GetTypeCount() )
	{
		Msg( "Invalid blackbox type: %s\n", type );
		return;
	}

	char szMessage[1024];	
	va_list marker;

	va_start( marker, pFormat);
	Q_vsnprintf( szMessage, sizeof( szMessage ), pFormat, marker);
	va_end( marker );	

	//Msg( "Record: %s: %s\n", type, szMessage );
	blackboxrecorder->Record( type_num, szMessage );
}

