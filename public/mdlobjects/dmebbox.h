//====== Copyright © 1996-2004, Valve Corporation, All rights reserved. =====//
//
// Dme version of an axis aligned bounding box
//
//===========================================================================//

#ifndef DMEBBOX_H
#define DMEBBOX_H


#ifdef _WIN32
#pragma once
#endif


// Valve includes
#include "movieobjects/dmeshape.h"


//-----------------------------------------------------------------------------
// Forward Declarations
//-----------------------------------------------------------------------------
class CDmeDrawSettings;
struct matrix3x4_t;


//-----------------------------------------------------------------------------
// A class representing an axis aligned bounding box
//-----------------------------------------------------------------------------
class CDmeBBox : public CDmeShape
{
	DEFINE_ELEMENT( CDmeBBox, CDmeShape );

public:
	void Clear();
	bool Empty() const;
	void TransformUsing ( const matrix3x4_t &mMatrix );
	void Expand( const Vector &vPoint );
	void Expand( const CDmeBBox &bbox );
	bool Contains( const Vector &vPoint ) const;
	bool Intersects( const CDmeBBox &bbox ) const;
	float Width() const;	// X
	float Height() const;	// Y
	float Depth() const;	// Z
	Vector Center() const;
	const Vector &Min() const;
	const Vector &Max() const;

	CDmaVar< Vector > m_vMinBounds;	
	CDmaVar< Vector > m_vMaxBounds;

	// From CDmeShape
	virtual void Draw( const matrix3x4_t &shapeToWorld, CDmeDrawSettings *pDrawSettings = NULL );
};


#endif // DMEBBOX_H
