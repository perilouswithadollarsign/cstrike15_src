//====== Copyright © 1996-2009, Valve Corporation, All rights reserved. =======
//
// Purpose: Implementation of the CDmeRig class, a class which groups a set of 
// associated constraints and operators together, allowing operations to be
// performed on the group of elements. Also contains the implementation of 
// CDmeRigAnimSetElements, a helper class used to store a list of elements which
// are all associated with a single animation set.
//
//=============================================================================
#include "movieobjects/dmerig.h"
#include "movieobjects/dmeanimationset.h"
#include "movieobjects/dmeoperator.h"
#include "movieobjects/dmeclip.h"
#include "movieobjects/dmetrackgroup.h"
#include "movieobjects/dmetrack.h"
#include "movieobjects/dmerigconstraintoperators.h"
#include "movieobjects/dmetransformcontrol.h"
#include "movieobjects/dmechannel.h"
#include "datamodel/dmelementfactoryhelper.h"
#include "tier1/fmtstr.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


// Expose this the classes to the scene database 
IMPLEMENT_ELEMENT_FACTORY( DmeRigAnimSetElements, CDmeRigAnimSetElements );
IMPLEMENT_ELEMENT_FACTORY( DmeRig, CDmeRig );


//-------------------------------------------------------------------------------------------------
// Purpose: Provide post construction processing.
//-------------------------------------------------------------------------------------------------
void CDmeRigAnimSetElements::OnConstruction()
{
	m_AnimationSet.Init( this, "animationSet" );
	m_ElementList.Init( this, "elementList" );
	m_HiddenGroups.Init( this, "hiddenGroups" );
}


//-------------------------------------------------------------------------------------------------
// Purpose: Provide processing and cleanup before shutdown
//-------------------------------------------------------------------------------------------------
void CDmeRigAnimSetElements::OnDestruction()
{

}


//-------------------------------------------------------------------------------------------------
// Purpose: Set the animation set elements in the list are to be associated with, only allowed 
// when the element list is empty.
//-------------------------------------------------------------------------------------------------
void CDmeRigAnimSetElements::SetAnimationSet( CDmeAnimationSet* pAnimationSet )
{
	// The element list must be empty when the animation set is assigned.
	Assert( m_ElementList.Count() == 0 );

	if ( m_ElementList.Count() == 0 )
	{
		m_AnimationSet = pAnimationSet;
	}
}


//-------------------------------------------------------------------------------------------------
// Purpose: Add an element to the list
//-------------------------------------------------------------------------------------------------
void CDmeRigAnimSetElements::AddElement( CDmElement *pElement )
{
	if ( pElement == NULL )
		return;

	m_ElementList.AddToTail( pElement );
}


//-------------------------------------------------------------------------------------------------
// Purpose: Remove the specified element from the list. Returns true if the element is found and 
// removed, return false if the element could not be found.
//-------------------------------------------------------------------------------------------------
bool CDmeRigAnimSetElements::RemoveElement( CDmElement *pElement )
{
	int index = m_ElementList.Find( pElement );
	if ( index != m_ElementList.InvalidIndex() )
	{
		m_ElementList.Remove( index );
		return true;
	}
	return false;
}


//-------------------------------------------------------------------------------------------------
// Purpose: Remove all of the elements from the list
//-------------------------------------------------------------------------------------------------
void CDmeRigAnimSetElements::RemoveAll()
{
	m_ElementList.RemoveAll();
}


//-------------------------------------------------------------------------------------------------
// Purpose: Add all of the elements to the provided array
//-------------------------------------------------------------------------------------------------
void CDmeRigAnimSetElements::GetElements( CUtlVector< CDmElement* > &elementList ) const
{
	int nElements = m_ElementList.Count();
	for ( int iElement = 0; iElement < nElements; ++iElement )
	{
		CDmElement *pElement = m_ElementList[ iElement ];
		if ( pElement )
		{
			elementList.AddToTail( pElement );
		}
	}
}


