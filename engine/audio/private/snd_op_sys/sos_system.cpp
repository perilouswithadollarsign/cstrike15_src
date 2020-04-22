 //============ Copyright (c) Valve Corporation, All rights reserved. ============
//
//
//
//===============================================================================
#include "audio_pch.h"

#include "snd_dma.h"
#include "sos_system.h"
#include "sos_op.h"
#include "sound.h"
#include "../../debugoverlay.h"


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


Color CollectionColor( 245, 245, 245, 255 );
Color StackColor( 225, 225, 245, 255 );

ConVar snd_sos_show_operator_init("snd_sos_show_operator_init", "0", FCVAR_CHEAT );
ConVar snd_sos_show_operator_start("snd_sos_show_operator_start", "0", FCVAR_CHEAT );
ConVar snd_sos_show_operator_prestart("snd_sos_show_operator_prestart", "0", FCVAR_CHEAT );
ConVar snd_sos_show_operator_updates("snd_sos_show_operator_updates", "0", FCVAR_CHEAT );
ConVar snd_sos_show_operator_shutdown("snd_sos_show_operator_shutdown", "0", FCVAR_CHEAT );
ConVar snd_sos_show_operator_parse("snd_sos_show_operator_parse", "0", FCVAR_CHEAT );

ConVar snd_sos_list_operator_updates("snd_sos_list_operator_updates", "0", FCVAR_CHEAT );

ConVar snd_sos_show_operator_entry_filter( "snd_sos_show_operator_entry_filter", "", FCVAR_CHEAT );


extern CScratchPad g_scratchpad;

//-----------------------------------------------------------------------------
// CSosOperatorStack
//-----------------------------------------------------------------------------
CSosOperatorStack::CSosOperatorStack( SosStackType_t SosType, stack_data_t &stackData )
{
	m_SOSType = SosType;
	m_pMemPool = NULL;
	m_nMemSize = 0;
	m_stopType = SOS_STOP_NONE;
	m_flStopTime = -1.0;
	m_nChannelGuid = stackData.m_nGuid;
	m_flStartTime = stackData.m_flStartTime;
	m_nScriptHash = stackData.m_nSoundScriptHash;
	m_pOperatorsKV = stackData.m_pOperatorsKV;

}
CSosOperatorStack::~CSosOperatorStack()
{
	g_pSoundOperatorSystem->RemoveChannelFromTracks( m_nChannelGuid );
	
	if ( m_pMemPool )
	{
		free( m_pMemPool );
	}
}

void CSosOperatorStack::SetStopType( SOSStopType_t stopType )
{
	if( stopType != SOS_STOP_NONE )
	{
		m_stopType = stopType;
	}
}
void CSosOperatorStack::SetScriptHash( HSOUNDSCRIPTHASH nHash )
 {
	 m_nScriptHash = nHash;
	 KeyValues *pOperatorsKV = g_pSoundEmitterSystem->GetOperatorKVByHandle( nHash );
	 m_pOperatorsKV = pOperatorsKV;
}
void CSosOperatorStack::AddToTail( CSosOperator *pOperator, const char *pName )
{
	int nIndex = m_vStack.AddToTail( pOperator );
	m_vOperatorMap.Insert( pName , nIndex );
}

int CSosOperatorStack::FindOperatorViaOffset( size_t nOffset )
{
	size_t memOffset = 0;
	int i;
	for( i = 0; i < m_vStack.Count(); i++ )
	{
		CSosOperator *pOp = m_vStack[i];
		if ( pOp )
		{
			memOffset += pOp->GetSize();
			if( memOffset > nOffset )
			{
				break;
			}
		}
	}
	return i;

}

CSosOperator *CSosOperatorStack::FindOperator( const char *pName, void **pStructHandle )
{
	int nOpIndex = m_vOperatorMap.Find( pName );
	size_t memOffset = 0;
	*pStructHandle = m_pMemPool;
	for( int i = 0; i < nOpIndex; i++ )
	{
		CSosOperator *pOp = m_vStack[i];
		if ( pOp )
		{
			memOffset += pOp->GetSize();
			char *pStructMem = ((char *)m_pMemPool) + memOffset;
			*pStructHandle = (void *)pStructMem;
		}
		else
		{
			// error!
			return NULL;
		}
	}
	return m_vStack[nOpIndex];

}
int CSosOperatorStack::FindOperator( const char *pName )
{
	int nIndex = m_vOperatorMap.Find( pName );
	if ( !m_vStack.IsValidIndex( nIndex ) )
	{
		// error
		return -1;
	}
	return nIndex;

}
int CSosOperatorStack::GetOperatorOffset( int nIndex )
{
	if ( !m_vStack.IsValidIndex( nIndex ) )
	{
		// error
		return -1;
	}

	size_t memOffset = 0;
	for ( int i = 0; i < nIndex; i++ )
	{
		CSosOperator *pOp = m_vStack[i];
		if ( pOp )
		{
			memOffset += pOp->GetSize();
		}
	}
	return memOffset;
}

int CSosOperatorStack::GetOperatorOutputOffset( const char *pOperatorName, const char *pOutputName )
{
	int nOpIndex = FindOperator( pOperatorName );
	if ( ! m_vStack.IsValidIndex( nOpIndex ) )
	{
		Log_Warning( LOG_SND_OPERATORS, "Error: Unable to find referenced operator: %s", pOperatorName );
		return -1;
	}
	CSosOperator *pOperator = m_vStack[ nOpIndex ];
	size_t nOpOffset = GetOperatorOffset( nOpIndex );
	size_t nOutputOffset = pOperator->GetOutputOffset( pOutputName );

	return nOpOffset + nOutputOffset;

}
void CSosOperatorStack::Init( )
{
	size_t TotalSize = 0;
	for( int i = 0 ; i < m_vStack.Count(); i++ )
	{
		char *pCharMem = ((char *)m_pMemPool) + TotalSize;

		m_vStack[i]->ResolveInputValues( (void *)pCharMem, m_pMemPool );
		m_vStack[i]->StackInit( (void *)pCharMem, this, i );
		TotalSize += m_vStack[i]->GetSize();
	}




}
void CSosOperatorStack::Shutdown( )
{
	// 	if ( snd_sos_show_operator_updates.GetInt() )
	// 	{
	// 		Log_Msg( LOG_SND_OPERATORS, StackColor, "Operator Stack: %s\n", m_nName );
	// 	}

	size_t TotalSize = 0;
	for( int i = 0 ; i < m_vStack.Count(); i++ )
	{
		char *pCharMem = ((char *)m_pMemPool) + TotalSize;

		m_vStack[i]->ResolveInputValues( (void *)pCharMem, m_pMemPool );
		m_vStack[i]->StackShutdown( (void *)pCharMem, this, i );
		TotalSize += m_vStack[i]->GetSize();
	}
}

bool CSosOperatorStack::ShouldPrintOperators( ) const
{
	const char *pFilterString = snd_sos_show_operator_entry_filter.GetString();

	if( !pFilterString || !pFilterString[0] )
	{
		return true;
	}
	if( V_stristr( g_pSoundEmitterSystem->GetSoundNameForHash( GetScriptHash() ), pFilterString ) )
	{
		return true;
	}
	return false;
}

