//===== Copyright (c) 1996-2009, Valve Corporation, All rights reserved. ====
//
// Animation commands
//
//==========================================================================


// Valve includes
#include "datamodel/dmelementfactoryhelper.h"
#include "movieobjects/dmemodel.h"
#include "movieobjects/dmedrawsettings.h"
#include "mdlobjects/dmeanimcmd.h"
#include "mdlobjects/dmebodygroup.h"
#include "mdlobjects/dmebodygrouplist.h"
#include "mdlobjects/dmelodlist.h"
#include "mdlobjects/dmematsysroot.h"
#include "mdlobjects/dmesequence.h"
#include "mdlobjects/dmesequencelist.h"
#include "tier1/utldict.h"
#include "bone_setup.h"


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//=============================================================================
// CDmeMatSysSettings
//=============================================================================
IMPLEMENT_ELEMENT_FACTORY( DmeMatSysPanelSettings, CDmeMatSysPanelSettings );


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeMatSysPanelSettings::OnConstruction()
{
	m_cBackgroundColor.InitAndSet( this, "backgroundColor", Color( 0.3f, 0.3f, 0.3f ) );
	m_cAmbientColor.InitAndSet( this, "ambientColor", Color( 0.3f, 0.3f, 0.3f ) );
	m_bDrawGroundPlane.InitAndSet( this, "drawGroundPlane", true );
	m_bDrawOriginAxis.InitAndSet( this, "drawOriginAxis", true );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeMatSysPanelSettings::OnDestruction()
{
}


//=============================================================================
// CDmeMatSysSettings
//=============================================================================
IMPLEMENT_ELEMENT_FACTORY( DmeMatSysRoot, CDmeMatSysRoot );


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeMatSysRoot::OnConstruction()
{
	m_Settings.InitAndCreate( this, "settings" );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeMatSysRoot::OnDestruction()
{
}


//=============================================================================
// IDmeMatSysModel
//=============================================================================
//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
int IDmeMatSysModel::SelectSequence( const char *pszSequenceName )
{
	Assert( 0 );
	return -1;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void IDmeMatSysModel::SetTime( DmeTime_t dmeTime )
{
	Assert( 0 );
	return;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void IDmeMatSysModel::SetFrame( float flFrame )
{
	Assert( 0 );
	return;
}


//=============================================================================
// CDmeMatSysMDLDag
//=============================================================================
IMPLEMENT_ELEMENT_FACTORY( DmeMatSysMDLDag, CDmeMatSysMDLDag );


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeMatSysMDLDag::OnConstruction()
{
	CDmeMDL *pDmeMDL = CreateElement< CDmeMDL >( CUtlString( GetName() ) + "Shape", GetFileId() );
	if ( pDmeMDL )
	{
		pDmeMDL->DrawInEngine( true );
		SetShape( pDmeMDL );
	}
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeMatSysMDLDag::OnDestruction()
{
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
studiohdr_t *CDmeMatSysMDLDag::GetStudioHdr() const
{
	CDmeMDL *pDmeMDL = GetDmeMDL();
	if ( !pDmeMDL )
		return NULL;

	const MDLHandle_t hMDL = pDmeMDL->GetMDL();
	if ( hMDL == MDLHANDLE_INVALID || g_pMDLCache->IsErrorModel( hMDL ) )
		return NULL;

	return g_pMDLCache->GetStudioHdr( hMDL );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeMatSysMDLDag::GetSequenceList( CUtlVector< CUtlString > *pOutList )
{
	if ( !pOutList )
		return;

	pOutList->RemoveAll();

	studiohdr_t *pStudioHdr = GetStudioHdr();
	if ( !pStudioHdr )
		return;
	
	for ( int j = 0; j < pStudioHdr->GetNumSeq(); ++j )
	{
		if ( !( pStudioHdr->pSeqdesc( j ).flags & STUDIO_HIDDEN ) )
		{
			const char *pszSequenceName = pStudioHdr->pSeqdesc(j).pszLabel();
			if ( pszSequenceName && pszSequenceName[0] )
			{
				pOutList->AddToTail( pszSequenceName );
			}
		}
	}
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeMatSysMDLDag::GetActivityList( CUtlVector< CUtlString > *pOutList )
{
	pOutList->RemoveAll();

	studiohdr_t *pStudioHdr = GetStudioHdr();
	if ( !pStudioHdr )
		return;
	
	CUtlDict<int, unsigned short> activityNames( true, 0, pStudioHdr->GetNumSeq() );

	for ( int j = 0; j < pStudioHdr->GetNumSeq(); ++j )
	{
		if ( !( pStudioHdr->pSeqdesc( j ).flags & STUDIO_HIDDEN ) )
		{
			const char *pszActivityName = pStudioHdr->pSeqdesc( j ).pszActivityName();
			if ( pszActivityName && pszActivityName[0] )
			{
				// Multiple sequences can have the same activity name; only add unique activity names
				if ( activityNames.Find( pszActivityName ) == activityNames.InvalidIndex() )
				{
					pOutList->AddToTail( pszActivityName );
					activityNames.Insert( pszActivityName, j );
				}
			}
		}
	}
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
int CDmeMatSysMDLDag::SelectSequence( const char *pszSequenceName )
{
	if ( !pszSequenceName || !pszSequenceName[0] )
		return -1;

	CDmeMDL *pDmeMdl = GetDmeMDL();

	studiohdr_t *pStudioHdr = GetStudioHdr();
	if ( !pStudioHdr )
		return -1;

	pDmeMdl->m_nSequence = -1;

	for ( int i = 0; i < pStudioHdr->GetNumSeq(); ++i )
	{
		mstudioseqdesc_t &seqdesc = pStudioHdr->pSeqdesc( i );
		if ( !Q_stricmp( seqdesc.pszLabel(), pszSequenceName ) )
		{
			pDmeMdl->m_nSequence = i;
			break;
		}
	}

	if ( pDmeMdl->m_nSequence < 0 )
	{
		pDmeMdl->m_nSequence = 0;
		return -1;
	}

	CStudioHdr cStudioHdr( pStudioHdr, g_pMDLCache );

	float flPoseParameter[MAXSTUDIOPOSEPARAM];
	Studio_CalcDefaultPoseParameters( &cStudioHdr, flPoseParameter, MAXSTUDIOPOSEPARAM );

	int nFrameCount = Studio_MaxFrame( &cStudioHdr, pDmeMdl->m_nSequence, flPoseParameter );
	if ( nFrameCount == 0 )
	{
		nFrameCount = 1;
	}

	return nFrameCount;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeMatSysMDLDag::SetTime( DmeTime_t dmeTime )
{
	CDmeMDL *pDmeMDL = GetDmeMDL();
	if ( !pDmeMDL )
		return;

	pDmeMDL->m_flTime = dmeTime.GetSeconds();
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeMatSysMDLDag::SetFrame( float flFrame )
{
	CDmeMDL *pDmeMDL = GetDmeMDL();
	if ( !pDmeMDL )
		return;

	pDmeMDL->m_flTime = flFrame / pDmeMDL->m_flPlaybackRate;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeMatSysMDLDag::SetMDL( MDLHandle_t hMDL )
{
	CDmeMDL *pDmeMDL = GetDmeMDL();
	if ( !pDmeMDL )
		return;

	pDmeMDL->SetMDL( hMDL );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
MDLHandle_t CDmeMatSysMDLDag::GetMDL() const
{
	CDmeMDL *pDmeMDL = GetDmeMDL();
	if ( !pDmeMDL )
		return MDLHANDLE_INVALID;

	return pDmeMDL->GetMDL();
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
CDmeMDL *CDmeMatSysMDLDag::GetDmeMDL() const
{
	return CastElement< CDmeMDL >( m_Shape );
}


//=============================================================================
// CDmeMatSysDMXDag
//=============================================================================
IMPLEMENT_ELEMENT_FACTORY( DmeMatSysDMXDag, CDmeMatSysDMXDag );


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeMatSysDMXDag::OnConstruction()
{
	m_eDmxRoot.InitAndSet( this, "dmxRoot", DMELEMENT_HANDLE_INVALID );
	m_hDmxModel = DMELEMENT_HANDLE_INVALID;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeMatSysDMXDag::OnDestruction()
{
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeMatSysDMXDag::Draw( CDmeDrawSettings *pDrawSettings /* = NULL */ )
{
	if ( !m_Visible )
		return;

	// This is meant to be called from mixed rendering environment and
	// DmeMesh windings are backwards relative to MDL windings

	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	pRenderContext->CullMode( MATERIAL_CULLMODE_CW );

	BaseClass::Draw( pDrawSettings );

	pRenderContext->CullMode( MATERIAL_CULLMODE_CCW );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeMatSysDMXDag::GetSequenceList( CUtlVector< CUtlString > *pOutList )
{
	// TODO: Look for sequences in the DMX file
	Assert( 0 );
	pOutList->RemoveAll();
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeMatSysDMXDag::GetActivityList( CUtlVector< CUtlString > *pOutList )
{
	// DMX Model files don't have any activities
	pOutList->RemoveAll();
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeMatSysDMXDag::SetDmxRoot( CDmElement *pDmxRoot )
{
	m_eDmxRoot.Set( pDmxRoot );

	// Remove old DmeModel child
	if ( m_hDmxModel != DMELEMENT_HANDLE_INVALID )
	{
		for ( int i = 0; i < GetChildCount(); ++i )
		{
			CDmeDag *pDmeDag = GetChild( i );
			if ( !pDmeDag )
				continue;

			if ( pDmeDag->GetHandle() == m_hDmxModel )
			{
				RemoveChild( i );
				break;
			}
		}
	}

	m_hDmxModel = DMELEMENT_HANDLE_INVALID;

	matrix3x4_t mIdentity;
	SetIdentityMatrix( mIdentity );
	GetTransform()->SetTransform( mIdentity );

	if ( pDmxRoot )
	{
		CDmeModel *pDmeModel = pDmxRoot->GetValueElement< CDmeModel >( "model" );
		if ( pDmeModel )
		{
			if ( !pDmeModel->IsZUp() )
			{
				GetTransform()->SetTransform(
					matrix3x4_t(
					0.0f,  0.0f,  1.0f, 0.0f,
					1.0f,  0.0f,  0.0f, 0.0f,
					0.0f,  1.0f,  0.0f, 0.0f ) );
			}

			m_hDmxModel = pDmeModel->GetHandle();

			AddChild( pDmeModel );
		}
	}
}


//=============================================================================
// CDmeMatSysMPPDag
//=============================================================================
IMPLEMENT_ELEMENT_FACTORY( DmeMatSysMPPDag, CDmeMatSysMPPDag );


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeMatSysMPPDag::OnConstruction()
{
	m_eMppRoot.InitAndSet( this, "mppRoot", DMELEMENT_HANDLE_INVALID );
	m_hDmeBodyGroupList = DMELEMENT_HANDLE_INVALID;
	m_hDmeSequenceList = DMELEMENT_HANDLE_INVALID;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeMatSysMPPDag::OnDestruction()
{
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeMatSysMPPDag::Draw( CDmeDrawSettings *pDrawSettings /* = NULL */ )
{
	if ( !m_Visible )
		return;

	// This is meant to be called from mixed rendering environment and
	// DmeMesh windings are backwards relative to MDL windings

	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	pRenderContext->CullMode( MATERIAL_CULLMODE_CW );

	BaseClass::Draw( pDrawSettings );

	pRenderContext->CullMode( MATERIAL_CULLMODE_CCW );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeMatSysMPPDag::GetSequenceList( CUtlVector< CUtlString > *pOutList )
{
	pOutList->RemoveAll();

	if ( !m_eMppRoot )
		return;

	CDmeSequenceList *pDmeSequenceList = m_eMppRoot->GetValueElement< CDmeSequenceList >( "sequenceList" );
	if ( !pDmeSequenceList )
		return;

	CUtlVector< CDmeSequenceBase * > sortedSequenceList;
	pDmeSequenceList->GetSortedSequenceList( sortedSequenceList );

	for ( int i = 0; i < sortedSequenceList.Count(); ++i )
	{
		pOutList->AddToTail( sortedSequenceList[i]->GetName() );
	}
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeMatSysMPPDag::GetActivityList( CUtlVector< CUtlString > *pOutList )
{
	pOutList->RemoveAll();

	if ( !m_eMppRoot )
		return;

	CDmeSequenceList *pDmeSequenceList = m_eMppRoot->GetValueElement< CDmeSequenceList >( "sequenceList" );
	if ( !pDmeSequenceList )
		return;

	CUtlVector< CDmeSequenceBase * > sortedSequenceList;
	pDmeSequenceList->GetSortedSequenceList( sortedSequenceList );

	CUtlDict<int, unsigned short> activityNames( true, 0, sortedSequenceList.Count() );

	for ( int i = 0; i < sortedSequenceList.Count(); ++i )
	{
		const char *pszActivityName = sortedSequenceList[i]->m_eActivity->GetName();
		if ( pszActivityName && pszActivityName[0] )
		{
			// Multiple sequences can have the same activity name; only add unique activity names
			if ( activityNames.Find( pszActivityName ) == activityNames.InvalidIndex() )
			{
				pOutList->AddToTail( pszActivityName );
				activityNames.Insert( pszActivityName, i );
			}
		}
	}
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
CUtlString UniqueIdToString( const UniqueId_t &uniqueId )
{
	char buf[64];
	UniqueIdToString( uniqueId, buf, ARRAYSIZE( buf ) );
	return CUtlString( buf );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeMatSysMPPDag::SetMppRoot( CDmElement *pMppRoot )
{
	m_eMppRoot.Set( pMppRoot );

	RemoveNullAndImplicitChildren();

	m_hDmeBodyGroupList = DMELEMENT_HANDLE_INVALID;
	m_hDmeSequenceList = DMELEMENT_HANDLE_INVALID;

	matrix3x4_t mIdentity;
	SetIdentityMatrix( mIdentity );
	GetTransform()->SetTransform( mIdentity );

	if ( pMppRoot )
	{
		CDmeBodyGroupList *pDmeBodyGroupList = pMppRoot->GetValueElement< CDmeBodyGroupList >( "bodyGroupList" );
		if ( pDmeBodyGroupList )
		{
			m_hDmeBodyGroupList = pDmeBodyGroupList->GetHandle();
		}

		CDmeSequenceList *pDmeSequenceList = pMppRoot->GetValueElement< CDmeSequenceList >( "sequenceList" );
		if ( pDmeSequenceList )
		{
			m_hDmeSequenceList = pDmeSequenceList->GetHandle();
		}

		bool bZUp = true;

		for ( int i = 0; i < pDmeBodyGroupList->m_BodyGroups.Count(); ++i )
		{
			CDmeBodyGroup *pDmeBodyGroup = pDmeBodyGroupList->m_BodyGroups[i];
			if ( !pDmeBodyGroup )
				continue;

			CDmeDag *pDmeBodyGroupDag = CreateElement< CDmeDag >( pDmeBodyGroup->GetName(), pDmeBodyGroup->GetFileId() );
			AddChild( pDmeBodyGroupDag );
			m_hChildren.AddToTail( pDmeBodyGroupDag->GetHandle() );

			for ( int j = 0; j < pDmeBodyGroup->m_BodyParts.Count(); ++j )
			{
				CDmeBodyPart *pDmeBodyPart = pDmeBodyGroup->m_BodyParts[j];
				if ( !pDmeBodyPart )
					continue;

				CDmeLODList *pDmeLODList = CastElement< CDmeLODList >( pDmeBodyPart );
				if ( !pDmeLODList )
					continue;

				for ( int k = 0; k < pDmeLODList->m_LODs.Count(); ++k )
				{
					CDmeLOD *pDmeLOD = pDmeLODList->m_LODs[k];
					if ( !pDmeLOD )
						continue;

					CDmeModel *pDmeModel = pDmeLOD->m_Model;
					if ( !pDmeModel )
						continue;

					pDmeBodyGroupDag->AddChild( pDmeModel );

					if ( !pDmeModel->IsZUp() )
						bZUp = false;
				}
			}
		}

		if ( !bZUp )
		{
			GetTransform()->SetTransform(
				matrix3x4_t(
				0.0f,  0.0f,  1.0f, 0.0f,
				1.0f,  0.0f,  0.0f, 0.0f,
				0.0f,  1.0f,  0.0f, 0.0f ) );
		}
	}
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
int CDmeMatSysMPPDag::SelectSequence( const char *pszSequenceName )
{
	m_hDmeSequence = DMELEMENT_HANDLE_INVALID;
	m_dmeOperatorList.RemoveAll();

	if ( !pszSequenceName || !pszSequenceName[0] )
		return -1;

	if ( !m_eMppRoot )
		return -1;

	CDmeSequenceList *pDmeSequenceList = m_eMppRoot->GetValueElement< CDmeSequenceList >( "sequenceList" );
	if ( !pDmeSequenceList )
		return -1;

	CUtlVector< CDmeSequenceBase * > sortedSequenceList;
	for ( int i = 0; i < pDmeSequenceList->m_Sequences.Count(); ++i )
	{
		CDmeSequenceBase *pDmeSequenceBase = pDmeSequenceList->m_Sequences[i];
		if ( pDmeSequenceBase && !Q_stricmp( pszSequenceName, pDmeSequenceBase->GetName() ) )
		{
			CDmeSequence *pDmeSequence = CastElement< CDmeSequence >( pDmeSequenceBase );
			if ( !pDmeSequence )
				continue;

			m_hDmeSequence = pDmeSequence->GetHandle();
			pDmeSequence->PrepareChannels( m_dmeOperatorList );

			return pDmeSequence->GetFrameCount();
		}
	}

	return -1;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeMatSysMPPDag::SetTime( DmeTime_t dmeTime )
{
	CDmeSequence *pDmeSequence = CastElement< CDmeSequence >( g_pDataModel->GetElement( m_hDmeSequence ) );
	if ( !pDmeSequence )
		return;

	pDmeSequence->UpdateChannels( m_dmeOperatorList, dmeTime );

	CUtlStack< CDmeDag * > depthFirstStack;
	depthFirstStack.Push( this );

	while ( depthFirstStack.Count() > 0 )
	{
		CDmeDag *pDmeDag = NULL;
		depthFirstStack.Pop( pDmeDag );
		if ( !pDmeDag )
			continue;

		for ( int i = pDmeDag->GetChildCount() - 1; i >= 0; --i )
		{
			depthFirstStack.Push( pDmeDag->GetChild( i ) );
		}

		if ( !Q_stricmp( pDmeDag->GetName(), "root" ) )
		{
			CDmeTransform *pDmeTransform = pDmeDag->GetTransform();
			if ( !pDmeTransform )
				continue;
		}
	}
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeMatSysMPPDag::SetFrame( float flFrame )
{
	// TODO: Handle a frame rate, Assume 30 fps :(
	SetTime( DmeTime_t( flFrame / 30.0f ) );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeMatSysMPPDag::RemoveNullAndImplicitChildren()
{
	// Clean up all automatically added children
	CUtlStack< int > childRemoveStack;
	for ( int i = 0; i < GetChildCount(); ++i )
	{
		CDmeDag *pDmeDag = GetChild( i );
		if ( pDmeDag )
		{
			for ( int j = 0; j < m_hChildren.Count(); ++j )
			{
				if ( pDmeDag->GetHandle() == m_hChildren[j] )
				{
					childRemoveStack.Push( i );
				}
			}
		}
		else
		{
			// Remove NULL Children
			childRemoveStack.Push( i );
		}
	}

	for ( int i = 0; i < childRemoveStack.Count(); ++i )
	{
		RemoveChild( childRemoveStack.Top() );
		childRemoveStack.Pop();
	}

	m_hChildren.RemoveAll();
}