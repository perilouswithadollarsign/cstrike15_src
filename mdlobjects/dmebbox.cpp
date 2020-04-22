//====== Copyright © 1996-2004, Valve Corporation, All rights reserved. =====//
//
// Dme version of an axis aligned bounding box
//
//===========================================================================//


// Valve includes
#include "datamodel/dmelementfactoryhelper.h"
#include "mdlobjects/dmebbox.h"
#include "mathlib/mathlib.h"
#include "tier2/renderutils.h"


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Expose this class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeBBox, CDmeBBox );


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDmeBBox::OnConstruction()
{
	Vector vMin;
	Vector vMax;

	ClearBounds( vMin, vMax );

	m_vMinBounds.InitAndSet( this, "minBounds", vMin );
	m_vMaxBounds.InitAndSet( this, "maxBounds", vMax );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeBBox::OnDestruction()
{
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeBBox::Clear()
{
	Vector vMin;
	Vector vMax;

	ClearBounds( vMin, vMax );

	m_vMinBounds = vMin;
	m_vMaxBounds = vMax;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CDmeBBox::Empty() const
{
	const Vector &vMin = m_vMinBounds.Get();
	const Vector &vMax = m_vMaxBounds.Get();

	return vMin.x > vMax.x || vMin.y > vMax.y || vMin.z > vMax.z;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeBBox::TransformUsing( const matrix3x4_t &mMatrix )
{
	Vector vMin;
	Vector vMax;

	TransformAABB( mMatrix, m_vMinBounds, m_vMaxBounds, vMin, vMax );

	m_vMinBounds = vMin;
	m_vMaxBounds = vMax;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeBBox::Expand( const Vector &vPoint )
{
	Vector vMin = m_vMinBounds;
	Vector vMax = m_vMaxBounds;

	AddPointToBounds( vPoint, vMin, vMax );

	m_vMinBounds = vMin;
	m_vMaxBounds = vMax;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeBBox::Expand( const CDmeBBox &bbox )
{
	Expand( bbox.m_vMinBounds );
	Expand( bbox.m_vMaxBounds );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CDmeBBox::Contains( const Vector &vPoint ) const
{
	const Vector &vMin = m_vMinBounds.Get();
	const Vector &vMax = m_vMaxBounds.Get();

	return
		vPoint.x >= vMin.x && vPoint.x <= vMax.x &&
		vPoint.y >= vMin.y && vPoint.y <= vMax.y &&
		vPoint.z >= vMin.z && vPoint.z <= vMax.z;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CDmeBBox::Intersects( const CDmeBBox &bbox ) const
{
	return QuickBoxIntersectTest( m_vMinBounds, m_vMaxBounds, bbox.m_vMinBounds, bbox.m_vMaxBounds );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
float CDmeBBox::Width() const
{
	const float flWidth = m_vMaxBounds.Get().x - m_vMinBounds.Get().x;
	return flWidth > 0.0f ? flWidth : 0.0f;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
float CDmeBBox::Height() const
{
	const float flHeight = m_vMaxBounds.Get().y - m_vMinBounds.Get().y;
	return flHeight > 0.0f ? flHeight : 0.0f;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
float CDmeBBox::Depth() const
{
	const float flDepth = m_vMaxBounds.Get().z - m_vMinBounds.Get().z;
	return flDepth > 0.0f ? flDepth : 0.0f;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
Vector CDmeBBox::Center() const
{
	const Vector &vMin = m_vMinBounds.Get();
	const Vector &vMax = m_vMaxBounds.Get();

	return Vector(
		( vMax.x + vMin.x ) / 2.0f,
		( vMax.y + vMin.y ) / 2.0f,
		( vMax.z + vMin.z ) / 2.0f );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
const Vector &CDmeBBox::Min() const
{
	return m_vMinBounds.Get();
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
const Vector &CDmeBBox::Max() const
{
	return m_vMaxBounds.Get();
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeBBox::Draw( const matrix3x4_t &shapeToWorld, CDmeDrawSettings *pDrawSettings /*= NULL */ )
{
	static const Color cRenderColor( 0, 192, 0 );
	Vector vOrigin;
	QAngle angles;
	MatrixAngles( shapeToWorld, angles, vOrigin );
	RenderBox( vOrigin, angles, m_vMinBounds, m_vMaxBounds, cRenderColor, true );
}