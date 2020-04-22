//====== Copyright © 1996-2009, Valve Corporation, All rights reserved. =======
//
// DmeSkinner
//
//=============================================================================


#ifndef DMESKINNER_H
#define DMESKINNER_H


#ifdef _WIN32
#pragma once
#endif


// Valve includes
#include "datamodel/dmattributevar.h"
#include "mdlobjects/dmemdllist.h"
#include "movieobjects/dmejoint.h"
#include "movieobjects/dmedag.h"
#include "movieobjects/dmemesh.h"


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class CDmeModel;
class CDmeDag;


//-----------------------------------------------------------------------------
// DmeSkinnerVolume
//-----------------------------------------------------------------------------
class CDmeSkinnerVolume : public CDmElement
{
	DEFINE_ELEMENT( CDmeSkinnerVolume, CDmElement );

public:
	enum FalloffType_t
	{
		FT_LINEAR = 0,
		FT_SMOOTH = 1,
		FT_SPIKE = 2,
		FT_DOME = 3
	};

	bool IsEllipse() const { return m_flPowerXZ == 1.0 && m_flPowerY == 1.0; }
	bool IsSuper() const { return m_flPowerXZ != 1.0 || m_flPowerY != 1.0; }

	CDmaVar< VMatrix > m_mMatrix;
	CDmaVar< float > m_flStrength;
	CDmaVar< float > m_flFalloff;
	CDmaVar< int > m_nFalloffType;	// 0:Linear, 1:Smooth, 2:Spike, 3:Dome
	CDmaVar< float > m_flPowerY;
	CDmaVar< float > m_flPowerXZ;
};


//-----------------------------------------------------------------------------
// DmeSkinnerJoint
//-----------------------------------------------------------------------------
class CDmeSkinnerJoint : public CDmeJoint
{
	DEFINE_ELEMENT( CDmeSkinnerJoint, CDmeJoint );

public:
	CDmAttribute *GetListAttr() { return m_eVolumeList.GetAttribute(); }

	CDmaVar< VMatrix > m_mBindWorldMatrix;
	CDmaElementArray< CDmeSkinnerVolume > m_eVolumeList;
};


//-----------------------------------------------------------------------------
// DmeSkinner
//-----------------------------------------------------------------------------
class CDmeSkinner : public CDmeDag
{
	DEFINE_ELEMENT( CDmeSkinner, CDmeDag );

public:
	CDmAttribute *GetListAttr() { return m_Children.GetAttribute(); }

	bool ReskinMeshes( CDmeModel *pDmeModel, int nJointPerVertexCount );

	bool ReskinMesh( CDmeModel *pDmeModel, CDmeMesh *pDmeMesh, int nJointPerVertexCount );

};

#endif // DMESKINNER_H