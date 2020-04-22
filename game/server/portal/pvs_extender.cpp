//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

#include "cbase.h"
#include "pvs_extender.h"
#include "util_shared.h"
#include "tier1/utlvector.h"


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static CUtlVector<CPVS_Extender *> s_AllExtenders;
CPVS_Extender::CPVS_Extender( void )
{
	s_AllExtenders.AddToTail( this );
}

CPVS_Extender::~CPVS_Extender( void )
{
	s_AllExtenders.FindAndFastRemove( this );
}


void CPVS_Extender::ComputeExtendedPVS( const CBaseEntity *pViewEntity, const Vector &vVisOrigin, unsigned char *outputPVS, int pvssize, int iMaxRecursions )
{
	int iAllExtenderCount = s_AllExtenders.Count();
	if( iAllExtenderCount == 0 )
		return;

	static CThreadFastMutex s_PVSExtenderMutex;
	AUTO_LOCK_FM( s_PVSExtenderMutex );

	CPVS_Extender **pAllExtenders = s_AllExtenders.Base();

	int iExtenderCount = 0;
	CPVS_Extender **pExtenders = (CPVS_Extender **)stackalloc( sizeof( CPVS_Extender * ) * iAllExtenderCount );

	//filter out portals that can't possibly extend visibility up-front. So we don't have to do so in our recursions
	for( int i = 0; i != iAllExtenderCount; ++i )
	{
		CPVS_Extender *pExtender = pAllExtenders[i];
		Assert( pExtender );

		if ( pExtender->IsExtenderValid() )
		{
			if ( pExtender->GetExtenderNetworkProp()->AreaNum() < 0 )
			{
				Assert( false );
				continue;
			}

			pExtenders[iExtenderCount] = pExtender;
			++iExtenderCount;
		}
	}

	if( iExtenderCount == 0 )
		return;

	int iAreasNetworked[MAX_MAP_AREAS];
	iAreasNetworked[0] = pViewEntity->NetworkProp()->AreaNum(); //safe assumption?
	for( int i = 1; i != MAX_MAP_AREAS; ++i )
	{
		iAreasNetworked[i] = -1;
	}

	//unsigned char *viewEntPVS = (unsigned char *)stackalloc( sizeof( unsigned char ) * pvssize );
	//memcpy( viewEntPVS, outputPVS, sizeof( unsigned char ) * pvssize );

	unsigned char viewEntPVS[MAX_MAP_LEAFS/8];
	engine->GetPVSForCluster( engine->GetClusterForOrigin( vVisOrigin ), sizeof( viewEntPVS ), viewEntPVS );
	//do we OR that into the output PVS?

	//grab the local pvs of every extender up front to avoid repeating it in recursions
	//unsigned char *pExtenderPVSs = (unsigned char *)stackalloc( sizeof( unsigned char ) * (MAX_MAP_LEAFS/8) * iPortalCount );
	ExtenderInstanceData_t *pExtenderData = (ExtenderInstanceData_t *)stackalloc( sizeof( ExtenderInstanceData_t ) * iExtenderCount );
	for( int i = 0; i != iExtenderCount; ++i )
	{
		pExtenders[i]->m_pExtenderData = &pExtenderData[i];
		engine->GetPVSForCluster( engine->GetClusterForOrigin( pExtenders[i]->GetExtensionPVSOrigin() ), sizeof( unsigned char ) * (MAX_MAP_LEAFS/8), pExtenderData[i].iPVSBits );
		pExtenders[i]->m_pExtenderData->bAddedToPVSAlready = false;
	}

	VisExtensionChain_t chainRoot;
	chainRoot.m_nArea = iAreasNetworked[0];
	chainRoot.pParentChain = NULL;

	const edict_t *viewEdict = pViewEntity->edict();

	for( int i = 0; i != iExtenderCount; ++i )
	{
		CPVS_Extender *pExtender = pExtenders[i];

		if ( pExtender->GetExtenderEdict() == viewEdict )
			continue;

		if ( pExtender->GetExtenderNetworkProp()->IsInPVS( viewEdict, viewEntPVS, pvssize ) ) //test against pViewEntity PVS, not aggregate PVS
		{
			chainRoot.pExtender = pExtender;
			pExtender->ComputeSubVisibility( pExtenders, iExtenderCount, outputPVS, pvssize, vVisOrigin, NULL, 0, &chainRoot, iAreasNetworked, iMaxRecursions );			
		}
	}

	for( int i = 0; i != iExtenderCount; ++i )
	{
		pExtenders[i]->m_pExtenderData = NULL;
	}
}


//does the grunt work of checking if the data has already been added, and if not, adding it
/*void CPVS_Extender::AddToPVS( unsigned char *outputPVS, int pvssize, int iAreasNetworked[MAX_MAP_AREAS] )
{
	

	if( !m_pExtenderData->bAddedToPVSAlready )
	{
		bool bFound = false;
		for( int i = 0; i != MAX_MAP_AREAS; ++i )
		{
			if( iAreasNetworked[i] == iLinkedArea )
			{
				bFound = true;
				break;
			}

			if( iAreasNetworked[i] == -1 )
			{
				bFound = true; //we found it by adding it
				iAreasNetworked[i] = iLinkedArea;
				int iOutputPVSIntSize = pvssize / sizeof( unsigned int );
				for( int j = 0; j != iOutputPVSIntSize; ++j )
				{
					((unsigned int *)outputPVS)[j] |= ((unsigned int *)pLinkedPVS)[j];
				}
				for( int j = iOutputPVSIntSize * sizeof( unsigned int ); j != pvssize; ++j )
				{
					outputPVS[j] |= pLinkedPVS[j];
				}
				break;
			}
		}

		if( !bFound )
			return;

		AddPortalCornersToEnginePVS( pLinkedPortal );
		m_pExtenderData->bAddedToPVSAlready = true;
	}
}*/