bool CSosOperatorStack::ShouldPrintOperatorUpdates( ) const
{
	if ( !snd_sos_show_operator_updates.GetInt() )
	{
		return false;
	}

	return ShouldPrintOperators();

}
void CSosOperatorStack::Execute( channel_t *pChannel, CScratchPad *pScratchPad )
{
	VPROF ( "CSosOperatorStack::Execute" );

	if ( ShouldPrintOperatorUpdates( ) || snd_sos_list_operator_updates.GetInt() )
	{
		Color SEColor(255, 255, 145, 255);
		Log_Msg( LOG_SND_OPERATORS, SEColor, "\nUpdate Sound Entry: %s\n", g_pSoundEmitterSystem->GetSoundNameForHash( GetScriptHash() ) );
		Log_Msg( LOG_SND_OPERATORS, StackColor, "Operator Stack: %s\n", m_nName );
	}
	if( !pChannel )
	{
		pChannel = S_FindChannelByGuid( m_nChannelGuid );
	}

	size_t TotalSize = 0;
	for( int i = 0 ; i < m_vStack.Count(); i++ )
	{
		char *pCharMem = ((char *)m_pMemPool) + TotalSize;
		CSosOperator_t *pStructMem = (CSosOperator_t *)pCharMem;

		m_vStack[i]->ResolveInputValues( (void *)pCharMem, m_pMemPool );
		if( pStructMem->m_flExecute[0] > 0.0 && !( pStructMem->m_bExecuteOnce && pStructMem->m_bHasExecuted ) )
		{
			m_pCurrentOperatorName = m_vOperatorMap.GetElementName( i );
			m_vStack[i]->Execute( (void *)pCharMem, pChannel, pScratchPad, this, i );
			pStructMem->m_bHasExecuted = true;

			if( ShouldPrintOperatorUpdates( ) )
			{
				m_vStack[i]->Print( (void *)pCharMem, this, i, 0 );
			}
			// diagnostics
	
		}
		else if( ShouldPrintOperatorUpdates( ) )
		{
			m_vStack[i]->Print( (void *)pCharMem, this, i, 1 );
		}

		TotalSize += m_vStack[i]->GetSize();
	}

}

void CSosOperatorStack::ExecuteIterator( channel_t *pChannel, CScratchPad *pScratchPad, const void *pStopMemBlock, const char *pOperatorName, int * pnOperatorIndex )
{
	if ( ShouldPrintOperatorUpdates( ) || snd_sos_list_operator_updates.GetInt() )
	{
		Color SEColor(255, 255, 145, 255);
		Log_Msg( LOG_SND_OPERATORS, SEColor, "\nUpdate Sound Entry: %s\n", g_pSoundEmitterSystem->GetSoundNameForHash( GetScriptHash() ) );
		Log_Msg( LOG_SND_OPERATORS, StackColor, "Operator Stack: %s\n", m_nName );
	}

	int nIndex = *pnOperatorIndex;
	if ( nIndex < 0 )
	{
		nIndex = FindOperator( pOperatorName );
		if ( nIndex < 0 )
		{
			Log_Warning(LOG_SND_OPERATORS, "Error: Execute iterator unable to find sound operator %s\n", pOperatorName );
			return;
		}
		*pnOperatorIndex = nIndex;	// Update the passed operator index
	}
	else
	{
		Assert( nIndex == FindOperator( pOperatorName ) );
	}

	size_t TotalSize = 0;
	for( int i = 0; i < m_vStack.Count(); i++ )
	{
		char *pCharMem = ((char *)m_pMemPool) + TotalSize;
		CSosOperator_t *pStructMem = (CSosOperator_t *)pCharMem;

		if ( i >= nIndex )
		{
			m_vStack[i]->ResolveInputValues( (void *)pCharMem, m_pMemPool );
			if( pStructMem->m_flExecute[0] > 0.0 && !( pStructMem->m_bExecuteOnce && pStructMem->m_bHasExecuted ) )
			{
				m_pCurrentOperatorName = m_vOperatorMap.GetElementName( i );;
				m_vStack[i]->Execute( (void *)pCharMem, pChannel, pScratchPad, this, i );
				pStructMem->m_bHasExecuted = true;

				// diagnostics
				if ( ShouldPrintOperatorUpdates( ) )
				{
					m_vStack[i]->Print( (void *)pCharMem, this, i, 0 );
				}
			}
			else if( ShouldPrintOperatorUpdates( ) )
			{
				const char *pFilterString = snd_sos_show_operator_entry_filter.GetString();
				if( pFilterString && pFilterString[0] && 
					V_stristr( g_pSoundEmitterSystem->GetSoundNameForHash( GetScriptHash() ), pFilterString ) )
				{
					m_vStack[i]->Print( (void *)pCharMem, this, i, 1 );
				}
			}
			if( pCharMem == pStopMemBlock )
			{
				return;
			}
		}
		TotalSize += m_vStack[i]->GetSize();
	}
}

void CSosOperatorStack::Copy( CSosOperatorStack *pSrcStack, size_t nMemOffset )
{
	// copy first so we can alter
	V_memcpy((char *)m_pMemPool + nMemOffset, pSrcStack->m_pMemPool, pSrcStack->m_nMemSize );
	
	size_t nTotalOffset = 0;

	// UGLY, MAKE POINTERS!
	for ( int i = 0; i < pSrcStack->m_vStack.Count(); i++ )
	{
 		CSosOperator *pOperator = pSrcStack->m_vStack[i];
		const char *pName = pSrcStack->m_vOperatorMap.GetElementName( i );

		// same names not allowed?
		int nIndex = m_vOperatorMap.Find( pName );
		if ( m_vOperatorMap.IsValidIndex( nIndex ) )
		{
			Log_Warning( LOG_SND_OPERATORS, "Error: Importing operator with same name as existing operator: %s\n", pName );
		}

		AddToTail( pOperator, pName );
		pOperator->OffsetConnections( (char *)m_pMemPool + nMemOffset + nTotalOffset, nMemOffset );

		nTotalOffset += pOperator->GetSize(); 
	}
}

void CSosOperatorStack::Print( int nLevel )
{
	if( !ShouldPrintOperators() )
	{
		return;
	}
	Log_Msg( LOG_SND_OPERATORS, StackColor, "\n%*sOperator Stack: %s\n%*sSize: %i\n", nLevel, "    ", m_nName, nLevel, "    ", static_cast<int>( m_nMemSize ) );
	size_t TotalSize = 0;
	for ( int i = 0 ; i < m_vStack.Count(); i++ )
	{
		m_vStack[i]->Print( (void *)((char *)m_pMemPool + TotalSize), this, i, nLevel );
		TotalSize += m_vStack[i]->GetSize();
	}
}

enum sosStackCreationPasses_t
{
	SND_SOS_MEMORY_ALLOCATION_PASS = 0,
	SND_SOS_MEMORY_INITIALIZATION_PASS,
	SND_SOS_TOTAL_MEMORY_PASSES

};