//-------------------------------------------------------------------------------------------------
// Add a control group to the list of hidden control groups
//-------------------------------------------------------------------------------------------------
void CDmeRigAnimSetElements::AddHiddenControlGroup( CDmeControlGroup *pControlGroup )
{
	m_HiddenGroups.AddToTail( pControlGroup->GetName() );
}


//-------------------------------------------------------------------------------------------------
// Purpose: Provide post construction processing.
//-------------------------------------------------------------------------------------------------
void CDmeRig::OnConstruction()
{
	m_AnimSetList.Init( this, "animSetList" );
}


//-------------------------------------------------------------------------------------------------
// Purpose: Provide processing and cleanup before shutdown
//-------------------------------------------------------------------------------------------------
void CDmeRig::OnDestruction()
{
	
}


//-------------------------------------------------------------------------------------------------
// Purpose: Add an element to the rig
//-------------------------------------------------------------------------------------------------
void CDmeRig::AddElement( CDmElement* pElement, CDmeAnimationSet *pAnimationSet )
{
	if ( ( pElement == NULL ) || ( pAnimationSet == NULL ) )
		return;

	// Search for an element set with the specified 
	// animation set, if none is found, create one.
	CDmeRigAnimSetElements *pAnimSetElementList = FindOrCreateAnimSetElementList( pAnimationSet );
	
	if ( pAnimSetElementList )
	{
		pAnimSetElementList->AddElement( pElement );
	}
}


//-------------------------------------------------------------------------------------------------
// Set the state of the specified control group and add it to list of control group modified by 
// the rig
//-------------------------------------------------------------------------------------------------
void CDmeRig::HideControlGroup( CDmeControlGroup *pGroup )
{
	if ( pGroup == NULL )
		return;

	CDmeAnimationSet *pAnimationSet = pGroup->FindAnimationSet( true );
	if ( pAnimationSet == NULL )
		return;

	CDmeRigAnimSetElements *pAnimSetElementList = FindOrCreateAnimSetElementList( pAnimationSet );

	if ( pAnimSetElementList)
	{
		pGroup->SetVisible( false );
		pAnimSetElementList->AddHiddenControlGroup( pGroup );
	}
}


//-------------------------------------------------------------------------------------------------
// Purpose: Remove an element from the rig
//-------------------------------------------------------------------------------------------------
void CDmeRig::RemoveElement( CDmElement *pElement, CDmeAnimationSet *pAnimationSet )
{
	if ( pElement == NULL )
		return;

	// Search each of the animation set element lists for the specified element, if the element
	// is found and removed from an animation set element list, stop and don't search the others, 
	// as each element should belong to only on animation set.
	int nAnimSets = m_AnimSetList.Count();
	for ( int iAnimSet = 0; iAnimSet < nAnimSets; ++iAnimSet )
	{
		CDmeRigAnimSetElements *pAnimSetElements = m_AnimSetList[ iAnimSet ];
		if ( pAnimSetElements )
		{
			if ( pAnimSetElements->AnimationSet() == pAnimationSet )
			{
				if ( pAnimSetElements->RemoveElement( pElement ) )
					break;
			}
		}
	}	
}


//-------------------------------------------------------------------------------------------------
// Purpose: Remove an animation set and all associated elements from the group
//-------------------------------------------------------------------------------------------------
void CDmeRig::RemoveAnimationSet( CDmeAnimationSet *pAnimationSet )
{
	int index = FindAnimSetElementList( pAnimationSet );

	if (  index != m_AnimSetList.InvalidIndex()  )
	{
		m_AnimSetList.Remove( index );
	}
}


//-------------------------------------------------------------------------------------------------
// Determine if the rig has any animation sets associated with it
//-------------------------------------------------------------------------------------------------
bool CDmeRig::HasAnyAnimationSets() const
{
	int nAnimSets = m_AnimSetList.Count();
	for ( int iAnimSet = 0; iAnimSet < nAnimSets; ++iAnimSet )
	{
		if ( CDmeRigAnimSetElements *pAnimSetElements = m_AnimSetList[ iAnimSet ] )
		{
			if ( CDmeAnimationSet *pAnimSet = pAnimSetElements->AnimationSet() )
				return true;
		}
	}

	return false;
}

