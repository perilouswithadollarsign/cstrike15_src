//=========== (C) Copyright 1999 Valve, L.L.C. All rights reserved. ===========
//
// The copyright to the contents herein is the property of Valve, L.L.C.
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
// $Header: $
// $NoKeywords: $
//
// Converts from any one DMX file format to another
//
//=============================================================================

#include "dmserializers.h"
#include "dmserializers/idmserializers.h"
#include "appframework/iappsystem.h"
#include "filesystem.h"
#include "datamodel/idatamodel.h"
#include "datamodel/dmattributevar.h"
#include "datamodel/dmelementfactoryhelper.h"
#include "tier2/tier2.h"
#include "tier1/timeutils.h"
#include "tier1/fmtstr.h"
#include "tier1/utldict.h"

//-----------------------------------------------------------------------------
// format updater macros
//-----------------------------------------------------------------------------
typedef bool (*FnUpdater)( CDmElement *pElement );

#define IMPLEMENT_UPDATER( n )											\
	bool Update##n##_R( CDmElement **ppElement, bool bParity )			\
	{																	\
		return UpdateElement_R( &Update##n, ppElement, bParity );		\
	}

#define DECLARE_FORMAT_UPDATER( _name, _description, _extension, _encoding, _updaters ) \
class CDmFormatUpdater_ ## _name : public IDmFormatUpdater \
	{ \
	public: \
		CDmFormatUpdater_ ## _name() {} \
		virtual const char *GetName() const { return #_name; } \
		virtual const char *GetDescription() const { return _description; } \
		virtual const char *GetExtension() const { return _extension; } \
		virtual const char *GetDefaultEncoding() const { return _encoding; } \
		virtual int GetCurrentVersion() const { return ARRAYSIZE( _updaters ); } \
		virtual bool Update( CDmElement **ppRoot, int nSourceVersion ) \
		{ \
			if ( !ppRoot || !*ppRoot ) \
				return false; \
			if ( nSourceVersion > GetCurrentVersion() ) \
				return false; \
			int nUpdater = MAX( 0, nSourceVersion - 1 ); \
			bool bParity = true; \
			while ( _updaters[ nUpdater ] ) \
			{ \
				if ( !_updaters[ nUpdater ]( ppRoot, bParity ) ) \
					return false; \
				bParity = !bParity; \
				++nUpdater; \
			} \
			return true; \
		} \
	}; \
	static CDmFormatUpdater_ ## _name s_FormatUpdater ## _name;

#define INSTALL_FORMAT_UPDATER( _name ) g_pDataModel->AddFormatUpdater( &s_FormatUpdater ## _name )

//-----------------------------------------------------------------------------
// updater helper functions
//-----------------------------------------------------------------------------

void ChangeAttributeType( CDmElement *pElement, const char *pAttrName, DmAttributeType_t type )
{
	CDmAttribute *pAttr = pElement->GetAttribute( pAttrName );
	Assert( pAttr );
	if ( !pAttr )
		return;

	pAttr->ChangeType_UNSAFE( type );
}

CDmElement *FindElementNamed( CDmrElementArray<> elements, const char *pTargetName )
{
	int nElements = elements.Count();
	for ( int i = 0; i < nElements; ++i )
	{
		CDmElement *pElement = elements[ i ];
		if ( pElement && !V_strcmp( pTargetName, pElement->GetName() ) )
			return pElement;
	}
	return NULL;
}

CDmElement *FindChannelsClipForChannel( CDmElement *pFilmClip, CDmElement *pChannel )
{
	const static CUtlSymbolLarge channelsClipSym = g_pDataModel->GetSymbol( "DmeChannelsClip" );
	const static CUtlSymbolLarge channelsSym = g_pDataModel->GetSymbol( "channels" );
	DmAttributeReferenceIterator_t i = g_pDataModel->FirstAttributeReferencingElement( pChannel->GetHandle() );
	while ( i != DMATTRIBUTE_REFERENCE_ITERATOR_INVALID )
	{
		CDmAttribute *pAttribute = g_pDataModel->GetAttribute( i );
		CDmElement *pParent = pAttribute->GetOwner();
		if ( pParent && pParent->GetType() == channelsClipSym && pAttribute->GetNameSymbol() == channelsSym )
		{
			CUtlVector< ElementPathItem_t > path;
			if ( pParent->FindReferer( pFilmClip->GetHandle(), path, TD_ALL ) ) // NOTE - with non-typed elements, TD_SHALLOW == TD_NONE and TD_DEEP == TD_ALL
				return pParent;
		}

		i = g_pDataModel->NextAttributeReferencingElement( i );
	}

	return NULL;
}

CDmElement *FindChannelTargettingElement( CDmElement *pTarget, const char *pTargetAttr, bool bFromTarget )
{
	const static CUtlSymbolLarge channelSym = g_pDataModel->GetSymbol( "DmeChannel" );
	const static CUtlSymbolLarge fromElementSym = g_pDataModel->GetSymbol( bFromTarget ? "fromElement" : "toElement" );
	DmAttributeReferenceIterator_t i = g_pDataModel->FirstAttributeReferencingElement( pTarget->GetHandle() );
	for( ; i != DMATTRIBUTE_REFERENCE_ITERATOR_INVALID; i = g_pDataModel->NextAttributeReferencingElement( i ) )
	{
		CDmAttribute *pAttribute = g_pDataModel->GetAttribute( i );
		if ( pAttribute->GetNameSymbol() != fromElementSym )
			continue;

		CDmElement *pParent = pAttribute->GetOwner();
		if ( !pParent || pParent->GetType() != channelSym )
			continue;

		if ( !V_strcmp( pParent->GetValueString( bFromTarget ? "fromAttribute" : "toAttribute" ), pTargetAttr ) )
			return pParent;
	}

	return NULL;
}

CDmElement *GetLogLayerFromChannel( CDmElement *pChannel )
{
	CDmElement *pLog = pChannel->GetValueElement< CDmElement >( "log" );
	if ( !pLog )
		return NULL;

	const CUtlVector< DmElementHandle_t > &layers = pLog->GetValue< CUtlVector< DmElementHandle_t > >( "layers" );
	if ( layers.Count() != 1 )
		return NULL;

	return g_pDataModel->GetElement( layers[ 0 ] );
}

void ConcatTransforms( Quaternion qParent, Vector vParent, Quaternion qLocal, Vector vLocal, Quaternion &qFinal, Vector &vFinal )
{
	matrix3x4_t mLocal, mParent, mFinal;
	QuaternionMatrix( qLocal, vLocal, mLocal );
	QuaternionMatrix( qParent, vParent, mParent );
	ConcatTransforms( mParent, mLocal, mFinal );
	MatrixQuaternion( mFinal, qFinal );
	MatrixPosition( mFinal, vFinal );
}





//-----------------------------------------------------------------------------
// format updater functions
//-----------------------------------------------------------------------------

// IMPORTANT: all elements created during update MUST have their parity set to the parity of the current updater
//            otherwise, they (and their children) won't be traversed during the next updater




//-----------------------------------------------------------------------------
// Update19 -- Removal of Control display sets. Removes all control display 
// sets from animation sets and removes references to display sets from all 
// controls. Also updates rigs to have hidden control groups instead of 
// display sets.
//-----------------------------------------------------------------------------
void FixupRigGroups( CDmElement *pRig );

bool Update19( CDmElement *pElement )
{
	const static CUtlSymbolLarge symDmeControlDisplaySet = g_pDataModel->GetSymbol( "DmeControlDisplaySet" );
	const static CUtlSymbolLarge symDmeAnimationSet = g_pDataModel->GetSymbol( "DmeAnimationSet" );
	const static CUtlSymbolLarge symDmeRig = g_pDataModel->GetSymbol( "DmeRig" );
	const static CUtlSymbolLarge symDisplaySetAttr = g_pDataModel->GetSymbol( "displaySet" );
	
	if ( pElement->GetType() == symDmeControlDisplaySet )
	{
		// This case shouldn't actually happen all paths which refer to display 
		// sets should be removed before the display set can actually be reached.
		DestroyElement( pElement );
	}
	else if ( pElement->GetType() == symDmeAnimationSet )
	{
		pElement->RemoveAttribute( "displaySets" );
	}
	else if ( pElement->GetType() == symDmeRig )
	{
		if ( pElement->HasAttribute( "displaySetElements" ) )
		{
			FixupRigGroups( pElement );
			pElement->RemoveAttribute( "displaySetElements" );
		}
	}
	else 
	{
		pElement->RemoveAttribute( "displaySet" );
	}
	
	return true;
}

CDmElement *FindChildControlGroup( CDmElement *pGroup, const char *pName, bool bRecursive )
{
	CDmAttribute *pChildrenAttr = pGroup->GetAttribute( "children", AT_ELEMENT_ARRAY );
	if ( pChildrenAttr == NULL )
		return NULL;
		
	CDmrElementArray< CDmElement > children( pChildrenAttr );
	int nNumChildren = children.Count();

	for ( int iChild = 0; iChild < nNumChildren; ++iChild )
	{
		CDmElement *pChild = children[ iChild ];
		if ( pChild ) 
		{
			if ( V_stricmp( pChild->GetName(), pName ) == 0 )
				return pChild;

			if ( bRecursive )
			{
				FindChildControlGroup( pChild, pName, true );
			}
		}
	}

	return NULL;
}


CDmElement *FindOrCreateControlGroup( CDmElement *pRig, CDmElement *pParentGroup, const char *pGroupName )
{
	CDmElement *pGroup = FindChildControlGroup( pParentGroup, pGroupName, true );
	
	if ( pGroup == NULL )
	{
		pGroup = CreateElement< CDmElement >( "DmeControlGroup", pGroupName, pParentGroup->GetFileId() );
		pGroup->SetParity( pRig->GetParity() );

		// Add the new group to the parent
		CDmAttribute *pChildrenAttr = pParentGroup->GetAttribute( "children", AT_ELEMENT_ARRAY );
		if ( pChildrenAttr )
		{
			CDmrElementArray< CDmElement > children( pChildrenAttr );
			children.AddToTail( pGroup );
		}
	}

	return pGroup;
}

void ReParentCongrolGroup( CDmElement *pControlGroup, CDmElement *pNewParent )
{
	const static CUtlSymbolLarge symChildren = g_pDataModel->GetSymbol( "children" );

	if ( ( pControlGroup == NULL ) || ( pNewParent == NULL ) )
		return;

	// Find the current parent of the control group
	CDmElement *pCurrentParent = FindReferringElement< CDmElement >( pControlGroup, symChildren );
	if ( pCurrentParent == pNewParent )
		return;

	// Add the group to the children of the new parent
	CDmAttribute *pNewChildrenAttr = pNewParent->GetAttribute( "children", AT_ELEMENT_ARRAY );
	if ( pNewChildrenAttr == NULL )
	{
		pNewChildrenAttr = pNewParent->AddAttributeElementArray< CDmElement >( "children" );
	}

	CDmrElementArray< CDmElement > newChildren( pNewChildrenAttr );
	newChildren.AddToTail( pControlGroup );

	// Remove the group from the children of the old parent
	if ( pCurrentParent )
	{
		CDmAttribute *pOldChildrenAttr = pCurrentParent->GetAttribute( "children", AT_ELEMENT_ARRAY );
		if ( pOldChildrenAttr )
		{
			CDmrElementArray< CDmElement > oldChildren( pOldChildrenAttr );
			int nIndex = oldChildren.Find( pControlGroup );
			oldChildren.Remove( nIndex );
		}
	}
}


bool IsControlGroupEmpty( CDmElement *pControlGroup )
{	
	CDmAttribute *pControlsAttr = pControlGroup->GetAttribute( "controls" );
	if ( pControlsAttr )
	{
		CDmrElementArray< CDmElement > controls( pControlsAttr );
		if ( controls.Count() > 0 )
			return false;
	}

	CDmAttribute *pChildrenAttr = pControlGroup->GetAttribute( "children", AT_ELEMENT_ARRAY );
	if ( pChildrenAttr )
	{
		CDmrElementArray< CDmElement > children( pChildrenAttr );
		int nNumChildren = children.Count();
		for ( int iChild = 0; iChild < nNumChildren; ++iChild )
		{
			CDmElement *pChild = children[ iChild ];
			if ( pChild )
			{
				if ( IsControlGroupEmpty( pChild ) == false )
					return false;
			}
		}
	}

	return true;			
}


void SetControlGroupState( CDmElement *pRootControlGroup, const char *pGroupName, bool bVisible, bool bSnappable )
{
	CDmElement *pControlGroup = FindChildControlGroup( pRootControlGroup, pGroupName, true );
	if ( pControlGroup == NULL )
		return;

	pControlGroup->SetValue< bool >( "visible", bVisible, true );
	pControlGroup->SetValue< bool >( "snappable", bSnappable, true );
}

void MoveRigControlsToGroup( CDmElement *pRig, CDmElement *pAnimSetElements, CDmElement *pRootGroup, const char *pSrcGroupName, const char *pDstGroupName, bool bTransformOnly )
{	
	const static CUtlSymbolLarge symDmeTransformControl = g_pDataModel->GetSymbol( "DmeTransformControl" );
	const static CUtlSymbolLarge symElementList = g_pDataModel->GetSymbol( "elementList" );

	// First find the specified source control group
	CDmElement *pSrcGroup = pRootGroup;
	if ( pSrcGroupName )
	{
		pSrcGroup = FindChildControlGroup( pRootGroup, pSrcGroupName, true );
	}
	if ( pSrcGroup == NULL )
		return;
	
	// Iterate through the controls in the source control group, 
	// if they are rig elements add them to the destination group.
	CDmAttribute *pControlsAttr = pSrcGroup->GetAttribute( "controls", AT_ELEMENT_ARRAY );
	if ( pControlsAttr == NULL )
		return;
	
	CDmrElementArray< CDmElement > controls( pControlsAttr );
	int nNumControls = controls.Count();

	CDmElement *pDstGroup = NULL;
	CDmAttribute *pDstControlsAttr = NULL;

	for ( int iControl = 0; iControl < nNumControls; ++iControl )
	{
		CDmElement *pControl = controls[ iControl ];
		if ( pControl == NULL )
			continue;

		if ( bTransformOnly && ( pControl->GetType() != symDmeTransformControl ) )
			continue;

		CUtlVector< CDmElement* > referringElements( 0, 4 );
		FindReferringElements( referringElements, pControl, symElementList );
		if ( referringElements.Find( pAnimSetElements ) != referringElements.InvalidIndex() )
		{
			if ( pDstGroup == NULL )
			{
				pDstGroup = FindOrCreateControlGroup( pRig, pRootGroup, pDstGroupName );
			}

			if ( pDstControlsAttr == NULL )
			{
				pDstControlsAttr = pDstGroup->GetAttribute( "controls", AT_ELEMENT_ARRAY );
				if ( pDstControlsAttr == NULL )
				{
					pDstControlsAttr = pDstGroup->AddAttributeElementArray< CDmElement >( "controls" );
				}
			}

			CDmrElementArray< CDmElement > dstControls( pDstControlsAttr );
			dstControls.AddToTail( pControl );			
		}
	}

	// Now remove the rig controls from the source group
	if ( pDstControlsAttr != NULL )
	{
		CDmrElementArray< CDmElement > rigControls( pDstControlsAttr );
		int nNumRigControls = rigControls.Count();

		for ( int iControl = 0; iControl < nNumRigControls; ++iControl )
		{
			CDmElement *pControl = rigControls[ iControl ];
			int nIndex = controls.Find( pControl );
			if ( nIndex != controls.InvalidIndex() )
			{
				controls.Remove( nIndex );
			}
		}
	}	
}


void RemoveInvalidTransformControls( CDmElement *pAnimSet )
{
	const static CUtlSymbolLarge symDmeTransformControl = g_pDataModel->GetSymbol( "DmeTransformControl" );
	const static CUtlSymbolLarge symControls = g_pDataModel->GetSymbol( "controls" );

	CDmAttribute *pControlsAttr = pAnimSet->GetAttribute( "controls", AT_ELEMENT_ARRAY );
	if ( pControlsAttr == NULL )
		return;
	
	CDmrElementArray< CDmElement > controls( pControlsAttr );
	int nNumControls = controls.Count();
	
	CUtlVector< CDmElement* > invalidControls( 0, nNumControls / 2 );
	for ( int iControl = 0; iControl < nNumControls; ++iControl )
	{
		CDmElement *pControl = controls[ iControl ];
		if ( pControl == NULL )
			continue;

		if ( pControl->GetType() == symDmeTransformControl )
		{
			CDmElement *pPosChannel = pControl->GetValueElement< CDmElement >( "positionChannel" );
			CDmElement *pRotChannel = pControl->GetValueElement< CDmElement >( "orientationChannel" );
			
			if ( ( pPosChannel == NULL ) && ( pRotChannel == NULL ) )
			{
				invalidControls.AddToTail( pControl );
			}
		}
	}

	int nNumInvalid = invalidControls.Count();
	for ( int iInvalid = 0; iInvalid < nNumInvalid; ++iInvalid )
	{
		CDmElement *pInvalidControl = invalidControls[ iInvalid ];
		if ( pInvalidControl == NULL )
			return;

		// Remove the control from the animation set
		int nIndex = controls.Find( pInvalidControl );
		if ( nIndex != controls.InvalidIndex() )
		{
			controls.Remove( nIndex );
		}

		// Remove the control from the control groups
		CUtlVector < CDmElement* > referringElements( 0, 4 );
		FindReferringElements( referringElements, pInvalidControl, symControls );
		int nNumReferringElements = referringElements.Count();

		for ( int iElement = 0; iElement < nNumReferringElements; ++iElement )
		{
			CDmElement *pElement = referringElements[ iElement ];
			if ( pElement == NULL )
				continue;

			CDmAttribute *pControlsAttr = pElement->GetAttribute( "controls", AT_ELEMENT_ARRAY );
			if ( pControlsAttr == NULL )
				continue;

			CDmrElementArray< CDmElement > controls( pControlsAttr );
			nIndex = controls.Find( pInvalidControl );
			if ( nIndex != controls.InvalidIndex() )
			{
				controls.Remove( nIndex );
			}
		}
	}
}


void FixupRigGroupsForAnimationSet( CDmElement *pRig, CDmElement *pAnimSetElements )
{
	const static CUtlSymbolLarge symDmeAnimationSet = g_pDataModel->GetSymbol( "DmeAnimationSet" );

	// Get the animation set
	CDmElement *pAnimSet = pAnimSetElements->GetValueElement< CDmElement >( "animationSet" );
	if ( ( pAnimSet == NULL ) || ( pAnimSet->GetType() != symDmeAnimationSet ) )
		return;

	// First clean out any invalid controls from the animations set
	RemoveInvalidTransformControls( pAnimSet );


	// Get the root control group of the animation set
	CDmElement *pRootControlGroup = pAnimSet->GetValueElement< CDmElement >( "rootControlGroup" );
	if ( pRootControlGroup == NULL )
		return;

	// Move the rig controls into their own groups
	MoveRigControlsToGroup( pRig, pAnimSetElements, pRootControlGroup, NULL, "RigHelpers", false );
	MoveRigControlsToGroup( pRig, pAnimSetElements, pRootControlGroup, "Body", "RigBody", true );
	MoveRigControlsToGroup( pRig, pAnimSetElements, pRootControlGroup, "Arms", "RigArms", true );
	MoveRigControlsToGroup( pRig, pAnimSetElements, pRootControlGroup, "Legs", "RigLegs", true );
	MoveRigControlsToGroup( pRig, pAnimSetElements, pRootControlGroup, "Root", "RigRoot", true );

	// Move the fingers into their own groups
	CDmElement *pArmsGroup = FindChildControlGroup( pRootControlGroup, "Arms", true );
	if ( pArmsGroup )
	{
		// Find the fingers group and make sure it is a child of the root and then 
		// make sure the left and right fingers groups children of the fingers group.
		CDmElement *pFingers = FindOrCreateControlGroup( pRig, pRootControlGroup, "Fingers" );
		if ( pFingers )
		{
			ReParentCongrolGroup( pFingers, pRootControlGroup );

			CDmElement *pFingersR = FindChildControlGroup( pArmsGroup, "Fingers_R", false );
			if ( pFingersR )
			{
				pFingersR->SetName( "RightFingers" );
				ReParentCongrolGroup( pFingersR, pFingers );
			}

			CDmElement *pFingersL = FindChildControlGroup( pArmsGroup, "Fingers_L", false );
			if ( pFingersL )
			{
				pFingersL->SetName( "LeftFingers" );
				ReParentCongrolGroup( pFingersL, pFingers );
			}
		}
	}

	// Hide the original body groups and rig utility groups
	SetControlGroupState( pRootControlGroup, "Arms",			false, true );
	SetControlGroupState( pRootControlGroup, "Legs",			false, true );
	SetControlGroupState( pRootControlGroup, "Body",			false, true );
	SetControlGroupState( pRootControlGroup, "Root",			false, true );
	SetControlGroupState( pRootControlGroup, "Other",			false, true );
	SetControlGroupState( pRootControlGroup, "Unknown",			false, true );
	SetControlGroupState( pRootControlGroup, "Attachments",		false, true );
	SetControlGroupState( pRootControlGroup, "RigHelpers",		false, false );
	SetControlGroupState( pRootControlGroup, "RigConstraints",	false, false );

	// Add the body groups to the list of groups hidden by the rig
	CDmAttribute *pHiddenGroupsAttr = pAnimSetElements->GetAttribute( "hiddenGroups", AT_STRING_ARRAY );
	if ( pHiddenGroupsAttr == NULL )
	{
		pHiddenGroupsAttr = pAnimSetElements->AddAttribute( "hiddenGroups", AT_STRING_ARRAY );
	}
	
	CDmrStringArray hiddenGroups( pHiddenGroupsAttr );
	hiddenGroups.AddToTail( "Body" );
	hiddenGroups.AddToTail( "Arms" );
	hiddenGroups.AddToTail( "Legs" );
	hiddenGroups.AddToTail( "Root" );
	hiddenGroups.AddToTail( "Other" );
	hiddenGroups.AddToTail( "Unknown" );

	
	// Set the color of all of the top level groups and remove any control groups which are empty
	CDmAttribute *pChildrenAttr = pRootControlGroup->GetAttribute( "children", AT_ELEMENT_ARRAY );
	if ( pChildrenAttr )
	{
		Color topLevelColor( 0, 128, 255, 255 );
		Color controlColor( 200, 200, 200, 255 );
		
		CDmrElementArray< CDmElement > children( pChildrenAttr );
		int nChildren = children.Count();

		CUtlVector< CDmElement* > emptyGroups( 0, nChildren );

		for ( int iChild = 0; iChild < nChildren; ++iChild )
		{
			CDmElement *pChildGroup = children[ iChild ];
			pChildGroup->SetValue< Color >( "groupColor", topLevelColor );
			pChildGroup->SetValue< Color >( "controlColor", controlColor );

			// If the child group is empty add it to the list of children to remove
			if ( IsControlGroupEmpty( pChildGroup ) )
			{
				emptyGroups.AddToTail( pChildGroup );
			}
		}
		
		// Remove the empty groups
		int nNumEmptyGroups = emptyGroups.Count();
		for ( int iEmptyGroup = 0; iEmptyGroup < nNumEmptyGroups; ++iEmptyGroup )
		{
			CDmElement *pEmptyGroup = emptyGroups[ iEmptyGroup ];
			int nIndex = children.Find( pEmptyGroup );
			if ( nIndex != children.InvalidIndex() )
			{
				children.Remove( nIndex );
			}
		}
	}
}


void FixupRigGroups( CDmElement *pRig )
{	
	CDmAttribute *pAnimSetListAttr = pRig->GetAttribute( "animSetList", AT_ELEMENT_ARRAY );
	if ( pAnimSetListAttr == NULL )
		return;

	CDmrElementArray<> animSetList( pAnimSetListAttr );

	int nNumAnimSets = animSetList.Count();
	for ( int iAnimSet = 0; iAnimSet < nNumAnimSets; ++iAnimSet )
	{
		CDmElement *pAnimSetElements = animSetList[ iAnimSet ];
		if ( pAnimSetElements )
		{
			FixupRigGroupsForAnimationSet( pRig, pAnimSetElements );
		}
	}
}


//-----------------------------------------------------------------------------
// Update18 -- removing procedural presetgroup and presets (now exist only in code, and not as elements)
//-----------------------------------------------------------------------------

bool Update18( CDmElement *pElement )
{
	const static CUtlSymbolLarge symDmeAnimationSet = g_pDataModel->GetSymbol( "DmeAnimationSet" );
	const static CUtlSymbolLarge symDmePreset = g_pDataModel->GetSymbol( "DmePreset" );

	if ( pElement->GetType() == symDmeAnimationSet )
	{
		CDmAttribute *pPresetGroupsAttr = pElement->GetAttribute( "presetGroups", AT_ELEMENT_ARRAY );
		if ( pPresetGroupsAttr )
		{		
			CDmrElementArray<> presetGroups( pPresetGroupsAttr );
			int nNumPresetGroups = presetGroups.Count();
			for ( int i = nNumPresetGroups - 1; i >= 0; --i )
			{
				if ( CDmElement *pPresetGroup = presetGroups[ i ] )
				{
					if ( V_strcmp( pPresetGroup->GetName(), "procedural" ) != 0 && V_strcmp( pPresetGroup->GetName(), "Procedural" ) != 0 )
						continue;
				}

				presetGroups.Remove( i );
			}
		}
	}
	else if ( pElement->GetType() == symDmePreset )
	{
		pElement->RemoveAttribute( "procedural" );
	}

	return true;
}


//-----------------------------------------------------------------------------
// Update17 -- Transform control unification. Separate transform controls for
// position and orientation are combine in to a single transform control and
// the control group which previously contained the separate position and 
// orientation controls is removed.
//-----------------------------------------------------------------------------
void UnifyTransformControl( CDmElement *pControl, CDmElement *pAnimSet );
void DestroyControl( CDmElement *pControl );
CDmElement *CollapseControlGroup( CDmElement *pControlGroup, CDmElement *pRootGroup );

bool Update17( CDmElement *pElement )
{
	const static CUtlSymbolLarge symDmeAnimationSet = g_pDataModel->GetSymbol( "DmeAnimationSet" );
	const static CUtlSymbolLarge symDmePreset = g_pDataModel->GetSymbol( "DmePreset" );

	// If the element is an animation set iterate through its controls and unify the transform controls.
	// Note that while the transform controls themselves would be visited they may be visited from the 
	// animation set or control group containing them, making removing the controls from the animation set
	// or control group problematic since the calling function is in the middle of walking the array that 
	// will be modified.
	if ( pElement->GetType() == symDmeAnimationSet )
	{	
		CDmAttribute *pControlsAttr = pElement->GetAttribute( "controls", AT_ELEMENT_ARRAY );
		if ( pControlsAttr )
		{		
			// Make a copy of the controls list. A copy is must be made 
			// since the actual list may be modified during iteration.
			CDmrElementArray<> controls( pControlsAttr );
			int nNumControls = controls.Count();
			
			CUtlVector< DmElementHandle_t > controlHandles;
			controlHandles.SetSize( nNumControls );
			for ( int iControl = 0; iControl < nNumControls; ++iControl )
			{
				controlHandles[ iControl ] = controls[ iControl ]->GetHandle();
			}

			// Iterate through the controls, unifying and of the transform controls which contain only a 
			// position or rotation with the corresponding control containing the other component.
			for ( int iControl = 0; iControl < nNumControls; ++iControl )
			{
				CDmElement *pControl = g_pDataModel->GetElement( controlHandles[ iControl ] );
				if ( pControl )
				{
					UnifyTransformControl( pControl, pElement );
				}
			}
		}
	}
	else if ( pElement->GetType() == symDmePreset )
	{
		// For presets merge all control value elements that share the same base name
		CDmAttribute *pControlValuesAttr = pElement->GetAttribute( "controlValues", AT_ELEMENT_ARRAY );
		if ( pControlValuesAttr )
		{			
			CDmrElementArray<> controlValueElements( pControlValuesAttr );
			int nControlValues = controlValueElements.Count();
			if ( nControlValues > 0 )
			{
				CUtlDict< CDmElement*, int > mergedControls;
				CUtlVector< DmElementHandle_t > validControlValues( 0, nControlValues );

				for ( int iValue = 0; iValue < nControlValues; ++iValue )
				{
					CDmElement *pControlValue = controlValueElements[ iValue ];
					if ( pControlValue == NULL )
						continue;
			
					bool bPos = pControlValue->HasAttribute( "valuePosition" );
					bool bRot = pControlValue->HasAttribute( "valueOrientation" );
					const char *pComponentStart = NULL;
					const char *pName = pControlValue->GetName();

					// Must have a position or rotation value, but not both
					if ( bPos && !bRot )
					{
						pComponentStart = V_strstr( pName, " - pos" );
					}
					else if ( bRot && !bPos )
					{
						pComponentStart = V_strstr( pName, " - rot" );
					}

					// If the control value will not be merged, 
					// add it to the list of valid control values.
					if ( pComponentStart == NULL )
					{					
						validControlValues.AddToTail( pControlValue->GetHandle() );
						continue;
					}

					// Construct the base name by removing the component extension
					char baseName[ 256 ];
					V_strncpy( baseName, pName, 256 );
					baseName[ pComponentStart - pName ] = 0;
				
					// Find or create the new control value
					CDmElement *pNewControlValue = NULL;
					int nIndex = mergedControls.Find( baseName );
					if ( nIndex == mergedControls.InvalidIndex() )
					{
						pNewControlValue = CreateElement< CDmElement >( "DmElement", baseName, pElement->GetFileId() );
						pNewControlValue->SetParity( pElement->GetParity() );
						nIndex = mergedControls.Insert( baseName, pNewControlValue );
						validControlValues.AddToTail( pNewControlValue->GetHandle() );
					}
					else
					{
						pNewControlValue = mergedControls.Element( nIndex );
					}

					// Copy the value from the old control value element
					if ( pNewControlValue )
					{
						if ( bPos )
						{
							Vector position = pControlValue->GetValue< Vector >( "valuePosition" );
							pNewControlValue->SetValue< Vector >( "valuePosition", position );
						}
						else
						{
							Quaternion orentation = pControlValue->GetValue< Quaternion >( "valueOrientation" );
							pNewControlValue->SetValue< Quaternion >( "valueOrientation", orentation );
						}
					}
				}

				// Replace the original list of control elements with 
				// the list of currently valid control elements.
				controlValueElements = validControlValues;
			}
		}
	}



	return true;
}


void UnifyTransformControl( CDmElement *pTransformControl, CDmElement *pAnimSet )
{
	const static CUtlSymbolLarge symDmeTransformControl = g_pDataModel->GetSymbol( "DmeTransformControl" );
	const static CUtlSymbolLarge symDmeControlGroup = g_pDataModel->GetSymbol( "DmeControlGroup" );
	const static CUtlSymbolLarge symDmeAnimationSet = g_pDataModel->GetSymbol( "DmeAnimationSet" );
	const static CUtlSymbolLarge symDmeRigAnimSetElements = g_pDataModel->GetSymbol( "DmeRigAnimSetElements" );
	const static CUtlSymbolLarge symDmeRig = g_pDataModel->GetSymbol( "DmeRig" );
	const static CUtlSymbolLarge symControls = g_pDataModel->GetSymbol( "controls" );
	const static CUtlSymbolLarge symElementList = g_pDataModel->GetSymbol( "elementList" );
	const static CUtlSymbolLarge symDisplaySetElements = g_pDataModel->GetSymbol( "displaySetElements" );

	if ( pTransformControl->GetType() == symDmeTransformControl )
	{			
		const char *baseName = pTransformControl->GetValueString( "baseName" );

		// Find the control group containing the specified control
		CDmElement *pTransformControlGroup = NULL;
		CUtlVector< CDmElement* > controlGroups;
		FindReferringElements( controlGroups, pTransformControl, symControls );
		int nNumGroups = controlGroups.Count();
		for ( int iGroup = 0; iGroup < nNumGroups; ++iGroup )
		{
			CDmElement *pGroup = controlGroups[ iGroup ];
			if ( pGroup->GetType() == symDmeControlGroup )
			{
				pTransformControlGroup = pGroup;
				break;
			}
		}

		Assert( pTransformControlGroup );
		if ( pTransformControlGroup == NULL )
			return;

		// Find the position or orientation control that corresponds to the given control
		CDmElement *pPositionControl = NULL;
		CDmElement *pOrientationControl = NULL;
		CDmElement *pPositionChannel = NULL;
		CDmElement *pOrientationChannel = NULL;

		CDmAttribute *pControlsAttr = pTransformControlGroup->GetAttribute( "controls", AT_ELEMENT_ARRAY );
		if ( pControlsAttr )
		{
			CDmrElementArray<> controlList( pControlsAttr );
			for ( int iControl = 0; iControl < controlList.Count(); ++iControl )
			{
				CDmElement *pControl = controlList[ iControl ];
				if ( pControl->HasAttribute( "position", AT_ELEMENT ) )
				{
					pPositionControl = pControl;
					pPositionChannel = pPositionControl->GetValueElement< CDmElement >( "position" );
				}
				else if ( pControl->HasAttribute( "orientation", AT_ELEMENT ) )
				{
					pOrientationControl = pControl;
					pOrientationChannel = pOrientationControl->GetValueElement< CDmElement >( "orientation" );
				}
				
				if ( pPositionControl && pOrientationControl )
					break;
			}
		}


		// Make  sure we got the correct element
		if ( ( pPositionControl != pTransformControl ) && ( pOrientationControl != pTransformControl ) )
			return;

		// Get the position and value controls from the old element
		Vector position = pPositionControl ? pPositionControl->GetValue< Vector >( "valuePosition", vec3_origin ) : vec3_origin;
		Quaternion orientation = pOrientationControl ? pOrientationControl->GetValue< Quaternion >( "valueOrientation", quat_identity ) : quat_identity;
						

		// Create the new transform control
		CDmElement *pNewTransformControl = CreateElement< CDmElement >( "DmeTransformControl", baseName, pTransformControl->GetFileId() );
		pNewTransformControl->SetParity( pTransformControl->GetParity() );
		pNewTransformControl->SetValue( "positionChannel", pPositionChannel );
		pNewTransformControl->SetValue( "orientationChannel", pOrientationChannel );
		pNewTransformControl->SetValue( "valuePosition", position );
		pNewTransformControl->SetValue( "valueOrientation", orientation );

		if ( pPositionChannel )
		{
			pPositionChannel->SetValue( "fromElement", pNewTransformControl );
		}

		if ( pOrientationChannel )
		{
			pOrientationChannel->SetValue( "fromElement", pNewTransformControl );
		}

		
		// If the old transform control was in a display set, add the new one to the display set as well
		CDmElement *pDisplaySet = pTransformControl->GetValueElement< CDmElement >( "displaySet" );
		if ( pDisplaySet )
		{
			pNewTransformControl->SetValue( "displaySet", pDisplaySet );
		}		

		// Add the new control to the animation set
		CDmAttribute *pAnimSetControlsAttr = pAnimSet->GetAttribute( "controls", AT_ELEMENT_ARRAY );
		if ( pAnimSetControlsAttr )
		{
			CDmrElementArray<> animSetControls( pAnimSetControlsAttr );
			animSetControls.AddToTail( pNewTransformControl );
		}

		// If the old transform control was part of a rig, add the new transform control to the rig as well
		CUtlVector< CDmElement * > animSetRigList;
		FindReferringElements( animSetRigList, pTransformControl, symElementList );
		int nNumAnimSetRigs = animSetRigList.Count();
		for ( int iAnimSetRig = 0; iAnimSetRig < nNumAnimSetRigs; ++iAnimSetRig )
		{
			CDmElement *pAnimElements = animSetRigList[ iAnimSetRig ];
			if ( ( pAnimElements != NULL ) && ( pAnimElements->GetType() == symDmeRigAnimSetElements ) && 
				 ( pAnimElements->GetValueElement< CDmElement >( "animationSet" ) == pAnimSet ) )
			{
				CDmAttribute *pElementsAttr = pAnimElements->GetAttribute( "elementList", AT_ELEMENT_ARRAY );
				if ( pElementsAttr )
				{
					CDmrElementArray<> elementsList( pElementsAttr );
					elementsList.AddToTail( pNewTransformControl );
				}
			}
		}

		CUtlVector< CDmElement * > rigList;
		FindReferringElements( rigList, pTransformControl, symDisplaySetElements );
		int nNumRigs = rigList.Count();
		for ( int iRig = 0; iRig < nNumRigs; ++iRig )
		{
			CDmElement *pRig = rigList[ iRig ];
			if ( ( pRig != NULL ) && ( pRig->GetType() == symDmeRig ) )
			{
				CDmAttribute *pDisplaySetElementsAttr = pRig->GetAttribute( "displaySetElements", AT_ELEMENT_ARRAY );
				if ( pDisplaySetElementsAttr )
				{
					CDmrElementArray<> displaySetElements( pDisplaySetElementsAttr );
					displaySetElements.AddToTail( pNewTransformControl );
				}
			}
		}


		// Move all the controls and children from the transform control group up to 
		// its parent and remove the the transform control group from its parent.
		CDmElement *pParentControlGroup = pAnimSet->GetValueElement< CDmElement >( "rootControlGroup" );
		if ( pTransformControlGroup )
		{
			pParentControlGroup = CollapseControlGroup( pTransformControlGroup, pParentControlGroup );
		}

		// Add the new control to parent control group
		if ( pParentControlGroup )
		{
			CDmAttribute *pParentControlsAttr = pParentControlGroup->GetAttribute( "controls", AT_ELEMENT_ARRAY );
			if ( pParentControlsAttr )
			{
				CDmrElementArray<> parentControls( pParentControlsAttr );
				parentControls.AddToTail( pNewTransformControl );
			}
		}
		
		// Remove the old controls from all animation sets and 
		// control groups they belong to, and then destroy them.
		DestroyControl( pPositionControl );
		DestroyControl( pOrientationControl );
	}
}


void DestroyControl( CDmElement *pControl )
{	
	const static CUtlSymbolLarge symControls = g_pDataModel->GetSymbol( "controls" );
	const static CUtlSymbolLarge symElementList = g_pDataModel->GetSymbol( "elementList" );
	const static CUtlSymbolLarge symDisplaySetElements = g_pDataModel->GetSymbol( "displaySetElements" );

	if ( pControl == NULL )
		return;

	// Remove the control from any animation sets and control groups it belongs to
	CUtlVector< CDmElement* > ownerList;
	FindReferringElements( ownerList, pControl, symControls );
	Assert( ownerList.Count() == 2 );

	for ( int iElement = 0; iElement < ownerList.Count(); ++iElement )
	{
		CDmElement *pElement = ownerList[ iElement ];
		if ( pElement == NULL )
			continue;

		CDmAttribute *pControlsAttr = pElement->GetAttribute( "controls", AT_ELEMENT_ARRAY );
		if ( pControlsAttr == NULL )
			continue;
		
		CDmrElementArray<> controlList( pControlsAttr );
		int nIndex = controlList.Find( pControl );
		if ( nIndex != controlList.InvalidIndex() )
		{
			controlList.Remove( nIndex );
		}		
	}

	// Remove the control from any rigs it is a part of
	CUtlVector< CDmElement * > animSetRigList;
	FindReferringElements( animSetRigList, pControl, symElementList );
	int nAimSetRigs = animSetRigList.Count();
	for ( int iAnimSetRig = 0; iAnimSetRig < nAimSetRigs; ++iAnimSetRig )
	{
		CDmElement *pElement = animSetRigList[ iAnimSetRig ];
		if ( pElement == NULL )
			continue;

		CDmAttribute *pElementsAttr = pElement->GetAttribute( "elementList", AT_ELEMENT_ARRAY );
		if ( pElementsAttr == NULL )
			continue;
		
		CDmrElementArray<> elementsList( pElementsAttr );
		int nIndex = elementsList.Find( pControl );
		if ( nIndex != elementsList.InvalidIndex() )
		{
			elementsList.Remove( nIndex );
		}
	}

	CUtlVector< CDmElement * > rigList;
	FindReferringElements( rigList, pControl, symDisplaySetElements );
	int nNumRigs = rigList.Count();
	for ( int iRig = 0; iRig < nNumRigs; ++iRig )
	{
		CDmElement *pRig = rigList[ iRig ];
		if ( pRig == NULL )
			continue;

		CDmAttribute *pDisplaySetElementsAttr = pRig->GetAttribute( "displaySetElements", AT_ELEMENT_ARRAY );
		if ( pDisplaySetElementsAttr == NULL )
			continue;
		
		CDmrElementArray<> displaySetElements( pDisplaySetElementsAttr );
		int nIndex = displaySetElements.Find( pControl );
		if ( nIndex != displaySetElements.InvalidIndex() )
		{
			displaySetElements.Remove( nIndex );
		}		
	}


	// Destroy the control even if something is holding on to it
	DestroyElement( pControl );
}


CDmElement *CollapseControlGroup( CDmElement *pControlGroup, CDmElement *pRootGroup )
{
	const static CUtlSymbolLarge symDmeControlGroup = g_pDataModel->GetSymbol( "DmeControlGroup" );
	const static CUtlSymbolLarge symChildren = g_pDataModel->GetSymbol( "children" );

	if ( pControlGroup->GetType() != symDmeControlGroup )
		return NULL;

	if ( pRootGroup && ( pRootGroup->GetType() != symDmeControlGroup ) )
		return NULL;

	// Find the parent control group of the specified control group
	CDmElement *pParentControlGroup = NULL;
	CUtlVector< CDmElement* > parentList;
	FindReferringElements( parentList, pControlGroup, symChildren );
	for ( int i = 0; i < parentList.Count(); ++i )
	{
		CDmElement *pElement = parentList[ i ];
		if ( pElement->GetType() == symDmeControlGroup )
		{
			pParentControlGroup = pElement;
			break;
		}
	}

	Assert( pParentControlGroup );

	// If no parent was found, use the supplied root group
	if ( pParentControlGroup == NULL )
	{
		pParentControlGroup = pRootGroup;
	}

	
	if ( ( pParentControlGroup != NULL ) && ( pParentControlGroup != pControlGroup ) )
	{
		// Move add the controls of the child to its parent
		CDmAttribute *pChildControlsAttr = pControlGroup->GetAttribute( "controls", AT_ELEMENT_ARRAY );
		CDmAttribute *pParentControlsAttr = pParentControlGroup->GetAttribute( "controls", AT_ELEMENT_ARRAY );
		if ( pChildControlsAttr && pParentControlsAttr )
		{
			CDmrElementArray<> childControls( pChildControlsAttr );
			CDmrElementArray<> parentControls( pParentControlsAttr );
			for ( int iControl = 0; iControl < childControls.Count(); ++iControl )
			{
				parentControls.AddToTail( childControls[ iControl ] );
			}
			childControls.RemoveAll();
		}

		// Move the children of the child to its parent
		CDmAttribute *pChildChildrenAttr = pControlGroup->GetAttribute( "children", AT_ELEMENT_ARRAY );
		CDmAttribute *pParentChildrenAttr = pParentControlGroup->GetAttribute( "children", AT_ELEMENT_ARRAY );

		
		if ( pParentChildrenAttr )
		{
			CDmrElementArray<> parentChildren( pParentChildrenAttr );
			
			// Move the children from the control group to its parent
			if ( pChildChildrenAttr )
			{
				CDmrElementArray<> childChildren( pChildChildrenAttr );
				for ( int iChild = 0; iChild < childChildren.Count(); ++iChild )
				{
					parentChildren.AddToTail( childChildren[ iChild ] );
				}
				childChildren.RemoveAll();
			}

			// Remove the control group from its parent
			int nGroupIndex = parentChildren.Find( pControlGroup );
			parentChildren.Remove( nGroupIndex );
		}
	}	


	return pParentControlGroup;
}



//-----------------------------------------------------------------------------
// Update16 -- remove settings from sfm sessions
//-----------------------------------------------------------------------------
bool Update16( CDmElement *pElement )
{
	// remove manipulatorsettings from sfm sessions
	if ( !V_strcmp( pElement->GetName(), "sessionSettings" ) )
	{
		pElement->RemoveAttribute( "manipulatorSettings" );

		CDmElement *pRenderSettings = pElement->GetValueElement< CDmElement >( "renderSettings" );
		if ( pRenderSettings )
		{
			CDmElement *pMovieSettings = pRenderSettings->GetValueElement< CDmElement >( "MovieSettings" );
			pElement->SetValue( "movieSettings", pMovieSettings ); // lower-casing first letter to match other settings
			pRenderSettings->RemoveAttribute( "MovieSettings" );

			if ( pMovieSettings )
			{
				pMovieSettings->RemoveAttribute( "shutterSpeed" );
				pMovieSettings->RemoveAttribute( "framerate" );
				pMovieSettings->RemoveAttribute( "nLayoffState" );
				pMovieSettings->RemoveAttribute( "startTime" );
				pMovieSettings->RemoveAttribute( "endTime" );
				pMovieSettings->RemoveAttribute( "renderOnlySelectedClips" );

				pMovieSettings->InitValue( "videoTarget", 2 ); // TGA
				pMovieSettings->InitValue( "audioTarget", 2 ); // WAV
				pMovieSettings->InitValue( "stereoscopic", false );
				pMovieSettings->InitValue( "stereoSingleFile", false );
				pMovieSettings->InitValue( "clearDecals", false );
				pMovieSettings->InitValue( "width", 1280 );
				pMovieSettings->InitValue( "height", 720 );
				pMovieSettings->InitValue( "filename", "" );
			}

			CDmElement *pPosterSettings = pRenderSettings->GetValueElement< CDmElement >( "PosterSettings" );
			pElement->SetValue( "posterSettings", pPosterSettings ); // lower-casing first letter to match other settings
			pRenderSettings->RemoveAttribute( "PosterSettings" );

			pRenderSettings->RemoveAttribute( "syncEngineFramerate" );
			pRenderSettings->RemoveAttribute( "showFocalPlane" );
			pRenderSettings->RemoveAttribute( "showCameraFrustum" );
			pRenderSettings->RemoveAttribute( "showViewTargets" );
			pRenderSettings->RemoveAttribute( "StereoFlip" );

			pRenderSettings->InitValue( "ambientOcclusionMode", 0 );		// Perform ambient occlusion and/or outlining
			pRenderSettings->InitValue( "showAmbientOcclusion", 0 );		// Show just AO/outlining (vs combined with normal scene rendering)
			pRenderSettings->InitValue( "drawGameRenderablesMask", 0xd8 );	// RENDER_GAME_ROPES | RENDER_GAME_BEAMS | RENDER_GAME_WORLD_BRUSHES | RENDER_GAME_WORLD_PROPS
			pRenderSettings->InitValue( "drawToolRenderablesMask", 0x0f );	// RENDER_TOOL_PUPPETS | RENDER_TOOL_EFFECTS | RENDER_TOOL_PARTICLE_SYSTEMS | RENDER_TOOL_LIGHTS
			pRenderSettings->InitValue( "toneMapScale", 1.0f );
		}
	}

	return true;
}

//-----------------------------------------------------------------------------
// Remove the "parentConstraint" attribute from all dmeDag elements and the
// "embeddedTarget" attribute from all operators.
//-----------------------------------------------------------------------------
bool IsRigConstraint( CDmElement *pElement )
{
	const static CUtlSymbolLarge rigSym[] = 
	{
		g_pDataModel->GetSymbol( "DmeRigPointConstraintOperator" ),
		g_pDataModel->GetSymbol( "DmeRigOrientConstraintOperator" ),
		g_pDataModel->GetSymbol( "DmeRigAimConstraintOperator" ),
		g_pDataModel->GetSymbol( "DmeRigRotationConstraintOperator" ),
		g_pDataModel->GetSymbol( "DmeRigParentConstraintOperator" ),
		g_pDataModel->GetSymbol( "DmeRigIKConstraintOperator" ),
	};

	static const int nNumConstraintOperators = sizeof( rigSym ) / sizeof( CUtlSymbolLarge );
	
	CUtlSymbolLarge elementSym = pElement->GetType();
	for ( int i = 0; i < nNumConstraintOperators; ++i )
	{
		if ( elementSym == rigSym[ i ] )
			return true;
	}

	return false;
}


bool Update15( CDmElement *pElement )
{
	const static CUtlSymbolLarge dmeDagSym = g_pDataModel->GetSymbol( "DmeDag" );

	if ( pElement->GetType() == dmeDagSym )
	{
		pElement->RemoveAttribute( "parentConstraint" );
	}
	else if ( IsRigConstraint( pElement ) )
	{
		pElement->RemoveAttribute( "embeddedTarget" );
	}

	return true;
}




void CollectAnimSetsAndDestroyAnimSetGroups( CDmrElementArray< CDmElement > animationSets, CDmElement *pSrcAnimSetGroup )
{
	if ( !pSrcAnimSetGroup || !animationSets.IsValid() )
		return;

	CDmrElementArray< CDmElement > srcAnimationSets( pSrcAnimSetGroup, "animationSets" );
	int nAnimationSets = srcAnimationSets.IsValid() ? srcAnimationSets.Count() : 0;
	for ( int i = 0; i < nAnimationSets; ++i )
	{
		if ( CDmElement *pAnimSet = srcAnimationSets[ i ] )
		{
			animationSets.AddToTail( pAnimSet );
		}
	}

	CDmrElementArray< CDmElement > children( pSrcAnimSetGroup, "children" );
	int nChildren = children.IsValid() ? children.Count() : 0;
	for ( int i = 0; i < nChildren; ++i )
	{
		CollectAnimSetsAndDestroyAnimSetGroups( animationSets, children[ i ] );
	}

	DestroyElement( pSrcAnimSetGroup );
}


//-----------------------------------------------------------------------------
// eliminating animationsetsgroups and moving their animationsets to a single filmclip attribute
//-----------------------------------------------------------------------------
bool Update14( CDmElement *pElement )
{
	const static CUtlSymbolLarge filmClipSym = g_pDataModel->GetSymbol( "DmeFilmClip" );
	if ( pElement->GetType() == filmClipSym )
	{
		CDmrElementArray< CDmElement > animationSets( pElement, "animationSets", true );

		CDmrElementArray< CDmElement > animSetGroups( pElement, "animationSetGroups" );
		int nAnimSetGroups = animSetGroups.IsValid() ? animSetGroups.Count() : 0;
		for ( int i = 0; i < nAnimSetGroups; ++i )
		{
			CollectAnimSetsAndDestroyAnimSetGroups( animationSets, animSetGroups[ i ] );
		}

		pElement->RemoveAttribute( "animationSetGroups" );
	}

	return true;
}

// this works because both CDmeFilmClip and CDmeAnimationSetGroup have an element array called "bookmarks" that is being replaced
void CreateBookmarkSetAndMoveBookmarks( CDmrElementArray< CDmElement > bookmarkSets, CDmElement *pSrcElement, const char *pSetName, bool bForce )
{
	CDmAttribute *pSrcBookmarks = pSrcElement->GetAttribute( "bookmarks" );
	CDmrElementArray< CDmElement > srcBookmarks( pSrcBookmarks );
	if ( bForce || ( srcBookmarks.IsValid() && srcBookmarks.Count() > 0 ) )
	{
		CDmElement *pOwner = bookmarkSets.GetAttribute()->GetOwner();

		CDmElement *pBookmarkSet = CreateElement< CDmElement >( "DmeBookmarkSet", pSetName, pOwner->GetFileId() );
		pBookmarkSet->SetParity( pOwner->GetParity() );

		bookmarkSets.AddToTail( pBookmarkSet );

		CDmAttribute *pSetBookmarks = pBookmarkSet->AddAttribute( "bookmarks", AT_ELEMENT_ARRAY );
		if ( pSrcBookmarks && pSetBookmarks )
		{
			pSetBookmarks->SetValue( pSrcBookmarks );
		}
	}
	pSrcElement->RemoveAttributeByPtr( pSrcBookmarks );
}

void MoveBookmarksFromAnimSetGroups( CDmrElementArray< CDmElement > bookmarkSets, CDmElement *pSrcAnimSetGroup )
{
	if ( !pSrcAnimSetGroup || !bookmarkSets.IsValid() )
		return;

	CreateBookmarkSetAndMoveBookmarks( bookmarkSets, pSrcAnimSetGroup, pSrcAnimSetGroup->GetName(), false );

	CDmrElementArray< CDmElement > children( pSrcAnimSetGroup, "children" );
	int nChildren = children.IsValid() ? children.Count() : 0;
	for ( int i = 0; i < nChildren; ++i )
	{
		MoveBookmarksFromAnimSetGroups( bookmarkSets, children[ i ] );
	}
}

//-----------------------------------------------------------------------------
// moving bookmarks from filmclip and animsetionsetgroup to bookmarkset
// and adding bookmarksets to filmclip to allow named sets of bookmarks,
// and in preparation for removing animationsetgroups
//-----------------------------------------------------------------------------
bool Update13( CDmElement *pElement )
{
	const static CUtlSymbolLarge filmClipSym = g_pDataModel->GetSymbol( "DmeFilmClip" );
	if ( pElement->GetType() == filmClipSym )
	{
		pElement->SetValue( "activeBookmarkSet", 0 );
		CDmrElementArray< CDmElement > bookmarkSets( pElement, "bookmarkSets", true );
		CreateBookmarkSetAndMoveBookmarks( bookmarkSets, pElement, "default set", true );

		CDmrElementArray< CDmElement > animSetGroups( pElement, "animationSetGroups" );
		int nAnimSetGroups = animSetGroups.IsValid() ? animSetGroups.Count() : 0;
		for ( int i = 0; i < nAnimSetGroups; ++i )
		{
			MoveBookmarksFromAnimSetGroups( bookmarkSets, animSetGroups[ i ] );
		}
	}

	return true;
}

//-----------------------------------------------------------------------------
// moving displaySets from animsetionsetgroup to animationset in preparation for removing animationsetgroups
//-----------------------------------------------------------------------------
bool Update12( CDmElement *pElement )
{
	const static CUtlSymbolLarge animSetSym = g_pDataModel->GetSymbol( "DmeAnimationSet" );
	const static CUtlSymbolLarge animSetGroupSym = g_pDataModel->GetSymbol( "DmeAnimationSetGroup" );
	const static CUtlSymbolLarge controlDisplaySetSym = g_pDataModel->GetSymbol( "DmeControlDisplaySet" );
	const static CUtlSymbolLarge displaySetSym = g_pDataModel->GetSymbol( "displaySet" );
	const static CUtlSymbolLarge controlsSym = g_pDataModel->GetSymbol( "controls" );

	if ( pElement->GetType() == animSetGroupSym )
	{
		CDmrElementArray<> displaySets( pElement, "displaySets" );
		int nDisplaySets = displaySets.IsValid() ? displaySets.Count() : 0;
		for ( int di = 0; di < nDisplaySets; ++di )
		{
			CDmElement *pDisplaySet = displaySets[ di ];
			if ( !pDisplaySet )
				continue;

			// gather controls referencing the displayset
			CUtlVector< CDmElement* > controls;
			FindReferringElements( controls, pDisplaySet, displaySetSym );

			// gather animationsets referencing those controls
			CUtlVector< CDmElement* > animsets;
			int nControls = controls.Count();
			for ( int ci = 0; ci < nControls; ++ci )
			{
				CDmElement *pControl = controls[ ci ];
				if ( !pControl )
					continue;

				CUtlVector< CDmElement* > controlGroupsAndAnimationSets;
				FindReferringElements( controlGroupsAndAnimationSets, pControl, controlsSym );

				int nControlGroupsAndAnimationSets = controlGroupsAndAnimationSets.Count();
				for ( int gi = 0; gi < nControlGroupsAndAnimationSets; ++gi )
				{
					CDmElement *pGroupOrAnimSet = controlGroupsAndAnimationSets[ gi ];
					if ( !pGroupOrAnimSet || pGroupOrAnimSet->GetType() != animSetSym )
						continue;

					if ( animsets.Find( pGroupOrAnimSet ) < 0 )
					{
						animsets.AddToTail( pGroupOrAnimSet );
					}
				}
			}

			// add displayset to gathered animationsets (and replace controls' displaysets with new copy if needed)
			int nAnimSets = animsets.Count();
			for ( int ai = 0; ai < nAnimSets; ++ai )
			{
				CDmElement *pAnimSet = animsets[ ai ];
				CDmElement *pNewDisplaySet = pDisplaySet;
				if ( ai > 0 )
				{
					// copy displayset
					pNewDisplaySet = pDisplaySet->Copy( TD_SHALLOW );
					pNewDisplaySet->SetParity( pElement->GetParity(), TD_SHALLOW );

					// replace displayset with copy on each control in this animationset
					CDmrElementArray<> controls( pAnimSet, "controls" );
					int nControls = controls.Count();
					for ( int ci = 0; ci < nControls; ++ci )
					{
						CDmElement *pControl = controls[ ci ];
						if ( !pControl )
							continue;

						if ( pControl->GetValueElement< CDmElement >( "displaySet" ) == pDisplaySet )
						{
							pControl->SetValue( "displaySet", pNewDisplaySet );
						}
					}
				}

				// add displayset (or copy) to animationset
				CDmrElementArray<> newDisplaySets( pAnimSet, "displaySets", true );
				newDisplaySets.AddToTail( pNewDisplaySet );
			}
		}

		// remove displaysets attribute from animationsetgroup
		pElement->RemoveAttribute( "displaySets" );
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Remove redundant attributes in animationset controls to save memory
//-----------------------------------------------------------------------------
bool Update11( CDmElement *pElement )
{
	const static CUtlSymbolLarge animSetSym = g_pDataModel->GetSymbol( "DmeAnimationSet" );

	if ( pElement->GetType() == animSetSym )
	{
		CDmrElementArray<> controls( pElement, "controls" );
		Assert( controls.IsValid() );
		if ( !controls.IsValid() )
			return false;

		int nControls = controls.Count();
		for ( int i = 0; i < nControls; ++i )
		{
			CDmElement *pControl = controls[ i ];
			if ( !pControl )
				continue;

			if ( pControl->HasAttribute( "combo" ) )
			{
				if ( pControl->GetValue< bool >( "combo" ) )
				{
					float flDefaultLeftValue  = pControl->GetValue< float >( "defaultLeftValue" );
					float flDefaultRightValue = pControl->GetValue< float >( "defaultRightValue" );
					pControl->SetValue( "defaultValue", 0.5f * ( flDefaultLeftValue + flDefaultRightValue ) );
					pControl->RemoveAttribute( "value" );
					pControl->RemoveAttribute( "channel" );
				}
				else
				{
					pControl->RemoveAttribute( "rightValue" );
					pControl->RemoveAttribute( "leftValue" );
					pControl->RemoveAttribute( "rightvaluechannel" );
					pControl->RemoveAttribute( "leftvaluechannel" );
				}
				pControl->RemoveAttribute( "combo" );
				pControl->RemoveAttribute( "defaultLeftValue" );
				pControl->RemoveAttribute( "defaultRightValue" );
			}
			else
			{
				if ( !pControl->GetValue< bool >( "isPosition" ) )
				{
					pControl->RemoveAttribute( "position" );
					pControl->RemoveAttribute( "valuePosition" );
				}
				if ( !pControl->GetValue< bool >( "isOrientation" ) )
				{
					pControl->RemoveAttribute( "orientation" );
					pControl->RemoveAttribute( "valueOrientation" );
				}
				pControl->RemoveAttribute( "transform" );
				pControl->RemoveAttribute( "isPosition" );
				pControl->RemoveAttribute( "isOrientation" );
			}
		}
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Create a control group with the specified name and add it to the
// provided parent if the parent is a DmeControlGroup. 
//-----------------------------------------------------------------------------
CDmElement *CreateControlGroup( const char *pchName, CDmElement *pParent )
{
	const static CUtlSymbolLarge controlGroupSym = g_pDataModel->GetSymbol( "DmeControlGroup" );

	CDmElement *pGroup = CreateElement< CDmElement >( "DmeControlGroup", pchName, pParent->GetFileId() );
	pGroup->SetParity( pParent->GetParity() );

	pGroup->AddAttribute( "children",		AT_ELEMENT_ARRAY );
	pGroup->AddAttribute( "controls",		AT_ELEMENT_ARRAY );
	pGroup->SetValue< Color >( "groupColor", Color( 200, 200, 200, 255 ) );
	pGroup->SetValue< Color >( "controlColor", Color( 200, 200, 200, 255 ) );

	if ( pParent->GetType() == controlGroupSym )
	{
		CDmAttribute *pChildrenAttr = pParent->GetAttribute( "children", AT_ELEMENT_ARRAY );
		CDmrElementArray<> childList( pChildrenAttr );
		childList.AddToTail( pGroup );
	}

	return pGroup;
}

#define MULTI_CONTROL_FORMAT_STRING "multi_%s"

bool ConvertControlGroup_R( CDmrElementArray<> &animSetControls, CDmElement *pControlGroup )
{
	if ( !pControlGroup )
		return true;

	CDmrElementArray<> children( pControlGroup, "children" );
	int nChildren = children.Count();
	for ( int i = 0; i < nChildren; ++i )
	{
		ConvertControlGroup_R( animSetControls, children[ i ] );
	}

	CDmrElementArray<> controls( pControlGroup, "controls" );
	Assert( controls.IsValid() );
	if ( !controls.IsValid() )
		return true;

	int nControls = controls.Count();
	for ( int i = 0; i < nControls; ++i )
	{
		CDmElement *pControl = controls[ i ];
		if ( !pControl )
			continue;

		if ( pControl->GetValue< bool >( "multi" ) )
		{
			CDmElement *pMultiChannel = pControl->GetValueElement< CDmElement >( "multilevelchannel" );
			Assert( pMultiChannel );
			if ( pMultiChannel )
			{
				// create new multi control
				char multiName[ 256 ];
				V_snprintf( multiName, sizeof( multiName ), MULTI_CONTROL_FORMAT_STRING, pControl->GetName() );
				CDmElement *pMultiControl = CreateElement< CDmElement >( "DmElement", multiName, pControl->GetFileId() );
				pMultiControl->SetParity( pControlGroup->GetParity() );
				pMultiControl->SetValue( "value",			pControl->GetValue< float >( "multilevel" ) );
				pMultiControl->SetValue( "defaultValue",	pControl->GetValue< float >( "defaultMultilevel" ) );
				pMultiControl->SetValue( "channel",		pMultiChannel );

				// add multi control to animset's flat control list
				int j = animSetControls.Find( pControl );
				if ( j < 0 )
					return false;

				// WARNING: this isn't a great practice, since it's technically possible that we're iterating controls,
				// but some control's channel would have to reference a control group (with multi controls) for that to be true
				animSetControls.Remove( j );
				animSetControls.InsertBefore( 0, pControl );
				animSetControls.InsertBefore( 1, pMultiControl );

				// update channel to point to new element
				pMultiChannel->SetValue( "fromElement", pMultiControl );
				pMultiChannel->SetValue( "fromAttribute", "value" );

				// create control group to contain both controls and add it to the parent group
				CDmElement *pNewControlGroup = CreateControlGroup( pControl->GetName(), pControlGroup );

				// add original and multi control to new control group
				CDmrElementArray<> newControls( pNewControlGroup, "controls" );
				if ( !newControls.IsValid() )
					return false;

				newControls.AddToTail( pControl );
				newControls.AddToTail( pMultiControl );

				// remove original control from its old parent group
				controls.Remove( i );
				--i; --nControls;
			}
		}

		// remove multi-related attributes from original control
		pControl->RemoveAttribute( "multi" );
		pControl->RemoveAttribute( "multilevel" );
		pControl->RemoveAttribute( "defaultMultilevel" );
		pControl->RemoveAttribute( "multilevelchannel" );
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Convert multi controls into a control group and two controls
//-----------------------------------------------------------------------------
bool Update10( CDmElement *pElement )
{
	const static CUtlSymbolLarge animSetSym = g_pDataModel->GetSymbol( "DmeAnimationSet" );
	const static CUtlSymbolLarge presetSym = g_pDataModel->GetSymbol( "DmePreset" );

	if ( pElement->GetType() == animSetSym )
	{
		CDmrElementArray<> controls( pElement, "controls" );
		Assert( controls.IsValid() );
		if ( !controls.IsValid() )
			return false;

		ConvertControlGroup_R( controls, pElement->GetValueElement< CDmElement >( "rootControlGroup" ) );
	}
	else if ( pElement->GetType() == presetSym )
	{
		CDmrElementArray<> controlValues( pElement, "controlValues" );
		Assert( controlValues.IsValid() );
		if ( !controlValues.IsValid() )
			return true;

		int nControls = controlValues.Count();
		for ( int ci = 0; ci < nControls; ++ci )
		{
			CDmElement *pControlValue = controlValues[ ci ];
			if ( !pControlValue )
				continue;

			CDmAttribute *pAttr = pControlValue->GetAttribute( "multilevel", AT_FLOAT );
			if ( !pAttr )
				continue;

			// create new multi control value
			char multiName[ 256 ];
			V_snprintf( multiName, sizeof( multiName ), MULTI_CONTROL_FORMAT_STRING, pControlValue->GetName() );
			CDmElement *pMultiControlValue = CreateElement< CDmElement >( "DmElement", multiName, pControlValue->GetFileId() );
			pMultiControlValue->SetParity( pElement->GetParity() );
			pMultiControlValue->SetValue( "value", pAttr->GetValue< float >() );
			controlValues.InsertBefore( ci + 1, pMultiControlValue );
			++ci; ++nControls;

			// remove multilevel attribute from original control
			pControlValue->RemoveAttributeByPtr( pAttr );
		}
	}

	return true;
}

//-------------------------------------------------------------------------------------------------
// Update9 -- AnimationSet DmElement to DmeControlGroup conversion
//-------------------------------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// Purpose: Find the control in the animation set with the specified name
//-----------------------------------------------------------------------------
CDmElement *FindAnimSetControl( CDmElement* pAnimSet, const char *pchControlName )
{
	CDmAttribute *pControlsAttr = pAnimSet->GetAttribute( "controls" );
	if ( pControlsAttr == NULL )
		return NULL;

	CDmrElementArray<> controls( pControlsAttr );
	int nControls = controls.Count();
	for ( int iControl = 0; iControl < nControls; ++iControl )
	{
		CDmElement *pElement = controls[ iControl ];
		if ( pElement )
		{
			if ( V_stricmp( pchControlName, pElement->GetName() ) == 0 )
			{
				return pElement;
			}
		}
	}

	return NULL;
}


//-----------------------------------------------------------------------------
// Purpose: Recursively convert the generic DmElement selection group and its
// children into a DmeControlGroup
//-----------------------------------------------------------------------------
void ConvertSelectionGroup_R( CDmElement* pAnimSet, CDmElement *pParentControlGroup, CDmElement *pSelectionElement )
{
	if ( ( pAnimSet == NULL ) || ( pParentControlGroup == NULL ) || ( pSelectionElement == NULL ) )
		return;

	bool isGroup = pSelectionElement->GetValue< bool >( "isGroup" );
	if ( isGroup )
	{
		// Create a control group and add it to the parent group.
		CDmElement *pControlGroup = CreateControlGroup( pSelectionElement->GetName(), pParentControlGroup );
		if ( pControlGroup )
		{
			// Copy the tree color of the element to the group color of the control group
			if ( pSelectionElement->HasAttribute( "treeColor", AT_COLOR ) )
			{
				pControlGroup->SetValue< Color >( "groupColor", pSelectionElement->GetValue< Color >( "treeColor" ) );
			}

			// Process the children of the selection element group
			CDmAttribute *pSelectionControlsAttr = pSelectionElement->GetAttribute( "controls" );
			if ( pSelectionControlsAttr )
			{	
				CDmrElementArray<> children( pSelectionControlsAttr );
				int nChildren = children.Count();
				for ( int iChild = 0; iChild < nChildren; ++iChild )
				{
					CDmElement *pChild = children[ iChild ];
					ConvertSelectionGroup_R( pAnimSet, pControlGroup, pChild );
				}
			}
		}
	}
	else
	{
		// If the selection element is not a group, it is a control selection element which shares 
		// its name with the control it targets. Find that control and add it to the parent group.
		CDmElement *pControl = FindAnimSetControl( pAnimSet, pSelectionElement->GetName() );
		if ( pControl )
		{
			CDmAttribute *pControlsAttr = pParentControlGroup->GetAttribute( "controls" );
			if ( pControlsAttr )
			{
				CDmrElementArray<> controls( pControlsAttr );
				controls.AddToTail( pControl );
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Convert the animation set selection group DmElements into
// DmeControlGroups. Removes "selectionGroups" and creates "rootControlGroup"
//-----------------------------------------------------------------------------
bool Update9( CDmElement *pElement )
{
	const static CUtlSymbolLarge animSetSym = g_pDataModel->GetSymbol( "DmeAnimationSet" );

	if ( pElement->GetType() == animSetSym )
	{
		CDmElement* pAnimSet = pElement;

		// Create the root selection group
		CDmElement *pRootGroup = CreateControlGroup( "rootGroup", pAnimSet );
		pAnimSet->SetValue( "rootControlGroup", pRootGroup );

		// Iterate through the selection DmElement groups and recursively 
		// convert them to them to the DmeControlGroup format.
		CDmAttribute *pSelectionGroupsAttr = pAnimSet->GetAttribute( "selectionGroups", AT_ELEMENT_ARRAY );
		if ( pSelectionGroupsAttr )
		{
			CDmrElementArray<> controls( pSelectionGroupsAttr );
			int nControls = controls.Count();
			for ( int ci = 0; ci < nControls; ++ci )
			{
				CDmElement *pSelectionElement = controls[ ci ];
				if ( !pSelectionElement )
					continue;

				// Convert the generic DmElement group into a DmeControlGroup
				ConvertSelectionGroup_R( pAnimSet, pRootGroup, pSelectionElement );

				// Destroy the generic DmElement group
				DestroyElement( pSelectionElement );
			}
		}

		// Remove the selection groups list
		pAnimSet->RemoveAttributeByPtr( pSelectionGroupsAttr );
	}

	return true;
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// Purpose: Change DmeModel.jointTransforms to DmeModel.jointList
//-----------------------------------------------------------------------------
bool Update8( CDmElement *pElement )
{
	const static CUtlSymbolLarge modelSetSym = g_pDataModel->GetSymbol( "DmeModel" );

	CUtlSymbolLarge typeSym = pElement->GetType();

	if ( typeSym == modelSetSym )
	{
		CDmAttribute *pJointTransformsAttr = pElement->GetAttribute( "jointTransforms", AT_ELEMENT_ARRAY );
		if ( pJointTransformsAttr )
		{
			CDmAttribute *pJointListAttr = pElement->GetAttribute( "jointList", AT_ELEMENT_ARRAY );
			if ( !pJointListAttr )
			{
				pJointListAttr = pElement->AddAttribute( "jointList", AT_ELEMENT_ARRAY );
			}

			if ( pJointListAttr )
			{
				const static CUtlSymbolLarge transformSym = g_pDataModel->GetSymbol( "transform" );
				const static CUtlSymbolLarge dmeJointSym = g_pDataModel->GetSymbol( "DmeJoint" );
				const static CUtlSymbolLarge dmeDagSym = g_pDataModel->GetSymbol( "DmeDag" );

				CUtlVector< CDmElement * > refList;

				CDmrElementArray<> jt( pJointTransformsAttr );
				CDmrElementArray<> jl( pJointListAttr );

				jl.RemoveAll();
				const int nJointCount = jt.Count();
				for ( int ji = 0; ji < nJointCount; ++ji )
				{
					refList.RemoveAll();
					if ( FindReferringElements( refList, jt[ ji ], transformSym ) )
					{
						const int nRefCount = refList.Count();
						for ( int ri = 0; ri < nRefCount; ++ri )
						{
							typeSym = refList[ ri ]->GetType();
							if ( typeSym == dmeJointSym || typeSym == dmeDagSym )
							{
								jl.AddToTail( refList[ ri ] );
								break;
							}
						}
					}
				}

				pElement->RemoveAttribute( "jointTransforms" );
			}
		}
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Convert base control elements with the "transform" flag set into 
// CDmeTransformControl elements and remove the obsolete attributes from the
// element which are now part of the CDmeTransfrom control class.
//-----------------------------------------------------------------------------
bool Update7( CDmElement *pElement )
{
	const static CUtlSymbolLarge animSetSym = g_pDataModel->GetSymbol( "DmeAnimationSet" );

	CUtlSymbolLarge typeSym = pElement->GetType();

	if ( typeSym == animSetSym )
	{
		CDmAttribute *pControlsAttr = pElement->GetAttribute( "controls", AT_ELEMENT_ARRAY );
		if ( pControlsAttr )
		{
			CDmrElementArray<> controls( pControlsAttr );
			int nControls = controls.Count();
			for ( int ci = 0; ci < nControls; ++ci )
			{
				CDmElement *pControl = controls[ ci ];
				if ( !pControl )
					continue;

				if ( pControl->GetValue<bool>( "transform", false ) )
				{
					// Remove the obsolete attributes that are now part of the transform control class
					pControl->RemoveAttribute( "valueTransform" );
					pControl->RemoveAttribute( "valueDeltaRotation" );
					pControl->RemoveAttribute( "valuePivot" );
					pControl->RemoveAttribute( "pivotOffset" );
					pControl->RemoveAttribute( "refOrientation" );

					// Change the element type to specify a CDmeTransformControl
					pControl->SetType( "DmeTransformControl" );
				}
			}
		}
	}

	return true;
}

bool Update6( CDmElement *pElement )
{
	CUtlSymbolLarge typeSym = pElement->GetType();

	const static CUtlSymbolLarge grainFXClipSym = g_pDataModel->GetSymbol( "DmeHSLGrainFXClip" );
	if ( typeSym == grainFXClipSym )
	{
		pElement->RemoveAttribute( "scale" );
		pElement->RemoveAttribute( "left" );
		pElement->RemoveAttribute( "top" );
		pElement->RemoveAttribute( "width" );
		pElement->RemoveAttribute( "height" );
		return true;
	}

	const static CUtlSymbolLarge dirLightSym = g_pDataModel->GetSymbol( "DmeDirectionalLight" );
	const static CUtlSymbolLarge pointLightSym = g_pDataModel->GetSymbol( "DmePointLight" );
	const static CUtlSymbolLarge spotLightSym = g_pDataModel->GetSymbol( "DmeSpotLight" );
	const static CUtlSymbolLarge projLightSym = g_pDataModel->GetSymbol( "DmeProjectedLight" );
	if ( typeSym == dirLightSym || typeSym == pointLightSym || typeSym == spotLightSym || typeSym == projLightSym )
	{
		Vector pos = pElement->GetValue< Vector >( "position" );
		Vector dir = pElement->GetValue< Vector >( "direction" );
		Quaternion quat = pElement->GetValue< Quaternion >( "orientation" );
		if ( dir != vec3_origin )
		{
			// HACK - there's some question here as to whether to use VectorAngles, which assumes forward is 1,0,0,
			// or to compute the 0,0,1->dir quaternion manually, but since dirlights aren't used anyways, I'm not worrying about it
			QAngle angles;
			VectorAngles( dir, angles );
			AngleQuaternion( angles, quat );
		}

		CDmElement *pTransform = pElement->GetValueElement< CDmElement >( "transform" );
		if ( !pTransform )
		{
			pTransform = CreateElement< CDmElement >( "DmeTransform", "transform", pElement->GetFileId() );
			pTransform->SetParity( pElement->GetParity() );
			pElement->SetValue( "transform", pTransform );
		}

		Vector parentPos = pTransform->GetValue< Vector >( "position" );
		Quaternion parentQuat = pTransform->GetValue< Quaternion >( "orientation" );

		Vector finalPos;
		Quaternion finalQuat;
		ConcatTransforms( parentQuat, parentPos, quat, pos, finalQuat, finalPos );

		pTransform->SetValue( "position", finalPos );
		pTransform->SetValue( "orientation", finalQuat );

		// NOTE - not bothering to convert logs on transform, since up until this change, updating the light's transform didn't do anything

		CDmElement *pPosChannel = FindChannelTargettingElement( pElement, "position", false );
		CDmElement *pPosLog = pPosChannel ? GetLogLayerFromChannel( pPosChannel ) : NULL;
		if ( pPosLog )
		{
			Quaternion qDummy;
			Vector temp;
			CDmAttribute *pAttr = pPosLog->GetAttribute( "values", AT_VECTOR3_ARRAY );
			CDmrArray< Vector > values( pAttr );
			int nValues = values.Count();
			for ( int i = 0; i < nValues; ++i )
			{
				ConcatTransforms( parentQuat, parentPos, quat_identity, values[ i ], qDummy, temp );
				values.Set( i, temp );
			}
			pPosChannel->SetValue( "toElement", pTransform );
		}

		CDmElement *pRotChannel = FindChannelTargettingElement( pElement, "orientation", false );
		CDmElement *pRotLog = pRotChannel ? GetLogLayerFromChannel( pRotChannel ) : NULL;
		if ( pRotLog )
		{
			Quaternion temp;
			Vector vDummy;
			CDmAttribute *pAttr = pRotLog->GetAttribute( "values", AT_QUATERNION_ARRAY );
			CDmrArray< Quaternion > values( pAttr );
			int nValues = values.Count();
			for ( int i = 0; i < nValues; ++i )
			{
				ConcatTransforms( parentQuat, parentPos, values[ i ], vec3_origin, temp, vDummy );
				values.Set( i, temp );
			}
			pRotChannel->SetValue( "toElement", pTransform );
		}

		pElement->RemoveAttribute( "position" );
		pElement->RemoveAttribute( "direction" );
		pElement->RemoveAttribute( "orientation" );
		return true;
	}

	return true;
}


bool Update4( CDmElement *pElement )
{
	const static CUtlSymbolLarge filmClipSym = g_pDataModel->GetSymbol( "DmeFilmClip" );
	const static CUtlSymbolLarge presetGroupSym = g_pDataModel->GetSymbol( "DmePresetGroup" );

	CUtlSymbolLarge typeSym = pElement->GetType();
	if ( typeSym == presetGroupSym )
	{
		CDmElement *pPresetRemap = pElement->GetValueElement< CDmElement >( "presetRemap" );
		if ( pPresetRemap )
		{
			DestroyElement( pPresetRemap, TD_NONE );
		}
		pElement->RemoveAttribute( "presetRemap" );
	}

	if ( typeSym != filmClipSym )
		return true;

	CDmAttribute *pClipAnimSetsAttr = pElement->GetAttribute( "animationSets", AT_ELEMENT_ARRAY );
	Assert( pClipAnimSetsAttr );
	if ( !pClipAnimSetsAttr )
		return true;

	CDmrElementArray<> clipAnimSets( pClipAnimSetsAttr );

	CDmAttribute *pAnimSetGroupsAttr = pElement->AddAttribute( "animationSetGroups", AT_ELEMENT_ARRAY );
	Assert( pAnimSetGroupsAttr );
	if ( !pAnimSetGroupsAttr )
		return true;

	CDmrElementArray<> animSetGroups( pAnimSetGroupsAttr );

	int nClipAnimSets = clipAnimSets.Count();
	for ( int i = 0; i < nClipAnimSets; ++i )
	{
		CDmElement *pAnimSet = clipAnimSets[ i ];
		if ( !pAnimSet )
		{
			animSetGroups.AddToTail( NULL );
			continue;
		}

		CDmElement *pAnimSetGroup = CreateElement< CDmElement >( "DmeAnimationSetGroup", pAnimSet->GetName(), pAnimSet->GetFileId() );
		pAnimSetGroup->SetParity( pElement->GetParity() );
		animSetGroups.AddToTail( pAnimSetGroup );

		CDmAttribute *pBookmarksAttr = pAnimSetGroup->AddAttribute( "bookmarks",	 AT_ELEMENT_ARRAY );
		CDmAttribute *pAnimSetsAttr  = pAnimSetGroup->AddAttribute( "animationSets", AT_ELEMENT_ARRAY );
		/*CDmAttribute *pChildrenAttr*/pAnimSetGroup->AddAttribute( "children",		 AT_ELEMENT_ARRAY );

		CDmrElementArray<> animSets( pAnimSetsAttr );
		animSets.AddToTail( pAnimSet );

		CDmAttribute *pClipBookmarksAttr = pElement->GetAttribute( "bookmarks", AT_ELEMENT_ARRAY );
		Assert( pClipBookmarksAttr );
		if ( pClipBookmarksAttr )
		{
			CDmrElementArray<> clipBookmarks( pClipBookmarksAttr );
			CDmrElementArray<> bookmarks( pBookmarksAttr );

			int nBookmarks = clipBookmarks.Count();
			for ( int bi = 0; bi < nBookmarks; ++bi )
			{
				bookmarks.AddToTail( clipBookmarks[ bi ] );
			}
		}

		pAnimSet->RemoveAttribute( "bookmarks" );
	}

	pElement->RemoveAttribute( "animationSets" );

	return true;
}

inline void ValueBalanceToLeftRight( float *pLeft, float *pRight, float flValue, float flBalance, float flDefaultValue )
{
	*pLeft = ( flBalance <= 0.5f ) ? flValue : ( ( 1.0f - flBalance ) / 0.5f ) * ( flValue - flDefaultValue ) + flDefaultValue;
	*pRight = ( flBalance >= 0.5f ) ? flValue : ( flBalance / 0.5f ) * ( flValue - flDefaultValue ) + flDefaultValue;
}

bool ConvertIntArrayToTimeArray( CDmAttribute *pAttribute )
{
	if ( !pAttribute )
		return false;

	DmAttributeType_t type = pAttribute->GetType();
	if ( type == AT_TIME_ARRAY )
		return true;

	Assert( type == AT_INT_ARRAY );
	if ( type != AT_INT_ARRAY )
		return false;

	pAttribute->ChangeType_UNSAFE( AT_TIME_ARRAY );

	Assert( pAttribute->GetType() == AT_TIME_ARRAY );
	if ( pAttribute->GetType() != AT_TIME_ARRAY )
		return false;

	return true;
}

float GetDefaultControlValue( const char *pControlName )
{
	// HACK - this is nasty, (and not 100% correct - the engineer's BrowInV is 1-way control) but it's either that or manually fix every preset file
	// fortunately, even if this isn't perfect, it's almost undetectable
	// we had to search the movies to find a spot where 0.0f wasn't a good enough default for every control, and the difference wasn't a big deal
	const static char *s_2WayControls[] = 
	{
		"TongueWidth",
		"TongueV",
		"TongueH",
		"TongueD",
		"TongueCurl",
		"ScalpD",
		"NostrilFlare",
		"NoseV",
		"LipUpV",
		"LipsV",
		"LipLoV",
		"JawV",
		"JawH",
		"JawD",
		"FoldLipUp",
		"FoldLipLo",
		"CheekH",
		"BrowInV",
		"CloseLid", // this is actually a multi (3-way) control, so it's defaultValue *should* be 0, but due to dmxedit's trickery it's (intentionally???) 0.5
	};

	for ( int i = 0; i < ARRAYSIZE( s_2WayControls ); ++i )
	{
		if ( !V_strcmp( pControlName, s_2WayControls[i] ) )
			return 0.5f;
	}
	return 0.0f;
}

bool UpdatePreset( CDmElement *pPreset )
{
	if ( !pPreset )
		return true;

	CDmAttribute *pControlValuesAttr = pPreset->GetAttribute( "controlValues", AT_ELEMENT_ARRAY );
	if ( !pControlValuesAttr )
		return true;

	const char *pPresetName = pPreset->GetName();
	bool bFixupBalance = !V_stricmp( pPresetName, "zero" ) || !V_stricmp( pPresetName, "one" );

	CDmrElementArray<> controlValues( pControlValuesAttr );
	int nControlValues = controlValues.Count();
	for ( int vi = 0; vi < nControlValues; ++vi )
	{
		CDmElement *pControlValue = controlValues[ vi ];
		if ( !pControlValue )
			return true;

		if ( pControlValue->HasAttribute( "balance" ) )
		{
			float flDefaultValue = GetDefaultControlValue( pControlValue->GetName() );

			float flValue   = pControlValue->GetValue< float >( "value" );
			float flBalance = pControlValue->GetValue< float >( "balance" );
			pControlValue->RemoveAttribute( "value" );
			pControlValue->RemoveAttribute( "balance" );

			if ( bFixupBalance )
			{
				flBalance = 0.5f;
			}

			float flLeft, flRight;
			ValueBalanceToLeftRight( &flLeft, &flRight, flValue, flBalance, flDefaultValue );

			pControlValue->SetValue( "leftValue",  flLeft );
			pControlValue->SetValue( "rightValue", flRight );
		}
	}

	return true;
}

bool Update3( CDmElement *pElement )
{
	const static CUtlSymbolLarge animSetSym			= g_pDataModel->GetSymbol( "DmeAnimationSet" );
	const static CUtlSymbolLarge presetSym			= g_pDataModel->GetSymbol( "DmePreset" );
	const static CUtlSymbolLarge comboOpSym			= g_pDataModel->GetSymbol( "DmeCombinationOperator" );

	const static CUtlSymbolLarge logLayerSym		= g_pDataModel->GetSymbol( "DmeLogLayer" );
	const static CUtlSymbolLarge intLogLayerSym		= g_pDataModel->GetSymbol( "DmeIntLogLayer" );
	const static CUtlSymbolLarge floatLogLayerSym	= g_pDataModel->GetSymbol( "DmeFloatLogLayer" );
	const static CUtlSymbolLarge boolLogLayerSym	= g_pDataModel->GetSymbol( "DmeBoolLogLayer" );
	const static CUtlSymbolLarge colorLogLayerSym	= g_pDataModel->GetSymbol( "DmeColorLogLayer" );
	const static CUtlSymbolLarge vec2LogLayerSym	= g_pDataModel->GetSymbol( "DmeVector2LogLayer" );
	const static CUtlSymbolLarge vec3LogLayerSym	= g_pDataModel->GetSymbol( "DmeVector3LogLayer" );
	const static CUtlSymbolLarge vec4LogLayerSym	= g_pDataModel->GetSymbol( "DmeVector4LogLayer" );
	const static CUtlSymbolLarge qangleLogLayerSym	= g_pDataModel->GetSymbol( "DmeQAngleLogLayer" );
	const static CUtlSymbolLarge quatLogLayerSym	= g_pDataModel->GetSymbol( "DmeQuaternionLogLayer" );
	const static CUtlSymbolLarge vmatrixLogLayerSym	= g_pDataModel->GetSymbol( "DmeVMatrixLogLayer" );
	const static CUtlSymbolLarge stringLogLayerSym	= g_pDataModel->GetSymbol( "DmeStringLogLayer" );
	const static CUtlSymbolLarge timeLogLayerSym	= g_pDataModel->GetSymbol( "DmeTimeLogLayer" );

	CUtlSymbolLarge typeSym = pElement->GetType();
	if ( typeSym == logLayerSym
		|| typeSym == intLogLayerSym
		|| typeSym == floatLogLayerSym
		|| typeSym == boolLogLayerSym
		|| typeSym == colorLogLayerSym
		|| typeSym == vec2LogLayerSym
		|| typeSym == vec3LogLayerSym
		|| typeSym == vec4LogLayerSym
		|| typeSym == qangleLogLayerSym
		|| typeSym == quatLogLayerSym
		|| typeSym == vmatrixLogLayerSym
		|| typeSym == stringLogLayerSym
		|| typeSym == timeLogLayerSym )
	{
		return ConvertIntArrayToTimeArray( pElement->GetAttribute( "times" ) );
	}

	if ( typeSym == comboOpSym )
	{
		CDmAttribute *pControlsAttr = pElement->GetAttribute( "controls", AT_ELEMENT_ARRAY );
		CDmAttribute *pControlValuesAttr = pElement->GetAttribute( "controlValues", AT_VECTOR3_ARRAY );
		CDmAttribute *pControlValuesLaggedAttr = pElement->GetAttribute( "controlValuesLagged", AT_VECTOR3_ARRAY );
		if ( !pControlsAttr || !pControlValuesAttr || !pControlValuesLaggedAttr )
			return false;

		CDmrElementArray<> controls( pControlsAttr );
		CDmrArray< Vector > controlValues( pControlValuesAttr );
		CDmrArray< Vector > controlValuesLagged( pControlValuesLaggedAttr );

		int nControls = controls.Count();
		for ( int i = 0; i < nControls; ++i )
		{
			CDmElement *pControl = controls[ i ];
			if ( !pControl || !pControl->GetValue<bool>( "stereo" ) )
				continue;

			CDmAttribute *pRawControlNames = pControl->GetAttribute( "rawControlNames", AT_STRING_ARRAY );
			if ( !pRawControlNames )
				continue;

			CDmrStringArray	rawControlNames( pRawControlNames );
			float flDefaultValue = rawControlNames.Count() == 2 ? 0.5f : 0.0f;

			float flLeftValue, flRightValue;

			Vector cv = controlValues[ i ];
			ValueBalanceToLeftRight( &flLeftValue, &flRightValue, cv.x, cv.y, flDefaultValue );
			controlValues.Set( i, Vector( flLeftValue, flRightValue, cv.z ) );

			Vector cvl = controlValuesLagged[ i ];
			ValueBalanceToLeftRight( &flLeftValue, &flRightValue, cvl.x, cvl.y, flDefaultValue );
			controlValuesLagged.Set( i, Vector( flLeftValue, flRightValue, cvl.z ) );
		}

		return true;
	}

	if ( typeSym == presetSym )
	{
		return UpdatePreset( pElement );
	}

	if ( typeSym == animSetSym )
	{
		CDmElement *pFilmClip = FindReferringElement< CDmElement >( pElement, "animationSets" );
		if ( !pFilmClip )
			return true;

		// remove multi from controls, and move multilevel, defaultMultilevel and multilevelChannel to new _alt control
		// split preset multilevel controlValues into new controlValue
		// put new _alt control into same selection group as original

		// convert stereo controls from value/balance to left/right
		// - convert old VB into new LR, remove VB attributes
		// - convert and copy old VB log data into LR log data, and delete VB channels and logs
		CDmAttribute *pControlsAttr = pElement->GetAttribute( "controls", AT_ELEMENT_ARRAY );
		if ( pControlsAttr )
		{
			CDmrElementArray<> controls( pControlsAttr );
			int nControls = controls.Count();
			for ( int ci = 0; ci < nControls; ++ci )
			{
				CDmElement *pControl = controls[ ci ];
				if ( !pControl )
					continue;

				if ( !pControl->GetValue<bool>( "multi", false ) )
				{
					// these shouldn't be here anyways, but sometimes they are so...
					pControl->RemoveAttribute( "multilevel" );
					pControl->RemoveAttribute( "defaultMultilevel" );
					pControl->RemoveAttribute( "multilevelchannel" );
				}

				if ( !pControl->GetValue<bool>( "combo", false ) )
				{
					// these shouldn't be here anyways, but sometimes they are so...
					pControl->RemoveAttribute( "balance" );
					pControl->RemoveAttribute( "defaultBalance" );
					pControl->RemoveAttribute( "balancechannel" );
					continue;
				}

				float flValue   = pControl->GetValue<float>( "value" );
				float flBalance = pControl->GetValue<float>( "balance" );

				float flDefaultValue   = pControl->GetValue<float>( "defaultValue" );
				float flDefaultBalance = pControl->GetValue<float>( "defaultBalance" );

				CDmElement *pValueChannel   = pControl->GetValueElement< CDmElement >( "valuechannel" );
				CDmElement *pBalanceChannel = pControl->GetValueElement< CDmElement >( "balancechannel" );

				pControl->RemoveAttribute( "value" );
				pControl->RemoveAttribute( "balance" );
				pControl->RemoveAttribute( "defaultValue" );
				pControl->RemoveAttribute( "defaultBalance" );
				pControl->RemoveAttribute( "valuechannel" );
				pControl->RemoveAttribute( "balancechannel" );

				float flLeftValue, flRightValue;
				ValueBalanceToLeftRight( &flLeftValue, &flRightValue, flValue, flBalance, flDefaultValue );
				pControl->SetValue( "leftValue", flLeftValue );
				pControl->SetValue( "rightValue", flRightValue );

				float flLeftDefaultValue, flRightDefaultValue;
				ValueBalanceToLeftRight( &flLeftDefaultValue, &flRightDefaultValue, flDefaultValue, flDefaultBalance, 0.0f ); // this is kind of silly, since we expect flDefaultBalance to always be 0.5
				pControl->SetValue( "defaultLeftValue", flLeftDefaultValue );
				pControl->SetValue( "defaultRightValue", flRightDefaultValue );

				if ( !pValueChannel || !pBalanceChannel )
					continue;

				CDmElement *pChannelsClip = FindChannelsClipForChannel( pFilmClip, pValueChannel );
				Assert( pChannelsClip == FindChannelsClipForChannel( pFilmClip, pBalanceChannel ) );
				if ( !pChannelsClip )
					continue;

				CDmAttribute *pChannelsAttr = pChannelsClip->GetAttribute( "channels" );
				if ( !pChannelsAttr )
					continue;

				CDmrElementArray<> channels( pChannelsAttr );
				channels.Remove( channels.Find( pValueChannel ) );
				channels.Remove( channels.Find( pBalanceChannel ) );

				CDmElement *pVB2LRCalcOp = pValueChannel->GetValueElement< CDmElement >( "toElement" );
				Assert( pVB2LRCalcOp  == pBalanceChannel->GetValueElement< CDmElement >( "toElement" ) );

				CDmElement *pLeftChannel  = FindChannelTargettingElement( pVB2LRCalcOp, "result_left", true );
				CDmElement *pRightChannel = FindChannelTargettingElement( pVB2LRCalcOp, "result_right", true );
				if ( !pLeftChannel || !pRightChannel )
					return false;

				pControl->SetValue( "leftvaluechannel", pLeftChannel );
				pControl->SetValue( "rightvaluechannel", pRightChannel );

				int nChannelMode = pValueChannel->GetValue< int >( "mode" );
				pLeftChannel ->SetValue( "fromElement", pControl );
				pLeftChannel ->SetValue( "fromAttribute", "leftValue" );
				pLeftChannel ->SetValue( "mode", nChannelMode );
				pRightChannel->SetValue( "fromElement", pControl );
				pRightChannel->SetValue( "fromAttribute", "rightValue" );
				pRightChannel->SetValue( "mode", nChannelMode );

				CDmElement *pValueLog   = GetLogLayerFromChannel( pValueChannel );
				CDmElement *pBalanceLog = GetLogLayerFromChannel( pBalanceChannel );
				CDmElement *pLeftLog    = GetLogLayerFromChannel( pLeftChannel );
				CDmElement *pRightLog   = GetLogLayerFromChannel( pRightChannel );
				if ( !pValueLog || !pBalanceLog || !pLeftLog || !pRightLog )
					continue;

				// doing this on-demand, in case the loglayer wasn't reached directly yet
				if ( !ConvertIntArrayToTimeArray( pValueLog->GetAttribute( "times" ) ) ||
					!ConvertIntArrayToTimeArray( pBalanceLog->GetAttribute( "times" ) ) )
					return false;

				const CUtlVector< DmeTime_t > &valueTimes    = pValueLog  ->GetValue< CUtlVector< DmeTime_t > >( "times" );
				const CUtlVector< float     > &valueValues   = pValueLog  ->GetValue< CUtlVector< float     > >( "values" );
				const CUtlVector< DmeTime_t > &balanceTimes  = pBalanceLog->GetValue< CUtlVector< DmeTime_t > >( "times" );
				const CUtlVector< float     > &balanceValues = pBalanceLog->GetValue< CUtlVector< float     > >( "values" );

				CDmAttribute *pLeftTimesAttr   = pLeftLog ->GetAttribute( "times" );
				CDmAttribute *pLeftValuesAttr  = pLeftLog ->GetAttribute( "values", AT_FLOAT_ARRAY );
				CDmAttribute *pRightTimesAttr  = pRightLog->GetAttribute( "times" );
				CDmAttribute *pRightValuesAttr = pRightLog->GetAttribute( "values", AT_FLOAT_ARRAY );

				if ( !pLeftTimesAttr || !pLeftValuesAttr || !pRightTimesAttr || !pRightValuesAttr )
					return false;

				// doing this on-demand, in case the loglayer wasn't reached directly yet
				if ( !ConvertIntArrayToTimeArray( pLeftTimesAttr ) ||
					!ConvertIntArrayToTimeArray( pRightTimesAttr ) )
					return false;

				CDmrArray< DmeTime_t > leftTimes  ( pLeftTimesAttr );
				CDmrArray< float > leftValues ( pLeftValuesAttr );
				CDmrArray< DmeTime_t > rightTimes ( pRightTimesAttr );
				CDmrArray< float > rightValues( pRightValuesAttr );

				// convert and copy log data
				int vi = 0, bi = 0;
				int vn = valueTimes.Count(), bn = balanceTimes.Count();
				leftTimes  .EnsureCapacity( MAX( vn, bn ) );
				leftValues .EnsureCapacity( MAX( vn, bn ) );
				rightTimes .EnsureCapacity( MAX( vn, bn ) );
				rightValues.EnsureCapacity( MAX( vn, bn ) );
				while ( vi < vn || bi < bn )
				{
					float value, balance;
					DmeTime_t t;
					if ( vi >= vn )
					{
						t = balanceTimes[ bi ];
						value = vn > 0 ? valueValues[ vn - 1 ] : flDefaultValue;
						balance = balanceValues[ bi ];
						++bi;
					}
					else if ( bi >= bn )
					{
						t = valueTimes[ vi ];
						value = valueValues[ vi ];
						balance = bn > 0 ? balanceValues[ bn - 1 ] : flDefaultBalance;
						++vi;
					}
					else
					{
						DmeTime_t vt = valueTimes  [ vi ];
						DmeTime_t bt = balanceTimes[ bi ];
						value   = valueValues  [ vi ];
						balance = balanceValues[ bi ];
						if ( vt < bt )
						{
							t = vt;
							if ( bi > 0 )
							{
								float f = ( t - balanceTimes[ bi - 1 ] ) / ( bt - balanceTimes[ bi - 1 ] );
								balance = Lerp( f, balanceValues[ bi - 1 ], balance );
							}
							++vi;
						}
						else if ( bt < vt )
						{
							t = bt;
							if ( vi > 0 )
							{
								float f = ( t - valueTimes[ vi - 1 ] ) / ( vt - valueTimes[ vi - 1 ] );
								value = Lerp( f, valueValues[ vi - 1 ], value );
							}
							++bi;
						}
						else
						{
							t = vt;
							++vi;
							++bi;
						}
					}

					float left, right;
					ValueBalanceToLeftRight( &left, &right, value, balance, flDefaultValue );

					leftTimes .AddToTail( t );
					rightTimes.AddToTail( t );
					leftValues .AddToTail( left );
					rightValues.AddToTail( right );

				}

				// NOTE - with non-typed elements, TD_SHALLOW == TD_NONE and TD_DEEP == TD_ALL
				DestroyElement( pValueChannel, TD_NONE );
				DestroyElement( pValueLog, TD_ALL );
				DestroyElement( pBalanceChannel, TD_NONE );
				DestroyElement( pBalanceLog, TD_ALL );
			}
		}

		// remove and delete BalanceToStereoCalculatorOperators
		CDmAttribute *pOperatorsAttr = pElement->GetAttribute( "operators" );
		if ( pOperatorsAttr )
		{
			const static CUtlSymbolLarge balanceToStereoSym = g_pDataModel->GetSymbol( "DmeBalanceToStereoCalculatorOperator" );
			CDmrElementArray<> operators( pOperatorsAttr );
			int nOperators = operators.Count();
			for ( int i = nOperators-1; i >= 0; --i )
			{
				CDmElement *pOperator = operators[ i ];
				if ( pOperator && pOperator->GetType() != balanceToStereoSym )
					continue;

				DestroyElement( pOperator, TD_NONE ); // NOTE - with non-typed elements, TD_SHALLOW == TD_NONE and TD_DEEP == TD_ALL
				operators.Remove( i );
			}
		}

		// convert preset control values from VB to LR
		CDmAttribute *pPresetGroupsAttr = pElement->GetAttribute( "presetGroups", AT_ELEMENT_ARRAY );
		if ( pPresetGroupsAttr )
		{
			CDmrElementArray<> presetGroups( pPresetGroupsAttr );
			int nPresetGroups = presetGroups.Count();
			for ( int gi = 0; gi < nPresetGroups; ++gi )
			{
				CDmElement *pPresetGroup = presetGroups[ gi ];
				if ( !pPresetGroup )
					continue;

				CDmAttribute *pPresetsAttr = pPresetGroup->GetAttribute( "presets", AT_ELEMENT_ARRAY  );
				if ( !pPresetsAttr )
					continue;

				CDmrElementArray<> presets( pPresetsAttr );
				int nPresets = presets.Count();
				for ( int pi = 0; pi < nPresets; ++pi )
				{
					if ( !UpdatePreset( presets[ pi ] ) )
						return false;
				}
			}
		}
	}

	return true;
}

static CDmElement *FindAnimSetControlByName( CDmrElementArray<> &array, char const *pchControlName, int *pIndex )
{
	Assert( pIndex );
	*pIndex = array.InvalidIndex();

	for ( int i = 0; i < array.Count(); ++i )
	{
		CDmElement *e = array[ i ];
		if ( !Q_stricmp( e->GetName(), pchControlName ) )
		{
			*pIndex = i;
			return e;
		}
	}
	return NULL;
}

bool Update2( CDmElement *pElement )
{
	const char *g_pControlType[] = { "isPosition", "isOrientation" };
	const char *g_pSuffix[] = { "pos", "rot" };

	const static CUtlSymbolLarge animSetSym = g_pDataModel->GetSymbol( "DmeAnimationSet" );

	CUtlSymbolLarge typeSym = pElement->GetType();
	if ( typeSym == animSetSym )
	{
		// Split transform controls out from the groups
		// Add the isPosition/isOrientation and baseName bools/string
		// Split the actual controls for transforms

		// First find the selectionGroups attribute and the controls attribute
		CDmAttribute *pGroupsAttr = pElement->GetAttribute( "selectionGroups", AT_ELEMENT_ARRAY );
		CDmAttribute *pControlsAttr = pElement->GetAttribute( "controls", AT_ELEMENT_ARRAY  );

		if ( pGroupsAttr && pControlsAttr )
		{
			CDmrElementArray<> groups( pGroupsAttr );
			CDmrElementArray<> controls( pControlsAttr );

			// Walk through groups and for the controls which point to transforms, build the subgroups
			int c = groups.Count();
			for ( int i = 0; i < c; ++i )
			{
				CDmElement *pGroup = groups[ i ];
				CDmAttribute *temp = pGroup->GetAttribute( "selectedControls", AT_STRING_ARRAY );
				CDmrStringArray temp2( temp );

				pGroup->RenameAttribute( "selectedControls", "selectedControls2" );
				CDmAttribute *pControlsAttr = pGroup->GetAttribute( "selectedControls2", AT_STRING_ARRAY );
				// First convert all of the string array stuff into an element array
				if ( pControlsAttr )
				{
					CDmrStringArray controlsInGroup( pControlsAttr );

					CDmAttribute *pNewControlsAttr = pGroup->AddAttribute( "controls", AT_ELEMENT_ARRAY );
					CDmrElementArray< CDmElement > newControls( pNewControlsAttr );

					for ( int j = 0 ; j < controlsInGroup.Count(); ++j )
					{
						char const *pchControlName = controlsInGroup[ j ];

						CDmElement *pNewControl = CreateElement< CDmElement >( pchControlName, pElement->GetFileId() );
						pNewControl->SetParity( pElement->GetParity() );
						// Assume the new ones are not groups
						pNewControl->SetValue< bool >( "isGroup", false );
						newControls.AddToTail( pNewControl );
					}

					// Discard the old one
					pGroup->RemoveAttribute( "selectedControls2" );
				}

				pGroup->SetValue< bool >( "isGroup", true );
			}

			// In the 2nd pass, create the subgroups for the transforms and update the "controls" array accordingly
			for ( int i = 0; i < c; ++i )
			{
				CDmElement *pGroup = groups[ i ];
				CDmAttribute *pControlsAttr = pGroup->GetAttribute( "controls", AT_ELEMENT_ARRAY );
				if ( pControlsAttr )
				{
					CDmrElementArray< CDmElement > controlsInGroup( pControlsAttr );
					for ( int j = 0 ; j < controlsInGroup.Count(); ++j )
					{
						char const *pchControlName = controlsInGroup[ j ]->GetName();

						// Now that we have a control, we'll see if it corresponds to a transform
						int nIndex = controls.InvalidIndex();
						CDmElement *pControl = FindAnimSetControlByName( controls, pchControlName, &nIndex );
						if ( pControl && pControl->GetValue< bool >( "transform" ) )
						{
							controlsInGroup[ j ]->SetValue< bool >( "isGroup", true );
							CDmAttribute *pNewControlsAttr = controlsInGroup[ j ]->AddAttribute( "controls", AT_ELEMENT_ARRAY );
							CDmrElementArray< CDmElement > newControls( pNewControlsAttr );

							CUtlVector< CDmElement * > added;

							// Build the pos and rot versions
							for ( int k = 0; k < 2; ++k )
							{
								CDmElement *control = CreateElement< CDmElement >( CFmtStr( "%s - %s", pchControlName, g_pSuffix[ k ] ), pElement->GetFileId() );
								control->SetParity( pElement->GetParity() );
								control->SetValue< bool >( "transform", true );
								control->SetValue< bool >( g_pControlType[ k ], true );
								control->SetValue< bool >( g_pControlType[ 1 - k ], false );
								control->SetValue( "baseName", pchControlName );

								if ( k == 0 )
								{
									control->SetValue< Vector >( "valuePosition", pControl->GetValue< Vector >( "valuePosition" ) );

									CDmElement *p = pControl->GetValueElement< CDmElement >( "position" );
									if ( p )
									{
										control->SetValue< CDmElement >( "position", p );
										// Now find the position channel's "from" and set it to the new control
										p->SetValue< CDmElement >( "fromElement", control );
									}
								}
								else
								{
									control->SetValue< Quaternion >( "valueOrientation", pControl->GetValue< Quaternion >( "valueOrientation" ) );
									CDmElement *p = pControl->GetValueElement< CDmElement >( "orientation" );
									if ( p )
									{
										control->SetValue< CDmElement >( "orientation", p );
										// Now find the orientation channel's "from" and set it to the new control
										p->SetValue< CDmElement >( "fromElement", control );
									}
								}

								added.AddToTail( control );

								CDmElement *groupControl = CreateElement< CDmElement >( CFmtStr( "%s - %s", pchControlName, g_pSuffix[ k ] ), pElement->GetFileId() );
								groupControl->SetParity( pElement->GetParity() );
								groupControl->SetValue< bool >( "isGroup", false );
								newControls.AddToTail( groupControl );
							}

							// Remove the singular one and replace with paired ones
							controls.Remove( nIndex );

							for ( int k = 0; k < 2; ++k )
							{
								controls.AddToTail( added[ k ] );
							}
						}
					}
				}
			}
		}
	}

	return true;
}

bool Update1( CDmElement *pElement )
{
	// remove lights attribute from filmclips

	const static CUtlSymbolLarge projectedLightSym		= g_pDataModel->GetSymbol( "DmeProjectedLight" );
	const static CUtlSymbolLarge filmClipSym			= g_pDataModel->GetSymbol( "DmeFilmClip" );
	const static CUtlSymbolLarge bookmarkSym			= g_pDataModel->GetSymbol( "DmeBookmark" );
	const static CUtlSymbolLarge timeframeSym			= g_pDataModel->GetSymbol( "DmeTimeFrame" );
	const static CUtlSymbolLarge timeSelectionSym		= g_pDataModel->GetSymbol( "DmeTimeSelection" );
	const static CUtlSymbolLarge proceduralPresetSym	= g_pDataModel->GetSymbol( "DmeProceduralPresetSettings" );
	const static CUtlSymbolLarge gameParticleSysSym		= g_pDataModel->GetSymbol( "DmeGameParticleSystem" );
	const static CUtlSymbolLarge cameraSym				= g_pDataModel->GetSymbol( "DmeCamera" );

	const static CUtlSymbolLarge intCurveInfoSym		= g_pDataModel->GetSymbol( "DmeIntCurveInfo" );
	const static CUtlSymbolLarge floatCurveInfoSym		= g_pDataModel->GetSymbol( "DmeFloatCurveInfo" );
	const static CUtlSymbolLarge boolCurveInfoSym		= g_pDataModel->GetSymbol( "DmeBoolCurveInfo" );
	const static CUtlSymbolLarge colorCurveInfoSym		= g_pDataModel->GetSymbol( "DmeColorCurveInfo" );
	const static CUtlSymbolLarge vec2CurveInfoSym		= g_pDataModel->GetSymbol( "DmeVector2CurveInfo" );
	const static CUtlSymbolLarge vec3CurveInfoSym		= g_pDataModel->GetSymbol( "DmeVector3CurveInfo" );
	const static CUtlSymbolLarge vec4CurveInfoSym		= g_pDataModel->GetSymbol( "DmeVector4CurveInfo" );
	const static CUtlSymbolLarge qangleCurveInfoSym		= g_pDataModel->GetSymbol( "DmeQAngleCurveInfo" );
	const static CUtlSymbolLarge quatCurveInfoSym		= g_pDataModel->GetSymbol( "DmeQuaternionCurveInfo" );
	const static CUtlSymbolLarge vmatrixCurveInfoSym	= g_pDataModel->GetSymbol( "DmeVMatrixCurveInfo" );
	const static CUtlSymbolLarge stringCurveInfoSym		= g_pDataModel->GetSymbol( "DmeStringCurveInfo" );
	const static CUtlSymbolLarge timeCurveInfoSym		= g_pDataModel->GetSymbol( "DmeTimeCurveInfo" );

	CUtlSymbolLarge typeSym = pElement->GetType();
	if ( typeSym == projectedLightSym )
	{
		pElement->RemoveAttribute( "textureFrame" );

		ChangeAttributeType( pElement, "animationTime", AT_TIME );
	}
	else if ( typeSym == filmClipSym )
	{
		pElement->RemoveAttribute( "lights" );

		ChangeAttributeType( pElement, "fadeIn", AT_TIME );
		ChangeAttributeType( pElement, "fadeOut", AT_TIME );
	}
	else if ( typeSym == bookmarkSym )
	{
		ChangeAttributeType( pElement, "time", AT_TIME );
		ChangeAttributeType( pElement, "duration", AT_TIME );
	}
	else if ( typeSym == timeframeSym )
	{
		pElement->RenameAttribute( "startTime", "start" );
		pElement->RenameAttribute( "durationTime", "duration" );
		pElement->RenameAttribute( "offsetTime", "offset" );
		ChangeAttributeType( pElement, "start", AT_TIME );
		ChangeAttributeType( pElement, "duration", AT_TIME );
		ChangeAttributeType( pElement, "offset", AT_TIME );
	}
	else if ( typeSym == timeSelectionSym )
	{
		ChangeAttributeType( pElement, "falloff_left", AT_TIME );
		ChangeAttributeType( pElement, "falloff_right", AT_TIME );
		ChangeAttributeType( pElement, "hold_left", AT_TIME );
		ChangeAttributeType( pElement, "hold_right", AT_TIME );
		ChangeAttributeType( pElement, "resampleinterval", AT_TIME );
	}
	else if ( typeSym == proceduralPresetSym )
	{
		ChangeAttributeType( pElement, "staggerinterval", AT_TIME );
	}
	else if ( typeSym == gameParticleSysSym )
	{
		ChangeAttributeType( pElement, "simulationTime", AT_TIME );
		ChangeAttributeType( pElement, "startTime", AT_TIME );
		ChangeAttributeType( pElement, "emissionStopTime", AT_TIME );
		ChangeAttributeType( pElement, "endTime", AT_TIME );
	}
	else if ( typeSym == cameraSym )
	{
		ChangeAttributeType( pElement, "shutterSpeed", AT_TIME );
	}
	else if ( typeSym == intCurveInfoSym
		|| typeSym == floatCurveInfoSym
		|| typeSym == boolCurveInfoSym
		|| typeSym == colorCurveInfoSym
		|| typeSym == vec2CurveInfoSym
		|| typeSym == vec3CurveInfoSym
		|| typeSym == vec4CurveInfoSym
		|| typeSym == qangleCurveInfoSym
		|| typeSym == quatCurveInfoSym
		|| typeSym == vmatrixCurveInfoSym
		|| typeSym == stringCurveInfoSym
		|| typeSym == timeCurveInfoSym )
	{
		ChangeAttributeType( pElement, "rightEdgeTime", AT_TIME );
	}

	return true;
}

static char *s_RemapOperatorNameTable[]={
	"alpha_fade", "Alpha Fade and Decay",
	"alpha_fade_in_random", "Alpha Fade In Random",
	"alpha_fade_out_random", "Alpha Fade Out Random",
	"basic_movement", "Movement Basic",
	"color_fade", "Color Fade",
	"controlpoint_light", "Color Light From Control Point",
	"Dampen Movement Relative to Control Point", "Movement Dampen Relative to Control Point",
	"Distance Between Control Points Scale", "Remap Distance Between Two Control Points to Scalar",
	"Distance to Control Points Scale", "Remap Distance to Control Point to Scalar",
	"lifespan_decay", "Lifespan Decay",
	"lock to bone",	"Movement Lock to Bone",
	"postion_lock_to_controlpoint", "Movement Lock to Control Point",
	"maintain position along path", "Movement Maintain Position Along Path",
	"Match Particle Velocities", "Movement Match Particle Velocities",
	"Max Velocity", "Movement Max Velocity",
	"noise", "Noise Scalar",
	"vector noise", "Noise Vector",
	"oscillate_scalar", "Oscillate Scalar",
	"oscillate_vector", "Oscillate Vector",
	"Orient Rotation to 2D Direction", "Rotation Orient to 2D Direction",
	"radius_scale", "Radius Scale",
	"Random Cull", "Cull Random",
	"remap_scalar", "Remap Scalar",
	"rotation_movement", "Rotation Basic",
	"rotation_spin", "Rotation Spin Roll",
	"rotation_spin yaw", "Rotation Spin Yaw",
	"alpha_random", "Alpha Random",
	"color_random", "Color Random",
	"create from parent particles", "Position From Parent Particles",
	"Create In Hierarchy", "Position In CP Hierarchy",
	"random position along path", "Position Along Path Random",
	"random position on model", "Position on Model Random",
	"sequential position along path", "Position Along Path Sequential",
	"position_offset_random", "Position Modify Offset Random",
	"position_warp_random", "Position Modify Warp Random",
	"position_within_box", "Position Within Box Random",
	"position_within_sphere", "Position Within Sphere Random",
	"Inherit Velocity", "Velocity Inherit from Control Point",
	"Initial Repulsion Velocity", "Velocity Repulse from World",
	"Initial Velocity Noise", "Velocity Noise",
	"Initial Scalar Noise", "Remap Noise to Scalar",
	"Lifespan from distance to world", "Lifetime from Time to Impact",
	"Pre-Age Noise", "Lifetime Pre-Age Noise",
	"lifetime_random", "Lifetime Random",
	"radius_random", "Radius Random",
	"random yaw", "Rotation Yaw Random",
	"Randomly Flip Yaw", "Rotation Yaw Flip Random",
	"rotation_random", "Rotation Random",
	"rotation_speed_random", "Rotation Speed Random",
	"sequence_random", "Sequence Random",
	"second_sequence_random", "Sequence Two Random",
	"trail_length_random", "Trail Length Random",
	"velocity_random", "Velocity Random",
};

bool Update5( CDmElement *pElement )
{
	const static CUtlSymbolLarge ParticleOperatorSym	= g_pDataModel->GetSymbol( "DmeParticleOperator" );

	CUtlSymbolLarge typeSym = pElement->GetType();

	if ( typeSym == ParticleOperatorSym )
	{
		const char *pOpName = pElement->GetValueString( "functionName" );
		for( int i = 0 ; i < ARRAYSIZE( s_RemapOperatorNameTable ) ; i += 2 )
		{
			if ( Q_stricmp( pOpName, s_RemapOperatorNameTable[i] ) == 0 )
			{
				pElement->SetValueFromString( "functionName", s_RemapOperatorNameTable[i + 1 ] );
//				pElement->SetValueFromString( "name", s_RemapOperatorNameTable[i + 1 ] );
				pElement->SetName( s_RemapOperatorNameTable[i + 1 ] );
				break;
			}
		}
	}

	return true;
}


bool UpdateElement_R( FnUpdater pfnUpdate, CDmElement **ppElement, bool bParity )
{
	CDmElement *pElement = *ppElement;
	if ( pElement->GetParity() == bParity )
		return true; // already visited

	pElement->SetParity( bParity );

	if ( !pfnUpdate( pElement ) )
		return false;

	for ( CDmAttribute *pAttribute = pElement->FirstAttribute(); pAttribute; pAttribute = pAttribute->NextAttribute() )
	{
		if ( pAttribute->GetType() == AT_ELEMENT )
		{
			CDmElement *pElement = pAttribute->GetValueElement< CDmElement >();
			if ( pElement )
			{
				if ( !UpdateElement_R( pfnUpdate, &pElement, bParity ) )
					return false;
				pAttribute->SetValue< CDmElement >( pElement );
			}
			continue;
		}

		if ( pAttribute->GetType() == AT_ELEMENT_ARRAY )
		{
			CDmrElementArray<> array( pAttribute );
			int nCount = array.Count();
			for ( int i = 0; i < nCount; ++i )
			{
				CDmElement *pChild = array[ i ];
				if ( pChild )
				{
					if ( !UpdateElement_R( pfnUpdate, &pChild, bParity ) )
						return false;
					array.Set( i, pChild );
				}
			}
			continue;
		}
	}

	return true;
}

IMPLEMENT_UPDATER( 1 );	// Creates Update1_R, which requires an Update1 func
IMPLEMENT_UPDATER( 2 );
IMPLEMENT_UPDATER( 3 );
IMPLEMENT_UPDATER( 4 );
IMPLEMENT_UPDATER( 5 );
IMPLEMENT_UPDATER( 6 );
IMPLEMENT_UPDATER( 7 );
IMPLEMENT_UPDATER( 8 );
IMPLEMENT_UPDATER( 9 );
IMPLEMENT_UPDATER( 10 );
IMPLEMENT_UPDATER( 11 );
IMPLEMENT_UPDATER( 12 );
IMPLEMENT_UPDATER( 13 );
IMPLEMENT_UPDATER( 14 );
IMPLEMENT_UPDATER( 15 );
IMPLEMENT_UPDATER( 16 );
IMPLEMENT_UPDATER( 17 );
IMPLEMENT_UPDATER( 18 );
IMPLEMENT_UPDATER( 19 );


typedef bool (*ElementUpdateFunction)( CDmElement **pRoot, bool bParity );
ElementUpdateFunction EmptyUpdateFunctionList[] = { NULL };
ElementUpdateFunction PresetUpdateFunctionList[] = { Update3_R, Update4_R, Update10_R, NULL };
ElementUpdateFunction MovieObjectsUpdateFunctionList[] =
{
	Update1_R, Update2_R, Update3_R, Update4_R, Update6_R, Update7_R, Update8_R, Update9_R,
	Update10_R, Update11_R, Update12_R, Update13_R, Update14_R, Update15_R, Update17_R, Update18_R, Update19_R, NULL
};
ElementUpdateFunction SFMSessionObjectsUpdateFunctionList[] =
{
	Update1_R, Update2_R, Update3_R, Update4_R, Update6_R, Update7_R, Update8_R, Update9_R,
	Update10_R, Update11_R, Update12_R, Update13_R, Update14_R, Update15_R, Update16_R, Update17_R, Update18_R, Update19_R,  NULL
};
ElementUpdateFunction ParticleUpdateFunctionList[] = { Update5_R, NULL };


//-----------------------------------------------------------------------------
// declare format updaters
//-----------------------------------------------------------------------------

DECLARE_FORMAT_UPDATER( dmx,				"Generic DMX",					"dmx", "binary",	MovieObjectsUpdateFunctionList )
DECLARE_FORMAT_UPDATER( movieobjects,		"Generic MovieObjects",			"dmx", "binary",	MovieObjectsUpdateFunctionList )
DECLARE_FORMAT_UPDATER( sfm,				"Generic SFM",					"dmx", "binary",	MovieObjectsUpdateFunctionList )
DECLARE_FORMAT_UPDATER( sfm_settings,		"SFM Settings",					"dmx", "keyvalues2",MovieObjectsUpdateFunctionList )
DECLARE_FORMAT_UPDATER( sfm_session,		"SFM Session",					"dmx", "binary",	SFMSessionObjectsUpdateFunctionList )
DECLARE_FORMAT_UPDATER( sfm_trackgroup,		"SFM TrackGroup",				"dmx", "binary",	MovieObjectsUpdateFunctionList )
DECLARE_FORMAT_UPDATER( pcf,				"Particle Configuration File",	"pcf", "binary",	ParticleUpdateFunctionList )
DECLARE_FORMAT_UPDATER( preset,				"Preset File",					"dmx", "keyvalues2",PresetUpdateFunctionList )
DECLARE_FORMAT_UPDATER( facial_animation,	"Facial Animation File",		"dmx", "binary",	MovieObjectsUpdateFunctionList )
DECLARE_FORMAT_UPDATER( model,				"DMX Model",					"dmx", "binary",	MovieObjectsUpdateFunctionList )
DECLARE_FORMAT_UPDATER( ved,				"Vgui editor file",				"ved", "keyvalues2",	EmptyUpdateFunctionList )
DECLARE_FORMAT_UPDATER( mks,				"Make sheet file",				"mks", "keyvalues2",	EmptyUpdateFunctionList )
DECLARE_FORMAT_UPDATER( mp_preprocess,		"DMX Model Pipeline Preprocess File",				"mpp",				"keyvalues2",	EmptyUpdateFunctionList )
DECLARE_FORMAT_UPDATER( mp_root,			"DMX Model Pipeline Root Script File",				"root",				"keyvalues2",	EmptyUpdateFunctionList )
DECLARE_FORMAT_UPDATER( mp_model,			"DMX Model Pipeline Model Script File",				"model",			"keyvalues2",	EmptyUpdateFunctionList )
DECLARE_FORMAT_UPDATER( mp_anim,			"DMX Model Pipeline Animation Script File",			"anim",				"keyvalues2",	EmptyUpdateFunctionList )
DECLARE_FORMAT_UPDATER( mp_physics,			"DMX Model Pipeline Physics Script File",			"physics",			"keyvalues2",	EmptyUpdateFunctionList )
DECLARE_FORMAT_UPDATER( mp_hitbox,			"DMX Model Pipeline Hitbox Script File",			"hitbox",			"keyvalues2",	EmptyUpdateFunctionList )
DECLARE_FORMAT_UPDATER( mp_materialgroup,	"DMX Model Pipeline Material Group Script File",	"materialgroup",	"keyvalues2",	EmptyUpdateFunctionList )
DECLARE_FORMAT_UPDATER( mp_keyvalues,		"DMX Model Pipeline KeyValues Script File",			"keyvalues",		"keyvalues2",	EmptyUpdateFunctionList )
DECLARE_FORMAT_UPDATER( mp_eyes,			"DMX Model Pipeline Eyes Script File",				"eyes",				"keyvalues2",	EmptyUpdateFunctionList )
DECLARE_FORMAT_UPDATER( mp_bonemask,		"DMX Model Pipeline Bone Mask Script File",			"bonemask",			"keyvalues2",	EmptyUpdateFunctionList )
DECLARE_FORMAT_UPDATER( gui,				"Compiled GUI file",			"gui", "keyvalues2",EmptyUpdateFunctionList )
DECLARE_FORMAT_UPDATER( schema,				"Schema description file",		"sch", "keyvalues2",EmptyUpdateFunctionList )
DECLARE_FORMAT_UPDATER( tex,				"Texture Configuration File",	"tex", "keyvalues2",EmptyUpdateFunctionList )
DECLARE_FORMAT_UPDATER( world,				"World Files",					"wld", "binary",	EmptyUpdateFunctionList )
DECLARE_FORMAT_UPDATER( worldnode,			"World Node Files",				"wnd", "binary",	EmptyUpdateFunctionList )

//-----------------------------------------------------------------------------
// The application object
//-----------------------------------------------------------------------------
class CDmSerializers : public CBaseAppSystem< IDmSerializers >
{
	typedef CBaseAppSystem< IDmSerializers > BaseClass;

public:
	// Inherited from IAppSystem 
	virtual bool Connect( CreateInterfaceFn factory );
	virtual void *QueryInterface( const char *pInterfaceName );
	virtual InitReturnVal_t Init();
};


//-----------------------------------------------------------------------------
// Singleton interface 
//-----------------------------------------------------------------------------
static CDmSerializers g_DmSerializers;
IDmSerializers *g_pDmSerializers = &g_DmSerializers;


//-----------------------------------------------------------------------------
// Here's where the app systems get to learn about each other 
//-----------------------------------------------------------------------------
bool CDmSerializers::Connect( CreateInterfaceFn factory ) 
{
	if ( !BaseClass::Connect( factory ) )
		return false;

	if ( !factory( FILESYSTEM_INTERFACE_VERSION, NULL ) )
	{
		Warning( "DmSerializers needs the file system to function" );
		return false;
	}

	// Here's the main point where all DM element classes get installed
	// Necessary to do it here so all type symbols for all DME classes are set 
	// up prior to install
	InstallDmElementFactories( );

	return true;
}


//-----------------------------------------------------------------------------
// Here's where systems can access other interfaces implemented by this object
//-----------------------------------------------------------------------------
void *CDmSerializers::QueryInterface( const char *pInterfaceName )
{
	if ( !V_strcmp( pInterfaceName, DMSERIALIZERS_INTERFACE_VERSION ) )
		return (IDmSerializers*)this;

	return NULL;
}


//-----------------------------------------------------------------------------
// Init, shutdown
//-----------------------------------------------------------------------------
InitReturnVal_t CDmSerializers::Init() 
{ 
	InitReturnVal_t nRetVal = BaseClass::Init();
	if ( nRetVal != INIT_OK )
		return nRetVal;

	// Install non-dmx importers
	InstallActBusyImporter( g_pDataModel );
	InstallCommentaryImporter( g_pDataModel );
	InstallVMTImporter( g_pDataModel );
	InstallVMFImporter( g_pDataModel );
	InstallMKSImporter( g_pDataModel );
	InstallTEXImporter( g_pDataModel );

	// Install legacy dmx importers
	InstallSFMV1Importer( g_pDataModel );
	InstallSFMV2Importer( g_pDataModel );
	InstallSFMV3Importer( g_pDataModel );
	InstallSFMV4Importer( g_pDataModel );
	InstallSFMV5Importer( g_pDataModel );
	InstallSFMV6Importer( g_pDataModel );
	InstallSFMV7Importer( g_pDataModel );
	InstallSFMV8Importer( g_pDataModel );
	InstallSFMV9Importer( g_pDataModel );

	// install dmx format updaters
	INSTALL_FORMAT_UPDATER( dmx );
	INSTALL_FORMAT_UPDATER( movieobjects );
	INSTALL_FORMAT_UPDATER( sfm );
	INSTALL_FORMAT_UPDATER( sfm_settings );
	INSTALL_FORMAT_UPDATER( sfm_session );
	INSTALL_FORMAT_UPDATER( sfm_trackgroup );
	INSTALL_FORMAT_UPDATER( pcf );
	INSTALL_FORMAT_UPDATER( gui );
	INSTALL_FORMAT_UPDATER( schema );
	INSTALL_FORMAT_UPDATER( preset );
	INSTALL_FORMAT_UPDATER( facial_animation );
	INSTALL_FORMAT_UPDATER( model );
	INSTALL_FORMAT_UPDATER( ved );
	INSTALL_FORMAT_UPDATER( mks );
	INSTALL_FORMAT_UPDATER( mp_preprocess );
	INSTALL_FORMAT_UPDATER( mp_root );
	INSTALL_FORMAT_UPDATER( mp_model );
	INSTALL_FORMAT_UPDATER( mp_anim );
	INSTALL_FORMAT_UPDATER( mp_physics );
	INSTALL_FORMAT_UPDATER( mp_hitbox );
	INSTALL_FORMAT_UPDATER( mp_materialgroup );
	INSTALL_FORMAT_UPDATER( mp_keyvalues );
	INSTALL_FORMAT_UPDATER( mp_eyes );
	INSTALL_FORMAT_UPDATER( mp_bonemask );
	INSTALL_FORMAT_UPDATER( tex );
	INSTALL_FORMAT_UPDATER( world );
	INSTALL_FORMAT_UPDATER( worldnode );

	return INIT_OK; 
}