void CSosOperatorStack::ParseKV( KeyValues *pOperatorsKV )
{
	size_t TotalSize = 0;
	KeyValues *pOperator = pOperatorsKV->GetFirstSubKey();

	CUtlDict < CSosOperator * > testDict;

	// 2 passes:
	//
	// SND_SOS_MEMORY_ALLOCATION_PASS:
	//
	//    Sum up the total memory necessary for this stack by
	//    accumulating imported stack sizes and newly declared
	//    operator sizes. Then allocate the resulting quantity of memory.
	//
	// SND_SOS_MEMORY_INITIALIZATION_PASS:
	//
	//    Copy the existing stack and operator data into the newly
	//    allocated block of stack memory. Properly set all parameters
	//    via defaults, parsing and updating connection offsets.
	//
	for ( int i = 0; i < SND_SOS_TOTAL_MEMORY_PASSES; i++ )
	{
		while ( pOperator )
		{
			const char *pOpName = pOperator->GetName();
			const char *pOpValue = pOperator->GetString();

			// if it has subkeys it's an operator, otw it's a parameter
			KeyValues *pParams = pOperator->GetFirstSubKey();
			if ( pParams )
			{
				// do we already have this one and just need to override params?
				int nNewStackIndex = testDict.Find( pOpName );
				int nExistingStackIndex = m_vOperatorMap.Find( pOpName );

				// is an override of an operator existing in the stack
				if ( m_vOperatorMap.IsValidIndex( nExistingStackIndex ) )
				{
					// only the second time through
					if ( i == SND_SOS_MEMORY_INITIALIZATION_PASS )
					{
						m_pCurrentOperatorName = pOpName;
						void *pStructMem;
						CSosOperator *pNewOp = FindOperator( pOpName, &pStructMem );
						// only overwriting
						pNewOp->ParseKV( this, pStructMem, pOperator );
					}
				}
				else if( !( testDict.IsValidIndex( nNewStackIndex ) && i == SND_SOS_MEMORY_ALLOCATION_PASS ) ) // do nothing if override of new op on first memory accumulating pass
				{
					CSosOperator *pNewOp = NULL;
					const char *pOpType = NULL;
					if( pOperator->FindKey( "operator" ) )
					{
						pOpType = pOperator->GetString( "operator", "" );
					}
					if( pOpType && *pOpType )
					{
						int nElementIndex = g_pSoundOperatorSystem->m_vOperatorCollection.Find( pOpType );
						if ( g_pSoundOperatorSystem->m_vOperatorCollection.IsValidIndex( nElementIndex ) )
						{
							pNewOp = g_pSoundOperatorSystem->m_vOperatorCollection[ nElementIndex ];
						}
						else
						{
							Log_Warning( LOG_SND_OPERATORS, "Error: Operator: %s : Unknown sound operator type %s\n", pOpName, pOpType );

						}
						// is a new operator
						if ( pNewOp )
						{
							if ( i == SND_SOS_MEMORY_ALLOCATION_PASS )
							{	
								// new operator
								testDict.Insert( pOperator->GetName(), pNewOp );
							}
							else if ( i == SND_SOS_MEMORY_INITIALIZATION_PASS )
							{

								// new operator
								m_pCurrentOperatorName = pOpName;
								pNewOp->SetBaseDefaults( (void *)(((char *)m_pMemPool) + TotalSize) );
								pNewOp->SetDefaults( (void *)(((char *)m_pMemPool) + TotalSize) );
								pNewOp->ParseKV( this, (void *)(((char *)m_pMemPool) + TotalSize), pOperator );
								AddToTail( pNewOp, pOperator->GetName() );
							}
							TotalSize += pNewOp->GetSize();
						}
					}
				}
			}
			else if ( pOpName && *pOpName )
			{
 				if ( !V_stricmp( pOpName, "import_stack" ) )
				{
					if ( pOpValue && *pOpValue )
					{
						CSosOperatorStack *pImportStack = g_pSoundOperatorSystem->m_MasterStackCollection.GetStack( pOpValue );

						if ( pImportStack )
						{
							if ( i == SND_SOS_MEMORY_INITIALIZATION_PASS )
							{
								Copy( pImportStack, TotalSize );
							}
							TotalSize += pImportStack->GetSize();
						}
					}
				}
			}
			pOperator = pOperator->GetNextKey();
		}
		// first time through
		if ( i == SND_SOS_MEMORY_ALLOCATION_PASS )
		{
			if ( TotalSize > 0 )
			{
				m_pMemPool = malloc( TotalSize );
				m_nMemSize = TotalSize;
			}
			TotalSize = 0;
			pOperator = pOperatorsKV->GetFirstSubKey();
		}
	}
}

KeyValues *S_GetStopTracksKV( KeyValues *pOperatorsKV )
{
	if( !pOperatorsKV )
	{
		if( snd_sos_show_operator_parse.GetInt() )
		{
			Log_Warning( LOG_SND_OPERATORS, "Error: Sound Operator System has invalid operator KV\n" );
		}
		return NULL;
	}
	KeyValues *pOperatorDataKV = pOperatorsKV->FindKey( "soundentry_operator_data" );
	if( ! pOperatorDataKV )
	{
		//		Log_Warning( LOG_SND_OPERATORS, "Error: Sound Operator System cannot find \"soundentry_operator_data\"\n" );
		return NULL;
	}

	KeyValues *pTrackDataKV = pOperatorDataKV->FindKey("track_data" );
	if( ! pTrackDataKV )
	{
		return NULL;
	}
	KeyValues *pStopTracksKV = pTrackDataKV->FindKey("stop_tracks_on_start" );
	if( ! pStopTracksKV )
	{
		return NULL;
	}


	KeyValues *pStopTracksList = pStopTracksKV->GetFirstSubKey();
	return pStopTracksList;
// 
// 	while ( pStopTracksList )
// 	{
// 		const char *pStopTracksTypeString = pStopTracksList->GetName();
// 
// 		if ( pStopTracksTypeString && *pStopTracksTypeString )
// 		{
// 			if ( !V_strcmp( pStopTracksTypeString, pListName ) )
// 			{
// 				return pStopTracksList;
// 			}
// 
// 		}
// 		pStopTracksList = pStopTracksList->GetNextKey();
// 	}
// 
// 
// 	return NULL;
}

KeyValues *S_GetStopTracksKV( HSOUNDSCRIPTHASH nSoundEntryHash )
{
	if( nSoundEntryHash != SOUNDEMITTER_INVALID_HASH )
	{
		KeyValues *pOperatorKV = g_pSoundEmitterSystem->GetOperatorKVByHandle( nSoundEntryHash );
		if( pOperatorKV )
		{
			return S_GetStopTracksKV( pOperatorKV );
		}
	}
	return NULL;
}


KeyValues *CSosOperatorStack::GetSyncPointsKV( KeyValues *pOperatorsKV, const char *pListName )
{
	if( !pOperatorsKV )
	{
		if( snd_sos_show_operator_parse.GetInt() )
		{
			Log_Warning( LOG_SND_OPERATORS, "Error: Sound Operator System has invalid operator KV\n" );
		}
		return NULL;
	}
	KeyValues *pOperatorDataKV = pOperatorsKV->FindKey( "soundentry_operator_data" );
	if( ! pOperatorDataKV )
	{
//		Log_Warning( LOG_SND_OPERATORS, "Error: Sound Operator System cannot find \"soundentry_operator_data\"\n" );
		return NULL;
	}

	KeyValues *pTrackDataKV = pOperatorDataKV->FindKey("track_data" );
	if( ! pTrackDataKV )
	{
		return NULL;
	}
	KeyValues *pSyncPointsKV = pTrackDataKV->FindKey("syncpoints" );
	if( ! pSyncPointsKV )
	{
		return NULL;
	}


	KeyValues *pSyncPointList = pSyncPointsKV->GetFirstSubKey();
	while ( pSyncPointList )
	{
		const char *pStackListTypeString = pSyncPointList->GetName();

		if ( pStackListTypeString && *pStackListTypeString )
		{
			if ( !V_strcmp( pStackListTypeString, pListName ) )
			{
				return pSyncPointList;
			}

		}
		pSyncPointList = pSyncPointList->GetNextKey();
	}


	return NULL;
}

KeyValues *CSosOperatorStack::GetSyncPointsKV( const char *pListName )
{
	KeyValues *pOperatorsKV = GetOperatorsKV();
	return GetSyncPointsKV( pOperatorsKV, pListName );
}

bool S_GetTrackData( KeyValues *pOperatorsKV, track_data_t &trackData )
{
	trackData.SetDefaults();

	if( !pOperatorsKV )
	{
		if( snd_sos_show_operator_parse.GetInt() )
		{
			Log_Warning( LOG_SND_OPERATORS, "Error: Sound Operator System has invalid operator KV\n" );
		}
		return false;
	}
	KeyValues *pOperatorDataKV = pOperatorsKV->FindKey( "soundentry_operator_data" );
	if( ! pOperatorDataKV )
	{
		//		Log_Warning( LOG_SND_OPERATORS, "Error: Sound Operator System cannot find \"soundentry_operator_data\"\n" );
		return false;
	}

	KeyValues *pTrackDataKV = pOperatorDataKV->FindKey("track_data" );
	if( ! pTrackDataKV )
	{
		return false;
	}
	const char *pTrackNumber = pTrackDataKV->GetString( "track_name", "-1" );
	trackData.m_pTrackName = pTrackNumber;
	if( pTrackNumber )
	{
		trackData.m_nTrackNumber = V_atoi( pTrackNumber );
	}
	const char *pTrackPriority = pTrackDataKV->GetString( "priority", "-1" );
	if( pTrackPriority )
	{
		trackData.m_nTrackPriority = V_atoi( pTrackPriority );
	}
	const char *pOverridePriority = pTrackDataKV->GetString( "priority_override", "-1" );
	if( pOverridePriority )
	{
		if ( !V_strcasecmp( pOverridePriority, "true" ) )
		{
			trackData.m_bPriorityOverride = true;
		}
	}
	const char *pBlockEqualPriority = pTrackDataKV->GetString( "block_equal_priority", "-1" );
	if( pBlockEqualPriority )
	{
		if ( !V_strcasecmp( pBlockEqualPriority, "true" ) )
		{
			trackData.m_bBlockEqualPriority = true;
		}
	}
	const char *pSyncTrackNumber = pTrackDataKV->GetString( "sync_track_name", "-1" );
	trackData.m_pSyncTrackName = pSyncTrackNumber;
	if( pSyncTrackNumber )
	{
		trackData.m_nSyncTrackNumber = V_atoi( pSyncTrackNumber );
	}
	const char *pStartPoint = pTrackDataKV->GetString( "start_point", "0.0" );
	if( pStartPoint )
	{
		trackData.m_flStartPoint = V_atof( pStartPoint );
	}
	const char *pEndPoint = pTrackDataKV->GetString( "end_point", "0.0" );
	if( pEndPoint )
	{
		trackData.m_flEndPoint = V_atof( pEndPoint );
	}
	return true;
}