//-------------------------------------------------------------------------------------------------
// Purpose: Get the list of animation sets in the group
//-------------------------------------------------------------------------------------------------
void CDmeRig::GetAnimationSets( CUtlVector< CDmeAnimationSet* > &animationSetList ) const
{
	int nAnimSets = m_AnimSetList.Count();
	animationSetList.EnsureCapacity( animationSetList.Count() + nAnimSets );

	for ( int iAnimSet = 0; iAnimSet < nAnimSets; ++iAnimSet )
	{
		CDmeRigAnimSetElements *pAnimSetElements = m_AnimSetList[ iAnimSet ];
		if ( pAnimSetElements != NULL )
		{
			CDmeAnimationSet *pAnimSet = pAnimSetElements->AnimationSet();
			if ( pAnimSet != NULL )
			{
				animationSetList.AddToTail( pAnimSet );
			}
		}
	}
}


//-------------------------------------------------------------------------------------------------
// Purpose: Get the list of elements for the specified animation set
//-------------------------------------------------------------------------------------------------
void CDmeRig::GetAnimationSetElements( const CDmeAnimationSet* pAnimationSet, CUtlVector< CDmElement* > &elementList ) const
{
	int nAnimSets = m_AnimSetList.Count();

	// Count the number of total elements in all the animation sets
	int nTotalElements = 0;
	for ( int iAnimSet = 0; iAnimSet < nAnimSets; ++iAnimSet )
	{
		CDmeRigAnimSetElements *pAnimSetElements = m_AnimSetList[ iAnimSet ];
		if ( pAnimSetElements != NULL )
		{
			nTotalElements = pAnimSetElements->NumElements();
		}
	}

	// Allocate enough space in the element list for all of the elements in the rig.
	elementList.EnsureCapacity( elementList.Count() + nTotalElements );

	// Add all the elements in the rig to the provided element list
	for ( int iAnimSet = 0; iAnimSet < nAnimSets; ++iAnimSet )
	{
		CDmeRigAnimSetElements *pAnimSetElements = m_AnimSetList[ iAnimSet ];
		if ( pAnimSetElements != NULL )
		{
			pAnimSetElements->GetElements( elementList );
		}
	}
}



//-------------------------------------------------------------------------------------------------
// Purpose: Determine if the rig has any elements from the specified animation set
//-------------------------------------------------------------------------------------------------
bool CDmeRig::HasAnimationSet( const CDmeAnimationSet *pAnimationSet ) const
{
	return ( FindAnimSetElementList( pAnimationSet ) != m_AnimSetList.InvalidIndex() );
}


//-------------------------------------------------------------------------------------------------
// Purpose: Find the element list for the specified animation set
//-------------------------------------------------------------------------------------------------
int CDmeRig::FindAnimSetElementList( const CDmeAnimationSet *pAnimationSet ) const
{
	int nAnimSets = m_AnimSetList.Count();

	for ( int iAnimSet = 0; iAnimSet < nAnimSets; ++iAnimSet )
	{
		CDmeRigAnimSetElements *pAnimSetElements = m_AnimSetList[ iAnimSet ];
		if ( pAnimSetElements == NULL )
			continue;
		
		if ( pAnimSetElements->AnimationSet() == pAnimationSet )
		{
			return iAnimSet;
		}
	}

	return m_AnimSetList.InvalidIndex();
}



//-------------------------------------------------------------------------------------------------
// Find the element list for the specified animation set or create one
//-------------------------------------------------------------------------------------------------
CDmeRigAnimSetElements *CDmeRig::FindOrCreateAnimSetElementList( CDmeAnimationSet *pAnimationSet )
{
	int nIndex = FindAnimSetElementList( pAnimationSet );
	if ( nIndex != m_AnimSetList.InvalidIndex() )
		return m_AnimSetList[ nIndex ];


	CDmeRigAnimSetElements *pAnimSetElementList = CreateElement< CDmeRigAnimSetElements >( CFmtStr( "rigElements_%s", pAnimationSet->GetName() ), GetFileId() );
	if ( pAnimSetElementList )
	{
		pAnimSetElementList->SetAnimationSet( pAnimationSet );
		m_AnimSetList.AddToTail( pAnimSetElementList );
	}

	return pAnimSetElementList;
}


