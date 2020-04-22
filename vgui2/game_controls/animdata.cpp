//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "animdata.h"
#include "dmxloader/dmxelement.h"
//#include "tier1/utlvector.h"


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


BEGIN_DMXELEMENT_UNPACK ( CAnimData ) 
	DMXELEMENT_UNPACK_FIELD_UTLSTRING( "name", "", m_pStateName ) 
	DMXELEMENT_UNPACK_FIELD_UTLSTRING( "animalias", "", m_pAnimAlias ) 
	DMXELEMENT_UNPACK_FIELD( "textureanimsequencesheetnumber", "0", int, m_TextureAnimSheetSeqNumber )
	DMXELEMENT_UNPACK_FIELD( "animationrate", "1", float, m_AnimationRate )  
END_DMXELEMENT_UNPACK( CAnimData, s_AnimDataUnpack )

//-----------------------------------------------------------------------------
// Constructor, Destructor
//-----------------------------------------------------------------------------
CAnimData::CAnimData( )
{
	m_pStateName = "";
	m_pAnimAlias = "";
	m_TextureAnimSheetSeqNumber = 0;
	m_AnimationRate = 1.0;
}

CAnimData::~CAnimData( )
{
}

//-----------------------------------------------------------------------------
// Populate with data from file.
//-----------------------------------------------------------------------------
bool CAnimData::Unserialize( CDmxElement *pElement )
{
	pElement->UnpackIntoStructure( this, s_AnimDataUnpack );

	CDmxAttribute *pAnimAttr = pElement->GetAttribute( "colorlog" );
	if ( pAnimAttr )
	{
		CDmxElement *pAnim = pAnimAttr->GetValue< CDmxElement * >();
		if ( !m_ColorAnim.Unserialize( pAnim ))
			return false;
	}

	pAnimAttr = pElement->GetAttribute( "centerlog" );
	if ( pAnimAttr )
	{
		CDmxElement *pAnim = pAnimAttr->GetValue< CDmxElement * >();
		if ( !m_CenterPosAnim.Unserialize( pAnim ) )
			return false;
	}

	pAnimAttr = pElement->GetAttribute( "scalelog" );
	if ( pAnimAttr )
	{
		CDmxElement *pAnim = pAnimAttr->GetValue< CDmxElement * >();
		if ( !m_ScaleAnim.Unserialize( pAnim ) )
			return false;
	}

	pAnimAttr = pElement->GetAttribute( "rotationlog" );
	if ( pAnimAttr )
	{
		CDmxElement *pAnim = pAnimAttr->GetValue< CDmxElement * >();
		if ( !m_RotationAnim.Unserialize( pAnim ) )
			return false;
	}

	pAnimAttr = pElement->GetAttribute( "fontlog" );
	if ( pAnimAttr )
	{
		CDmxElement *pAnim = pAnimAttr->GetValue< CDmxElement * >();
		if ( !m_FontAnim.Unserialize( pAnim ) )
			return false;
	}

	return true;
}


//-----------------------------------------------------------------------------
// Return true if this anim is done playing.
//-----------------------------------------------------------------------------
bool CAnimData::IsDone( DmeTime_t time )
{
	if ( m_ColorAnim.IsDone( time ) &&
		m_CenterPosAnim.IsDone( time ) &&
		m_ScaleAnim.IsDone( time ) &&
		m_RotationAnim.IsDone( time ) &&
		m_FontAnim.IsDone( time ) )
	{
		return true;
	}

	return false;
}





