void CSosOperatorStack::GetTrackData( track_data_t &trackData ) const
{
	KeyValues *pOperatorsKV = GetOperatorsKV();
	S_GetTrackData( pOperatorsKV, trackData );
}

bool S_GetTrackData( HSOUNDSCRIPTHASH nSoundEntryHash, track_data_t &trackData )
{
	if( nSoundEntryHash != SOUNDEMITTER_INVALID_HASH )
	{
		KeyValues *pOperatorKV = g_pSoundEmitterSystem->GetOperatorKVByHandle( nSoundEntryHash );
		if( pOperatorKV )
		{
			return S_GetTrackData( pOperatorKV, trackData );
		}
	}
	return false;
}


bool S_TrackHasPriority( track_data_t &newTrackData, track_data_t &existingTrackData )
{
	if( !( newTrackData.m_bPriorityOverride && existingTrackData.m_bPriorityOverride ) )
	{
	if( newTrackData.m_bPriorityOverride )
	{
		return true;
	}
	if( existingTrackData.m_bPriorityOverride )
	{
 		return false;
 	}
	}

	if( existingTrackData.m_bBlockEqualPriority && ( newTrackData.m_nTrackPriority == existingTrackData.m_nTrackPriority ) )
	{
		return false;
	}

	if( newTrackData.m_nTrackPriority >= existingTrackData.m_nTrackPriority )
	{
		return true;
	}
	return false;
}


//-----------------------------------------------------------------------------
// CSosOperatorStackList
//-----------------------------------------------------------------------------

CSosOperatorStackList::CSosOperatorStackList()
{
	m_vUpdateStack = NULL;
// 	m_vStartStack = NULL;
	m_vStopStack  = NULL;
// 	m_vCueStack = NULL;
	m_stopType = SOS_STOP_NONE;
	m_flStopTime = -1.0;
}
CSosOperatorStackList::~CSosOperatorStackList()
{

// 	if ( m_vCueStack )
// 	{
// 		m_vCueStack->Shutdown();
// 		delete m_vCueStack;
// 	}
// 	if ( m_vStartStack )
// 	{
// 		m_vStartStack->Shutdown();
// 		delete m_vStartStack;
// 	}
	if ( m_vUpdateStack )
	{
		m_vUpdateStack->Shutdown();
		delete m_vUpdateStack;
	}
	if ( m_vStopStack )
	{
		m_vStopStack->Shutdown();
		delete m_vStopStack;
	}
}



void CSosOperatorStackList::Execute( CSosOperatorStack::SosStackType_t SosType, channel_t *pChannel, CScratchPad *pScratchPad )
{
	switch( SosType )
	{
	case CSosOperatorStack::SOS_UPDATE:
		if ( m_vUpdateStack )
		{
			m_vUpdateStack->SetStopType( m_stopType );
			m_vUpdateStack->Execute( pChannel, pScratchPad );
			SetStopType( m_vUpdateStack->GetStopType( ) );
		}
		break;
// 	case CSosOperatorStack::SOS_START:
// 		if ( m_vStartStack )
// 		{
// 			m_vStartStack->Execute( pChannel, pScratchPad );
// 		}
// 		break;
	case CSosOperatorStack::SOS_STOP:
		if ( m_vStopStack )
		{
			m_vStopStack->Execute( pChannel, pScratchPad );
		}
		break;
// 	case CSosOperatorStack::SOS_CUE:
// 		if ( m_vCueStack )
// 		{
// 			m_vCueStack->Execute( pChannel, pScratchPad );
// 		}
// 		break;

	default:
		break;
	}
}

bool CSosOperatorStackList::HasStack( CSosOperatorStack::SosStackType_t SosType )
{
	switch( SosType )
	{
	case CSosOperatorStack::SOS_UPDATE:
		if ( m_vUpdateStack )
		{
			return true;
		}
		break;
// 	case CSosOperatorStack::SOS_START:
// 		if ( m_vStartStack )
// 		{
// 			return true;
// 		}
// 		break;

	case CSosOperatorStack::SOS_STOP:
		if ( m_vStopStack )
		{
			return true;
		}
		break;
// 	case CSosOperatorStack::SOS_CUE:
// 		if ( m_vCueStack )
// 		{
// 			return true;
// 		}
// 		break;
	default:
		return false;
	}
	return false;
}


void CSosOperatorStackList::Print()
{
	Log_Msg( LOG_SOUND_OPERATOR_SYSTEM, StackColor, "\nStack List:\n");

// 	Log_Msg( LOG_SOUND_OPERATOR_SYSTEM, StackColor, "\nCUE Operators:\n");
// 	if ( m_vCueStack )
// 	{
// 		m_vCueStack->Print( 1 );
// 	}
// 	Log_Msg( LOG_SOUND_OPERATOR_SYSTEM, StackColor, "\nSTART Operators:\n");
// 	if ( m_vStartStack )
// 	{
// 		m_vStartStack->Print( 1 );
// 	}
	Log_Msg( LOG_SOUND_OPERATOR_SYSTEM, StackColor, "\nUPDATE Operators:\n");
	if ( m_vUpdateStack )
	{
		m_vUpdateStack->Print( 1 );
	}

	Log_Msg( LOG_SOUND_OPERATOR_SYSTEM, StackColor, "\nSTOP Operators:\n");
	if ( m_vStopStack )
	{
		m_vStopStack->Print( 1 );
	}
}
CSosOperatorStack *CSosOperatorStackList::GetStack( CSosOperatorStack::SosStackType_t SosType )
{
	switch( SosType )
	{
	case CSosOperatorStack::SOS_UPDATE:
		return m_vUpdateStack;
		break;
// 	case CSosOperatorStack::SOS_START:
// 		return m_vStartStack;
// 		break;
	case CSosOperatorStack::SOS_STOP:
		return m_vStopStack;
		break;
// 	case CSosOperatorStack::SOS_CUE:
// 		return m_vCueStack;
// 		break;
	default:
		return NULL;
	}
	return NULL;
}
void CSosOperatorStackList::SetScriptHash( HSOUNDSCRIPTHASH nHash )
{

// 	if ( m_vCueStack )
// 	{
// 		m_vCueStack->SetScriptHash( nHash );	
// 	}
	// 	if ( m_vStartStack )
	// 	{
	// 		m_vStartStack->SetScriptHash( nHash );	
	// 	}

	if ( m_vUpdateStack )
	{
		m_vUpdateStack->SetScriptHash( nHash );
	}
	if ( m_vStopStack )
	{
		m_vStopStack->SetScriptHash( nHash );
	}
}