//-------------------------------------------------------------------------------------------------
// Purpose: Build a list of all of the dag nodes which are influenced by rig, does not include
// dag nodes that are part of the rig.
//-------------------------------------------------------------------------------------------------
void CDmeRig::FindInfluencedDags( CUtlVector< CDmeDag* > &dagList ) const
{
	// Count the total number of elements, this is done to calculate a good pre-allocation size for lists
	int totalElements = 0;
	int nAnimSets = m_AnimSetList.Count();
	for ( int iAnimSet = 0; iAnimSet < nAnimSets; ++iAnimSet )
	{
		totalElements += m_AnimSetList[ iAnimSet ]->NumElements();
	}

	// First build a tree of all of the dag nodes that are part of the rig, we do this so that we
	// can quickly determine if a dag which is influenced by some element of the rig is part of the
	// rig and therefore should not be added to the list of influenced dag nodes.
	CUtlVector< CDmElement* > elementList( 0, totalElements );
	CUtlRBTree< const CDmeDag* > rigDags( 0, totalElements, DefLessFunc( const CDmeDag *) );
	for ( int iAnimSet = 0; iAnimSet < nAnimSets; ++iAnimSet )
	{
		const CDmaElementArray< CDmElement > &animSetElements = m_AnimSetList[ iAnimSet ]->Elements();
		int nElements = animSetElements.Count();
		for ( int iElement = 0; iElement < nElements; ++iElement )
		{
			CDmElement *pElement = animSetElements[ iElement ];
			if ( pElement )
			{		
				elementList.AddToTail( pElement );

				CDmeDag *pDag = CastElement< CDmeDag >( pElement );
				if ( pDag )
				{
					rigDags.Insert( pDag );
				}
			}
		}
	}

	// Now iterate through all of the elements that belong to the rig and find any dag nodes which
	// are influenced by the rig elements. This is done by looking for any operators and then 
	// getting the output attributes of the operator and determining if any of those attributes 
	// belong to a dag node which is not part of the rig.
	CUtlRBTree< CDmeDag* > influencedDags( 0, totalElements, DefLessFunc( CDmeDag *) );
	CUtlVector< CDmAttribute* > outputAttributes( 0, 32 );
	int nElements = elementList.Count();
	for ( int iElement = 0; iElement < nElements; ++iElement )
	{
		CDmElement *pElement = elementList[ iElement ];

		CDmeOperator *pOperator = CastElement< CDmeOperator >( pElement );
		if ( pOperator )
		{	
			outputAttributes.RemoveAll();
			pOperator->GetOutputAttributes( outputAttributes );

			int nAttributes = outputAttributes.Count();
			for ( int iAttr = 0; iAttr < nAttributes; ++iAttr )
			{
				CDmAttribute *pAttr = outputAttributes[ iAttr ];
				if ( pAttr == NULL )
					continue;
			
				CDmeDag *pDag = CastElement< CDmeDag >( pAttr->GetOwner() );
				if ( pDag == NULL )
				{
					CDmeTransform *pTransform = CastElement< CDmeTransform >( pAttr->GetOwner() );
					if ( pTransform )
					{
						pDag = pTransform->GetDag();
					}
				}

				if ( pDag == NULL )
					continue;

				// Make sure the dag is not part of the rig, if 
				// not add it to the list of influenced dag nodes.
				if ( rigDags.Find( pDag ) == rigDags.InvalidIndex() )
				{
					influencedDags.InsertIfNotFound( pDag );
				}
			}
		}		
	}

	// Copy the influenced dag nodes into the provided list
	int nDagNodes = influencedDags.Count();
	dagList.SetCount( nDagNodes );
	for ( int iDag = 0; iDag < nDagNodes; ++iDag )
	{
		dagList[ iDag ] = influencedDags[ iDag ];
	}
}



