//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef GAMESTAGE_H
#define GAMESTAGE_H

#ifdef _WIN32
#pragma once
#endif

#include "graphicgroup.h"
#include "dmxloader/dmxelement.h"
#include "tier1/utlvector.h"


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
class CGameStage : public CGraphicGroup
{
	DECLARE_DMXELEMENT_UNPACK()

public:

	CGameStage();
	virtual ~CGameStage();

	bool Unserialize( CDmxElement *pElement, CUtlDict< CGameGraphic *, int > &unserializedGraphicMapping );

	// Update geometry and execute scripting.
	virtual void UpdateRenderTransforms( const StageRenderInfo_t &stageRenderInfo, const CGraphicGroup *pGroup ){}
	void UpdateRenderTransforms( const Rect_t &viewport );

	virtual void GetRenderTransform( matrix3x4_t &relToScreen, bool bMaintainAspectRatio ) const; 
	const StageRenderInfo_t &GetRenderInfo() const { return m_StageRenderInfo; }

	void UpdateAspectRatio( const Rect_t &viewport );
	void SetStageSize( int nWide, int nTall );
	void GetStageSize( Vector2D &stageSize ) const { stageSize = m_StageSize; }
	void GetMaintainAspectRatioStageSize( Vector2D &stageSize ){ stageSize = m_MaintainAspectRatioStageSize; }
	color32 GetStageColor() const { return m_Geometry.m_Color; }


	virtual bool IsStageGroup() const { return true; }



private:

	Vector2D m_StageSize;
	Vector2D m_MaintainAspectRatioStageSize;
	bool m_bFullscreen;
	StageRenderInfo_t m_StageRenderInfo;
};





#endif // GAMESTAGE_H
