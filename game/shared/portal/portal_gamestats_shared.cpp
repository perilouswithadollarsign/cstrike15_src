//====== Copyright © 1996-2006, Valve Corporation, All rights reserved. =======//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#if !defined( _GAMECONSOLE ) && !defined( NO_STEAM )
#ifdef GAME_DLL
#include "GameStats.h"
#endif
#include "portal_gamestats_shared.h"
#include "fmtstr.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"

//=============================================================================
//
// Helper functions for creating key values
//
void AddDataToKV( KeyValues* pKV, const char* name, int data )
{
	pKV->SetInt( name, data );
}
void AddDataToKV( KeyValues* pKV, const char* name, uint64 data )
{
	pKV->SetUint64( name, data );
}
void AddDataToKV( KeyValues* pKV, const char* name, float data )
{
	pKV->SetFloat( name, data );
}
void AddDataToKV( KeyValues* pKV, const char* name, bool data )
{
	pKV->SetBool( name, data );
}
void AddDataToKV( KeyValues* pKV, const char* name, const char* data )
{
	pKV->SetString( name, data );
}
void AddDataToKV( KeyValues* pKV, const char* name, const Color& data )
{
	pKV->SetColor( name, data );
}
void AddDataToKV( KeyValues* pKV, const char* name, short data )
{
	pKV->SetInt( name, data );
}
void AddDataToKV( KeyValues* pKV, const char* name, unsigned data )
{
	pKV->SetInt( name, data );
}
void AddPositionDataToKV( KeyValues* pKV, const char* name, const Vector &data )
{
	// Append the data name to the member
	pKV->SetFloat( CFmtStr("%s%s", name, "_X"), data.x );
	pKV->SetFloat( CFmtStr("%s%s", name, "_Y"), data.y );
	pKV->SetFloat( CFmtStr("%s%s", name, "_Z"), data.z );
}

//=============================================================================//

//=============================================================================
//
// Helper functions for creating key values from arrays
//
void AddArrayDataToKV( KeyValues* pKV, const char* name, const short *data, unsigned size )
{
	for( unsigned i=0; i<size; ++i )
		pKV->SetInt( CFmtStr("%s_%d", name, i) , data[i] );
}
void AddArrayDataToKV( KeyValues* pKV, const char* name, const byte *data, unsigned size )
{
	for( unsigned i=0; i<size; ++i )
		pKV->SetInt( CFmtStr("%s_%d", name, i), data[i] );
}
void AddArrayDataToKV( KeyValues* pKV, const char* name, const unsigned *data, unsigned size )
{
	for( unsigned i=0; i<size; ++i )
		pKV->SetInt( CFmtStr("%s_%d", name, i), data[i] );
}
void AddStringDataToKV( KeyValues* pKV, const char* name, const char*data )
{
	if( name == NULL )
		return;

	pKV->SetString( name, data );
}
//=============================================================================//


void IGameStatTracker::PrintGamestatMemoryUsage( void )
{
	StatContainerList_t* pStatList = GetStatContainerList();
	if( !pStatList )
		return;

	int iListSize = pStatList->Count();

	// For every stat list being tracked, print out its memory usage
	for( int i=0; i < iListSize; ++i )
	{
		pStatList->operator []( i )->PrintMemoryUsage();
	}
}

#endif // _GAMECONSOLE
