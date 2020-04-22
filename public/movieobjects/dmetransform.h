//====== Copyright © 1996-2004, Valve Corporation, All rights reserved. =======
//
// A class representing a transform
//
//=============================================================================

#ifndef DMETRANSFORM_H
#define DMETRANSFORM_H
#ifdef _WIN32
#pragma once
#endif

#include "datamodel/dmelement.h"

static const char TRANSFORM_POSITION[] = "position";
static const char TRANSFORM_ORIENTATION[] = "orientation";

//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
struct matrix3x4_t;
class CDmeDag;

//-----------------------------------------------------------------------------
// A class representing a transformation matrix
//-----------------------------------------------------------------------------
class CDmeTransform : public CDmElement
{
	DEFINE_ELEMENT( CDmeTransform, CDmElement );

public:
	virtual void OnAttributeChanged( CDmAttribute *pAttribute );

	void SetTransform( const matrix3x4_t &transform );
	void GetTransform( matrix3x4_t &transform );

	const Vector &GetPosition() const;
	void SetPosition( const Vector &vecPosition );
	const Quaternion &GetOrientation() const;
	void SetOrientation( const Quaternion &orientation );

	CDmAttribute *GetPositionAttribute();
	CDmAttribute *GetOrientationAttribute();

	// If transform is contained inside some kind of CDmeDag, return that (it's "parent")
	CDmeDag *GetDag();

private:
	CDmaVar<Vector> m_Position;
	CDmaVar<Quaternion> m_Orientation;
};


#endif // DMETRANSFORM_H