void CSosOperatorStackList::SetChannelGuid( int nGuid )
{

// 	if ( m_vCueStack )
// 	{
// 		m_vCueStack->SetChannelGuid( nGuid );
// 	}
// 	if ( m_vStartStack )
// 	{
// 		m_vStartStack->SetChannelGuid( nGuid );
// 	}
	if ( m_vUpdateStack )
	{
		m_vUpdateStack->SetChannelGuid( nGuid );
	}
	if ( m_vStopStack )
	{
		m_vStopStack->SetChannelGuid( nGuid );
	}
}
void CSosOperatorStackList::SetStartTime( float flStartTime )
{

// 	if ( m_vCueStack )
// 	{
// 		m_vCueStack->SetStartTime( flStartTime );
// 	}
// 	if ( m_vStartStack )
// 	{
// 		m_vStartStack->SetStartTime( flStartTime );
// 	}
	if ( m_vUpdateStack )
	{
		m_vUpdateStack->SetStartTime( flStartTime );
	}
	if ( m_vStopStack )
	{
		m_vStopStack->SetStartTime( flStartTime );
	}
}
void CSosOperatorStackList::SetStopTime( float flStopTime )
{
	m_flStopTime = flStopTime;

// 	if ( m_vCueStack )
// 	{
// 		m_vCueStack->SetStopTime( flStopTime );
// 	}
	// 	if ( m_vStopStack )
	// 	{
	// 		m_vStopStack->SetStopTime( flStopTime );
	// 	}
	if ( m_vUpdateStack )
	{
		m_vUpdateStack->SetStopTime( flStopTime );
	}
	if ( m_vStopStack )
	{
		m_vStopStack->SetStopTime( flStopTime );
	}
}
void CSosOperatorStackList::SetStopType( SOSStopType_t stopType )
{
	m_stopType = stopType;

// 	if ( m_vCueStack )
// 	{
// 		m_vCueStack->SetStopType( stopType );
// 	}
	// 	if ( m_vStopStack )
	// 	{
	// 		m_vStopStack->SetStopTime( stopTime );
	// 	}
	if ( m_vUpdateStack )
	{
		m_vUpdateStack->SetStopType( stopType );
	}
	if ( m_vStopStack )
	{
		m_vStopStack->SetStopType( stopType );
	}
}
void CSosOperatorStackList::SetStack( CSosOperatorStack *pStack )
{
	switch ( pStack->GetType() )
	{
	case CSosOperatorStack::SOS_UPDATE:
		m_vUpdateStack = pStack;
		break;
// 	case CSosOperatorStack::SOS_START:
// 		m_vStartStack = pStack;
// 		break;
	case CSosOperatorStack::SOS_STOP:
		m_vStopStack = pStack;
		break;
// 	case CSosOperatorStack::SOS_CUE:
// 		m_vCueStack = pStack;
// 		break;
	default:
		break;

	}
}


void CSosOperatorStackList::StopStacks( SOSStopType_t stopType )
{
	if( stopType == SOS_STOP_FORCE )
	{
		SetStopType( SOS_STOP_FORCE );
	}
	else if( ( m_stopType == SOS_STOP_NONE || m_stopType == SOS_STOP_QUEUE ) && ( stopType != SOS_STOP_NONE &&  stopType != SOS_STOP_QUEUE ) )
	{
		SetStopType( stopType );
		SetStopTime( g_pSoundServices->GetHostTime() );
		
		Execute( CSosOperatorStack::SOS_STOP, NULL, &g_scratchpad );
	}
}

void CSosOperatorStackList::ParseKV( stack_data_t &stackData )
{
	KeyValues *pOperatorsKV = stackData.m_pOperatorsKV;

	KeyValues *pStackListType = pOperatorsKV->GetFirstSubKey();
	while ( pStackListType )
	{
		CSosOperatorStack::SosStackType_t SosType = CSosOperatorStack::SOS_NONE;

		const char *pDefaultStack = NULL;
		const char *pStackListTypeString = pStackListType->GetName();
		if ( pStackListTypeString && *pStackListTypeString )
		{
			if ( !V_strcmp( pStackListTypeString, "update_stack" ) )
			{
				SosType = CSosOperatorStack::SOS_UPDATE;
				pDefaultStack = "update_default";
			}
			else if ( !V_strcmp( pStackListTypeString, "start_stack" ) )
			{
				pStackListType = pStackListType->GetNextKey();
				continue;
// 				SosType = CSosOperatorStack::SOS_START;
// 				pDefaultStack = "start_default";
			}
			else if ( !V_strcmp( pStackListTypeString, "stop_stack" ) )
			{
				SosType = CSosOperatorStack::SOS_STOP;
				pDefaultStack = "stop_default";
			}
			else if ( !V_strcmp( pStackListTypeString, "prestart_stack" ) )
			{
				pStackListType = pStackListType->GetNextKey();
				continue;
// 				SosType = CSosOperatorStack::SOS_CUE;
// 				pDefaultStack = "cue_default";
			}
			else if ( !V_strcmp( pStackListTypeString, "soundentry_operator_data" ) )
			{

				pStackListType = pStackListType->GetNextKey();
				continue;
			}
			else
			{
				Log_Warning( LOG_SND_OPERATORS, "Error: Unknown sound operator stack type: %s\n", pStackListTypeString );
				// gets us out to the stack type level
				pStackListType = pStackListType->GetNextKey();
				continue;
			}

			if ( pDefaultStack && SosType != CSosOperatorStack::SOS_NONE )
			{
				// todo: morasky, default stacks not implemented?
 				CSosOperatorStack *pNewStack = new CSosOperatorStack( SosType, stackData );
				pNewStack->SetName( pStackListTypeString );
 				SetStack( pNewStack );
 				pNewStack->ParseKV( pStackListType );
			}
		}
		pStackListType = pStackListType->GetNextKey();
	}
}

//-----------------------------------------------------------------------------
// S_ParseOperatorsKV
//  
//-----------------------------------------------------------------------------
CSosOperatorStackList *S_ParseOperatorsKV( stack_data_t &stackData )
{
	CSosOperatorStackList *pNewStackList = new CSosOperatorStackList();
	pNewStackList->ParseKV( stackData );
	return pNewStackList;

}

CSosOperatorStack *S_GetStack( CSosOperatorStack::SosStackType_t stackType, stack_data_t &stackData )
{
	KeyValues *pOperatorsKV = stackData.m_pOperatorsKV;

	KeyValues *pStackListType = pOperatorsKV->GetFirstSubKey();

	// morasky: should "GetString" instead of brute force search?
	while ( pStackListType )
	{
		const char *pStackListTypeString = pStackListType->GetName();
		if ( pStackListTypeString && *pStackListTypeString )
		{
			if ( ( V_strcmp( pStackListTypeString, "start_stack" ) && stackType == CSosOperatorStack::SOS_START )  ||
				( V_strcmp( pStackListTypeString, "prestart_stack" ) && stackType == CSosOperatorStack::SOS_CUE ) )
			{
				pStackListType = pStackListType->GetNextKey();
				continue;
			}
			else
			{
				CSosOperatorStack *pNewStack = new CSosOperatorStack( stackType, stackData );
				if( pNewStack )
				{
					pNewStack->SetName( pStackListTypeString );
					pNewStack->ParseKV( pStackListType );

					if( snd_sos_show_operator_init.GetInt() )
					{
						if( stackType == CSosOperatorStack::SOS_START )
						{
							Log_Msg( LOG_SOUND_OPERATOR_SYSTEM, StackColor, "\nSTART Operators:\n");
						}
						else if( stackType == CSosOperatorStack::SOS_CUE )
						{
							Log_Msg( LOG_SOUND_OPERATOR_SYSTEM, StackColor, "\nCUE Operators:\n");
						}
						
						pNewStack->Print( 1 );
					}
				}
				return pNewStack;
			}
		}
		pStackListType = pStackListType->GetNextKey();
	}

	return NULL;
}

CSosOperatorStackList *S_InitChannelOperators( stack_data_t &stackData )
{

	CSosOperatorStackList *pStackList = NULL;

	if ( stackData.m_pOperatorsKV != NULL )
	{
		pStackList = S_ParseOperatorsKV( stackData );
	}

	if ( ! pStackList )
	{
// 		Log_Warning( LOG_SND_OPERATORS, "Error: Unable to create operator stack list on channel\n");
// 		return;
	}
	else
	{
		if ( snd_sos_show_operator_init.GetInt() )
		{
			pStackList->Print();
		}
	}
	return pStackList;
}


