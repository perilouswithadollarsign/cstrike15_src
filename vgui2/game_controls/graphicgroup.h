//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef GRAPHICGROUP_H
#define GRAPHICGROUP_H

#ifdef _WIN32
#pragma once
#endif

#include "dmxloader/dmxelement.h"
#include "tier1/utlvector.h"
#include "tier1/utldict.h"
#include "gamegraphic.h"
#include "hitarea.h"


//-----------------------------------------------------------------------------
// A class that holds a group of graphics.
//-----------------------------------------------------------------------------
class CGraphicGroup : public CGameGraphic 
{
	DECLARE_DMXELEMENT_UNPACK()

public:

	CGraphicGroup();
	virtual ~CGraphicGroup();

	bool Unserialize( CDmxElement *pElement, CUtlDict< CGameGraphic *, int > &unserializedGraphicMapping );


	virtual void UpdateGeometry();
	void UpdateRenderData( color32 parentColor );
	virtual void UpdateRenderTransforms( const StageRenderInfo_t &stageRenderInfo );

	color32 GetResultantColor() const { return m_ResultantColor; } // needed by group children.
	virtual void GetRenderTransform( matrix3x4_t &relToScreen, bool bMaintainAspectRatio ) const; // needed by group children.

	void AddToGroup( CGameGraphic *pGraphic );
	void RemoveFromGroup( CGameGraphic *pGraphic );

	// From GameGraphic

	virtual bool HasState( const char *pStateName );
	virtual void SetState( const char *pStateName );
	virtual void StartPlaying();
	virtual void StopPlaying();
	virtual void AdvanceState();

	virtual bool IsGroup() const { return true; } 
	virtual bool IsStageGroup()const { return false; }

	CHitArea *GetKeyFocusRequestGraphic();

	virtual CGameGraphic *FindGraphicByName( const char *pName ) const;

	virtual void SetVisible( bool bVisible );

	bool MaintainAspectRatio() const;


protected:

	CUtlVector< CGameGraphic * > m_MemberList;
	color32 m_ResultantColor;

private:

	matrix3x4_t m_RelToScreenHoldAspectRatio;
	
};


#endif // GRAPHICGROUP_H