//-------------------------------------------------------------------------------------------------
// Purpose: Remove all of elements in the rig from the specified shot
//-------------------------------------------------------------------------------------------------
void CDmeRig::RemoveElementsFromShot( CDmeFilmClip *pShot )
{
	// Find the animation set channels track group, this will be used to find 
	// the the channels clip for each of the animation sets referenced by the rig.
	CDmeTrack *pAnimSetEditorTrack = NULL;
	CDmeTrackGroup *pTrackGroup = pShot->FindOrAddTrackGroup( "channelTrackGroup" );
	if ( pTrackGroup )
	{
		pAnimSetEditorTrack = pTrackGroup->FindOrAddTrack( "animSetEditorChannels", DMECLIP_CHANNEL );
	}
		

	int nAnimSets = m_AnimSetList.Count();
	for ( int iAnimSet = 0; iAnimSet < nAnimSets; ++iAnimSet )
	{
		CDmeRigAnimSetElements *pAnimSetElements = m_AnimSetList[ iAnimSet ];
		if ( pAnimSetElements == NULL )
			continue;

		CDmeAnimationSet *pAnimSet = pAnimSetElements->AnimationSet();
		if ( pAnimSet == NULL )
			continue;

		CDmeChannelsClip *pChannelsClip = NULL;
		if ( pAnimSetEditorTrack ) 
		{
			pChannelsClip = CastElement< CDmeChannelsClip >( pAnimSetEditorTrack->FindNamedClip( pAnimSet->GetName() ) );
		}

		// Make a copy of the array since we may remove elements from the original while iterating
		const CDmaElementArray< CDmElement > &elements = pAnimSetElements->Elements();
		int nElements = elements.Count();

		CUtlVector< DmElementHandle_t > elementHandles( 0, nElements );
		for ( int iElement = 0; iElement < nElements; ++iElement )
		{
			CDmElement *pElement = elements[ iElement ];
			if ( pElement )
			{
				elementHandles.AddToTail( pElement->GetHandle() );
			}
		}

		int nElementHandles = elementHandles.Count();
		for ( int iHandle = 0; iHandle < nElementHandles; ++iHandle )
		{
			// Get the element using its handle because it may have been destroyed already
			CDmElement *pElement = GetElement< CDmElement >( elementHandles[ iHandle ] );
			if ( pElement == NULL )
				continue;

			// If the element is an operator make sure it is removed from the 
			// list of operators maintained by the shot and animation set.
			CDmeOperator *pOperator = CastElement< CDmeOperator >( pElement );
			if ( pOperator )
			{					
				pAnimSet->RemoveOperator( pOperator );
				pShot->RemoveOperator( pOperator );

				// If the element is a channel remove it from the animation set's channel clip.
				if ( pChannelsClip )
				{
					CDmeChannel *pChannel = CastElement< CDmeChannel >( pElement );
					if ( pChannel )
					{
						pChannelsClip->RemoveChannel( pChannel );
					}
				}

				// If the element is a constraint reconnect the original
				// channels of the constrained dag back to the transform.
				CDmeRigBaseConstraintOperator *pConstraint = CastElement< CDmeRigBaseConstraintOperator >( pElement );
				if ( pConstraint )
				{
					pConstraint->ReconnectTransformChannels();
				}				
			}
			else if ( pElement->IsA( CDmeDag::GetStaticTypeSymbol() ) )
			{
				CDmeDag *pDag = CastElement< CDmeDag >( pElement );
				CDmeDag *pParent = pDag->GetParent();

				if ( pParent )
				{
					// Make sure the parent hasn't already been destroyed
					if ( g_pDataModel->GetElement( pParent->GetHandle() ) )
					{
						pParent->RemoveChild( pDag );
					}
				}

				// If the dag has any constraints on it, remove them
				CDmeRigBaseConstraintOperator::RemoveConstraintsFromDag( pDag );		
			}
			else if ( ( pElement->GetType() == CDmElement::GetStaticTypeSymbol() ) || pElement->IsA( CDmeTransformControl::GetStaticTypeSymbol() ) )
			{
				// If the element is just only element assume it may be a control or if the element is
				// a transform control try to remove it from the animation set's list of controls.
				pAnimSet->RemoveControl( pElement );
			}

			// Destroy the element, this is done because many elements refer to each other so if undo is enabled 
			// the auto cleanup will not take place and the elements will persist until the undo history is cleared.
			g_pDataModel->DestroyElement( pElement->GetHandle() );

		} // For iElement


		// Reset the visibility on control groups that were hidden by the rig
		SetHiddenControlGroupVisibility( pAnimSetElements, true );
		

		// Clear the element list since they have all been destroyed
		pAnimSetElements->RemoveAll();
		
	} // For iAnimSet

	m_AnimSetList.RemoveAll();
}