//-----------------------------------------------------------------------------
// CSosOperatorStackCollection
//-----------------------------------------------------------------------------
CSosOperatorStackCollection::~CSosOperatorStackCollection()
{
	Clear();
}

void CSosOperatorStackCollection::Clear()
{
	for ( unsigned int i = 0; i < m_vUpdateStacks.Count(); i++ )
	{
		delete m_vUpdateStacks[i];
	}
	m_vUpdateStacks.RemoveAll();
	for ( unsigned int i = 0; i < m_vStartStacks.Count(); i++ )
	{
		delete m_vStartStacks[i];
	}
	m_vStartStacks.RemoveAll();
	for ( unsigned int i = 0; i < m_vStopStacks.Count(); i++ )
	{
		delete m_vStopStacks[i];
	}
	m_vStopStacks.RemoveAll();
	for ( unsigned int i = 0; i < m_vCueStacks.Count(); i++ )
	{
		delete m_vCueStacks[i];
	}
	m_vCueStacks.RemoveAll();
	m_vAllStacks.RemoveAll();
}
CSosOperatorStack *CSosOperatorStackCollection::GetStack( const char *pStackName )
{
	int nIndex = m_vAllStacks.Find( pStackName );
	if ( m_vAllStacks.IsValidIndex( nIndex ) )
	{
		return m_vAllStacks[nIndex];
	}
	return NULL;
}
CSosOperatorStack *CSosOperatorStackCollection::GetStack( CSosOperatorStack::SosStackType_t SosType,  const char *pStackName )
{
	CSosOperatorStack *pOpStack = GetStack( pStackName );
	if ( pOpStack )
	{
		if ( pOpStack->IsType( SosType ) )
		{
			return pOpStack;
		}
	}
	return NULL;
}
void CSosOperatorStackCollection::ParseKV( CSosOperatorStack::SosStackType_t SosType, KeyValues *pStackType )
{
	stack_data_t stackData;
	KeyValues *pStack = pStackType->GetFirstSubKey();

	while ( pStack )
	{
		const char *pStackName = pStack->GetName();
		CSosOperatorStack *pNewStack = new CSosOperatorStack( SosType, stackData );
		pNewStack->SetName( pStackName );

		pNewStack->ParseKV( pStack );

		switch ( SosType )
		{
		case CSosOperatorStack::SOS_UPDATE:
			m_vUpdateStacks.Insert( pStackName, pNewStack );
			m_vAllStacks.Insert( pStackName, pNewStack );
			break;
		case CSosOperatorStack::SOS_START:
			m_vStartStacks.Insert( pStackName, pNewStack );
			m_vAllStacks.Insert( pStackName, pNewStack );
			break;
		case CSosOperatorStack::SOS_STOP:
			m_vStopStacks.Insert( pStackName, pNewStack );
			m_vAllStacks.Insert( pStackName, pNewStack );
			break;
		case CSosOperatorStack::SOS_CUE:
			m_vCueStacks.Insert( pStackName, pNewStack  );
			m_vAllStacks.Insert( pStackName, pNewStack );
			break;
		default:
			// BETTER ERROR HERE
			Log_Warning( LOG_SND_OPERATORS, "Error: Unknown sound operator stack type" );
			delete pNewStack;
			break;
		}
		pStack = pStack->GetNextKey();
	}
}
void CSosOperatorStackCollection::Print()
{
	Log_Msg( LOG_SND_OPERATORS, CollectionColor, "\n\nSound Operators Collection:\n");

	Log_Msg( LOG_SND_OPERATORS, CollectionColor, "\n%*sCue Stacks:\n", 1, "    ");
	for ( unsigned int i = 0; i < m_vCueStacks.Count(); i++ )
	{
		m_vCueStacks[i]->Print( 3 );
	}
	Log_Msg( LOG_SND_OPERATORS, CollectionColor, "\n%*sStart Stacks:\n", 1, "    " );
	for ( unsigned int i = 0; i < m_vStartStacks.Count(); i++ )
	{
		m_vStartStacks[i]->Print( 3 );
	}
	Log_Msg ( LOG_SND_OPERATORS, CollectionColor, "\n%*sUpdate Stacks:\n", 1, "    " );
	for ( unsigned int i = 0; i < m_vUpdateStacks.Count(); i++ )
	{
		m_vUpdateStacks[i]->Print( 3 );
	}
	Log_Msg ( LOG_SND_OPERATORS, CollectionColor, "\n%*sStop Stacks:\n", 1, "    " );
	for ( unsigned int i = 0; i < m_vStopStacks.Count(); i++ )
	{
		m_vStopStacks[i]->Print( 3 );
	}
	Log_Msg( LOG_SND_OPERATORS, CollectionColor, "\n");
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
CSosOperatorSystem::CSosOperatorSystem()
{
	m_bHasInitialized = false;
}

CSosOperatorSystem::~CSosOperatorSystem()
{
	for( int i = 0; i < m_sosStopChannelQueue.Count(); i++ )
	{
		SosStopQueueData_t *pStopQueue = m_sosStopChannelQueue[i];
		if( pStopQueue )
		{
			delete pStopQueue;
		}
	}
	m_sosStopChannelQueue.RemoveAll();

	for( int i = 0; i < m_sosStartEntryQueue.Count(); i++ )
	{
		SosStartQueueData_t *pStartQueue = m_sosStartEntryQueue[i];
		if( pStartQueue )
		{
			delete pStartQueue;
		}
	}
	m_sosStartEntryQueue.RemoveAll();
}

#define SOUND_OPERATORS_FILE "scripts/sound_operator_stacks.txt"
void CSosOperatorSystem::Flush()
{
	m_MasterStackCollection.Clear();
	m_bHasInitialized = false;
	Init();
}

void CSosOperatorSystem::Init()
{
	// checking for "restarting sound system"

	if( m_bHasInitialized )
	{
		return;
	}
	m_bHasInitialized = true;

	CRC32_t crc;
	CRC32_Init( &crc );

	KeyValues *pSoundOperatorStacksKV = new KeyValues( SOUND_OPERATORS_FILE );
	if ( g_pFullFileSystem->LoadKeyValues( *pSoundOperatorStacksKV, IFileSystem::TYPE_SOUNDOPERATORS, SOUND_OPERATORS_FILE, "GAME" ) )
	{
		// TODO: Morasky
	//	AccumulateFileNameAndTimestampIntoChecksum( &crc, SOUND_OPERATORS_FILE );

		KeyValues *pStackType = pSoundOperatorStacksKV;
		while ( pStackType )
		{
			CSosOperatorStack::SosStackType_t curStackType = CSosOperatorStack::SOS_NONE;
			const char *pStackTypeString = pStackType->GetName(  );
			if ( pStackTypeString && *pStackTypeString )
			{
				if ( !V_strcmp( pStackTypeString, "update_stacks" ) )
				{
					curStackType = CSosOperatorStack::SOS_UPDATE;
				}
				else if ( !V_strcmp( pStackTypeString, "start_stacks" ) )
				{
					curStackType = CSosOperatorStack::SOS_START;
				}
				else if ( !V_strcmp( pStackTypeString, "stop_stacks" ) )
				{
					curStackType = CSosOperatorStack::SOS_STOP;
				}
				else if ( !V_strcmp( pStackTypeString, "prestart_stacks" ) )
				{
					curStackType = CSosOperatorStack::SOS_CUE;
				}
				else
				{
					// BETTER ERROR HERE
					Log_Warning( LOG_SND_OPERATORS, "Unknown sound operator stack arg %s", pStackTypeString  );
					continue;
				}
			}
			m_MasterStackCollection.ParseKV( curStackType, pStackType );
			pStackType = pStackType->GetNextKey();
		}
	}
	else
	{
		Log_Warning( LOG_SND_OPERATORS, "Warning: Unable to load sound operators file '%s'\n", SOUND_OPERATORS_FILE );
	}
	pSoundOperatorStacksKV->deleteThis();

	CRC32_Final( &crc );

	if( snd_sos_show_operator_init.GetInt() )
	{
		m_MasterStackCollection.Print();
	}

	//	m_uManifestPlusScriptChecksum = ( unsigned int )crc;
}
void CSosOperatorSystem::PrintOperatorList()
{
	for( unsigned int i = 0; i < m_vOperatorCollection.Count(); i++)
	{
		DevMsg("%s\n", m_vOperatorCollection.GetElementName( i ) );
	}
}
void S_PrintOperatorList( void )
{
	g_pSoundOperatorSystem->PrintOperatorList();
}

ConCommand snd_sos_print_operators( "snd_sos_print_operators", S_PrintOperatorList, "Prints a list of currently available operators", FCVAR_CHEAT );

void CSosOperatorSystem::ClearSubSystems()
{
	m_vTrackDict.RemoveAll();
	m_sosStopChannelQueue.PurgeAndDeleteElements();
	m_sosStartEntryQueue.PurgeAndDeleteElements();
}

void CSosOperatorSystem::QueueStartEntry( StartSoundParams_t &startParams, float flDelay /*= 0.0*/, bool bFromPrestart /* = false */ )
{
	SosStartQueueData_t *pStartQueue = new SosStartQueueData_t();
	pStartQueue->m_nStartTime = g_pSoundServices->GetHostTime() + flDelay;
	pStartQueue->m_bFromPrestart = bFromPrestart;

	startParams.Copy( pStartQueue->m_startSoundParams );

	m_sosStartEntryQueue.AddToTail( pStartQueue );
}

ConVar snd_sos_show_startqueue("snd_sos_show_startqueue", "0", FCVAR_CHEAT );
void CSosOperatorSystem::StartQueuedEntries()
{
	for( int i = m_sosStartEntryQueue.Count() -1; i > -1; i-- )
	{
		SosStartQueueData_t *pStartQueue = m_sosStartEntryQueue[i];
		if( snd_sos_show_startqueue.GetInt() )
		{
			Log_Msg( LOG_SND_OPERATORS, "STARTQUEUE: %s time to start of %f\n", g_pSoundEmitterSystem->GetSoundNameForHash( pStartQueue->m_startSoundParams.m_nSoundScriptHash ), pStartQueue->m_nStartTime - g_pSoundServices->GetHostTime() );
		}

		if( pStartQueue->m_nStartTime <= g_pSoundServices->GetHostTime() )
		{
			if( snd_sos_show_startqueue.GetInt() )
			{
				Log_Msg( LOG_SND_OPERATORS, "STARTQUEUE: %s satisfies time\n", g_pSoundEmitterSystem->GetSoundNameForHash( pStartQueue->m_startSoundParams.m_nSoundScriptHash ) );
			}
			// check if new entry has priority over old
			track_data_t newTrackData;
			track_data_t existingTrackData;

			bool bHasData = S_GetTrackData( pStartQueue->m_startSoundParams.m_nSoundScriptHash, newTrackData );
			bool bHasData2 = false;
			bool bHasPriority = false;

			if( bHasData )
			{
				bHasData2 = GetTrackDataOnTrack( newTrackData.m_pTrackName, existingTrackData );
				if( bHasData2 )
				{
					bHasPriority = S_TrackHasPriority( newTrackData, existingTrackData );
				}
			}

			if( !bHasPriority && bHasData && bHasData2 )
			{
				if( snd_sos_show_startqueue.GetInt() )
				{
					Log_Msg( LOG_SND_OPERATORS, "STARTQUEUE: Not starting %s due to existing priority.\n", g_pSoundEmitterSystem->GetSoundNameForHash( pStartQueue->m_startSoundParams.m_nSoundScriptHash ) );
				}
			}
			else
			{
				if( snd_sos_show_startqueue.GetInt() )
				{
					Log_Msg( LOG_SND_OPERATORS, "STARTQUEUE: %s StartSoundEntry\n", g_pSoundEmitterSystem->GetSoundNameForHash( pStartQueue->m_startSoundParams.m_nSoundScriptHash ) );
				}
				S_StartSoundEntry( pStartQueue->m_startSoundParams , -1, pStartQueue->m_bFromPrestart );

// 				if( bHasData )
// 				{
// 					KeyValues *pStopTracksList = S_GetStopTracksKV( pStartQueue->m_startSoundParams.m_nSoundScriptHash );
// 
// 					if( pStopTracksList )
// 					{
// 						for ( KeyValues *pValue = pStopTracksList->GetFirstValue(); pValue; pValue = pValue->GetNextValue() )
// 						{
// 							DevMsg("***STOP TRACK: %s", pValue->GetString() );
// 						}
// 					}
// 				}
			}

			m_sosStartEntryQueue.Remove( i );
			delete pStartQueue;
		}
	}
}

void CSosOperatorSystem::QueueStopChannel( int nChannelGuid, float flDelay /* = 0.0*/ )
{
	SosStopQueueData_t *pStopQueue = new SosStopQueueData_t;
	pStopQueue->m_nChannelGuid = nChannelGuid;
	pStopQueue->m_nStopTime = g_pSoundServices->GetHostTime() + flDelay;

	m_sosStopChannelQueue.AddToTail( pStopQueue );
}
bool CSosOperatorSystem::IsInStopQueue( int nChannelGuid )
{
	for( int i = 0; i < m_sosStopChannelQueue.Count(); i++ )
	{
		SosStopQueueData_t *pStopQueue = m_sosStopChannelQueue[i];
		if( nChannelGuid == pStopQueue->m_nChannelGuid )
		{
			return true;
		}
	}
	return false;
}

void CSosOperatorSystem::StopQueuedChannels()
{
	for( int i = m_sosStopChannelQueue.Count() -1; i > -1; i-- )
	{
		SosStopQueueData_t *pStopQueue = m_sosStopChannelQueue[i];
		if( pStopQueue->m_nStopTime <= g_pSoundServices->GetHostTime() )
		{
			S_StopSoundByGuid( pStopQueue->m_nChannelGuid );
			m_sosStopChannelQueue.Remove( i );
			delete pStopQueue;
		}
	}
}

void CSosOperatorSystem::SetChannelOnTrack( const char *pTrackName, channel_t *pChannel )
{
	int nTrackIndex = m_vTrackDict.Find( pTrackName );
	if( m_vTrackDict.IsValidIndex( nTrackIndex ) )
	{
		g_pSoundOperatorSystem->m_vTrackDict[nTrackIndex] = pChannel;
	}
	else
	{
		int nNewIndex = g_pSoundOperatorSystem->m_vTrackDict.Insert( pTrackName );
		g_pSoundOperatorSystem->m_vTrackDict[ nNewIndex ] = pChannel;
	}

}

channel_t *CSosOperatorSystem::GetChannelOnTrack( const char *pTrackName )
{
	int nTrackIndex = m_vTrackDict.Find( pTrackName );
	if( m_vTrackDict.IsValidIndex( nTrackIndex ) )
	{
		return g_pSoundOperatorSystem->m_vTrackDict[nTrackIndex];
	}
	else
	{
		return NULL;
	}
}
HSOUNDSCRIPTHASH CSosOperatorSystem::GetSoundEntryOnTrack( const char *pTrackName )
{
	channel_t *pChannel = GetChannelOnTrack( pTrackName );
	if( pChannel && pChannel->sfx )
	{
		return pChannel->m_nSoundScriptHash;
	}
	else
	{
		return SOUNDEMITTER_INVALID_HASH;
	}
}
bool CSosOperatorSystem::GetTrackDataOnTrack( const char *pTrackName, track_data_t &trackData )
{
	HSOUNDSCRIPTHASH nSoundScriptHash = GetSoundEntryOnTrack( pTrackName );
	if( nSoundScriptHash != SOUNDEMITTER_INVALID_HASH )
	{
		S_GetTrackData( nSoundScriptHash, trackData );
		return true;
	}
	return false;
}

void CSosOperatorSystem::StopChannelOnTrack( const char *pTrackName, bool bStopAll /* false */, float flStopDelay /* = 0.0 */ )
{
	int nTrackIndex = m_vTrackDict.Find( pTrackName );
	if( m_vTrackDict.IsValidIndex( nTrackIndex ) )
	{
		channel_t *pPreviousChannelOnTrack = m_vTrackDict[ nTrackIndex ];

		if( pPreviousChannelOnTrack && pPreviousChannelOnTrack->sfx )
		{
			//DevMsg("***STOPPING: Track: %s, Entry: %s Delay: %f\n", pTrackName, g_pSoundEmitterSystem->GetSoundNameForHash( pPreviousChannelOnTrack->m_nSoundScriptHash ), flStopDelay );
			if( pPreviousChannelOnTrack->m_pStackList )
			{
				CSosOperatorStack *pStack = pPreviousChannelOnTrack->m_pStackList->GetStack( CSosOperatorStack::SOS_UPDATE);
				if( pStack )
				{
					pStack->SetStopType( SOS_STOP_QUEUE );
				}
			}
			g_pSoundOperatorSystem->QueueStopChannel( pPreviousChannelOnTrack->guid, flStopDelay );
		}
	}

	// remove a queued entry that is on the same track
	if( bStopAll )
	{
		for( int i = m_sosStartEntryQueue.Count() -1; i > -1; i-- )
		{
			SosStartQueueData_t *pStartQueue = m_sosStartEntryQueue[i];
			track_data_t trackData;
			S_GetTrackData( pStartQueue->m_startSoundParams.m_pOperatorsKV, trackData );
			if( trackData.m_nTrackNumber == nTrackIndex )
			{
				m_sosStartEntryQueue.Remove( i );
				delete pStartQueue;
			}
		}
	}
}

void CSosOperatorSystem::RemoveChannelFromTracks( int nGuid )
{
	// no negative guids
	if( nGuid < 0 )
	{
		return;
	}

	FOR_EACH_DICT( m_vTrackDict, i )
	{
		channel_t *pChannel = m_vTrackDict[ i ];
		if( pChannel )
		{
			if( pChannel->guid == nGuid )
			{
				m_vTrackDict[i] = NULL;
			}
		}
	}
}
void CSosOperatorSystem::RemoveChannelFromTrack( const char *pTrackName, int nGuid )
{
	// no negative guids
	if( nGuid < 0 )
	{
		return;
	}

	int nTrackIndex = m_vTrackDict.Find( pTrackName );
	if( m_vTrackDict.IsValidIndex( nTrackIndex ) )
	{
		channel_t *pChannelOnTrack = m_vTrackDict[ nTrackIndex ];
		if( pChannelOnTrack && pChannelOnTrack->guid == nGuid )
		{
			m_vTrackDict[ nTrackIndex ] = NULL;
		}
	}
}

ConVar snd_sos_show_track_list("snd_sos_show_track_list", "0", FCVAR_NONE );
void CSosOperatorSystem::DEBUG_ShowTrackList( void )
{
	if( !snd_sos_show_track_list.GetInt() )
	{
		return;
	}

	FOR_EACH_DICT( m_vTrackDict, i )
	{
		channel_t *pChannel = m_vTrackDict[ i ];
		char chanStr[128];
		sprintf( chanStr, "Channel %i : %s = %s", i, m_vTrackDict.GetElementName( i ),  pChannel ? g_pSoundEmitterSystem->GetSoundNameForHash( pChannel->m_nSoundScriptHash ) : "empty" );
		CDebugOverlay::AddScreenTextOverlay( 0.01, 0.2 + (0.012 * (float)  i) , 0.5, 200, 200, 200, 255, chanStr );
	}
}

//Returns a valid id into the string table
bool CSosOperatorSystem::SetOpVarFloat( const char *pString, float flVariable )
{
	int nIndex = m_sosOpVarFloatMap.Find( pString );
	if ( m_sosOpVarFloatMap.IsValidIndex( nIndex ) )
	{
		m_sosOpVarFloatMap[nIndex] = flVariable;
		return true;
	}
	nIndex = m_sosOpVarFloatMap.Insert( pString );

	if ( m_sosOpVarFloatMap.IsValidIndex( nIndex ) )
	{
		m_sosOpVarFloatMap[ nIndex ] = flVariable;
		return true;
	}

	DevWarning( "SOS System Warning: Unable to set or create float variable %s\n", pString );
	return false;

// 	int stringId = m_sosFloatOpvarMap.Find( pString );
// 	if ( stringId != m_sosFloatOpvarMap.InvalidIndex() )
// 	{
// 		// found in pool
// 		m_sosFloatOpvarMap[stringId] = flVariable;
// 		return true;
// 	}
// 	stringId = m_sosFloatOpvarMap.AddString( pString );
// 	if ( stringId != m_sosFloatOpvarMap.InvalidIndex() )
// 	{
// 		// found in pool
// 		m_sosFloatOpvarMap[stringId] = flVariable;
// 		return true;
// 	}
// 
// 	DevWarning( "SOS System Warning: Unable to set or create float variable %s\n", pString );
// 	return false;

}

// Returns a valid id into the string table
bool CSosOperatorSystem::GetOpVarFloat( const char *pString, float &flVariable )
{

	int nIndex = m_sosOpVarFloatMap.Find( pString );
	if ( !m_sosOpVarFloatMap.IsValidIndex( nIndex ) )
	{
		// error
		return false;
	}
	float flTest = m_sosOpVarFloatMap.Element( nIndex );
	flVariable = flTest;
	return true;


// 	int stringId = m_sosFloatOpvarMap.Find( pString );
// 	if ( stringId != m_sosFloatOpvarMap.InvalidIndex() )
// 	{
// 		// found in pool
// 		flVariable =  m_sosFloatOpvarMap[stringId];
// 		return true;
// 	}
// 	return false;

}

ConVar snd_sos_show_opvar_list("snd_sos_show_opvar_list", "0", FCVAR_NONE );
void CSosOperatorSystem::DEBUG_ShowOpvarList( void )
{
	if( !snd_sos_show_opvar_list.GetInt() )
	{
		return;
	}


	int nCount = m_sosOpVarFloatMap.Count();

	char opVarStr[128];
	sprintf( opVarStr, "Opvars %i", nCount );

	CDebugOverlay::AddScreenTextOverlay( 0.01, 0.5, 0.5, 200, 200, 200, 255, opVarStr );

	
	for ( int i = 0 ; i < nCount; i++ )
	{
		if( m_sosOpVarFloatMap.IsValidIndex( i ) )
		{
			float flValue = m_sosOpVarFloatMap[ i ];
			char chanStr[128];
			sprintf( chanStr, "- %s = %f", m_sosOpVarFloatMap.GetElementName( i ), flValue );
			CDebugOverlay::AddScreenTextOverlay( 0.01, 0.512 + (0.012 * (float)  i) , 0.5, 200, 200, 200, 255, chanStr );
		}
	}

}


void CSosOperatorSystem::Update()
{
	m_sosEntryBlockList.Update();
}


void CSosOperatorSystem::Shutdown()
{

}

CSosOperatorSystem *CSosOperatorSystem::GetSoundOperatorSystem()
{
	static CSosOperatorSystem s_SoundOperatorSystem;
	return &s_SoundOperatorSystem;
}

void S_SOSFlush()
{
	g_pSoundOperatorSystem->Flush();
}


ConCommand snd_sos_flush_operators( "snd_sos_flush_operators", S_SOSFlush, "Flush and re-parse the sound operator system", FCVAR_CHEAT );

bool S_SOSSetOpvarFloat( const char *pOpVarName, float flValue )
{
	if( g_pSoundOperatorSystem )
	{
		return g_pSoundOperatorSystem->SetOpVarFloat( pOpVarName,flValue );
	}
	else
	{
		DevMsg("SOS Error: g_pSoundOperatorSystem used via SOSSetOpvarFloat before it's initialized.");
		return false;
	}
}
bool S_SOSGetOpvarFloat( const char *pOpVarName, float &flValue )
{
	if( g_pSoundOperatorSystem )
	{
		return g_pSoundOperatorSystem->GetOpVarFloat( pOpVarName, flValue );
	}
	else
	{
		DevMsg("SOS Error: g_pSoundOperatorSystem used via SOSGetOpvarFloat before it's initialized.");
		return false;
	}
}