//-------------------------------------------------------------------------------------------------
// Set the visibility of the control groups in the hidden list
//-------------------------------------------------------------------------------------------------
void CDmeRig::SetHiddenControlGroupVisibility( CDmeRigAnimSetElements *pAnimSetElements, bool bVisible )
{
	CDmeAnimationSet *pAnimSet = pAnimSetElements->AnimationSet();
	if ( pAnimSet == NULL )
		return;

	const CDmaStringArray &hiddenGroupList = pAnimSetElements->HiddenControlGroups();
	int nNumGroups = hiddenGroupList.Count();
	for ( int iGroup = 0; iGroup < nNumGroups; ++iGroup )
	{
		CDmeControlGroup *pGroup = pAnimSet->FindControlGroup( hiddenGroupList[ iGroup ] );
		if ( pGroup )
		{
			pGroup->SetVisible( bVisible );
		}
	}
}


//-------------------------------------------------------------------------------------------------
// Hide all of the control groups in the rig's list of hidden control groups
//-------------------------------------------------------------------------------------------------
void CDmeRig::HideHiddenControlGroups( CDmeAnimationSet *pAnimationSet )
{
	int nAnimSetIndex = FindAnimSetElementList( pAnimationSet );
	if ( nAnimSetIndex == m_AnimSetList.InvalidIndex() )
		return;

	CDmeRigAnimSetElements *pAnimSetElements = m_AnimSetList[ nAnimSetIndex ];
	if ( pAnimSetElements == NULL )
		return;

	SetHiddenControlGroupVisibility( pAnimSetElements, false );
}


//-------------------------------------------------------------------------------------------------
// Purpose: Remove the specified element from any rig which it may be associated with.
//-------------------------------------------------------------------------------------------------
void CDmeRig::RemoveElementFromRig( CDmElement *pElement )
{
	if ( pElement == NULL )
		return;

	CUtlVector< CDmeRigAnimSetElements* > rigElementLists;
	FindAncestorsReferencingElement( pElement, rigElementLists );

	for ( int iList = 0; iList < rigElementLists.Count(); ++iList )
	{
		CDmeRigAnimSetElements *pElementList = rigElementLists[ iList ];
		if ( pElementList )
		{
			pElementList->RemoveElement( pElement );
		}
	}
}


//-------------------------------------------------------------------------------------------------
// Find all of the rigs which refer to the specified animation set
//-------------------------------------------------------------------------------------------------
void CollectRigsOnAnimationSet( CDmeAnimationSet *pAnimSet, CUtlVector< CDmeRig* > &rigList )
{
	CDmeFilmClip *pFilmClip = FindReferringElement< CDmeFilmClip >( pAnimSet, "animationSets" );
	if ( pFilmClip == NULL )
		return;

	CDmeDag *pScene = pFilmClip->GetScene();

	if ( !pScene || !pAnimSet )
		return;

	pScene->FindChildrenOfType( rigList );

	int i = rigList.Count();
	while ( --i >= 0 )
	{
		CDmeRig *pRig = rigList[ i ];
		if ( !pRig || !pRig->HasAnimationSet( pAnimSet ) )
		{
			rigList.Remove( i );
		}
	}
}
